#include "BreakdownArrangement.h"
#include "BassRhythmGrammar.h" // kBassPitchOffsets - the best available REAL evidence for
                                // "what non-root interval actually occurs in this corpus";
                                // measured for rhythmic bass, not pad harmony (no pad-harmony
                                // corpus exists - sound design tier was LOW-confidence/no local
                                // data), reused and disclosed as such rather than inventing an
                                // un-evidenced "root and fifth" voicing.
#include <random>

namespace Engine
{
    namespace
    {
        constexpr uint32_t kPadSalt = 0x50414400u; // 'PAD\0' - own RNG stream, same salting
                                                     // convention as DrumEngine.cpp's 'DROP',
                                                     // BassEngine.cpp's 'BASS', BassArchetype.cpp's
                                                     // 'ARCH' - independent-but-deterministic from
                                                     // the SAME shared MusicIdentity::seed.
        std::mt19937 makeRng(uint32_t seed) { return std::mt19937(seed ^ kPadSalt); }

        // Weighted pick from the real measured pitch-offset table
        // (BassRhythmGrammar.h). preferWide biases the draw toward the
        // table's higher-magnitude entries (a disclosed interpretation for
        // PreDrop's "reaching toward the drop" character) while never
        // selecting an interval the corpus didn't actually measure - no
        // octave jump is introduced, matching the measured max magnitude
        // of 7 semitones.
        int8_t pickPitchOffset(std::mt19937& rng, bool preferWide)
        {
            float weights[kNumBassPitchOffsets];
            float total = 0.0f;
            for (int i = 0; i < kNumBassPitchOffsets; ++i)
            {
                const auto& entry = kBassPitchOffsets[i];
                float w = entry.probability;
                if (preferWide)
                {
                    const float magnitude = (float) (entry.offsetSemitones < 0 ? -entry.offsetSemitones : entry.offsetSemitones);
                    w *= (1.0f + magnitude * 0.6f); // real intervals only, just reweighted
                }
                weights[i] = w;
                total += w;
            }
            std::uniform_real_distribution<float> dist(0.0f, total);
            float r = dist(rng);
            for (int i = 0; i < kNumBassPitchOffsets; ++i)
            {
                r -= weights[i];
                if (r <= 0.0f)
                    return (int8_t) kBassPitchOffsets[i].offsetSemitones;
            }
            return 0;
        }

        // Entry/Body: sparse, sustained material - one onset per 2-bar
        // block (jittered position near the block start), long gate
        // length, mostly-root pitch. This is the "dedicated melodic/
        // harmonic material" the breakdown introduces - HIGH-confidence
        // per melodic_techno_research.md section 11.4 (a real, dedicated
        // pad/string/choir element was found gated specifically to the
        // break span in 3 of 3 measured reference arrangements).
        //
        // For very short phases (e.g. a 1-bar compact-loop Entry - see
        // generateCompactLoop) the "one onset per 2-bar block" placement
        // would round down to zero onsets, silently producing an empty
        // phase - guard against that by always placing at least one onset
        // in bar 0 regardless of block size.
        PadMotif buildSustainedPad(std::mt19937& rng, int bars, int minGate, int maxGate)
        {
            PadMotif out;
            const int totalSteps = bars * kBreakdownStepsPerBar;
            out.pitchOffsets.assign((size_t) totalSteps, kPadOffValue);
            out.gateLengthSteps.assign((size_t) totalSteps, 0);

            std::uniform_int_distribution<int> jitterDist(0, 3);
            std::uniform_int_distribution<int> gateDist(minGate, maxGate);

            const int blockSteps = 2 * kBreakdownStepsPerBar; // one onset per 2-bar block
            bool placedAny = false;
            for (int blockStart = 0; blockStart < totalSteps; blockStart += blockSteps)
            {
                const int step = blockStart + jitterDist(rng);
                if (step >= totalSteps)
                    continue;
                out.pitchOffsets[(size_t) step] = pickPitchOffset(rng, /*preferWide*/ false);
                out.gateLengthSteps[(size_t) step] = (int8_t) gateDist(rng);
                placedAny = true;
            }
            if (!placedAny && totalSteps > 0)
            {
                // Short-phase guard (see comment above): still commit one
                // real, sustained note rather than leaving the phase silent.
                const int rawStep = jitterDist(rng);
                const int step = rawStep < totalSteps - 1 ? rawStep : totalSteps - 1;
                out.pitchOffsets[(size_t) step] = pickPitchOffset(rng, false);
                out.gateLengthSteps[(size_t) step] = (int8_t) gateDist(rng);
            }
            return out;
        }

