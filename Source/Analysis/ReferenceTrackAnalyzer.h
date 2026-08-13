#pragma once
#include <JuceHeader.h>
#include <functional>
#include "FeatureExtractor.h"

// Result of analyzing a user-supplied reference audio file - same
// AudioFeatures shape AudioAnalyzer produces for live capture, plus the
// file identity so Tier 2 (ReferenceFragmentJob) can point transcription
// at the exact same file without the user picking it twice.
struct ReferenceAnalysisResult
{
    bool          valid = false;
    juce::String  displayName; // file name, no extension - for display only
    juce::File    sourceFile;
    AudioFeatures features;
};

// File-sourced counterpart to AudioAnalyzer: reads a whole audio file from
// disk instead of draining AudioAnalyzer's live-capture ring buffer, then
// runs it through the same FeatureExtractor::extract() every other analysis
// path already uses. Kept as its own class rather than generalizing
// AudioAnalyzer because that class's ring-buffer/snapshot machinery is
// specifically for live DAW capture and doesn't apply here.
class ReferenceTrackAnalyzer : private juce::Thread
{
public:
    ReferenceTrackAnalyzer();
    ~ReferenceTrackAnalyzer() override;

    // UI/message thread - fires background read+analysis, posts result via
    // callback on the message thread. No-ops (silently) if already running.
    void analyzeFile(const juce::File& file,
                      std::function<void(const ReferenceAnalysisResult&)> callback);

    bool isAnalyzing() const noexcept { return isThreadRunning(); }

private:
    void run() override;

    juce::File   pendingFile;
    std::function<void(const ReferenceAnalysisResult&)> pendingCallback;

    FeatureExtractor extractor;

    // Long reference tracks don't need to be read/analyzed in full - this
    // caps memory/CPU while still giving FeatureExtractor plenty of signal.
    static constexpr double kMaxAnalyzeSecs = 180.0;
};
