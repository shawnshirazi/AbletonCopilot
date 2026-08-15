#include "../Grid.h"
#include "TestSupport.h"
#include <cmath>

using namespace Engine;

static bool approxEqual(double a, double b, double eps = 1e-9)
{
    return std::fabs(a - b) < eps;
}

int main()
{
    StepGridConfig grid; // 16 steps/bar, 16 bars, no swing

    CHECK(totalSteps(grid) == 256);

    StepGridConfig small { 4, 2, 0.0f };
    CHECK(totalSteps(small) == 8);

    // At 120 BPM, a bar is 2 seconds (4 beats * 0.5s/beat); with 16
    // steps/bar, each 16th note is 0.125s.
    const double bpm = 120.0;
    CHECK(approxEqual(stepTimeSeconds(0, bpm, grid), 0.0));
    CHECK(approxEqual(stepTimeSeconds(1, bpm, grid), 0.125));
    CHECK(approxEqual(stepTimeSeconds(4, bpm, grid), 0.5));   // one beat in
    CHECK(approxEqual(stepTimeSeconds(16, bpm, grid), 2.0));  // one bar in

    // No swing - odd steps land exactly on-grid too.
    CHECK(approxEqual(stepTimeSeconds(3, bpm, grid), 0.375));

    // Full swing (1.0): odd (off-beat) 16ths shift later by 1/6 of an
    // 8th-note; even steps are untouched.
    StepGridConfig swung { 16, 8, 1.0f };
    const double secPerStep   = 0.125;
    const double eighthNoteSec = secPerStep * 2.0;
    const double expectedShift = eighthNoteSec / 6.0;

    CHECK(approxEqual(stepTimeSeconds(0, bpm, swung), 0.0));           // even - untouched
    CHECK(approxEqual(stepTimeSeconds(2, bpm, swung), 0.25));          // even - untouched
    CHECK(approxEqual(stepTimeSeconds(1, bpm, swung), secPerStep + expectedShift)); // odd - shifted
    CHECK(approxEqual(stepTimeSeconds(3, bpm, swung), 3.0 * secPerStep + expectedShift));

    // Half swing shifts by exactly half as much as full swing.
    StepGridConfig halfSwung { 16, 8, 0.5f };
    CHECK(approxEqual(stepTimeSeconds(1, bpm, halfSwung), secPerStep + expectedShift * 0.5));

    // Tempo scaling: doubling BPM halves every step time.
    CHECK(approxEqual(stepTimeSeconds(4, bpm * 2.0, grid), 0.25));

    // ---- Real playback swing (Part 4 of the groove-improvement pass):
    // PluginProcessor.cpp's generated-drum trigger loop delays hatClosed/
    // percA/percB's off-beat 16ths by exactly
    // stepTimeSeconds(step, bpm, swungGrid) - stepTimeSeconds(step, bpm,
    // straightGrid) at a sourced 10% swing amount (8-12% converging across
    // two independent guides - see PluginProcessor.cpp's kSwingAmount
    // comment). This checks that exact delta formula at that exact value,
    // deterministic and reproducible - same inputs always produce the
    // same delay, never per-cycle random jitter. ----
    {
        constexpr float kSourcedSwingAmount = 0.10f;
        StepGridConfig straight16 { 16, 16, 0.0f };
        StepGridConfig swung16    { 16, 16, kSourcedSwingAmount };

        // Even (on-beat) steps: zero extra delay at any swing amount.
        for (int step : { 0, 2, 4, 100, 254 })
            CHECK(approxEqual(stepTimeSeconds(step, 124.0, swung16) - stepTimeSeconds(step, 124.0, straight16), 0.0));

        // Odd (off-beat) steps: a real, positive, deterministic delay -
        // same 1/6-of-an-8th-note-scaled-by-swing shape test_grid.cpp
        // already establishes above, just re-checked at the ACTUAL
        // shipped value instead of only 1.0/0.5.
        const double bpmForSwing   = 124.0; // matches this codebase's default tempo
        const double secPerStep124 = (60.0 / bpmForSwing) * 4.0 / 16.0;
        const double expectedDelta = kSourcedSwingAmount * (secPerStep124 * 2.0 / 6.0);
        for (int step : { 1, 3, 5, 101, 255 })
        {
            const double delta = stepTimeSeconds(step, bpmForSwing, swung16) - stepTimeSeconds(step, bpmForSwing, straight16);
            CHECK(approxEqual(delta, expectedDelta));
            CHECK(delta > 0.0); // a real, audible delay, not a no-op
        }

        // Deterministic: calling the same inputs twice gives the exact
        // same result (no time-based/random component).
        CHECK(approxEqual(stepTimeSeconds(101, bpmForSwing, swung16), stepTimeSeconds(101, bpmForSwing, swung16)));

        // Sanity: at 124 BPM this is a genuinely small, "groove" amount of
        // delay, not something large enough to sound like a timing error
        // (a few milliseconds, matching "SP1200 Swing-67 at 10%"-scale
        // guidance, not e.g. tens of milliseconds).
        CHECK(expectedDelta < 0.02);
    }

    TEST_SUMMARY_AND_EXIT();
}
