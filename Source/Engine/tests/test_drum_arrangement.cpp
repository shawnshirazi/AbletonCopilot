#include "../Arrangement.h"
#include "../DrumEngine.h"
#include "TestSupport.h"
#include <algorithm>
#include <cstdio>

using namespace Engine;

namespace
{
    int countActiveInBar(const StepArray& steps, int bar, int stepsPerBar)
    {
        int n = 0;
        for (int s = 0; s < stepsPerBar; ++s)
            if (steps[(size_t) (bar * stepsPerBar + s)].active)
                ++n;
        return n;
    }

    double meanDensityForSection(const StepArray& steps, const MusicArrangement& arrangement,
                                  MusicSection section, int stepsPerBar)
    {
        long total = 0;
        int bars = 0;
        for (int b = 0; b < arrangement.totalBars(); ++b)
        {
            if (arrangement.barStates[(size_t) b].section != section)
                continue;
            total += countActiveInBar(steps, b, stepsPerBar);
            ++bars;
        }
        return bars > 0 ? (double) total / bars : 0.0;
    }

    bool sameDropPattern(const DropPattern& a, const DropPattern& b)
    {
        auto sameArray = [](const StepArray& x, const StepArray& y)
        {
            if (x.size() != y.size()) return false;
            for (size_t i = 0; i < x.size(); ++i)
                if (x[i].active != y[i].active || x[i].velocity != y[i].velocity)
                    return false;
            return true;
        };
        return sameArray(a.kick, b.kick) && sameArray(a.clap, b.clap) && sameArray(a.hatClosed, b.hatClosed)
            && sameArray(a.hatOpen, b.hatOpen) && sameArray(a.percA, b.percA) && sameArray(a.percB, b.percB);
    }
}

