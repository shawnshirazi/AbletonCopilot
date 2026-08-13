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

            // Rare fill: drop one beat this bar. Kept genuinely rare
            // (scaled well below variation's raw value) since a steady,
            // reliable kick is the point, not the exception.
            bool dropABeat  = uniform01(rng) < params.variation * 0.15f;
            int  droppedBeat = dropABeat ? (int) (uniform01(rng) * 4.0f) : -1;
            droppedBeat = std::min(droppedBeat, 3);

            for (int beat = 0; beat < 4; ++beat)
            {
                if (beat == droppedBeat)
                    continue;
                setHit(steps, base + beat * stepsPerBeat, 1.0f);
            }

            // Rare syncopated push into the next bar - a real technique,
            // not the default.
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
        const int backbeatStep = (grid.stepsPerBar * 3) / 4;
        const int doubletGapStep = std::max(1, grid.stepsPerBar / 8); // one 8th-note later

        auto jitter = [&](int step) -> int
        {
            // Occasional +/-1 step nudge, scaled by syncopation - keeps the
            // sourced pattern from being bar-identical every repeat.
            if (uniform01(rng) < params.syncopation * 0.3f)
                return step + (uniform01(rng) < 0.5f ? -1 : 1);
            return step;
        };

        for (int cycleStart = 0; cycleStart < grid.numBars; cycleStart += 4)
        {
            // Bar 2 of the cycle: single clap.
            if (cycleStart + 1 < grid.numBars)
            {
                const int base = (cycleStart + 1) * grid.stepsPerBar;
                setHit(steps, jitter(base + backbeatStep), 1.0f);
            }
            // Bar 4 of the cycle: doublet flourish.
            if (cycleStart + 3 < grid.numBars)
            {
                const int base = (cycleStart + 3) * grid.stepsPerBar;
                setHit(steps, jitter(base + backbeatStep), 1.0f);
                setHit(steps, jitter(base + backbeatStep + doubletGapStep), 0.8f);
            }
        }

        // density can add a rare extra ghost clap elsewhere in a bar that
        // would otherwise be silent - kept low-probability, this role is
        // meant to stay sparse.
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

            // Primary pulse: off-beat 8ths (the "and" of each beat).
            for (int k = 0; k < 4; ++k)
            {
                const int step = base + eighthStep * (2 * k + 1);
                if (step < base + grid.stepsPerBar)
                    setHit(steps, step, 0.9f);
            }

            // Second layer: the remaining 16th positions, velocity-reduced
            // and probability-gated by density, for organic dynamics
            // rather than a second, equally-loud pulse.
            for (int step = base; step < base + grid.stepsPerBar; ++step)
            {
                if (steps[(size_t) step].active)
                    continue;
                if (uniform01(rng) < params.density * 0.5f)
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

        for (int step = 0; step < total; ++step)
        {
            // Sparse by construction - density scales the chance but is
            // deliberately dampened so this role stays an accent, not a
            // second full pattern.
            if (uniform01(rng) < params.density * 0.15f)
            {
                const float vel = 0.4f + uniform01(rng) * (0.3f + params.syncopation * 0.3f);
                setHit(steps, step, std::min(vel, 1.0f));
            }
        }

        return steps;
    }
}
