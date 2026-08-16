#pragma once

#include "MusicIdentity.h"
#include <cstdint>
#include <vector>

// A 3-phase breakdown/arrangement state machine sitting ALONGSIDE
// MusicIdentity's existing RenderMode{Drop,Breakdown} - it does not touch
// RenderMode, renderMode(), or any Drop-path generation. DROP keeps its
// own, already-tested, unmodified path (Engine::renderMode(identity,
// RenderMode::Drop)).
//
// Evidence base: MLPipeline/musical_target/melodic_techno_research.md
// section 11.4 (the reference-arrangement-derived Breakdown Grammar,
// cross-validated across 3 real .als arrangements) and
// MLPipeline/musical_target/melodic_techno_production_grammar_v2.md
// (cross-referenced against YouTube/production-technique research). Every
// design decision below is either a HIGH-confidence hard constraint, a
// MEDIUM-confidence weighted default, or an explicitly-disclosed
// interpretation where the research left something unresolved - see the
// per-decision comments and the plan this was built from.
//
// BREAK_ENTRY -> BREAK_BODY -> PRE_DROP -> (existing, untouched DROP)
namespace Engine
{
    enum class BreakdownPhase { Entry, Body, PreDrop };

    // Bar lengths: medians from the measured range across 3 real reference
    // arrangements (Entry 4-8, Body 16-24, PreDrop/Development 4-8 bars -
    // see melodic_techno_research.md section 11.4). 32 bars total, a
    // separate loop length from the existing 16-bar Drop loop.
    constexpr int kBreakdownEntryBars = 8;
    constexpr int kBreakdownBodyBars  = 16;
    constexpr int kPreDropBars        = 8;
    constexpr int kBreakdownStepsPerBar = 16; // matches kLoopStepsPerBar

    constexpr int8_t kPadOffValue = -128; // same convention as kBassOffValue/kMelodyOffValue

    // Which phase a given bar (0-based, counted from the start of the
    // whole 32-bar breakdown) falls into, and the total bar count.
    BreakdownPhase phaseForBar(int barIndexWithinBreakdown);
    int totalBreakdownBars();
    int barsInPhase(BreakdownPhase phase);

    // The dedicated melodic/harmonic material a breakdown introduces - the
    // single highest-leverage missing capability identified by the
    // research (this project's engine previously only ever muted existing
    // roles during a Breakdown; it never added a new musical presence).
    // Same array shape/convention as MusicIdentity::bassMotif/
    // bassGateLengthSteps: one entry per 16th step across the phase's own
    // bars, kPadOffValue where silent.
    struct PadMotif
    {
        std::vector<int8_t> pitchOffsets;
        std::vector<int8_t> gateLengthSteps;
    };

    // What a given BreakdownPhase actually sounds like, mirroring
    // RenderedLoop's RoleMix shape so the same
    // setGeneratedDrumRoleMuted/Gain mechanism the Drop/Breakdown toggle
    // already uses can apply this too, later. kick/clap/hatClosed/
    // hatOpen/percA/percB and bassMuted are never a pattern rewrite - see
    // MusicIdentity.h's own RoleMix comment for why that guarantee
    // matters. pad is the new content this state machine actually adds.
    struct BreakdownRenderedLoop
    {
        RoleMix kick, clap, hatClosed, hatOpen, percA, percB;
        bool    bassMuted = true;
        PadMotif pad;
    };

    // Pure function: same identity + phase always produces the same
    // output (no RNG beyond a deterministic seed-derived stream, no
    // mutation) - same guarantee as Engine::renderMode(), required for
    // "the same generated loop is repeatable when the loop restarts."
    // barsOverride lets a caller ask for a phase's content at a DIFFERENT
    // length than the standalone 32-bar grammar's own kBreakdownEntryBars/
    // kBreakdownBodyBars/kPreDropBars constants (see generateCompactLoop
    // below, which embeds a compressed Entry/Body/PreDrop span inside one
    // 16-bar loop) - the character (sustained vs. tension-building etc.)
    // stays the same, only the bar count changes.
    BreakdownRenderedLoop renderBreakdownPhase(const MusicIdentity& identity, BreakdownPhase phase, int barsOverride);
    BreakdownRenderedLoop renderBreakdownPhase(const MusicIdentity& identity, BreakdownPhase phase);

