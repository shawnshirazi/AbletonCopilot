#include "../DrumSampleSelection.h"
#include "TestSupport.h"

using namespace Engine;

namespace
{
    const DrumRole kAllRoles[6] = { DrumRole::Kick, DrumRole::Clap, DrumRole::HatClosed,
                                     DrumRole::HatOpen, DrumRole::PercA, DrumRole::PercB };
}

int main()
{
    // =====================================================================
    // No candidates -> -1, not a crash or an out-of-range index.
    // =====================================================================
    {
        for (auto role : kAllRoles)
            CHECK(selectSampleIndex(role, 42, 0) == -1);
    }

    // =====================================================================
    // Single candidate -> always index 0, regardless of seed.
    // =====================================================================
    {
        for (auto role : kAllRoles)
        {
            CHECK(selectSampleIndex(role, 1, 1) == 0);
            CHECK(selectSampleIndex(role, 999, 1) == 0);
        }
    }

    // =====================================================================
    // Same seed -> same selection, every time, for every role.
    // =====================================================================
    {
        for (auto role : kAllRoles)
        {
            const int a = selectSampleIndex(role, 12345, 20);
            const int b = selectSampleIndex(role, 12345, 20);
            CHECK(a == b);
            CHECK(a >= 0 && a < 20);
        }
    }

    // =====================================================================
    // Index is always in range [0, candidateCount) across many seeds/sizes.
    // =====================================================================
    {
        for (auto role : kAllRoles)
            for (uint32_t seed = 0; seed < 200; ++seed)
                for (int n : { 2, 3, 5, 8, 13, 50 })
                {
                    const int idx = selectSampleIndex(role, seed, n);
                    CHECK(idx >= 0 && idx < n);
                }
    }

    // =====================================================================
    // Different seeds explore different indices (not literally guaranteed
    // for every pair, but across many seeds on a reasonably large
    // candidate pool, the selection must not collapse to a single index).
    // =====================================================================
    {
        for (auto role : kAllRoles)
        {
            bool sawDifferent = false;
            const int first = selectSampleIndex(role, 0, 30);
            for (uint32_t seed = 1; seed < 30; ++seed)
            {
                if (selectSampleIndex(role, seed, 30) != first)
                {
                    sawDifferent = true;
                    break;
                }
            }
            CHECK(sawDifferent);
        }
    }

    // =====================================================================
    // Roles are salted independently - same seed, same candidateCount,
    // different role can (and typically does) pick a different index. Not
    // asserting they're ALWAYS different (a coincidence is possible), but
    // at least one role must differ from Kick's choice across a spread of
    // seeds, proving the salts actually do something.
    // =====================================================================
    {
        bool anyRoleDiffersFromKick = false;
        for (uint32_t seed = 0; seed < 20 && !anyRoleDiffersFromKick; ++seed)
        {
            const int kickIdx = selectSampleIndex(DrumRole::Kick, seed, 40);
            for (auto role : { DrumRole::Clap, DrumRole::HatClosed, DrumRole::HatOpen, DrumRole::PercA, DrumRole::PercB })
            {
                if (selectSampleIndex(role, seed, 40) != kickIdx)
                {
                    anyRoleDiffersFromKick = true;
                    break;
                }
            }
        }
        CHECK(anyRoleDiffersFromKick);
    }

    TEST_SUMMARY_AND_EXIT();
}
