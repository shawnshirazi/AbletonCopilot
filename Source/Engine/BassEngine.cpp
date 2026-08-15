#include "BassEngine.h"
#include "BassRhythmGrammar.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace Engine
{
    namespace
    {
        constexpr uint32_t kBassSalt    = 0x42415353u; // 'BASS' - own RNG stream, independent of DrumEngine's 'DROP'
        constexpr int       kStepsPerBar = 16;
        constexpr int       kBlockBars   = 4;
        constexpr int       kNumBars     = kBassSteps / kStepsPerBar; // 8

        std::mt19937 makeRng(uint32_t seed) { return std::mt19937(seed ^ kBassSalt); }

        float uniform01(std::mt19937& rng)
        {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            return dist(rng);
        }

        float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

        // Real measured kick-avoidance, expressed the same way DrumEngine's
        // CrossRoleCorrelation coefficients are: onKickFraction (0.2126) is
        // BELOW the 0.25 a position-independent/uniform placement would
        // produce (4 of 16 positions are kick positions), so this derives an
        // effective negative "correlation" from that real ratio rather than
        // inventing a separate constant - the same
        // correlationFactor(corr, occupied) = max(0, 1+corr) mechanism
        // DrumEngine.cpp uses for its own measured cross-role correlations.
        const float kBassKickCorrelation = kBassGrooveRhythm.onKickFraction / 0.25f - 1.0f;

        bool occupiedAt(const std::array<bool, kBlockBars * kStepsPerBar>& kickBlock, int bar, int stepInBar)
        {
            return kickBlock[(size_t) (bar * kStepsPerBar + stepInBar)];
        }

        // Weighted pick from the real measured pitch-offset distribution
        // (BassRhythmGrammar.h's kBassPitchOffsets) - overwhelmingly the
        // root (0), occasionally a real diatonic bass interval.
        int pickPitchOffset(std::mt19937& rng)
        {
            float r = uniform01(rng);
            float cumulative = 0.0f;
            for (int i = 0; i < kNumBassPitchOffsets; ++i)
            {
                cumulative += kBassPitchOffsets[i].probability;
                if (r < cumulative)
                    return kBassPitchOffsets[i].offsetSemitones;
            }
            return kBassPitchOffsets[0].offsetSemitones; // root - fallback for float rounding at the tail
        }

        using Block = std::array<int8_t, kBlockBars * kStepsPerBar>;

        bool decideBassPosition(std::mt19937& rng, int stepInBar, float densityScale,
                                 const std::array<bool, kBlockBars * kStepsPerBar>& kickBlock, int kickBar)
        {
            float activation = kBassGrooveRhythm.step16Probability[stepInBar]
                              * kBassGrooveRhythm.meanNotesPerBar * densityScale;
            if (occupiedAt(kickBlock, kickBar, stepInBar))
                activation *= std::max(0.0f, 1.0f + kBassKickCorrelation);
            activation = clamp01(activation);
            return uniform01(rng) < activation;
        }

        // Builds the 4-bar motif from scratch: bar 0 is the canonical shape
        // (fresh weighted decision + pitch choice per position, real
        // kick-avoidance against the synthetic four-on-the-floor
        // reference); bars 1-3 either literally repeat bar 0 or apply 1-2
        // small touches - the exact same "controlled bar-to-bar movement
        // inside one motif" mechanism as DrumEngine.cpp's buildRoleBlock,
        // matching the real evidence that groove basslines split between
        // clean 1-bar loops and longer, lightly-varying 4-bar phrases (see
        // analyze_bass_grammar.py's motif-length inspection).
        Block buildBassBlock(std::mt19937& rng, float densityScale,
                              const std::array<bool, kBlockBars * kStepsPerBar>& kickBlock)
        {
            Block block;
            block.fill(kBassOffValue);

            for (int s = 0; s < kStepsPerBar; ++s)
                if (decideBassPosition(rng, s, densityScale, kickBlock, 0))
                    block[(size_t) s] = (int8_t) pickPitchOffset(rng);

            // Real measured adjacent-bar identity isn't available for bass
            // the way it is for drum roles (no per-file multi-bar-pairs
            // stat computed - see BassRhythmGrammar.h's own comment on what
            // it does and doesn't measure), so this reuses the same
            // fallback DrumEngine.cpp's buildRoleBlock uses for roles
            // without one: a disclosed 0.7 default, not a fabricated
          // measurement.
            constexpr float kIdenticalFraction = 0.7f;

            for (int bar = 1; bar < kBlockBars; ++bar)
            {
                const int base = bar * kStepsPerBar;
                for (int s = 0; s < kStepsPerBar; ++s)
                    block[(size_t) (base + s)] = block[(size_t) s];

                if (densityScale <= 0.0f)
                    continue; // genuinely silent at this scale - no touch can introduce content from nothing (see DrumEngine.cpp's own fix for the same issue)

                if (uniform01(rng) < kIdenticalFraction)
                    continue; // literal repeat of bar 0

                const int touches = 1 + (uniform01(rng) < 0.5f ? 0 : 1);
                for (int t = 0; t < touches; ++t)
                {
                    const int s = (int) (uniform01(rng) * (float) kStepsPerBar);
                    const size_t idx = (size_t) (base + s);
                    if (block[idx] != kBassOffValue)
                        block[idx] = kBassOffValue;
                    else if (decideBassPosition(rng, s, densityScale, kickBlock, bar))
                        block[idx] = (int8_t) pickPitchOffset(rng);
                }
            }

            return block;
        }

        // Derives bars 4-7's motif from bars 0-3's (development, not
        // replacement) - guaranteed add/remove touches scaled by
        // params.variation, add-type touches re-checking real kick
        // avoidance before committing (same "no impossible/overlapping
        // role behaviour" guarantee as DrumEngine.cpp's deriveStageBlock:
        // try a few candidates, skip the touch entirely rather than force
        // one onto a bad position).
        Block deriveBassBlock(std::mt19937& rng, const Block& previous, float variation,
                               const std::array<bool, kBlockBars * kStepsPerBar>& kickBlock)
        {
            Block block = previous;
            const int touches = std::max(1, (int) std::lround(variation * 6.0f));
            for (int t = 0; t < touches; ++t)
            {
                const int bar = (int) (uniform01(rng) * (float) kBlockBars);
                int chosenStep = -1;
                for (int attempt = 0; attempt < 5; ++attempt)
                {
                    const int s = (int) (uniform01(rng) * (float) kStepsPerBar);
                    const size_t idx = (size_t) (bar * kStepsPerBar + s);
                    if (block[idx] != kBassOffValue)
                    {
                        chosenStep = s; // removing is always fine
                        break;
                    }
                    const bool kickOccupied = occupiedAt(kickBlock, bar, s);
                    const float weight = kickOccupied ? std::max(0.0f, 1.0f + kBassKickCorrelation) : 1.0f;
                    if (weight > 0.6f)
                    {
                        chosenStep = s;
                        break;
                    }
                }
                if (chosenStep < 0)
                    continue;

                const size_t idx = (size_t) (bar * kStepsPerBar + chosenStep);
                if (block[idx] != kBassOffValue)
                    block[idx] = kBassOffValue;
                else
                    block[idx] = (int8_t) pickPitchOffset(rng);
            }
            return block;
        }

        void copyBlock(std::array<int8_t, kBassSteps>& out, int destBarStart, int barCount, const Block& block)
        {
            for (int i = 0; i < barCount; ++i)
            {
                const int destBar = destBarStart + i;
                if (destBar >= kNumBars)
                    break;
                const int srcBar  = i % kBlockBars;
                for (int s = 0; s < kStepsPerBar; ++s)
                    out[(size_t) (destBar * kStepsPerBar + s)] = block[(size_t) (srcBar * kStepsPerBar + s)];
            }
        }

        // Dynamically-sized version of copyBlock above, for
        // generateArrangementBassPattern's arbitrary-length output.
        void copyBlockDynamic(std::vector<int8_t>& out, int destBarStart, int barCount, int numBars, const Block& block)
        {
            for (int i = 0; i < barCount; ++i)
            {
                const int destBar = destBarStart + i;
                if (destBar >= numBars)
                    break;
                const int srcBar = i % kBlockBars;
                for (int s = 0; s < kStepsPerBar; ++s)
                    out[(size_t) (destBar * kStepsPerBar + s)] = block[(size_t) (srcBar * kStepsPerBar + s)];
            }
        }

        // Same rationale as DrumEngine.cpp's deriveOrRebuildRoleBlock /
        // kBigDeltaRebuildThreshold: deriveBassBlock's touch count depends
        // only on params.variation, not on how much the density scale
        // actually changed between blocks - fine for the small deltas a
        // smoothly ramping section produces, but a real arrangement can
        // also jump a lot (e.g. Breakdown's near-zero bass density into
        // Drop's full density) where a handful of touches would badly
        // under/over-shoot the new target. Beyond this threshold, rebuild
        // fresh at the new density instead.
        constexpr float kBigDeltaRebuildThreshold = 0.3f;

        Block deriveOrRebuildBassBlock(std::mt19937& rng, const Block& previousBlock, float scaleDelta,
                                        float newScale, float variation,
                                        const std::array<bool, kBlockBars * kStepsPerBar>& kickBlock)
        {
            if (std::abs(scaleDelta) > kBigDeltaRebuildThreshold)
                return buildBassBlock(rng, newScale, kickBlock);
            return deriveBassBlock(rng, previousBlock, variation, kickBlock);
        }
    }

    std::array<int8_t, kBassSteps> generateBassPattern(const BassPatternParams& params)
    {
        std::array<int8_t, kBassSteps> out;
        out.fill(kBassOffValue);

        std::mt19937 rng = makeRng(params.seed);

        // Synthetic 4-bar four-on-the-floor kick reference, same
        // measured fact and construction as DrumEngine.cpp's own
        // kickBlock (kKickRhythm.onBeatFraction == 1.0 - a real drop's
        // kick is on every beat, not assumed) - built locally rather than
        // depending on Engine::DropPattern so BassEngine has no ordering
        // dependency on drum generation having already run.
        std::array<bool, kBlockBars * kStepsPerBar> kickBlock {};
        for (int bar = 0; bar < kBlockBars; ++bar)
            for (int beat = 0; beat < 4; ++beat)
                kickBlock[(size_t) (bar * kStepsPerBar + beat * (kStepsPerBar / 4))] = true;

        const float densityScale = 0.7f + params.density * 0.6f; // 0.5 default -> ~1.0, tracks the measured groove-subset's own scale

        const Block established = buildBassBlock(rng, densityScale, kickBlock);
        const Block developed   = deriveBassBlock(rng, established, params.variation, kickBlock);

        copyBlock(out, 0, kBlockBars, established);
        copyBlock(out, kBlockBars, kBlockBars, developed);

        return out;
    }

    std::vector<int8_t> generateArrangementBassPattern(const MusicArrangement& arrangement, const BassPatternParams& params)
    {
        const int numBars = arrangement.totalBars();
        const int total = numBars * kStepsPerBar;
        std::vector<int8_t> out((size_t) total, kBassOffValue);

        std::mt19937 rng = makeRng(params.seed);

        std::array<bool, kBlockBars * kStepsPerBar> kickBlock {};
        for (int bar = 0; bar < kBlockBars; ++bar)
            for (int beat = 0; beat < 4; ++beat)
                kickBlock[(size_t) (bar * kStepsPerBar + beat * (kStepsPerBar / 4))] = true;

        const float densityBase = 0.7f + params.density * 0.6f; // matches generateBassPattern's own scale - at bassEnergy==1.0 this reproduces its exact density

        const int numBlocks = (numBars + kBlockBars - 1) / kBlockBars;
        Block previousBlock {};
        previousBlock.fill(kBassOffValue);
        float previousScale = 0.0f;

        for (int blockIdx = 0; blockIdx < numBlocks; ++blockIdx)
        {
            const int blockBarStart = blockIdx * kBlockBars;
            const int blockBarCount = std::min(kBlockBars, numBars - blockBarStart);

            float avgBassEnergy = 0.0f;
            for (int b = 0; b < blockBarCount; ++b)
                avgBassEnergy += musicStateForBar(arrangement, blockBarStart + b).bassEnergy;
            avgBassEnergy /= (float) blockBarCount;

            const float thisScale = densityBase * avgBassEnergy;

            const Block thisBlock = (blockIdx == 0)
                ? buildBassBlock(rng, thisScale, kickBlock)
                : deriveOrRebuildBassBlock(rng, previousBlock, thisScale - previousScale, thisScale, params.variation, kickBlock);

            copyBlockDynamic(out, blockBarStart, blockBarCount, numBars, thisBlock);

            previousBlock = thisBlock;
            previousScale = thisScale;
        }

        return out;
    }
}
