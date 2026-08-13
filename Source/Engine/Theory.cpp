#include "Theory.h"

namespace Engine
{
    namespace
    {
        constexpr int kAeolianScale[7] = { 0, 2, 3, 5, 7, 8, 10 };
        constexpr int kDorianScale[7]  = { 0, 2, 3, 5, 7, 9, 10 }; // raised 6th vs Aeolian
        constexpr int kIonianScale[7]  = { 0, 2, 4, 5, 7, 9, 11 }; // plain major

        const int* scaleTable(ScaleType scale)
        {
            switch (scale)
            {
                case ScaleType::Aeolian: return kAeolianScale;
                case ScaleType::Dorian:  return kDorianScale;
                case ScaleType::Ionian:  return kIonianScale;
            }
            return kAeolianScale;
        }
    }

    bool isInScale(int semitoneOffsetFromRoot, ScaleType scale)
    {
        const int* table = scaleTable(scale);
        const int  pitchClass = ((semitoneOffsetFromRoot % 12) + 12) % 12;
        for (int i = 0; i < 7; ++i)
            if (table[i] == pitchClass)
                return true;
        return false;
    }

    int nearestInScale(int semitoneOffsetFromRoot, ScaleType scale)
    {
        if (isInScale(semitoneOffsetFromRoot, scale))
            return semitoneOffsetFromRoot;

        // Search outward - every semitone is within 6 of some scale tone,
        // so this always terminates with a real answer.
        for (int d = 1; d <= 6; ++d)
        {
            if (isInScale(semitoneOffsetFromRoot + d, scale)) return semitoneOffsetFromRoot + d;
            if (isInScale(semitoneOffsetFromRoot - d, scale)) return semitoneOffsetFromRoot - d;
        }
        return semitoneOffsetFromRoot; // unreachable in practice
    }

    int degreeToSemitone(int degree, ScaleType scale)
    {
        const int* table  = scaleTable(scale);
        const int  octave = degree >= 0 ? degree / 7 : (degree - 6) / 7;
        const int  idx    = ((degree % 7) + 7) % 7;
        return octave * 12 + table[idx];
    }

    const char* noteName(int absoluteSemitone)
    {
        static const char* kNames[12] = { "C", "C#", "D", "D#", "E", "F",
                                           "F#", "G", "G#", "A", "A#", "B" };
        const int pitchClass = ((absoluteSemitone % 12) + 12) % 12;
        return kNames[pitchClass];
    }
}
