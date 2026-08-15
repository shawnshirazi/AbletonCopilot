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
// HAT: n=33 loop files, 602 onsets, 83 bars analyzed, packs: Odd Frequency Exo, Odd Frequency Exo2, PML Mirage, PML Mystique
// PERC: n=56 loop files, 1590 onsets, 163 bars analyzed, packs: Odd Frequency Exo, Odd Frequency Exo2, PML Mirage, PML Mystique
// RIDE: n=19 loop files, 352 onsets, 47 bars analyzed, packs: Odd Frequency Exo, Odd Frequency Exo2, PML Mirage

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
    // kickRide/clapRide/hatRide/percRide let HatOpen (whose position
    // shape comes from kRideRhythm) weigh its placement against every
    // role generated before it, the same way HAT/PERC already do for
    // their own roles - real measured numbers, not assumed (e.g.
    // HAT<->RIDE is a very strong 0.93 - both genuinely favor the same
    // offbeat-8th positions in the corpus).
    struct CrossRoleCorrelation
    {
        float kickClap, kickHat, kickPerc, clapHat, clapPerc, hatPerc;
        float kickRide, clapRide, hatRide, percRide;
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
        { 0.064800f, 0.029900f, 0.127900f, 0.026600f, 0.073100f, 0.026600f, 0.124600f, 0.018300f, 0.061500f, 0.034900f, 0.122900f, 0.034900f, 0.071400f, 0.029900f, 0.126200f, 0.026600f },
        { 0.505900f, 0.350000f, 0.720800f, 0.426500f, 0.402400f, 0.263800f, 0.803000f, 0.384700f, 0.406200f, 0.350000f, 0.746900f, 0.401500f, 0.419900f, 0.277300f, 0.788600f, 0.421600f },
        0.270800f, 0.501700f, 0.227600f, 0.660000f, 6.856061f
    };

    constexpr RoleRhythmStats kPercRhythm {
        { 0.059100f, 0.056000f, 0.066700f, 0.067300f, 0.052200f, 0.052200f, 0.074200f, 0.062300f, 0.061000f, 0.056600f, 0.076700f, 0.075500f, 0.052800f, 0.058500f, 0.069800f, 0.059100f },
        { 0.579700f, 0.364800f, 0.532800f, 0.539100f, 0.303000f, 0.450300f, 0.502800f, 0.447800f, 0.518500f, 0.371300f, 0.497800f, 0.507300f, 0.372300f, 0.425900f, 0.504300f, 0.429800f },
        0.225200f, 0.287400f, 0.487400f, 0.570100f, 10.486607f
    };

    constexpr RoleRhythmStats kRideRhythm {
        { 0.059700f, 0.062500f, 0.122200f, 0.011400f, 0.059700f, 0.051100f, 0.122200f, 0.011400f, 0.054000f, 0.068200f, 0.122200f, 0.011400f, 0.059700f, 0.051100f, 0.122200f, 0.011400f },
        { 0.310500f, 0.151600f, 0.887900f, 0.147000f, 0.241300f, 0.152500f, 0.875000f, 0.159500f, 0.247200f, 0.163000f, 0.887700f, 0.155600f, 0.254800f, 0.137300f, 0.895000f, 0.159500f },
        0.233000f, 0.488600f, 0.278400f, 0.821400f, 7.526316f
    };

    constexpr CrossRoleCorrelation kCrossRoleCorrelation {
        0.639100f, 0.075100f, -0.448800f, 0.129000f, -0.425000f, 0.507600f,
        -0.062000f, 0.001800f, 0.906400f, 0.337900f
    };

    // RIDE (kRideRhythm above) is real measured data but not one of
    // DrumRole's own roles - it's a distinct instrument category in the
    // corpus (ride cymbal / "top" loops), consulted directly by name from
    // DrumEngine.cpp to shape the OPEN HAT role's placement (a real, measured
    // stand-in for "open hat / ride / top" material, per the corpus - see
    // DrumEngine.cpp's hat-hierarchy comment for the reasoning), so it has no
    // case below.
    inline const RoleRhythmStats& rhythmStatsForRole(DrumRole role)
    {
        switch (role)
        {
            case DrumRole::Kick:      return kKickRhythm;
            case DrumRole::Clap:      return kClapRhythm;
            case DrumRole::HatClosed: return kHatRhythm;
            case DrumRole::HatOpen:   return kRideRhythm;
            case DrumRole::PercA:     return kPercRhythm;
            case DrumRole::PercB:     return kPercRhythm;
            case DrumRole::Count:     return kKickRhythm;
        }
        return kKickRhythm;
    }
}
