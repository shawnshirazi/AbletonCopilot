#pragma once

// Phase 1 deterministic engine: 16th-note step grid representation.
//
// Zero JUCE dependency (see Theory.h for why). Generic and reusable - not
// drum-specific; the bass/melody engines planned for later passes use the
// same grid.

namespace Engine
{
    struct StepGridConfig
    {
        int   stepsPerBar = 16;
        int   numBars     = 8;
        // 0 = straight, 1 = full triplet feel - Ableton's own swing
        // convention, already sourced/used elsewhere in this plugin
        // (only the off-beat 16th of each 8th-note pair is delayed).
        float swing       = 0.0f;
    };

    int totalSteps(const StepGridConfig& config);

    // Absolute time in seconds of the given step (0-based, may exceed one
    // bar) at the given tempo, applying swing to off-beat 16ths.
    double stepTimeSeconds(int step, double bpm, const StepGridConfig& config);
}
