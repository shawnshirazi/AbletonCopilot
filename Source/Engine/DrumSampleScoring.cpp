#include "DrumSampleScoring.h"
#include "DrumSampleFingerprint.h"
#include <algorithm>
#include <cmath>

namespace Engine
{
    namespace
    {
        // Gaussian similarity to the measured target: 1.0 at an exact match
        // to the corpus mean, falling off smoothly as the candidate
        // diverges - scaled by the target's own measured stddev, so a
        // dimension the real corpus agreed on tightly (small stddev)
        // demands a closer match than one the corpus itself was spread out
        // on (large stddev, where a wide match is still perfectly typical).
        // This is the direct statistical shape of "how well does this fit
        // what Melodic Techno samples actually measure like", not a
        // hand-picked acceptable range.
        float gaussianFit(float value, const FingerprintDim& target)
        {
            const float z = (value - target.mean) / target.stddev;
            return std::exp(-0.5f * z * z);
        }

        constexpr double kReferenceBpm = 124.0; // typical melodic techno tempo, used when no real host tempo is known yet

        struct Weights { float duration, attack, zcr, pitch, peak, rms; };

        // Kick leans harder on pitch (a real, tuned low-end fundamental is
        // what separates a kick from a thud) and less on zero-crossing
        // rate (kick brightness varies far more across the measured corpus
        // - see DrumSampleFingerprint.h's kKickFingerprint.zeroCrossingHz
        // stddev - than it discriminates). Every other role shares one
        // profile: duration+attack+zcr dominate (the three dimensions that
        // most separate "closed hat" from "open hat" from "clap" from
        // "perc" in the measured data), pitch/peak/rms are minor
        // tie-breakers. Weights sum to 1.0 in both profiles.
        constexpr Weights kKickWeights    { 0.25f, 0.20f, 0.15f, 0.30f, 0.05f, 0.05f };
        constexpr Weights kDefaultWeights { 0.30f, 0.25f, 0.25f, 0.10f, 0.05f, 0.05f };

        float scoreAgainstFingerprint(const DrumSampleFeatures& f, const DrumRoleFingerprint& fp, const Weights& w)
        {
            float s = 0.0f;
            s += gaussianFit((float) f.durationSec, fp.durationSec)   * w.duration;
            s += gaussianFit(f.attackTimeMs,        fp.attackMs)      * w.attack;
            s += gaussianFit(f.zeroCrossingRate,     fp.zeroCrossingHz) * w.zcr;
            s += gaussianFit(f.estimatedPitchHz,     fp.estimatedPitchHz) * w.pitch;
            s += gaussianFit(f.peakAmplitude,        fp.peak)         * w.peak;
            s += gaussianFit(f.rms,                  fp.rms)          * w.rms;
            return s;
        }
    }

    float scoreForRole(DrumRole role, const DrumSampleFeatures& features, double bpmHint)
    {
        if (!features.valid)
            return 0.0f;

        const DrumRoleFingerprint& fp = targetFingerprintForRole(role);

        switch (role)
        {
            case DrumRole::Kick:
            {
                float s = scoreAgainstFingerprint(features, fp, kKickWeights);

                // Playability constraint, not a character mismatch: a kick
                // that rings out past the current tempo's beat length
                // audibly overlaps the next 4/4 hit. This is tempo-
                // dependent (the measured fingerprint isn't), so it's
                // applied as a penalty on top of the measured-character
                // score rather than folded into the fingerprint itself.
                const double bpm     = bpmHint > 0.0 ? bpmHint : kReferenceBpm;
                const double beatSec = 60.0 / bpm;
                if (features.durationSec > beatSec * 0.9)
                    s *= 0.5f;

                return s;
            }
            case DrumRole::Clap:      return scoreAgainstFingerprint(features, fp, kDefaultWeights);
            case DrumRole::HatClosed: return scoreAgainstFingerprint(features, fp, kDefaultWeights);
            case DrumRole::HatOpen:   return scoreAgainstFingerprint(features, fp, kDefaultWeights);
            case DrumRole::PercA:     return scoreAgainstFingerprint(features, fp, kDefaultWeights);
            case DrumRole::PercB:     return scoreAgainstFingerprint(features, fp, kDefaultWeights);
            case DrumRole::Count:     return 0.0f;
        }
        return 0.0f;
    }
}
