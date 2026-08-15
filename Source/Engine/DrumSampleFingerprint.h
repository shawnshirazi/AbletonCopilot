#pragma once

// GENERATED FILE - do not hand-edit.
//
// Regenerate with:
//   MLPipeline/venv/bin/python3 MLPipeline/drum_grammar/generate_fingerprint_header.py
// which reads MLPipeline/drum_grammar/output/drum_grammar.json's
// sound_fingerprints block - real mean/stddev measurements (duration,
// attack time, zero-crossing rate, estimated pitch, peak, RMS) taken
// directly from one-shot samples in four Melodic-Techno-branded sample
// packs already in the user's library (PML Mirage, PML Mystique, Odd
// Frequency Exo, Odd Frequency Exo 2) - see
// MLPipeline/drum_grammar/analyze_drum_grammar.py for the measurement
// methodology. Not hand-tuned guesses - if a number here looks wrong,
// fix the analysis or the corpus and regenerate, don't edit this file.
// Generated: 2026-08-14
// KICK: n=83 one-shot samples analyzed
// CLAP: n=61 one-shot samples analyzed
// HAT: n=85 one-shot samples analyzed
// PERC: n=86 one-shot samples analyzed

#include "DrumVoiceSynth.h" // DrumRole

namespace Engine
{
    struct FingerprintDim { float mean; float stddev; };

    // One dimension per DrumSampleFeatures field the runtime plugin can
    // actually measure (see Source/DrumSampleAnalysis.cpp) - stddev is
    // the real measured spread, used by scoreForRole as a Gaussian
    // similarity width, not a hand-picked tolerance.
    struct DrumRoleFingerprint
    {
        FingerprintDim durationSec;
        FingerprintDim attackMs;
        FingerprintDim zeroCrossingHz;
        FingerprintDim estimatedPitchHz;
        FingerprintDim peak;
        FingerprintDim rms;
    };

    constexpr DrumRoleFingerprint kKickFingerprint {
        { 0.493575f, 0.185957f },   // durationSec
        { 14.1500f, 16.6230f },   // attackMs
        { 736.740f, 1557.704f },   // zeroCrossingHz
        { 32.398f, 30.277f },   // estimatedPitchHz
        { 0.9150f, 0.0860f },   // peak
        { 0.3010f, 0.0750f },   // rms
    };

    constexpr DrumRoleFingerprint kClapFingerprint {
        { 0.525989f, 0.249142f },   // durationSec
        { 10.8120f, 12.3160f },   // attackMs
        { 3150.432f, 2420.049f },   // zeroCrossingHz
        { 0.000f, 5.000f },   // estimatedPitchHz
        { 0.8700f, 0.0930f },   // peak
        { 0.0870f, 0.0350f },   // rms
    };

    constexpr DrumRoleFingerprint kHatFingerprint {
        { 0.353639f, 0.253443f },   // durationSec
        { 5.4480f, 5.3330f },   // attackMs
        { 7202.553f, 3130.177f },   // zeroCrossingHz
        { 6.492f, 34.122f },   // estimatedPitchHz
        { 0.8400f, 0.1440f },   // peak
        { 0.0940f, 0.0540f },   // rms
    };

    constexpr DrumRoleFingerprint kPercFingerprint {
        { 0.534889f, 0.308194f },   // durationSec
        { 11.3640f, 23.9460f },   // attackMs
        { 2048.114f, 2109.665f },   // zeroCrossingHz
        { 77.981f, 111.846f },   // estimatedPitchHz
        { 0.7840f, 0.1950f },   // peak
        { 0.0770f, 0.0410f },   // rms
    };

    inline const DrumRoleFingerprint& targetFingerprintForRole(DrumRole role)
    {
        switch (role)
        {
            case DrumRole::Kick:  return kKickFingerprint;
            case DrumRole::Clap:  return kClapFingerprint;
            case DrumRole::Hat:   return kHatFingerprint;
            case DrumRole::Perc:  return kPercFingerprint;
            case DrumRole::Count: return kKickFingerprint;
        }
        return kKickFingerprint;
    }
}
