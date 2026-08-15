#pragma once

// GENERATED FILE - do not hand-edit.
//
// Regenerate with:
//   MLPipeline/venv/bin/python3 MLPipeline/drum_grammar/generate_rhythm_grammar_header.py
// which reads MLPipeline/drum_grammar/output/drum_grammar.json's
// role_rhythm_stats and cross_role_aggregate_vector_correlation blocks -
// real per-16th-step hit-probability/velocity distributions and
// cross-role correlation coefficients, measured by onset-detecting real
// audio loops from four Melodic-Techno-branded sample packs already in
// the user's library (PML Mirage, PML Mystique, Odd Frequency Exo, Odd
// Frequency Exo 2) - see MLPipeline/drum_grammar/analyze_drum_grammar.py
// for the measurement methodology. Not hand-tuned guesses - if a number
// here looks wrong, fix the analysis or the corpus and regenerate, don't
// edit this file.
// Generated: 2026-08-14
// KICK: n=17 loop files, 68 onsets, 17 bars analyzed, packs: PML Mirage, PML Mystique
// CLAP: n=26 loop files, 176 onsets, 73 bars analyzed, packs: PML Mirage, PML Mystique
// HAT: n=94 loop files, 3058 onsets, 240 bars analyzed, packs: Odd Frequency Exo, Odd Frequency Exo2, PML Mirage, PML Mystique
// PERC: n=56 loop files, 1590 onsets, 163 bars analyzed, packs: Odd Frequency Exo, Odd Frequency Exo2, PML Mirage, PML Mystique

#include "DrumVoiceSynth.h" // DrumRole

namespace Engine
{
    // One dimension per real, measurable rhythmic characteristic (see
    // analyze_drum_grammar.py's compute_role_stats) - step16Probability and
    // step16RelativeVelocity are indexed by 16th-note position within one
    // bar (0-15), folded across every bar in the analyzed corpus.
    struct RoleRhythmStats
    {
        float step16Probability[16];      // measured fraction of this role's onsets landing at each position (sums to 1.0)
        float step16RelativeVelocity[16];  // measured mean onset amplitude at each position, relative to that file's own loudest hit (0..1)
        float onBeatFraction;               // fraction of onsets landing exactly on a beat (step % 4 == 0)
        float eighthOffbeatFraction;        // fraction landing on the 8th-note offbeat (step % 4 == 2)
        float weakSixteenthFraction;        // fraction landing on the weakest 16ths (step % 4 in {1, 3})
        float adjacentBarsIdenticalFraction; // -1 = not enough multi-bar material to measure
        float meanOnsetsPerBar;              // real measured average hit count per bar
    };

    // Real measured Pearson correlation between each role-pair's aggregate
    // 16-step hit-probability vectors (corpus-wide, not tied to one song) -
    // positive means the two roles tend to favor the same positions,
    // negative means one favors positions the other avoids.
    struct CrossRoleCorrelation
    {
        float kickClap, kickHat, kickPerc, clapHat, clapPerc, hatPerc;
    };

    constexpr RoleRhythmStats kKickRhythm {
        { 0.250000f, 0.000000f, 0.000000f, 0.000000f, 0.250000f, 0.000000f, 0.000000f, 0.000000f, 0.250000f, 0.000000f, 0.000000f, 0.000000f, 0.250000f, 0.000000f, 0.000000f, 0.000000f },
        { 0.987600f, 0.000000f, 0.000000f, 0.000000f, 0.861600f, 0.000000f, 0.000000f, 0.000000f, 0.853600f, 0.000000f, 0.000000f, 0.000000f, 0.856100f, 0.000000f, 0.000000f, 0.000000f },
        1.000000f, 0.000000f, 0.000000f, -1.000000f, 4.000000f
    };

    constexpr RoleRhythmStats kClapRhythm {
        { 0.005700f, 0.005700f, 0.005700f, 0.005700f, 0.414800f, 0.000000f, 0.034100f, 0.011400f, 0.005700f, 0.017000f, 0.022700f, 0.022700f, 0.414800f, 0.000000f, 0.022700f, 0.011400f },
        { 0.012400f, 0.006800f, 0.007000f, 0.003200f, 0.941400f, 0.000000f, 0.042000f, 0.138800f, 0.014800f, 0.356400f, 0.373800f, 0.452300f, 0.928000f, 0.000000f, 0.324500f, 0.225400f },
        0.840900f, 0.085200f, 0.073900f, 0.808500f, 2.634615f
    };

    constexpr RoleRhythmStats kHatRhythm {
        { 0.060800f, 0.055900f, 0.076500f, 0.055300f, 0.065700f, 0.055600f, 0.076500f, 0.051300f, 0.063400f, 0.058200f, 0.075500f, 0.055900f, 0.064700f, 0.056600f, 0.076500f, 0.051300f },
        { 0.408300f, 0.368200f, 0.742000f, 0.366400f, 0.300400f, 0.354000f, 0.776400f, 0.352700f, 0.304700f, 0.369200f, 0.756000f, 0.373600f, 0.330500f, 0.365100f, 0.766700f, 0.396100f },
        0.254700f, 0.305100f, 0.440200f, 0.650700f, 12.609043f
    };

    constexpr RoleRhythmStats kPercRhythm {
        { 0.059100f, 0.056000f, 0.066700f, 0.067300f, 0.052200f, 0.052200f, 0.074200f, 0.062300f, 0.061000f, 0.056600f, 0.076700f, 0.075500f, 0.052800f, 0.058500f, 0.069800f, 0.059100f },
        { 0.579700f, 0.364800f, 0.532800f, 0.539100f, 0.303000f, 0.450300f, 0.502800f, 0.447800f, 0.518500f, 0.371300f, 0.497800f, 0.507300f, 0.372300f, 0.425900f, 0.504300f, 0.429800f },
        0.225200f, 0.287400f, 0.487400f, 0.570100f, 10.486607f
    };

    constexpr CrossRoleCorrelation kCrossRoleCorrelation {
        0.639100f, 0.075800f, -0.448800f, 0.150900f, -0.425000f, 0.475800f
    };

    inline const RoleRhythmStats& rhythmStatsForRole(DrumRole role)
    {
        switch (role)
        {
            case DrumRole::Kick:  return kKickRhythm;
            case DrumRole::Clap:  return kClapRhythm;
            case DrumRole::Hat:   return kHatRhythm;
            case DrumRole::Perc:  return kPercRhythm;
            case DrumRole::Count: return kKickRhythm;
        }
        return kKickRhythm;
    }
}
