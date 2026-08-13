#include "FeatureExtractor.h"
#include <cmath>
#include <algorithm>
#include <numeric>

//==============================================================================
// Biquad coefficient factories
//==============================================================================

FeatureExtractor::Biquad FeatureExtractor::makeHighShelf(double fs, double f0, double Q, double dBGain)
{
    // Audio EQ Cookbook high-shelf (Robert Bristow-Johnson)
    double A     = std::pow(10.0, dBGain / 40.0);
    double w0    = 2.0 * M_PI * f0 / fs;
    double cosW  = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * Q);
    double sqA   = std::sqrt(A);

    double b0 =    A * ((A+1) + (A-1)*cosW + 2.0*sqA*alpha);
    double b1 = -2*A * ((A-1) + (A+1)*cosW);
    double b2 =    A * ((A+1) + (A-1)*cosW - 2.0*sqA*alpha);
    double a0 =         (A+1) - (A-1)*cosW + 2.0*sqA*alpha;
    double a1 =     2.0*((A-1) - (A+1)*cosW);
    double a2 =         (A+1) - (A-1)*cosW - 2.0*sqA*alpha;

    Biquad bq;
    bq.b0 = b0/a0; bq.b1 = b1/a0; bq.b2 = b2/a0;
    bq.a1 = a1/a0; bq.a2 = a2/a0;
    return bq;
}

FeatureExtractor::Biquad FeatureExtractor::makeHighPass(double fs, double f0, double Q)
{
    double w0    = 2.0 * M_PI * f0 / fs;
    double cosW  = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * Q);

    double b0 =  (1.0 + cosW) * 0.5;
    double b1 = -(1.0 + cosW);
    double b2 =  (1.0 + cosW) * 0.5;
    double a0 =   1.0 + alpha;
    double a1 =  -2.0 * cosW;
    double a2 =   1.0 - alpha;

    Biquad bq;
    bq.b0 = b0/a0; bq.b1 = b1/a0; bq.b2 = b2/a0;
    bq.a1 = a1/a0; bq.a2 = a2/a0;
    return bq;
}

FeatureExtractor::Biquad FeatureExtractor::makeLowShelf(double fs, double f0, double Q, double dBGain)
{
    double A    = std::pow(10.0, dBGain / 40.0);
    double w0   = 2.0 * M_PI * f0 / fs;
    double cosW = std::cos(w0);
    double sqA  = std::sqrt(A);
    double alpha = std::sin(w0) / (2.0 * Q);

    double b0 =    A * ((A+1) - (A-1)*cosW + 2.0*sqA*alpha);
    double b1 =  2*A * ((A-1) - (A+1)*cosW);
    double b2 =    A * ((A+1) - (A-1)*cosW - 2.0*sqA*alpha);
    double a0 =        (A+1)  + (A-1)*cosW + 2.0*sqA*alpha;
    double a1 =  -2.0*((A-1)  + (A+1)*cosW);
    double a2 =        (A+1)  + (A-1)*cosW - 2.0*sqA*alpha;

    Biquad bq;
    bq.b0 = b0/a0; bq.b1 = b1/a0; bq.b2 = b2/a0;
    bq.a1 = a1/a0; bq.a2 = a2/a0;
    return bq;
}

FeatureExtractor::Biquad FeatureExtractor::makePeakingEQ(double fs, double f0, double Q, double dBGain)
{
    double A     = std::pow(10.0, dBGain / 40.0);
    double w0    = 2.0 * M_PI * f0 / fs;
    double alpha = std::sin(w0) / (2.0 * Q);
    double cosW  = std::cos(w0);

    double b0 =  1.0 + alpha * A;
    double b1 = -2.0 * cosW;
    double b2 =  1.0 - alpha * A;
    double a0 =  1.0 + alpha / A;
    double a1 = -2.0 * cosW;
    double a2 =  1.0 - alpha / A;

    Biquad bq;
    bq.b0 = b0/a0; bq.b1 = b1/a0; bq.b2 = b2/a0;
    bq.a1 = a1/a0; bq.a2 = a2/a0;
    return bq;
}

//==============================================================================
// Cooley-Tukey radix-2 FFT
//==============================================================================

