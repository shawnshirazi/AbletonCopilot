#pragma once

// Phase 1 deterministic engine: music theory / scale handling.
//
// Deliberately zero JUCE dependency (no juce::String, no juce::Component,
// no JUCE headers at all) - plain C++17 so this compiles and can be unit
// tested with a bare `clang++`, independent of the plugin build. The same
// files also get added to the JUCE project so they compile as part of the
// real plugin target too (see AbletonCopilot.jucer) - not a separate,
// disconnected codebase, just not entangled with UI/audio code either.
//
// The scale/key math here isn't new - it's a clean-room re-expression of
// logic already proven correct earlier this session in
// Source/MusicTheory/MelodicTechnoTheory.cpp (kAeolianScale/kDorianScale/
// kIonianScale/degreeToSemitone) and Source/MusicTheory/MelodyCritic.cpp
// (nearestInScaleTone), re-homed into a module with no UI/JUCE coupling.

namespace Engine
{
    // Melodic techno leans on natural minor (Aeolian) and Dorian (raised
    // 6th) for minor keys; Ionian (plain major) covers major keys - the
    // same three scales the existing plugin already uses.
    enum class ScaleType { Aeolian, Dorian, Ionian };

    struct Key
    {
        int       rootSemitone = 0; // 0=C, 1=C#, ... 11=B
        ScaleType scale        = ScaleType::Aeolian;
    };

    // True if semitoneOffsetFromRoot (can be negative, any octave) lands on
    // a scale tone relative to the key's root.
    bool isInScale(int semitoneOffsetFromRoot, ScaleType scale);

    // Nearest in-scale semitone to semitoneOffsetFromRoot, searching
    // outward (+1, -1, +2, -2, ...) so the result stays as close as
    // possible to the input. Returns the input unchanged if it's already
    // in scale.
    int nearestInScale(int semitoneOffsetFromRoot, ScaleType scale);

    // Scale-degree (0-based, can exceed the scale's own length or go
    // negative to move across octaves) to a semitone offset from the root.
    int degreeToSemitone(int degree, ScaleType scale);

    // "C", "C#", "D", ... for absoluteSemitone mod 12 (any octave/sign).
    const char* noteName(int absoluteSemitone);
}
