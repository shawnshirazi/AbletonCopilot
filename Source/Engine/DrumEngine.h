#pragma once

#include "Grid.h"
#include <cstdint>
#include <vector>

// Phase 1 deterministic engine: Melodic Techno drum-pattern generation.
//
// A real generator, not a lookup table - default/prior shapes come from
// sourced melodic-techno production research already gathered earlier this
// session (Attack Magazine's drum breakdown of an actual deep/melodic
// progressive techno track, Native Instruments' production guide, and a
// documented set of modern melodic-techno "drop" rhythmic principles - see
// the comment above generateKick below). Zero JUCE dependency (see
// Theory.h for why).

namespace Engine
{
    struct Hit
    {
        bool  active   = false;
        float velocity = 1.0f; // 0..1
    };

    using StepArray = std::vector<Hit>; // sized to totalSteps(grid)

    // A section of a track (intro/build/drop/breakdown/...). Only Drop
    // exists so far - this milestone is deliberately scoped to producing
    // one convincing 16-bar Melodic Techno drop, not every section type.
    // The other sections are real future work, not stubs pretending to be
    // finished: adding one means giving generateKick/Clap/Hat/Perc a new
    // switch case with its own rules, not touching this enum's shape.
    enum class DrumSection
    {
        Drop
    };

    // ------------------------------------------------------------------
    // Parameter contract
    //
    // Drop generation is MOTIF-based, not per-step independent
    // probability: each role builds a small, fixed 4-bar (or narrower)
    // idea once, then repeats/develops that same idea across the 16-bar
    // phrase (see the phrase structure comment above generateKick). These
    // four knobs still mean the same general thing everywhere (how busy /
    // how off-grid / how much the idea develops across the phrase / how
    // reproducible), but now they shape MOTIF CONSTRUCTION and
    // phrase-to-phrase DEVELOPMENT, not a fresh independent roll on every
    // single step of every single bar - that's precisely the "256
    // independent random decisions" outcome this design intentionally
    // avoids.
    //
    //   density     - how many hits the role's motif contains. 0 = as few
    //                 as the idiom allows, 1 = as many as it allows before
    //                 stopping being that role (a hi-hat motif doesn't
    //                 become a wall of noise; kick's four-on-the-floor
    //                 doesn't have a "denser" state).
    //   syncopation - WHERE motif hits land: bias toward the rhythmically
    //                 weakest 16th-note positions (the "a" just before
    //                 each beat) rather than strong beat-aligned ones.
    //   variation   - how much the motif is allowed to develop across the
    //                 phrase sections (establish -> subtle variation ->
    //                 development -> build tension -> phrase-ending fill)
    //                 rather than repeating completely unchanged. 0 =
    //                 bars 1-4/5-8/9-12/13-15 are all the identical
    //                 4-bar motif, 1 = each later section visibly departs
    //                 from the established idea. Never means "randomize
    //                 every bar" - even at 1, the motif inside a given
    //                 4-bar group is still one idea repeated within that
    //                 group.
    //   seed        - reproducibility only: same seed + same params
    //                 always produces the same motif and the same
    //                 pattern; a different seed explores a different
    //                 (but equally valid, same-params-shaped) motif.
    struct DrumPatternParams
    {
        float    density     = 0.5f;
        float    syncopation = 0.3f;
        float    variation   = 0.2f;
        uint32_t seed        = 0;
    };

    // Drop phrase structure (all four generate*() functions below share
    // this, matched to actual bar proportions so it degrades sensibly for
    // numBars != 16):
    //   bars  1-4    establish groove       - the motif is built here
    //   bars  5-8    repeat with subtle variation
    //   bars  9-12   develop the groove
    //   bars 13-15   maintain driving energy, subtle variation
    //   bar  16      restrained phrase-ending fill
    // Sourced Melodic Techno drop principles this engine encodes as
    // explicit rules (not arbitrary randomness) - general rhythmic
    // characteristics of the genre, not any single copyrighted track:
    //   - kick: uncompromising four-on-the-floor, the dominant and most
    //     reliable element in the pattern, consistent velocity with only
    //     subtle phrase accenting.
    //   - hat: a strong, always-present offbeat-8th pulse locked to the
    //     kick, with a restrained, motif-repeated 16th-note layer on top
    //     (not constant 16th activity) plus occasional ghost hits and
    //     occasional deliberate omissions from the offbeat pulse itself -
    //     both real groove techniques, not just "more" or "less" density.
    //   - clap/snare: on the backbeat, sparse and asymmetric (a single hit
    //     and a doublet across a 4-bar cycle, not every-bar 2-and-4),
    //     restrained so it complements rather than competes with the kick.
    //   - percussion: sparse, syncopated, placed in the negative space the
    //     other three roles leave behind (never on their fixed idiom
    //     positions), built as a small repeated motif with small
    //     per-repeat variations rather than continuous per-step chance.
    //   - phrase variation happens at the level of WHICH 4-bar group
    //     you're in, not bar-to-bar or step-to-step.

