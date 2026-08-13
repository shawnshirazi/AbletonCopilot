#include "DrumPatternRenderer.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr double kSampleRate = 44100.0;
    constexpr int    kNumSteps   = 128; // 8 bars x 16th notes — must match DrumMachineComponent::kNumSteps
}

DrumPatternRenderer::DrumPatternRenderer() : juce::Thread("DrumPatternRenderer") {}

DrumPatternRenderer::~DrumPatternRenderer()
{
    stopThread(4000);
}

void DrumPatternRenderer::render(std::vector<DrumHitRow> rowsToRender, float bpm,
                                  DrumRenderCallback callback)
{
    if (isThreadRunning())
        return;

    pendingRows     = std::move(rowsToRender);
    pendingBpm      = bpm > 0.0f ? bpm : 124.0f;
    pendingCallback = std::move(callback);

    startThread();
}

void DrumPatternRenderer::run()
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    auto outDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                      .getChildFile("AbletonCopilot/Session/DrumStack/MelodicTechno");
    if (outDir.exists())
        outDir.deleteRecursively();
    outDir.createDirectory();

    const double secPerStep   = (60.0 / (double) pendingBpm) / 4.0; // one 16th note
    const int    totalSamples = (int) std::round(secPerStep * kNumSteps * kSampleRate);

    std::vector<StackStem> stems;
    juce::String log;

    for (auto& row : pendingRows)
    {
        if (threadShouldExit())
            return;

        const bool anyStep = std::any_of(row.steps.begin(), row.steps.end(),
                                          [](bool b) { return b; });
        if (!row.sampleFile.existsAsFile() || !anyStep)
            continue;

        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(row.sampleFile));
        if (reader == nullptr)
        {
            log << "Could not read " << row.sampleFile.getFullPathName() << "\n";
            continue;
        }

        const int srcChannels = (int) reader->numChannels;
        const int srcLength   = (int) reader->lengthInSamples;

        juce::AudioBuffer<float> src(juce::jmax(1, srcChannels), juce::jmax(1, srcLength));
        reader->read(&src, 0, srcLength, 0, true, true);

        juce::AudioBuffer<float> out(2, totalSamples);
        out.clear();

        for (int step = 0; step < kNumSteps; ++step)
        {
            if (!row.steps[(size_t) step])
                continue;

            const int start = (int) std::round(step * secPerStep * kSampleRate);
            if (start >= totalSamples)
                continue;

            const int copyLen = juce::jmin(srcLength, totalSamples - start);
            for (int ch = 0; ch < 2; ++ch)
            {
                const int srcCh = juce::jmin(ch, srcChannels - 1);
                out.addFrom(ch, start, src, srcCh, 0, copyLen);
            }
        }

        const float peak = out.getMagnitude(0, totalSamples);
        if (peak > 0.98f)
            out.applyGain(0.98f / peak);

        auto outFile = outDir.getChildFile(row.elementName + ".wav");
        std::unique_ptr<juce::FileOutputStream> stream(outFile.createOutputStream());
        if (stream == nullptr)
        {
            log << "Could not write " << outFile.getFullPathName() << "\n";
            continue;
        }

        stream->setPosition(0);
        stream->truncate();

        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(stream.get(), kSampleRate, 2, 24, {}, 0));
        if (writer == nullptr)
        {
            log << "Could not encode " << outFile.getFullPathName() << "\n";
            continue;
        }

        stream.release(); // writer now owns the stream
        writer->writeFromAudioSampleBuffer(out, 0, totalSamples);
        stems.push_back({ row.elementName, outFile });
    }

    if (threadShouldExit())
        return;

    const bool ok = !stems.empty();
    if (!ok && log.isEmpty())
        log = "No samples assigned to any drum row yet.";

    auto callback = pendingCallback;
    juce::MessageManager::callAsync([callback, ok, log, stems]() mutable
    {
        if (callback)
            callback(ok, log, std::move(stems));
    });
}
