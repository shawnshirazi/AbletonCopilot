#pragma once
#include <JuceHeader.h>
#include <complex>
#include <vector>
#include <array>

struct AudioFeatures
{
    float lufs               = -99.0f;
    float peakDb             = -99.0f;
    float dynamicRangeDb     = 0.0f;
    float bpm                = 0.0f;
    juce::String key;

    // Frequency band proportions (sum to ~1.0)
    float subPct             = 0.0f;   // 20-80 Hz
    float lowPct             = 0.0f;   // 80-250 Hz
    float lowMidPct          = 0.0f;   // 250-2k Hz
    float highMidPct         = 0.0f;   // 2k-8k Hz
    float airPct             = 0.0f;   // 8k+ Hz

    float stereoWidth        = 0.0f;   // 0 = mono, 1 = max width
    float spectralCentroidHz = 0.0f;

    bool  valid              = false;
};

class FeatureExtractor
{
public:
    // Transposed-direct-form II biquad -- public so PluginProcessor can reuse it
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;

        float process(float x) noexcept
        {
            double out = b0 * x + z1;
            z1 = b1 * x - a1 * out + z2;
            z2 = b2 * x - a2 * out;
            return (float)out;
        }

        void reset() noexcept { z1 = z2 = 0.0; }
    };

    AudioFeatures extract(const juce::AudioBuffer<float>& audio, double sampleRate);

    // Filter factories -- public for reuse in correction DSP
    static Biquad makeHighShelf(double fs, double f0, double Q, double dBGain);
    static Biquad makeHighPass(double fs, double f0, double Q);
    static Biquad makeLowShelf(double fs, double f0, double Q, double dBGain);
    static Biquad makePeakingEQ(double fs, double f0, double Q, double dBGain);

private:
    static void               performFFT(std::vector<std::complex<float>>& x);
    static std::vector<float>  powerSpectrum(const float* samples, int numSamples, int fftSize);
    static std::vector<float>  mixToMono(const juce::AudioBuffer<float>& audio);

    float        computeLUFS(const juce::AudioBuffer<float>& audio, double sampleRate);
    float        computePeakDb(const juce::AudioBuffer<float>& audio);
    // Onset-strength (spectral flux) autocorrelation over the 50-200 BPM lag
    // range. Returns 0 if the buffer's too short to get a confident read.
    // Only used for offline/file-based analysis - the live-capture path
    // overwrites this with the DAW's reported tempo, which is more accurate
    // when it's available.
    float        computeBpm(const juce::AudioBuffer<float>& audio, double sampleRate);
    juce::String computeKey(const juce::AudioBuffer<float>& audio, double sampleRate);
    void         computeSpectral(const juce::AudioBuffer<float>& audio,
                                 double sampleRate, AudioFeatures& out);
    float        computeStereoWidth(const juce::AudioBuffer<float>& audio);
};
