#include "DrumEngine.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <utility>
#include <vector>

namespace Engine
{
    namespace
    {
        // Role-specific salts so generateClap/Hat/Perc called with the
        // same seed don't all draw identical random sequences. Kick no
        // longer draws any randomness at all in Drop mode (see
        // generateKick) - four-on-the-floor is deterministic by design,
        // not a probability outcome - so it has no salt to need.
        constexpr uint32_t kClapSalt = 0x434C4150u; // 'CLAP'
        constexpr uint32_t kHatSalt  = 0x48415420u; // 'HAT '
        constexpr uint32_t kPercSalt = 0x50455243u; // 'PERC'

        // Hat's foundation+support+ghost layers are built once per this
        // many bars (a "block"), then copyBlock() below blits that block
        // literally into every destination range meant to repeat it. This
        // is what keeps e.g. bars 0-3 and 4-7 byte-identical: the RNG is
        // consumed exactly once per block, never once per destination bar.
        constexpr int kBlockBars = 4;

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

        // The rhythmically weakest 16th-note position in each beat is the
        // "a" just before the next beat (steps 3/7/11/15 of a 16-step bar).
        bool isWeakPosition(int stepWithinBar, int stepsPerBar)
        {
            const int stepsPerBeat = std::max(1, stepsPerBar / 4);
            return (stepWithinBar % stepsPerBeat) == (stepsPerBeat - 1);
        }

        // Probability MULTIPLIER, not a probability itself - 1.0 at
        // syncopation=0 (no bias), up to 2x at weak positions / down to
        // 0.5x elsewhere at syncopation=1.
        float weakPositionBias(int stepWithinBar, int stepsPerBar, float syncopation)
        {
            return isWeakPosition(stepWithinBar, stepsPerBar)
                       ? (1.0f + syncopation)
                       : (1.0f - syncopation * 0.5f);
        }

        float clamp01(float v)
        {
            return std::max(0.0f, std::min(1.0f, v));
        }

        // Structural positions kick/hat/clap claim in Drop mode - their
        // FIXED idiom positions (four-on-the-floor beats, the offbeat-8th
        // hat pulse, the clap backbeat). Perc's motif is built avoiding
        // these entirely, so it answers the groove from genuine negative
        // space instead of layering on top of what's already sounding.
        bool isStructurallyClaimed(int stepInBar, int stepsPerBar)
        {
            const int stepsPerBeat = std::max(1, stepsPerBar / 4);
            if (stepInBar % stepsPerBeat == 0)               return true; // kick's four-on-the-floor beats
            if (stepInBar % stepsPerBeat == stepsPerBeat / 2) return true; // hat's primary offbeat pulse
            if (stepInBar == (stepsPerBar * 3) / 4)           return true; // clap's backbeat
            return false;
        }

        using MotifHit = std::pair<int, float>; // (stepInBar, velocity)

        // Copies `motif` (a set of fixed stepInBar positions + velocities)
        // into `barCount` consecutive bars starting at `barStart` -
        // literal repetition, not a fresh roll per bar. This is the
        // mechanism that makes bars 1-4 and 5-8 (etc.) recognizably the
        // same musical idea: they're not just similarly-shaped, they're
        // the identical motif placed at a different bar offset. Never
        // overwrites a hit another role/layer already placed at that
        // step (checked by the caller via steps[].active where that
        // matters, e.g. the hat foundation taking priority over its own
        // supporting layer).
        void applyMotif(StepArray& steps, int barStart, int barCount, int numBars, int stepsPerBar,
                         const std::vector<MotifHit>& motif, bool skipIfActive)
        {
            for (int i = 0; i < barCount; ++i)
            {
                const int bar = barStart + i;
                if (bar >= numBars)
                    break;
                const int base = bar * stepsPerBar;
                for (auto& hit : motif)
                {
                    const int step = base + hit.first;
                    if (skipIfActive && steps[(size_t) step].active)
                        continue;
                    setHit(steps, step, hit.second);
                }
            }
        }

