#pragma once

#include <array>
#include <cstdint>

// Real, recurring 1-bar bass RHYTHMIC ARCHETYPES, found by analyzing the
// 20-file groove-subset bass MIDI corpus (PML Mirage/Mystique, Odd
// Frequency Exo2 - the same corpus BassRhythmGrammar.h's pooled statistics
// come from) FILE BY FILE instead of as one blended average. The pooled
// average (kBassGrooveRhythm's per-step probability) can reproduce
// plausible AGGREGATE stats while sounding like nothing in particular -
// no single real bassline plays "a little bit of every position with the
// right overall frequency." What real files actually do is commit to ONE
// clean, recognizable shape for the whole part. This file is that
// evidence, encoded directly - not re-derived from the pooled stats.
//
// Each archetype below is a literal transcription of real corpus
// examples (file names in the comments), not an invented shape. Where a
// template moves off the root, the interval used is a real, measured one
// from BassRhythmGrammar.h's own pitch-offset distribution (+7 = fifth,
// +3 = minor third) - never an arbitrary number.
namespace Engine
{
    enum class BassArchetype
    {
        SteadyOffbeat,     // hits only the off-8th (2/6/10/14), root, held 2-3 steps
        Syncopated,        // off-4-grid spacing (~every 3 steps), root, short/staccato
        AnticipationPairs, // paired 16ths landing right before each beat, root, short
        Rolling16th,       // dense/near-continuous, mostly root with passing tones
        SparseMelodic       // real pitch movement, one long anchor note + short passing tones
    };

    // One note in an archetype's canonical bar: step (0-15), lengthSteps
    // (real hold duration - see PluginProcessor's gate-length scheduling,
    // the mechanism that actually makes this audible), semitoneOffset
    // (0 = root).
    struct ArchetypeNote
    {
        int   step;
        int   lengthSteps;
        int8_t semitoneOffset;
    };

    // Deterministic weighted pick - same seed always selects the same
    // archetype. Weights are a disclosed renormalization of the measured
    // corpus mix (SteadyOffbeat ~20%, Syncopated ~20%, AnticipationPairs
    // ~10%, Rolling16th ~15%, SparseMelodic ~5%, ~30% unclassified
    // hybrids) - the hybrid mass is folded into the two majority
    // archetypes (SteadyOffbeat/Syncopated) rather than kept as a vague
    // 6th bucket, which is a design choice, not a new measurement.
    BassArchetype selectBassArchetype(uint32_t seed);

    // Returns the archetype's canonical bar as parallel 16-entry arrays -
    // pitchOffsets (-128 elsewhere, matching BassEngine.h's kBassOffValue -
    // this file deliberately doesn't depend on BassEngine.h, so the
    // constant is repeated as a literal rather than shared, to avoid a
    // circular include since BassEngine.cpp is what calls into this file)
    // and gateLengthSteps (0 elsewhere; only meaningful at an onset).
    struct ArchetypeBar
    {
        std::array<int8_t, 16> pitchOffsets;
        std::array<int8_t, 16> gateLengthSteps;
    };
    ArchetypeBar instantiateArchetypeBar(BassArchetype archetype);
}
