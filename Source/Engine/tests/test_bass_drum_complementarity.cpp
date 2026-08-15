// Regression tests for real bass/drum complementarity (Part 3 of the
// groove-improvement pass): proves generateBassLoop16's bass activation is
// measurably lower on steps the REAL generated kick/hatClosed/percA
// occupy than on steps they don't - not just that the correlation code
// exists, but that it actually changes the output. Aggregated over many
// seeds (a single seed's random noise could accidentally show the
// opposite of the intended effect even with correct code) - this is a
// statistical claim about the mechanism, checked the same way, not a
// single anecdotal example.
#include "../MusicIdentity.h"
#include "TestSupport.h"
#include <cstdio>

using namespace Engine;

namespace
{
    // Fraction of (bar,step) positions where bass is active, restricted to
    // positions where `role` is active vs. positions where it isn't.
    struct OccupancySplit { double onFraction; double offFraction; int onCount; int offCount; };

    OccupancySplit splitByRoleOccupancy(const std::vector<int8_t>& bass, const StepArray& role)
    {
        int onActive = 0, onTotal = 0, offActive = 0, offTotal = 0;
        const size_t n = std::min(bass.size(), role.size());
        for (size_t i = 0; i < n; ++i)
        {
            const bool bassActive = bass[i] != kBassOffValue;
            if (role[i].active) { onTotal++; if (bassActive) onActive++; }
            else                { offTotal++; if (bassActive) offActive++; }
        }
        OccupancySplit s;
        s.onFraction  = onTotal  > 0 ? (double) onActive  / (double) onTotal  : 0.0;
        s.offFraction = offTotal > 0 ? (double) offActive / (double) offTotal : 0.0;
        s.onCount  = onTotal;
        s.offCount = offTotal;
        return s;
    }
}

int main()
{
    constexpr int kNumSeeds = 60;

    double sumKickOn = 0.0, sumKickOff = 0.0;
    double sumHatOn = 0.0, sumHatOff = 0.0;
    double sumPercOn = 0.0, sumPercOff = 0.0;
    int seedsWithKickData = 0, seedsWithHatData = 0, seedsWithPercData = 0;

    for (uint32_t seed = 1; seed <= (uint32_t) kNumSeeds; ++seed)
    {
        MusicIdentityParams params;
        params.seed     = seed;
        params.bpm      = 124.0;
        params.rootNote = 9;
        params.isMinor  = true;
        const MusicIdentity id = generateMusicIdentity(params);

        const auto kickSplit = splitByRoleOccupancy(id.bassMotif, id.drumMotif.kick);
        if (kickSplit.onCount > 0 && kickSplit.offCount > 0)
        {
            sumKickOn  += kickSplit.onFraction;
            sumKickOff += kickSplit.offFraction;
            ++seedsWithKickData;
        }

        const auto hatSplit = splitByRoleOccupancy(id.bassMotif, id.drumMotif.hatClosed);
        if (hatSplit.onCount > 0 && hatSplit.offCount > 0)
        {
            sumHatOn  += hatSplit.onFraction;
            sumHatOff += hatSplit.offFraction;
            ++seedsWithHatData;
        }

        const auto percSplit = splitByRoleOccupancy(id.bassMotif, id.drumMotif.percA);
        if (percSplit.onCount > 0 && percSplit.offCount > 0)
        {
            sumPercOn  += percSplit.onFraction;
            sumPercOff += percSplit.offFraction;
            ++seedsWithPercData;
        }
    }

    CHECK(seedsWithKickData > kNumSeeds / 2);
    CHECK(seedsWithHatData  > kNumSeeds / 2);
    CHECK(seedsWithPercData > kNumSeeds / 2);

    const double meanKickOn  = sumKickOn  / (double) seedsWithKickData;
    const double meanKickOff = sumKickOff / (double) seedsWithKickData;
    const double meanHatOn   = sumHatOn   / (double) seedsWithHatData;
    const double meanHatOff  = sumHatOff  / (double) seedsWithHatData;
    const double meanPercOn  = sumPercOn  / (double) seedsWithPercData;
    const double meanPercOff = sumPercOff / (double) seedsWithPercData;

    std::printf("bass-on-kick fraction:      on=%.3f off=%.3f (%d seeds)\n", meanKickOn, meanKickOff, seedsWithKickData);
    std::printf("bass-on-hatClosed fraction: on=%.3f off=%.3f (%d seeds)\n", meanHatOn, meanHatOff, seedsWithHatData);
    std::printf("bass-on-percA fraction:     on=%.3f off=%.3f (%d seeds)\n", meanPercOn, meanPercOff, seedsWithPercData);

    // The real, measured effect: bass is genuinely less likely to land on
    // a step the real kick/hatClosed/percA already occupies than on a
    // step they don't - this is what "designed together" actually means
    // as a checkable property, not just correlation code existing
    // somewhere in the source.
    CHECK(meanKickOn < meanKickOff);
    CHECK(meanHatOn  < meanHatOff);
    CHECK(meanPercOn < meanPercOff);

    // Kick is the measured, higher-confidence correlation (real corpus
    // ratio, see BassEngine.cpp's kBassKickCorrelation) - its effect
    // should be at least as pronounced as the disclosed-design-decision
    // hat/perc correlations, not smaller than noise would produce anyway.
    CHECK((meanKickOff - meanKickOn) > 0.01);

    TEST_SUMMARY_AND_EXIT();
}
