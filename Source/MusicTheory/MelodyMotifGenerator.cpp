#include "MelodyMotifGenerator.h"
#include "MelodicTechnoTheory.h"
#include "MelodyScorer.h"
#include <vector>

namespace MelodyMotifGenerator
{
    namespace
    {
        constexpr int8_t kMelodyOff = -128; // matches MelodyGridComponent::kMelodyOff

        // Diatonic scale intervals (semitones from the root) - same as
        // Companion/patterns.py's MINOR_SCALE/MAJOR_SCALE.
        constexpr int kMinorScale[7] = { 0, 2, 3, 5, 7, 8, 10 };
        constexpr int kMajorScale[7] = { 0, 2, 4, 5, 7, 9, 11 };

        // A motif is a short rhythmic/melodic cell - (step, scale-degree)
        // pairs within a 32-step (2-bar) unit. Modelled on real house/
        // techno bassline/lead idioms (not arbitrary shapes) - see the
        // comment on each one. Repetition (not novelty) is the point in
        // this genre, so the whole 8-bar phrase carries the SAME motif,
        // just transposed along a chord-progression arc (kArcShapes
        // below) - that's what makes it read as a composed idea instead
        // of random notes.
        struct MotifNote { int step; int degree; };

        // Off-beat pulse: hits on the "and" of each beat (steps 2/6/10/14
        // per bar) - the classic house/garage syncopation that sits in the
        // gaps of a four-on-the-floor kick rather than doubling it.
        const std::vector<MotifNote> kMotifOffbeatPulse = {
            {2,0},{6,0},{10,1},{14,0}, {18,0},{22,0},{26,1},{30,0}
        };

        // Rolling 16ths: near-continuous 16th-note movement with a couple
        // of rests for breathing room - what "rolling" means in house and
        // melodic techno production.
        const std::vector<MotifNote> kMotifRolling16ths = {
            {0,0},{2,0},{4,1},{8,0},{10,0},{12,1},{14,0},
            {16,0},{18,0},{20,1},{24,0},{26,0},{28,1},{30,0}
        };

        // Walking octaves: root/fifth/octave movement outlining the chord
        // across the bar - classic deep-house walking bassline, works as a
        // lead/pluck idiom too.
        const std::vector<MotifNote> kMotifWalkingOctaves = {
            {0,0},{4,7},{8,0},{12,4}, {16,0},{20,7},{24,0},{28,4}
        };

        // Kick interplay: melody deliberately sits in the gaps between
        // four-on-the-floor kick hits (steps 0/4/8/12) instead of stacking
        // on them - the interlocking kick/melodic relationship most house
        // and techno records are actually built on.
        const std::vector<MotifNote> kMotifKickInterplay = {
            {2,0},{3,1},{6,0},{11,0},{14,1}, {18,0},{19,1},{22,0},{27,0},{30,1}
        };

        // Sparse hypnotic: a handful of hits, mostly root - minimal/
        // hypnotic techno feel, and the natural pick for an "atmospheric"
        // prompt.
        const std::vector<MotifNote> kMotifSparseHypnotic = {
            {0,0},{7,1},{14,0},{20,2},{28,0}
        };

        // Motif pools by energy - a style word actually changes which
        // idiom gets picked, not just how many notes get thinned out of a
        // random one.
        const std::vector<MotifNote>* const kDenseMotifs[] = {
            &kMotifRolling16ths, &kMotifOffbeatPulse, &kMotifKickInterplay
        };
        const std::vector<MotifNote>* const kSparseMotifs[] = {
            &kMotifSparseHypnotic, &kMotifWalkingOctaves
        };
        const std::vector<MotifNote>* const kAllMotifs[] = {
            &kMotifRolling16ths, &kMotifOffbeatPulse, &kMotifKickInterplay,
            &kMotifSparseHypnotic, &kMotifWalkingOctaves
        };

