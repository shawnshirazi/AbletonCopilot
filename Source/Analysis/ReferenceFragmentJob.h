#pragma once
#include <JuceHeader.h>
#include <array>
#include <functional>
#include <vector>

// One drum role's reference-extracted pattern + real one-shot sample - see
// fragment_lib.extract_drum_patterns's output shape (KICK/SNARE/CLAP/HIHAT,
// matching DrumMachineComponent's rackIds). Frequency-band onset detection
// on a mixed drum stem, not a clean per-instrument transcription - snare
// and clap share identical data (nothing in the signal can tell them apart).
struct DrumRolePattern
{
    juce::String           rackId;
    std::array<bool, 128>  steps {};
    juce::File              sampleFile;
};

// Result of running extract_reference_fragments.py on a reference track.
struct ReferenceFragmentResult
{
    bool         success = false;
    int          fragmentCount = 0;
    juce::File   outputJson;   // valid only when success is true
    juce::String errorMessage; // set when success is false

    // Drum roles found (possibly empty even on success - a track with no
    // detectable drum stem/onsets just yields no entries here).
    std::vector<DrumRolePattern> drumPatterns;
};

// Tier 2: shells out to the offline Python pipeline (see
// MLPipeline/extract_reference_fragments.py) to isolate the bass + drum
// stems of a user-supplied reference track (via Demucs, run locally) and
// extract real bass fragments (transcription) and drum patterns/one-shots
// (frequency-band onset detection) into JSON shapes the C++ side parses
// (MelodyGridComponent::loadReferenceFragments,
// DrumMachineComponent::applyReferenceDrumPattern). This is a real,
// possibly multi-minute local job - kept off the message thread the same
// way every other background analysis in this codebase is (AudioAnalyzer,
// ReferenceTrackAnalyzer), not attempted in-process since the separation/
// transcription/onset-detection stack is Python, not something this plugin
// re-implements.
class ReferenceFragmentJob : private juce::Thread
{
public:
    ReferenceFragmentJob();
    ~ReferenceFragmentJob() override;

    // UI/message thread - fires the background job, posts the result via
    // callback on the message thread. No-ops (silently) if already running.
    void extract(const juce::File& sourceAudioFile, float bpm, int keyRootSemitone, bool isMinor,
                  std::function<void(const ReferenceFragmentResult&)> callback);

    bool isRunning() const noexcept { return isThreadRunning(); }

private:
    void run() override;

    juce::File   pendingSourceFile;
    float        pendingBpm = 126.0f;
    int          pendingKeyRoot = 0;
    bool         pendingIsMinor = true;
    std::function<void(const ReferenceFragmentResult&)> pendingCallback;
};
