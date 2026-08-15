#pragma once

#include <cstdint>

// Shared musical context for a single bar, consumed by DrumEngine/BassEngine
// (and, later, MelodyEngine/HarmonyEngine - see MusicState::melodicEnergy/
// rootNote, present now specifically so those engines can plug in later
// without another state-model change). Zero JUCE dependency, same
// convention as every other Engine/ header.
//
// The point of this type existing at all: drums and bass previously made
// independent musical decisions (their own density/section logic, no shared
// notion of "where are we in the arrangement right now"). MusicState is the
// one thing both engines read to answer that question consistently - see
// Arrangement.h for how a full multi-section arrangement is built from a
// sequence of these, one per bar.
namespace Engine
{
    // Real section vocabulary, not invented: matches the state machine
    // found by parsing the actual Arrangement-view locators out of three
    // complete Melodic Techno reference tracks already in the user's
    // library (PML Arrangement Academy .als files) - see
    // MLPipeline/musical_target/melodic_techno_research.md section 3 and
    // arrangement_target.json's "state_machine" block. PreDrop and
    // BreakdownBuild in particular are real, distinct states those
    // reference tracks show as their own multi-bar sections ("Pre-Break",
    // "Buildup") - not collapsed into a single generic "build" the way a
    // generic EDM template would.
    enum class MusicSection
    {
        Intro,
        Establish,
        Build,
        PreDrop,
        Drop,
        Breakdown,
        BreakdownBuild,
        FinalDrop,
        Outro
    };

    struct MusicState
    {
        MusicSection section = MusicSection::Intro;

        int bar          = 0; // absolute bar index within the whole arrangement (0-based)
        int barInSection = 0; // 0-based bar index within the current section
        int barsInSection = 1;

        // Two independent axes, deliberately not collapsed into one -
        // Breakdown is the clearest case where they diverge (low energy,
        // high tension: quiet, but not relaxed - see the user's own
        // energy/tension worked example and melodic_techno_research.md's
        // Layton Giordani filter-automation-during-breakdown finding).
        float energy  = 0.0f; // 0..1, overall arrangement intensity
        float tension = 0.0f; // 0..1, anticipation/unresolved-ness independent of energy

        // Per-role energy - real reference-track evidence (the three .als
        // arrangements) shows elements entering/leaving at different times
        // as their OWN named sections ("Bass in", "Drums Reintroduction"),
        // not a single global switch flipping every role at once.
        float drumEnergy    = 0.0f;
        float bassEnergy    = 0.0f;
        float melodicEnergy = 0.0f; // unused by any engine yet - reserved for MelodyEngine/HarmonyEngine

        double   bpm      = 124.0;
        int      rootNote = 0; // semitone offset from C, shared key context
        uint32_t seed     = 0;
    };
}