        // Copies one full kBlockBars-bar block literally into `barCount`
        // consecutive destination bars (wrapping through the block's own
        // bars if barCount exceeds kBlockBars, e.g. the 3-bar
        // maintain-section copy) - this is what makes a repeat group
        // byte-identical to its source block, rather than each
        // destination bar re-deriving its own version.
        void copyBlock(StepArray& steps, int destBarStart, int barCount, int numBars, int stepsPerBar,
                        const StepArray& block)
        {
            const int blockBars = (int) block.size() / stepsPerBar;
            for (int i = 0; i < barCount; ++i)
            {
                const int destBar = destBarStart + i;
                if (destBar >= numBars)
                    break;
                const int srcBar   = i % blockBars;
                const int srcBase  = srcBar * stepsPerBar;
                const int destBase = destBar * stepsPerBar;
                for (int s = 0; s < stepsPerBar; ++s)
                    steps[(size_t) (destBase + s)] = block[(size_t) (srcBase + s)];
            }
        }
    }

    StepArray generateKick(const StepGridConfig& grid, const DrumPatternParams& params, DrumSection section)
    {
        switch (section)
        {
            case DrumSection::Drop: break; // only section implemented so far
        }

        const int total = totalSteps(grid);
        StepArray steps((size_t) total);
        const int stepsPerBeat = std::max(1, grid.stepsPerBar / 4);

        // Uncompromising four-on-the-floor - the dominant, most reliable
        // element in the pattern. Consistent velocity with a subtle
        // phrase accent (the start of each 4-bar group sits marginally
        // hotter), not per-step randomness - this role doesn't roll dice.
        for (int bar = 0; bar < grid.numBars; ++bar)
        {
            const int base = bar * grid.stepsPerBar;
            const bool isPhraseDownbeat = (bar % 4 == 0);
            const float vel = isPhraseDownbeat ? 1.0f : 0.92f;
            for (int beat = 0; beat < 4; ++beat)
                setHit(steps, base + beat * stepsPerBeat, vel);
        }

        // Bar 16 restrained phrase-ending push: a single deterministic
        // pushed hit on the weakest 16th, driving into the loop restart.
        // Gated by variation (0 = kick stays perfectly steady through the
        // last bar too), not a coin flip - this IS the drop's kick-side
        // phrase-ending gesture when enabled, always present, not random.
        if (grid.numBars > 0 && params.variation > 0.0f)
        {
            const int lastBar = grid.numBars - 1;
            setHit(steps, lastBar * grid.stepsPerBar + grid.stepsPerBar - 2, 0.85f);
        }

        return steps;
    }