        // PreDrop: onset density rises and gate length shortens across the
        // phase's own bars - a disclosed pattern-level analogue of the
        // HIGH-confidence "riser is the reliable pre-drop signal" finding
        // (real audio risers/filter automation aren't buildable without a
        // DSP-automation feature this engine doesn't have - see the plan).
        // Pitch selection also biases toward the wider measured intervals
        // as the phase progresses, for a "reaching toward the drop"
        // quality without introducing any unmeasured interval.
        PadMotif buildPreDropPad(std::mt19937& rng, int bars)
        {
            PadMotif out;
            const int totalSteps = bars * kBreakdownStepsPerBar;
            out.pitchOffsets.assign((size_t) totalSteps, kPadOffValue);
            out.gateLengthSteps.assign((size_t) totalSteps, 0);

            std::uniform_real_distribution<float> onsetRoll(0.0f, 1.0f);
            int lastOnsetStep = -100;
            const int minGapSteps = 2; // avoid clashing back-to-back onsets

            for (int step = 0; step < totalSteps; ++step)
            {
                const int barIndex = step / kBreakdownStepsPerBar;
                const float progress = bars > 1 ? (float) barIndex / (float) (bars - 1) : 0.0f;
                const float onsetProbability = 0.05f + progress * (0.35f - 0.05f); // rises toward the drop

                if (step - lastOnsetStep < minGapSteps)
                    continue;
                if (onsetRoll(rng) >= onsetProbability)
                    continue;

                lastOnsetStep = step;
                out.pitchOffsets[(size_t) step] = pickPitchOffset(rng, /*preferWide*/ progress > 0.5f);

                const int gateHigh = 16, gateLow = 3;
                const int gate = gateHigh - (int) ((gateHigh - gateLow) * progress); // shortens toward the drop
                out.gateLengthSteps[(size_t) step] = (int8_t) gate;
            }
            return out;
        }

        RoleMix mutedMix()  { return RoleMix { true,  1.0f }; }
        RoleMix unmutedMix(){ return RoleMix { false, 1.0f }; }
    }

    BreakdownPhase phaseForBar(int barIndexWithinBreakdown)
    {
        if (barIndexWithinBreakdown < kBreakdownEntryBars)
            return BreakdownPhase::Entry;
        if (barIndexWithinBreakdown < kBreakdownEntryBars + kBreakdownBodyBars)
            return BreakdownPhase::Body;
        return BreakdownPhase::PreDrop;
    }

    int totalBreakdownBars()
    {
        return kBreakdownEntryBars + kBreakdownBodyBars + kPreDropBars;
    }

    int barsInPhase(BreakdownPhase phase)
    {
        switch (phase)
        {
            case BreakdownPhase::Entry:   return kBreakdownEntryBars;
            case BreakdownPhase::Body:    return kBreakdownBodyBars;
            case BreakdownPhase::PreDrop: return kPreDropBars;
        }
        return 0;
    }

    PadMotif generateBreakdownPad(uint32_t seed, BreakdownPhase phase, int barsOverride)
    {
        std::mt19937 rng = makeRng(seed);
        const int bars = barsOverride;

        switch (phase)
        {
            case BreakdownPhase::Entry:
                // Fading in: shorter holds than Body, still clearly sustained.
                return buildSustainedPad(rng, bars, /*minGate*/ 12, /*maxGate*/ 20);
            case BreakdownPhase::Body:
                // Fully exposed - the longest, most sustained holds of the
                // whole breakdown (this phase's defining content).
                return buildSustainedPad(rng, bars, /*minGate*/ 20, /*maxGate*/ 30);
            case BreakdownPhase::PreDrop:
                return buildPreDropPad(rng, bars);
        }
        return {};
    }

