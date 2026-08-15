#pragma once

#include "Arrangement.h"
#include <array>
#include <cstdint>
#include <vector>

// Deterministic Melodic Techno DROP bassline engine, driven by MEASURED
// data (Source/Engine/BassRhythmGrammar.h, generated from
// MLPipeline/drum_grammar/output/bass_grammar.json - real onset/pitch
// statistics from vendor-authored bassline MIDI files already in the
// user's library) - see generateBassPattern's own comment in
// BassEngine.cpp for the full design. Zero JUCE dependency, same
// convention as DrumEngine.h/Theory.h.
//
// Output shape deliberately matches AbletonCopilotAudioProcessor's
// existing melody-voice grid exactly (kBassSteps == kMelodySteps == 128,
// kBassOffValue == kMelodyOffValue == -128) so the caller (PluginEditor's
// Generate Bass button) can hand the result straight to
// processor.setMelodyPattern() on the existing "Bass" voice (track 0) -
// this reuses the ALREADY-hosted Serum2 instance and note-triggering path
// (see PluginProcessor.cpp's melody-voice block) rather than inventing a
// second one. There is deliberately no BassEngine-side Serum/plugin code
// at all - this file only ever produces note data.
namespace Engine
{
    struct BassPatternParams
    {
        // 0.5 tracks the measured groove-subset's own density
        // (kBassGrooveRhythm.meanNotesPerBar); not an arbitrary midpoint.
        float    density   = 0.5f;
        // How much bars 4-7 are allowed to develop from bars 0-3's motif
        // (see buildBassBlock/deriveBassBlock in BassEngine.cpp) - 0 stays
        // closest to a literal repeat, 1 develops the most.
        float    variation = 0.2f;
        uint32_t seed       = 0;
    };

    static constexpr int    kBassSteps    = 128; // matches AbletonCopilotAudioProcessor::kMelodySteps
    static constexpr int8_t kBassOffValue = -128; // matches AbletonCopilotAudioProcessor::kMelodyOffValue

    // The full 8-bar (128-step) monophonic Drop bassline: a 4-bar motif
    // (bars 0-3), built once from the measured groove data and a
    // synthetic four-on-the-floor kick reference (real measured
    // kick-avoidance - see BassRhythmGrammar.h's onKickFraction), then
    // copied into bars 4-7 with controlled, params.variation-scaled
    // development - the same motif-block-then-copy/develop mechanism
    // DrumEngine.cpp uses for its own roles, applied to one monophonic
    // pitched voice. Each entry is a semitone offset from the melody
    // voice's own root (added to 36 + keyRoot by PluginProcessor, same as
    // every other melody track), or kBassOffValue for silence.
    std::array<int8_t, kBassSteps> generateBassPattern(const BassPatternParams& params);

    // ------------------------------------------------------------------
    // Arrangement-aware generation - the primary entry point for a full
    // Melodic Techno arrangement (Intro through Outro, see Arrangement.h),
    // sharing the SAME MusicState timeline DrumEngine::generateArrangementDrop
    // consumes so drums and bass never drift out of sync with each other.
    // Reuses the exact same measured-data-driven mechanism as
    // generateBassPattern above (decideBassPosition/buildBassBlock/
    // deriveBassBlock - see BassEngine.cpp); additive, not a replacement -
    // generateBassPattern/kBassSteps are untouched.
    //
    // Density per 4-bar block comes from that block's own (averaged)
    // MusicState::bassEnergy, not a single fixed params.density - a
    // Breakdown block (bassEnergy ~0.05-0.10) naturally produces only
    // occasional notes ("largely disappear... possibly retain an
    // occasional root" per the brief), no special-case silence rule
    // needed. Kick-avoidance still uses BassEngine's own synthetic
    // four-on-the-floor reference (not the actual drum pattern's kick,
    // which may itself be silent during a Breakdown) - BassEngine
    // deliberately has no ordering dependency on drum generation, same
    // as generateBassPattern.
    //
    // Output length is dynamic (arrangement.totalBars() * 16), unlike
    // generateBassPattern's fixed 128 - the caller is responsible for
    // routing a dynamically-sized bass pattern to the audio engine (see
    // PluginProcessor's generated-bass-pattern path, which mirrors its
    // existing generated-drum-pattern path rather than reusing the
    // fixed-128-step melody-voice array manual editing depends on).
    std::vector<int8_t> generateArrangementBassPattern(const MusicArrangement& arrangement,
                                                         const BassPatternParams& params);
}