    // Exposed directly (not just through renderBreakdownPhase) so tests
    // can check the pad content in isolation. Salts identity.seed with a
    // 'PAD' constant, the same convention DrumEngine.cpp ('DROP') and
    // BassEngine.cpp/BassArchetype.cpp ('BASS'/'ARCH') already use for
    // independent-but-deterministic per-subsystem RNG streams from one
    // shared MusicIdentity seed.
    PadMotif generateBreakdownPad(uint32_t seed, BreakdownPhase phase, int barsOverride);
    PadMotif generateBreakdownPad(uint32_t seed, BreakdownPhase phase);

    // ------------------------------------------------------------------
    // The single 16-bar "hear the whole arc on one Generate click" loop -
    // the actual playback-facing shape, built by STITCHING (not
    // regenerating) the existing Drop pattern with a compressed
    // Entry/Body/PreDrop breakdown span:
    //
    //   bars 0-7  (steps 0-127):   DROP - bassMotif's own bars 0-7,
    //                              byte-identical, untouched; drumMotif's
    //                              own 4-stage Establish/Develop/Increase/
    //                              FullDrop arc (which spans generateDrop()'s
    //                              full 16 bars) compressed 2-bars-per-stage
    //                              into this 8-bar span (see
    //                              compressDropArcInto, BreakdownArrangement.cpp)
    //                              so the played loop actually reaches
    //                              full-drop drum energy, not just
    //                              Establish+Develop - a fix for the
    //                              stage-arc-truncation bug documented in
    //                              MLPipeline/musical_target/
    //                              sound_and_rhythm_diagnostic_pass4.md
    //   bar  8    (steps 128-143): BREAK_ENTRY (1 bar)
    //   bars 9-11 (steps 144-191): BREAK_BODY (3 bars)
    //   bars 12-15(steps 192-255): PRE_DROP (4 bars)
    //   (the loop wrapping back to bar 0 IS the drop-return - no separate
    //    transition step needed)
    //
    // This compresses the full research-measured 8/16/8-bar breakdown
    // grammar down to fit one 16-bar loop - a disclosed design necessity
    // for "GENERATE -> immediately hear the complete loop," not a claim
    // that 1/3/4 bars is itself a measured proportion.
    enum class CompactSection { Drop, BreakEntry, BreakBody, PreDrop };

    constexpr int kCompactLoopBars       = 16;
    constexpr int kCompactDropBars       = 8;
    constexpr int kCompactEntryBars      = 1;
    constexpr int kCompactBodyBars       = 3;
    constexpr int kCompactPreDropBars    = 4;

    // bar: 0-based bar index within the 16-bar compact loop (wraps via
    // modulo internally, so any bar/step derived from a wrapped playhead
    // position is safe to pass directly).
    CompactSection compactSectionForBar(int bar);

    struct CompactLoop
    {
        DropPattern          drum;                // 256 steps - Drop bars 0-7 (compressed 4-stage arc, see generateCompactLoop), muted-role-zeroed bars 8-15
        std::vector<int8_t>  bass;                 // 256 steps - Drop bars 0-7, off bars 8-15
        std::vector<int8_t>  bassGateLengthSteps;   // 256 steps, parallel to bass
        PadMotif              pad;                  // 256 steps - off bars 0-7, breakdown content bars 8-15
    };

    // Pure function: same identity always produces the same CompactLoop
    // (same guarantee as generateMusicIdentity/renderMode/
    // renderBreakdownPhase - required for "the same generated loop is
    // repeatable when the loop restarts"). Calls no DrumEngine.cpp/
    // BassEngine.cpp/MusicIdentity.cpp generation function beyond what
    // identity already contains - bars 0-7 are proven unmodified by
    // construction (see test_breakdown_arrangement.cpp).
    CompactLoop generateCompactLoop(const MusicIdentity& identity);
}
