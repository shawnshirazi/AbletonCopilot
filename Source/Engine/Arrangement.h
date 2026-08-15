#pragma once

#include "MusicState.h"
#include <cstdint>
#include <vector>

// Builds a full, deterministic, bar-by-bar musical arrangement (a sequence
// of MusicState values) that DrumEngine and BassEngine both walk to decide
// what happens in each bar - see MusicState.h's own comment for why this
// exists. Zero JUCE dependency.
namespace Engine
{
    // One section's length and its energy/tension targets. Energy fields
    // are given as (start, end) so a section can ramp across its own bars
    // (Build/BreakdownBuild/Outro do; Drop/Breakdown/Intro/Establish/
    // PreDrop hold roughly flat - start==end for those). Interpolation is
    // linear across barInSection/barsInSection - deliberately simple, not
    // an eased curve; nothing in the evidence gathered justifies a more
    // elaborate curve shape.
    struct SectionEnergyTarget
    {
        float energyStart = 0.0f, energyEnd = 0.0f;
        float tension      = 0.0f; // held flat across the section - the reference material's own tension examples are one value per section, not a ramp
        float drumEnergyStart = 0.0f, drumEnergyEnd = 0.0f;
        float bassEnergyStart = 0.0f, bassEnergyEnd = 0.0f;
        float melodicEnergyStart = 0.0f, melodicEnergyEnd = 0.0f;
    };

    struct SectionSpec
    {
        MusicSection section;
        int bars;
        SectionEnergyTarget target;
    };

    // Default Melodic Techno cycle - section order, lengths (bars), and
    // energy/tension targets. Lengths come directly from
    // MLPipeline/musical_target/arrangement_target.json's
    // section_length_stats (every real section length found there is a
    // multiple of 8, except one 4-bar Pre-Break micro-transition; Drop/
    // Breakdown use 16, matching the most common real length in that
    // data). Energy/tension NUMBERS themselves are a design choice
    // informed by, not copied verbatim from, the research (no audio
    // loudness analysis was performed on the reference tracks - see
    // melodic_techno_research.md section H) - each value's rationale is
    // in Arrangement.cpp next to where it's defined, not asserted here as
    // measured fact.
    std::vector<SectionSpec> defaultMelodicTechnoCycle();

    struct ArrangementConfig
    {
        std::vector<SectionSpec> sections = defaultMelodicTechnoCycle();
        double   bpm      = 124.0; // matches the two 124 BPM and one 125 BPM real reference tracks
        int      rootNote = 0;
        uint32_t seed     = 0;
    };

    struct MusicArrangement
    {
        std::vector<MusicState> barStates; // one entry per bar, in section order
        int totalBars() const { return (int) barStates.size(); }
    };

    MusicArrangement buildArrangement(const ArrangementConfig& config);

    // Convenience accessor - barStates[bar], clamped to a valid index (so
    // callers never need to bounds-check a bar count they already trust).
    const MusicState& musicStateForBar(const MusicArrangement& arrangement, int bar);

    // True for the single bar the "leave space before the drop" technique
    // targets - the LAST bar of a PreDrop section (a real, sourced
    // technique: "Festival Pre Drops: Short pauses (1-4 bars) before a
    // drop to maximize impact", melodic_techno_research.md section 3.1).
    // DrumEngine/BassEngine both consult this directly rather than each
    // re-deriving "last bar of PreDrop" independently.
    bool isPreDropFinalBar(const MusicArrangement& arrangement, int bar);
}
