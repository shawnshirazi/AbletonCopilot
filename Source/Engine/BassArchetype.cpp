#include "BassArchetype.h"
#include <random>

namespace Engine
{
    namespace
    {
        constexpr int8_t kOff = -128; // matches BassEngine.h's kBassOffValue - see header comment

        // Real corpus transcriptions - file names are the actual source
        // (Source/Engine/tests/ has the corpus analysis script output this
        // session used to derive these; see melodic_techno_research.md and
        // MLPipeline/drum_grammar/output/bass_grammar_report.md for the
        // pooled statistics these individual shapes were found underneath).

        // PML BS Low I Without / Rolling Without, Odd Frequency CLARITY/
        // LIGHTTHEFIRE: hits ONLY the off-8th, root, held most of the gap
        // to the next hit (measured note length 2 steps in a 4-step gap;
        // 3 used here for the first three hits to let it ring a bit
        // longer, matching the "avoids the kick entirely" idiom the
        // Myloops guide describes - a modest, disclosed interpretation of
        // the measured 1-2 step range, not a re-measurement).
        constexpr ArchetypeNote kSteadyOffbeat[] = {
            { 2, 3, 0 }, { 6, 3, 0 }, { 10, 3, 0 }, { 14, 2, 0 },
        };

        // PML BS Crunchy Dream, Odd Frequency AFTERPARTY/LETEMKNOW/
        // KEEPTRYING: off-4-grid spacing (roughly every 3 steps, doesn't
        // align to the beat), root, short/staccato.
        constexpr ArchetypeNote kSyncopated[] = {
            { 0, 1, 0 }, { 3, 1, 0 }, { 6, 1, 0 }, { 9, 1, 0 }, { 12, 1, 0 },
        };

        // PML BS Plucky Pegasus, Odd Frequency FINDME: paired 16ths
        // landing right before each beat (real "anticipation" - see
        // melodic_techno_research.md's own anticipation-fraction
        // measurement), root, short.
        constexpr ArchetypeNote kAnticipationPairs[] = {
            { 2, 1, 0 }, { 3, 1, 0 }, { 6, 1, 0 }, { 7, 1, 0 },
            { 10, 1, 0 }, { 11, 1, 0 }, { 14, 1, 0 }, { 15, 1, 0 },
        };

        // PML BS Rolling Touch / Short Touch, Short Believe: dense/near-
        // continuous, mostly root with real passing-tone movement - +7
        // (fifth) is BassRhythmGrammar.h's own second-most-common real
        // measured interval, used here for the file's own consistent
        // upward movement.
        constexpr ArchetypeNote kRolling16th[] = {
            { 0, 1, 0 }, { 1, 1, 0 }, { 2, 1, 0 }, { 3, 1, 0 }, { 4, 1, 0 },
            { 6, 1, 7 }, { 7, 1, 7 }, { 10, 1, 0 }, { 11, 1, 0 },
        };

        // PML BS Ritual: the one file in the groove subset with real
        // melodic movement - a long anchor note on the root, then short
        // passing tones a minor third up (+3, BassRhythmGrammar.h's
        // second-most-common measured offset), resolving back to root.
        constexpr ArchetypeNote kSparseMelodic[] = {
            { 0, 4, 0 }, { 4, 1, 3 }, { 5, 1, 3 }, { 6, 1, 3 }, { 7, 1, 3 },
            { 9, 2, 0 }, { 11, 1, 0 }, { 13, 2, 0 },
        };

        template <size_t N>
        ArchetypeBar buildBar(const ArchetypeNote (&notes)[N])
        {
            ArchetypeBar bar;
            bar.pitchOffsets.fill(kOff);
            bar.gateLengthSteps.fill(0);
            for (auto& n : notes)
            {
                bar.pitchOffsets[(size_t) n.step]    = n.semitoneOffset;
                bar.gateLengthSteps[(size_t) n.step] = (int8_t) n.lengthSteps;
            }
            return bar;
        }
    }

    BassArchetype selectBassArchetype(uint32_t seed)
    {
        // Own RNG stream/salt, independent of BassEngine.cpp's own
        // makeRng - archetype selection is a single deterministic draw,
        // not part of the per-position random-decision stream, so it
        // can't drift if that stream's call count ever changes.
        std::mt19937 rng(seed ^ 0x41524348u); // 'ARCH'
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        const float r = dist(rng);

        // Disclosed renormalized weights - see header comment.
        if (r < 0.30f) return BassArchetype::SteadyOffbeat;
        if (r < 0.60f) return BassArchetype::Syncopated;
        if (r < 0.75f) return BassArchetype::AnticipationPairs;
        if (r < 0.95f) return BassArchetype::Rolling16th;
        return BassArchetype::SparseMelodic;
    }

    ArchetypeBar instantiateArchetypeBar(BassArchetype archetype)
    {
        switch (archetype)
        {
            case BassArchetype::SteadyOffbeat:     return buildBar(kSteadyOffbeat);
            case BassArchetype::Syncopated:        return buildBar(kSyncopated);
            case BassArchetype::AnticipationPairs: return buildBar(kAnticipationPairs);
            case BassArchetype::Rolling16th:       return buildBar(kRolling16th);
            case BassArchetype::SparseMelodic:     return buildBar(kSparseMelodic);
        }
        return buildBar(kSteadyOffbeat);
    }
}