    PadMotif generateBreakdownPad(uint32_t seed, BreakdownPhase phase)
    {
        return generateBreakdownPad(seed, phase, barsInPhase(phase));
    }

    BreakdownRenderedLoop renderBreakdownPhase(const MusicIdentity& identity, BreakdownPhase phase, int barsOverride)
    {
        BreakdownRenderedLoop out;

        // Kick and core/sub bass are HIGH-confidence absent for the whole
        // breakdown span (melodic_techno_research.md section 11.4: kick
        // OFF in 3/3 measured instances, core/sub bass OFF in the 2/2
        // unambiguous instances, in every phase up to the Drop itself).
        // The driving hat/percussion groove is MEDIUM-confidence absent
        // too - the corrected finding (section 11.3) is that it more
        // often reintroduces gradually INSIDE the drop rather than before
        // it, so keeping it muted through Entry/Body/PreDrop is the
        // simpler, safer option that never contradicts any measured
        // instance (see the plan's evidence-tier discipline section).
        // Clap is left unmuted, preserving the SAME judgment call the
        // existing RenderMode::Breakdown already made (MusicIdentity.cpp)
        // rather than introducing a new, unevidenced rule for it.
        out.kick      = mutedMix();
        out.hatClosed = mutedMix();
        out.hatOpen   = mutedMix();
        out.percA     = mutedMix();
        out.percB     = mutedMix();
        out.clap      = unmutedMix();
        out.bassMuted = true;

        out.pad = generateBreakdownPad(identity.seed, phase, barsOverride);

        return out;
    }

    BreakdownRenderedLoop renderBreakdownPhase(const MusicIdentity& identity, BreakdownPhase phase)
    {
        return renderBreakdownPhase(identity, phase, barsInPhase(phase));
    }

    CompactSection compactSectionForBar(int bar)
    {
        const int wrapped = ((bar % kCompactLoopBars) + kCompactLoopBars) % kCompactLoopBars;
        if (wrapped < kCompactDropBars)
            return CompactSection::Drop;
        if (wrapped < kCompactDropBars + kCompactEntryBars)
            return CompactSection::BreakEntry;
        if (wrapped < kCompactDropBars + kCompactEntryBars + kCompactBodyBars)
            return CompactSection::BreakBody;
        return CompactSection::PreDrop;
    }

    namespace
    {
        // Zeroes a role's velocity for every step in [fromStep, toStep) -
        // the actual "muted during the breakdown span" mechanism for the
        // compact loop: not a mute/gain overlay applied uniformly for the
        // whole loop (that's what setGeneratedDrumRoleMuted/Gain already
        // does for manual per-role mute, and stays untouched), but a
        // literal zero written into a specific bar range of an otherwise
        // real, already-generated Drop pattern.
        void zeroRange(StepArray& role, int fromStep, int toStep)
        {
            for (int s = fromStep; s < toStep && s < (int) role.size(); ++s)
                role[(size_t) s] = Hit {};
        }

        // Appends src's steps into dst starting at dstOffset - used to
        // concatenate the three compact-loop breakdown phases (Entry+Body+
        // PreDrop) into one contiguous pad array occupying bars 8-15.
        void appendAt(std::vector<int8_t>& dst, int dstOffset, const std::vector<int8_t>& src)
        {
            for (size_t i = 0; i < src.size(); ++i)
                dst[(size_t) dstOffset + i] = src[i];
        }

