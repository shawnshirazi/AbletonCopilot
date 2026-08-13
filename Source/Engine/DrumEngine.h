#pragma once

#include "Grid.h"
#include <cstdint>
#include <vector>

// Phase 1 deterministic engine: Melodic Techno drum-pattern generation.
//
// A real generator, not a lookup table - default/prior shapes come from
// sourced melodic-techno production research already gathered earlier this
// session (Attack Magazine's drum breakdown of an actual deep/melodic
// progressive techno track, Native Instruments' production guide), not
// invented fresh. Zero JUCE dependency (see Theory.h for why).

namespace Engine
{
    struct Hit
    {
        bool  active   = false;
        float velocity = 1.0f; // 0..1
    };

    using StepArray = std::vector<Hit>; // sized to totalSteps(grid)

    // ------------------------------------------------------------------
    // Parameter contract
    //
    // These four knobs mean the SAME general thing everywhere (how busy /
    // how off-grid / how bar-to-bar-varied / how reproducible), but not
    // every role uses every knob - a parameter is only wired up where it's
    // musically meaningful for that specific role's idiom. Where a knob is
    // intentionally a no-op for a role, that's stated explicitly below and
    // in that generate*() function's own doc comment - never a silent gap.
    //
    //   density     - how many hits are present. 0 = as few as this role's
    //                 idiom allows (its fixed baseline, if it has one, or
    //                 silence if it doesn't), 1 = as many as this role's
    //                 idiom allows before it stops being that role (a hi-hat
    //                 doesn't become a wall of noise; a kick doesn't get
    //                 busier at all, since four-on-the-floor IS its density).
    //   syncopation - WHERE hits land: bias toward the rhythmically weakest
    //                 16th-note positions (the "a" just before each beat -
    //                 steps 3/7/11/15 of a 16-step bar) rather than strong
    //                 beat-aligned positions. 0 = no bias (or role-specific
    //                 fixed idiom position), 1 = strong bias toward weak
    //                 positions.
    //   variation   - how much bar-to-bar the realized pattern differs from
    //                 a perfectly regular repeat, rather than every bar/
    //                 cycle being identical. 0 = fully regular, 1 = clearly
    //                 uneven/varied across the loop.
    //   seed        - reproducibility only: same seed + same params always
    //                 produces the same pattern; a different seed explores
    //                 a different (but equally valid, same-params-shaped)
    //                 pattern. Never changes what the OTHER parameters mean.
    struct DrumPatternParams
    {
        float    density     = 0.5f;
        float    syncopation = 0.3f;
        float    variation   = 0.2f;
        uint32_t seed        = 0;
    };

    // KICK - four-on-the-floor baseline (the genre-defining convention,
    // sourced research: "steady, not the feature").
    //   density:     NOT USED, intentionally. Kick doesn't get busier or
    //                sparser - four-on-the-floor every beat IS its density;
    //                there's no musically valid "denser than that" for this
    //                role in this genre, and "sparser" would stop being a
    //                four-on-the-floor kick at all.
    //   syncopation: probability of ONE extra pushed hit per bar, on the
    //                weakest position (the "and" of the last beat) - a real,
    //                sourced technique for driving into the next bar. 0 =
    //                never, 1 = essentially every bar.
    //   variation:   probability of dropping ONE beat per bar (a rare fill).
    //                0 = never (perfectly steady), 1 = frequent drops (still
    //                capped well below "usually," since a kick that
    //                regularly skips beats isn't four-on-the-floor anymore).
    StepArray generateKick(const StepGridConfig& grid, const DrumPatternParams& params);

    // CLAP - sparse, asymmetric per the sourced breakdown: a single hit at
    // the "2-bar mark" and a doublet flourish at the "4-bar mark" of each
    // 4-bar cycle - not a steady 2-and-4 backbeat. This fixed 4-bar shape is
    // the idiom itself, always present as the baseline.
    //   density:     probability of an extra ghost hit somewhere in a bar
    //                that the fixed pattern would otherwise leave silent.
    //                0 = pure sourced pattern only, 1 = noticeably fuller
    //                (but the sourced hits stay the loudest/primary ones).
    //   syncopation: probability of nudging a sourced hit +/-1 step off its
    //                canonical position. 0 = always exactly on the sourced
    //                position, 1 = frequently nudged off it.
    //   variation:   probability that a given 4-bar cycle SWAPS which bar
    //                gets the single hit vs. the doublet (instead of always
    //                bar-2=single/bar-4=doublet). 0 = every cycle uses the
    //                canonical bar-2/bar-4 layout, 1 = every cycle swaps it.
    StepArray generateClap(const StepGridConfig& grid, const DrumPatternParams& params);

    // HAT - closed-hat pulse on the off-beat 8ths (the "and" of each beat)
    // is the genre-defining baseline, always present (sourced), like kick's
    // four-on-the-floor. A second, quieter layer fills in the remaining
    // 16th positions for organic dynamics (also sourced).
    //   density:     how filled-in the second layer is. 0 = just the
    //                primary off-beat-8th pulse, nothing else. 1 = nearly
    //                every remaining 16th position gets a soft secondary hit.
    //   syncopation: biases the second layer's placement toward the
    //                weakest 16th positions (steps 3/7/11/15) rather than
    //                spread evenly across all remaining positions. 0 =
    //                uniform, 1 = strongly weighted toward the weak
    //                positions.
    //   variation:   thins the second layer on even-indexed bars (a real
    //                arrangement technique - alternating full/thinned bars
    //                instead of identical density every bar). 0 = every bar
    //                uses the same density, 1 = even bars run at half the
    //                density of odd bars.
    StepArray generateHat(const StepGridConfig& grid, const DrumPatternParams& params);

    // PERC - sparse accents, no fixed idiom position of its own (unlike
    // kick/clap/hat, nothing about this role is a "baseline" - it's
    // entirely density/syncopation-driven).
    //   density:     per-step probability of a hit. 0 = silent, 1 =
    //                frequent accents (still capped well under kick/hat
    //                density - this role is an accent, not a second full
    //                pattern, even at maximum).
    //   syncopation: biases hit placement toward the weakest 16th positions
    //                (steps 3/7/11/15), same mechanism as HAT's second
    //                layer. 0 = uniform placement, 1 = strongly weighted
    //                toward weak positions. (Previously this parameter only
    //                affected velocity range, which didn't match its name -
    //                fixed so "syncopation" actually controls placement.)
    //   variation:   thins even-indexed bars (same mechanism as HAT) and
    //                widens the velocity range (more dynamic swing hit-to-
    //                hit). 0 = uniform density and a narrow velocity range,
    //                1 = uneven bar-to-bar density and a wide velocity range.
    StepArray generatePerc(const StepGridConfig& grid, const DrumPatternParams& params);
}
