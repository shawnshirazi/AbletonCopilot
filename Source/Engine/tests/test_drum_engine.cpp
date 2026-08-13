#include "../DrumEngine.h"
#include "../Grid.h"
#include "TestSupport.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

using namespace Engine;

namespace
{
    int countActive(const StepArray& steps)
    {
        int n = 0;
        for (auto& h : steps)
            if (h.active) ++n;
        return n;
    }

    bool sameArray(const StepArray& a, const StepArray& b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (a[i].active != b[i].active || a[i].velocity != b[i].velocity)
                return false;
        return true;
    }

    // Quarter-note (strong-beat) positions within a 16-step bar: 0,4,8,12.
    bool isStrongBeat(int stepInBar, int stepsPerBar)
    {
        return (stepInBar % std::max(1, stepsPerBar / 4)) == 0;
    }

    // Weakest 16th position within a 16-step bar: 3,7,11,15 (same definition
    // DrumEngine.cpp's weakPositionBias uses).
    bool isWeakPos(int stepInBar, int stepsPerBar)
    {
        const int stepsPerBeat = std::max(1, stepsPerBar / 4);
        return (stepInBar % stepsPerBeat) == (stepsPerBeat - 1);
    }

    int countInWeakPositions(const StepArray& steps, const StepGridConfig& grid)
    {
        int n = 0;
        for (int i = 0; i < (int) steps.size(); ++i)
            if (steps[(size_t) i].active && isWeakPos(i % grid.stepsPerBar, grid.stepsPerBar))
                ++n;
        return n;
    }
}

