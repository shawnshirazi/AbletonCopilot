#pragma once
#include <JuceHeader.h>
#include "Analysis/FeatureExtractor.h"

// "Does this sound good in this exact musical context" answered from
// REAL RENDERED AUDIO, not metadata/filenames - the audio-domain half of
// the sound-selection workflow this pass investigated. Built entirely on
// top of the ALREADY-EXISTING FeatureExtractor (the same spectral/
// frequency-band/stereo-width analysis this codebase already uses for
// genre-matching, Source/Analysis/FeatureExtractor.h) applied to a real
// candidate rendered via PluginProcessor::renderVoiceAuditionAudio - that
// function only ever renders a track's ALREADY-CAPTURED real Serum2
// state playing its ALREADY-GENERATED real MIDI pattern; nothing here can
// fabricate a candidate that doesn't actually exist.
//
// HONESTY CONTRACT: every score below is an explicitly-labeled HEURISTIC
// derived from real, measured audio features - never a validated
// psychoacoustic masking model, never a claim about what a specific
// preset "is" beyond what was actually measured from its own rendered
// output this run.
namespace CandidateEvaluator
{
    struct RoleFit
    {
        AudioFeatures features;
        bool  hasAudio          = false; // false = nothing could be rendered (no captured state loaded onto that voice / no Serum2 instance yet) - callers must not report a score in this case, only the honest reason
        float estimatedLowEndPct = 0.0f;  // subPct + lowPct, straight from FeatureExtractor - [HEURISTIC] proxy for "how much low-end energy this candidate's render carries"
        float brightnessHz      = 0.0f;   // FeatureExtractor's own spectralCentroidHz, unchanged - [HEURISTIC] proxy for "brightness"
    };

    // Runs the existing FeatureExtractor over a real rendered candidate
    // buffer. `rendered` with 0 samples (see renderVoiceAuditionAudio's
    // own contract for exactly when that happens) yields hasAudio=false,
    // not a fabricated zero-score.
    inline RoleFit evaluate(const juce::AudioBuffer<float>& rendered, double sampleRate)
    {
        RoleFit fit;
        if (rendered.getNumSamples() <= 0)
            return fit;

        FeatureExtractor extractor;
        fit.features           = extractor.extract(rendered, sampleRate);
        fit.hasAudio            = fit.features.valid;
        fit.estimatedLowEndPct  = fit.features.subPct + fit.features.lowPct;
        fit.brightnessHz        = fit.features.spectralCentroidHz;
        return fit;
    }

    // [HEURISTIC, NOT a validated masking model] - a simple proxy for
    // "how much do these two candidates' low-end/low-mid energy overlap,"
    // e.g. Bass vs Melody. Real auditory masking depends on exact
    // frequency, simultaneity, and level relationships within
    // psychoacoustic critical bands - none of which this computes. This
    // only answers "do both candidates carry real low-end energy at the
    // same time," which is the coarse, honestly-labeled signal available
    // from FeatureExtractor's existing band-percentage output. Returns
    // 0..1 (roughly), higher = more low-end energy present in BOTH.
    inline float estimateLowEndConflict(const AudioFeatures& a, const AudioFeatures& b)
    {
        if (!a.valid || !b.valid)
            return 0.0f;
        const float aLow = a.subPct + a.lowPct + a.lowMidPct * 0.5f;
        const float bLow = b.subPct + b.lowPct + b.lowMidPct * 0.5f;
        return juce::jmin(aLow, bLow) * 2.0f;
    }

    // Human-readable summary, grounded only in the real measured features
    // passed in - never invents a claim about a preset's sound design
    // beyond what this run's own render actually measured.
    inline juce::String describeFit(const juce::String& roleLabel, const RoleFit& fit)
    {
        if (!fit.hasAudio)
            return roleLabel + ": (no captured Serum 2 state loaded for this track yet - nothing real to render or analyze)";

        juce::String s;
        s << roleLabel << ": spectral centroid " << juce::String(fit.brightnessHz, 0) << " Hz, "
          << "low-end (sub+low) " << juce::String(fit.estimatedLowEndPct * 100.0f, 0) << "%, "
          << "stereo width " << juce::String(fit.features.stereoWidth, 2) << ", "
          << "peak " << juce::String(fit.features.peakDb, 1) << " dBFS";
        return s;
    }
}
