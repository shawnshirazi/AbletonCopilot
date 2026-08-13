#include "DrumEngine.h"
#include <algorithm>
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
            // the density=NOT-USED note in DrumEngine.h.
            bool dropABeat  = uniform01(rng) < params.variation * 0.15f;
            int  droppedBeat = dropABeat ? (int) (uniform01(rng) * 4.0f) : -1;
            droppedBeat = std::min(droppedBeat, 3);

            for (int beat = 0; beat < 4; ++beat)
            {
                if (beat == droppedBeat)
                    continue;
                setHit(steps, base + beat * stepsPerBeat, 1.0f);
            }

            // syncopation: rare pushed hit on the weakest position (the
            // "and" of the last beat), driving into the next bar.
            if (uniform01(rng) < params.syncopation * 0.2f)
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

        // syncopation: occasional +/-1 step nudge off the canonical position.
        auto jitter = [&](int step) -> int
        {
            if (uniform01(rng) < params.syncopation * 0.3f)
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
            // boundaries are exact, not just statistically likely.
            const bool swapped = uniform01(rng) < params.variation;
            const int  singleBarOffset  = swapped ? 3 : 1;
            const int  doubletBarOffset = swapped ? 1 : 3;

            if (cycleStart + singleBarOffset < grid.numBars)
            {
                const int base = (cycleStart + singleBarOffset) * grid.stepsPerBar;
                setHit(steps, jitter(base + backbeatStep), 1.0f);
            }
            if (cycleStart + doubletBarOffset < grid.numBars)
            {
                const int base = (cycleStart + doubletBarOffset) * grid.stepsPerBar;
                setHit(steps, jitter(base + backbeatStep), 1.0f);
                setHit(steps, jitter(base + backbeatStep + doubletGapStep), 0.8f);
            }
        }

        // density: rare extra ghost clap somewhere a bar would otherwise
        // leave silent - kept low-probability, this role stays sparse even
        // at density=1.
        for (int bar = 0; bar < grid.numBars; ++bar)
        {
            if (uniform01(rng) >= params.density * 0.1f)
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
            // (same reasoning as kick's four-on-the-floor).
            for (int k = 0; k < 4; ++k)
            {
                const int step = base + eighthStep * (2 * k + 1);
                if (step < base + grid.stepsPerBar)
                    setHit(steps, step, 0.9f);
            }

            // Second layer: density controls fill amount, syncopation
            // biases WHICH remaining positions get filled (toward the
            // weakest 16ths), variation thins even-indexed bars.
            const float barDensity = clamp01(params.density * barThinningFactor(bar, params.variation));

            for (int stepInBar = 0; stepInBar < grid.stepsPerBar; ++stepInBar)
            {
                const int step = base + stepInBar;
                if (steps[(size_t) step].active)
                    continue;

                const float bias = weakPositionBias(stepInBar, grid.stepsPerBar, params.syncopation);
                const float p    = clamp01(barDensity * 0.5f * bias);

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

            for (int stepInBar = 0; stepInBar < grid.stepsPerBar; ++stepInBar)
            {
                // density: base per-step probability, deliberately damped
                // so this role stays an accent, not a second full pattern,
                // even at density=1. syncopation: biases placement toward
                // the weakest 16th positions (same mechanism as HAT's
                // second layer) - this used to only affect velocity range,
                // which didn't match its name; fixed.
                const float bias = weakPositionBias(stepInBar, grid.stepsPerBar, params.syncopation);
                const float p    = clamp01(barDensity * 0.15f * bias);

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

        return steps;
    }
}
