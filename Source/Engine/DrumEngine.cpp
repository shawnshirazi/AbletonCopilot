#include "DrumEngine.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace Engine
{
    namespace
    {
        // Role-specific salts so generateKick/Clap/Hat/Perc called with the
        // same seed don't all draw identical random sequences.
        constexpr uint32_t kKickSalt = 0x4B49434Bu; // 'KICK'
        constexpr uint32_t kClapSalt = 0x434C4150u; // 'CLAP'
        constexpr uint32_t kHatSalt  = 0x48415420u; // 'HAT '
        constexpr uint32_t kPercSalt = 0x50455243u; // 'PERC'

        std::mt19937 makeRng(uint32_t seed, uint32_t salt)
        {
            return std::mt19937(seed ^ salt);
        }

        float uniform01(std::mt19937& rng)
        {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            return dist(rng);
        }

        void setHit(StepArray& steps, int step, float velocity)
        {
            if (step >= 0 && step < (int) steps.size())
            {
                steps[(size_t) step].active   = true;
                steps[(size_t) step].velocity = velocity;
            }
        }

        // Shared "syncopation" mechanism for HAT's second layer and PERC:
        // the rhythmically weakest 16th-note position in each beat is the
        // "a" just before the next beat (steps 3/7/11/15 of a 16-step bar).
        // Returns a probability MULTIPLIER, not a probability itself - 1.0
        // at syncopation=0 (no bias, uniform), up to 2x at weak positions /
        // down to 0.5x elsewhere at syncopation=1.
        bool isWeakPosition(int stepWithinBar, int stepsPerBar)
        {
            const int stepsPerBeat = std::max(1, stepsPerBar / 4);
            return (stepWithinBar % stepsPerBeat) == (stepsPerBeat - 1);
        }

        float weakPositionBias(int stepWithinBar, int stepsPerBar, float syncopation)
        {
            return isWeakPosition(stepWithinBar, stepsPerBar)
                       ? (1.0f + syncopation)
                       : (1.0f - syncopation * 0.5f);
        }

        // Shared "variation" mechanism for HAT and PERC: thin even-indexed
        // bars relative to odd ones (a real arrangement technique -
        // alternating full/thinned bars) instead of pure per-step noise.
        // 1.0 at variation=0 (every bar identical), down to 0.5x on even
        // bars at variation=1.
        float barThinningFactor(int bar, float variation)
        {
            return (bar % 2 == 0) ? (1.0f - variation * 0.5f) : 1.0f;
        }

        float clamp01(float v)
        {
            return std::max(0.0f, std::min(1.0f, v));
        }

        // Phrase-aware intensity curve, applied to variation/syncopation-
        // driven probabilities so the 16-bar loop builds across itself
        // instead of every bar being independently random:
        //   bars  1-4  (phrase 0) = establish      - calmest
        //   bars  5-8  (phrase 1) = subtle variation
        //   bars  9-12 (phrase 2) = development
        //   bars 13-16 (phrase 3) = variation / phrase ending - busiest
        // Pure function of (bar, numBars) - no RNG, fully deterministic,
        // and at any probability that's already 0 (a knob left at its
        // baseline) this multiplies 0 by a number and stays 0, so it never
        // changes the flat/baseline behaviour of any role, only how
        // strongly the already-present optional effects lean in as the
        // pattern progresses. Degrades gracefully for numBars < 4 (still
        // divides into 4 phrases, just shorter ones).
        float phraseIntensity(int bar, int numBars)
        {
            if (numBars <= 0)
                return 1.0f;

            constexpr int phraseCount = 4;
            const int barsPerPhrase   = std::max(1, numBars / phraseCount);
            const int phrase          = std::min(phraseCount - 1, bar / barsPerPhrase);

            static const float kPhraseScale[phraseCount] = { 0.6f, 0.85f, 1.1f, 1.4f };
            return kPhraseScale[phrase];
        }
    }

    StepArray generateKick(const StepGridConfig& grid, const DrumPatternParams& params)
    {
        const int total = totalSteps(grid);
        StepArray steps((size_t) total);
        std::mt19937 rng = makeRng(params.seed, kKickSalt);

        const int stepsPerBeat = std::max(1, grid.stepsPerBar / 4);

        for (int bar = 0; bar < grid.numBars; ++bar)
        {
            const int base = bar * grid.stepsPerBar;

            // variation: rare per-bar fill (drop one beat). Kept genuinely
            // rare (scaled well below variation's raw value) since a
            // steady, reliable kick is the point, not the exception - see
            // the density=NOT-USED note in DrumEngine.h. Phrase-scaled so
            // drops stay rarer in the establishing bars and become more
            // likely toward the phrase-ending bars - four-on-the-floor
            // itself never changes, only how often the rare drop fires.
            const float phrase = phraseIntensity(bar, grid.numBars);
            bool dropABeat  = uniform01(rng) < params.variation * 0.15f * phrase;
            int  droppedBeat = dropABeat ? (int) (uniform01(rng) * 4.0f) : -1;
            droppedBeat = std::min(droppedBeat, 3);

            for (int beat = 0; beat < 4; ++beat)
            {
                if (beat == droppedBeat)
                    continue;
                setHit(steps, base + beat * stepsPerBeat, 1.0f);
            }

            // syncopation: rare pushed hit on the weakest position (the
            // "and" of the last beat), driving into the next bar - same
            // phrase scaling as the drop above.
            if (uniform01(rng) < params.syncopation * 0.2f * phrase)
                setHit(steps, base + grid.stepsPerBar - 2, 0.7f);
        }

        return steps;
    }

    StepArray generateClap(const StepGridConfig& grid, const DrumPatternParams& params)
    {
        const int total = totalSteps(grid);
        StepArray steps((size_t) total);
        std::mt19937 rng = makeRng(params.seed, kClapSalt);

        // Traditional backbeat position (beat 3) - the sourced pattern
        // still lands there, it's just sparse across bars rather than
        // hitting every bar.
        const int backbeatStep   = (grid.stepsPerBar * 3) / 4;
        const int doubletGapStep = std::max(1, grid.stepsPerBar / 8); // one 8th-note later

        // syncopation: occasional +/-1 step nudge off the canonical
        // position. Phrase-scaled (controlled clap variation - jitter
        // leans in more as the pattern develops) by the bar the hit
        // actually lands on.
        auto jitter = [&](int step, int bar) -> int
        {
            if (uniform01(rng) < params.syncopation * 0.3f * phraseIntensity(bar, grid.numBars))
                return step + (uniform01(rng) < 0.5f ? -1 : 1);
            return step;
        };

        for (int cycleStart = 0; cycleStart < grid.numBars; cycleStart += 4)
        {
            // variation: probability this cycle swaps which bar gets the
            // single hit vs. the doublet, instead of the canonical
            // bar-2=single/bar-4=doublet layout. At variation=0 this is
            // never true (uniform01 never returns a negative number, so
            // "< 0.0f" never fires); at variation=1 it's always true
            // (uniform01's [0,1) range means "< 1.0f" always fires) - both
            // boundaries are exact, not just statistically likely. Not
            // phrase-scaled - this one stays a pure function of variation
            // alone so the boundary guarantee holds regardless of phrase.
            const bool swapped = uniform01(rng) < params.variation;
            const int  singleBarOffset  = swapped ? 3 : 1;
            const int  doubletBarOffset = swapped ? 1 : 3;

            const int singleBar  = cycleStart + singleBarOffset;
            const int doubletBar = cycleStart + doubletBarOffset;

            if (singleBar < grid.numBars)
            {
                const int base = singleBar * grid.stepsPerBar;
                setHit(steps, jitter(base + backbeatStep, singleBar), 1.0f);
            }
            if (doubletBar < grid.numBars)
            {
                const int base = doubletBar * grid.stepsPerBar;
                setHit(steps, jitter(base + backbeatStep, doubletBar), 1.0f);
                setHit(steps, jitter(base + backbeatStep + doubletGapStep, doubletBar), 0.8f);
            }
        }

        // density: rare extra ghost clap somewhere a bar would otherwise
        // leave silent - kept low-probability, this role stays sparse even
        // at density=1. Phrase-scaled: ghosts lean in more in the
        // development/ending phrases than the establishing bars.
        for (int bar = 0; bar < grid.numBars; ++bar)
        {
            if (uniform01(rng) >= params.density * 0.1f * phraseIntensity(bar, grid.numBars))
                continue;
            const int base = bar * grid.stepsPerBar;
            const int step = base + (int) (uniform01(rng) * (float) grid.stepsPerBar);
            setHit(steps, step, 0.5f);
        }

        return steps;
    }

    StepArray generateHat(const StepGridConfig& grid, const DrumPatternParams& params)
    {
        const int total = totalSteps(grid);
        StepArray steps((size_t) total);
        std::mt19937 rng = makeRng(params.seed, kHatSalt);

        const int eighthStep = std::max(1, grid.stepsPerBar / 8);

        for (int bar = 0; bar < grid.numBars; ++bar)
        {
            const int base = bar * grid.stepsPerBar;

            // Primary pulse: off-beat 8ths (the "and" of each beat) - the
            // genre-defining baseline, always present, not parameterized
            // (same reasoning as kick's four-on-the-floor). Controlled
            // velocity accent, not flat: the offbeats leading into beats 1
            // and 3 sit slightly hotter than the ones leading into 2 and 4
            // - a fixed, deterministic groove shape (not RNG-driven), the
            // same every time for a given grid, giving the foundation some
            // shape instead of a uniform pulse.
            for (int k = 0; k < 4; ++k)
            {
                const int step = base + eighthStep * (2 * k + 1);
                if (step < base + grid.stepsPerBar)
                {
                    const float accentVel = (k % 2 == 0) ? 0.95f : 0.8f;
                    setHit(steps, step, accentVel);
                }
            }

            // Second layer: density controls fill amount, syncopation
            // biases WHICH remaining positions get filled (toward the
            // weakest 16ths), variation thins even-indexed bars, and the
            // phrase curve leans the whole layer in more as the pattern
            // develops toward the phrase-ending bars.
            const float barDensity = clamp01(params.density * barThinningFactor(bar, params.variation));
            const float phrase     = phraseIntensity(bar, grid.numBars);

            for (int stepInBar = 0; stepInBar < grid.stepsPerBar; ++stepInBar)
            {
                const int step = base + stepInBar;
                if (steps[(size_t) step].active)
                    continue;

                const float bias = weakPositionBias(stepInBar, grid.stepsPerBar, params.syncopation);
                const float p    = clamp01(barDensity * 0.5f * bias * phrase);

                if (uniform01(rng) < p)
                {
                    const float vel = 0.3f + uniform01(rng) * 0.25f; // 0.3-0.55, clearly under the primary pulse
                    setHit(steps, step, vel);
                }
            }
        }

        return steps;
    }

    StepArray generatePerc(const StepGridConfig& grid, const DrumPatternParams& params)
    {
        const int total = totalSteps(grid);
        StepArray steps((size_t) total);
        std::mt19937 rng = makeRng(params.seed, kPercSalt);

        for (int bar = 0; bar < grid.numBars; ++bar)
        {
            const int base = bar * grid.stepsPerBar;
            const float barDensity = clamp01(params.density * barThinningFactor(bar, params.variation));
            const float phrase     = phraseIntensity(bar, grid.numBars);

            for (int stepInBar = 0; stepInBar < grid.stepsPerBar; ++stepInBar)
            {
                // density: base per-step probability, deliberately damped
                // so this role stays an accent, not a second full pattern,
                // even at density=1. syncopation: biases placement toward
                // the weakest 16th positions (same mechanism as HAT's
                // second layer) - this used to only affect velocity range,
                // which didn't match its name; fixed. Restrained by design:
                // the phrase curve leans density in gradually rather than
                // flooding every bar equally, keeping this an accent role
                // even as the pattern develops.
                const float bias = weakPositionBias(stepInBar, grid.stepsPerBar, params.syncopation);
                const float p    = clamp01(barDensity * 0.15f * bias * phrase);

                if (uniform01(rng) < p)
                {
                    // variation widens the velocity range (more dynamic
                    // hit-to-hit swing) in addition to the bar-thinning
                    // above - both readings of "how varied" under one knob.
                    const float velRange = 0.3f + params.variation * 0.3f;
                    const float vel = std::min(1.0f, 0.4f + uniform01(rng) * velRange);
                    setHit(steps, base + stepInBar, vel);
                }
            }
        }

        // Occasional phrase-ending fill: the very last bar of the pattern
        // gets a small chance of one extra accent hit, driven by variation
        // - an explicit "the loop is about to restart" cue (a real
        // arrangement technique) rather than uniform per-bar randomness.
        // Never fires at variation=0, same as every other variation-gated
        // effect in this file.
        if (grid.numBars > 0 && uniform01(rng) < params.variation * 0.35f)
        {
            const int lastBar = grid.numBars - 1;
            const int base    = lastBar * grid.stepsPerBar;
            const int step    = base + (int) (uniform01(rng) * (float) grid.stepsPerBar);
            const float vel   = 0.7f + uniform01(rng) * 0.3f;
            setHit(steps, step, vel);
        }

        return steps;
    }

    std::vector<int> toVelocityArray(const StepArray& steps)
    {
        std::vector<int> out(steps.size(), 0);
        for (size_t i = 0; i < steps.size(); ++i)
        {
            if (!steps[i].active)
                continue;

            int vel = (int) std::lround(steps[i].velocity * 127.0f);
            if (vel < 1)   vel = 1;
            if (vel > 127) vel = 127;
            out[i] = vel;
        }
        return out;
    }
}