        // Fix for the diagnosed stage-arc-truncation bug (see
        // MLPipeline/musical_target/sound_and_rhythm_diagnostic_pass4.md
        // section 2): generateDrop()'s own 4-stage energy arc (Establish
        // bars0-3, Develop bars4-7, Increase bars8-11, FullDrop
        // bars12-14+transition15 - see DrumEngine.cpp lines ~297-301,
        // 627-661) spans its own full 16 bars, but this compact loop's
        // "Drop" span is only ever 8 bars (kCompactDropBars) - bars 8-15
        // are the breakdown span (see generateCompactLoop below) and must
        // stay untouched. A flat copy of generateDrop()'s own bars 0-7
        // therefore only ever exposes Establish+Develop - Increase and
        // FullDrop are generated (real work) and then silently discarded
        // every loop repeat, confirmed empirically across 3 seeds in the
        // diagnostic doc above (closed-hat density ~42% of the measured
        // corpus, percussion ~23%).
        //
        // Fix is compositional, not a rhythm change: pick the FIRST 2 bars
        // of each of generateDrop()'s own 4 stage windows (never the
        // deliberately-thinned transition bar, bar 15) and concatenate
        // them into the compact loop's 8-bar Drop span, 2 bars per stage -
        // Establish->bars0-1, Develop->bars4-5, Increase->bars8-9,
        // FullDrop->bars12-13 of the ORIGINAL 16-bar generateDrop() output,
        // landing at bars0-1/2-3/4-5/6-7 of the compact loop. Every sampled
        // bar is real, already-generated, per-role-grammar-correct content
        // - no new probability, density, or timing decision is made here,
        // and DrumEngine.cpp/kHatRhythm/kPercRhythm/kClapRhythm are never
        // touched.
        //
        // Kick and clap are DELIBERATELY EXCLUDED from this remapping (an
        // earlier version of this fix included them and broke a real test:
        // kick's downbeat velocity is slightly higher in bar 0 than later
        // bars - 0.99 vs 0.85/0.86, see drum_grammar report.md - and
        // clap's own 4-bar block is not internally uniform bar-to-bar
        // within itself, only PERIODIC at every 4 bars, so re-selecting
        // which bar of that block lands where measurably changes clap's
        // played sequence, not just which bar-copy of an identical pattern
        // is shown). The diagnostic already found kick/clap density and
        // position both already match the reference corpus - remapping
        // them risked a real, audible regression for zero benefit, so they
        // keep their existing, untouched bars 0-7 exactly as before this
        // fix (see generateCompactLoop below - out.drum = identity.drumMotif
        // already gives them that, and this function never overwrites them).
        constexpr int kCompressedSourceBar[kCompactDropBars] = { 0, 1, 4, 5, 8, 9, 12, 13 };

        void copyBar(StepArray& dst, const StepArray& src, int dstBar, int srcBar, int stepsPerBar)
        {
            for (int s = 0; s < stepsPerBar; ++s)
            {
                const int dstIdx = dstBar * stepsPerBar + s;
                const int srcIdx = srcBar * stepsPerBar + s;
                if (dstIdx < (int) dst.size() && srcIdx < (int) src.size())
                    dst[(size_t) dstIdx] = src[(size_t) srcIdx];
            }
        }

        // Overwrites dst's hatClosed/hatOpen/percA/percB bars
        // [0, kCompactDropBars) in place with the compressed 4-stage arc
        // sampled from src (generateDrop()'s own full 16-bar output) - src
        // itself is never modified, dst's kick/clap are never touched (see
        // the comment above), and dst's own bars from kCompactDropBars
        // onward (the breakdown span) are never touched by this function
        // at all.
        void compressDropArcInto(DropPattern& dst, const DropPattern& src, int stepsPerBar)
        {
            for (int destBar = 0; destBar < kCompactDropBars; ++destBar)
            {
                const int srcBar = kCompressedSourceBar[destBar];
                copyBar(dst.hatClosed, src.hatClosed, destBar, srcBar, stepsPerBar);
                copyBar(dst.hatOpen,   src.hatOpen,   destBar, srcBar, stepsPerBar);
                copyBar(dst.percA,     src.percA,     destBar, srcBar, stepsPerBar);
                copyBar(dst.percB,     src.percB,     destBar, srcBar, stepsPerBar);
            }
        }
    }