void FeatureExtractor::performFFT(std::vector<std::complex<float>>& x)
{
    int N = (int)x.size();

    // Bit-reversal permutation
    for (int i = 1, j = 0; i < N; ++i)
    {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(x[i], x[j]);
    }

    // Butterfly stages
    for (int len = 2; len <= N; len <<= 1)
    {
        float angle = -2.0f * (float)M_PI / (float)len;
        std::complex<float> wlen(std::cos(angle), std::sin(angle));

        for (int i = 0; i < N; i += len)
        {
            std::complex<float> w(1.0f, 0.0f);
            for (int j = 0; j < len / 2; ++j)
            {
                auto u = x[i + j];
                auto v = x[i + j + len/2] * w;
                x[i + j]         = u + v;
                x[i + j + len/2] = u - v;
                w *= wlen;
            }
        }
    }
}

std::vector<float> FeatureExtractor::powerSpectrum(const float* samples, int numSamples, int fftSize)
{
    std::vector<std::complex<float>> buf(fftSize, {0.0f, 0.0f});
    int copyLen = std::min(numSamples, fftSize);

    for (int i = 0; i < copyLen; ++i)
    {
        float w = 0.5f * (1.0f - std::cos(2.0f * (float)M_PI * i / (float)(fftSize - 1)));
        buf[i] = {samples[i] * w, 0.0f};
    }

    performFFT(buf);

    int bins = fftSize / 2 + 1;
    std::vector<float> power(bins);
    for (int k = 0; k < bins; ++k)
        power[k] = std::norm(buf[k]);

    return power;
}

//==============================================================================
// Mono mix helper
//==============================================================================

std::vector<float> FeatureExtractor::mixToMono(const juce::AudioBuffer<float>& audio)
{
    int n   = audio.getNumSamples();
    int nCh = audio.getNumChannels();
    std::vector<float> mono(n, 0.0f);

    for (int ch = 0; ch < nCh; ++ch)
    {
        const float* data = audio.getReadPointer(ch);
        for (int i = 0; i < n; ++i)
            mono[i] += data[i];
    }

    float inv = 1.0f / (float)nCh;
    for (auto& s : mono) s *= inv;
    return mono;
}

//==============================================================================
// Entry point
//==============================================================================

AudioFeatures FeatureExtractor::extract(const juce::AudioBuffer<float>& audio, double sampleRate)
{
    AudioFeatures out;

    if (audio.getNumSamples() < 4096 || audio.getNumChannels() < 1)
        return out;

    out.lufs           = computeLUFS(audio, sampleRate);
    out.peakDb         = computePeakDb(audio);
    out.dynamicRangeDb = out.peakDb - out.lufs;
    out.bpm            = computeBpm(audio, sampleRate);
    out.key            = computeKey(audio, sampleRate);
    out.stereoWidth    = computeStereoWidth(audio);
    computeSpectral(audio, sampleRate, out);

    out.valid = true;
    return out;
}

//==============================================================================
// LUFS — simplified ITU-R BS.1770 (K-weighting, no gating gate for captures <30s)
//==============================================================================

float FeatureExtractor::computeLUFS(const juce::AudioBuffer<float>& audio, double sampleRate)
{
    // K-weighting: pre-filter (high shelf ~1682 Hz, +4 dB) then high-pass (38 Hz)
    // Exact BS.1770 parameters
    auto prefilter = makeHighShelf(sampleRate, 1681.974, 0.7071752, 3.99984);
    auto hpFilter  = makeHighPass(sampleRate, 38.13547, 0.5003270);

    double sumMeanSquare = 0.0;

    for (int ch = 0; ch < audio.getNumChannels(); ++ch)
    {
        Biquad s1 = prefilter, s2 = hpFilter;
        s1.reset(); s2.reset();

        const float* data = audio.getReadPointer(ch);
        int n = audio.getNumSamples();

        double ms = 0.0;
        for (int i = 0; i < n; ++i)
        {
            float s = s1.process(data[i]);
            s = s2.process(s);
            ms += (double)s * s;
        }
        sumMeanSquare += ms / n;
    }

    if (sumMeanSquare <= 1e-10)
        return -99.0f;

    return (float)(-0.691 + 10.0 * std::log10(sumMeanSquare));
}

//==============================================================================
// Peak level
//==============================================================================

float FeatureExtractor::computePeakDb(const juce::AudioBuffer<float>& audio)
{
    float peak = 0.0f;
    for (int ch = 0; ch < audio.getNumChannels(); ++ch)
    {
        const float* data = audio.getReadPointer(ch);
        for (int i = 0; i < audio.getNumSamples(); ++i)
            peak = std::max(peak, std::abs(data[i]));
    }
    return peak > 0.0f ? juce::Decibels::gainToDecibels(peak) : -99.0f;
}

//==============================================================================
// BPM — onset-strength (spectral flux) autocorrelation
//==============================================================================

