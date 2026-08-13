#include "AudioAnalyzer.h"

AudioAnalyzer::AudioAnalyzer()
    : juce::Thread("AbletonCopilot_Analyzer")
{
}

AudioAnalyzer::~AudioAnalyzer()
{
    stopThread(3000);
}

void AudioAnalyzer::prepare(double sr, int nCh)
{
    stopThread(1000);

    sampleRate    = sr;
    numChannels   = nCh;
    ringCapacity  = (int)(sr * kMaxCaptureSecs);

    ring.setSize(nCh, ringCapacity);
    ring.clear();
    writePos.store(0);
    samplesLogged.store(0);
}

//==============================================================================
// Audio thread
//==============================================================================

void AudioAnalyzer::resetCapture() noexcept
{
    // Zero samplesLogged so the next snapshot sees no data from the previous session.
    // writePos keeps advancing - old ring data is overwritten naturally.
    samplesLogged.store(0, std::memory_order_release);
}

void AudioAnalyzer::pushSamples(const juce::AudioBuffer<float>& buffer) noexcept
{
    int n  = buffer.getNumSamples();
    int wp = writePos.load(std::memory_order_relaxed);

    // Write, wrapping around the ring
    int firstChunk  = std::min(n, ringCapacity - wp);
    int secondChunk = n - firstChunk;

    for (int ch = 0; ch < std::min(buffer.getNumChannels(), numChannels); ++ch)
    {
        ring.copyFrom(ch, wp, buffer.getReadPointer(ch), firstChunk);
        if (secondChunk > 0)
            ring.copyFrom(ch, 0, buffer.getReadPointer(ch) + firstChunk, secondChunk);
    }

    writePos.store((wp + n) % ringCapacity, std::memory_order_release);

    // Track how much new audio is available, capped at capacity
    int prev = samplesLogged.load(std::memory_order_relaxed);
    int next = std::min(prev + n, ringCapacity);
    samplesLogged.store(next, std::memory_order_release);
}

int AudioAnalyzer::bufferedSeconds() const noexcept
{
    return (int)(samplesLogged.load(std::memory_order_relaxed) / sampleRate);
}

//==============================================================================
// Snapshot (called from analysis thread after transport stops)
//==============================================================================

juce::AudioBuffer<float> AudioAnalyzer::snapshot() const
{
    // Read both atomics close together - minor race is harmless (few samples off)
    int available = samplesLogged.load(std::memory_order_acquire);
    int wp        = writePos.load(std::memory_order_acquire);

    if (available <= 0)
        return juce::AudioBuffer<float>(numChannels, 0);

    // The most recent 'available' samples end just before wp
    int startPos    = (wp - available + ringCapacity) % ringCapacity;
    int firstChunk  = std::min(available, ringCapacity - startPos);
    int secondChunk = available - firstChunk;

    juce::AudioBuffer<float> out(numChannels, available);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        out.copyFrom(ch, 0, ring.getReadPointer(ch) + startPos, firstChunk);
        if (secondChunk > 0)
            out.copyFrom(ch, firstChunk, ring.getReadPointer(ch), secondChunk);
    }
    return out;
}

//==============================================================================
// UI thread - trigger
//==============================================================================

void AudioAnalyzer::triggerAnalysis(const juce::String& genreId,
                                     float dawBpm,
                                     std::function<void(const AnalysisResult&)> callback)
{
    if (isThreadRunning()) return;

    pendingGenreId  = genreId;
    pendingDawBpm   = dawBpm;
    pendingCallback = std::move(callback);
    startThread();
}

//==============================================================================
// Background thread
//==============================================================================

void AudioAnalyzer::run()
{
    auto audio = snapshot();

    if (audio.getNumSamples() < (int)(sampleRate * 3.0))
    {
        AnalysisResult r;
        auto cb = pendingCallback;
        juce::MessageManager::callAsync([cb, r] { cb(r); });
        return;
    }

    if (threadShouldExit()) return;

    AudioFeatures features = extractor.extract(audio, sampleRate);

    if (threadShouldExit()) return;

    AnalysisResult result;
    result.genreId = pendingGenreId;

    const GenreProfile* profile = GenreProfiles::getInstance().getProfile(pendingGenreId);
    if (profile && features.valid)
    {
        features.bpm            = pendingDawBpm; // use DAW session tempo, not estimated
        result.features         = features;
        result.feedback         = feedbackEngine.generate(features, *profile);
        result.genreDisplayName = profile->displayName;
        result.valid            = true;
    }

    auto cb = pendingCallback;
    juce::MessageManager::callAsync([cb, result] { cb(result); });
}