        // Per-pass scale-degree progression across the 4 x 2-bar phrase -
        // real chord progressions used constantly in house/melodic techno,
        // in scale-degree terms (0=i, 2=III, 4=v, 5=VI, 6=VII), not
        // arbitrary shapes. Picked (deterministically, from the caller's
        // seed) for variety between generations.
        constexpr int kArcShapes[][4] = {
            { 0, 5, 2, 6 }, // i - VI - III - VII (the melodic-techno/house progression)
            { 0, 4, 5, 3 }, // i - v - VI - iv (moodier, plagal-leaning)
            { 0, 6, 5, 4 }, // i - VII - VI - v (descending, hypnotic)
            { 0, 0, 5, 6 }, // i - i - VI - VII (static verse, then a lift)
        };
    }

    std::array<int8_t, 128> generateMelodyMotif(int keyRootSemitone, bool isMinor,
                                                  MelodyCategory category, MelodyStyle style,
                                                  uint32_t seed)
    {
        juce::ignoreUnused(keyRootSemitone); // root-relative; playback adds the key root itself

        const int* scale = isMinor ? kMinorScale : kMajorScale;

        auto degreeToSemitone = [&](int degree)
        {
            const int octave = degree >= 0 ? degree / 7 : (degree - 6) / 7;
            const int idx     = ((degree % 7) + 7) % 7;
            return octave * 12 + scale[idx];
        };

        juce::Random rng(seed); // deterministic - see header comment

        // Pick the motif pool by style - Rolling/Atmospheric pick a
        // genuinely different real idiom, not just a thinned-out random one.
        const std::vector<MotifNote>* const* pool = kAllMotifs;
        int poolSize = (int) (sizeof(kAllMotifs) / sizeof(kAllMotifs[0]));
        if (style == MelodyStyle::Rolling)
        {
            pool = kDenseMotifs;
            poolSize = (int) (sizeof(kDenseMotifs) / sizeof(kDenseMotifs[0]));
        }
        else if (style == MelodyStyle::Atmospheric)
        {
            pool = kSparseMotifs;
            poolSize = (int) (sizeof(kSparseMotifs) / sizeof(kSparseMotifs[0]));
        }

        constexpr int kNumSteps     = 128; // matches MelodyGridComponent::kNumSteps
        constexpr int stepsPerPass = kNumSteps / 4; // 32 steps = 2 bars per pass

        // Constrain, then rank the legal space: build several candidates
        // (each already register-clamped/scale-correct by construction)
        // and keep the most idiomatic one instead of the first roll.
        constexpr int kNumCandidates = 4;
        std::array<int8_t, 128> best {};
        float bestScore = -1.0f;

        for (int candidate = 0; candidate < kNumCandidates; ++candidate)
        {
            std::array<int8_t, 128> buf;
            buf.fill(kMelodyOff);

            const auto& motif = *pool[rng.nextInt(poolSize)];
            const auto& arc    = kArcShapes[rng.nextInt((int) (sizeof(kArcShapes) / sizeof(kArcShapes[0])))];
            const int   baseDegree = rng.nextInt(2); // start on or just above the root

            for (int pass = 0; pass < 4; ++pass)
            {
                const int segStart = pass * stepsPerPass;

                for (size_t i = 0; i < motif.size(); ++i)
                {
                    const bool isFinalResolveNote = pass == 3 && i == motif.size() - 1;

                    const int step = segStart + motif[i].step;
                    if (step < 0 || step >= kNumSteps)
                        continue;

                    int degree = baseDegree + arc[pass] + motif[i].degree;

                    // Resolve the final note of the final pass back to the
                    // root, so the 8-bar loop restarts cleanly instead of
                    // hanging mid-phrase.
                    if (isFinalResolveNote)
                        degree = 0;

                    buf[(size_t) step] = (int8_t) juce::jlimit(-24, 24, degreeToSemitone(degree));
                }
            }

            const float score = MelodyScorer::scoreTrack(buf, category, keyRootSemitone, isMinor).total;
            if (score > bestScore)
            {
                bestScore = score;
                best = buf;
            }
        }

        return best;
    }
}
