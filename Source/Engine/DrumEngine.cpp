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
        // kick, clap, hat, and perc are no longer generated independently
        // (see generateDrop below), so there is no longer a per-role salt.
        constexpr uint32_t kDropSalt = 0x44524F50u; // 'DROP'
        constexpr int      kBlockBars = 4;

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

        bool isBeatPosition(int stepInBar, int stepsPerBar)
        {
            const int stepsPerBeat = std::max(1, stepsPerBar / 4);
            return (stepInBar % stepsPerBeat) == 0;
        }

        // ------------------------------------------------------------------
        // The core data-driven mechanism this whole engine is built on:
        // convert a role's MEASURED per-position statistics
        // (DrumRhythmGrammar.h) into a per-position activation PROBABILITY,
        // not a guessed range.
        //
        // step16Probability[pos] is a normalized distribution (sums to 1.0
        // across all 16 positions - "of this role's onsets, what fraction
        // landed here"), so it isn't directly usable as a per-bar
        // activation chance. Multiplying by meanOnsetsPerBar converts it:
        // activation[pos] = step16Probability[pos] * meanOnsetsPerBar is
        // exactly the expected number of times position `pos` is hit per
        // bar, which for a single 16th-note slot is a real, derivable
        // activation probability (verified directly against the measured
        // KICK table: step16Probability[0]=0.25, meanOnsetsPerBar=4.0 ->
        // activation=1.0 - i.e. "always active", which is exactly what a
        // 100%-four-on-the-floor corpus should produce).
        float measuredActivation(const RoleRhythmStats& stats, int stepInBar)
        {
            return clamp01(stats.step16Probability[stepInBar] * stats.meanOnsetsPerBar);
        }

        // Any measured velocity of exactly 0 means the corpus never
        // recorded an onset there - if density/syncopation/correlation
        // still pushes this position active, it needs SOME audible
        // velocity rather than a silent (effectively vel=0) "hit", so it
        // falls back to a low, ghost-appropriate level instead of 0.
        float measuredVelocity(const RoleRhythmStats& stats, int stepInBar)
        {
            const float v = stats.step16RelativeVelocity[stepInBar];
            return v > 0.0f ? v : 0.15f;
        }

        // syncopation knob, layered ON TOP of the measured base rate (not
        // replacing it) - biases toward the weakest 16th in each beat,
        // same shape as the previous milestone's knob.
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
        // kick/clap/hat already sound" implemented directly from
        // DrumRhythmGrammar.h's measured CrossRoleCorrelation, not an
        // arbitrary avoidance rule.
        float correlationFactor(float corr, bool otherOccupied)
        {
            return otherOccupied ? std::max(0.0f, 1.0f + corr) : 1.0f;
        }

        bool occupiedAt(const StepArray& roleBlock, int bar, int stepInBar, int stepsPerBar)
        {
            const size_t idx = (size_t) (bar * stepsPerBar + stepInBar);
            return idx < roleBlock.size() && roleBlock[idx].active;
        }

        // One reference role (already built for this same relative bar)
        // and its measured correlation with the role currently being
        // built - buildCanonicalPosition below takes up to two of these
        // (hat references kick+clap; perc references kick+clap+hat).
        struct CorrelationRef
        {
            const StepArray* block;     // nullptr = no reference (kick uses a static predicate instead, see kickOccupied)
            float             corr;
            int               bar;      // which relative bar of *block to check
        };

        // Decides ONE position for ONE role at ONE relative bar: measured
        // base rate -> syncopation -> cross-role correlation against every
        // supplied reference -> a single weighted coin flip. This is the
        // one place density/syncopation/correlation/randomness actually
        // meet - everything above it is data, everything below it is
        // musical rules laid out this milestone's own header (motif
        // build-then-copy, phrase structure), not more probability logic.
        bool decidePosition(std::mt19937& rng, const RoleRhythmStats& stats, int stepInBar, int stepsPerBar,
                             float densityScale, float syncopation, bool kickOccupied, float kickCorr,
                             const CorrelationRef* ref1, const CorrelationRef* ref2, float& outVelocity)
        {
            float activation = measuredActivation(stats, stepInBar) * densityScale;
            activation = applySyncopation(activation, stepInBar, stepsPerBar, syncopation);
            activation *= correlationFactor(kickCorr, kickOccupied);
            if (ref1 != nullptr && ref1->block != nullptr)
                activation *= correlationFactor(ref1->corr, occupiedAt(*ref1->block, ref1->bar, stepInBar, stepsPerBar));
            if (ref2 != nullptr && ref2->block != nullptr)
                activation *= correlationFactor(ref2->corr, occupiedAt(*ref2->block, ref2->bar, stepInBar, stepsPerBar));
            activation = clamp01(activation);

            outVelocity = measuredVelocity(stats, stepInBar);
            return uniform01(rng) < activation;
        }

        // Builds one role's full kBlockBars-bar block. Bar 0 is the
        // canonical shape (fresh weighted decision per position, see
        // decidePosition). Bars 1-3 either literally repeat bar 0 (with
        // probability = the role's OWN measured adjacentBarsIdentical
        // fraction - a real corpus number, not a guess) or apply 1-2
        // small "touches" (repositioned/added-or-removed hits, re-decided
        // with the same formula) - controlled bar-to-bar movement inside
        // one 4-bar idea, matching what the corpus actually shows (these
        // roles are NOT 100% identical bar-to-bar, but they're not fully
        // independent either).
        StepArray buildRoleBlock(std::mt19937& rng, const RoleRhythmStats& stats, int stepsPerBar,
                                  float densityScale, float syncopation,
                                  const char* kickOccupied, float kickCorr,
                                  const CorrelationRef* ref1, const CorrelationRef* ref2)
        {
            StepArray block((size_t) (kBlockBars * stepsPerBar));

            for (int s = 0; s < stepsPerBar; ++s)
            {
                float vel = 0.0f;
                CorrelationRef r1 = ref1 != nullptr ? CorrelationRef{ ref1->block, ref1->corr, 0 } : CorrelationRef{ nullptr, 0.0f, 0 };
                CorrelationRef r2 = ref2 != nullptr ? CorrelationRef{ ref2->block, ref2->corr, 0 } : CorrelationRef{ nullptr, 0.0f, 0 };
                if (decidePosition(rng, stats, s, stepsPerBar, densityScale, syncopation,
                                    kickOccupied != nullptr && kickOccupied[s], kickCorr,
                                    ref1 != nullptr ? &r1 : nullptr, ref2 != nullptr ? &r2 : nullptr, vel))
                    setHit(block, s, vel);
            }

            const float identicalFraction = stats.adjacentBarsIdenticalFraction >= 0.0f
                                                 ? stats.adjacentBarsIdenticalFraction
                                                 : 0.7f; // unmeasured (e.g. mostly-1-bar corpus) - a reasonable, documented default, not a real measurement

            for (int bar = 1; bar < kBlockBars; ++bar)
            {
                const int base = bar * stepsPerBar;
                for (int s = 0; s < stepsPerBar; ++s)
                    block[(size_t) (base + s)] = block[(size_t) s]; // start from bar 0's canonical shape

                if (uniform01(rng) < identicalFraction)
                    continue; // literal repeat of bar 0 - matches the corpus's own measured repetition rate

                // A "touch" TOGGLES a position (add if currently silent,
                // remove if currently active) rather than re-running the
                // full weighted decision - re-deciding would often just
                // re-confirm the same (usually low-probability, usually
                // silent) outcome and produce an invisible "variation"
                // that never actually changes the pattern. A guaranteed
                // toggle is what makes this bar-to-bar movement audible,
                // while still only touching 1-2 positions (never a full
                // re-roll) and still using the role's own measured
                // velocity when adding a hit.
                const int touches = 1 + (uniform01(rng) < 0.5f ? 0 : 1); // 1 or 2 positions touched
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

        // Copies one full kBlockBars-bar block literally into `barCount`
        // consecutive destination bars (wrapping through the block's own
        // bars if barCount exceeds kBlockBars, e.g. the 3-bar
        // maintain-section copy) - this is what makes a repeat group
        // byte-identical to its source block, rather than each
        // destination bar re-deriving its own version.
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

        // The developed block starts as a copy of the establish block's
        // corresponding role, then applies a handful of TOGGLES (add a
        // hit where it was silent, using the role's own measured
        // velocity; remove one where it was active) so bars 9-12 are
        // recognizably the SAME idea as 1-4 with real, controlled,
        // AUDIBLE movement, not a fresh independent pattern. The touch
        // count scales continuously with params.variation (1 at the
        // lowest non-zero setting, up to 4 at variation=1) rather than a
        // single coin-flip gate that could skip development for an
        // entire role - variation=0 is the only setting where the block
        // is left completely unchanged.
        StepArray developRoleBlock(std::mt19937& rng, const StepArray& establishBlock, const RoleRhythmStats& stats,
                                    int stepsPerBar, float variation)
        {
            StepArray block = establishBlock;
            if (variation <= 0.0f)
                return block;

            const int touches = std::max(1, (int) std::lround(variation * 4.0f)); // 1-4 touches, scaled by how much development is asked for
            for (int t = 0; t < touches; ++t)
            {
                const int bar = (int) (uniform01(rng) * (float) kBlockBars);
                const int s   = (int) (uniform01(rng) * (float) stepsPerBar);
                const size_t idx = (size_t) (bar * stepsPerBar + s);
                if (block[idx].active)
                    block[idx] = Hit{};
                else
                    setHit(block, bar * stepsPerBar + s, measuredVelocity(stats, s));
            }
            return block;
        }
    }

    // ----------------------------------------------------------------------
    // generateDrop - the coordinated 16-bar Melodic Techno drop.
    //
    // Sourced rhythmic principles this engine encodes as MEASURED data
    // (DrumRhythmGrammar.h), not hand-picked genre lore - see that file's
    // header for the exact corpus:
    //   - kick: 100% of measured onsets land exactly on the four beats
    //     (uncompromising four-on-the-floor) - built directly from that
    //     number, not assumed.
    //   - clap: measured ~84% on-beat, overwhelmingly concentrated at
    //     beats 2 and 4 (the backbeat), ~81% adjacent-bar repetition.
    //   - hat: measured offbeat-8th positions (steps 2/6/10/14) carry
    //     roughly double the velocity of the surrounding 16th activity -
    //     the real "strong pulse, quieter filler" hierarchy, applied
    //     directly from the measured per-position velocity table.
    //   - percussion: built from its OWN measured position distribution,
    //     THEN reweighted by the real measured correlation with kick
    //     (-0.45), clap (-0.43, both "percussion tends to avoid these
    //     roles' positions") and hat (+0.48, "percussion tends to share
    //     hat's busier positions") - genuine cross-role coordination, not
    //     independent placement.
    //
    // Coordination order (matches the brief's own diagram): kick is built
    // first and is the one role every other role can reference; clap is
    // built next (referencing kick); hat next (referencing kick + clap);
    // percussion last (referencing kick + clap + hat). One shared RNG
    // stream for the whole composition - there is no per-role seed
    // anymore, because generating each role from an independent stream is
    // exactly what made them independent instead of coordinated.
    //
    // Phrase structure (unchanged shape from the previous milestone, now
    // filled with measured-data-driven content): bars 1-4 establish the
    // block, 5-8 repeat it literally (byte-identical - see copyBlock),
    // 9-12 develop it (developRoleBlock, gated by params.variation),
    // 13-15 repeat the developed block, bar 16 is a restrained
    // phrase-ending fill built from the establish block plus a single
    // variation-gated accent - never a busy roll.
    DropPattern generateDrop(const StepGridConfig& grid, const DrumPatternParams& params, DrumSection section)
    {
        switch (section)
        {
            case DrumSection::Drop: break; // only section implemented so far
        }

        const int total       = totalSteps(grid);
        const int stepsPerBar = grid.stepsPerBar;
        const int stepsPerBeat = std::max(1, stepsPerBar / 4);

        DropPattern out;
        out.kick = StepArray((size_t) total);
        out.clap = StepArray((size_t) total);
        out.hat  = StepArray((size_t) total);
        out.perc = StepArray((size_t) total);

        std::mt19937 rng = makeRng(params.seed);

        // ---- KICK: deterministic, no RNG - the measured corpus showed
        // 100% on-beat placement, so there is no probability to roll.
        // Velocity uses the MEASURED per-beat-position relative velocity
        // (the corpus shows beat 1 hitting marginally harder than beats
        // 2-4), layered with a phrase-downbeat accent (every 4th bar
        // slightly hotter) - a real structural device, not contradicted
        // by the single/double-bar corpus data, which can't itself speak
        // to 16-bar phrase position. ----
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
        // Bar 16 restrained phrase-ending push, gated by variation (0 =
        // kick stays perfectly steady through the last bar too).
        if (grid.numBars > 0 && params.variation > 0.0f)
        {
            const int lastBar = grid.numBars - 1;
            setHit(out.kick, lastBar * stepsPerBar + stepsPerBar - 2, 0.85f);
        }

        // Static per-position predicate (kick's shape never varies bar to
        // bar) - every other role's correlation lookup against kick uses
        // this instead of a per-block StepArray.
        std::vector<char> kickOccupied((size_t) stepsPerBar, 0); // char, not vector<bool> - needs a real .data() pointer below
        for (int s = 0; s < stepsPerBar; ++s)
            kickOccupied[(size_t) s] = isBeatPosition(s, stepsPerBar) ? 1 : 0;

        const auto& clapStats = rhythmStatsForRole(DrumRole::Clap);
        const auto& hatStats  = rhythmStatsForRole(DrumRole::Hat);
        const auto& percStats = rhythmStatsForRole(DrumRole::Perc);

        // density knob: 0.5 tracks the measured corpus average for each
        // role (scale = 1.0); hat and clap track their own measured
        // density fairly directly (comparable, single-role corpus
        // material). Percussion's corpus is full multi-instrument
        // percussion LOOPS (shakers/congas/rolls, not one hand-placed
        // accent role in a 4-role mix) - its raw measured density
        // (~10.5 hits/bar) doesn't transfer 1:1 to an accent role
        // alongside three already-active roles, so it's deliberately
        // scaled down to a restrained accent budget while still using
        // the corpus's real POSITION SHAPE and cross-role correlation
        // faithfully - a documented interpretive choice, not a silent
        // substitution.
        const float clapDensityScale = 0.7f + params.density * 0.6f;
        const float hatDensityScale  = 0.7f + params.density * 0.6f;
        const float percDensityScale = (0.12f + params.density * 0.28f);

        auto buildEstablish = [&](std::mt19937& localRng, StepArray& clapBlock, StepArray& hatBlock, StepArray& percBlock)
        {
            clapBlock = buildRoleBlock(localRng, clapStats, stepsPerBar, clapDensityScale, params.syncopation,
                                        kickOccupied.data(), kCrossRoleCorrelation.kickClap, nullptr, nullptr);

            CorrelationRef hatRefClap{ &clapBlock, kCrossRoleCorrelation.clapHat, 0 };
            hatBlock = buildRoleBlock(localRng, hatStats, stepsPerBar, hatDensityScale, params.syncopation,
                                       kickOccupied.data(), kCrossRoleCorrelation.kickHat, &hatRefClap, nullptr);

            CorrelationRef percRefClap{ &clapBlock, kCrossRoleCorrelation.clapPerc, 0 };
            CorrelationRef percRefHat{ &hatBlock, kCrossRoleCorrelation.hatPerc, 0 };
            percBlock = buildRoleBlock(localRng, percStats, stepsPerBar, percDensityScale, params.syncopation,
                                        kickOccupied.data(), kCrossRoleCorrelation.kickPerc, &percRefClap, &percRefHat);
        };

        StepArray establishClap, establishHat, establishPerc;
        buildEstablish(rng, establishClap, establishHat, establishPerc);
        copyBlock(out.clap, 0, 4, grid.numBars, stepsPerBar, establishClap);
        copyBlock(out.clap, 4, 4, grid.numBars, stepsPerBar, establishClap);
        copyBlock(out.hat, 0, 4, grid.numBars, stepsPerBar, establishHat);
        copyBlock(out.hat, 4, 4, grid.numBars, stepsPerBar, establishHat);
        copyBlock(out.perc, 0, 4, grid.numBars, stepsPerBar, establishPerc);
        copyBlock(out.perc, 4, 4, grid.numBars, stepsPerBar, establishPerc);

        // Develop block: each role's establish shape, touched (see
        // developRoleBlock) - order doesn't matter for correlation here
        // since touches don't re-run the cross-role activation formula,
        // just toggle a position and use that role's own measured
        // velocity, so clap/hat/perc can develop in any order.
        const auto developClap = developRoleBlock(rng, establishClap, clapStats, stepsPerBar, params.variation);
        const auto developHat  = developRoleBlock(rng, establishHat,  hatStats,  stepsPerBar, params.variation);
        const auto developPerc = developRoleBlock(rng, establishPerc, percStats, stepsPerBar, params.variation);

        copyBlock(out.clap, 8, 4, grid.numBars, stepsPerBar, developClap);
        copyBlock(out.clap, 12, 3, grid.numBars, stepsPerBar, developClap);
        copyBlock(out.hat, 8, 4, grid.numBars, stepsPerBar, developHat);
        copyBlock(out.hat, 12, 3, grid.numBars, stepsPerBar, developHat);
        copyBlock(out.perc, 8, 4, grid.numBars, stepsPerBar, developPerc);
        copyBlock(out.perc, 12, 3, grid.numBars, stepsPerBar, developPerc);

        // Bar 16: restrained phrase-ending fill - the establish block's
        // own bar 0 for every role, plus at most one small variation-
        // gated accent (never a busy roll, per the brief's explicit
        // caution against "fill spam").
        if (grid.numBars > 0)
        {
            const int lastBar = grid.numBars - 1;
            copyBlock(out.clap, lastBar, 1, grid.numBars, stepsPerBar, establishClap);
            copyBlock(out.hat, lastBar, 1, grid.numBars, stepsPerBar, establishHat);
            copyBlock(out.perc, lastBar, 1, grid.numBars, stepsPerBar, establishPerc);

            if (params.variation > 0.0f && uniform01(rng) < params.variation)
            {
                const int base = lastBar * stepsPerBar;
                const int accentStep = base + std::max(1, stepsPerBar / 8);
                if (!out.hat[(size_t) accentStep].active)
                    setHit(out.hat, accentStep, 0.6f);
            }
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
