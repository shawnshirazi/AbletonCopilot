#pragma once

#include "DrumSampleFeatures.h"
#include "DrumVoiceSynth.h" // DrumRole

// Phase 1 deterministic engine: turns measured sample characteristics
// (DrumSampleFeatures) into a single "how well does this fit the measured
// Melodic Techno character for this role" score. Pure, explainable Gaussian
// similarity against a target fingerprint (Source/Engine/
// DrumSampleFingerprint.h) - real mean/stddev statistics measured from real
// one-shot samples in four Melodic-Techno-branded packs (see
// MLPipeline/drum_grammar/), not hand-picked ranges and not a black-box/ML
// model. Zero JUCE dependency, so it's testable the same way as the rest of
// Engine/.
//
// This file only judges sound character - it has no idea what a step, a
// bar, or a pattern is (that's DrumEngine's job) and no idea how samples
// are discovered/cached (that's Source/DrumSampleIndex.h). Ranking/
// shortlisting/final seeded pick lives in Source/DrumSampleSelector.h,
// which calls this once per candidate.
namespace Engine
{
    // bpmHint: used only for Kick, where a sample that rings out longer
    // than roughly a beat at the current tempo will audibly overlap the
    // next 4/4 hit - the other roles' one-shot lengths are always well
    // under a beat at any real techno tempo, so BPM doesn't meaningfully
    // change their scoring. Pass <= 0 to fall back to a fixed reference
    // tempo (124 BPM, a typical melodic techno tempo) when no real host
    // tempo is available yet (e.g. transport not playing).
    float scoreForRole(DrumRole role, const DrumSampleFeatures& features, double bpmHint);
}
