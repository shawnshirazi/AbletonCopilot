#pragma once

#include "DrumEngine.h"
#include "BassEngine.h"
#include <cstdint>
#include <vector>

// The shared musical identity for the "click Generate, immediately hear a
// looping Melodic Techno idea" workflow - built once per Generate click,
// then rendered two different ways (DROP / BREAKDOWN) without ever being
// regenerated. Reuses the EXISTING, already-tested generators as-is:
// Engine::generateDrop() (already a native, deterministic, seeded 16-bar/
// 256-step Drop - see DrumEngine.h) for drums, Engine::generateBassPattern()
// (deterministic, seeded, 8-bar/128-step) for bass, tiled twice to fill the
// same 16-bar loop. Zero JUCE dependency, same convention as the rest of
// Engine/ - testable in Source/Engine/tests/.
//
// Deliberately does NOT include a melody motif: the real melody generator
// (MelodyGridComponent::generateMelodyFromMotifShapes's extracted core, see
// Source/MusicTheory/MelodyMotifGenerator.h) already lives on the JUCE side
// (it depends on MelodyScorer/MelodicTechnoTheory, which are JUCE-linked
// today) - forcing it across that boundary into this zero-JUCE file isn't
// worth the risk of touching that working, research-informed generator
// just to relocate it. The JUCE layer (PluginEditor) composes this
// Engine::MusicIdentity with the melody array it generates separately into
// one editor-side "the current loop" bundle - see generateLoopClicked().
namespace Engine
{
    constexpr int kLoopBars           = 16;
    constexpr int kLoopStepsPerBar    = 16;
    constexpr int kLoopTotalSteps     = kLoopBars * kLoopStepsPerBar; // 256

    struct MusicIdentityParams
    {
        uint32_t seed     = 0;
        double   bpm      = 124.0;
        int      rootNote = 0;
        bool     isMinor  = true;
    };

    struct MusicIdentity
    {
        uint32_t seed     = 0;
        double   bpm      = 124.0;
        int      rootNote = 0;
        bool     isMinor  = true;

        // 16-bar/256-step, straight from Engine::generateDrop() - the
        // ALREADY-EXISTING native 16-bar Drop generator (4-stage energy
        // arc, measured-grammar-driven, explicit loop-seam design at bar
        // 16 - see DrumEngine.h/.cpp). Untouched, just finally wired to a
        // loop-shaped caller instead of only the long-form arrangement.
        DropPattern drumMotif;

        // 16-bar/256-step, from Engine::generateBassLoop16(drumMotif, ...)
        // - a native 4-block generator that develops across the full loop,
        // is really aware of this identity's own drumMotif occupancy, and
        // commits to ONE real corpus-transcribed rhythmic archetype (see
        // BassEngine.h/BassArchetype.h). generateBassPattern (the plain
        // 8-bar generator this replaced here) is untouched and still used
        // as-is by the manual-editing/arrangement paths.
        std::vector<int8_t> bassMotif;

        // Parallel to bassMotif - each onset's real intended hold duration
        // in steps (0 = no explicit gate). Only meaningful at a step where
        // bassMotif has an onset. See PluginProcessor's gate-length note-
        // off scheduling for what actually makes this audible.
        std::vector<int8_t> bassGateLengthSteps;
    };

    // seed/bpm/rootNote/isMinor drive BOTH generators consistently (same
    // seed -> same drum AND bass motif every time - see the caller's own
    // single juce::Random draw in PluginEditor::generateLoopClicked).
    MusicIdentity generateMusicIdentity(const MusicIdentityParams& params);

    enum class RenderMode { Drop, Breakdown };

    // Per-role mix suggestion for a given RenderMode - never a pattern
    // change. gain scales trigger velocity (1.0 = unchanged); muted stops
    // new notes being triggered entirely. Applied by the caller via
    // PluginProcessor::setGeneratedDrumRoleMuted/setGeneratedDrumRoleGain,
    // which gate at the SAME point the codebase already uses for
    // drumRowMuted/melody-voice mute (trigger-time, never touching stored
    // pattern/sample-selection state - see PluginProcessor.cpp).
    struct RoleMix
    {
        bool  muted = false;
        float gain  = 1.0f;
    };

    // What a given RenderMode actually changes about how identity is
    // heard. drum/bass are ALWAYS byte-identical to the identity's own
    // motifs - renderMode() never thins, rewrites, or regenerates them;
    // only the per-role RoleMix suggestions differ between modes. This is
    // what makes "switching modes doesn't regenerate motifs" true by
    // construction: a caller that only ever reads .drum/.bass from this
    // struct cannot observe a difference between modes there, by
    // definition, no matter how renderMode() is implemented internally.
    struct RenderedLoop
    {
        DropPattern          drum;
        std::vector<int8_t>  bass;
        std::vector<int8_t>  bassGateLengthSteps; // ALWAYS byte-identical to identity.bassGateLengthSteps, same guarantee as .bass

        RoleMix kick, clap, hatClosed, hatOpen, percA, percB;
        bool    bassMuted = false;
    };

    // Pure function: no RNG, no mutation of identity, safe to call
    // repeatedly on the same identity (e.g. every DROP/BREAKDOWN toggle)
    // with no side effects and no drift.
    RenderedLoop renderMode(const MusicIdentity& identity, RenderMode mode);
}