    CompactLoop generateCompactLoop(const MusicIdentity& identity)
    {
        constexpr int kTotalSteps      = kCompactLoopBars * kBreakdownStepsPerBar; // 256
        constexpr int kBreakdownStartStep = kCompactDropBars * kBreakdownStepsPerBar; // 128

        CompactLoop out;

        // Bars 0-7 start as the existing, untouched Drop pattern's own
        // bars 0-7 (still byte-identical to identity.drumMotif itself -
        // this copy never mutates identity.drumMotif/bassMotif), then are
        // OVERWRITTEN below by compressDropArcInto with the compressed
        // 4-stage arc (see that function's own comment) - bars 8-15 are
        // never touched by that overwrite, so the breakdown span's own
        // source data is exactly what it was before this fix.
        out.drum = identity.drumMotif;
        out.bass = identity.bassMotif;
        out.bassGateLengthSteps = identity.bassGateLengthSteps;
        if ((int) out.bass.size() < kTotalSteps)
            out.bass.resize((size_t) kTotalSteps, kBassOffValue);
        if ((int) out.bassGateLengthSteps.size() < kTotalSteps)
            out.bassGateLengthSteps.resize((size_t) kTotalSteps, 0);

        // Fix for the stage-arc-truncation bug: compress generateDrop()'s
        // own Establish/Develop/Increase/FullDrop arc into the 8 bars that
        // are actually audible as "Drop" (see compressDropArcInto's own
        // comment above) - the played loop now reaches full-drop energy
        // by bar 8 instead of only ever hearing Establish+Develop. Applied
        // strictly to bars [0, kCompactDropBars) - bars 8-15 (assigned
        // just above from identity.drumMotif) are untouched by this call.
        compressDropArcInto(out.drum, identity.drumMotif, kBreakdownStepsPerBar);

        // Bars 8-15: zero the muted roles (kick/hatClosed/hatOpen/percA/
        // percB) - clap is deliberately left as whatever generateDrop()
        // already produced there, matching the existing Breakdown
        // judgment call (see renderBreakdownPhase's own comment above).
        // Bass is fully off for the whole breakdown span (HIGH-confidence,
        // matches renderBreakdownPhase's bassMuted=true for all 3 phases).
        zeroRange(out.drum.kick,      kBreakdownStartStep, kTotalSteps);
        zeroRange(out.drum.hatClosed, kBreakdownStartStep, kTotalSteps);
        zeroRange(out.drum.hatOpen,   kBreakdownStartStep, kTotalSteps);
        zeroRange(out.drum.percA,     kBreakdownStartStep, kTotalSteps);
        zeroRange(out.drum.percB,     kBreakdownStartStep, kTotalSteps);
        for (int s = kBreakdownStartStep; s < kTotalSteps; ++s)
        {
            out.bass[(size_t) s] = kBassOffValue;
            out.bassGateLengthSteps[(size_t) s] = 0;
        }

        // Pad: off for bars 0-7, then Entry(1 bar)+Body(3 bars)+
        // PreDrop(4 bars) concatenated for bars 8-15 - each generated at
        // ITS compact-loop bar count via the barsOverride overload, not
        // the standalone 32-bar grammar's own longer defaults.
        out.pad.pitchOffsets.assign((size_t) kTotalSteps, kPadOffValue);
        out.pad.gateLengthSteps.assign((size_t) kTotalSteps, 0);

        const PadMotif entry   = generateBreakdownPad(identity.seed, BreakdownPhase::Entry,   kCompactEntryBars);
        const PadMotif body    = generateBreakdownPad(identity.seed, BreakdownPhase::Body,    kCompactBodyBars);
        const PadMotif preDrop = generateBreakdownPad(identity.seed, BreakdownPhase::PreDrop, kCompactPreDropBars);

        int offset = kBreakdownStartStep;
        appendAt(out.pad.pitchOffsets,    offset, entry.pitchOffsets);
        appendAt(out.pad.gateLengthSteps, offset, entry.gateLengthSteps);
        offset += kCompactEntryBars * kBreakdownStepsPerBar;

        appendAt(out.pad.pitchOffsets,    offset, body.pitchOffsets);
        appendAt(out.pad.gateLengthSteps, offset, body.gateLengthSteps);
        offset += kCompactBodyBars * kBreakdownStepsPerBar;

        appendAt(out.pad.pitchOffsets,    offset, preDrop.pitchOffsets);
        appendAt(out.pad.gateLengthSteps, offset, preDrop.gateLengthSteps);

        return out;
    }
}
