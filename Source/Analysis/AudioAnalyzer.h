#pragma once
#include <JuceHeader.h>
#include <vector>
#include <functional>
#include "FeatureExtractor.h"
#include "GenreProfiles.h"
#include "FeedbackEngine.h"

struct AnalysisResult
{
    AudioFeatures             features;
    std::vector<FeedbackItem> feedback;
    juce::String              genreId;
    juce::String              genreDisplayName;
    bool                      valid = false;
};

class AudioAnalyzer : private juce::Thread
{
public:
    AudioAnalyzer();
    ~AudioAnalyzer() override;

    void prepare(double sampleRate, int numChannels);

    // Audio thread only - lock-free circular write
    void pushSamples(const juce::AudioBuffer<float>& buffer) noexcept;

    // Audio thread only - clears the capture window for a fresh session
    void resetCapture() noexcept;

    // UI/message thread - fires background analysis, posts result via callback
    void triggerAnalysis(const juce::String& genreId,
                         float dawBpm,
                         std::function<void(const AnalysisResult&)> callback);

    bool isAnalyzing()     const noexcept { return isThreadRunning(); }
    int  bufferedSeconds() const noexcept;

private:
    void run() override;

    // Takes a linear snapshot of the most recent samplesLogged samples.
    // Safe to call from the analysis thread when transport is stopped.
    juce::AudioBuffer<float> snapshot() const;

    static constexpr int kMaxCaptureSecs = 60;

    double sampleRate  = 44100.0;
    int    numChannels = 2;

    // Circular buffer — written from the audio thread with atomics
    juce::AudioBuffer<float> ring;
    int                      ringCapacity = 0;
    std::atomic<int>         writePos     { 0 };
    std::atomic<int>         samplesLogged{ 0 }; // capped at ringCapacity; 0 after reset

    juce::String pendingGenreId;
    float        pendingDawBpm = 0.0f;
    std::function<void(const AnalysisResult&)> pendingCallback;

    FeatureExtractor extractor;
    FeedbackEngine   feedbackEngine;
};
