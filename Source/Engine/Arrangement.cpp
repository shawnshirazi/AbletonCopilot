#include "Arrangement.h"
#include <algorithm>

namespace Engine
{
    std::vector<SectionSpec> defaultMelodicTechnoCycle()
    {
        // Bar counts: MLPipeline/musical_target/arrangement_target.json's
        // section_length_stats (real .als-derived evidence - every real
        // section length found was a multiple of 8 except one 4-bar
        // Pre-Break). Drop/Breakdown use 16 (the most common real drop
        // length, and a real break length). PreDrop uses 4 (the real
        // Pre-Break range is 4-8; 4 is chosen so the "final bar" drop-out
        // technique - see isPreDropFinalBar - reads as a real pause, not
        // an entire section of silence).
        //
        // Energy/tension targets: a DESIGN CHOICE informed by (not lifted
        // verbatim from) melodic_techno_research.md and the user's own
        // worked example - no loudness/audio analysis exists to measure
        // these as fact. Each section's rationale:
        //   Intro: low energy, low tension, drums present but restrained,
        //     bass absent (real tracks: "Intro: atmospheric pads, filtered
        //     synths" - PML guide), melody/atmosphere leads.
        //   Establish: the groove settles in fully (drums+bass both
        //     active) but there's still no drop - matches "Full Theme"/
        //     "Bass in" real section behavior.
        //   Build: energy and drum/bass energy both ramp up across the
        //     section (start!=end) - ghost kicks/percussion increasing,
        //     per every build-technique source gathered.
        //   PreDrop: energy DIPS below Build's level (0.55->0.35) even
        //   though tension is at its highest (0.95) - this is the energy/
        //     tension divergence the user's own example calls out
        //     directly; drumEnergy/bassEnergy both thin out, creating the
        //     "leave space for the drop to feel bigger" effect on top of
        //     the explicit final-bar drop-out (isPreDropFinalBar).
        //   Drop / FinalDrop: maximum energy, moderate-low tension (a
        //     drop is a release, not itself tense) - all three per-role
        //     energies at 1.0.
        //   Breakdown: energy near zero but tension stays elevated (0.65)
        //     - "energy = low, tension = high" is exactly the state the
        //     user's own example specifies for a breakdown. drumEnergy
        //     near-zero (kick/hats/perc removed - see DrumEngine.cpp),
        //     bassEnergy low but not exactly zero (real tracks retain an
        //     occasional root/anchor note per the Myloops/PML breakdown
        //     guidance), melodicEnergy highest of any section (melody is
        //     what's exposed).
        //   BreakdownBuild: energy and drum/bass energy ramp from
        //     Breakdown's level back toward Drop's, tension even higher
        //     than Breakdown's own (0.85) - the classic "quiet but
        //     tightening" build-into-drop feeling.
        //   Outro: energy and drum/bass energy ramp DOWN - the reverse of
        //     Intro, for a DJ-mixing-friendly fade rather than a hard
        //     stop.
        return {
            { MusicSection::Intro,          8,  { 0.20f, 0.20f, 0.15f, 0.35f, 0.35f, 0.00f, 0.00f, 0.55f, 0.55f } },
            { MusicSection::Establish,      8,  { 0.40f, 0.40f, 0.20f, 0.65f, 0.65f, 0.55f, 0.55f, 0.45f, 0.45f } },
            { MusicSection::Build,          8,  { 0.50f, 0.65f, 0.55f, 0.60f, 0.80f, 0.30f, 0.60f, 0.50f, 0.55f } },
            { MusicSection::PreDrop,        4,  { 0.55f, 0.35f, 0.95f, 0.60f, 0.30f, 0.55f, 0.20f, 0.55f, 0.35f } },
            { MusicSection::Drop,           16, { 0.95f, 0.95f, 0.30f, 1.00f, 1.00f, 1.00f, 1.00f, 0.80f, 0.80f } },
            { MusicSection::Breakdown,      16, { 0.15f, 0.15f, 0.65f, 0.05f, 0.05f, 0.10f, 0.10f, 0.90f, 0.90f } },
            { MusicSection::BreakdownBuild, 8,  { 0.30f, 0.55f, 0.85f, 0.20f, 0.60f, 0.10f, 0.40f, 0.70f, 0.75f } },
            { MusicSection::FinalDrop,      16, { 1.00f, 1.00f, 0.25f, 1.00f, 1.00f, 1.00f, 1.00f, 0.90f, 0.90f } },
            { MusicSection::Outro,          8,  { 0.25f, 0.10f, 0.10f, 0.30f, 0.10f, 0.10f, 0.00f, 0.40f, 0.25f } },
        };
    }

    namespace
    {
        float lerp(float a, float b, float t) { return a + (b - a) * t; }
    }

    MusicArrangement buildArrangement(const ArrangementConfig& config)
    {
        MusicArrangement out;

        int absoluteBar = 0;
        for (auto& spec : config.sections)
        {
            for (int b = 0; b < spec.bars; ++b)
            {
                const float t = spec.bars <= 1 ? 0.0f : (float) b / (float) (spec.bars - 1);

                MusicState state;
                state.section       = spec.section;
                state.bar           = absoluteBar;
                state.barInSection  = b;
                state.barsInSection = spec.bars;
                state.energy        = lerp(spec.target.energyStart, spec.target.energyEnd, t);
                state.tension       = spec.target.tension;
                state.drumEnergy    = lerp(spec.target.drumEnergyStart, spec.target.drumEnergyEnd, t);
                state.bassEnergy    = lerp(spec.target.bassEnergyStart, spec.target.bassEnergyEnd, t);
                state.melodicEnergy = lerp(spec.target.melodicEnergyStart, spec.target.melodicEnergyEnd, t);
                state.bpm           = config.bpm;
                state.rootNote      = config.rootNote;
                state.seed          = config.seed;

                out.barStates.push_back(state);
                ++absoluteBar;
            }
        }

        return out;
    }

    const MusicState& musicStateForBar(const MusicArrangement& arrangement, int bar)
    {
        const int clamped = std::max(0, std::min(bar, (int) arrangement.barStates.size() - 1));
        return arrangement.barStates[(size_t) clamped];
    }

    bool isPreDropFinalBar(const MusicArrangement& arrangement, int bar)
    {
        if (bar < 0 || bar >= (int) arrangement.barStates.size())
            return false;
        const MusicState& s = arrangement.barStates[(size_t) bar];
        return s.section == MusicSection::PreDrop && s.barInSection == s.barsInSection - 1;
    }
}