    StepArray generateClap(const StepGridConfig& grid, const DrumPatternParams& params, DrumSection section)
    {
        switch (section)
        {
            case DrumSection::Drop: break;
        }

        const int total = totalSteps(grid);
        StepArray steps((size_t) total);
        std::mt19937 rng = makeRng(params.seed, kClapSalt);

        // Traditional backbeat position (beat 3). The sourced asymmetric
        // single/doublet-per-4-bar-cycle shape IS the motif: by default
        // every cycle uses the identical canonical layout, which is what
        // makes bars 1-4 and 5-8 sound like the same idea.
        const int backbeatStep   = (grid.stepsPerBar * 3) / 4;
        const int doubletGapStep = std::max(1, grid.stepsPerBar / 8);

        // syncopation: occasional +/-1 step nudge off the canonical
        // position - kept restrained (the backbeat should sit rock-solid
        // in a drop, not wander), only lightly scaled up in later cycles.
        auto jitter = [&](int step, int cycleIndex, int cycleCount) -> int
        {
            const float lean = cycleCount > 1 ? (float) cycleIndex / (float) (cycleCount - 1) : 0.0f; // 0 (first cycle) .. 1 (last)
            if (uniform01(rng) < params.syncopation * 0.2f * (0.6f + 0.6f * lean))
                return step + (uniform01(rng) < 0.5f ? -1 : 1);
            return step;
        };

        int cycleIndex = 0;
        const int cycleCount = (grid.numBars + 3) / 4;
        for (int cycleStart = 0; cycleStart < grid.numBars; cycleStart += 4, ++cycleIndex)
        {
            // variation: probability THIS cycle swaps which bar gets the
            // single hit vs. the doublet - "occasional phrase variation,
            // but keep the drop stable", so kept low by design (the
            // caller's default variation is modest). At variation=0 this
            // never fires (uniform01 never returns a negative number);
            // at variation=1 it always fires - both boundaries exact.
            const bool swapped = uniform01(rng) < params.variation;
            const int  singleBarOffset  = swapped ? 3 : 1;
            const int  doubletBarOffset = swapped ? 1 : 3;

            const int singleBar  = cycleStart + singleBarOffset;
            const int doubletBar = cycleStart + doubletBarOffset;

            if (singleBar < grid.numBars)
            {
                const int base = singleBar * grid.stepsPerBar;
                setHit(steps, jitter(base + backbeatStep, cycleIndex, cycleCount), 1.0f);
            }
            if (doubletBar < grid.numBars)
            {
                const int base = doubletBar * grid.stepsPerBar;
                setHit(steps, jitter(base + backbeatStep, cycleIndex, cycleCount), 1.0f);
                setHit(steps, jitter(base + backbeatStep + doubletGapStep, cycleIndex, cycleCount), 0.8f);
            }
        }

        // density: rare extra ghost clap somewhere a bar would otherwise
        // leave silent - kept deliberately low-probability so this role
        // stays restrained and complements the kick rather than competing
        // with it, even at density=1.
        for (int bar = 0; bar < grid.numBars; ++bar)
        {
            if (uniform01(rng) >= params.density * 0.06f)
                continue;
            const int base = bar * grid.stepsPerBar;
            const int step = base + (int) (uniform01(rng) * (float) grid.stepsPerBar);
            setHit(steps, step, 0.45f);
        }

        return steps;
    }

