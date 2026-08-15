// Regression tests for Engine::BassArchetype - the real, corpus-
// transcribed bass rhythmic archetypes (see BassArchetype.h's own header
// comment for the evidence). Proves each template matches its designed
// shape exactly, and that selection is deterministic and distributed
// close to the stated weights - not just that the tables exist.
#include "../BassArchetype.h"
#include "TestSupport.h"
#include <map>
#include <cstdio>

using namespace Engine;

namespace
{
    void checkOnset(const ArchetypeBar& bar, int step, int expectedLen, int8_t expectedOffset)
    {
        CHECK(bar.pitchOffsets[(size_t) step] == expectedOffset);
        CHECK(bar.gateLengthSteps[(size_t) step] == (int8_t) expectedLen);
    }

    void checkRest(const ArchetypeBar& bar, int step)
    {
        CHECK(bar.pitchOffsets[(size_t) step] == -128);
        CHECK(bar.gateLengthSteps[(size_t) step] == 0);
    }
}

int main()
{
    // ---- SteadyOffbeat: PML BS Low I Without/Rolling Without, Odd
    // Frequency CLARITY/LIGHTTHEFIRE - hits only the off-8th, root. ----
    {
        const auto bar = instantiateArchetypeBar(BassArchetype::SteadyOffbeat);
        checkOnset(bar, 2, 3, 0);
        checkOnset(bar, 6, 3, 0);
        checkOnset(bar, 10, 3, 0);
        checkOnset(bar, 14, 2, 0);
        for (int s : { 0, 1, 3, 4, 5, 7, 8, 9, 11, 12, 13, 15 })
            checkRest(bar, s);
    }

    // ---- Syncopated: Crunchy Dream, AFTERPARTY/LETEMKNOW/KEEPTRYING -
    // off-4-grid, root, short. ----
    {
        const auto bar = instantiateArchetypeBar(BassArchetype::Syncopated);
        for (int s : { 0, 3, 6, 9, 12 })
            checkOnset(bar, s, 1, 0);
        for (int s : { 1, 2, 4, 5, 7, 8, 10, 11, 13, 14, 15 })
            checkRest(bar, s);
    }

    // ---- AnticipationPairs: Plucky Pegasus, FINDME - paired 16ths before
    // each beat, root, short. ----
    {
        const auto bar = instantiateArchetypeBar(BassArchetype::AnticipationPairs);
        for (int s : { 2, 3, 6, 7, 10, 11, 14, 15 })
            checkOnset(bar, s, 1, 0);
        for (int s : { 0, 1, 4, 5, 8, 9, 12, 13 })
            checkRest(bar, s);
    }

    // ---- Rolling16th: Rolling/Short Touch, Short Believe - dense, mostly
    // root with a real +7 (fifth) passing-tone pair. ----
    {
        const auto bar = instantiateArchetypeBar(BassArchetype::Rolling16th);
        for (int s : { 0, 1, 2, 3, 4 })
            checkOnset(bar, s, 1, 0);
        checkOnset(bar, 6, 1, 7);
        checkOnset(bar, 7, 1, 7);
        checkOnset(bar, 10, 1, 0);
        checkOnset(bar, 11, 1, 0);
        for (int s : { 5, 8, 9, 12, 13, 14, 15 })
            checkRest(bar, s);
    }

    // ---- SparseMelodic: Ritual - a long anchor note plus a real +3
    // (minor third) passing-tone run, resolving to root. ----
    {
        const auto bar = instantiateArchetypeBar(BassArchetype::SparseMelodic);
        checkOnset(bar, 0, 4, 0);
        checkOnset(bar, 4, 1, 3);
        checkOnset(bar, 5, 1, 3);
        checkOnset(bar, 6, 1, 3);
        checkOnset(bar, 7, 1, 3);
        checkOnset(bar, 9, 2, 0);
        checkOnset(bar, 11, 1, 0);
        checkOnset(bar, 13, 2, 0);
        for (int s : { 1, 2, 3, 8, 10, 12, 14, 15 })
            checkRest(bar, s);
    }

    // ---- No archetype's own onsets overrun the next onset (a length
    // that would make one note's gate collide with the next one's
    // trigger point - a real "does this template make sense as written"
    // sanity check, not a musical judgment). ----
    for (int a = 0; a < 5; ++a)
    {
        const auto arch = (BassArchetype) a;
        const auto bar = instantiateArchetypeBar(arch);
        int lastOnset = -1;
        for (int s = 0; s < 16; ++s)
        {
            if (bar.pitchOffsets[(size_t) s] == -128)
                continue;
            if (lastOnset >= 0)
            {
                const int lastLen = bar.gateLengthSteps[(size_t) lastOnset];
                CHECK(lastOnset + lastLen <= s);
            }
            lastOnset = s;
        }
    }

    // ---- Deterministic selection: same seed -> same archetype, every
    // time. ----
    for (uint32_t seed = 1; seed <= 50; ++seed)
        CHECK(selectBassArchetype(seed) == selectBassArchetype(seed));

    // ---- Distribution over many seeds roughly matches the disclosed
    // weights (30/30/15/20/5) - a real statistical claim about the
    // selector, not just "it returns something." Generous tolerance since
    // this isn't trying to prove exact probabilities, just that the
    // mechanism isn't broken (e.g. always returning one archetype, or a
    // wildly different distribution). ----
    {
        std::map<BassArchetype, int> counts;
        constexpr int kNumSeeds = 4000;
        for (uint32_t seed = 1; seed <= (uint32_t) kNumSeeds; ++seed)
            counts[selectBassArchetype(seed)]++;

        auto pct = [&](BassArchetype a) { return 100.0 * counts[a] / (double) kNumSeeds; };
        std::printf("archetype distribution over %d seeds: steady=%.1f%% sync=%.1f%% antic=%.1f%% rolling=%.1f%% sparse=%.1f%%\n",
                    kNumSeeds, pct(BassArchetype::SteadyOffbeat), pct(BassArchetype::Syncopated),
                    pct(BassArchetype::AnticipationPairs), pct(BassArchetype::Rolling16th), pct(BassArchetype::SparseMelodic));

        CHECK(pct(BassArchetype::SteadyOffbeat) > 25.0 && pct(BassArchetype::SteadyOffbeat) < 35.0);
        CHECK(pct(BassArchetype::Syncopated) > 25.0 && pct(BassArchetype::Syncopated) < 35.0);
        CHECK(pct(BassArchetype::AnticipationPairs) > 10.0 && pct(BassArchetype::AnticipationPairs) < 20.0);
        CHECK(pct(BassArchetype::Rolling16th) > 15.0 && pct(BassArchetype::Rolling16th) < 25.0);
        CHECK(pct(BassArchetype::SparseMelodic) > 2.0 && pct(BassArchetype::SparseMelodic) < 8.0);
        CHECK(counts.size() == 5); // every archetype is actually reachable
    }

    TEST_SUMMARY_AND_EXIT();
}
