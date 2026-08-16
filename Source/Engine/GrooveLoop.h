#pragma once

#include "DrumEngine.h"
#include "BassEngine.h"
#include <cstdint>
#include <vector>

// A deliberately SIMPLE, self-contained, flat 8-bar Melodic Techno groove
// generator - the "simplify before more sound-design/arrangement work"
// phase. Zero dependency on MusicIdentity.h/BreakdownArrangement.h/
// Arrangement.h - no energy arc, no Drop/Breakdown state machine, no
// 16-bar arrangement, no transition fills. The 16-bar loop-generator
// (Engine::generateMusicIdentity/generateCompactLoop) and the full
// long-form arrangement (Engine::generateArrangementDrop/
// generateArrangementBassPattern) are both untouched and still compile
// and pass their own tests - this file coexists alongside them, it does
// not replace or delete them.
//
// See MLPipeline/musical_target/sound_and_rhythm_diagnostic_pass4.md and
// the earlier melodic_techno_research.md/youtube_melodic_techno_research.md
// passes (already read for this change, not re-derived) for the evidence
// behind every composition choice below - summarized in this file's own
// .cpp comments at each decision point.
namespace Engine
{
    constexpr int kGrooveLoopBars         = 8;
    constexpr int kGrooveLoopStepsPerBar  = 16;
    constexpr int kGrooveLoopTotalSteps   = kGrooveLoopBars * kGrooveLoopStepsPerBar; // 128

    struct GrooveLoopParams
    {
        // 0.5 tracks the measured corpus's own baseline density (same
        // convention as DrumPatternParams/BassPatternParams) - not an
        // arbitrary midpoint.
        float    density     = 0.5f;
        float    syncopation = 0.3f;
        // How much bars 4-7 are allowed to develop from bars 0-3's
        // established block - 0 stays closest to a literal repeat, 1
        // develops the most. There is no separate "energy" knob here -
        // deliberately, see GrooveLoop.cpp's own comment on why this
        // phase uses ONE flat energy level, not an arc.
        float    variation   = 0.35f;
        uint32_t seed         = 0;
    };

    struct GrooveLoop
    {
        // Reuses DrumEngine.h's own DropPattern type directly (kick, clap,
        // hatClosed, hatOpen, percA, percB) - 128 steps (8 bars) per role,
        // not 256. No pad field - there is no breakdown/pad concept in
        // this phase.
        DropPattern          drum;
        std::vector<int8_t>  bass;                // 128 steps, BassEngine's kBassOffValue = silence
        std::vector<int8_t>  bassGateLengthSteps;  // 128 steps, parallel to bass, 0 = no explicit gate
    };

    // Pure, deterministic function: same params.seed always produces the
    // same GrooveLoop (same guarantee as generateMusicIdentity/
    // generateDrop) - required for "same seed -> byte-identical output."
    // See GrooveLoop.cpp for exactly which existing, already-tested
    // Engine:: building blocks each role reuses.
    GrooveLoop generateGrooveLoop(const GrooveLoopParams& params);
}