    StepArray generateHat(const StepGridConfig& grid, const DrumPatternParams& params, DrumSection section)
    {
        switch (section)
        {
            case DrumSection::Drop: break;
        }

        const int total = totalSteps(grid);
        StepArray steps((size_t) total);
        std::mt19937 rng = makeRng(params.seed, kHatSalt);

        const int eighthStep  = std::max(1, grid.stepsPerBar / 8);
        const int stepsPerBar = grid.stepsPerBar;

        auto isPrimaryPulseStep = [&](int stepInBar) -> bool
        {
            for (int k = 0; k < 4; ++k)
                if (stepInBar == eighthStep * (2 * k + 1))
                    return true;
            return false;
        };

        // Layer 1 - primary offbeat pulse: always present, locked tightly
        // to the kick, every bar of the block. Can occasionally omit one
        // hit for groove (a real technique, not noise) - the omission
        // decision for all kBlockBars bars is made in this ONE pass
        // (see buildBlock below), not re-rolled per destination bar, so a
        // literal repeat (bars 0-3 copied into 4-7) stays byte-identical.
        auto buildFoundation = [&](StepArray& block, float lean)
        {
            const float omitP = 0.03f + 0.05f * lean; // 3% early -> 8% late
            for (int bar = 0; bar < kBlockBars; ++bar)
            {
                const int base = bar * stepsPerBar;
                const bool isPhraseDownbeat = (bar == 0); // block-relative bar 0 is always the phrase downbeat it represents
                for (int k = 0; k < 4; ++k)
                {
                    const int step = base + eighthStep * (2 * k + 1);
                    if (step >= base + stepsPerBar)
                        continue;
                    if (uniform01(rng) < omitP)
                        continue; // deliberately skipped this repeat

                    float accentVel = (k % 2 == 0) ? 0.95f : 0.8f;
                    if (isPhraseDownbeat)
                        accentVel = std::min(1.0f, accentVel + 0.05f);
                    setHit(block, step, accentVel);
                }
            }
        };

        // Layer 2/3 - supporting 16ths + ghost hits, built as a MOTIF:
        // a small (0-3), fixed set of positions chosen once, avoiding
        // wherever the offbeat pulse's canonical positions sit, weighted
        // toward the weakest 16ths. "Selective 16th-note movement...
        // never constant machine-gun activity."
        auto buildSupportMotif = [&](float density) -> std::vector<MotifHit>
        {
            std::vector<MotifHit> motif;
            const int maxPositions = std::max(0, (int) std::lround(density * 3.0f)); // 0-3, restrained by design
            int attempts = 0;
            while ((int) motif.size() < maxPositions && attempts < stepsPerBar * 3)
            {
                ++attempts;
                const int stepInBar = (int) (uniform01(rng) * (float) stepsPerBar);
                if (isPrimaryPulseStep(stepInBar))
                    continue;

                bool already = false;
                for (auto& m : motif)
                    if (m.first == stepInBar)
                        already = true;
                if (already)
                    continue;

                const float bias = weakPositionBias(stepInBar, stepsPerBar, params.syncopation);
                if (uniform01(rng) < clamp01(0.55f * bias))
                    motif.emplace_back(stepInBar, 0.35f + uniform01(rng) * 0.2f); // 0.35-0.55, clearly under the primary pulse
            }
            return motif;
        };

        // Ghost layer: same motif mechanism as the support layer (built
        // once, repeated), just a much lower acceptance rate and much
        // lower velocity - a quiet textural presence rather than a
        // rhythmic statement, but still part of the block's ONE repeated
        // idea, not fresh randomness per destination bar (which would
        // silently break the "bars 1-4 == bars 5-8" repetition this whole
        // design exists for).
        auto buildGhostMotif = [&](float acceptRate) -> std::vector<MotifHit>
        {
            std::vector<MotifHit> motif;
            for (int stepInBar = 0; stepInBar < stepsPerBar; ++stepInBar)
            {
                if (isPrimaryPulseStep(stepInBar))
                    continue;
                if (uniform01(rng) < acceptRate)
                    motif.emplace_back(stepInBar, 0.12f + uniform01(rng) * 0.1f); // 0.12-0.22 - textural, not rhythmic
            }
            return motif;
        };

        // Builds one complete kBlockBars-bar block (foundation + support
        // + ghost), consuming the RNG exactly once for the whole block -
        // this is what makes copyBlock() below produce byte-identical
        // repeats instead of each destination bar re-rolling its own
        // chance.
        auto buildBlock = [&](float lean, float supportDensity, float ghostRate) -> StepArray
        {
            StepArray block((size_t) (kBlockBars * stepsPerBar));
            buildFoundation(block, lean);
            const auto support = buildSupportMotif(supportDensity);
            const auto ghosts  = buildGhostMotif(ghostRate);
            applyMotif(block, 0, kBlockBars, kBlockBars, stepsPerBar, support, true);
            applyMotif(block, 0, kBlockBars, kBlockBars, stepsPerBar, ghosts, true);
            return block;
        };

        // Bars 1-4 establish the block; bars 5-8 repeat it literally -
        // the listener hears the same idea twice.
        const auto baseBlock = buildBlock(0.0f, params.density, 0.05f);
        copyBlock(steps, 0, 4, grid.numBars, stepsPerBar, baseBlock);
        copyBlock(steps, 4, 4, grid.numBars, stepsPerBar, baseBlock);

        // Bars 9-12 develop the block (variation-gated chance the support
        // layer gains one extra position, a later-phrase omission feel,
        // and a slightly busier ghost layer); 13-15 repeat that
        // development literally rather than rolling something new.
        const float devLean = grid.numBars > 1 ? 8.0f / (float) (grid.numBars - 1) : 0.0f;
        auto devBlock = buildBlock(devLean, params.density, 0.08f);
        if (uniform01(rng) < params.variation)
        {
            auto extra = buildSupportMotif(0.35f);
            if (!extra.empty())
                applyMotif(devBlock, 0, kBlockBars, kBlockBars, stepsPerBar, { extra.front() }, true);
        }
        copyBlock(steps, 8, 4, grid.numBars, stepsPerBar, devBlock);
        copyBlock(steps, 12, 3, grid.numBars, stepsPerBar, devBlock);

        // Bar 16: restrained fill - the establishing block's content plus
        // one clear accent, not a busy roll.
        if (grid.numBars > 0)
        {
            copyBlock(steps, grid.numBars - 1, 1, grid.numBars, stepsPerBar, baseBlock);
            const int base        = (grid.numBars - 1) * stepsPerBar;
            const int accentStep  = base + std::max(1, stepsPerBar / 8);
            if (!steps[(size_t) accentStep].active)
                setHit(steps, accentStep, 0.6f);
        }

        return steps;
    }