float FeatureExtractor::computeBpm(const juce::AudioBuffer<float>& audio, double sampleRate)
{
    auto mono      = mixToMono(audio);
    int numSamples = (int)mono.size();

    const int fftSize = 1024;
    const int hopSize  = 512;
    const int bins     = fftSize / 2 + 1;
    const double frameHz = sampleRate / hopSize;

    std::vector<float> flux;
    std::vector<float> prevPower(bins, 0.0f);
    bool havePrev = false;

    for (int start = 0; start + fftSize <= numSamples; start += hopSize)
    {
        auto power = powerSpectrum(mono.data() + start, fftSize, fftSize);

        if (havePrev)
        {
            double sum = 0.0;
            for (int k = 0; k < bins; ++k)
            {
                double diff = std::sqrt((double)power[(size_t)k]) - std::sqrt((double)prevPower[(size_t)k]);
                if (diff > 0.0) sum += diff;
            }
            flux.push_back((float)sum);
        }

        prevPower = power;
        havePrev  = true;
    }

    // Need a few seconds of onset envelope for the lowest-BPM lag to fit twice.
    if ((int)flux.size() < (int)(frameHz * 4.0))
        return 0.0f;

    double meanFlux = std::accumulate(flux.begin(), flux.end(), 0.0) / (double)flux.size();
    for (auto& v : flux) v = (float)((double)v - meanFlux);

    const double minBpm = 50.0, maxBpm = 200.0;
    int minLag = (int)std::round((60.0 / maxBpm) * frameHz);
    int maxLag = (int)std::round((60.0 / minBpm) * frameHz);
    maxLag     = std::min(maxLag, (int)flux.size() - 1);

    if (minLag < 1 || minLag >= maxLag)
        return 0.0f;

    // Raw autocorrelation of an onset envelope reliably peaks at every
    // integer multiple of the true beat period, not just the fundamental
    // (e.g. a clap on every OTHER beat can make the half-tempo lag
    // correlate at least as strongly as the real tempo) - picking the
    // single highest-magnitude lag is a classic recipe for octave errors
    // (detecting half or double the real BPM). Standard fix (same one
    // librosa's default tempo estimator uses): weight each candidate by a
    // log-normal prior centered on a plausible tempo for this plugin's
    // actual genre target (melodic techno, ~124-132 BPM per
    // GenreProfiles.cpp) rather than trusting raw correlation alone. Wide
    // enough (1 octave std-dev) to still find genuinely different tempos,
    // not a hard clamp.
    constexpr double kPriorCenterBpm = 126.0;
    constexpr double kPriorSigmaOctaves = 1.0;

    double bestScore = -1.0;
    int    bestLag    = 0;

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double corr = 0.0;
        int count = (int)flux.size() - lag;
        for (int i = 0; i < count; ++i)
            corr += (double)flux[(size_t)i] * (double)flux[(size_t)(i + lag)];
        corr /= (double)count;

        if (corr <= 0.0)
            continue;

        const double bpm = 60.0 / ((double)lag / frameHz);
        const double octavesFromCenter = std::log2(bpm / kPriorCenterBpm) / kPriorSigmaOctaves;
        const double prior = std::exp(-0.5 * octavesFromCenter * octavesFromCenter);
        const double score = corr * prior;

        if (score > bestScore) { bestScore = score; bestLag = lag; }
    }

    if (bestLag <= 0)
        return 0.0f;

    return (float)(60.0 / ((double)bestLag / frameHz));
}

//==============================================================================
// Key — chroma correlation against Krumhansl-Schmuckler profiles
//==============================================================================

