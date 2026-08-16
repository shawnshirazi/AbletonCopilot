#include "GrooveLoop.h"
#include "DrumRhythmGrammar.h"
#include <random>

namespace Engine
{
    namespace
    {
        // Independent RNG stream from generateDrop's own ('DROP') and
        // BassEngine's own ('BASS'/'ARCH') - same per-subsystem-salt
        // convention already used throughout Engine/, so this generator's
        // randomness is deterministic but never accidentally correlated
        // with another subsystem's stream for the same seed.
        constexpr uint32_t kGrooveSalt = 0x47525556u; // 'GRUV'

        // Single flat energy level for BOTH 4-bar blocks (bars0-3 and
        // bars4-7) - deliberately NOT an arc (no kEstablish/kDevelop/
        // kIncrease/kFullDrop progression - those stay private to
        // DrumEngine.cpp and are exactly what this phase removes). Values
        // match DrumEngine.cpp's own existing kDevelop constant exactly -
        // reused as a single reasonable, already-tuned calibration point
        // (a real "moderate, both hat/perc voices present, hatOpen sparse,
        // percB present-but-secondary" level, per the same measured-data-
        // driven scaling buildStageFresh/deriveStage already apply), not a
        // new number invented for this file.
        constexpr StageEnergy kGrooveEnergy { 0.50f, 0.20f, 0.55f, 0.30f };

        // Kick: fresh, minimal - deliberately NOT reusing generateDrop's
        // own kick code, which has two arrangement-flavored embellishments
        // explicitly out of scope for a flat loop: a "phrase downbeat"
        // accent (bar % 4 == 0) and a bar-16 "transition fill" hit. Every
        // one of the 8 bars gets the identical measured four-on-the-floor
        // shape - kKickRhythm.step16RelativeVelocity[0/4/8/12] = beat 1
        // strongest (0.988), beats 2-4 close together (~0.85-0.86) - the
        // "keep the existing kick foundation" ask, with only the two
        // removed embellishments actually removed.
        StepArray buildGrooveKick(int stepsPerBar, int numBars)
        {
            StepArray kick((size_t) (numBars * stepsPerBar));
            const int stepsPerBeat = stepsPerBar / 4;
            for (int bar = 0; bar < numBars; ++bar)
                for (int beat = 0; beat < 4; ++beat)
                {
                    const int stepInBar = beat * stepsPerBeat;
                    const float vel = kKickRhythm.step16RelativeVelocity[stepInBar];
                    kick[(size_t) (bar * stepsPerBar + stepInBar)] = Hit { true, vel };
                }
            return kick;
        }
    }

    GrooveLoop generateGrooveLoop(const GrooveLoopParams& params)
    {
        GrooveLoop out;
        const int stepsPerBar = kGrooveLoopStepsPerBar;
        const int numBars     = kGrooveLoopBars;

        std::mt19937 rng(params.seed ^ kGrooveSalt);

        // ---- KICK ----
        out.drum.kick = buildGrooveKick(stepsPerBar, numBars);

        // 4-bar correlation reference for clap/hat/perc below - the real
        // generated kick (bars 0-3), not a synthetic stand-in. Kick is
        // identical on every bar, so any 4 consecutive bars would do; the
        // first 4 are used directly, no separate synthetic block needed.
        StepArray kickRef((size_t) (4 * stepsPerBar));
        for (int i = 0; i < 4 * stepsPerBar; ++i)
            kickRef[(size_t) i] = out.drum.kick[(size_t) i];

        // ---- CLAP - one 4-bar block (buildRoleBlock, reused unmodified
        // from DrumEngine.cpp/.h), tiled into both halves of the loop.
        // buildRoleBlock's own internal repeat-or-touch logic (driven by
        // kClapRhythm.adjacentBarsIdenticalFraction, real measured 80.9%)
        // already gives real, evidence-matched bar-to-bar behavior - no
        // new clap logic needed here at all. ----
        const StepArray clapBlock = buildRoleBlock(rng, kClapRhythm, stepsPerBar,
                                                     0.7f + params.density * 0.6f, params.syncopation,
                                                     { { &kickRef, kCrossRoleCorrelation.kickClap, 0 } });
        out.drum.clap = StepArray((size_t) (numBars * stepsPerBar));
        copyBlock(out.drum.clap, 0, 4, numBars, stepsPerBar, clapBlock);
        copyBlock(out.drum.clap, 4, 4, numBars, stepsPerBar, clapBlock);

        // ---- HAT CLOSED / HAT OPEN / PERC A / PERC B - one established
        // 4-bar block (bars 0-3, buildStageFresh) and one derived 4-bar
        // block (bars 4-7, deriveStage) at the SAME flat energy level
        // (previousEnergy == thisEnergy == kGrooveEnergy for every role,
        // so energyDelta is exactly 0.0 for every role - no energy
        // change at all, only params.variation drives the touches that
        // differentiate block 2 from block 1). This directly reuses
        // buildStageFresh/deriveStage's own already-tested cross-role
        // coordination (kick/clap correlation, hatClosed->hatOpen->
        // percA->percB ordering, the disclosed non-measured percB-avoids-
        // percA rule) with zero new composition logic. ----
        const StageBlocks block1 = buildStageFresh(rng, kickRef, clapBlock, stepsPerBar,
                                                     params.density, params.syncopation, kGrooveEnergy);
        const StageBlocks block2 = deriveStage(rng, block1, kGrooveEnergy, kGrooveEnergy, kickRef, clapBlock,
                                                 stepsPerBar, params.density, params.syncopation, params.variation);

        out.drum.hatClosed = StepArray((size_t) (numBars * stepsPerBar));
        out.drum.hatOpen   = StepArray((size_t) (numBars * stepsPerBar));
        out.drum.percA     = StepArray((size_t) (numBars * stepsPerBar));
        out.drum.percB     = StepArray((size_t) (numBars * stepsPerBar));

        copyBlock(out.drum.hatClosed, 0, 4, numBars, stepsPerBar, block1.hatClosed);
        copyBlock(out.drum.hatOpen,   0, 4, numBars, stepsPerBar, block1.hatOpen);
        copyBlock(out.drum.percA,     0, 4, numBars, stepsPerBar, block1.percA);
        copyBlock(out.drum.percB,     0, 4, numBars, stepsPerBar, block1.percB);

        copyBlock(out.drum.hatClosed, 4, 4, numBars, stepsPerBar, block2.hatClosed);
        copyBlock(out.drum.hatOpen,   4, 4, numBars, stepsPerBar, block2.hatOpen);
        copyBlock(out.drum.percA,     4, 4, numBars, stepsPerBar, block2.percA);
        copyBlock(out.drum.percB,     4, 4, numBars, stepsPerBar, block2.percB);

        // ---- BASS - Engine::generateBassPattern, completely unmodified.
        // Already produces exactly 128 steps (8 bars: a 4-bar motif +
        // 4-bar params.variation-scaled development) using a real,
        // measured kick-avoidance correlation (kBassKickCorrelation,
        // BassRhythmGrammar.h) against its own synthetic four-on-the-floor
        // reference - which matches this file's own kick exactly, since
        // this loop's kick IS plain four-on-the-floor too. ----
        BassPatternParams bassParams;
        bassParams.density   = params.density;
        bassParams.variation = params.variation;
        bassParams.seed      = params.seed;
        const auto bassPattern = generateBassPattern(bassParams);
        out.bass.assign(bassPattern.begin(), bassPattern.end());
        out.bassGateLengthSteps.assign((size_t) kGrooveLoopTotalSteps, 0);

        return out;
    }
}