int main()
{
    const StepGridConfig grid; // default: 16 steps/bar, 16 bars = 256 steps

    // A larger grid for tests that need statistical confidence on a
    // probabilistic effect (no flakiness from a single 8-bar sample) - a
    // single seed's worth of hits across 64 bars makes "basically always
    // happens" vs "basically never happens" unambiguous without needing to
    // sweep multiple seeds.
    const StepGridConfig bigGrid { 16, 64, 0.0f };

    // =====================================================================
    // KICK
    // =====================================================================
    {
        // density is documented as NOT USED for kick - prove it: identical
        // params except density must produce byte-identical output.
        DrumPatternParams pLowDensity;
        pLowDensity.density = 0.0f; pLowDensity.syncopation = 0.3f; pLowDensity.variation = 0.2f; pLowDensity.seed = 7;
        DrumPatternParams pHighDensity = pLowDensity;
        pHighDensity.density = 1.0f;
        CHECK(sameArray(generateKick(grid, pLowDensity), generateKick(grid, pHighDensity)));

        // syncopation=0, variation=0 -> exactly the four-on-the-floor
        // baseline, nothing else: exactly 4 hits/bar * 8 bars = 32, all on
        // strong beats.
        DrumPatternParams pFlat;
        pFlat.density = 0.5f; pFlat.syncopation = 0.0f; pFlat.variation = 0.0f; pFlat.seed = 1;
        auto kickFlat = generateKick(grid, pFlat);
        CHECK(countActive(kickFlat) == grid.numBars * 4);
        for (int i = 0; i < (int) kickFlat.size(); ++i)
            if (kickFlat[(size_t) i].active)
                CHECK(isStrongBeat(i % grid.stepsPerBar, grid.stepsPerBar));

        // syncopation=1 on a big grid -> pushed hits (off the
        // four-on-the-floor grid) must actually appear; syncopation=0 must
        // never produce any, even on the same big grid/seed.
        DrumPatternParams pSyncOn = pFlat;  pSyncOn.syncopation = 1.0f; pSyncOn.seed = 42;
        DrumPatternParams pSyncOff = pFlat; pSyncOff.syncopation = 0.0f; pSyncOff.seed = 42;
        auto kickSyncOnBig  = generateKick(bigGrid, pSyncOn);
        auto kickSyncOffBig = generateKick(bigGrid, pSyncOff);
        CHECK(countActive(kickSyncOnBig) > countActive(kickSyncOffBig));
        CHECK(countActive(kickSyncOffBig) == bigGrid.numBars * 4); // no pushes at all

        // variation=1 on a big grid -> beats get dropped (fewer than the
        // full four-on-the-floor count); variation=0 never drops any.
        DrumPatternParams pVarOn = pFlat;  pVarOn.variation = 1.0f; pVarOn.seed = 42;
        DrumPatternParams pVarOff = pFlat; pVarOff.variation = 0.0f; pVarOff.seed = 42;
        auto kickVarOnBig  = generateKick(bigGrid, pVarOn);
        auto kickVarOffBig = generateKick(bigGrid, pVarOff);
        CHECK(countActive(kickVarOnBig) < countActive(kickVarOffBig));
        CHECK(countActive(kickVarOffBig) == bigGrid.numBars * 4); // never drops a beat

        // Determinism: identical seed+params -> byte-identical output.
        CHECK(sameArray(generateKick(grid, pFlat), generateKick(grid, pFlat)));

        // Different seed -> output actually differs somewhere (with
        // syncopation/variation both active so there's real randomness to
        // differ on).
        DrumPatternParams pA = pFlat; pA.syncopation = 0.5f; pA.variation = 0.5f; pA.seed = 1;
        DrumPatternParams pB = pA;    pB.seed = 2;
        CHECK(!sameArray(generateKick(grid, pA), generateKick(grid, pB)));

        printPattern("KICK (default params)", generateKick(grid, DrumPatternParams{}), grid);
    }

    // =====================================================================
    // CLAP
    // =====================================================================
    {
        DrumPatternParams pBase;
        pBase.density = 0.0f; pBase.syncopation = 0.0f; pBase.variation = 0.0f; pBase.seed = 3;

        // density: 0 -> only the fixed sourced hits (3 per 4-bar cycle: 1
        // single + 2-hit doublet = 3), no ghosts. 1 -> meaningfully more.
        auto clapDensity0 = generateClap(bigGrid, pBase);
        DrumPatternParams pDensity1 = pBase; pDensity1.density = 1.0f;
        auto clapDensity1 = generateClap(bigGrid, pDensity1);
        const int cyclesInBigGrid = bigGrid.numBars / 4;
        CHECK(countActive(clapDensity0) == cyclesInBigGrid * 3);
        CHECK(countActive(clapDensity1) > countActive(clapDensity0));

        // syncopation: 0 -> every sourced hit lands exactly on the
        // canonical backbeat position (or the doublet's fixed gap offset),
        // never jittered. 1 -> jittered hits actually appear somewhere.
        const int backbeatStep   = (grid.stepsPerBar * 3) / 4;
        const int doubletGapStep = std::max(1, grid.stepsPerBar / 8);
        auto clapSync0 = generateClap(bigGrid, pBase);
        bool anyOffCanonical = false;
        for (int i = 0; i < (int) clapSync0.size(); ++i)
        {
            if (!clapSync0[(size_t) i].active) continue;
            const int s = i % grid.stepsPerBar;
            if (s != backbeatStep && s != backbeatStep + doubletGapStep)
                anyOffCanonical = true;
        }
        CHECK(!anyOffCanonical);

        DrumPatternParams pSync1 = pBase; pSync1.syncopation = 1.0f;
        auto clapSync1 = generateClap(bigGrid, pSync1);
        bool anyJittered = false;
        for (int i = 0; i < (int) clapSync1.size(); ++i)
        {
            if (!clapSync1[(size_t) i].active) continue;
            const int s = i % grid.stepsPerBar;
            if (s != backbeatStep && s != backbeatStep + doubletGapStep)
                anyJittered = true;
        }
        CHECK(anyJittered);

        // variation: exact boundary behaviour (see DrumEngine.cpp comment -
        // uniform01's [0,1) range makes both boundaries deterministic, not
        // just statistically likely). 0 -> every cycle uses the canonical
        // bar-2=single/bar-4=doublet layout (bar-2 has exactly 1 hit,
        // bar-4 has exactly 2). 1 -> every cycle is swapped (bar-2 has 2,
        // bar-4 has 1).
        auto countInBar = [&](const StepArray& steps, int barIndex)
        {
            int n = 0;
            for (int s = 0; s < grid.stepsPerBar; ++s)
                if (steps[(size_t) (barIndex * grid.stepsPerBar + s)].active) ++n;
            return n;
        };
        auto clapVar0 = generateClap(grid, pBase); // grid: 16 bars = 4 cycles
        CHECK(countInBar(clapVar0, 1) == 1);
        CHECK(countInBar(clapVar0, 3) == 2);
        CHECK(countInBar(clapVar0, 5) == 1);
        CHECK(countInBar(clapVar0, 7) == 2);
        CHECK(countInBar(clapVar0, 9) == 1);
        CHECK(countInBar(clapVar0, 11) == 2);
        CHECK(countInBar(clapVar0, 13) == 1);
        CHECK(countInBar(clapVar0, 15) == 2);

        DrumPatternParams pVar1 = pBase; pVar1.variation = 1.0f;
        auto clapVar1 = generateClap(grid, pVar1);
        CHECK(countInBar(clapVar1, 1) == 2);
        CHECK(countInBar(clapVar1, 3) == 1);
        CHECK(countInBar(clapVar1, 5) == 2);
        CHECK(countInBar(clapVar1, 7) == 1);
        CHECK(countInBar(clapVar1, 9) == 2);
        CHECK(countInBar(clapVar1, 11) == 1);
        CHECK(countInBar(clapVar1, 13) == 2);
        CHECK(countInBar(clapVar1, 15) == 1);

        // Determinism + different-seed variation.
        CHECK(sameArray(generateClap(grid, pBase), generateClap(grid, pBase)));
        DrumPatternParams pSeedA = pBase; pSeedA.density = 0.5f; pSeedA.syncopation = 0.5f; pSeedA.seed = 10;
        DrumPatternParams pSeedB = pSeedA; pSeedB.seed = 11;
        CHECK(!sameArray(generateClap(grid, pSeedA), generateClap(grid, pSeedB)));

        printPattern("CLAP (default params)", generateClap(grid, DrumPatternParams{}), grid);
    }

    // =====================================================================
    // HAT
    // =====================================================================
    {
        DrumPatternParams pBase;
        pBase.density = 0.0f; pBase.syncopation = 0.0f; pBase.variation = 0.0f; pBase.seed = 5;

        // density: 0 -> only the primary off-beat-8th pulse (4/bar), no
        // second layer at all. 1 -> meaningfully more.
        auto hatDensity0 = generateHat(grid, pBase);
        CHECK(countActive(hatDensity0) == grid.numBars * 4);
        DrumPatternParams pDensity1 = pBase; pDensity1.density = 1.0f;
        auto hatDensity1 = generateHat(grid, pDensity1);
        CHECK(countActive(hatDensity1) > countActive(hatDensity0));

        // syncopation: second-layer hits should concentrate on the weakest
        // 16th positions (3/7/11/15) as syncopation increases. Use a dense
        // second layer (density=1) on the big grid so there's enough
        // second-layer material to measure the bias reliably, and compare
        // the WEAK-POSITION SHARE of second-layer hits (not raw count,
        // since density also changes total count) between syncopation=0
        // and syncopation=1.
        DrumPatternParams pSync0 = pBase; pSync0.density = 1.0f; pSync0.syncopation = 0.0f;
        DrumPatternParams pSync1 = pBase; pSync1.density = 1.0f; pSync1.syncopation = 1.0f;
        auto hatSync0Big = generateHat(bigGrid, pSync0);
        auto hatSync1Big = generateHat(bigGrid, pSync1);

        // Weak-position SHARE of all active hits (primary pulse included -
        // it's never at a weak position by construction, so it only
        // dilutes the share, which both sides are diluted by equally).
        const double shareSync0 = double(countInWeakPositions(hatSync0Big, bigGrid)) / double(countActive(hatSync0Big));
        const double shareSync1 = double(countInWeakPositions(hatSync1Big, bigGrid)) / double(countActive(hatSync1Big));
        CHECK(shareSync1 > shareSync0);

        // variation: even-indexed bars get thinned relative to odd bars.
        // 0 -> even/odd bar hit counts should be close (same probability
        // every bar); 1 -> even bars are clearly sparser than odd bars.
        DrumPatternParams pVar0 = pBase; pVar0.density = 1.0f; pVar0.variation = 0.0f;
        DrumPatternParams pVar1 = pBase; pVar1.density = 1.0f; pVar1.variation = 1.0f;
        auto hatVar0Big = generateHat(bigGrid, pVar0);
        auto hatVar1Big = generateHat(bigGrid, pVar1);

        auto evenOddCounts = [&](const StepArray& steps)
        {
            int even = 0, odd = 0;
            for (int bar = 0; bar < bigGrid.numBars; ++bar)
            {
                int c = 0;
                for (int s = 0; s < bigGrid.stepsPerBar; ++s)
                    if (steps[(size_t) (bar * bigGrid.stepsPerBar + s)].active) ++c;
                (bar % 2 == 0 ? even : odd) += c;
            }
            return std::pair<int, int>(even, odd);
        };
        auto [even0, odd0] = evenOddCounts(hatVar0Big);
        auto [even1, odd1] = evenOddCounts(hatVar1Big);
        // At variation=0 even/odd should be close (within 15% of each
        // other - same underlying probability, sampling noise only).
        CHECK(std::abs(even0 - odd0) < odd0 * 0.15);
        // At variation=1 even bars are structurally thinned to ~half -
        // clearly and unambiguously lower than odd bars.
        CHECK(even1 < odd1 * 0.75);

        // Determinism + different-seed variation.
        CHECK(sameArray(generateHat(grid, pBase), generateHat(grid, pBase)));
        DrumPatternParams pSeedA = pBase; pSeedA.density = 0.5f; pSeedA.seed = 20;
        DrumPatternParams pSeedB = pSeedA; pSeedB.seed = 21;
        CHECK(!sameArray(generateHat(grid, pSeedA), generateHat(grid, pSeedB)));

        printPattern("HAT (default params)", generateHat(grid, DrumPatternParams{}), grid);
    }

    // =====================================================================
    // PERC
    // =====================================================================
    {
        DrumPatternParams pBase;
        pBase.density = 0.0f; pBase.syncopation = 0.0f; pBase.variation = 0.0f; pBase.seed = 9;

        // density: 0 -> silent. 1 -> meaningfully more than a mid density.
        auto percDensity0 = generatePerc(grid, pBase);
        CHECK(countActive(percDensity0) == 0);

        DrumPatternParams pDensityMid = pBase; pDensityMid.density = 0.5f;
        DrumPatternParams pDensity1   = pBase; pDensity1.density   = 1.0f;
        auto percDensityMid = generatePerc(bigGrid, pDensityMid);
        auto percDensity1   = generatePerc(bigGrid, pDensity1);
        CHECK(countActive(percDensity1) > countActive(percDensityMid));
        // Still an accent role even at max density - shouldn't approach
        // anywhere near every step being active.
        CHECK(countActive(percDensity1) < (int) percDensity1.size() / 2);

        // syncopation (fixed this pass - now placement bias, not velocity):
        // weak-position share should be higher at syncopation=1 than at 0,
        // same measurement approach as HAT above.
        DrumPatternParams pSync0 = pBase; pSync0.density = 1.0f; pSync0.syncopation = 0.0f;
        DrumPatternParams pSync1 = pBase; pSync1.density = 1.0f; pSync1.syncopation = 1.0f;
        auto percSync0Big = generatePerc(bigGrid, pSync0);
        auto percSync1Big = generatePerc(bigGrid, pSync1);
        const double percShare0 = double(countInWeakPositions(percSync0Big, bigGrid)) / double(countActive(percSync0Big));
        const double percShare1 = double(countInWeakPositions(percSync1Big, bigGrid)) / double(countActive(percSync1Big));
        CHECK(percShare1 > percShare0);

        // variation: even-bar thinning (same mechanism as HAT) plus a
        // wider velocity range. Check both effects independently.
        DrumPatternParams pVar0 = pBase; pVar0.density = 1.0f; pVar0.variation = 0.0f;
        DrumPatternParams pVar1 = pBase; pVar1.density = 1.0f; pVar1.variation = 1.0f;
        auto percVar0Big = generatePerc(bigGrid, pVar0);
        auto percVar1Big = generatePerc(bigGrid, pVar1);

        auto evenOddCountsP = [&](const StepArray& steps)
        {
            int even = 0, odd = 0;
            for (int bar = 0; bar < bigGrid.numBars; ++bar)
            {
                int c = 0;
                for (int s = 0; s < bigGrid.stepsPerBar; ++s)
                    if (steps[(size_t) (bar * bigGrid.stepsPerBar + s)].active) ++c;
                (bar % 2 == 0 ? even : odd) += c;
            }
            return std::pair<int, int>(even, odd);
        };
        auto [pEven0, pOdd0] = evenOddCountsP(percVar0Big);
        auto [pEven1, pOdd1] = evenOddCountsP(percVar1Big);
        CHECK(std::abs(pEven0 - pOdd0) < pOdd0 * 0.20);
        CHECK(pEven1 < pOdd1 * 0.75);

        // Velocity range widens with variation - max velocity seen across
        // a big, dense sample should be higher at variation=1 (range
        // 0.4-0.7) than at variation=0 (range 0.4-0.4, i.e. always 0.4).
        // Formula: velRange = 0.3 + variation*0.3, vel = min(1, 0.4 + rand*velRange).
        // variation=0 -> range [0.4, 0.7]; variation=1 -> range [0.4, 1.0].
        // A non-zero base spread even at variation=0 is intentional (a real
        // hit is never perfectly flat-velocity) - "narrow vs. wide", not
        // "flat vs. varied".
        float maxVelVar0 = 0.0f, maxVelVar1 = 0.0f;
        for (auto& h : percVar0Big) if (h.active) maxVelVar0 = std::max(maxVelVar0, h.velocity);
        for (auto& h : percVar1Big) if (h.active) maxVelVar1 = std::max(maxVelVar1, h.velocity);
        CHECK(maxVelVar0 <= 0.71f);  // never exceeds the variation=0 range's ceiling
        CHECK(maxVelVar1 > 0.71f);   // big grid guarantees reaching well into the wider variation=1 range
        CHECK(maxVelVar1 > maxVelVar0);

        // Determinism + different-seed variation.
        DrumPatternParams pDet = pBase; pDet.density = 0.5f;
        CHECK(sameArray(generatePerc(grid, pDet), generatePerc(grid, pDet)));
        DrumPatternParams pSeedA = pDet; pSeedA.seed = 30;
        DrumPatternParams pSeedB = pDet; pSeedB.seed = 31;
        CHECK(!sameArray(generatePerc(grid, pSeedA), generatePerc(grid, pSeedB)));

        printPattern("PERC (default params)", generatePerc(grid, DrumPatternParams{}), grid);
    }

    TEST_SUMMARY_AND_EXIT();
}
