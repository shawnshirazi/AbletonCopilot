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
    StepGridConfig grid; // 16 steps/bar, 8 bars, no swing

    CHECK(totalSteps(grid) == 128);

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

    TEST_SUMMARY_AND_EXIT();
}