    StepArray generatePerc(const StepGridConfig& grid, const DrumPatternParams& params, DrumSection section)
    {
        switch (section)
        {
            case DrumSection::Drop: break;
        }

        const int total = totalSteps(grid);
        StepArray steps((size_t) total);
        std::mt19937 rng = makeRng(params.seed, kPercSalt);
        const int stepsPerBar = grid.stepsPerBar;

        // The groove-defining motif: a small (1-3) set of fixed positions
        // in genuine negative space (never on kick's beats, hat's offbeat
        // pulse, or clap's backbeat), weighted toward the weakest 16ths.
        // "Avoid placing percussion simply because a step is empty" -
        // this is a deliberately chosen handful of positions, not a
        // per-step coin flip across the whole bar.
        auto buildMotif = [&](float density) -> std::vector<MotifHit>
        {
            std::vector<MotifHit> motif;
            const int maxPositions = std::max(1, (int) std::lround(1.0f + density * 2.0f)); // 1-3 positions
            int attempts = 0;
            while ((int) motif.size() < maxPositions && attempts < stepsPerBar * 4)
            {
                ++attempts;
                const int stepInBar = (int) (uniform01(rng) * (float) stepsPerBar);
                if (isStructurallyClaimed(stepInBar, stepsPerBar))
                    continue;

                bool already = false;
                for (auto& m : motif)
                    if (m.first == stepInBar)
                        already = true;
                if (already)
                    continue;

                const float bias = weakPositionBias(stepInBar, stepsPerBar, params.syncopation);
                if (uniform01(rng) < clamp01(0.6f * bias))
                {
                    const float velRange = 0.3f + params.variation * 0.3f;
                    motif.emplace_back(stepInBar, std::min(1.0f, 0.4f + uniform01(rng) * velRange));
                }
            }
            return motif;
        };

        // Bars 1-4 establish, 5-8 repeat the identical motif.
        const auto baseMotif = buildMotif(params.density);
        applyMotif(steps, 0, 4, grid.numBars, stepsPerBar, baseMotif, false);
        applyMotif(steps, 4, 4, grid.numBars, stepsPerBar, baseMotif, false);

        // 9-12 develop (variation-gated chance of one more accent
        // position, still respecting negative space); 13-15 maintain
        // that development.
        auto devMotif = baseMotif;
        if (uniform01(rng) < params.variation)
        {
            auto extra = buildMotif(0.4f);
            if (!extra.empty())
            {
                bool duplicate = false;
                for (auto& m : devMotif)
                    if (m.first == extra.front().first)
                        duplicate = true;
                if (!duplicate)
                    devMotif.push_back(extra.front());
            }
        }
        applyMotif(steps, 8, 4, grid.numBars, stepsPerBar, devMotif, false);
        applyMotif(steps, 12, 3, grid.numBars, stepsPerBar, devMotif, false);

        // Bar 16: a single restrained accent (not a busy fill), gated by
        // variation, landing on a fresh negative-space position rather
        // than repeating the motif verbatim - "occasionally introduce a
        // phrase-ending accent."
        if (grid.numBars > 0 && params.variation > 0.0f)
        {
            const int lastBar = grid.numBars - 1;
            const int base    = lastBar * stepsPerBar;
            int chosenStep = -1;
            for (int attempt = 0; attempt < 6; ++attempt)
            {
                const int candidate = (int) (uniform01(rng) * (float) stepsPerBar);
                if (!isStructurallyClaimed(candidate, stepsPerBar))
                {
                    chosenStep = candidate;
                    break;
                }
            }
            if (chosenStep >= 0)
                setHit(steps, base + chosenStep, 0.75f + uniform01(rng) * 0.2f);
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
