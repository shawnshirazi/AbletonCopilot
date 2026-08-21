// Regression tests for Source/CandidateEvaluator.h - the audio-domain
// candidate-comparison layer (built on the existing FeatureExtractor).
// JUCE-dependent, same lightweight build technique as
// test_sound_recommendation.cpp/test_serum_preset_status.cpp.
#include "../CandidateEvaluator.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>

static int g_checksRun = 0;
static int g_checksFailed = 0;

#define CHECK(cond) \
    do { \
        ++g_checksRun; \
        if (!(cond)) { \
            ++g_checksFailed; \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

int main()
{
    // ---- evaluate() on an empty (0-sample) buffer - the exact contract
    // renderVoiceAuditionAudio uses to signal "nothing could be rendered"
    // (no captured state / no instance) - must report hasAudio=false,
    // never a fabricated score. ----
    {
        juce::AudioBuffer<float> empty(2, 0);
        const auto fit = CandidateEvaluator::evaluate(empty, 44100.0);
        CHECK(fit.hasAudio == false);
        CHECK(CandidateEvaluator::describeFit("BASS", fit).contains("no captured Serum 2 state"));
    }

    // ---- evaluate() on a real, non-empty synthetic buffer (a plain sine
    // tone - a real, decodable audio signal, not silence) actually runs
    // the real FeatureExtractor and reports hasAudio=true with a real
    // measured peak level. ----
    {
        const double sr = 44100.0;
        const int numSamples = (int) sr; // 1 second
        juce::AudioBuffer<float> buf(2, numSamples);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* data = buf.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] = 0.5f * std::sin(2.0 * juce::MathConstants<double>::pi * 110.0 * (double) i / sr); // 110 Hz - a real low tone
        }
        const auto fit = CandidateEvaluator::evaluate(buf, sr);
        CHECK(fit.hasAudio == true);
        CHECK(fit.features.valid == true);
        CHECK(fit.features.peakDb > -20.0f); // a real, audible 0.5-amplitude tone must not read as near-silent
        CHECK(!CandidateEvaluator::describeFit("BASS", fit).contains("no captured Serum 2 state"));
        CHECK(CandidateEvaluator::describeFit("BASS", fit).contains("BASS"));
    }

    // ---- estimateLowEndConflict: honest degrade-to-zero when either
    // input is invalid (never fabricates a conflict score from
    // nonexistent audio). ----
    {
        AudioFeatures validA; validA.valid = true; validA.subPct = 0.5f; validA.lowPct = 0.3f;
        AudioFeatures invalidB; invalidB.valid = false;
        CHECK(CandidateEvaluator::estimateLowEndConflict(validA, invalidB) == 0.0f);
        CHECK(CandidateEvaluator::estimateLowEndConflict(invalidB, validA) == 0.0f);
    }

    // ---- estimateLowEndConflict: two candidates that BOTH carry real
    // low-end energy score higher than a case where only one does - the
    // documented, disclosed-heuristic behaviour ("both must carry real
    // low-end for this to be high"). ----
    {
        AudioFeatures bothLow1; bothLow1.valid = true; bothLow1.subPct = 0.4f; bothLow1.lowPct = 0.3f; bothLow1.lowMidPct = 0.1f;
        AudioFeatures bothLow2; bothLow2.valid = true; bothLow2.subPct = 0.35f; bothLow2.lowPct = 0.25f; bothLow2.lowMidPct = 0.1f;
        AudioFeatures brightOnly; brightOnly.valid = true; brightOnly.subPct = 0.0f; brightOnly.lowPct = 0.02f; brightOnly.highMidPct = 0.6f; brightOnly.airPct = 0.3f;

        const float bothLowConflict   = CandidateEvaluator::estimateLowEndConflict(bothLow1, bothLow2);
        const float mixedConflict     = CandidateEvaluator::estimateLowEndConflict(bothLow1, brightOnly);
        CHECK(bothLowConflict > mixedConflict);
        CHECK(bothLowConflict > 0.3f);   // two genuinely bass-heavy candidates should read as a real, non-trivial conflict
        CHECK(mixedConflict < 0.1f);     // a bass-heavy candidate paired with a genuinely bright one should read as low conflict
    }

    // ---- RoleFit's estimatedLowEndPct/brightnessHz are a direct,
    // untransformed pass-through of FeatureExtractor's own subPct+lowPct/
    // spectralCentroidHz - never independently recomputed or guessed. ----
    {
        const double sr = 44100.0;
        const int numSamples = (int) sr / 2;
        juce::AudioBuffer<float> buf(1, numSamples);
        auto* data = buf.getWritePointer(0);
        for (int i = 0; i < numSamples; ++i)
            data[i] = 0.4f * std::sin(2.0 * juce::MathConstants<double>::pi * 8000.0 * (double) i / sr); // a real high (8kHz) tone
        const auto fit = CandidateEvaluator::evaluate(buf, sr);
        CHECK(fit.hasAudio);
        CHECK(fit.estimatedLowEndPct == fit.features.subPct + fit.features.lowPct);
        CHECK(fit.brightnessHz == fit.features.spectralCentroidHz);
        // A genuine 8kHz tone should measure with a real high spectral
        // centroid, not near-zero - a sanity check that this is really
        // running FeatureExtractor's real FFT-based analysis, not a stub.
        CHECK(fit.brightnessHz > 1000.0f);
    }

    std::printf("%d/%d checks passed\n", g_checksRun - g_checksFailed, g_checksRun);
    return g_checksFailed == 0 ? 0 : 1;
}