juce::String FeatureExtractor::computeKey(const juce::AudioBuffer<float>& audio, double sampleRate)
{
    auto mono      = mixToMono(audio);
    int numSamples = (int)mono.size();

    const int fftSize = 4096;
    const int hopSize = 2048;

    std::array<double, 12> chroma{};
    chroma.fill(0.0);
    int frameCount = 0;

    for (int start = 0; start + fftSize <= numSamples; start += hopSize)
    {
        auto power = powerSpectrum(mono.data() + start, fftSize, fftSize);

        for (int k = 1; k < (int)power.size(); ++k)
        {
            float freq = (float)k * (float)sampleRate / (float)fftSize;
            if (freq < 27.5f || freq > 4186.0f) continue;

            double midi       = 12.0 * std::log2((double)freq / 440.0) + 69.0;
            int    pitchClass = ((int)std::round(midi) % 12 + 12) % 12;
            chroma[(size_t)pitchClass] += power[(size_t)k];
        }
        ++frameCount;
    }

    if (frameCount == 0) return "Unknown";

    double total = 0.0;
    for (auto c : chroma) total += c;
    if (total <= 0.0) return "Unknown";
    for (auto& c : chroma) c /= total;

    // Krumhansl-Schmuckler tonal hierarchy profiles
    static const double major[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09,
                                       2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double minor[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53,
                                       2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
    static const char* notes[12]  = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

    // Pearson r between chroma and a circularly-shifted profile
    auto pearson = [&](const double* profile, int rootShift) -> double
    {
        double meanC = 0.0, meanP = 0.0;
        for (int i = 0; i < 12; ++i) { meanC += chroma[i]; meanP += profile[i]; }
        meanC /= 12.0; meanP /= 12.0;

        double num = 0.0, denC = 0.0, denP = 0.0;
        for (int i = 0; i < 12; ++i)
        {
            // Shift profile so profile[0] (tonic) aligns with chroma[rootShift]
            double dc = chroma[i] - meanC;
            double dp = profile[(i - rootShift + 12) % 12] - meanP;
            num  += dc * dp;
            denC += dc * dc;
            denP += dp * dp;
        }
        double den = std::sqrt(denC * denP);
        return den > 0.0 ? num / den : 0.0;
    };

    double bestCorr   = -2.0;
    int    bestRoot   = 0;
    bool   bestIsMaj  = true;

    for (int root = 0; root < 12; ++root)
    {
        double cMaj = pearson(major, root);
        double cMin = pearson(minor, root);

        if (cMaj > bestCorr) { bestCorr = cMaj; bestRoot = root; bestIsMaj = true;  }
        if (cMin > bestCorr) { bestCorr = cMin; bestRoot = root; bestIsMaj = false; }
    }

    return juce::String(notes[bestRoot]) + (bestIsMaj ? " major" : " minor");
}

//==============================================================================
// Spectral balance + centroid
//==============================================================================

void FeatureExtractor::computeSpectral(const juce::AudioBuffer<float>& audio,
                                        double sampleRate, AudioFeatures& out)
{
    auto mono      = mixToMono(audio);
    int numSamples = (int)mono.size();

    const int fftSize = 4096;
    const int hopSize = fftSize / 2;
    const int bins    = fftSize / 2 + 1;

    std::vector<double> avgPow(bins, 0.0);
    int frameCount = 0;

    for (int start = 0; start + fftSize <= numSamples; start += hopSize)
    {
        auto frame = powerSpectrum(mono.data() + start, fftSize, fftSize);
        for (int k = 0; k < bins; ++k)
            avgPow[k] += frame[k];
        ++frameCount;
    }

    if (frameCount == 0) return;
    for (auto& p : avgPow) p /= frameCount;

    double binHz = sampleRate / fftSize;

    auto binFor = [&](double hz) { return (int)std::min((double)(bins - 1), hz / binHz); };

    double subE = 0, lowE = 0, loMidE = 0, hiMidE = 0, airE = 0;
    double totalE = 0, wFreq = 0;

    for (int k = 1; k < bins; ++k)
    {
        double freq = k * binHz;
        double p    = avgPow[k];
        totalE     += p;
        wFreq      += freq * p;

        if      (k <= binFor(80.0))   subE   += p;
        else if (k <= binFor(250.0))  lowE   += p;
        else if (k <= binFor(2000.0)) loMidE += p;
        else if (k <= binFor(8000.0)) hiMidE += p;
        else                          airE   += p;
    }

    if (totalE > 0.0)
    {
        out.subPct             = (float)(subE   / totalE);
        out.lowPct             = (float)(lowE   / totalE);
        out.lowMidPct          = (float)(loMidE / totalE);
        out.highMidPct         = (float)(hiMidE / totalE);
        out.airPct             = (float)(airE   / totalE);
        out.spectralCentroidHz = (float)(wFreq  / totalE);
    }
}

//==============================================================================
// Stereo width — mid/side RMS ratio
//==============================================================================

float FeatureExtractor::computeStereoWidth(const juce::AudioBuffer<float>& audio)
{
    if (audio.getNumChannels() < 2) return 0.0f;

    const float* L = audio.getReadPointer(0);
    const float* R = audio.getReadPointer(1);
    int n = audio.getNumSamples();

    double midPow = 0.0, sidePow = 0.0;
    for (int i = 0; i < n; ++i)
    {
        double m = (L[i] + R[i]) * 0.5;
        double s = (L[i] - R[i]) * 0.5;
        midPow  += m * m;
        sidePow += s * s;
    }

    double total = midPow + sidePow;
    if (total <= 0.0) return 0.0f;

    // 0 = mono (no side energy), 1 = maximum width
    return (float)std::sqrt(sidePow / total);
}
