#include "../BassEngine.h"
#include "../BassRhythmGrammar.h"
#include "TestSupport.h"
#include <algorithm>
#include <cstdio>

using namespace Engine;

namespace
{
    int countActive(const std::array<int8_t, kBassSteps>& p)
    {
        int n = 0;
        for (auto v : p)
            if (v != kBassOffValue) ++n;
        return n;
    }

    bool sameArray(const std::array<int8_t, kBassSteps>& a, const std::array<int8_t, kBassSteps>& b)
    {
        return a == b;
    }

    double sharedFraction(const std::array<int8_t, kBassSteps>& p, int barA, int barB)
    {
        int matches = 0;
        for (int s = 0; s < 16; ++s)
        {
            const bool a = p[(size_t) (barA * 16 + s)] != kBassOffValue;
            const bool b = p[(size_t) (barB * 16 + s)] != kBassOffValue;
            if (a == b) ++matches;
        }
        return (double) matches / 16.0;
    }

    void printPattern(const char* label, const std::array<int8_t, kBassSteps>& p)
    {
        std::printf("--- %s (%d/%d active) ---\n", label, countActive(p), kBassSteps);
        for (int bar = 0; bar < 8; ++bar)
        {
            std::printf("%d: ", bar);
            for (int s = 0; s < 16; ++s)
            {
                int8_t v = p[(size_t) (bar * 16 + s)];
                if (v == kBassOffValue) std::printf(" .  ");
                else                    std::printf("%+3d ", (int) v);
            }
            std::printf("\n");
        }
    }
}

int main()
{
    BassPatternParams standard;
    standard.density = 0.5f; standard.variation = 0.2f; standard.seed = 7;

    // =====================================================================
    // Shape sanity: 128 steps (matches PluginProcessor::kMelodySteps), a
    // real but restrained number of active steps - not silent, not filled.
    // =====================================================================
    {
        const auto p = generateBassPattern(standard);
        CHECK((int) p.size() == kBassSteps);
        const int active = countActive(p);
        CHECK(active > 10);  // a real, audible bassline
        CHECK(active < 90);  // still selective, nowhere near filling every step
        printPattern("BASS (default params, seed=7)", p);
    }

    // =====================================================================
    // Determinism: same seed -> byte-identical pattern; different seed ->
    // a genuinely different pattern.
    // =====================================================================
    {
        CHECK(sameArray(generateBassPattern(standard), generateBassPattern(standard)));

        BassPatternParams seedB = standard; seedB.seed = 8;
        CHECK(!sameArray(generateBassPattern(standard), generateBassPattern(seedB)));
    }

    // =====================================================================
    // Kick avoidance: the measured onKickFraction (0.2126) is below the
    // 0.25 a kick-position-blind placement would produce - aggregated
    // across many seeds so this tests the actual mechanism (real
    // BassRhythmGrammar.h data, not an absolute exclusion), not one lucky
    // roll.
    // =====================================================================
    {
        CHECK(kBassGrooveRhythm.onKickFraction < 0.25f); // the measured fact the mechanism rests on

        constexpr int kNumSeeds = 60;
        long onKick = 0, total = 0;
        for (uint32_t seed = 0; seed < kNumSeeds; ++seed)
        {
            BassPatternParams p = standard; p.seed = seed;
            const auto pattern = generateBassPattern(p);
            for (int i = 0; i < kBassSteps; ++i)
            {
                if (pattern[(size_t) i] == kBassOffValue) continue;
                ++total;
                if ((i % 16) % 4 == 0) ++onKick;
            }
        }
        CHECK(total > 0);
        const double onKickFraction = (double) onKick / (double) total;
        std::printf("generated on-kick fraction: %.3f (measured=%.3f, uniform baseline=0.25)\n",
                    onKickFraction, (double) kBassGrooveRhythm.onKickFraction);
        CHECK(onKickFraction < 0.25); // measurably below uniform - real, not absolute, avoidance
    }

    // =====================================================================
    // Pitch: overwhelmingly the root (offset 0), matching the real
    // measured ~75% root fraction - aggregated across many seeds.
    // =====================================================================
    {
        CHECK(kBassPitchOffsets[0].offsetSemitones == 0);
        CHECK(kBassPitchOffsets[0].probability > 0.5f); // root is the dominant real-measured choice

        constexpr int kNumSeeds = 60;
        long rootCount = 0, total = 0;
        for (uint32_t seed = 0; seed < kNumSeeds; ++seed)
        {
            BassPatternParams p = standard; p.seed = seed;
            const auto pattern = generateBassPattern(p);
            for (auto v : pattern)
            {
                if (v == kBassOffValue) continue;
                ++total;
                if (v == 0) ++rootCount;
            }
        }
        CHECK(total > 0);
        const double rootFraction = (double) rootCount / (double) total;
        std::printf("generated root-note fraction: %.3f (measured=%.3f)\n", rootFraction,
                    (double) kBassPitchOffsets[0].probability);
        CHECK(rootFraction > 0.5); // root-dominant, matching the real measured character
        CHECK(rootFraction < 1.0); // but genuinely does move sometimes, not root-only
    }

    // =====================================================================
    // Motif: bars 0-3 are a real 4-bar block (adjacent bars mostly share
    // content - buildBassBlock's own repeat/touch mechanism), and bars 4-7
    // develop from bars 0-3 without becoming an unrelated pattern.
    // =====================================================================
    {
        constexpr int kNumSeeds = 40;
        double withinBlockShared = 0.0; int withinBlockN = 0;
        double establishToDevelopShared = 0.0;
        for (uint32_t seed = 0; seed < kNumSeeds; ++seed)
        {
            BassPatternParams p = standard; p.seed = seed;
            const auto pattern = generateBassPattern(p);
            for (int bar = 0; bar < 3; ++bar)
            {
                withinBlockShared += sharedFraction(pattern, bar, bar + 1);
                ++withinBlockN;
            }
            establishToDevelopShared += sharedFraction(pattern, 0, 4);
        }
        const double meanWithinBlockShared = withinBlockShared / withinBlockN;
        const double meanEstablishToDevelop = establishToDevelopShared / kNumSeeds;
        std::printf("bass within-block adjacent-bar shared fraction: %.3f\n", meanWithinBlockShared);
        std::printf("bass bar0-vs-bar4 (establish vs developed) shared fraction: %.3f\n", meanEstablishToDevelop);
        CHECK(meanWithinBlockShared > 0.75);     // bars 0-3 are recognizably one motif
        CHECK(meanEstablishToDevelop > 0.5);     // bars 4-7 still recognizably related to bars 0-3
        CHECK(meanEstablishToDevelop < 1.0);     // but genuinely developed, not a frozen copy

        // variation=0 -> minimal (but not necessarily zero, touches has a
        // floor of 1) departure from the established motif.
        BassPatternParams noVar = standard; noVar.variation = 0.0f;
        const auto dropNoVar = generateBassPattern(noVar);
        CHECK(sharedFraction(dropNoVar, 0, 4) > 0.75);
    }

    // =====================================================================
    // Different seeds produce controlled variation - not wildly different
    // compositions from run to run.
    // =====================================================================
    {
        int counts[5];
        for (uint32_t seed = 0; seed < 5; ++seed)
        {
            BassPatternParams p = standard; p.seed = seed * 17 + 3;
            counts[seed] = countActive(generateBassPattern(p));
        }
        int minCount = counts[0], maxCount = counts[0];
        for (int c : counts) { minCount = std::min(minCount, c); maxCount = std::max(maxCount, c); }
        CHECK(maxCount < minCount * 3 + 15);
    }

    TEST_SUMMARY_AND_EXIT();
}
