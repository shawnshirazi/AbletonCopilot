#include "../DrumEngine.h"
#include "../DrumRhythmGrammar.h"
#include "../DrumVoiceSynth.h"
#include "../Grid.h"
#include "TestSupport.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

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

    int countActiveInBars(const StepArray& steps, int barFrom, int barToExclusive, int stepsPerBar)
    {
        int n = 0;
        for (int bar = barFrom; bar < barToExclusive; ++bar)
            for (int s = 0; s < stepsPerBar; ++s)
                if (steps[(size_t) (bar * stepsPerBar + s)].active)
                    ++n;
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

    bool sameDrop(const DropPattern& a, const DropPattern& b)
    {
        return sameArray(a.kick, b.kick) && sameArray(a.clap, b.clap)
            && sameArray(a.hatClosed, b.hatClosed) && sameArray(a.hatOpen, b.hatOpen)
            && sameArray(a.percA, b.percA) && sameArray(a.percB, b.percB);
    }

    // True if bar `a` and bar `b` (0-based) contain byte-identical hit
    // patterns - the direct test for "these bars are literally the same
    // musical idea repeated", not just similarly-shaped.
    bool barsIdentical(const StepArray& steps, int barA, int barB, int stepsPerBar)
    {
        for (int s = 0; s < stepsPerBar; ++s)
        {
            const auto& ha = steps[(size_t) (barA * stepsPerBar + s)];
            const auto& hb = steps[(size_t) (barB * stepsPerBar + s)];
            if (ha.active != hb.active || (ha.active && ha.velocity != hb.velocity))
                return false;
        }
        return true;
    }

    // Fraction of the 16 positions where two bars agree on active/inactive
    // (ignores velocity) - the graded version of barsIdentical, for
    // comparing bars that are expected to be RELATED (developed from one
    // another) rather than byte-identical.
    double sharedFraction(const StepArray& steps, int barA, int barB, int stepsPerBar)
    {
        int matches = 0;
        for (int s = 0; s < stepsPerBar; ++s)
        {
            const auto& a = steps[(size_t) (barA * stepsPerBar + s)];
            const auto& b = steps[(size_t) (barB * stepsPerBar + s)];
            if (a.active == b.active) ++matches;
        }
        return (double) matches / (double) stepsPerBar;
    }

    bool isStrongBeat(int stepInBar, int stepsPerBar)
    {
        return (stepInBar % std::max(1, stepsPerBar / 4)) == 0;
    }

    // Prints one role's velocity per step, one bar per line, matching the
    // format the brief explicitly asked to see ("show velocity levels as
    // well, not merely on/off").
    void printPatternWithVelocity(const char* roleName, const StepArray& steps, const StepGridConfig& grid)
    {
        int active = countActive(steps);
        std::printf("--- %s (%d/%d active) ---\n", roleName, active, (int) steps.size());
        for (int bar = 0; bar < grid.numBars; ++bar)
        {
            std::printf("%2d: ", bar);
            for (int s = 0; s < grid.stepsPerBar; ++s)
            {
                const auto& h = steps[(size_t) (bar * grid.stepsPerBar + s)];
                if (!h.active)
                    std::printf(" .   ");
                else
                    std::printf("%4d ", (int) std::lround(h.velocity * 127.0f));
                if ((s + 1) % 4 == 0 && s + 1 < grid.stepsPerBar)
                    std::printf("| ");
            }
            std::printf("\n");
        }
    }

    // Aggregate, across `numSeeds` different seeds, the same per-16th-step
    // hit-probability/velocity statistics analyze_drum_grammar.py computed
    // from real audio - so the generator's OWN output can be compared
    // directly, number for number, against DrumRhythmGrammar.h's measured
    // table (which is itself sourced from drum_grammar.json).
    struct GeneratedStats
    {
        float step16Probability[16] = {};
        float step16MeanVelocity[16] = {};
        double meanOnsetsPerBar = 0.0;
        double onBeatFraction = 0.0;
    };

    GeneratedStats measureGenerated(const StepArray& steps, const StepGridConfig& grid)
    {
        GeneratedStats out;
        long counts[16] = {};
        double velSum[16] = {};
        long totalOnsets = 0;
        long onBeat = 0;

        for (int i = 0; i < (int) steps.size(); ++i)
        {
            if (!steps[(size_t) i].active) continue;
            const int s = i % grid.stepsPerBar;
            ++counts[s];
            velSum[s] += steps[(size_t) i].velocity;
            ++totalOnsets;
            if (isStrongBeat(s, grid.stepsPerBar)) ++onBeat;
        }

        for (int s = 0; s < 16; ++s)
        {
            out.step16Probability[s] = totalOnsets > 0 ? (float) counts[s] / (float) totalOnsets : 0.0f;
            out.step16MeanVelocity[s] = counts[s] > 0 ? (float) (velSum[s] / (double) counts[s]) : 0.0f;
        }
        out.meanOnsetsPerBar = grid.numBars > 0 ? (double) totalOnsets / (double) grid.numBars : 0.0;
        out.onBeatFraction   = totalOnsets > 0 ? (double) onBeat / (double) totalOnsets : 0.0;
        return out;
    }
}

