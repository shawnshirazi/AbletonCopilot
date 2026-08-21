#pragma once
#include <JuceHeader.h>
#include "SoundDnaLibrary.h"
#include "PluginProcessor.h"
#include <functional>
#include <vector>

// Ties the Sound DNA library together: real on-disk candidate discovery ->
// (skip if already learned) -> load (existing Capture round-trip for
// user-captured sounds, or SerumAutomation for real factory-library
// files, always independently VERIFIED, never assumed) -> standardized
// test render (PluginProcessor::renderStandardizedNote) -> analysis
// (SoundDnaLibrary::analyzeBuffer) -> persist. No second Serum-hosting
// system - drives whichever melodyVoices[] slot the caller already has
// loaded/open.
namespace SoundLibraryLearner
{
    struct Candidate
    {
        juce::File   file;        // the real, on-disk .SerumPreset file (or invalid File() for a "learn from Serum's own current browser position" automation-only candidate)
        juce::String role;        // "Bass"/"Melody"/"Pad" - organizational only, per the user's own "category only organizes the learning set" instruction
        juce::String sourceHint;  // human-readable provenance, e.g. "Factory/Bass/Reese/BA - Gnarly Reese.SerumPreset"
    };

    // Real, on-disk discovery under the given role's real Factory category
    // folders (Bass -> Factory/Bass/**; Melody -> Factory/{Lead,Pluck,Arp,
    // Seq}/**; Pad -> Factory/{Pad,Soundscape,String}/** - all folder
    // names verified to exist on this machine, not assumed). Sorted,
    // capped at maxPerCategory. Empty if the role is unrecognized or no
    // matching folder exists on this machine - never fabricated.
    std::vector<Candidate> discoverCandidates(const juce::String& role, int maxPerCategory,
                                               const juce::File& factoryRoot);

    struct ProgressEvent
    {
        juce::String message;
        int index = 0;
        int total = 0;
    };
    using ProgressCallback = std::function<void(const ProgressEvent&)>;

    struct BatchResult
    {
        int learned = 0;
        int failed  = 0;
        int skipped = 0; // already present in the library with status Success and a matching fingerprint
        std::vector<SoundDna::LearnedSound> sounds; // every attempted candidate this run, success or not
    };

    // The real end-to-end batch. `trackIndex` is whichever melody-voice
    // slot's Serum2 instance to drive (its Serum2 editor window must
    // ALREADY be open with a title containing `windowTitleHint`, e.g. via
    // the existing PluginEditor::openSerumWindowForTrack, before calling
    // this - this function never opens/creates that window itself).
    // Persists after EACH candidate (not just at the end), so closing
    // AbletonCopilot mid-batch never loses already-learned entries.
    BatchResult learnBatch(AbletonCopilotAudioProcessor& processor, int trackIndex,
                            const juce::String& role, int maxPerCategory,
                            const juce::String& windowTitleHint,
                            ProgressCallback onProgress = nullptr);
}
