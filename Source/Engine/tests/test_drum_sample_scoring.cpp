#include "../DrumSampleScoring.h"
#include "../DrumSampleFingerprint.h"
#include "TestSupport.h"
#include <cmath>

using namespace Engine;

namespace
{
    DrumSampleFeatures makeFeatures(double durationSec, float attackTimeMs, float zcr,
                                     float peak = 0.6f, float pitchHz = 0.0f)
    {
        DrumSampleFeatures f;
        f.valid            = true;
        f.durationSec       = durationSec;
        f.attackTimeMs      = attackTimeMs;
        f.zeroCrossingRate  = zcr;
        f.peakAmplitude     = peak;
        f.rms               = peak * 0.5f;
        f.estimatedPitchHz  = pitchHz;
        return f;
    }

    // Builds features `stddevsAway` standard deviations from `fp`'s mean on
    // every dimension at once (0 = an exact match to the measured target).
    DrumSampleFeatures makeFeaturesAtFingerprintOffset(const DrumRoleFingerprint& fp, float stddevsAway)
    {
        DrumSampleFeatures f;
        f.valid            = true;
        f.durationSec       = fp.durationSec.mean + stddevsAway * fp.durationSec.stddev;
        f.attackTimeMs      = fp.attackMs.mean + stddevsAway * fp.attackMs.stddev;
        f.zeroCrossingRate  = fp.zeroCrossingHz.mean + stddevsAway * fp.zeroCrossingHz.stddev;
        f.estimatedPitchHz  = fp.estimatedPitchHz.mean + stddevsAway * fp.estimatedPitchHz.stddev;
        f.peakAmplitude     = fp.peak.mean + stddevsAway * fp.peak.stddev;
        f.rms               = fp.rms.mean + stddevsAway * fp.rms.stddev;
        return f;
    }
}

