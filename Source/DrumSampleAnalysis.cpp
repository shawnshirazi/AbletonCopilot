#include "DrumSampleAnalysis.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    // Mixes to mono (simple average) so every measurement below is a
    // single number per sample, not per-channel - drum one-shots are
    // rarely meaningfully stereo-different for these purposes.
    std::vector<float> toMono(const juce::AudioBuffer<float>& buf)
    {
        std::vector<float> mono((size_t) buf.getNumSamples(), 0.0f);
        const int nCh = buf.getNumChannels();
        for (int ch = 0; ch < nCh; ++ch)
        {
            const float* d = buf.getReadPointer(ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
                mono[(size_t) i] += d[i];
        }
        if (nCh > 1)
            for (auto& v : mono)
                v /= (float) nCh;
        return mono;
    }

    // Time (in samples) from the start until |signal| first reaches
    // `fraction` of the overall peak - a cheap, deterministic proxy for
    // transient strength. Shorter = sharper/punchier attack.
    int attackTimeSamples(const std::vector<float>& mono, float peak, float fraction)
    {
        if (peak <= 0.0f)
            return (int) mono.size();

        const float threshold = peak * fraction;
        for (size_t i = 0; i < mono.size(); ++i)
            if (std::abs(mono[i]) >= threshold)
                return (int) i;
        return (int) mono.size();
    }

    float computeZeroCrossingRate(const std::vector<float>& mono, double sampleRate)
    {
        if (mono.size() < 2 || sampleRate <= 0.0)
            return 0.0f;

        int crossings = 0;
        for (size_t i = 1; i < mono.size(); ++i)
            if ((mono[i - 1] >= 0.0f) != (mono[i] >= 0.0f))
                ++crossings;

        const double durationSec = (double) mono.size() / sampleRate;
        return durationSec > 0.0 ? (float) (crossings / durationSec) : 0.0f;
    }

    // Autocorrelation pitch estimate over the sample's first ~100ms
    // (where a kick's fundamental is clearest, before it decays into
    // noise/silence) - searches lags corresponding to 30-300Hz, a range
    // that comfortably covers techno kick fundamentals without wasting
    // time on the rest of the audible spectrum this isn't meant to
    // characterize. Returns 0 (no clear pitch) if the best-lag
    // correlation is too weak to trust - expected and fine for
    // noise-heavy hats/percs/claps, "where practical" per the brief.
    float estimatePitchHz(const std::vector<float>& mono, double sampleRate)
    {
        if (sampleRate <= 0.0 || mono.empty())
            return 0.0f;

        const int windowSamples = std::min((int) mono.size(), (int) (sampleRate * 0.1));
        if (windowSamples < 64)
            return 0.0f;

        const int minLag = std::max(1, (int) (sampleRate / 300.0));
        const int maxLag = std::min(windowSamples - 1, (int) (sampleRate / 30.0));
        if (maxLag <= minLag)
            return 0.0f;

        double energy0 = 0.0;
        for (int i = 0; i < windowSamples; ++i)
            energy0 += (double) mono[(size_t) i] * (double) mono[(size_t) i];
        if (energy0 <= 1e-9)
            return 0.0f;

        int    bestLag = -1;
        double bestCorr = 0.0;

        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double corr = 0.0;
            for (int i = 0; i < windowSamples - lag; ++i)
                corr += (double) mono[(size_t) i] * (double) mono[(size_t) (i + lag)];

            const double normalized = corr / energy0;
            if (normalized > bestCorr)
            {
                bestCorr = normalized;
                bestLag  = lag;
            }
        }

        // A weak best-correlation means no convincing periodicity - noise/
        // transient-dominated material (most hats/percs), not a real pitch.
        constexpr double kMinConfidence = 0.35;
        if (bestLag <= 0 || bestCorr < kMinConfidence)
            return 0.0f;

        return (float) (sampleRate / (double) bestLag);
    }
}

Engine::DrumSampleFeatures analyzeDrumSample(const juce::File& file)
{
    Engine::DrumSampleFeatures features;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
        return features;

    juce::AudioBuffer<float> buf(juce::jmax(1, (int) reader->numChannels), (int) reader->lengthInSamples);
    reader->read(&buf, 0, (int) reader->lengthInSamples, 0, true, true);

    const auto mono = toMono(buf);

    float peak = 0.0f;
    double sumSq = 0.0;
    for (float v : mono)
    {
        peak = std::max(peak, std::abs(v));
        sumSq += (double) v * (double) v;
    }

    features.valid            = true;
    features.durationSec      = (double) reader->lengthInSamples / reader->sampleRate;
    features.peakAmplitude    = peak;
    features.rms              = mono.empty() ? 0.0f : (float) std::sqrt(sumSq / (double) mono.size());
    features.attackTimeMs     = (float) (attackTimeSamples(mono, peak, 0.9f) / reader->sampleRate * 1000.0);
    features.zeroCrossingRate = computeZeroCrossingRate(mono, reader->sampleRate);
    features.estimatedPitchHz = estimatePitchHz(mono, reader->sampleRate);

    return features;
}
