#pragma once
#include <JuceHeader.h>
#include "Engine/DrumVoiceSynth.h"
#include <array>
#include <vector>

// Offline, per-role WAV export of the CURRENTLY GENERATED drum pattern -
// not a second generator. Every timing/gain/swing/voice-behaviour formula
// here is deliberately the SAME one PluginProcessor.cpp's real-time
// trigger loop already uses (Engine::stepTimeSeconds for swing,
// Engine::triggerDrumVoice/renderDrumVoice for the synth fallback - both
// already zero-JUCE, already deterministic, already tested), applied to a
// SNAPSHOT of the stored pattern/sample/mute/gain state instead of a live
// audio-thread loop. This deliberately does NOT drive the real
// processBlock() via a synthetic AudioPlayHead - processBlock also mixes
// melody/Serum2/analyzer state that has nothing to do with drum stems, so
// reusing it offline would risk side effects for no benefit. See the plan
// this was built from for the full architecture rationale.
//
// One real, disclosed simplification: live playback's kick/clap/hat/perc
// step trigger only fires once per host audio block (block-quantized), so
// its exact sample position jitters by up to one block's worth of samples
// depending on the host's buffer size at the time - not a stable "exact"
// target to match, since it changes with host settings. This renderer
// instead computes each hit's mathematically exact continuous-time
// position (the plugin's INTENDED deterministic timing), which is more
// precise than, and a superset of, what any specific live run produces -
// not a deviation from the pattern, a removal of an incidental
// host-buffer-size artifact that was never part of the pattern itself.
namespace DrumStemExporter
{
    // Roles are always index-matched to Engine::DrumRole (Kick=0 ...
    // PercB=5) - same convention as PluginProcessor's generatedDrumRoles/
    // generatedRoleSampleBuffers arrays, so a snapshot can be built with a
    // straight per-index copy.
    struct RoleInput
    {
        int                                        midiNote = -1;   // -1 = unused/no pattern for this role
        std::vector<int>                            velocity;        // 0 = no hit, 1-127 = velocity, same array setGeneratedDrumPattern stored
        std::shared_ptr<juce::AudioBuffer<float>>   sampleBuffer;     // null/empty -> Engine::DrumVoiceSynth fallback, same rule triggerGeneratedRole uses
        juce::File                                   loadedFile;       // the REAL final loaded path (PluginProcessor::generatedRoleLoadedFile) - NOT a candidate path
        bool                                          muted = false;
        float                                         gain  = 1.0f;
    };

    struct Input
    {
        std::array<RoleInput, (size_t) Engine::DrumRole::Count> roles;
        int    totalSteps = 0;    // Engine::kLoopTotalSteps in the shipped product (256), kept as data so tests can use smaller grids
        double bpm         = 124.0;
        double sampleRate  = 44100.0;
    };

    struct RoleResult
    {
        juce::AudioBuffer<float> audio;            // stereo, exact loop length, silence where no hit
        juce::String              roleLabel;         // "Kick"/"Clap"/"Closed Hat"/"Open Hat"/"Perc A"/"Perc B"
        juce::String              fileBaseName;       // "Kick"/"Clap"/"ClosedHat"/"OpenHat"/"PercA"/"PercB" - no spaces, for the .wav filename
        juce::File                 loadedFile;         // mirrors RoleInput::loadedFile, for the diagnostic block
        bool                        isSynthFallback = false;
    };

    using Result = std::array<RoleResult, (size_t) Engine::DrumRole::Count>;

    // Exact total sample count for the loop at the given bpm/sampleRate -
    // the same formula used to size every stem AND the reference mix, so
    // "all stems have identical duration" and "sample count matches 16
    // bars at the current BPM/sample rate" hold by construction, not by
    // coincidence.
    int computeTotalSamples(int totalSteps, double bpm, double sampleRate);

    // Pure, no file I/O, no RNG, no host/transport dependency - same input
    // always produces byte-identical output (required for "exporting
    // twice produces identical audio" and for this to be safely unit-
    // testable). Renders each of the 6 roles in isolation (only that
    // role's own velocity array is read) into its own stereo buffer.
    Result renderDrumStems(const Input& input);

    // Same onset placement as renderDrumStems, but every role's audio is
    // accumulated into ONE buffer instead of 6 separate ones - the
    // reconstruction-test reference: summing renderDrumStems' 6 buffers
    // must match this within a small float tolerance (both paths share
    // the exact same per-onset placement code, so this is a near-exact
    // match, not an approximation).
    juce::AudioBuffer<float> renderDrumMixReference(const Input& input);

    // ---- I/O layer (file writing - not exercised by the pure-rendering
    // tests above, only by the editor's real Export button) ----
    struct WriteResult
    {
        juce::File                                                outputDirectory;
        std::array<juce::File, (size_t) Engine::DrumRole::Count>  writtenFiles;
        bool                                                       allSucceeded = false;
        juce::String                                                errorMessage; // only meaningful if !allSucceeded
    };

    // Writes each RoleResult to "<baseFolder>/DrumStems_<timestamp>/<fileBaseName>.wav"
    // (24-bit WAV via juce::WavAudioFormat, same format/bit-depth
    // precedent as the existing DrumPatternRenderer.cpp) and returns the
    // real subfolder actually used plus each file actually written.
    // Synchronous (rendering itself is fast, pure buffer math - no sample
    // decode needed, everything's already decoded in RoleInput::
    // sampleBuffer) - the caller (PluginEditor) is expected to run this
    // off the message thread if desired, same as it already does for
    // other file-system work.
    WriteResult writeStemsToWav(const Result& stems, const juce::File& baseFolder, double sampleRate);
}