    // KICK - see the phrase-structure comment above. In Drop mode this
    // role is deliberately the least "generated": four-on-the-floor every
    // beat, every bar, is the whole idiom, not a probability outcome.
    //   density:     NOT USED. Four-on-the-floor IS the density; there is
    //                no denser or sparser state for this role in a drop.
    //   syncopation: NOT USED in Drop mode - a drop's kick doesn't push/
    //                pull against the grid, it anchors it.
    //   variation:   gates the single bar-16 phrase-ending push (see
    //                above) - 0 = kick stays perfectly steady even through
    //                the last bar, >0 = the restrained fill is present.
    StepArray generateKick(const StepGridConfig& grid, const DrumPatternParams& params,
                            DrumSection section = DrumSection::Drop);

    // CLAP - sparse, asymmetric per the sourced breakdown: a single hit at
    // the "2-bar mark" and a doublet flourish at the "4-bar mark" of each
    // 4-bar cycle - not a steady 2-and-4 backbeat. This fixed shape IS the
    // motif; by default (variation=0) every 4-bar cycle in the drop uses
    // the identical canonical layout, which is what makes bars 1-4 and
    // 5-8 recognizably the same idea.
    //   density:     probability of an extra ghost hit somewhere a cycle
    //                would otherwise leave silent. Kept low so this role
    //                stays restrained/complementary even at density=1.
    //   syncopation: probability of nudging a sourced hit +/-1 step off
    //                its canonical position - phrase-scaled, so the
    //                backbeat sits rock-solid in the establishing bars and
    //                only loosens slightly by the tension-building ones.
    //   variation:   probability a given 4-bar cycle swaps which bar gets
    //                the single hit vs. the doublet, instead of the
    //                canonical layout - "occasional phrase variation" from
    //                the brief, kept low by default so the drop stays
    //                stable. 0 = every cycle identical (exact boundary);
    //                1 = every cycle swapped (exact boundary).
    StepArray generateClap(const StepGridConfig& grid, const DrumPatternParams& params,
                            DrumSection section = DrumSection::Drop);

    // HAT - primary offbeat-8th pulse (always present, locked to kick) +
    // a restrained, MOTIF-based supporting 16th layer + a quieter ghost
    // layer. The supporting/ghost motif is built once from bars 1-4 and
    // repeated literally into bars 5-8 (same positions, same shape) -
    // bars 9-12 develop a related but distinct variant, 13-15 keep that
    // variant with a small tweak, bar 16 gets its own fill. The offbeat
    // pulse can occasionally omit a hit for groove (real technique, not
    // random noise) - phrase-scaled, rarest in the establishing bars.
    //   density:     how filled-in the supporting motif is when it's
    //                first built (bars 1-4). Kept restrained - "selective
    //                16th-note movement", never constant.
    //   syncopation: biases the motif's hit positions toward the weakest
    //                16ths rather than spread evenly.
    //   variation:   how much bars 9-12/13-15 are allowed to depart from
    //                the bars-1-4 motif (0 = they repeat it exactly too;
    //                1 = clearly different, still restrained).
    StepArray generateHat(const StepGridConfig& grid, const DrumPatternParams& params,
                          DrumSection section = DrumSection::Drop);

    // PERC - this is where the groove comes alive, per the brief: sparse,
    // syncopated accents placed only in the negative space kick/hat/clap
    // don't already occupy, built as a small repeated MOTIF (2-3 fixed
    // hit positions within a 4-bar unit, chosen once) rather than
    // continuous per-step chance - "avoid placing percussion simply
    // because a step is empty". Bars 1-4 and 5-8 repeat the identical
    // motif; 9-12 develop it (may add one position); 13-15 keep that
    // development; bar 16 gets a single restrained accent, not a fill
    // roll.
    //   density:     how many motif positions get chosen (kept low - an
    //                accent role, not a second pattern, even at 1).
    //   syncopation: biases motif position choice toward the weakest
    //                16ths.
    //   variation:   probability the development sections (9-12/13-15)
    //                actually add a position on top of the base motif,
    //                and widens per-hit velocity range for movement
    //                across the 16 bars.
    StepArray generatePerc(const StepGridConfig& grid, const DrumPatternParams& params,
                           DrumSection section = DrumSection::Drop);

    // Converts a generated StepArray into a flat 0-127 integer velocity
    // array (0 = no hit, matching MIDI velocity range). This is the single
    // conversion both the audio path (PluginProcessor's
    // GeneratedDrumRole::velocity, driving DrumVoiceSynth) and the UI grid
    // display consume - calling this once per role and handing the SAME
    // result to both is what guarantees the UI always shows exactly the
    // pattern that's playing, never a second independently-derived one.
    std::vector<int> toVelocityArray(const StepArray& steps);
}
