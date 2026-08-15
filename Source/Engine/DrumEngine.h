#pragma once

#include "Grid.h"
#include <cstdint>
#include <vector>

// Phase 6 deterministic engine: Melodic Techno DROP drum-pattern
// generation, driven by MEASURED data (Source/Engine/DrumRhythmGrammar.h,
// generated from MLPipeline/drum_grammar/output/drum_grammar.json - real
// onset-detected statistics from Melodic-Techno-branded sample packs
// already in the user's library), not hand-authored heuristic ranges.
// Zero JUCE dependency (see Theory.h for why).

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
    // together first, hat is built with awareness of where they already
    // sound, and percussion is placed by explicitly weighing the real
    // measured correlation between percussion and each other role's
    // occupied positions (Source/Engine/DrumRhythmGrammar.h's
    // CrossRoleCorrelation) - never independently. One shared seed drives
    // the whole composition (a single RNG stream), not one seed per role.
    //
    //   density     - how filled-in each role's motif is, scaled from
    //                 that role's OWN measured baseline (see
    //                 DrumRhythmGrammar.h) - 0.5 tracks the measured
    //                 corpus average, not an arbitrary midpoint. Kick
    //                 doesn't use this (four-on-the-floor IS its density
    //                 - the corpus measured 100% on-beat placement, there
    //                 is no denser/sparser state for this role in a drop).
    //   syncopation - WHERE motif hits land: bias toward the rhythmically
    //                 weakest 16th-note positions (the "a" just before
    //                 each beat), layered on top of (not replacing) each
    //                 role's measured position distribution.
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

    // The full coordinated 16-bar drop: all four roles, generated
    // together in one pass (kick -> clap -> hat -> perc, each role aware
    // of every role generated before it - see generateDrop's own doc
    // comment in DrumEngine.cpp for exactly how). This is the ONLY public
    // entry point for Drop generation - there is deliberately no
    // per-role generateKick/generateClap/generateHat/generatePerc
    // anymore, because generating them independently is exactly the
    // architectural problem this milestone replaces (see the commit this
    // shipped in for the full rationale).
    struct DropPattern
    {
        StepArray kick, clap, hat, perc;
    };

    // Drop phrase structure (matched to actual bar proportions so it
    // degrades sensibly for numBars != 16):
    //   bars  1-4    establish groove       - the coordinated block is built here
    //   bars  5-8    repeat that block literally
    //   bars  9-12   develop the block (controlled, seed-driven variation)
    //   bars 13-15   repeat the developed block
    //   bar  16      restrained phrase-ending fill
    // Every role's base shape (which positions it favors, how loud each
    // position is, how dense it is, how it relates to the other three
    // roles) comes from DrumRhythmGrammar.h's measured statistics - see
    // that file's own header comment for the exact corpus (four
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
