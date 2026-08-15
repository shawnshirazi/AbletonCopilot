#pragma once

#include "Grid.h"
#include <cstdint>
#include <vector>

// Phase 6+ deterministic engine: Melodic Techno DROP drum-pattern
// generation, driven by MEASURED data (Source/Engine/DrumRhythmGrammar.h,
// generated from MLPipeline/drum_grammar/output/drum_grammar.json - real
// onset-detected statistics from Melodic-Techno-branded sample packs
// already in the user's library) layered with an explicit ARRANGEMENT
// structure (a hat hierarchy, two percussion voices, and a 4-stage energy
// arc across the 16-bar phrase) - see generateDrop's own comment in
// DrumEngine.cpp for the full design. Zero JUCE dependency (see Theory.h
// for why).

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
    // finished: adding one means giving generateDrop a new switch case,
    // not touching this enum's shape.
    enum class DrumSection
    {
        Drop
    };

    // ------------------------------------------------------------------
    // Parameter contract
    //
    // Drop generation is MOTIF-based and COORDINATED, not per-role
    // independent probability: kick and clap establish the groove
    // together first, the hat hierarchy (closed pulse + open/ride accent)
    // is built with awareness of where they already sound, and two
    // percussion voices are placed by explicitly weighing the real
    // measured correlation between percussion and every other role's
    // occupied positions (Source/Engine/DrumRhythmGrammar.h's
    // CrossRoleCorrelation) - never independently. One shared seed drives
    // the whole composition (a single RNG stream), not one seed per role.
    //
    //   density     - how filled-in each role's motif is, scaled from
    //                 that role's OWN measured baseline (see
    //                 DrumRhythmGrammar.h) AND from the current phrase
    //                 section's energy level (see the energy-arc comment
    //                 above generateDrop) - 0.5 tracks the measured
    //                 corpus average at the phrase's peak-energy section,
    //                 not an arbitrary midpoint. Kick doesn't use this
    //                 (four-on-the-floor IS its density - the corpus
    //                 measured 100% on-beat placement, there is no
    //                 denser/sparser state for this role in a drop).
    //   syncopation - WHERE motif hits land: bias toward the rhythmically
    //                 weakest 16th-note positions (the "a" just before
    //                 each beat), layered on top of (not replacing) each
    //                 role's measured position distribution.
    //   variation   - how much each phrase section's motif is allowed to
    //                 develop from the previous section's (establish ->
    //                 repeat+subtle variation -> increase energy ->
    //                 full drop), and how much the bar-16 transition
    //                 thins/accents the established material. 0 =
    //                 minimal movement between sections (still not
    //                 identical - the energy arc itself always applies),
    //                 1 = each later section visibly departs from the
    //                 previous one.
    //   seed        - reproducibility only: same seed + same params
    //                 always produces the same 16-bar composition; a
    //                 different seed explores a different (but equally
    //                 valid, same-params-shaped, same-measured-grammar)
    //                 composition.
    struct DrumPatternParams
    {
        float    density     = 0.5f;
        float    syncopation = 0.3f;
        float    variation   = 0.2f;
        uint32_t seed        = 0;
    };

    // The full coordinated 16-bar drop: six roles, generated together in
    // one pass (kick -> clap -> hatClosed -> hatOpen -> percA -> percB,
    // each role aware of every role generated before it - see
    // generateDrop's own doc comment in DrumEngine.cpp for exactly how).
    // This is the ONLY public entry point for Drop generation - there is
    // deliberately no per-role generateKick/generateClap/generateHat*/
    // generatePerc* function, because generating roles independently is
    // exactly the architectural problem this design replaces.
    //
    // hatClosed carries BOTH the primary offbeat pulse and its quieter
    // ghost/secondary 16th movement (one StepArray, velocity-differentiated
    // - real closed-hat samples commonly serve both roles at different
    // velocities, and DrumRhythmGrammar.h's measured HAT data doesn't
    // distinguish a separate "ghost" instrument category to justify a
    // seventh voice). hatOpen, percA, and percB are each a fully
    // independent voice/role with their own sample selection (see
    // Source/DrumSampleSelector.h) - "ride/top" material is folded into
    // hatOpen's candidate pool rather than given its own eighth role, per
    // the brief's own "optional... don't make it constant" framing.
    struct DropPattern
    {
        StepArray kick, clap, hatClosed, hatOpen, percA, percB;
    };

    // Drop phrase structure - a real 4-stage ENERGY ARC across the 16
    // bars, not a flat "establish once, copy forever" loop:
    //   bars  1-4   (0-3)   establish  - the core groove, restrained energy
    //   bars  5-8   (4-7)   develop    - same groove, subtle hat/perc variation
    //   bars  9-12  (8-11)  increase   - more hat/open-hat/perc movement,
    //                                    percB (if it wasn't already present)
    //                                    can enter here
    //   bars 13-15  (12-14) full drop  - strongest, most sustained version
    //   bar  16     (15)    transition - thinned/accented, not filled -
    //                                    density/velocity/omission signal
    //                                    the phrase is about to loop, never
    //                                    a random hit spray
    // Every role's base shape (which positions it favors, how loud each
    // position is, how dense it is, how it relates to the other roles
    // already placed) comes from DrumRhythmGrammar.h's measured statistics
    // - see that file's own header comment for the exact corpus (four
    // Melodic-Techno-branded sample packs, real onset-detected audio, not
    // hand-picked genre lore).
    DropPattern generateDrop(const StepGridConfig& grid, const DrumPatternParams& params,
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
