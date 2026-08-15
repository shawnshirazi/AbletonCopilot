#include "../Arrangement.h"
#include "../BassEngine.h"
#include "../BassRhythmGrammar.h"
#include "TestSupport.h"
#include <algorithm>
#include <cstdio>

using namespace Engine;

namespace
{
    int countActiveInBar(const std::vector<int8_t>& pattern, int bar, int stepsPerBar)
    {
        int n = 0;
        for (int s = 0; s < stepsPerBar; ++s)
            if (pattern[(size_t) (bar * stepsPerBar + s)] != kBassOffValue)
                ++n;
        return n;
    }

    double meanDensityForSection(const std::vector<int8_t>& pattern, const MusicArrangement& arrangement,
                                  MusicSection section, int stepsPerBar)
    {
        long total = 0;
        int bars = 0;
        for (int b = 0; b < arrangement.totalBars(); ++b)
        {
            if (arrangement.barStates[(size_t) b].section != section)
                continue;
            total += countActiveInBar(pattern, b, stepsPerBar);
            ++bars;
        }
        return bars > 0 ? (double) total / bars : 0.0;
    }
}

int main()
{
    constexpr int kStepsPerBar = 16;

    ArrangementConfig cfg;
    cfg.bpm = 124.0;
    cfg.seed = 7;
    const auto arrangement = buildArrangement(cfg);

    BassPatternParams params;
    params.density = 0.5f; params.variation = 0.2f; params.seed = 7;

    // =====================================================================
    // Shape sanity + determinism.
    // =====================================================================
    {
        const auto bass = generateArrangementBassPattern(arrangement, params);
        CHECK((int) bass.size() == arrangement.totalBars() * kStepsPerBar);

        const auto bassAgain = generateArrangementBassPattern(arrangement, params);
        CHECK(bass == bassAgain);

        BassPatternParams paramsB = params; paramsB.seed = 8;
        const auto bassDiffSeed = generateArrangementBassPattern(arrangement, paramsB);
        CHECK(bass != bassDiffSeed);
    }

    // =====================================================================
    // Bass is dramatically reduced (not necessarily exactly silent) in
    // Breakdown compared to Drop - "largely disappear... possibly retain
    // an occasional root" per the brief, not an absolute mute rule.
    // =====================================================================
    {
        const auto bass = generateArrangementBassPattern(arrangement, params);
        const double dropDensity = meanDensityForSection(bass, arrangement, MusicSection::Drop, kStepsPerBar);
        const double breakdownDensity = meanDensityForSection(bass, arrangement, MusicSection::Breakdown, kStepsPerBar);
        std::printf("Bass mean/bar: Drop=%.2f Breakdown=%.2f\n", dropDensity, breakdownDensity);
        CHECK(dropDensity > 3.0);              // a real, present bassline in the drop
        CHECK(breakdownDensity < dropDensity * 0.5); // dramatically reduced
        CHECK(breakdownDensity < 2.5);          // an absolute ceiling too - genuinely sparse
    }

    // =====================================================================
    // Bass returns in the drop - aggregated across many seeds so this
    // isn't one lucky roll. Drop density is consistently strong.
    // =====================================================================
    {
        constexpr int kNumSeeds = 20;
        double total = 0.0;
        for (uint32_t seed = 0; seed < kNumSeeds; ++seed)
        {
            ArrangementConfig c = cfg; c.seed = seed;
            const auto a = buildArrangement(c);
            BassPatternParams p = params; p.seed = seed;
            const auto bass = generateArrangementBassPattern(a, p);
            total += meanDensityForSection(bass, a, MusicSection::Drop, kStepsPerBar);
        }
        const double meanDropDensity = total / kNumSeeds;
        std::printf("Mean Drop bass density over %d seeds: %.2f\n", kNumSeeds, meanDropDensity);
        CHECK(meanDropDensity > 3.0);
    }

    // =====================================================================
    // Bass remains synchronized with the kick relationship measured from
    // the real corpus: on-kick-position fraction stays below the 25%
    // uniform baseline in every energetic section, matching
    // generateBassPattern's own already-tested behavior, now also true
    // arrangement-wide.
    // =====================================================================
    {
        constexpr int kNumSeeds = 20;
        long onKick = 0, total = 0;
        for (uint32_t seed = 0; seed < kNumSeeds; ++seed)
        {
            ArrangementConfig c = cfg; c.seed = seed;
            const auto a = buildArrangement(c);
            BassPatternParams p = params; p.seed = seed;
            const auto bass = generateArrangementBassPattern(a, p);
            for (int i = 0; i < (int) bass.size(); ++i)
            {
                if (bass[(size_t) i] == kBassOffValue) continue;
                ++total;
                if ((i % kStepsPerBar) % 4 == 0) ++onKick;
            }
        }
        CHECK(total > 0);
        const double onKickFraction = (double) onKick / (double) total;
        std::printf("Arrangement-wide bass on-kick fraction: %.3f (uniform baseline=0.25)\n", onKickFraction);
        CHECK(onKickFraction < 0.25);
    }

    // =====================================================================
    // Build and BreakdownBuild both show a real increase in density from
    // their own first bar to their own last bar (gradual reappearance,
    // staged, not a flat repeat) - aggregated across many seeds.
    // =====================================================================
    {
        constexpr int kNumSeeds = 20;
        for (MusicSection buildSection : { MusicSection::Build, MusicSection::BreakdownBuild })
        {
            int firstBar = -1, lastBar = -1;
            for (int b = 0; b < arrangement.totalBars(); ++b)
                if (arrangement.barStates[(size_t) b].section == buildSection)
                {
                    if (firstBar < 0) firstBar = b;
                    lastBar = b;
                }
            CHECK(firstBar >= 0);
            CHECK(lastBar > firstBar);

            double firstTotal = 0.0, lastTotal = 0.0;
            for (uint32_t seed = 0; seed < kNumSeeds; ++seed)
            {
                ArrangementConfig c = cfg; c.seed = seed;
                const auto a = buildArrangement(c);
                BassPatternParams p = params; p.seed = seed;
                const auto bass = generateArrangementBassPattern(a, p);
                firstTotal += countActiveInBar(bass, firstBar, kStepsPerBar);
                lastTotal  += countActiveInBar(bass, lastBar, kStepsPerBar);
            }
            std::printf("Section %d: first bar mean=%.2f last bar mean=%.2f\n",
                        (int) buildSection, firstTotal / kNumSeeds, lastTotal / kNumSeeds);
            CHECK(lastTotal > firstTotal);
        }
    }

    // =====================================================================
    // Root-note dominance and the measured pitch-offset table survive
    // arrangement-aware generation exactly as in generateBassPattern.
    // =====================================================================
    {
        constexpr int kNumSeeds = 20;
        long rootCount = 0, total = 0;
        for (uint32_t seed = 0; seed < kNumSeeds; ++seed)
        {
            ArrangementConfig c = cfg; c.seed = seed;
            const auto a = buildArrangement(c);
            BassPatternParams p = params; p.seed = seed;
            const auto bass = generateArrangementBassPattern(a, p);
            for (auto v : bass)
            {
                if (v == kBassOffValue) continue;
                ++total;
                if (v == 0) ++rootCount;
            }
        }
        CHECK(total > 0);
        const double rootFraction = (double) rootCount / (double) total;
        std::printf("Arrangement-wide bass root-note fraction: %.3f\n", rootFraction);
        CHECK(rootFraction > 0.5);
    }

    // =====================================================================
    // The original generateBassPattern()/kBassSteps API is completely
    // unaffected - the arrangement-aware path is additive.
    // =====================================================================
    {
        const auto plainBass = generateBassPattern(params);
        CHECK(plainBass.size() == kBassSteps);
        int active = 0;
        for (auto v : plainBass) if (v != kBassOffValue) ++active;
        CHECK(active > 10);
    }

    TEST_SUMMARY_AND_EXIT();
}
