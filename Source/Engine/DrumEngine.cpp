#include "DrumEngine.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace Engine
{
    namespace
    {
        // Role-specific salts so generateKick/Clap/Hat/Perc called with the
        // same seed don't all draw identical random sequences.
        constexpr uint32_t kKickSalt = 0x4B49434Bu; // 'KICK'
        constexpr uint32_t kClapSalt = 0x434C4150u; // 'CLAP'
        constexpr uint32_t kHatSalt  = 0x48415420u; // 'HAT '
        constexpr uint32_t kPercSalt = 0x50455243u; // 'PERC'

        std::mt19937 makeRng(uint32_t seed, uint32_t salt)
        {
            return std::mt19937(seed ^ salt);
        }

        float uniform01(std::mt19937& rng)
        {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            return dist(rng);
        }

        void setHit(StepArray& steps, int step, float velocity)
        {
            if (step >= 0 && step < (int) steps.size())
            {
                steps[(size_t) step].active   = true;
                steps[(size_t) step].velocity = velocity;
            }
        }

        // Shared "syncopation" mechanism for HAT's second layer and PERC:
        // the rhythmically weakest 16th-note position in each beat is the
        // "a" just before the next beat (steps 3/7/11/15 of a 16-step bar).
        // Returns a probability MULTIPLIER, not a probability itself - 1.0
        // at syncopation=0 (no bias, uniform), up to 2x at weak positions /
        // down to 0.5x elsewhere at syncopation=1.
        bool isWeakPosition(int stepWithinBar, int stepsPerBar)
        {
            const int stepsPerBeat = std::max(1, stepsPerBar / 4);
            return (stepWithinBar % stepsPerBeat) == (stepsPerBeat - 1);
        }

        float weakPositionBias(int stepWithinBar, int stepsPerBar, float syncopation)
        {
            return isWeakPosition(stepWithinBar, stepsPerBar)
                       ? (1.0f + syncopation)
                       : (1.0f - syncopation * 0.5f);
        }

        // Shared "variation" mechanism for HAT and PERC: thin even-indexed
        // bars relative to odd ones (a real arrangement technique -
        // alternating full/thinned bars) instead of pure per-step noise.
        // 1.0 at variation=0 (every bar identical), down to 0.5x on even
        // bars at variation=1.
        float barThinningFactor(int bar, float variation)
        {
            return (bar % 2 == 0) ? (1.0f - variation * 0.5f) : 1.0f;
        }

        float clamp01(float v)
        {
            return std::max(0.0f, std::min(1.0f, v));
        }

        // Structural positions kick/hat/clap already claim - their FIXED
        // idiom positions (four-on-the-floor beats, the offbeat-8th hat
        // pulse, the clap backbeat), not the rare/random push/drop/ghost
        // variants. Perc uses this to answer the groove from the gaps
        // rather than piling on top of what's already sounding there - a
        // real "negative space" arrangement technique, not a literal
        // cross-role coupling (DrumEngine's roles stay independently
        // generated functions; this only encodes the KNOWN fixed idiom
        // shape each one always starts from).
        bool isStructurallyClaimed(int stepInBar, int stepsPerBar)
        {
            const int stepsPerBeat = std::max(1, stepsPerBar / 4);
            if (stepInBar % stepsPerBeat == 0)                    return true; // kick's four-on-the-floor beats
            if (stepInBar % stepsPerBeat == stepsPerBeat / 2)      return true; // hat's primary offbeat pulse
            if (stepInBar == (stepsPerBar * 3) / 4)                return true; // clap's backbeat
            return false;
        }

        // A 16-bar phrase isn't 4 equal quarters - it's five distinct
        // musical moments, matched here to actual bar proportions (not
        // literal bar counts, so this degrades sensibly for numBars !=
        // 16 too):
        //   0%   - 25%  bars  1-4  establish groove       - calmest
        //   25%  - 50%  bars  5-8  introduce subtle variation
        //   50%  - 75%  bars  9-12 develop groove
        //   75%  - 93.75% bars 13-15 build tension          - busiest
        //   93.75% - 100%  bar 16   restrained phrase-ending fill - NOT
        //                            the busiest section; a focused,
        //                            deliberate gesture rather than the
        //                            loudest bar in the loop.
        enum class PhraseSection { Establish, SubtleVariation, Development, BuildTension, PhraseEnding };

        PhraseSection phraseSectionFor(int bar, int numBars)
        {
            if (numBars <= 0)
                return PhraseSection::Establish;

            const double t = (double) bar / (double) numBars;
            if (t < 4.0  / 16.0) return PhraseSection::Establish;
            if (t < 8.0  / 16.0) return PhraseSection::SubtleVariation;
            if (t < 12.0 / 16.0) return PhraseSection::Development;
            if (t < 15.0 / 16.0) return PhraseSection::BuildTension;
            return PhraseSection::PhraseEnding;
        }

        // Phrase-aware intensity curve, applied to variation/syncopation-
        // driven probabilities so the 16-bar loop builds across itself
        // instead of every bar being independently random. Pure function
        // of (bar, numBars) - no RNG, fully deterministic, and at any
        // probability that's already 0 (a knob left at its baseline) this
        // multiplies 0 by a number and stays 0, so it never changes the
        // flat/baseline behaviour of any role, only how strongly the
        // already-present optional effects lean in as the pattern
        // progresses. PhraseEnding is deliberately NOT the highest value -
        // "restrained... fill", not the busiest bar in the loop; the
        // per-role phrase-ending gestures (see generatePerc) are what
        // actually mark it, not raw density.
        float phraseIntensity(int bar, int numBars)
        {
            switch (phraseSectionFor(bar, numBars))
            {
                case PhraseSection::Establish:       return 0.55f;
                case PhraseSection::SubtleVariation: return 0.8f;
                case PhraseSection::Development:     return 1.1f;
                case PhraseSection::BuildTension:    return 1.35f;
                case PhraseSection::PhraseEnding:    return 1.0f;
            }
            return 1.0f;
        }
    }

    StepArray generateKick(const StepGridConfig& grid, const DrumPatternParams& params)
    {
        const int total = totalSteps(grid);
        StepArray steps((size_t) total);
        std::mt19937 rng = makeRng(params.seed, kKickSalt);

        const int stepsPerBeat = std::max(1, grid.stepsPerBar / 4);

        for (int bar = 0; bar < grid.numBars; ++bar)
        {
            const int base = bar * grid.stepsPerBar;

            // variation: rare per-bar fill (drop one beat). Kept genuinely
            // rare (scaled well below variation's raw value) since a
            // steady, reliable kick is the point, not the exception - see
            // the density=NOT-USED note in DrumEngine.h. Phrase-scaled so
            // drops stay rarer in the establishing bars and become more
            // likely toward the phrase-ending bars - four-on-the-floor
            // itself never changes, only how often the rare drop fires.
            const float phrase = phraseIntensity(bar, grid.numBars);
            bool dropABeat  = uniform01(rng) < params.variation * 0.15f * phrase;
            int  droppedBeat = dropABeat ? (int) (uniform01(rng) * 4.0f) : -1;
            droppedBeat = std::min(droppedBeat, 3);

            for (int beat = 0; beat < 4; ++beat)
            {
                if (beat == droppedBeat)
                    continue;
                setHit(steps, base + beat * stepsPerBeat, 1.0f);
            }

            // syncopation: rare pushed hit on the weakest position (the
            // "and" of the last beat), driving into the next bar - same
            // phrase scaling as the drop above.
            if (uniform01(rng) < params.syncopation * 0.2f * phrase)
                setHit(steps, base + grid.stepsPerBar - 2, 0.7f);
        }

        return steps;
    }

    StepArray generateClap(const StepGridConfig& grid, const DrumPatternParams& params)
    {
        const int total = totalSteps(grid);
        StepArray steps((size_t) total);
        std::mt19937 rng = makeRng(params.seed, kClapSalt);

        // Traditional backbeat position (beat 3) - the sourced pattern
        // still lands there, it's just sparse across bars rather than
        // hitting every bar.
        const int backbeatStep   = (grid.stepsPerBar * 3) / 4;
        const int doubletGapStep = std::max(1, grid.stepsPerBar / 8); // one 8th-note later

        // syncopation: occasional +/-1 step nudge off the canonical
        // position. Phrase-scaled (controlled clap variation - jitter
        // leans in more as the pattern develops) by the bar the hit
        // actually lands on.
        auto jitter = [&](int step, int bar) -> int
        {
            if (uniform01(rng) < params.syncopation * 0.3f * phraseIntensity(bar, grid.numBars))
                return step + (uniform01(rng) < 0.5f ? -1 : 1);
            return step;
        };

        for (int cycleStart = 0; cycleStart < grid.numBars; cycleStart += 4)
        {
            // variation: probability this cycle swaps which bar gets the
            // single hit vs. the doublet, instead of the canonical
            // bar-2=single/bar-4=doublet layout. At variation=0 this is
            // never true (uniform01 never returns a negative number, so
            // "< 0.0f" never fires); at variation=1 it's always true
            // (uniform01's [0,1) range means "< 1.0f" always fires) - both
            // boundaries are exact, not just statistically likely. Not
            // phrase-scaled - this one stays a pure function of variation
            // alone so the boundary guarantee holds regardless of phrase.
            const bool swapped = uniform01(rng) < params.variation;
            const int  singleBarOffset  = swapped ? 3 : 1;
            const int  doubletBarOffset = swapped ? 1 : 3;

            const int singleBar  = cycleStart + singleBarOffset;
            const int doubletBar = cycleStart + doubletBarOffset;

            if (singleBar < grid.numBars)
            {
                const int base = singleBar * grid.stepsPerBar;
                setHit(steps, jitter(base + backbeatStep, singleBar), 1.0f);
            }
            if (doubletBar < grid.numBars)
            {
                const int base = doubletBar * grid.stepsPerBar;
                setHit(steps, jitter(base + backbeatStep, doubletBar), 1.0f);
                setHit(steps, jitter(base + backbeatStep + doubletGapStep, doubletBar), 0.8f);
            }
        }

        // density: rare extra ghost clap somewhere a bar would otherwise
        // leave silent - kept low-probability, this role stays sparse even
        // at density=1. Phrase-scaled: ghosts lean in more in the
        // development/ending phrases than the establishing bars.
        for (int bar = 0; bar < grid.numBars; ++bar)
        {
            if (uniform01(rng) >= params.density * 0.1f * phraseIntensity(bar, grid.numBars))
                continue;
            const int base = bar * grid.stepsPerBar;
            const int step = base + (int) (uniform01(rng) * (float) grid.stepsPerBar);
            setHit(steps, step, 0.5f);
        }

        return steps;
    }

    StepArray generateHat(const StepGridConfig& grid, const DrumPatternParams& params)
    {
        const int total = totalSteps(grid);
        StepArray steps((size_t) total);
        std::mt19937 rng = makeRng(params.seed, kHatSalt);

        const int eighthStep = std::max(1, grid.stepsPerBar / 8);

        for (int bar = 0; bar < grid.numBars; ++bar)
        {
            const int base    = bar * grid.stepsPerBar;
            const float phrase = phraseIntensity(bar, grid.numBars);
            // Start of each 4-bar segment - a musically useful accent
            // position (the phrase's own downbeat), not an arbitrary one.
            const bool isPhraseDownbeat = (bar % 4 == 0);

            // Layer 1 - primary offbeat pulse: off-beat 8ths (the "and" of
            // each beat), the genre-defining foundation, always present in
            // principle (same reasoning as kick's four-on-the-floor) - but
            // "always present" doesn't mean "never varies": variation can
            // occasionally REMOVE one of the four, a real groove technique
            // (the hat that's conspicuously not there), phrase-scaled so
            // it's essentially never in the establishing bars and more
            // likely by the tension-building ones. At variation=0 this
            // never fires (0 * anything = 0), so the foundation is exactly
            // as reliable as before whenever variation is left off.
            bool primaryHit[4] = { true, true, true, true };
            for (auto&& hit : primaryHit)
                if (uniform01(rng) < params.variation * 0.08f * phrase)
                    hit = false;

            for (int k = 0; k < 4; ++k)
            {
                if (!primaryHit[k])
                    continue;
                const int step = base + eighthStep * (2 * k + 1);
                if (step < base + grid.stepsPerBar)
                {
                    // Controlled velocity accent, not flat: the offbeats
                    // leading into beats 1 and 3 sit slightly hotter than
                    // those leading into 2 and 4, and phrase-downbeat bars
                    // (the start of each 4-bar segment) sit hotter still -
                    // a fixed, deterministic groove shape, not RNG-driven.
                    float accentVel = (k % 2 == 0) ? 0.95f : 0.8f;
                    if (isPhraseDownbeat)
                        accentVel = std::min(1.0f, accentVel + 0.05f);
                    setHit(steps, step, accentVel);
                }
            }

            // Layer 2 - supporting 16ths: selective, syncopation-biased
            // fills on top of the primary pulse. Density controls how
            // much, but kept deliberately restrained (a lower base rate
            // than a "second full pulse" would need) - this is
            // "selective 16th-note activity", not a wall of hats, and the
            // phrase curve leans it in gradually rather than flooding
            // every bar equally.
            const float barDensity = clamp01(params.density * barThinningFactor(bar, params.variation));

            for (int stepInBar = 0; stepInBar < grid.stepsPerBar; ++stepInBar)
            {
                const int step = base + stepInBar;
                if (steps[(size_t) step].active)
                    continue;

                const float bias = weakPositionBias(stepInBar, grid.stepsPerBar, params.syncopation);
                const float supportP = clamp01(barDensity * 0.35f * bias * phrase);

                if (uniform01(rng) < supportP)
                {
                    const float vel = 0.35f + uniform01(rng) * 0.2f; // 0.35-0.55, clearly under the primary pulse
                    setHit(steps, step, vel);
                    continue;
                }

                // Layer 3 - ghost hats: much quieter, much rarer,
                // independently rolled - a low-level textural presence
                // rather than a rhythmic statement, filling in some of the
                // negative space the supporting layer leaves behind.
                const float ghostP = clamp01(barDensity * 0.08f * phrase);
                if (uniform01(rng) < ghostP)
                {
                    const float vel = 0.12f + uniform01(rng) * 0.1f; // 0.12-0.22
                    setHit(steps, step, vel);
                }
            }
        }

        return steps;
    }

    StepArray generatePerc(const StepGridConfig& grid, const DrumPatternParams& params)
    {
        const int total = totalSteps(grid);
        StepArray steps((size_t) total);
        std::mt19937 rng = makeRng(params.seed, kPercSalt);

        for (int bar = 0; bar < grid.numBars; ++bar)
        {
            const int base = bar * grid.stepsPerBar;
            const float barDensity = clamp01(params.density * barThinningFactor(bar, params.variation));
            const float phrase     = phraseIntensity(bar, grid.numBars);

            for (int stepInBar = 0; stepInBar < grid.stepsPerBar; ++stepInBar)
            {
                // density: base per-step probability, deliberately damped
                // so this role stays an accent, not a second full pattern,
                // even at density=1. syncopation: biases placement toward
                // the weakest 16th positions (same mechanism as HAT's
                // second layer). Restrained by design: the phrase curve
                // leans density in gradually rather than flooding every
                // bar equally, keeping this an accent role even as the
                // pattern develops. claimedPenalty pushes perc away from
                // kick's beats/hat's offbeat pulse/clap's backbeat - the
                // fixed idiom positions those roles already occupy - so
                // perc answers the groove from the gaps (negative space)
                // instead of piling onto what's already sounding, rather
                // than just being "more percussion" everywhere.
                const float bias           = weakPositionBias(stepInBar, grid.stepsPerBar, params.syncopation);
                const float claimedPenalty = isStructurallyClaimed(stepInBar, grid.stepsPerBar) ? 0.15f : 1.0f;
                const float p = clamp01(barDensity * 0.15f * bias * phrase * claimedPenalty);

                if (uniform01(rng) < p)
                {
                    // variation widens the velocity range (more dynamic
                    // hit-to-hit swing) in addition to the bar-thinning
                    // above - both readings of "how varied" under one
                    // knob, giving movement across the 16 bars rather
                    // than a flat, static accent every time.
                    const float velRange = 0.3f + params.variation * 0.3f;
                    const float vel = std::min(1.0f, 0.4f + uniform01(rng) * velRange);
                    setHit(steps, base + stepInBar, vel);
                }
            }
        }

        // Occasional phrase-ending fill: the very last bar of the pattern
        // gets a small chance of one extra accent hit at an unclaimed
        // (negative-space) position, driven by variation - an explicit
        // "the loop is about to restart" cue (a real arrangement
        // technique), restrained to at most one hit rather than a busy
        // roll, and never fires at variation=0, same as every other
        // variation-gated effect in this file.
        if (grid.numBars > 0 && uniform01(rng) < params.variation * 0.35f)
        {
            const int lastBar = grid.numBars - 1;
            const int base    = lastBar * grid.stepsPerBar;

            // A handful of attempts to land on an unclaimed step rather
            // than accepting whatever the first roll gives - keeps the
            // fill consistent with the negative-space placement used
            // everywhere else in this role, without a hard guarantee
            // (falls back to whatever step the last attempt lands on).
            int step = base + (int) (uniform01(rng) * (float) grid.stepsPerBar);
            for (int attempt = 0; attempt < 4; ++attempt)
            {
                const int candidate = base + (int) (uniform01(rng) * (float) grid.stepsPerBar);
                if (!isStructurallyClaimed(candidate - base, grid.stepsPerBar))
                {
                    step = candidate;
                    break;
                }
            }

            const float vel = 0.7f + uniform01(rng) * 0.3f;
            setHit(steps, step, vel);
        }

        return steps;
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
