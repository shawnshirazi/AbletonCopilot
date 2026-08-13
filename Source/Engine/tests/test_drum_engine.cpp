#include "../DrumEngine.h"
#include "../Grid.h"
#include "TestSupport.h"

using namespace Engine;

static int countActive(const StepArray& steps)
{
    int n = 0;
    for (auto& h : steps)
        if (h.active) ++n;
    return n;
}

int main()
{
    StepGridConfig grid; // 16 steps/bar, 8 bars = 128 steps
    DrumPatternParams params; // defaults: density 0.5, syncopation 0.3, variation 0.2

    // --- Kick ---
    {
        auto kick = generateKick(grid, params);
        CHECK((int) kick.size() == totalSteps(grid));

        // Every bar's downbeat (step 0 of the bar) should be a kick in the
        // overwhelming common case - with variation=0.2 and drop
        // probability scaled to 0.15*variation = 3%, no single bar is
        // guaranteed, but across 8 bars at least most downbeats must hit.
        int downbeats = 0;
        for (int bar = 0; bar < grid.numBars; ++bar)
            if (kick[(size_t) (bar * grid.stepsPerBar)].active)
                ++downbeats;
        CHECK(downbeats >= grid.numBars - 2); // allow for rare drops

        // Determinism: same seed -> identical output.
        auto kick2 = generateKick(grid, params);
        CHECK(kick.size() == kick2.size());
        bool identical = true;
        for (size_t i = 0; i < kick.size(); ++i)
            if (kick[i].active != kick2[i].active || kick[i].velocity != kick2[i].velocity)
                identical = false;
        CHECK(identical);

        // Different seed -> can (not must, but overwhelmingly likely with
        // real randomized embellishments) differ somewhere, at minimum the
        // function must not crash/mismatch size.
        DrumPatternParams params2 = params;
        params2.seed = 12345;
        auto kick3 = generateKick(grid, params2);
        CHECK(kick3.size() == kick.size());
    }

    // --- Clap ---
    {
        auto clap = generateClap(grid, params);
        CHECK((int) clap.size() == totalSteps(grid));
        // Sparse by design - clap should never approach kick-like density.
        CHECK(countActive(clap) < totalSteps(grid) / 2);
        // The sourced pattern's bar-2 single hit should exist somewhere in
        // bar index 1 (0-based) of the first 4-bar cycle.
        bool hitInBar1 = false;
        for (int s = grid.stepsPerBar; s < grid.stepsPerBar * 2; ++s)
            if (clap[(size_t) s].active) hitInBar1 = true;
        CHECK(hitInBar1);
    }

    // --- Hat ---
    {
        auto hat = generateHat(grid, params);
        CHECK((int) hat.size() == totalSteps(grid));
        // Off-beat 8th primary pulse should be present in every bar (step
        // 2 of each 16-step bar, i.e. the first off-beat 8th).
        for (int bar = 0; bar < grid.numBars; ++bar)
            CHECK(hat[(size_t) (bar * grid.stepsPerBar + 2)].active);
        // Primary-pulse hits should read louder than the secondary layer.
        CHECK(hat[2].velocity > 0.6f);
    }

    // --- Perc ---
    {
        auto perc = generatePerc(grid, params);
        CHECK((int) perc.size() == totalSteps(grid));
        // Sparse - well under half the grid active at default density.
        CHECK(countActive(perc) < totalSteps(grid) / 2);

        // density scaling: near-zero density should produce a near-empty
        // pattern; higher density should produce more hits (same seed).
        DrumPatternParams lowDensity = params;
        lowDensity.density = 0.0f;
        auto percLow = generatePerc(grid, lowDensity);
        CHECK(countActive(percLow) == 0);

        DrumPatternParams highDensity = params;
        highDensity.density = 1.0f;
        auto percHigh = generatePerc(grid, highDensity);
        CHECK(countActive(percHigh) > countActive(perc));
    }

    TEST_SUMMARY_AND_EXIT();
}
