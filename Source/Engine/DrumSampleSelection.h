#pragma once

#include "DrumVoiceSynth.h" // DrumRole

// Phase 1 deterministic engine: WHICH sample to play, not WHEN/WHERE/how
// loud. Deliberately knows nothing about steps, patterns, or velocity -
// DrumEngine (Grid.h/DrumEngine.h) owns all of that. This file only
// answers "given N candidate samples for a role and a seed, which index
// do we pick" - a pure, zero-JUCE-dependency function so it's testable the
// same way as the rest of Engine/. The caller (Source/DrumSampleSelector.h,
// which knows about juce::File/Rack) resolves the returned index to an
// actual file.
//
// This separation matters beyond code hygiene: DrumEngine decides the
// musical pattern, this decides the sound, and neither has to change when
// the other does - e.g. a future reference-track-driven sample choice can
// replace this file without touching pattern generation at all.

namespace Engine
{
    // Deterministically picks an index in [0, candidateCount). Same
    // (role, seed, candidateCount) always returns the same index; a
    // different seed usually (not guaranteed for very small
    // candidateCount) returns a different one. Returns -1 if
    // candidateCount <= 0 (no candidates to choose from).
    int selectSampleIndex(DrumRole role, uint32_t seed, int candidateCount);
}