int main()
{
    // =====================================================================
    // Invalid features always score 0, regardless of role.
    // =====================================================================
    {
        DrumSampleFeatures invalid; // valid = false by default
        for (auto role : { DrumRole::Kick, DrumRole::Clap, DrumRole::Hat, DrumRole::Perc })
            CHECK(scoreForRole(role, invalid, 124.0) == 0.0f);
    }

    // =====================================================================
    // Kick: a tight, punchy, low-pitched sample scores clearly higher than
    // a long, slow-attack, unpitched (or wrongly-pitched) one - directly
    // exercising the stated preference ("tight low-end... avoid overly
    // acoustic/boomy kicks").
    // =====================================================================
    {
        auto tightPunchy = makeFeatures(0.3, 2.0f, 400.0f, 0.7f, 55.0f);   // short, fast attack, controlled ZCR, in-range pitch
        auto boomySlow   = makeFeatures(1.2, 25.0f, 1500.0f, 0.7f, 0.0f); // long, slow attack, noisy, no clear pitch
        CHECK(scoreForRole(DrumRole::Kick, tightPunchy, 124.0) > scoreForRole(DrumRole::Kick, boomySlow, 124.0));
    }

    // =====================================================================
    // Kick + BPM: the same borderline-long sample scores lower at a
    // higher BPM (shorter beat, less room for a kick to ring out) than at
    // a lower BPM - this is the one role BPM actually changes scoring for.
    // =====================================================================
    {
        auto borderline = makeFeatures(0.55, 3.0f, 300.0f, 0.7f, 60.0f); // ~0.55s decay
        const float scoreSlow = scoreForRole(DrumRole::Kick, borderline, 90.0);  // long beat, plenty of room
        const float scoreFast = scoreForRole(DrumRole::Kick, borderline, 160.0); // short beat, less room
        CHECK(scoreSlow > scoreFast);
    }

    // =====================================================================
    // Kick: bpmHint <= 0 falls back to the fixed reference tempo rather
    // than crashing or producing a nonsensical (e.g. negative/huge) beat
    // length.
    // =====================================================================
    {
        auto f = makeFeatures(0.3, 2.0f, 400.0f, 0.7f, 55.0f);
        const float withFallback = scoreForRole(DrumRole::Kick, f, 0.0);
        const float atReference  = scoreForRole(DrumRole::Kick, f, 124.0);
        CHECK(withFallback == atReference);
    }

    // =====================================================================
    // Hat: short, bright, fast-attack scores higher than long, dull,
    // slow-attack ("crisp transient... short decay... avoid overly
    // noisy/long/open-hat samples").
    // =====================================================================
    {
        auto crisp = makeFeatures(0.12, 1.5f, 5000.0f);
        auto dull  = makeFeatures(0.9,  20.0f, 400.0f);
        CHECK(scoreForRole(DrumRole::Hat, crisp, 124.0) > scoreForRole(DrumRole::Hat, dull, 124.0));
    }

    // =====================================================================
    // Perc: short/moderate/not-dominant scores higher than long/very loud
    // ("avoid sounds that dominate the mix").
    // =====================================================================
    {
        auto restrained = makeFeatures(0.15, 4.0f, 2500.0f, 0.5f);
        auto dominant    = makeFeatures(1.0,  4.0f, 2500.0f, 0.99f);
        CHECK(scoreForRole(DrumRole::Perc, restrained, 124.0) > scoreForRole(DrumRole::Perc, dominant, 124.0));
    }

    // =====================================================================
    // Clap: appropriate duration/attack/character scores higher than an
    // extreme (very long, muddy) sample.
    // =====================================================================
    {
        auto clean = makeFeatures(0.2, 3.0f, 3000.0f);
        auto muddy = makeFeatures(1.5, 30.0f, 300.0f);
        CHECK(scoreForRole(DrumRole::Clap, clean, 124.0) > scoreForRole(DrumRole::Clap, muddy, 124.0));
    }

    // =====================================================================
    // Determinism: identical features -> identical score, every time, for
    // every role (pure function, no hidden state).
    // =====================================================================
    {
        auto f = makeFeatures(0.25, 3.0f, 2000.0f, 0.6f, 50.0f);
        for (auto role : { DrumRole::Kick, DrumRole::Clap, DrumRole::Hat, DrumRole::Perc })
            CHECK(scoreForRole(role, f, 124.0) == scoreForRole(role, f, 124.0));
    }

    // =====================================================================
    // Scores are always non-negative (clamped) and finite.
    // =====================================================================
    {
        auto extreme = makeFeatures(50.0, 5000.0f, 100000.0f, 5.0f, 5000.0f); // absurd out-of-range values
        for (auto role : { DrumRole::Kick, DrumRole::Clap, DrumRole::Hat, DrumRole::Perc })
        {
            const float s = scoreForRole(role, extreme, 124.0);
            CHECK(s >= 0.0f);
            CHECK(s == s); // NaN check (NaN != NaN)
        }
    }

    // =====================================================================
    // Fingerprint-based scoring (Phase 5): a candidate that exactly matches
    // its role's MEASURED target fingerprint (Source/Engine/
    // DrumSampleFingerprint.h, generated from real analysis - see
    // MLPipeline/drum_grammar/) scores higher than the same candidate
    // pushed 1, 2, and 3 measured standard deviations away - a direct,
    // real check that scoring is genuinely shaped by the measured data,
    // not an incidental side effect of some other heuristic.
    // =====================================================================
    {
        struct RoleFp { DrumRole role; const DrumRoleFingerprint& fp; };
        const RoleFp roleFps[] = {
            { DrumRole::Kick, targetFingerprintForRole(DrumRole::Kick) },
            { DrumRole::Clap, targetFingerprintForRole(DrumRole::Clap) },
            { DrumRole::Hat,  targetFingerprintForRole(DrumRole::Hat) },
            { DrumRole::Perc, targetFingerprintForRole(DrumRole::Perc) },
        };
        for (auto& rf : roleFps)
        {
            const float atMean  = scoreForRole(rf.role, makeFeaturesAtFingerprintOffset(rf.fp, 0.0f), 124.0);
            const float at1Sd   = scoreForRole(rf.role, makeFeaturesAtFingerprintOffset(rf.fp, 1.0f), 124.0);
            const float at2Sd   = scoreForRole(rf.role, makeFeaturesAtFingerprintOffset(rf.fp, 2.0f), 124.0);
            const float at3Sd   = scoreForRole(rf.role, makeFeaturesAtFingerprintOffset(rf.fp, 3.0f), 124.0);
            CHECK(atMean > at1Sd);
            CHECK(at1Sd > at2Sd);
            CHECK(at2Sd > at3Sd);
        }
    }

    // =====================================================================
    // A candidate exactly matching KICK's own measured fingerprint scores
    // higher for Kick than a candidate exactly matching a DIFFERENT role's
    // fingerprint - the four measured targets are genuinely distinct
    // enough to discriminate, not interchangeable copies of each other.
    // Uses a slow bpmHint (90) so this isolates fingerprint-character
    // discrimination from the separate BPM-overlap playability penalty
    // (already covered above) - the real measured KICK mean duration
    // (~0.49s) is close enough to a typical 124bpm beat length that the
    // playability penalty can legitimately fire on it, which would muddy
    // what this check is actually testing.
    // =====================================================================
    {
        auto kickIdeal = makeFeaturesAtFingerprintOffset(targetFingerprintForRole(DrumRole::Kick), 0.0f);
        auto hatIdeal  = makeFeaturesAtFingerprintOffset(targetFingerprintForRole(DrumRole::Hat), 0.0f);
        CHECK(scoreForRole(DrumRole::Kick, kickIdeal, 90.0) > scoreForRole(DrumRole::Kick, hatIdeal, 90.0));
        CHECK(scoreForRole(DrumRole::Hat, hatIdeal, 90.0) > scoreForRole(DrumRole::Hat, kickIdeal, 90.0));
    }

    // =====================================================================
    // The four generated target fingerprints are real, non-degenerate
    // measurements, not placeholder/zeroed data - every stddev is strictly
    // positive (a zero stddev would make gaussianFit divide by zero) and at
    // least the discriminating dimensions (duration/attack/zcr) differ
    // materially between roles (a real corpus wouldn't measure HAT and
    // KICK as acoustically identical).
    // =====================================================================
    {
        for (auto role : { DrumRole::Kick, DrumRole::Clap, DrumRole::Hat, DrumRole::Perc })
        {
            const auto& fp = targetFingerprintForRole(role);
            CHECK(fp.durationSec.stddev > 0.0f);
            CHECK(fp.attackMs.stddev > 0.0f);
            CHECK(fp.zeroCrossingHz.stddev > 0.0f);
            CHECK(fp.estimatedPitchHz.stddev > 0.0f);
            CHECK(fp.peak.stddev > 0.0f);
            CHECK(fp.rms.stddev > 0.0f);
        }
        const auto& kickFp = targetFingerprintForRole(DrumRole::Kick);
        const auto& hatFp  = targetFingerprintForRole(DrumRole::Hat);
        CHECK(std::abs(kickFp.zeroCrossingHz.mean - hatFp.zeroCrossingHz.mean) > 1000.0f); // real kicks measure far darker than real hats
    }

    TEST_SUMMARY_AND_EXIT();
}
