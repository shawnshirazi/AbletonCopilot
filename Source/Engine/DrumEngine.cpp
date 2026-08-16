#include "DrumEngine.h"
#include "DrumRhythmGrammar.h"
#include "DrumVoiceSynth.h" // DrumRole, used only to index DrumRhythmGrammar's measured tables
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace Engine
{
    namespace
    {
        // ONE shared RNG stream drives the WHOLE coordinated composition -
        // no role is generated independently (see generateDrop below), so
        // there is no per-role salt.
        constexpr uint32_t kDropSalt   = 0x44524F50u; // 'DROP'
        constexpr int       kBlockBars = 4;

        std::mt19937 makeRng(uint32_t seed) { return std::mt19937(seed ^ kDropSalt); }

        float uniform01(std::mt19937& rng)
        {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            return dist(rng);
        }

        float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

        void setHit(StepArray& steps, int step, float velocity)
        {
            if (step >= 0 && step < (int) steps.size())
            {
                steps[(size_t) step].active   = true;
                steps[(size_t) step].velocity = velocity;
            }
        }

        // The rhythmically weakest 16th-note position in each beat is the
        // "a" just before the next beat (steps 3/7/11/15 of a 16-step bar).
        bool isWeakPosition(int stepInBar, int stepsPerBar)
        {
            const int stepsPerBeat = std::max(1, stepsPerBar / 4);
            return (stepInBar % stepsPerBeat) == (stepsPerBeat - 1);
        }

        // ------------------------------------------------------------------
        // The core data-driven mechanism this whole engine is built on:
        // convert a role's MEASURED per-position statistics
        // (DrumRhythmGrammar.h) into a per-position activation PROBABILITY.
        // step16Probability[pos] is a normalized distribution (sums to 1.0
        // - "of this role's onsets, what fraction landed here"), so
        // multiplying by meanOnsetsPerBar converts it into the expected
        // number of hits at that position per bar, which for a single
        // 16th-note slot is a real, derivable activation probability
        // (verified against the measured KICK table: step16Probability[0]
        // =0.25, meanOnsetsPerBar=4.0 -> activation=1.0, exactly matching
        // the corpus's 100%-four-on-the-floor finding).
        float measuredActivation(const RoleRhythmStats& stats, int stepInBar)
        {
            return clamp01(stats.step16Probability[stepInBar] * stats.meanOnsetsPerBar);
        }

        // A measured velocity of exactly 0 means the corpus never recorded
        // an onset there - if density/correlation still pushes this
        // position active, it needs SOME audible velocity rather than a
        // silent (effectively vel=0) "hit", so it falls back to a low,
        // ghost-appropriate level instead of 0.
        float measuredVelocity(const RoleRhythmStats& stats, int stepInBar)
        {
            const float v = stats.step16RelativeVelocity[stepInBar];
            return v > 0.0f ? v : 0.15f;
        }

        // syncopation knob, layered ON TOP of the measured base rate (not
        // replacing it) - biases toward the weakest 16th in each beat.
        float applySyncopation(float activation, int stepInBar, int stepsPerBar, float syncopation)
        {
            const float bias = isWeakPosition(stepInBar, stepsPerBar)
                                    ? (1.0f + syncopation)
                                    : (1.0f - syncopation * 0.3f);
            return clamp01(activation * bias);
        }

        // Real measured correlation, applied as a multiplicative
        // adjustment: a position already occupied by a role this one
        // correlates POSITIVELY with becomes more likely; NEGATIVELY
        // correlated, less likely. This is "percussion responds to where
        // kick/clap/hat/ride already sound" implemented directly from
        // DrumRhythmGrammar.h's measured CrossRoleCorrelation.
        float correlationFactor(float corr, bool otherOccupied)
        {
            return otherOccupied ? std::max(0.0f, 1.0f + corr) : 1.0f;
        }

        bool occupiedAt(const StepArray& block, int bar, int stepInBar, int stepsPerBar)
        {
            const size_t idx = (size_t) (bar * stepsPerBar + stepInBar);
            return idx < block.size() && block[idx].active;
        }
    } // end anonymous namespace - internal helpers above stay private to this file

    // CorrelationRef/StageEnergy/StageBlocks are declared+defined in
    // DrumEngine.h now (public - see that header's own comment for why),
    // not redefined here.

    namespace
    {
        // Decides ONE position for ONE role: measured base rate ->
        // syncopation -> cross-role correlation against every supplied
        // reference -> a single weighted coin flip. This is the one place
        // density/syncopation/correlation/randomness actually meet -
        // everything above it is data, everything below it is the
        // arrangement structure (phrase energy arc, hat hierarchy, motif
        // build-then-copy/develop) documented above generateDrop.
        bool decidePosition(std::mt19937& rng, const RoleRhythmStats& stats, int stepInBar, int stepsPerBar,
                             float densityScale, float syncopation,
                             const std::vector<CorrelationRef>& refs, float& outVelocity)
        {
            float activation = measuredActivation(stats, stepInBar) * densityScale;
            activation = applySyncopation(activation, stepInBar, stepsPerBar, syncopation);
            for (auto& ref : refs)
                if (ref.block != nullptr)
                    activation *= correlationFactor(ref.corr, occupiedAt(*ref.block, ref.bar, stepInBar, stepsPerBar));
            activation = clamp01(activation);

            outVelocity = measuredVelocity(stats, stepInBar);
            return uniform01(rng) < activation;
        }
    } // end anonymous namespace

    // Builds one role's full kBlockBars-bar block from scratch. Bar 0
    // is the canonical shape (fresh weighted decision per position,
    // see decidePosition, referencing every role already placed via
    // `refs`). Bars 1-3 either literally repeat bar 0 (with
    // probability = the role's OWN measured adjacentBarsIdentical
    // fraction - a real corpus number) or apply 1-2 small "touches"
    // (a position toggled on/off, using the role's own measured
    // velocity when adding) - controlled bar-to-bar movement inside
    // one 4-bar idea, matching what the corpus actually shows (not
    // 100% identical bar-to-bar, but not independent either).
    //
    // Public - reused as-is by Source/Engine/GrooveLoop.cpp for its own
    // clap/hat/perc blocks (see that file), not just generateDrop below.
    StepArray buildRoleBlock(std::mt19937& rng, const RoleRhythmStats& stats, int stepsPerBar,
                              float densityScale, float syncopation, const std::vector<CorrelationRef>& refs)
    {
        StepArray block((size_t) (kBlockBars * stepsPerBar));

        for (int s = 0; s < stepsPerBar; ++s)
        {
            float vel = 0.0f;
            if (decidePosition(rng, stats, s, stepsPerBar, densityScale, syncopation, refs, vel))
                setHit(block, s, vel);
        }

        const float identicalFraction = stats.adjacentBarsIdenticalFraction >= 0.0f
                                             ? stats.adjacentBarsIdenticalFraction
                                             : 0.7f; // unmeasured fallback (not used by any role in this engine - every role here has real multi-bar data)

        for (int bar = 1; bar < kBlockBars; ++bar)
        {
            const int base = bar * stepsPerBar;
            for (int s = 0; s < stepsPerBar; ++s)
                block[(size_t) (base + s)] = block[(size_t) s]; // start from bar 0's canonical shape

            if (densityScale <= 0.0f)
                continue; // genuinely silent role at this scale (e.g. percB before it enters) - a touch can't introduce content from nothing without contradicting its own zero density

            if (uniform01(rng) < identicalFraction)
                continue; // literal repeat of bar 0 - matches the corpus's own measured repetition rate

            const int touches = 1 + (uniform01(rng) < 0.5f ? 0 : 1); // 1 or 2 positions touched, never a full re-roll
            for (int t = 0; t < touches; ++t)
            {
                const int s = (int) (uniform01(rng) * (float) stepsPerBar);
                const size_t idx = (size_t) (base + s);
                if (block[idx].active)
                    block[idx] = Hit{};
                else
                    setHit(block, base + s, measuredVelocity(stats, s));
            }
        }

        return block;
    }

    namespace
    {
        // Derives a role's block for a LATER phrase section from the
        // PREVIOUS section's block for the same role - "develop existing
        // motifs rather than replacing everything", not a fresh
        // independent pattern. `touches` toggles (add a hit where it was
        // silent using the role's measured velocity, remove one where it
        // was active) are applied across the whole kBlockBars-bar block,
        // scaled by how much this section's energy actually changed from
        // the previous one (see the StageEnergy table in generateDrop) and
        // by params.variation - a bigger jump in energy, or a higher
        // variation setting, means more (and more audible) movement.
        //
        // ADD-type touches still consult `refs` (the same cross-role
        // correlations the fresh build used) before committing to a
        // position - up to 5 candidates are tried, and if NONE clears the
        // weight floor the touch is skipped entirely rather than forced
        // onto a bad position. Without this, a touch could land percussion
        // directly on the kick's beat purely because that's where the RNG
        // pointed, silently contradicting the measured negative
        // correlation the fresh build already respects - "no impossible/
        // overlapping role behavior" is a real, checked property, not
        // just true by luck. REMOVE-type touches (the position was
        // already active) never need this check - removing a hit can't
        // violate a negative correlation.
        StepArray deriveStageBlock(std::mt19937& rng, const StepArray& previousBlock, const RoleRhythmStats& stats,
                                    int stepsPerBar, float energyDelta, float variation,
                                    const std::vector<CorrelationRef>& refs)
        {
            StepArray block = previousBlock;
            const int touches = std::max(1, (int) std::lround(std::abs(energyDelta) * 6.0f + variation * 2.0f));
            for (int t = 0; t < touches; ++t)
            {
                const int bar = (int) (uniform01(rng) * (float) kBlockBars);
                int chosenStep = -1;
                for (int attempt = 0; attempt < 5; ++attempt)
                {
                    const int s = (int) (uniform01(rng) * (float) stepsPerBar);
                    const size_t idx = (size_t) (bar * stepsPerBar + s);
                    if (block[idx].active)
                    {
                        chosenStep = s; // removing is always fine - no correlation concern
                        break;
                    }
                    float weight = 1.0f;
                    for (auto& ref : refs)
                        if (ref.block != nullptr)
                            weight *= correlationFactor(ref.corr, occupiedAt(*ref.block, ref.bar, s, stepsPerBar));
                    if (weight > 0.6f) // a position not strongly negatively correlated with anything already occupied
                    {
                        chosenStep = s;
                        break;
                    }
                }
                if (chosenStep < 0)
                    continue; // no acceptable ADD position found in 5 tries - skip this touch rather than force a bad one

                const size_t idx = (size_t) (bar * stepsPerBar + chosenStep);
                if (block[idx].active)
                    block[idx] = Hit{};
                else
                    setHit(block, bar * stepsPerBar + chosenStep, measuredVelocity(stats, chosenStep));
            }
            return block;
        }
    } // end anonymous namespace

    // Copies one full kBlockBars-bar block literally into `barCount`
    // consecutive destination bars (wrapping through the block's own
    // bars if barCount exceeds kBlockBars) - this is what makes a
    // repeat group byte-identical to its source block. Public - reused
    // as-is by Source/Engine/GrooveLoop.cpp.
    void copyBlock(StepArray& steps, int destBarStart, int barCount, int numBars, int stepsPerBar,
                    const StepArray& block)
    {
        const int blockBars = (int) block.size() / stepsPerBar;
        for (int i = 0; i < barCount; ++i)
        {
            const int destBar = destBarStart + i;
            if (destBar >= numBars)
                break;
            const int srcBar   = i % blockBars;
            const int srcBase  = srcBar * stepsPerBar;
            const int destBase = destBar * stepsPerBar;
            for (int s = 0; s < stepsPerBar; ++s)
                steps[(size_t) (destBase + s)] = block[(size_t) (srcBase + s)];
        }
    }

    // Per-phrase-section relative weight (multiplied by each role's own
    // measured/calibrated density scale, see generateDrop) - the real
    // ENERGY ARC: establish stays restrained, develop nudges up,
    // increase pushes further, fullDrop is the strongest sustained
    // version. hatOpen's own numbers ramp far more steeply than
    // hatClosed's (0.10 -> 0.75, nearly 7.5x) so it's genuinely
    // near-absent in bars 1-4 and only becomes a real presence from
    // bars 9 on, matching "don't make it constant from bar 1... consider
    // introducing it later, especially bars 9-15". percB is silent
    // (0.0) in the establish section entirely - a real "this layer
    // hasn't entered yet" arrangement decision, not a density
    // rounding-to-zero accident - and enters at moderate presence from
    // the develop section.
    //
    // The actual kEstablish/kDevelop/kIncrease/kFullDrop arc data stays
    // private below, since that specific 4-stage arc is exactly what
    // Source/Engine/GrooveLoop.cpp's flat 8-bar loop deliberately does NOT
    // use (GrooveLoop.cpp builds its own single flat StageEnergy value
    // instead of reusing any of these 4) - only the StageEnergy TYPE
    // itself (declared in DrumEngine.h) is public.
    namespace
    {
        // Values below were reduced (~25-30%) from an earlier pass in
        // direct response to real listening feedback: "hats/perc still too
        // busy," even though the generated output already measured BELOW
        // the corpus's own mean activity (drum_grammar.json HAT.mean_
        // onsets_per_bar=6.86 vs generated ~4.08/bar at the old values) -
        // per melodic_techno_research.md section 13's own warning, matching
        // a measured average is not the same as sounding intentional; real
        // tracks read as restrained because of contrast/negative space, not
        // because they hit a specific event count. This is a disclosed
        // subjective tuning response, not a new measurement - the relative
        // establish<develop<increase<fullDrop arc shape (the actual
        // "groove intensifies through the drop" story) is preserved
        // exactly, only the absolute levels are pulled back.
        constexpr StageEnergy kEstablish { 0.40f, 0.10f, 0.45f, 0.00f };
        constexpr StageEnergy kDevelop   { 0.50f, 0.20f, 0.55f, 0.30f };
        constexpr StageEnergy kIncrease  { 0.65f, 0.40f, 0.65f, 0.55f };
        constexpr StageEnergy kFullDrop  { 0.80f, 0.55f, 0.75f, 0.70f };
    } // end anonymous namespace

    // StageBlocks (the return type of buildStageFresh/deriveStage below,
    // both reused by Source/Engine/GrooveLoop.cpp) is declared+defined in
    // DrumEngine.h now, not redefined here.

    namespace
    {
        // percA<->percB is NOT a measured correlation - the corpus has no
        // second percussion instrument category to measure. This is a
        // disclosed, deliberate arrangement rule ("avoid simply
        // duplicating" the other percussion voice), applied through the
        // exact same correlationFactor mechanism as every measured
        // correlation, just with a stated-not-measured coefficient.
        constexpr float kPercBAvoidsPercA = -0.5f;
    } // end anonymous namespace

    // ----------------------------------------------------------------------
    // generateDrop - the coordinated, arranged 16-bar Melodic Techno drop.
    //
    // FOUNDATION (Kick, Clap) is built first and stays essentially constant
    // through the whole 16 bars - the corpus measured kick at 100% on-beat
    // placement and clap at ~81% adjacent-bar repetition, i.e. real
    // melodic-techno drops don't meaningfully vary these two roles bar to
    // bar; every other role is generated with awareness of where they
    // already sound (kickClap/kickHat/kickPerc/kickRide, clapHat/clapPerc/
    // clapRide correlations from DrumRhythmGrammar.h).
    //
    // HIGH END is a real hierarchy, not one hat role:
    //   - hatClosed carries the measured offbeat-8th pulse (roughly double
    //     the velocity of its own supporting 16th movement - the real
    //     "strong pulse, quieter ghost" relationship, read directly off
    //     kHatRhythm.step16RelativeVelocity) plus that quieter secondary
    //     16th movement, in ONE role/one sample (see DrumEngine.h for why).
    //   - hatOpen is a SEPARATE role/sample, shaped by the corpus's real
    //     RIDE measurements (kRideRhythm) - ride cymbals and open hats
    //     share the same "open, ringing, offbeat-favoring" acoustic
    //     character in real production, and no other one-shot corpus in
    //     this library distinguishes them. Its presence is scaled by the
    //     StageEnergy table above so it's genuinely near-absent early in
    //     the phrase and only becomes prominent from bars 9-15, per the
    //     brief's explicit "don't make it constant from bar 1".
    //
    // GROOVE is two independent percussion voices (percA, percB - see
    // Source/DrumSampleSelector.h for how two different real samples get
    // picked). Both use the corpus's measured PERC position/velocity shape
    // (kPercRhythm) and both are reweighted by the real measured
    // kickPerc/clapPerc/hatPerc/percRide correlations - genuine "negative
    // space" (percussion measurably avoids kick/clap's positions and
    // favors hat/ride's, per the signs of those correlations) rather than
    // an absolute exclusion rule. percB is ADDITIONALLY reweighted away
    // from percA's own occupied positions (a disclosed, NOT separately
    // measured rule - the corpus doesn't distinguish two percussion
    // instrument categories - see kPercBAvoidsPercA below) so the two
    // voices play complementary parts instead of doubling each other.
    // percB is also the one role that's genuinely silent in the establish
    // section (see StageEnergy) - a real "this layer hasn't entered yet"
    // decision, not every layer firing on every generation.
    //
    // TRANSITIONS: bar 16 is built by THINNING the full-drop section's own
    // material (keep hatClosed's strong pulse, drop most of its ghost
    // layer; keep at most one hatOpen/percA/percB hit each) rather than
    // adding anything - real density/velocity/omission-driven tension, not
    // a fill roll - see buildTransitionBar below.
    //
    // Coordination order within every section: kick/clap (built once,
    // shared by every section) -> hatClosed -> hatOpen -> percA -> percB,
    // each referencing every role already placed in THAT section. One
    // shared RNG stream for the whole composition.
    namespace
    {
        // Shared by buildStageFresh and deriveStage's rebuild-fresh path
        // (see kBigDropRebuildThreshold below) so both compute the
        // identical percA/percB baseline density scale, not two copies
        // that could silently drift apart. Calibrated so a SINGLE
        // percussion voice's realized density lands near the measured
        // accent-style subset's mean (~4.78 hits/bar) rather than the raw
        // corpus-wide average (~10.5, which blends in continuous
        // "*Perc Loop*"-named material - see the PERC density milestone
        // commit for the full investigation).
        float percRoleBaseScale()
        {
            constexpr float kPercAccentMeanHitsPerBar = 4.78f;
            constexpr float kPercTouchAmplification   = 0.75f;
            return kPercAccentMeanHitsPerBar / kPercRhythm.meanOnsetsPerBar * kPercTouchAmplification;
        }
    } // end anonymous namespace

    // `energy` is a parameter (not hardcoded to kEstablish) so this one
    // function serves both generateDrop's original fixed 4-stage arc
    // (called once, with kEstablish, as its stage1), generateArrangementDrop's
    // continuous per-4-bar-block chain (called once per arrangement, for
    // its first block, with that block's own MusicState-derived energy),
    // and Source/Engine/GrooveLoop.cpp's flat 8-bar loop (called once, with
    // a single non-arc energy level, for its first/established 4-bar
    // block) - reused as-is by all three, never duplicated.
    StageBlocks buildStageFresh(std::mt19937& rng, const StepArray& kickBlock, const StepArray& clapBlock,
                                 int stepsPerBar, float overallDensity, float syncopation, const StageEnergy& energy)
    {
        StageBlocks sb;

        const float hatClosedScale = (0.7f + overallDensity * 0.6f) * energy.hatClosed;
        sb.hatClosed = buildRoleBlock(rng, kHatRhythm, stepsPerBar, hatClosedScale, syncopation,
                                       { { &kickBlock, kCrossRoleCorrelation.kickHat, 0 },
                                         { &clapBlock, kCrossRoleCorrelation.clapHat, 0 } });

        const float hatOpenScale = (0.7f + overallDensity * 0.6f) * energy.hatOpen;
        sb.hatOpen = buildRoleBlock(rng, kRideRhythm, stepsPerBar, hatOpenScale, syncopation,
                                    { { &kickBlock, kCrossRoleCorrelation.kickRide, 0 },
                                      { &clapBlock, kCrossRoleCorrelation.clapRide, 0 },
                                      { &sb.hatClosed, kCrossRoleCorrelation.hatRide, 0 } });

        // Calibrated so a SINGLE percussion voice's realized density
        // lands near the measured accent-style subset's mean (~4.78
        // hits/bar) rather than the raw corpus-wide average (~10.5,
        // which blends in continuous "*Perc Loop*"-named material -
        // see the PERC density milestone commit for the full
        // investigation). Both percA and percB share this same
        // calibrated baseline scale (percRoleBaseScale, defined below
        // - shared with deriveStage's own rebuild-fresh path so both
        // compute the identical baseline); `energy` then modulates
        // each independently per section.
        const float percAScale = (percRoleBaseScale() - 0.15f + overallDensity * 0.3f) * energy.percA;
        sb.percA = buildRoleBlock(rng, kPercRhythm, stepsPerBar, percAScale, syncopation,
                                   { { &kickBlock, kCrossRoleCorrelation.kickPerc, 0 },
                                     { &clapBlock, kCrossRoleCorrelation.clapPerc, 0 },
                                     { &sb.hatClosed, kCrossRoleCorrelation.hatPerc, 0 },
                                     { &sb.hatOpen, kCrossRoleCorrelation.percRide, 0 } });

        const float percBScale = (percRoleBaseScale() - 0.15f + overallDensity * 0.3f) * energy.percB;
        sb.percB = buildRoleBlock(rng, kPercRhythm, stepsPerBar, percBScale, syncopation,
                                   { { &kickBlock, kCrossRoleCorrelation.kickPerc, 0 },
                                     { &clapBlock, kCrossRoleCorrelation.clapPerc, 0 },
                                     { &sb.hatClosed, kCrossRoleCorrelation.hatPerc, 0 },
                                     { &sb.hatOpen, kCrossRoleCorrelation.percRide, 0 },
                                     { &sb.percA, kPercBAvoidsPercA, 0 } });
        return sb;
    }

    namespace
    {
        // deriveStageBlock's touch-count formula (energyDelta*6 + variation*2)
        // was designed for the SMALL deltas between adjacent stages of a
        // smoothly ramping arc (the old fixed 4-stage table's biggest
        // adjacent delta was ~0.45, starting from an EMPTY previous block,
        // where a handful of add-touches is exactly the right amount of
        // movement). A real arrangement can also produce a much bigger
        // delta in EITHER direction - most obviously Drop -> Breakdown
        // (percA/hatClosed/etc. need to go from a busy full-drop pattern
        // to near-silence in one step - a handful of remove-touches
        // can't get there, leaving the block still mostly full) and
        // symmetrically PreDrop -> Drop (jumping from PreDrop's sparse
        // level up to full-drop density - a handful of add-touches
        // UNDER-shoots the true target just as badly). Both directions
        // were caught the same way: inspecting a printed arrangement and
        // comparing measured density against the expected target, not
        // assumed. Beyond this threshold (either sign), rebuild fresh at
        // the new density instead of touch-deriving from the old one - a
        // genuine section change, not a subtle nudge.
        constexpr float kBigDeltaRebuildThreshold = 0.3f;

        StepArray deriveOrRebuildRoleBlock(std::mt19937& rng, const StepArray& previousBlock, const RoleRhythmStats& stats,
                                            int stepsPerBar, float energyDelta, float newAbsoluteScale,
                                            float syncopation, float variation, const std::vector<CorrelationRef>& refs)
        {
            if (std::abs(energyDelta) > kBigDeltaRebuildThreshold)
                return buildRoleBlock(rng, stats, stepsPerBar, newAbsoluteScale, syncopation, refs);
            return deriveStageBlock(rng, previousBlock, stats, stepsPerBar, energyDelta, variation, refs);
        }
    } // end anonymous namespace

    // Same coordination order as buildStageFresh (hatClosed ->
    // hatOpen -> percA -> percB, each referencing every role already
    // finalized for THIS stage) so deriveStageBlock's correlation-aware
    // touches have the right same-stage siblings to check against, not
    // last stage's (which could have quite different content once a
    // few sections' worth of touches have accumulated). Public - reused
    // by generateArrangementDrop below and by
    // Source/Engine/GrooveLoop.cpp (with previousEnergy==thisEnergy, i.e.
    // energyDelta==0 for every role, so only `variation` drives movement -
    // no energy arc at all).
    StageBlocks deriveStage(std::mt19937& rng, const StageBlocks& previous, const StageEnergy& previousEnergy,
                             const StageEnergy& thisEnergy, const StepArray& kickBlock, const StepArray& clapBlock,
                             int stepsPerBar, float overallDensity, float syncopation, float variation)
    {
        const float hatScaleBase = 0.7f + overallDensity * 0.6f;
        const float percScaleBase = percRoleBaseScale() - 0.15f + overallDensity * 0.3f;

        StageBlocks sb;
        sb.hatClosed = deriveOrRebuildRoleBlock(rng, previous.hatClosed, kHatRhythm, stepsPerBar,
                                         thisEnergy.hatClosed - previousEnergy.hatClosed,
                                         hatScaleBase * thisEnergy.hatClosed, syncopation, variation,
                                         { { &kickBlock, kCrossRoleCorrelation.kickHat, 0 },
                                           { &clapBlock, kCrossRoleCorrelation.clapHat, 0 } });
        sb.hatOpen   = deriveOrRebuildRoleBlock(rng, previous.hatOpen, kRideRhythm, stepsPerBar,
                                         thisEnergy.hatOpen - previousEnergy.hatOpen,
                                         hatScaleBase * thisEnergy.hatOpen, syncopation, variation,
                                         { { &kickBlock, kCrossRoleCorrelation.kickRide, 0 },
                                           { &clapBlock, kCrossRoleCorrelation.clapRide, 0 },
                                           { &sb.hatClosed, kCrossRoleCorrelation.hatRide, 0 } });
        sb.percA     = deriveOrRebuildRoleBlock(rng, previous.percA, kPercRhythm, stepsPerBar,
                                         thisEnergy.percA - previousEnergy.percA,
                                         percScaleBase * thisEnergy.percA, syncopation, variation,
                                         { { &kickBlock, kCrossRoleCorrelation.kickPerc, 0 },
                                           { &clapBlock, kCrossRoleCorrelation.clapPerc, 0 },
                                           { &sb.hatClosed, kCrossRoleCorrelation.hatPerc, 0 },
                                           { &sb.hatOpen, kCrossRoleCorrelation.percRide, 0 } });
        sb.percB     = deriveOrRebuildRoleBlock(rng, previous.percB, kPercRhythm, stepsPerBar,
                                         thisEnergy.percB - previousEnergy.percB,
                                         percScaleBase * thisEnergy.percB, syncopation, variation,
                                         { { &kickBlock, kCrossRoleCorrelation.kickPerc, 0 },
                                           { &clapBlock, kCrossRoleCorrelation.clapPerc, 0 },
                                           { &sb.hatClosed, kCrossRoleCorrelation.hatPerc, 0 },
                                           { &sb.hatOpen, kCrossRoleCorrelation.percRide, 0 },
                                           { &sb.percA, kPercBAvoidsPercA, 0 } });
        return sb;
    }

    namespace
    {
        // Bar 16: THINS the full-drop section's own bar-0 material rather
        // than adding anything - real density/velocity/omission-driven
        // tension ("the phrase is ending"), never a fill roll. hatClosed
        // keeps only its strong pulse (velocity above the midpoint between
        // the measured pulse and ghost levels); hatOpen/percA/percB keep
        // at most their single loudest hit each. A single accent (elevated
        // velocity, on hatOpen, gated by variation so variation=0 leaves
        // even that out) marks the transition itself.
        void buildTransitionBar(StepArray& hatClosedOut, StepArray& hatOpenOut, StepArray& percAOut, StepArray& percBOut,
                                 const StageBlocks& fullDrop, int stepsPerBar, float variation, std::mt19937& rng)
        {
            hatClosedOut.assign((size_t) stepsPerBar, Hit{});
            hatOpenOut.assign((size_t) stepsPerBar, Hit{});
            percAOut.assign((size_t) stepsPerBar, Hit{});
            percBOut.assign((size_t) stepsPerBar, Hit{});

            const float hatClosedKeepThreshold = 0.55f; // strictly between the measured ghost (~0.3-0.4) and pulse (~0.75-0.9) levels
            for (int s = 0; s < stepsPerBar; ++s)
            {
                const auto& h = fullDrop.hatClosed[(size_t) s];
                if (h.active && h.velocity >= hatClosedKeepThreshold)
                    setHit(hatClosedOut, s, h.velocity);
            }

            auto keepLoudestOnly = [stepsPerBar](StepArray& out, const StepArray& src)
            {
                int bestStep = -1;
                float bestVel = -1.0f;
                for (int s = 0; s < stepsPerBar; ++s)
                    if (src[(size_t) s].active && src[(size_t) s].velocity > bestVel)
                    {
                        bestVel = src[(size_t) s].velocity;
                        bestStep = s;
                    }
                if (bestStep >= 0)
                    setHit(out, bestStep, bestVel);
            };
            keepLoudestOnly(hatOpenOut, fullDrop.hatOpen);
            keepLoudestOnly(percAOut, fullDrop.percA);
            keepLoudestOnly(percBOut, fullDrop.percB);

            // Transition accent: a single elevated-velocity push near the
            // end of the bar, driving into the loop restart - gated by
            // variation (0 = the thinning above is the whole transition,
            // no extra accent) rather than always firing.
            if (variation > 0.0f && uniform01(rng) < variation)
            {
                const int accentStep = std::max(1, stepsPerBar / 8);
                if (!hatOpenOut[(size_t) accentStep].active)
                    setHit(hatOpenOut, accentStep, 0.9f);
            }
        }
    }

    DropPattern generateDrop(const StepGridConfig& grid, const DrumPatternParams& params, DrumSection section)
    {
        switch (section)
        {
            case DrumSection::Drop: break; // only section implemented so far
        }

        const int total        = totalSteps(grid);
        const int stepsPerBar  = grid.stepsPerBar;
        const int stepsPerBeat = std::max(1, stepsPerBar / 4);

        DropPattern out;
        out.kick      = StepArray((size_t) total);
        out.clap      = StepArray((size_t) total);
        out.hatClosed = StepArray((size_t) total);
        out.hatOpen   = StepArray((size_t) total);
        out.percA     = StepArray((size_t) total);
        out.percB     = StepArray((size_t) total);

        std::mt19937 rng = makeRng(params.seed);

        // ---- FOUNDATION: KICK - deterministic, no RNG (100% measured
        // on-beat placement). Velocity uses the MEASURED per-beat-position
        // relative velocity, layered with a phrase-downbeat accent (every
        // 4th bar sits marginally hotter). ----
        const auto& kickStats = rhythmStatsForRole(DrumRole::Kick);
        for (int bar = 0; bar < grid.numBars; ++bar)
        {
            const int base = bar * stepsPerBar;
            const bool isPhraseDownbeat = (bar % 4 == 0);
            for (int beat = 0; beat < 4; ++beat)
            {
                const int stepInBar = beat * stepsPerBeat;
                float vel = measuredVelocity(kickStats, stepInBar);
                if (isPhraseDownbeat)
                    vel = clamp01(vel + 0.05f);
                setHit(out.kick, base + stepInBar, vel);
            }
        }
        if (grid.numBars > 0 && params.variation > 0.0f)
        {
            const int lastBar = grid.numBars - 1;
            setHit(out.kick, lastBar * stepsPerBar + stepsPerBar - 2, 0.85f);
        }

        // Synthetic 4-bar kick block (every real 4-bar group in a 16-bar
        // grid starts on a phrase downbeat, so one static shape - built
        // once - is valid as a correlation reference for every section).
        StepArray kickBlock((size_t) (kBlockBars * stepsPerBar));
        for (int bar = 0; bar < kBlockBars; ++bar)
            for (int beat = 0; beat < 4; ++beat)
                setHit(kickBlock, bar * stepsPerBar + beat * stepsPerBeat, 1.0f);

        // ---- FOUNDATION: CLAP - built ONCE, stable through the whole
        // phrase (measured ~81% adjacent-bar repetition - real drops don't
        // meaningfully vary the backbeat bar to bar). ----
        const auto& clapStats = rhythmStatsForRole(DrumRole::Clap);
        const float clapDensityScale = 0.7f + params.density * 0.6f;
        const StepArray clapBlock = buildRoleBlock(rng, clapStats, stepsPerBar, clapDensityScale, params.syncopation,
                                                     { { &kickBlock, kCrossRoleCorrelation.kickClap, 0 } });
        copyBlock(out.clap, 0, 4, grid.numBars, stepsPerBar, clapBlock);
        copyBlock(out.clap, 4, 4, grid.numBars, stepsPerBar, clapBlock);
        copyBlock(out.clap, 8, 4, grid.numBars, stepsPerBar, clapBlock);
        copyBlock(out.clap, 12, 3, grid.numBars, stepsPerBar, clapBlock);
        if (grid.numBars > 0)
            copyBlock(out.clap, grid.numBars - 1, 1, grid.numBars, stepsPerBar, clapBlock);

        // ---- HIGH END + GROOVE: the 4-stage energy arc. ----
        const StageBlocks stage1 = buildStageFresh(rng, kickBlock, clapBlock, stepsPerBar, params.density, params.syncopation, kEstablish);
        const StageBlocks stage2 = deriveStage(rng, stage1, kEstablish, kDevelop, kickBlock, clapBlock, stepsPerBar, params.density, params.syncopation, params.variation);
        const StageBlocks stage3 = deriveStage(rng, stage2, kDevelop, kIncrease, kickBlock, clapBlock, stepsPerBar, params.density, params.syncopation, params.variation);
        const StageBlocks stage4 = deriveStage(rng, stage3, kIncrease, kFullDrop, kickBlock, clapBlock, stepsPerBar, params.density, params.syncopation, params.variation);

        copyBlock(out.hatClosed, 0, 4, grid.numBars, stepsPerBar, stage1.hatClosed);
        copyBlock(out.hatOpen,   0, 4, grid.numBars, stepsPerBar, stage1.hatOpen);
        copyBlock(out.percA,     0, 4, grid.numBars, stepsPerBar, stage1.percA);
        copyBlock(out.percB,     0, 4, grid.numBars, stepsPerBar, stage1.percB);

        copyBlock(out.hatClosed, 4, 4, grid.numBars, stepsPerBar, stage2.hatClosed);
        copyBlock(out.hatOpen,   4, 4, grid.numBars, stepsPerBar, stage2.hatOpen);
        copyBlock(out.percA,     4, 4, grid.numBars, stepsPerBar, stage2.percA);
        copyBlock(out.percB,     4, 4, grid.numBars, stepsPerBar, stage2.percB);

        copyBlock(out.hatClosed, 8, 4, grid.numBars, stepsPerBar, stage3.hatClosed);
        copyBlock(out.hatOpen,   8, 4, grid.numBars, stepsPerBar, stage3.hatOpen);
        copyBlock(out.percA,     8, 4, grid.numBars, stepsPerBar, stage3.percA);
        copyBlock(out.percB,     8, 4, grid.numBars, stepsPerBar, stage3.percB);

        copyBlock(out.hatClosed, 12, 3, grid.numBars, stepsPerBar, stage4.hatClosed);
        copyBlock(out.hatOpen,   12, 3, grid.numBars, stepsPerBar, stage4.hatOpen);
        copyBlock(out.percA,     12, 3, grid.numBars, stepsPerBar, stage4.percA);
        copyBlock(out.percB,     12, 3, grid.numBars, stepsPerBar, stage4.percB);

        // ---- TRANSITIONS: bar 16, thinned from the full-drop section. ----
        if (grid.numBars > 0)
        {
            StepArray hcFill, hoFill, paFill, pbFill;
            buildTransitionBar(hcFill, hoFill, paFill, pbFill, stage4, stepsPerBar, params.variation, rng);
            const int lastBar = grid.numBars - 1;
            copyBlock(out.hatClosed, lastBar, 1, grid.numBars, stepsPerBar, hcFill);
            copyBlock(out.hatOpen,   lastBar, 1, grid.numBars, stepsPerBar, hoFill);
            copyBlock(out.percA,     lastBar, 1, grid.numBars, stepsPerBar, paFill);
            copyBlock(out.percB,     lastBar, 1, grid.numBars, stepsPerBar, pbFill);
        }

        return out;
    }

    namespace
    {
        // Extracts exactly kBlockBars bars' worth of content starting at
        // `barStart` from an already-built full-arrangement StepArray, for
        // use as a correlation reference (buildRoleBlock/deriveStageBlock
        // always index a reference block's own bars 0..kBlockBars-1). Bars
        // past the end of `source` (a partial final block, if the
        // arrangement's total bar count isn't a multiple of kBlockBars)
        // are left silent rather than read out of bounds.
        StepArray sliceBlockReference(const StepArray& source, int barStart, int stepsPerBar, int totalBars)
        {
            StepArray block((size_t) (kBlockBars * stepsPerBar));
            for (int b = 0; b < kBlockBars; ++b)
            {
                const int srcBar = barStart + b;
                if (srcBar >= totalBars)
                    break;
                for (int s = 0; s < stepsPerBar; ++s)
                    block[(size_t) (b * stepsPerBar + s)] = source[(size_t) (srcBar * stepsPerBar + s)];
            }
            return block;
        }
    }

    DropPattern generateArrangementDrop(const MusicArrangement& arrangement, int stepsPerBar,
                                         const DrumPatternParams& params)
    {
        const int numBars = arrangement.totalBars();
        const int total = numBars * stepsPerBar;
        const int stepsPerBeat = std::max(1, stepsPerBar / 4);

        DropPattern out;
        out.kick      = StepArray((size_t) total);
        out.clap      = StepArray((size_t) total);
        out.hatClosed = StepArray((size_t) total);
        out.hatOpen   = StepArray((size_t) total);
        out.percA     = StepArray((size_t) total);
        out.percB     = StepArray((size_t) total);

        std::mt19937 rng = makeRng(params.seed);

        // ---- FOUNDATION: KICK - four-on-the-floor whenever the current
        // bar's section allows a kick at all (silent during Breakdown -
        // "remove the kick" - and on the single final bar of PreDrop, the
        // real "leave space before the drop" pause technique). Otherwise
        // unchanged from generateDrop's own kick logic - the brief
        // explicitly says the kick itself doesn't need redesigning, only
        // to become section-aware about WHEN it plays. isSectionDownbeat
        // uses each section's OWN first bar (barInSection==0), not the
        // arrangement's absolute bar count, so every section reads as
        // starting on a strong beat regardless of how many bars came
        // before it. ----
        const auto& kickStats = rhythmStatsForRole(DrumRole::Kick);
        for (int bar = 0; bar < numBars; ++bar)
        {
            const MusicState& state = musicStateForBar(arrangement, bar);
            const bool kickAllowed = state.section != MusicSection::Breakdown
                                   && !isPreDropFinalBar(arrangement, bar);
            if (!kickAllowed)
                continue;

            const int base = bar * stepsPerBar;
            const bool isSectionDownbeat = (state.barInSection % 4 == 0);
            for (int beat = 0; beat < 4; ++beat)
            {
                const int stepInBar = beat * stepsPerBeat;
                float vel = measuredVelocity(kickStats, stepInBar);
                if (isSectionDownbeat)
                    vel = clamp01(vel + 0.05f);
                setHit(out.kick, base + stepInBar, vel);
            }
        }
        // Same "push" accent generateDrop uses at its final bar, applied
        // here at the arrangement's own final bar (only if a kick is
        // actually allowed to sound there at all).
        if (numBars > 0 && params.variation > 0.0f)
        {
            const int lastBar = numBars - 1;
            if (musicStateForBar(arrangement, lastBar).section != MusicSection::Breakdown)
                setHit(out.kick, lastBar * stepsPerBar + stepsPerBar - 2, 0.85f);
        }

        // Synthetic always-four-on-the-floor reference block - used only
        // to build CLAP's own canonical shape (clap's character should
        // reflect the kick's TRUE pattern, independent of which specific
        // bars the kick or clap end up gated into - see below).
        StepArray kickRefBlock((size_t) (kBlockBars * stepsPerBar));
        for (int bar = 0; bar < kBlockBars; ++bar)
            for (int beat = 0; beat < 4; ++beat)
                setHit(kickRefBlock, bar * stepsPerBar + beat * stepsPerBeat, 1.0f);

        // ---- FOUNDATION: CLAP - ONE canonical shape, built once and
        // never independently varied section to section (the brief's own
        // "keep it sparse and supportive... don't let random clap
        // variations destroy the backbeat") - gated on/off per bar purely
        // by that bar's MusicState::drumEnergy, not regenerated. ----
        const auto& clapStats = rhythmStatsForRole(DrumRole::Clap);
        const float clapDensityScale = 0.7f + params.density * 0.6f;
        const StepArray clapBlock = buildRoleBlock(rng, clapStats, stepsPerBar, clapDensityScale, params.syncopation,
                                                     { { &kickRefBlock, kCrossRoleCorrelation.kickClap, 0 } });
        constexpr float kClapEnergyThreshold = 0.3f;
        for (int bar = 0; bar < numBars; ++bar)
            if (musicStateForBar(arrangement, bar).drumEnergy >= kClapEnergyThreshold)
                copyBlock(out.clap, bar, 1, numBars, stepsPerBar, clapBlock);

        // ---- HIGH END + GROOVE: the SAME motif-block-then-develop chain
        // generateDrop's 4-stage arc uses, generalized to run continuously
        // across every 4-bar block in the WHOLE arrangement instead of 4
        // fixed stages over 16 bars. Each block's per-role density scale
        // is derived from that block's own (averaged) MusicState::
        // drumEnergy, with the SAME relative shape the old fixed
        // kEstablish/kDevelop/kIncrease/kFullDrop table had (hatOpen and
        // percB scale with drumEnergy SQUARED, not linearly, so they stay
        // disproportionately quiet at low energy and only become a real
        // presence as drumEnergy approaches 1.0 - reproducing the old
        // table's ~7.5x hatClosed-vs-hatOpen establish-to-fullDrop ratio
        // without hardcoding 4 fixed levels).
        const int numBlocks = (numBars + kBlockBars - 1) / kBlockBars;

        StageBlocks previousBlocks;
        StageEnergy previousEnergy { 0.0f, 0.0f, 0.0f, 0.0f };

        for (int blockIdx = 0; blockIdx < numBlocks; ++blockIdx)
        {
            const int blockBarStart = blockIdx * kBlockBars;
            const int blockBarCount = std::min(kBlockBars, numBars - blockBarStart);

            float avgDrumEnergy = 0.0f;
            for (int b = 0; b < blockBarCount; ++b)
                avgDrumEnergy += musicStateForBar(arrangement, blockBarStart + b).drumEnergy;
            avgDrumEnergy /= (float) blockBarCount;

            const MusicSection blockSection = musicStateForBar(arrangement, blockBarStart).section;

            // hatOpen's own measured base rate (kRideRhythm.meanOnsetsPerBar
            // = 7.53) is actually HIGHER than hatClosed's groove-subset
            // rate (kHatRhythm = 6.86), so the same 0.75 ceiling the old
            // fixed-4-stage kFullDrop table used (which only ever reaches
            // its ceiling once, at the very end of a short 16-bar arc)
            // lets hatOpen's realized density catch up to and even exceed
            // hatClosed's over a long arrangement, once the energy-squared
            // suppression stops mattering at energy==1.0 - caught by
            // measuring section-mean density directly, not assumed. 0.45
            // keeps real, measured headroom below hatClosed even at full
            // energy, matching the brief's "should NOT become another
            // continuous hat layer".
            StageEnergy thisEnergy;
            thisEnergy.hatClosed = avgDrumEnergy * 1.05f;
            thisEnergy.hatOpen   = avgDrumEnergy * avgDrumEnergy * 0.45f;
            thisEnergy.percA     = avgDrumEnergy * 1.15f;
            thisEnergy.percB     = avgDrumEnergy * avgDrumEnergy * 1.05f;

            const StepArray kickRef = sliceBlockReference(out.kick, blockBarStart, stepsPerBar, numBars);
            const StepArray clapRef = sliceBlockReference(out.clap, blockBarStart, stepsPerBar, numBars);

            StageBlocks thisBlocks = (blockIdx == 0)
                ? buildStageFresh(rng, kickRef, clapRef, stepsPerBar, params.density, params.syncopation, thisEnergy)
                : deriveStage(rng, previousBlocks, previousEnergy, thisEnergy, kickRef, clapRef, stepsPerBar, params.density, params.syncopation, params.variation);

            // "Remove open hats" in a Breakdown is explicit and absolute
            // in the brief, unlike closed hat/percussion's "heavily
            // reduce" - so this is a real, disclosed hard rule on top of
            // the natural (already very low, since drumEnergy is
            // near-zero here) probabilistic thinning, not left to chance.
            if (blockSection == MusicSection::Breakdown)
                thisBlocks.hatOpen.assign(thisBlocks.hatOpen.size(), Hit{});

            copyBlock(out.hatClosed, blockBarStart, blockBarCount, numBars, stepsPerBar, thisBlocks.hatClosed);
            copyBlock(out.hatOpen,   blockBarStart, blockBarCount, numBars, stepsPerBar, thisBlocks.hatOpen);
            copyBlock(out.percA,     blockBarStart, blockBarCount, numBars, stepsPerBar, thisBlocks.percA);
            copyBlock(out.percB,     blockBarStart, blockBarCount, numBars, stepsPerBar, thisBlocks.percB);

            // TRANSITION: if the bar immediately after this block starts a
            // genuine "wind down" section (Breakdown or Outro - the only
            // two sections in the default cycle whose whole job is
            // reduction, not PreDrop/BreakdownBuild, which have their OWN
            // internal energy ramp and shouldn't ALSO be pre-emptively
            // thinned by the section before them - an earlier version of
            // this check compared raw energy deltas and incorrectly
            // thinned the end of Build just because PreDrop's own start is
            // quieter than Build's peak, undercutting Build's own "density
            // increases toward its end" property), thin THIS block's own
            // final bar using the same buildTransitionBar mechanism
            // generateDrop uses for its bar-16 transition - real density/
            // velocity/omission-driven tension signaling an ending, not a
            // fill roll. Also applies at the very end of the whole
            // arrangement (nothing follows).
            const int nextBar = blockBarStart + blockBarCount;
            // Must be a real ENTRY into the wind-down section (different
            // from this block's own section), not a block-to-block
            // boundary WITHIN Breakdown/Outro itself - otherwise
            // buildTransitionBar's own accent-hit logic (which adds a
            // hatOpen hit unconditionally on a variation roll, regardless
            // of whether the block it's "transitioning" was already
            // silenced) reintroduces exactly the content Breakdown's
            // force-mute above just removed. Caught the same way as the
            // other fixes here: inspecting actual generated output, not
            // assumed.
            const bool nextIsWindDown = nextBar < numBars
                && (musicStateForBar(arrangement, nextBar).section == MusicSection::Breakdown
                    || musicStateForBar(arrangement, nextBar).section == MusicSection::Outro)
                && musicStateForBar(arrangement, nextBar).section != blockSection;
            const bool preceedsQuieterSection = nextBar >= numBars || nextIsWindDown;
            if (preceedsQuieterSection && blockBarCount > 0)
            {
                StepArray hcFill, hoFill, paFill, pbFill;
                buildTransitionBar(hcFill, hoFill, paFill, pbFill, thisBlocks, stepsPerBar, params.variation, rng);
                const int transitionBar = blockBarStart + blockBarCount - 1;
                copyBlock(out.hatClosed, transitionBar, 1, numBars, stepsPerBar, hcFill);
                copyBlock(out.hatOpen,   transitionBar, 1, numBars, stepsPerBar, hoFill);
                copyBlock(out.percA,     transitionBar, 1, numBars, stepsPerBar, paFill);
                copyBlock(out.percB,     transitionBar, 1, numBars, stepsPerBar, pbFill);
            }

            previousBlocks = thisBlocks;
            previousEnergy = thisEnergy;
        }

        return out;
    }

    std::vector<int> toVelocityArray(const StepArray& steps)
    {
        std::vector<int> out(steps.size(), 0);
        for (size_t i = 0; i < steps.size(); ++i)
        {
            if (!steps[i].active)
                continue;

            int vel = (int) std::lround(steps[i].velocity * 127.0f);
            if (vel < 1)   vel = 1;
            if (vel > 127) vel = 127;
            out[i] = vel;
        }
        return out;
    }
}
