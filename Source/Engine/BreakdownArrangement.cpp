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
        PadMotif buildSustainedPad(std::mt19937& rng, int bars, int minGate, int maxGate)
        {
            PadMotif out;
            const int totalSteps = bars * kBreakdownStepsPerBar;
            out.pitchOffsets.assign((size_t) totalSteps, kPadOffValue);
            out.gateLengthSteps.assign((size_t) totalSteps, 0);

            std::uniform_int_distribution<int> jitterDist(0, 3);
            std::uniform_int_distribution<int> gateDist(minGate, maxGate);

            const int blockSteps = 2 * kBreakdownStepsPerBar; // one onset per 2-bar block
            for (int blockStart = 0; blockStart < totalSteps; blockStart += blockSteps)
            {
                const int step = blockStart + jitterDist(rng);
                if (step >= totalSteps)
                    continue;
                out.pitchOffsets[(size_t) step] = pickPitchOffset(rng, /*preferWide*/ false);
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

    PadMotif generateBreakdownPad(uint32_t seed, BreakdownPhase phase)
    {
        std::mt19937 rng = makeRng(seed);
        const int bars = barsInPhase(phase);

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

    BreakdownRenderedLoop renderBreakdownPhase(const MusicIdentity& identity, BreakdownPhase phase)
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

        out.pad = generateBreakdownPad(identity.seed, phase);

        return out;
    }
}
