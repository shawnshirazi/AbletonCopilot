#pragma once

#include "Grid.h"
#include <cstdint>
#include <vector>

// Phase 1 deterministic engine: Melodic Techno drum-pattern generation.
//
// A real generator, not a lookup table - default/prior shapes come from
// sourced melodic-techno production research already gathered earlier this
// session (Attack Magazine's drum breakdown of an actual deep/melodic
// progressive techno track, Native Instruments' production guide), not
// invented fresh. Zero JUCE dependency (see Theory.h for why).

namespace Engine
{
    struct Hit
    {
        bool  active   = false;
        float velocity = 1.0f; // 0..1
    };

    using StepArray = std::vector<Hit>; // sized to totalSteps(grid)

    struct DrumPatternParams
    {
        float    density     = 0.5f; // 0..1 overall busy-ness
        float    syncopation = 0.3f; // 0..1 off-grid/extra-hit placement amount
        float    variation   = 0.2f; // 0..1 bar-to-bar variation vs exact repetition
        uint32_t seed        = 0;    // same seed+params -> same pattern, reproducible
    };

    // Four-on-the-floor baseline (the genre-defining convention) - variation
    // can rarely drop a beat for a fill or add a syncopated push into the
    // next bar, but never the majority behaviour.
    StepArray generateKick(const StepGridConfig& grid, const DrumPatternParams& params);

    // Sparse, asymmetric per the sourced breakdown: a single hit at the
    // "2-bar mark" and a doublet flourish at the "4-bar mark" of each
    // 4-bar cycle - not a steady 2-and-4 backbeat.
    StepArray generateClap(const StepGridConfig& grid, const DrumPatternParams& params);

    // Closed-hat pulse on the off-beat 8ths, plus a second, velocity-varied
    // layer for organic dynamics (both sourced).
    StepArray generateHat(const StepGridConfig& grid, const DrumPatternParams& params);

    // Sparse syncopated accents, density-scaled.
    StepArray generatePerc(const StepGridConfig& grid, const DrumPatternParams& params);
}
