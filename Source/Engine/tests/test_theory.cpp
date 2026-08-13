#include "../Theory.h"
#include "TestSupport.h"
#include <cstring>
#include <initializer_list>

using namespace Engine;

int main()
{
    // Aeolian (natural minor) scale tones relative to root.
    for (int t : { 0, 2, 3, 5, 7, 8, 10 })
        CHECK(isInScale(t, ScaleType::Aeolian));
    for (int t : { 1, 4, 6, 9, 11 })
        CHECK(!isInScale(t, ScaleType::Aeolian));

    // Dorian has a raised 6th (9) vs Aeolian's 8.
    CHECK(isInScale(9, ScaleType::Dorian));
    CHECK(!isInScale(9, ScaleType::Aeolian));

    // Ionian (major) scale tones.
    for (int t : { 0, 2, 4, 5, 7, 9, 11 })
        CHECK(isInScale(t, ScaleType::Ionian));

    // Octave/negative invariance - scale membership only depends on pitch class.
    CHECK(isInScale(12, ScaleType::Aeolian));   // octave up
    CHECK(isInScale(-2, ScaleType::Aeolian));   // -2 mod 12 == 10, in Aeolian
    CHECK(!isInScale(-1, ScaleType::Aeolian));  // -1 mod 12 == 11, not in Aeolian

    // nearestInScale: in-scale input returns unchanged.
    CHECK(nearestInScale(3, ScaleType::Aeolian) == 3);
    // 1 is out of Aeolian - nearest should be 0 or 2 (both distance 1).
    {
        int n = nearestInScale(1, ScaleType::Aeolian);
        CHECK(n == 0 || n == 2);
        CHECK(isInScale(n, ScaleType::Aeolian));
    }
    // 6 is out of Aeolian (5 and 7 are both distance 1 away).
    {
        int n = nearestInScale(6, ScaleType::Aeolian);
        CHECK(n == 5 || n == 7);
    }

    // degreeToSemitone: degree 0 is always the root; degree 7 is one octave up.
    CHECK(degreeToSemitone(0, ScaleType::Aeolian) == 0);
    CHECK(degreeToSemitone(7, ScaleType::Aeolian) == 12);
    CHECK(degreeToSemitone(-7, ScaleType::Aeolian) == -12);
    // Aeolian degree 2 -> semitone 3 (minor third).
    CHECK(degreeToSemitone(2, ScaleType::Aeolian) == 3);
    // Ionian degree 2 -> semitone 4 (major third).
    CHECK(degreeToSemitone(2, ScaleType::Ionian) == 4);

    // noteName - basic sanity + octave/sign invariance.
    CHECK(std::strcmp(noteName(0), "C") == 0);
    CHECK(std::strcmp(noteName(1), "C#") == 0);
    CHECK(std::strcmp(noteName(11), "B") == 0);
    CHECK(std::strcmp(noteName(12), "C") == 0);
    CHECK(std::strcmp(noteName(-1), "B") == 0);

    TEST_SUMMARY_AND_EXIT();
}