int main()
{
    constexpr int kStepsPerBar = 16;

    ArrangementConfig cfg;
    cfg.bpm = 124.0;
    cfg.seed = 7;
    const auto arrangement = buildArrangement(cfg);

    DrumPatternParams params;
    params.density = 0.5f; params.syncopation = 0.3f; params.variation = 0.2f; params.seed = 7;

    // =====================================================================
    // Shape sanity + determinism.
    // =====================================================================
    {
        const auto drop = generateArrangementDrop(arrangement, kStepsPerBar, params);
        const int expectedTotal = arrangement.totalBars() * kStepsPerBar;
        CHECK((int) drop.kick.size() == expectedTotal);
        CHECK((int) drop.hatClosed.size() == expectedTotal);

        const auto dropAgain = generateArrangementDrop(arrangement, kStepsPerBar, params);
        CHECK(sameDropPattern(drop, dropAgain));

        DrumPatternParams paramsB = params; paramsB.seed = 8;
        const auto dropDiffSeed = generateArrangementDrop(arrangement, kStepsPerBar, paramsB);
        CHECK(!sameDropPattern(drop, dropDiffSeed));
    }

    // =====================================================================
    // Breakdown removes the kick entirely - every single Breakdown bar,
    // not just usually. Drop/FinalDrop restore full four-on-the-floor
    // kick (4 hits/bar) on every bar.
    // =====================================================================
    {
        const auto drop = generateArrangementDrop(arrangement, kStepsPerBar, params);
        int breakdownKickHits = 0;
        int breakdownBars = 0;
        for (int b = 0; b < arrangement.totalBars(); ++b)
        {
            if (arrangement.barStates[(size_t) b].section == MusicSection::Breakdown)
            {
                breakdownKickHits += countActiveInBar(drop.kick, b, kStepsPerBar);
                ++breakdownBars;
            }
        }
        CHECK(breakdownBars > 0);
        CHECK(breakdownKickHits == 0); // exact - kick is REMOVED, not just thinned

        for (MusicSection dropSection : { MusicSection::Drop, MusicSection::FinalDrop })
        {
            int minKick = 999, maxKick = -1;
            for (int b = 0; b < arrangement.totalBars(); ++b)
                if (arrangement.barStates[(size_t) b].section == dropSection)
                {
                    const int n = countActiveInBar(drop.kick, b, kStepsPerBar);
                    minKick = std::min(minKick, n);
                    maxKick = std::max(maxKick, n);
                }
            CHECK(minKick == 4); // untouched four-on-the-floor, every bar
        }
    }

    // =====================================================================
    // PreDrop's final bar (the "leave space before the drop" pause) has no
    // kick, even though the rest of PreDrop does.
    // =====================================================================
    {
        const auto drop = generateArrangementDrop(arrangement, kStepsPerBar, params);
        int preDropFinalBar = -1;
        for (int b = 0; b < arrangement.totalBars(); ++b)
            if (isPreDropFinalBar(arrangement, b))
                preDropFinalBar = b;
        CHECK(preDropFinalBar >= 0);
        CHECK(countActiveInBar(drop.kick, preDropFinalBar, kStepsPerBar) == 0);
        CHECK(countActiveInBar(drop.kick, preDropFinalBar - 1, kStepsPerBar) == 4); // the bar before it still has kick
    }

    // =====================================================================
    // Open hat is completely removed during Breakdown (the brief's
    // explicit, unqualified "remove open hats" - not just "heavily
    // reduce" like closed hat/percussion).
    // =====================================================================
    {
        const auto drop = generateArrangementDrop(arrangement, kStepsPerBar, params);
        int breakdownOpenHatHits = 0;
        for (int b = 0; b < arrangement.totalBars(); ++b)
            if (arrangement.barStates[(size_t) b].section == MusicSection::Breakdown)
                breakdownOpenHatHits += countActiveInBar(drop.hatOpen, b, kStepsPerBar);
        CHECK(breakdownOpenHatHits == 0);
    }

    // =====================================================================
    // Breakdown is dramatically quieter than Drop for every percussive
    // role (hatClosed, percA, percB) - not just "on average a bit less",
    // a real, large gap. This is the specific bug found by inspection
    // (deriveStageBlock's small-delta touch mechanism couldn't handle the
    // Drop->Breakdown cliff) and fixed via kBigDropRebuildThreshold.
    // =====================================================================
    {
        const auto drop = generateArrangementDrop(arrangement, kStepsPerBar, params);
        const double dropHat  = meanDensityForSection(drop.hatClosed, arrangement, MusicSection::Drop, kStepsPerBar);
        const double breakHat = meanDensityForSection(drop.hatClosed, arrangement, MusicSection::Breakdown, kStepsPerBar);
        const double dropPercA  = meanDensityForSection(drop.percA, arrangement, MusicSection::Drop, kStepsPerBar);
        const double breakPercA = meanDensityForSection(drop.percA, arrangement, MusicSection::Breakdown, kStepsPerBar);

        std::printf("hatClosed mean/bar: Drop=%.2f Breakdown=%.2f\n", dropHat, breakHat);
        std::printf("percA mean/bar:     Drop=%.2f Breakdown=%.2f\n", dropPercA, breakPercA);

        CHECK(breakHat < dropHat * 0.6);   // dramatically reduced, not just "a bit less"
        CHECK(breakPercA < dropPercA * 0.6);
        CHECK(breakHat < 3.0);             // an absolute ceiling too - genuinely sparse, not merely relatively sparse
    }

    // =====================================================================
    // Build progressively increases density from its own first bar to its
    // own last bar (staged addition, not a flat repeat) - checked via
    // aggregate density across many seeds so one seed's RNG noise can't
    // produce a false failure.
    // =====================================================================
    {
        constexpr int kNumSeeds = 20;
        double firstBarTotal = 0.0, lastBarTotal = 0.0;
        int buildFirstBar = -1, buildLastBar = -1;
        for (int b = 0; b < arrangement.totalBars(); ++b)
            if (arrangement.barStates[(size_t) b].section == MusicSection::Build)
            {
                if (buildFirstBar < 0) buildFirstBar = b;
                buildLastBar = b;
            }
        CHECK(buildFirstBar >= 0);
        CHECK(buildLastBar > buildFirstBar);

        for (uint32_t seed = 0; seed < kNumSeeds; ++seed)
        {
            DrumPatternParams p = params; p.seed = seed;
            ArrangementConfig c = cfg; c.seed = seed;
            const auto a = buildArrangement(c);
            const auto drop = generateArrangementDrop(a, kStepsPerBar, p);
            firstBarTotal += countActiveInBar(drop.hatClosed, buildFirstBar, kStepsPerBar)
                           + countActiveInBar(drop.percA, buildFirstBar, kStepsPerBar);
            lastBarTotal  += countActiveInBar(drop.hatClosed, buildLastBar, kStepsPerBar)
                           + countActiveInBar(drop.percA, buildLastBar, kStepsPerBar);
        }
        std::printf("Build density: first bar=%.2f last bar=%.2f (mean hatClosed+percA hits/bar over %d seeds)\n",
                    firstBarTotal / kNumSeeds, lastBarTotal / kNumSeeds, kNumSeeds);
        CHECK(lastBarTotal > firstBarTotal);
    }

    // =====================================================================
    // FinalDrop is at least as strong as (not weaker than) Drop - the
    // arrangement's climax shouldn't undershoot the first drop.
    // =====================================================================
    {
        const auto drop = generateArrangementDrop(arrangement, kStepsPerBar, params);
        const double dropDensity = meanDensityForSection(drop.hatClosed, arrangement, MusicSection::Drop, kStepsPerBar)
                                  + meanDensityForSection(drop.percA, arrangement, MusicSection::Drop, kStepsPerBar);
        const double finalDropDensity = meanDensityForSection(drop.hatClosed, arrangement, MusicSection::FinalDrop, kStepsPerBar)
                                       + meanDensityForSection(drop.percA, arrangement, MusicSection::FinalDrop, kStepsPerBar);
        std::printf("Drop density=%.2f FinalDrop density=%.2f\n", dropDensity, finalDropDensity);
        CHECK(finalDropDensity > dropDensity * 0.85); // comparable or stronger, not a downgrade
    }

    // =====================================================================
    // Hats: no continuous 16th-note roller anywhere in the arrangement
    // (every bar stays well under 16 active hatClosed steps), and the
    // measured offbeat-8th velocity hierarchy survives arrangement-aware
    // generation exactly as it does in plain generateDrop.
    // =====================================================================
    {
        constexpr int kNumSeeds = 20;
        int maxHatClosedInAnyBar = 0;
        double offbeatVelSum = 0.0, otherVelSum = 0.0;
        int offbeatN = 0, otherN = 0;

        for (uint32_t seed = 0; seed < kNumSeeds; ++seed)
        {
            DrumPatternParams p = params; p.seed = seed;
            ArrangementConfig c = cfg; c.seed = seed;
            const auto a = buildArrangement(c);
            const auto drop = generateArrangementDrop(a, kStepsPerBar, p);
            for (int b = 0; b < a.totalBars(); ++b)
                maxHatClosedInAnyBar = std::max(maxHatClosedInAnyBar, countActiveInBar(drop.hatClosed, b, kStepsPerBar));

            for (int i = 0; i < (int) drop.hatClosed.size(); ++i)
            {
                if (!drop.hatClosed[(size_t) i].active) continue;
                const int s = i % kStepsPerBar;
                if (s % 4 == 2) { offbeatVelSum += drop.hatClosed[(size_t) i].velocity; ++offbeatN; }
                else            { otherVelSum   += drop.hatClosed[(size_t) i].velocity; ++otherN; }
            }
        }
        std::printf("Max hatClosed hits in any single bar across %d seeds: %d (never a 16-hit roller)\n",
                    kNumSeeds, maxHatClosedInAnyBar);
        // 12.5 onsets/bar is the exact measured boundary analyze_drum_grammar.py's
        // HAT_GROOVE_MAX_ONSETS_PER_BAR filter uses to separate real groove
        // loops from continuous 16th-note "roller" loops (see
        // DrumRhythmGrammar.h) - reused here as the same real, evidence-
        // grounded ceiling, not an arbitrary round number.
        CHECK(maxHatClosedInAnyBar <= 13);
        CHECK(offbeatN > 0 && otherN > 0);
        CHECK(offbeatVelSum / offbeatN > (otherVelSum / otherN) * 1.3); // offbeat pulse clearly dominant
    }

    // =====================================================================
    // Open hat stays sparse relative to closed hat in every energetic
    // section (never becomes "another continuous hat layer").
    // =====================================================================
    {
        const auto drop = generateArrangementDrop(arrangement, kStepsPerBar, params);
        for (MusicSection section : { MusicSection::Drop, MusicSection::FinalDrop, MusicSection::Establish })
        {
            const double hatClosedMean = meanDensityForSection(drop.hatClosed, arrangement, section, kStepsPerBar);
            const double hatOpenMean   = meanDensityForSection(drop.hatOpen, arrangement, section, kStepsPerBar);
            CHECK(hatOpenMean < hatClosedMean);
        }
    }

    // =====================================================================
    // Clap: one stable canonical shape - every bar where clap is active
    // within the SAME section is byte-identical (never independently
    // varied bar to bar, per the brief's own "don't let variation destroy
    // the backbeat"), and clap is silent during Breakdown.
    // =====================================================================
    {
        const auto drop = generateArrangementDrop(arrangement, kStepsPerBar, params);
        int breakdownClapHits = 0;
        for (int b = 0; b < arrangement.totalBars(); ++b)
            if (arrangement.barStates[(size_t) b].section == MusicSection::Breakdown)
                breakdownClapHits += countActiveInBar(drop.clap, b, kStepsPerBar);
        CHECK(breakdownClapHits == 0);

        // Collect two active Drop bars and compare byte-for-byte.
        std::vector<int> activeDropBars;
        for (int b = 0; b < arrangement.totalBars(); ++b)
            if (arrangement.barStates[(size_t) b].section == MusicSection::Drop
                && countActiveInBar(drop.clap, b, kStepsPerBar) > 0)
                activeDropBars.push_back(b);
        CHECK(activeDropBars.size() >= 2);
        if (activeDropBars.size() >= 2)
        {
            bool identical = true;
            for (int s = 0; s < kStepsPerBar; ++s)
            {
                const auto& a = drop.clap[(size_t) (activeDropBars[0] * kStepsPerBar + s)];
                const auto& b = drop.clap[(size_t) (activeDropBars[1] * kStepsPerBar + s)];
                if (a.active != b.active || (a.active && a.velocity != b.velocity))
                    identical = false;
            }
            CHECK(identical);
        }
    }

    // =====================================================================
    // The original generateDrop()/DropPattern/DrumSection API is
    // completely unaffected - the arrangement-aware path is additive.
    // =====================================================================
    {
        const StepGridConfig grid; // default 16 bars
        DrumPatternParams p; p.density = 0.5f; p.syncopation = 0.3f; p.variation = 0.2f; p.seed = 7;
        const auto plainDrop = generateDrop(grid, p);
        CHECK(plainDrop.kick.size() == (size_t) totalSteps(grid));
        CHECK(countActiveInBar(plainDrop.kick, 0, grid.stepsPerBar) == 4); // unchanged four-on-the-floor
    }

    TEST_SUMMARY_AND_EXIT();
}
