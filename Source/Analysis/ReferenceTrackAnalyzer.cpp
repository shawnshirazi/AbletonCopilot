#include "ReferenceTrackAnalyzer.h"

ReferenceTrackAnalyzer::ReferenceTrackAnalyzer() : juce::Thread("ReferenceTrackAnalyzer") {}

ReferenceTrackAnalyzer::~ReferenceTrackAnalyzer()
{
    stopThread(4000);
}

void ReferenceTrackAnalyzer::analyzeFile(const juce::File& file,
                                          std::function<void(const ReferenceAnalysisResult&)> callback)
{
    if (isThreadRunning())
        return;

    pendingFile     = file;
    pendingCallback = std::move(callback);
    startThread();
}

void ReferenceTrackAnalyzer::run()
{
    ReferenceAnalysisResult result;
    result.sourceFile  = pendingFile;
    result.displayName = pendingFile.getFileNameWithoutExtension();

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(pendingFile));
    if (reader == nullptr || threadShouldExit())
    {
        auto cb = pendingCallback;
        juce::MessageManager::callAsync([cb, result] { cb(result); });
        return;
    }

    const double sampleRate = reader->sampleRate;
    const int    srcChannels = (int) reader->numChannels;
    const int    maxSamples  = (int) std::round(kMaxAnalyzeSecs * sampleRate);
    const int    numSamples  = (int) juce::jmin((juce::int64) maxSamples, reader->lengthInSamples);

    juce::AudioBuffer<float> audio(juce::jmax(1, srcChannels), juce::jmax(1, numSamples));
    reader->read(&audio, 0, numSamples, 0, true, true);

    if (threadShouldExit())
        return;

    result.features = extractor.extract(audio, sampleRate);
    result.valid    = result.features.valid;

    auto cb = pendingCallback;
    juce::MessageManager::callAsync([cb, result] { cb(result); });
}
