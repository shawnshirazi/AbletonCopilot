#pragma once

// Phase 1 deterministic engine: plain measured characteristics of one
// drum one-shot sample. Zero JUCE dependency (see Theory.h for why) - the
// actual decoding/measurement (Source/DrumSampleAnalysis.h, which does
// need JUCE to read audio files) fills one of these in; everything that
// judges or ranks a sample against a role's desired character
// (Engine/DrumSampleScoring.h) works only with this plain data, so it's
// testable the same way as the rest of Engine/.
namespace Engine
{
    struct DrumSampleFeatures
    {
        bool   valid             = false; // false if the file couldn't be read/decoded
        double durationSec       = 0.0;
        float  peakAmplitude     = 0.0f;  // 0..1
        float  rms               = 0.0f;  // 0..1, over the whole sample
        float  attackTimeMs      = 0.0f;  // time from start to 90% of peak - lower = sharper/stronger transient
        float  zeroCrossingRate  = 0.0f;  // crossings/sec - cheap, FFT-free proxy for spectral brightness
        float  estimatedPitchHz  = 0.0f;  // 0 = no clear pitch found (typical/expected for noise-heavy hats/percs)
    };
}
