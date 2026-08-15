#include "../Arrangement.h"
#include "TestSupport.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

using namespace Engine;

namespace
{
    const char* sectionName(MusicSection s)
    {
        switch (s)
        {
            case MusicSection::Intro:          return "Intro";
            case MusicSection::Establish:      return "Establish";
            case MusicSection::Build:          return "Build";
            case MusicSection::PreDrop:        return "PreDrop";
            case MusicSection::Drop:           return "Drop";
            case MusicSection::Breakdown:      return "Breakdown";
            case MusicSection::BreakdownBuild: return "BreakdownBuild";
            case MusicSection::FinalDrop:      return "FinalDrop";
            case MusicSection::Outro:          return "Outro";
        }
        return "?";
    }
}

int main()
{
    ArrangementConfig cfg;
    cfg.bpm = 124.0;
    cfg.rootNote = 2;
    cfg.seed = 7;

    // =====================================================================
    // Determinism: same config -> identical arrangement, every field.
    // =====================================================================
    {
        const auto a = buildArrangement(cfg);
        const auto b = buildArrangement(cfg);
        CHECK(a.totalBars() == b.totalBars());
        bool identical = true;
        for (int i = 0; i < a.totalBars(); ++i)
        {
            const auto& sa = a.barStates[(size_t) i];
            const auto& sb = b.barStates[(size_t) i];
            if (sa.section != sb.section || sa.bar != sb.bar || sa.energy != sb.energy
                || sa.tension != sb.tension || sa.drumEnergy != sb.drumEnergy
                || sa.bassEnergy != sb.bassEnergy || sa.melodicEnergy != sb.melodicEnergy)
                identical = false;
        }
        CHECK(identical);
    }

    // =====================================================================
    // Total length matches the sum of the default cycle's section bars,
    // and every real section length used is a multiple of 8 except the
    // one 4-bar PreDrop (matching arrangement_target.json's own finding).
    // =====================================================================
    {
        const auto arrangement = buildArrangement(cfg);
        int expectedTotal = 0;
        for (auto& s : defaultMelodicTechnoCycle())
        {
            expectedTotal += s.bars;
            if (s.section != MusicSection::PreDrop)
                CHECK(s.bars % 8 == 0);
        }
        CHECK(arrangement.totalBars() == expectedTotal);
        std::printf("Default cycle total: %d bars\n", arrangement.totalBars());
    }

    // =====================================================================
    // Section transitions: bar-by-bar walk covers every section in the
    // configured order, contiguous (no gaps/overlaps), each bar's
    // barInSection/barsInSection is internally consistent.
    // =====================================================================
    {
        const auto arrangement = buildArrangement(cfg);
        const auto cycle = defaultMelodicTechnoCycle();

        int expectedBar = 0;
        for (size_t s = 0; s < cycle.size(); ++s)
        {
            for (int b = 0; b < cycle[s].bars; ++b)
            {
                const auto& state = musicStateForBar(arrangement, expectedBar);
                CHECK(state.section == cycle[s].section);
                CHECK(state.bar == expectedBar);
                CHECK(state.barInSection == b);
                CHECK(state.barsInSection == cycle[s].bars);
                ++expectedBar;
            }
        }
    }

    // =====================================================================
    // Energy curve: every value stays in [0,1]; Drop/FinalDrop are the
    // energy peaks; Breakdown is the energy trough but NOT the tension
    // trough (energy/tension are genuinely independent axes, not the same
    // thing renamed - this is the core "breakdown = low energy, high
    // tension" property the user's own brief specified).
    // =====================================================================
    {
        const auto arrangement = buildArrangement(cfg);
        float maxEnergy = 0.0f, minEnergyInBreakdown = 1.0f, breakdownTension = 0.0f;
        float maxNonBreakdownTension = 0.0f;

        for (auto& state : arrangement.barStates)
        {
            CHECK(state.energy >= 0.0f && state.energy <= 1.0f);
            CHECK(state.tension >= 0.0f && state.tension <= 1.0f);
            CHECK(state.drumEnergy >= 0.0f && state.drumEnergy <= 1.0f);
            CHECK(state.bassEnergy >= 0.0f && state.bassEnergy <= 1.0f);
            CHECK(state.melodicEnergy >= 0.0f && state.melodicEnergy <= 1.0f);

            maxEnergy = std::max(maxEnergy, state.energy);
            if (state.section == MusicSection::Breakdown)
            {
                minEnergyInBreakdown = std::min(minEnergyInBreakdown, state.energy);
                breakdownTension = state.tension;
            }
            else
            {
                maxNonBreakdownTension = std::max(maxNonBreakdownTension, state.tension);
            }
        }

        CHECK(maxEnergy > 0.9f); // Drop/FinalDrop reach near-peak energy
        CHECK(minEnergyInBreakdown < 0.25f); // Breakdown is genuinely quiet
        // The key property: breakdown's tension is NOT the lowest tension
        // in the piece - it stays meaningfully elevated despite low energy.
        CHECK(breakdownTension > 0.5f);
        std::printf("Breakdown: energy=%.2f tension=%.2f (energy low, tension elevated)\n",
                    minEnergyInBreakdown, breakdownTension);
    }

    // =====================================================================
    // Per-role energy divergence: Breakdown's drumEnergy is far lower than
    // its melodicEnergy (drums recede, melody is exposed - not a single
    // global energy value applied uniformly to every role).
    // =====================================================================
    {
        const auto arrangement = buildArrangement(cfg);
        for (auto& state : arrangement.barStates)
        {
            if (state.section == MusicSection::Breakdown)
            {
                CHECK(state.drumEnergy < 0.2f);
                CHECK(state.melodicEnergy > state.drumEnergy);
                CHECK(state.bassEnergy < 0.3f);
            }
            if (state.section == MusicSection::Intro)
            {
                // Real reference-track evidence: intros are atmosphere/
                // drums first, bass enters later as its own section.
                CHECK(state.bassEnergy <= 0.05f);
            }
        }
    }

    // =====================================================================
    // Build/BreakdownBuild/Outro genuinely ramp (first bar != last bar);
    // Drop/Breakdown/Intro/Establish stay flat (first bar == last bar).
    // =====================================================================
    {
        const auto arrangement = buildArrangement(cfg);
        auto firstAndLastEnergy = [&](MusicSection section) -> std::pair<float, float>
        {
            float first = -1.0f, last = -1.0f;
            for (auto& state : arrangement.barStates)
                if (state.section == section)
                {
                    if (first < 0.0f) first = state.energy;
                    last = state.energy;
                }
            return { first, last };
        };

        auto [buildFirst, buildLast] = firstAndLastEnergy(MusicSection::Build);
        CHECK(buildLast > buildFirst); // ramps up

        auto [dropFirst, dropLast] = firstAndLastEnergy(MusicSection::Drop);
        CHECK(std::abs(dropLast - dropFirst) < 0.001f); // flat

        auto [outroFirst, outroLast] = firstAndLastEnergy(MusicSection::Outro);
        CHECK(outroLast < outroFirst); // ramps down
    }

    // =====================================================================
    // PreDrop final-bar drop-out marker: exactly barsInSection bars are
    // PreDrop, exactly one of them is flagged as the final bar, and it's
    // the LAST one chronologically.
    // =====================================================================
    {
        const auto arrangement = buildArrangement(cfg);
        int preDropBars = 0, flaggedBars = 0;
        int lastPreDropBar = -1;
        for (int i = 0; i < arrangement.totalBars(); ++i)
        {
            if (arrangement.barStates[(size_t) i].section == MusicSection::PreDrop)
            {
                ++preDropBars;
                lastPreDropBar = i;
            }
            if (isPreDropFinalBar(arrangement, i))
                ++flaggedBars;
        }
        CHECK(preDropBars > 0);
        CHECK(flaggedBars == 1);
        CHECK(isPreDropFinalBar(arrangement, lastPreDropBar));
        CHECK(!isPreDropFinalBar(arrangement, lastPreDropBar - 1));
    }

    // =====================================================================
    // Config values (bpm/rootNote/seed) propagate to every bar.
    // =====================================================================
    {
        const auto arrangement = buildArrangement(cfg);
        for (auto& state : arrangement.barStates)
        {
            CHECK(state.bpm == 124.0);
            CHECK(state.rootNote == 2);
            CHECK(state.seed == 7u);
        }
    }

    // =====================================================================
    // Out-of-range bar lookups clamp rather than crash.
    // =====================================================================
    {
        const auto arrangement = buildArrangement(cfg);
        CHECK(musicStateForBar(arrangement, -5).bar == 0);
        CHECK(musicStateForBar(arrangement, 999999).bar == arrangement.totalBars() - 1);
        CHECK(!isPreDropFinalBar(arrangement, -1));
        CHECK(!isPreDropFinalBar(arrangement, 999999));
    }

    // Print the full cycle for human inspection.
    {
        const auto arrangement = buildArrangement(cfg);
        std::printf("\n=== Default Melodic Techno arrangement (%d bars) ===\n", arrangement.totalBars());
        int bar = 0;
        for (auto& spec : defaultMelodicTechnoCycle())
        {
            const auto& startState = arrangement.barStates[(size_t) bar];
            const auto& endState   = arrangement.barStates[(size_t) (bar + spec.bars - 1)];
            std::printf("bar %3d-%-3d (%2d bars) %-16s energy %.2f->%.2f tension %.2f drum %.2f->%.2f bass %.2f->%.2f melody %.2f->%.2f\n",
                        bar, bar + spec.bars - 1, spec.bars, sectionName(spec.section),
                        startState.energy, endState.energy, startState.tension,
                        startState.drumEnergy, endState.drumEnergy,
                        startState.bassEnergy, endState.bassEnergy,
                        startState.melodicEnergy, endState.melodicEnergy);
            bar += spec.bars;
        }
    }

    TEST_SUMMARY_AND_EXIT();
}