int main()
{
    const StepGridConfig grid; // default: 16 steps/bar, 16 bars = 256 steps - the Drop default

    DrumPatternParams standard;
    standard.density = 0.5f; standard.syncopation = 0.3f; standard.variation = 0.2f; standard.seed = 7;

    // =====================================================================
    // 16-bar Drop pattern - basic shape sanity, every one of the 6 roles,
    // via the ONE coordinated entry point (there is no per-role
    // generateKick/generateClap/generateHat*/generatePerc* - see
    // DrumEngine.h).
    // =====================================================================
    {
        CHECK(totalSteps(grid) == 256);
        CHECK(grid.numBars == 16);
        const auto drop = generateDrop(grid, standard);
        CHECK((int) drop.kick.size()      == 256);
        CHECK((int) drop.clap.size()      == 256);
        CHECK((int) drop.hatClosed.size() == 256);
        CHECK((int) drop.hatOpen.size()   == 256);
        CHECK((int) drop.percA.size()     == 256);
        CHECK((int) drop.percB.size()     == 256);
    }

    // =====================================================================
    // Determinism: same seed + same params -> byte-identical composition,
    // every role, every call. Different seed -> a different composition
    // (real variation between seeds, not a fixed output).
    // =====================================================================
    {
        CHECK(sameDrop(generateDrop(grid, standard), generateDrop(grid, standard)));

        DrumPatternParams seedB = standard; seedB.seed = 8;
        CHECK(!sameDrop(generateDrop(grid, standard), generateDrop(grid, seedB)));

        // Variation between seeds shows up in EVERY role, not just one -
        // otherwise a "different seed" could silently still produce the
        // same hat/perc content.
        const auto dropA = generateDrop(grid, standard);
        const auto dropB = generateDrop(grid, seedB);
        CHECK(!sameArray(dropA.hatClosed, dropB.hatClosed));
        CHECK(!sameArray(dropA.hatOpen,   dropB.hatOpen));
        CHECK(!sameArray(dropA.percA,     dropB.percA));
        CHECK(!sameArray(dropA.percB,     dropB.percB));
    }

    // =====================================================================
    // KICK FOUNDATION - unchanged by this milestone's work: the corpus
    // measured 100% on-beat placement (kKickRhythm.onBeatFraction), so
    // this is a deterministic four-on-the-floor foundation, not a
    // probability outcome. Velocity hierarchy is the MEASURED
    // per-beat-position value, verified directly against the generated
    // table.
    // =====================================================================
    {
        CHECK(kKickRhythm.onBeatFraction == 1.0f); // the measured fact the whole kick design rests on

        DrumPatternParams noVar = standard; noVar.variation = 0.0f;
        const auto drop = generateDrop(grid, noVar);
        CHECK(countActive(drop.kick) == grid.numBars * 4);
        for (int i = 0; i < (int) drop.kick.size(); ++i)
            if (drop.kick[(size_t) i].active)
                CHECK(isStrongBeat(i % grid.stepsPerBar, grid.stepsPerBar));

        // Every bar's kick shape is identical (modulo the fixed
        // phrase-downbeat velocity accent) - the foundation role stays
        // intact through the whole phrase regardless of the energy arc
        // driving every other role.
        for (int bar = 4; bar < grid.numBars - 1; bar += 4)
            CHECK(barsIdentical(drop.kick, 0, bar, grid.stepsPerBar));
        for (int bar = 2; bar < grid.numBars - 1; ++bar)
            if (bar % 4 != 0)
                CHECK(barsIdentical(drop.kick, 1, bar, grid.stepsPerBar));

        // Beat-1 velocity (measured 0.9876) must be strictly the loudest
        // of the four beat positions, matching kKickRhythm exactly - a
        // real measured hierarchy, not a guess.
        const int stepsPerBeat = grid.stepsPerBar / 4;
        const float velBeat1 = drop.kick[(size_t) (1 * grid.stepsPerBar)].velocity; // bar 1 (non-phrase-downbeat), beat 1
        const float velBeat2 = drop.kick[(size_t) (1 * grid.stepsPerBar + stepsPerBeat)].velocity;
        CHECK(velBeat1 > velBeat2);
        CHECK(std::abs(velBeat1 - kKickRhythm.step16RelativeVelocity[0]) < 0.001f);
        CHECK(std::abs(velBeat2 - kKickRhythm.step16RelativeVelocity[stepsPerBeat]) < 0.001f);

        printPatternWithVelocity("KICK (drop, default params, seed=7)", drop.kick, grid);
    }

    // =====================================================================
    // HAT HIERARCHY (closed) - measured offbeat-8th positions (steps
    // 2/6/10/14) carry roughly double the velocity of the surrounding
    // ghost 16th movement (kHatRhythm.step16RelativeVelocity), i.e. the
    // "strong pulse, quieter ghost, same role/sample" hierarchy described
    // in the brief. Must survive into the GENERATED pattern's own average
    // velocity per position, aggregated across many seeds so a single
    // unlucky seed can't produce a false failure.
    // =====================================================================
    {
        const int stepsPerBar = grid.stepsPerBar;
        double offbeatVelSum = 0.0, otherVelSum = 0.0;
        int offbeatCount = 0, otherCount = 0;

        constexpr int kNumSeeds = 40;
        double withinBlockSharedSum = 0.0;
        int withinBlockSharedN = 0;

        for (uint32_t seed = 0; seed < kNumSeeds; ++seed)
        {
            DrumPatternParams p = standard; p.seed = seed;
            const auto drop = generateDrop(grid, p);
            for (int i = 0; i < (int) drop.hatClosed.size(); ++i)
            {
                if (!drop.hatClosed[(size_t) i].active) continue;
                const int s = i % stepsPerBar;
                const bool isOffbeat8th = (s % 4 == 2);
                if (isOffbeat8th) { offbeatVelSum += drop.hatClosed[(size_t) i].velocity; ++offbeatCount; }
                else              { otherVelSum   += drop.hatClosed[(size_t) i].velocity; ++otherCount; }
            }

            // Within-stage-block bar-to-bar continuity - buildRoleBlock's
            // own bar1-3 mechanism (literal repeat OR 1-2 small touches),
            // so adjacent bars WITHIN one 4-bar stage group should mostly
            // agree, aggregated across seeds/stages to avoid one unlucky
            // touch making a single comparison flaky.
            for (int stageStart : { 0, 4, 8, 12 })
            {
                withinBlockSharedSum += sharedFraction(drop.hatClosed, stageStart, stageStart + 1, stepsPerBar);
                ++withinBlockSharedN;
            }
        }
        CHECK(offbeatCount > 0);
        CHECK(otherCount > 0);
        const double meanOffbeatVel = offbeatVelSum / offbeatCount;
        const double meanOtherVel   = otherVelSum / otherCount;
        // Real measured gap is roughly 2x (0.75 vs 0.35) - require at
        // least a clearly-audible 30% difference, not a razor-thin one.
        CHECK(meanOffbeatVel > meanOtherVel * 1.3);

        // Density: the corpus measured ~12.6 hits/bar for hat - the
        // generator at default density (0.5, scale ~1.0) should land in
        // the same ballpark (not 2 hits, not all 16), aggregated the same
        // way for statistical stability.
        double totalHatHits = 0.0;
        for (uint32_t seed = 0; seed < kNumSeeds; ++seed)
        {
            DrumPatternParams p = standard; p.seed = seed;
            totalHatHits += countActive(generateDrop(grid, p).hatClosed);
        }
        const double meanHatHitsPerBar = totalHatHits / kNumSeeds / grid.numBars;
        CHECK(meanHatHitsPerBar > 6.0);  // clearly busier than an accent role
        CHECK(meanHatHitsPerBar < 15.0); // but not literally every 16th every bar

        const double meanWithinBlockShared = withinBlockSharedSum / withinBlockSharedN;
        std::printf("hatClosed within-stage-block adjacent-bar shared fraction: %.3f (buildRoleBlock's own repeat/touch mechanism)\n",
                    meanWithinBlockShared);
        CHECK(meanWithinBlockShared > 0.75); // mostly the same idea, controlled movement, not independent bars

        const auto drop = generateDrop(grid, standard);
        printPatternWithVelocity("HAT-CLOSED (drop, default params, seed=7)", drop.hatClosed, grid);
    }

    // =====================================================================
    // OPEN-HAT CONSTRAINTS - hatOpen's StageEnergy ramps far more steeply
    // than hatClosed's (0.10 -> 0.75, ~7.5x) so it must be genuinely
    // near-absent in the establish section (bars 1-4) and a real presence
    // by the full-drop section (bars 13-15) - "strategically placed... not
    // constant from bar 1", aggregated across many seeds.
    // =====================================================================
    {
        constexpr int kNumSeeds = 60;
        long establishHits = 0, fullDropHits = 0;
        for (uint32_t seed = 0; seed < kNumSeeds; ++seed)
        {
            DrumPatternParams p = standard; p.seed = seed;
            const auto drop = generateDrop(grid, p);
            establishHits += countActiveInBars(drop.hatOpen, 0, 4, grid.stepsPerBar);
            fullDropHits  += countActiveInBars(drop.hatOpen, 12, 15, grid.stepsPerBar);
        }
        const double meanEstablishPerBar = (double) establishHits / kNumSeeds / 4.0;
        const double meanFullDropPerBar  = (double) fullDropHits  / kNumSeeds / 3.0;
        std::printf("hatOpen mean hits/bar: establish(bars1-4)=%.3f full-drop(bars13-15)=%.3f (must grow clearly)\n",
                    meanEstablishPerBar, meanFullDropPerBar);
        CHECK(meanEstablishPerBar < meanFullDropPerBar * 0.65); // clearly sparser early (measured ratio ~1.9x), not just nominally
        CHECK(meanFullDropPerBar > 0.0); // it does become a real presence by the drop

        const auto drop = generateDrop(grid, standard);
        printPatternWithVelocity("HAT-OPEN (drop, default params, seed=7)", drop.hatOpen, grid);
    }

    // =====================================================================
    // CLAP - measured overwhelmingly on the backbeat (steps 4 and 12 -
    // beats 2 and 4), matching kClapRhythm.onBeatFraction (~84%).
    // =====================================================================
    {
        constexpr int kNumSeeds = 40;
        long backbeatHits = 0, totalHits = 0;
        for (uint32_t seed = 0; seed < kNumSeeds; ++seed)
        {
            DrumPatternParams p = standard; p.seed = seed;
            const auto clap = generateDrop(grid, p).clap;
            for (int i = 0; i < (int) clap.size(); ++i)
            {
                if (!clap[(size_t) i].active) continue;
                ++totalHits;
                const int s = i % grid.stepsPerBar;
                if (s == 4 || s == 12) ++backbeatHits;
            }
        }
        CHECK(totalHits > 0);
        const double backbeatFraction = (double) backbeatHits / (double) totalHits;
        CHECK(backbeatFraction > 0.6); // measured ~0.83 (steps 4+12 combined) - require a clear majority, not a razor edge

        // Restrained: mean clap hits/bar stays clearly under kick's
        // (measured 2.63 vs 4.0), aggregated across seeds.
        double clapTotal = 0.0, kickTotal = 0.0;
        for (uint32_t seed = 0; seed < kNumSeeds; ++seed)
        {
            DrumPatternParams p = standard; p.seed = seed;
            const auto d = generateDrop(grid, p);
            clapTotal += countActive(d.clap);
            kickTotal += countActive(d.kick);
        }
        CHECK(clapTotal < kickTotal);

        const auto drop = generateDrop(grid, standard);

        // CLAP is FOUNDATION, built once and copied verbatim into every
        // bar range - bars 1-4 repeat into 5-8 exactly, unlike every
        // other (energy-arc-driven) role.
        for (int i = 0; i < 4; ++i)
            CHECK(barsIdentical(drop.clap, i, i + 4, grid.stepsPerBar));

        printPatternWithVelocity("CLAP (drop, default params, seed=7)", drop.clap, grid);
    }

    // =====================================================================
    // PERCUSSION MOTIFS (percA/percB) - "responds to occupied space": real
    // measured cross-role correlation (kCrossRoleCorrelation), not an
    // absolute exclusion rule. The measured KICK<->PERC (-0.4488) and
    // CLAP<->PERC (-0.425) correlations must show up as perc landing on
    // kick/clap positions LESS OFTEN than it lands on hat positions
    // (HAT<->PERC is +0.4758) - aggregated across many seeds. Also checks
    // percB is a genuinely distinct second voice (not perc A duplicated):
    // silent in the establish section (StageEnergy.percB == 0.0, a real
    // "this layer hasn't entered yet" decision) and reweighted away from
    // percA's own positions (kPercBAvoidsPercA).
    // =====================================================================
    {
        CHECK(kCrossRoleCorrelation.kickPerc < 0.0f);
        CHECK(kCrossRoleCorrelation.clapPerc < 0.0f);
        CHECK(kCrossRoleCorrelation.hatPerc > 0.0f);

        constexpr int kNumSeeds = 60;
        long percOnKick = 0, percOnClap = 0, percOnHat = 0, percTotal = 0;
        long percBSilentEstablishSeeds = 0;
        long percBOnPercA = 0, percBTotal = 0;
        long percAEqualsPercBSeeds = 0;

        for (uint32_t seed = 0; seed < kNumSeeds; ++seed)
        {
            DrumPatternParams p = standard; p.seed = seed;
            const auto drop = generateDrop(grid, p);
            for (int i = 0; i < (int) drop.percA.size(); ++i)
            {
                if (!drop.percA[(size_t) i].active) continue;
                ++percTotal;
                if (drop.kick[(size_t) i].active)      ++percOnKick;
                if (drop.clap[(size_t) i].active)      ++percOnClap;
                if (drop.hatClosed[(size_t) i].active) ++percOnHat;
            }

            // percB is silent in the establish section - deterministic
            // (StageEnergy.percB == 0.0 forces the density scale to
            // exactly 0 regardless of RNG), so this must hold for every
            // seed, not just on average.
            if (countActiveInBars(drop.percB, 0, 4, grid.stepsPerBar) == 0)
                ++percBSilentEstablishSeeds;

            for (int i = 0; i < (int) drop.percB.size(); ++i)
            {
                if (!drop.percB[(size_t) i].active) continue;
                ++percBTotal;
                if (drop.percA[(size_t) i].active) ++percBOnPercA;
            }

            if (sameArray(drop.percA, drop.percB))
                ++percAEqualsPercBSeeds;
        }
        CHECK(percTotal > 0);
        const double kickCoOccur = (double) percOnKick / (double) percTotal;
        const double clapCoOccur = (double) percOnClap / (double) percTotal;
        const double hatCoOccur  = (double) percOnHat  / (double) percTotal;

        // The real, meaningful assertion: perc co-occurs with hat (positive
        // correlation) measurably MORE than it co-occurs with kick or clap
        // (both negative correlations) - "no impossible/overlapping role
        // behavior": the negative correlation demonstrably suppresses (but
        // doesn't need to eliminate) overlap.
        CHECK(hatCoOccur > kickCoOccur);
        CHECK(hatCoOccur > clapCoOccur);

        std::printf("percA co-occurrence: kick=%.3f clap=%.3f hat=%.3f (hat should be highest - positive measured correlation)\n",
                    kickCoOccur, clapCoOccur, hatCoOccur);

        CHECK(percBSilentEstablishSeeds == kNumSeeds); // exact, every seed - percB genuinely hasn't entered yet in bars 1-4
        CHECK(percAEqualsPercBSeeds == 0); // two distinct voices, never identical

        CHECK(percBTotal > 0);
        const double percBOnPercAFraction = (double) percBOnPercA / (double) percBTotal;
        std::printf("percB landing on percA's own positions: %.3f (kPercBAvoidsPercA=-0.5 should keep this well under 0.5)\n",
                    percBOnPercAFraction);
        CHECK(percBOnPercAFraction < 0.4); // measurably avoiding percA's own positions, not duplicating them

        // Density target check: at the default density (0.5), PERC-A's
        // mean hits/bar should land near 4.78 - the mean of the 15
        // non-"Loop"-named PERC files in the corpus (a real accent-style
        // subset, not the full corpus's 10.49 raw average, which blends in
        // 41 dense continuous "*Perc Loop*" files - see
        // buildEstablishStage). Aggregated for statistical stability.
        double percHitsTotal = 0.0;
        for (uint32_t seed = 0; seed < kNumSeeds; ++seed)
        {
            DrumPatternParams p = standard; p.seed = seed;
            percHitsTotal += countActive(generateDrop(grid, p).percA);
        }
        const double meanPercHitsPerBar = percHitsTotal / kNumSeeds / grid.numBars;
        std::printf("PERC-A mean hits/bar at density=0.5: generated=%.2f target=4.78 (accent-subset mean) corpus-wide=%.2f (all PERC material, incl. dense loops)\n",
                    meanPercHitsPerBar, (double) kPercRhythm.meanOnsetsPerBar);
        CHECK(meanPercHitsPerBar > 2.0);
        CHECK(meanPercHitsPerBar < 7.0);

        // Restrained accent role: even at density=1, PERC-A stays clearly
        // less dense than HAT-CLOSED.
        DrumPatternParams denser = standard; denser.density = 1.0f;
        const auto dropDenser = generateDrop(grid, denser);
        CHECK(countActive(dropDenser.percA) < countActive(dropDenser.hatClosed));
        CHECK(countActive(dropDenser.percA) < (int) grid.stepsPerBar * grid.numBars / 2); // still nowhere close to filling the grid

        const auto drop = generateDrop(grid, standard);
        printPatternWithVelocity("PERC-A (drop, default params, seed=7)", drop.percA, grid);
        printPatternWithVelocity("PERC-B (drop, default params, seed=7)", drop.percB, grid);
    }

    // =====================================================================
    // PHRASE DEVELOPMENT / 4-STAGE ENERGY ARC - bars 1-4 (establish) ->
    // 5-8 (develop) -> 9-12 (increase) -> 13-15 (full drop) must show a
    // real, monotonic energy increase for every arc-driven role (mean
    // active hit count per bar across each stage's bar range), aggregated
    // over many seeds so individual derive-stage touches (which can
    // occasionally remove more than they add) don't produce a false
    // failure. hatClosed/percA (present from bar 1, ramping moderately)
    // and hatOpen/percB (near-absent/silent early, ramping steeply) both
    // checked - "layers enter and leave", not every layer flat from bar 1.
    // =====================================================================
    {
        constexpr int kNumSeeds = 60;
        double stageHits[4][4] = {}; // [role: hatClosed,hatOpen,percA,percB][stage: establish,develop,increase,fullDrop]
        const int stageBarStart[4]  = { 0, 4, 8, 12 };
        const int stageBarCount[4]  = { 4, 4, 4, 3 };

        for (uint32_t seed = 0; seed < kNumSeeds; ++seed)
        {
            DrumPatternParams p = standard; p.seed = seed;
            const auto drop = generateDrop(grid, p);
            const StepArray* roleArrays[4] = { &drop.hatClosed, &drop.hatOpen, &drop.percA, &drop.percB };
            for (int r = 0; r < 4; ++r)
                for (int st = 0; st < 4; ++st)
                    stageHits[r][st] += (double) countActiveInBars(*roleArrays[r], stageBarStart[st],
                                                                     stageBarStart[st] + stageBarCount[st],
                                                                     grid.stepsPerBar)
                                        / stageBarCount[st];
        }
        for (int r = 0; r < 4; ++r)
            for (int st = 0; st < 4; ++st)
                stageHits[r][st] /= kNumSeeds;

        const char* roleNames[4] = { "hatClosed", "hatOpen", "percA", "percB" };
        std::printf("\n=== ENERGY ARC: mean hits/bar per stage, averaged over %d seeds ===\n", kNumSeeds);
        std::printf("%-10s %10s %10s %10s %10s\n", "role", "establish", "develop", "increase", "fullDrop");
        for (int r = 0; r < 4; ++r)
            std::printf("%-10s %10.3f %10.3f %10.3f %10.3f\n", roleNames[r],
                        stageHits[r][0], stageHits[r][1], stageHits[r][2], stageHits[r][3]);

        // Real energy arc: each stage is at least as busy as the previous
        // one, on average, for every arc-driven role - "increase energy",
        // not a flat loop.
        for (int r = 0; r < 4; ++r)
            for (int st = 0; st < 3; ++st)
                CHECK(stageHits[r][st] <= stageHits[r][st + 1] + 0.05); // small epsilon for aggregate float noise

        // The overall arc must be a REAL increase end to end, not a
        // rounding-noise flat line - full-drop clearly busier than
        // establish for every arc-driven role.
        for (int r = 0; r < 4; ++r)
            CHECK(stageHits[r][3] > stageHits[r][0]);

        // Adjacent stages remain related (development, not replacement):
        // bars 9-12 share most of their content with bars 1-4, aggregated,
        // even though they're not byte-identical (see deriveStageBlock).
        double sharedEstablishToIncrease = 0.0;
        for (uint32_t seed = 0; seed < kNumSeeds; ++seed)
        {
            DrumPatternParams p = standard; p.seed = seed;
            const auto drop = generateDrop(grid, p);
            sharedEstablishToIncrease += sharedFraction(drop.hatClosed, 0, 8, grid.stepsPerBar);
        }
        sharedEstablishToIncrease /= kNumSeeds;
        std::printf("hatClosed bar0 vs bar8 (establish vs increase) shared fraction: %.3f (develops, doesn't replace)\n",
                    sharedEstablishToIncrease);
        CHECK(sharedEstablishToIncrease > 0.5);  // still recognizably the same idea
        CHECK(sharedEstablishToIncrease < 1.0);  // but has genuinely moved on from bar 0, not frozen
    }

    // =====================================================================
    // BAR-16 TRANSITION - built by THINNING the full-drop section's own
    // material (never adding a fill roll): hatClosed keeps only its
    // strong-pulse hits (velocity >= 0.55); hatOpen/percA/percB each keep
    // at most their single loudest hit (plus hatOpen's own variation-gated
    // accent). These are structural/near-deterministic properties, checked
    // directly rather than statistically.
    // =====================================================================
    {
        constexpr int kNumSeeds = 30;
        for (uint32_t seed = 0; seed < kNumSeeds; ++seed)
        {
            DrumPatternParams p = standard; p.seed = seed;
            const auto drop = generateDrop(grid, p);
            const int lastBar = grid.numBars - 1;
            const int stepsPerBar = grid.stepsPerBar;

            // hatClosed: bar 15's active positions are a SUBSET of bar 12's
            // (stage4's own canonical bar 0, the thinning source), every
            // kept hit at or above the strong-pulse threshold.
            int percAHitsBar15 = 0, percBHitsBar15 = 0, hatOpenHitsBar15 = 0;
            for (int s = 0; s < stepsPerBar; ++s)
            {
                const auto& h15 = drop.hatClosed[(size_t) (lastBar * stepsPerBar + s)];
                if (!h15.active) continue;
                CHECK(h15.velocity >= 0.55f - 1e-6f);
                const auto& h12 = drop.hatClosed[(size_t) (12 * stepsPerBar + s)];
                CHECK(h12.active); // thinned FROM bar 12's content, never a new position
            }
            for (int s = 0; s < stepsPerBar; ++s)
            {
                if (drop.percA[(size_t) (lastBar * stepsPerBar + s)].active) ++percAHitsBar15;
                if (drop.percB[(size_t) (lastBar * stepsPerBar + s)].active) ++percBHitsBar15;
                if (drop.hatOpen[(size_t) (lastBar * stepsPerBar + s)].active) ++hatOpenHitsBar15;
            }
            CHECK(percAHitsBar15 <= 1);   // keepLoudestOnly - at most one hit
            CHECK(percBHitsBar15 <= 1);
            CHECK(hatOpenHitsBar15 <= 2); // keepLoudestOnly (<=1) + at most one variation-gated accent

            // Thinning, not adding: bar 15 is never busier than bar 12 (the
            // section it was thinned from) for the keep-a-subset roles.
            CHECK(countActiveInBars(drop.hatClosed, lastBar, lastBar + 1, stepsPerBar)
                  <= countActiveInBars(drop.hatClosed, 12, 13, stepsPerBar));
        }

        // variation=0 disables the extra accent entirely - bar 15's
        // hatOpen content is then ONLY the (<=1) loudest-kept hit.
        DrumPatternParams noVar = standard; noVar.variation = 0.0f;
        const auto dropNoVar = generateDrop(grid, noVar);
        const int lastBar = grid.numBars - 1;
        int hatOpenHitsNoVar = countActiveInBars(dropNoVar.hatOpen, lastBar, lastBar + 1, grid.stepsPerBar);
        CHECK(hatOpenHitsNoVar <= 1);

        printPatternWithVelocity("HAT-CLOSED (bar16 transition, seed=7)", generateDrop(grid, standard).hatClosed, grid);
    }

    // =====================================================================
    // Different seeds produce controlled variation: patterns differ, but
    // aggregate statistics (hit counts) stay in a similar range - not
    // wildly different compositions from run to run.
    // =====================================================================
    {
        int hatCounts[5];
        for (uint32_t seed = 0; seed < 5; ++seed)
        {
            DrumPatternParams p = standard; p.seed = seed * 17 + 3;
            hatCounts[seed] = countActive(generateDrop(grid, p).hatClosed);
        }
        int minCount = hatCounts[0], maxCount = hatCounts[0];
        for (int c : hatCounts) { minCount = std::min(minCount, c); maxCount = std::max(maxCount, c); }
        // Controlled variation: the busiest seed isn't more than ~2x the
        // quietest - real variety, not chaos.
        CHECK(maxCount < minCount * 2 + 10);
    }

    // =====================================================================
    // toVelocityArray - the single conversion both the audio path
    // (PluginProcessor's GeneratedDrumRole/DrumVoiceSynth) and the UI grid
    // display consume.
    // =====================================================================
    {
        StepArray steps(8);
        steps[0] = { true,  1.0f };
        steps[1] = { false, 1.0f };
        steps[2] = { true,  0.0f };
        steps[3] = { true,  0.5f };
        steps[4] = { true,  -1.0f };
        steps[5] = { true,  2.0f };

        auto vel = toVelocityArray(steps);
        CHECK(vel.size() == steps.size());
        CHECK(vel[0] == 127);
        CHECK(vel[1] == 0);
        CHECK(vel[2] == 1);
        CHECK(vel[3] == 64);
        CHECK(vel[4] == 1);
        CHECK(vel[5] == 127);

        for (size_t i = 0; i < steps.size(); ++i)
            CHECK(steps[i].active == (vel[i] > 0));

        CHECK(toVelocityArray(steps) == vel);

        const auto drop = generateDrop(grid, standard);
        auto kickVel = toVelocityArray(drop.kick);
        CHECK(kickVel.size() == drop.kick.size());
        for (size_t i = 0; i < drop.kick.size(); ++i)
            CHECK(drop.kick[i].active == (kickVel[i] > 0));
    }

    // =====================================================================
    // Statistical comparison against the measured corpus (drum_grammar.json
    // via DrumRhythmGrammar.h) - printed for direct human inspection, the
    // "I want evidence the generated pattern resembles the analyzed corpus"
    // requirement. Aggregated across many seeds/compositions so it reflects
    // the generator's real behavior, not one lucky roll. hatOpen is
    // compared against kRideRhythm (its real measured source, see
    // rhythmStatsForRole) rather than kHatRhythm.
    // =====================================================================
    {
        constexpr int kNumSeeds = 50;
        double kickOn = 0, clapOn = 0, hatOn = 0, hatOpenOn = 0, percAOn = 0, percBOn = 0;
        double kickBar = 0, clapBar = 0, hatBar = 0, hatOpenBar = 0, percABar = 0, percBBar = 0;
        double hatOffbeatVel = 0, hatOtherVel = 0; int hatOffbeatN = 0, hatOtherN = 0;

        for (uint32_t seed = 0; seed < kNumSeeds; ++seed)
        {
            DrumPatternParams p = standard; p.seed = seed;
            const auto drop = generateDrop(grid, p);
            auto sK  = measureGenerated(drop.kick, grid);
            auto sC  = measureGenerated(drop.clap, grid);
            auto sH  = measureGenerated(drop.hatClosed, grid);
            auto sHO = measureGenerated(drop.hatOpen, grid);
            auto sPA = measureGenerated(drop.percA, grid);
            auto sPB = measureGenerated(drop.percB, grid);
            kickOn += sK.onBeatFraction; clapOn += sC.onBeatFraction;
            hatOn  += sH.onBeatFraction; hatOpenOn += sHO.onBeatFraction;
            percAOn += sPA.onBeatFraction; percBOn += sPB.onBeatFraction;
            kickBar += sK.meanOnsetsPerBar; clapBar += sC.meanOnsetsPerBar;
            hatBar  += sH.meanOnsetsPerBar; hatOpenBar += sHO.meanOnsetsPerBar;
            percABar += sPA.meanOnsetsPerBar; percBBar += sPB.meanOnsetsPerBar;

            for (int i = 0; i < (int) drop.hatClosed.size(); ++i)
            {
                if (!drop.hatClosed[(size_t) i].active) continue;
                const int s = i % grid.stepsPerBar;
                if (s % 4 == 2) { hatOffbeatVel += drop.hatClosed[(size_t) i].velocity; ++hatOffbeatN; }
                else            { hatOtherVel   += drop.hatClosed[(size_t) i].velocity; ++hatOtherN; }
            }
        }

        std::printf("\n=== GENERATED vs MEASURED (drum_grammar.json), averaged over %d seeds ===\n", kNumSeeds);
        std::printf("%-11s %14s %14s   %14s %14s\n", "role", "gen onBeat%", "measured", "gen hits/bar", "measured");
        std::printf("%-11s %14.3f %14.3f   %14.2f %14.2f\n", "KICK", kickOn / kNumSeeds, kKickRhythm.onBeatFraction, kickBar / kNumSeeds, kKickRhythm.meanOnsetsPerBar);
        std::printf("%-11s %14.3f %14.3f   %14.2f %14.2f\n", "CLAP", clapOn / kNumSeeds, kClapRhythm.onBeatFraction, clapBar / kNumSeeds, kClapRhythm.meanOnsetsPerBar);
        std::printf("%-11s %14.3f %14.3f   %14.2f %14.2f\n", "HAT-CLOSED", hatOn / kNumSeeds, kHatRhythm.onBeatFraction, hatBar / kNumSeeds, kHatRhythm.meanOnsetsPerBar);
        std::printf("%-11s %14.3f %14.3f   %14.2f %14.2f\n", "HAT-OPEN (vs RIDE)", hatOpenOn / kNumSeeds, kRideRhythm.onBeatFraction, hatOpenBar / kNumSeeds, kRideRhythm.meanOnsetsPerBar);
        std::printf("%-11s %14.3f %14.3f   %14.2f %14.2f\n", "PERC-A", percAOn / kNumSeeds, kPercRhythm.onBeatFraction, percABar / kNumSeeds, kPercRhythm.meanOnsetsPerBar);
        std::printf("%-11s %14.3f %14.3f   %14.2f %14.2f\n", "PERC-B (vs PERC)", percBOn / kNumSeeds, kPercRhythm.onBeatFraction, percBBar / kNumSeeds, kPercRhythm.meanOnsetsPerBar);
        std::printf("HAT-CLOSED offbeat-8th mean velocity: generated=%.3f measured=%.3f (avg of steps 2/6/10/14)\n",
                    hatOffbeatN > 0 ? hatOffbeatVel / hatOffbeatN : 0.0,
                    (kHatRhythm.step16RelativeVelocity[2] + kHatRhythm.step16RelativeVelocity[6] +
                     kHatRhythm.step16RelativeVelocity[10] + kHatRhythm.step16RelativeVelocity[14]) / 4.0f);
        std::printf("HAT-CLOSED other-position mean velocity:   generated=%.3f\n", hatOtherN > 0 ? hatOtherVel / hatOtherN : 0.0);

        // KICK's on-beat fraction must match the corpus almost exactly
        // (it's a deterministic role) - a real, tight, meaningful bound.
        CHECK(std::abs(kickOn / kNumSeeds - kKickRhythm.onBeatFraction) < 0.02);
    }

    TEST_SUMMARY_AND_EXIT();
}
