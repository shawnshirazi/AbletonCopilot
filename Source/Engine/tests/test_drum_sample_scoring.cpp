#include "../DrumSampleScoring.h"
#include "TestSupport.h"

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

    TEST_SUMMARY_AND_EXIT();
}
