#include "DrumSampleScoring.h"
#include <algorithm>
#include <cmath>

namespace Engine
{
    namespace
    {
        float clamp01(float v)
        {
            return std::max(0.0f, std::min(1.0f, v));
        }

        // How well `value` fits the target range [lo, hi]: 1.0 inside the
        // range, falling off linearly outside it at `falloff` per unit -
        // a simple, explainable shape (not a black box), used for every
        // characteristic below.
        float rangeFit(float value, float lo, float hi, float falloff)
        {
            if (value >= lo && value <= hi)
                return 1.0f;
            const float dist = value < lo ? (lo - value) : (value - hi);
            return clamp01(1.0f - dist * falloff);
        }

        constexpr double kReferenceBpm = 124.0; // typical melodic techno tempo, used when no real host tempo is known yet

        float scoreKick(const DrumSampleFeatures& f, double bpmHint)
        {
            const double bpm     = bpmHint > 0.0 ? bpmHint : kReferenceBpm;
            const double beatSec = 60.0 / bpm;

            float s = 0.0f;

            // Controlled decay suitable for 4/4 techno: shouldn't ring
            // out past roughly 80% of a beat at the current tempo, or it
            // audibly overlaps the next kick. Also penalizes very short
            // (<60ms, probably a click/transient-only fragment, not a
            // usable kick body).
            s += rangeFit((float) f.durationSec, 0.06f, (float) (beatSec * 0.8), 3.0f) * 0.30f;

            // Strong transient - fast attack is what makes a kick punchy
            // rather than a slow swell.
            s += rangeFit(f.attackTimeMs, 0.0f, 8.0f, 0.15f) * 0.30f;

            // Controlled top end - a very high zero-crossing rate usually
            // means a noisy/distorted/overly-bright kick, not the tight
            // low-end character asked for.
            s += rangeFit(f.zeroCrossingRate, 0.0f, 900.0f, 0.002f) * 0.20f;

            // Tight low end: reward a clear, plausible kick fundamental
            // when one was found (30-100Hz covers typical techno kick
            // tuning) - "where practical", so a sample with no detected
            // pitch (noise-heavy/very short) isn't penalized hard, just
            // scored lower on this one factor.
            s += (f.estimatedPitchHz > 30.0f && f.estimatedPitchHz < 100.0f) ? 0.20f : 0.05f;

            return s;
        }

        float scoreHat(const DrumSampleFeatures& f)
        {
            float s = 0.0f;

            // Short decay - closed-hat character, not an open/ringing
            // sample that would smear a restrained 16th/offbeat pattern.
            s += rangeFit((float) f.durationSec, 0.02f, 0.30f, 4.0f) * 0.35f;

            // Crisp/bright - controlled high-frequency energy is exactly
            // what a high zero-crossing rate measures.
            s += rangeFit(f.zeroCrossingRate, 2500.0f, 9000.0f, 0.0006f) * 0.35f;

            // Sharp transient.
            s += rangeFit(f.attackTimeMs, 0.0f, 5.0f, 0.25f) * 0.30f;

            return s;
        }

        float scorePerc(const DrumSampleFeatures& f)
        {
            float s = 0.0f;

            // Short and dry - an accent, not a loop or a long tail.
            s += rangeFit((float) f.durationSec, 0.03f, 0.35f, 3.5f) * 0.35f;

            // Present but controlled brightness - electronic/organic
            // texture without being harsh noise.
            s += rangeFit(f.zeroCrossingRate, 800.0f, 5000.0f, 0.0008f) * 0.30f;

            // Shouldn't be the loudest thing in the room - percussion is
            // an accent, not something that should dominate the mix.
            s += rangeFit(f.peakAmplitude, 0.1f, 0.85f, 2.0f) * 0.20f;

            s += rangeFit(f.attackTimeMs, 0.0f, 10.0f, 0.1f) * 0.15f;

            return s;
        }

        float scoreClap(const DrumSampleFeatures& f)
        {
            float s = 0.0f;

            // Appropriate decay - present but not a wash that would
            // compete with the kick's own tail.
            s += rangeFit((float) f.durationSec, 0.08f, 0.45f, 3.0f) * 0.35f;

            s += rangeFit(f.attackTimeMs, 0.0f, 10.0f, 0.15f) * 0.30f;

            // Clean electronic character - a clear noise/tone band
            // rather than a dull thud or harsh distortion.
            s += rangeFit(f.zeroCrossingRate, 1200.0f, 6000.0f, 0.0007f) * 0.35f;

            return s;
        }
    }

    float scoreForRole(DrumRole role, const DrumSampleFeatures& features, double bpmHint)
    {
        if (!features.valid)
            return 0.0f;

        switch (role)
        {
            case DrumRole::Kick:  return scoreKick(features, bpmHint);
            case DrumRole::Hat:   return scoreHat(features);
            case DrumRole::Perc:  return scorePerc(features);
            case DrumRole::Clap:  return scoreClap(features);
            case DrumRole::Count: return 0.0f;
        }
        return 0.0f;
    }
}
