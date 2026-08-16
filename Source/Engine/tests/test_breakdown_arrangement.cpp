// Regression tests for Engine::BreakdownArrangement - the 3-phase
// Entry/Body/PreDrop breakdown state machine that sits ALONGSIDE the
// existing MusicIdentity::RenderMode{Drop,Breakdown}, never touching it.
// One test block per property the user explicitly asked to see
// demonstrated BEFORE any existing generator was changed - see the plan
// this was built from (tender-marinating-truffle.md) and
// MLPipeline/musical_target/melodic_techno_research.md section 11.4 for
// the evidence each assertion traces back to.
#include "../BreakdownArrangement.h"
#include "../MusicIdentity.h"
#include "TestSupport.h"
#include <cstdint>

using namespace Engine;

namespace
{
    int countActivePad(const PadMotif& pad)
    {
        int n = 0;
        for (auto v : pad.pitchOffsets)
            if (v != kPadOffValue) ++n;
        return n;
    }

    double meanGateLengthAtOnsets(const PadMotif& pad)
    {
        int64_t sum = 0;
        int n = 0;
        for (size_t i = 0; i < pad.pitchOffsets.size(); ++i)
        {
            if (pad.pitchOffsets[i] != kPadOffValue)
            {
                sum += pad.gateLengthSteps[i];
                ++n;
            }
        }
        return n > 0 ? (double) sum / n : 0.0;
    }

    bool stepArraysEqual(const StepArray& a, const StepArray& b)
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (a[i].active != b[i].active || a[i].velocity != b[i].velocity)
                return false;
        return true;
    }

    MusicIdentity makeIdentity(uint32_t seed)
    {
        MusicIdentityParams p;
        p.seed = seed;
        p.bpm = 124.0;
        p.rootNote = 0;
        p.isMinor = true;
        return generateMusicIdentity(p);
    }
}

int main()
{
    // ---- Property 1: kick/sub remain absent during the appropriate
    // breakdown (HIGH-confidence hard constraint - kick OFF and core/sub
    // bass OFF in every measured reference arrangement's break body, and
    // in the unambiguous instances, for the whole break span). ----
    {
        const BreakdownPhase phases[] = { BreakdownPhase::Entry, BreakdownPhase::Body, BreakdownPhase::PreDrop };
        for (uint32_t seed = 1; seed < 40; ++seed)
        {
            const MusicIdentity id = makeIdentity(seed);
            for (auto phase : phases)
            {
                const BreakdownRenderedLoop out = renderBreakdownPhase(id, phase);
                CHECK(out.kick.muted == true);
                CHECK(out.bassMuted == true);
            }
        }
    }

    // ---- Property 2: breakdown introduces dedicated melodic/harmonic
    // material rather than simply thinning drums - the pad must have
    // real, sustained content (not silence, not short drum-like stabs).
    // meanNoteLenSteps for real GROOVE bass is 1.32 steps (short/punchy,
    // BassRhythmGrammar.h) - the pad must read as clearly longer/more
    // sustained than that, not just "a quieter drum hit." ----
    {
        for (uint32_t seed = 1; seed < 40; ++seed)
        {
            const MusicIdentity id = makeIdentity(seed);
            const BreakdownRenderedLoop body = renderBreakdownPhase(id, BreakdownPhase::Body);
            const int active = countActivePad(body.pad);
            CHECK(active > 0); // real content, not silence
            const double meanGate = meanGateLengthAtOnsets(body.pad);
            CHECK(meanGate >= 8.0); // clearly sustained, not a thinned drum stab (cf. bass's own 1.32-step mean)

            const BreakdownRenderedLoop entry = renderBreakdownPhase(id, BreakdownPhase::Entry);
            CHECK(countActivePad(entry.pad) > 0);
        }
    }

    // ---- Property 3: pre-drop increases tension progressively - checked
    // as two concrete, measurable trends across PreDrop's own 8 bars:
    // onset density rises in the second half vs the first half, and mean
    // gate length shortens (notes get more urgent, not more relaxed).
    // Averaged across many seeds to avoid single-seed flakiness, per the
    // plan. This is a disclosed pattern-level analogue of the HIGH-
    // confidence "riser is the reliable pre-drop signal" finding - real
    // audio risers/filter automation aren't buildable without a DSP-
    // automation feature this engine doesn't have. ----
    {
        int64_t firstHalfOnsetsTotal = 0, secondHalfOnsetsTotal = 0;
        double firstHalfGateTotal = 0.0, secondHalfGateTotal = 0.0;
        int firstHalfGateCount = 0, secondHalfGateCount = 0;
        const int seeds = 60;
        for (uint32_t seed = 1; seed <= (uint32_t) seeds; ++seed)
        {
            const MusicIdentity id = makeIdentity(seed);
            const PadMotif pad = generateBreakdownPad(id.seed, BreakdownPhase::PreDrop);
            const int stepsPerHalf = (int) pad.pitchOffsets.size() / 2;
            for (int i = 0; i < (int) pad.pitchOffsets.size(); ++i)
            {
                if (pad.pitchOffsets[(size_t) i] == kPadOffValue)
                    continue;
                if (i < stepsPerHalf)
                {
                    ++firstHalfOnsetsTotal;
                    firstHalfGateTotal += pad.gateLengthSteps[(size_t) i];
                    ++firstHalfGateCount;
                }
                else
                {
                    ++secondHalfOnsetsTotal;
                    secondHalfGateTotal += pad.gateLengthSteps[(size_t) i];
                    ++secondHalfGateCount;
                }
            }
        }
        CHECK(secondHalfOnsetsTotal > firstHalfOnsetsTotal); // density rises toward the drop
        const double meanGateFirst  = firstHalfGateCount  > 0 ? firstHalfGateTotal  / firstHalfGateCount  : 0.0;
        const double meanGateSecond = secondHalfGateCount > 0 ? secondHalfGateTotal / secondHalfGateCount : 0.0;
        CHECK(meanGateSecond < meanGateFirst); // notes get shorter/more urgent toward the drop
    }

    // ---- Property 4: drop restores the groove coherently - the
    // EXISTING, untouched Engine::renderMode(identity, RenderMode::Drop)
    // still produces the full active kick/clap/hat/perc/bass groove. Not
    // new behavior - a guard proving this pass didn't weaken it. ----
    {
        for (uint32_t seed = 1; seed < 20; ++seed)
        {
            const MusicIdentity id = makeIdentity(seed);
            const RenderedLoop drop = renderMode(id, RenderMode::Drop);
            CHECK(drop.kick.muted == false);
            CHECK(drop.clap.muted == false);
            CHECK(drop.hatClosed.muted == false);
            CHECK(drop.hatOpen.muted == false);
            CHECK(drop.percA.muted == false);
            CHECK(drop.percB.muted == false);
            CHECK(drop.bassMuted == false);

            int activeKick = 0;
            for (auto& h : drop.drum.kick) if (h.active) ++activeKick;
            CHECK(activeKick > 0);
        }
    }

    // ---- Property 5: bass/melody/drums share the same musical
    // identity/seed and remain synchronized - the pad is a pure function
    // of identity.seed (same seed used by drumMotif/bassMotif already),
    // not an independently-seeded/unsynchronized generator. ----
    {
        for (uint32_t seed = 1; seed < 30; ++seed)
        {
            const MusicIdentity idA = makeIdentity(seed);
            const MusicIdentity idB = makeIdentity(seed);
            CHECK(idA.seed == idB.seed); // same seed drives drum/bass already (existing guarantee)

            const PadMotif padA = generateBreakdownPad(idA.seed, BreakdownPhase::Body);
            const PadMotif padB = generateBreakdownPad(idB.seed, BreakdownPhase::Body);
            CHECK(padA.pitchOffsets == padB.pitchOffsets);
            CHECK(padA.gateLengthSteps == padB.gateLengthSteps);
        }

        // A different seed can (not must, but generally does) produce a
        // different pad - proves generateBreakdownPad actually reads the
        // seed rather than ignoring it.
        bool sawDifference = false;
        const PadMotif reference = generateBreakdownPad(1u, BreakdownPhase::Body);
        for (uint32_t seed = 2; seed < 30; ++seed)
        {
            const PadMotif other = generateBreakdownPad(seed, BreakdownPhase::Body);
            if (other.pitchOffsets != reference.pitchOffsets)
            {
                sawDifference = true;
                break;
            }
        }
        CHECK(sawDifference);
    }

    // ---- Property 6: the same generated loop is repeatable when the
    // loop restarts - renderBreakdownPhase/generateBreakdownPad called
    // twice on the identical identity produce byte-identical output, the
    // same determinism guarantee generateMusicIdentity already has. ----
    {
        for (uint32_t seed = 1; seed < 30; ++seed)
        {
            const MusicIdentity id = makeIdentity(seed);
            const BreakdownPhase phases[] = { BreakdownPhase::Entry, BreakdownPhase::Body, BreakdownPhase::PreDrop };
            for (auto phase : phases)
            {
                const BreakdownRenderedLoop a = renderBreakdownPhase(id, phase);
                const BreakdownRenderedLoop b = renderBreakdownPhase(id, phase);
                CHECK(a.pad.pitchOffsets == b.pad.pitchOffsets);
                CHECK(a.pad.gateLengthSteps == b.pad.gateLengthSteps);
                CHECK(a.kick.muted == b.kick.muted);
                CHECK(a.bassMuted == b.bassMuted);
            }
        }
    }

    // ---- Property 7: DROP behavior remains unchanged - drumMotif/
    // bassMotif from generateMusicIdentity, and Drop-mode rendering, are
    // untouched by this pass (BreakdownArrangement.cpp never calls into
    // DrumEngine.cpp/BassEngine.cpp/MusicIdentity.cpp's generation
    // functions, and those files were not edited). Checked here as
    // byte-identity across two independent generateMusicIdentity calls
    // for the same seed (same guarantee test_music_identity.cpp already
    // establishes - re-asserted here so this test file stands on its own
    // as proof DROP wasn't touched by anything added in this pass). All 6
    // roles checked (not just kick/clap) - the stage-arc-compression fix
    // (see CompactLoop tests below) only ever READS identity.drumMotif, so
    // generateDrop()'s own output for every role must stay exactly as
    // deterministic as before. ----
    {
        for (uint32_t seed = 1; seed < 20; ++seed)
        {
            const MusicIdentity a = makeIdentity(seed);
            const MusicIdentity b = makeIdentity(seed);
            CHECK(stepArraysEqual(a.drumMotif.kick, b.drumMotif.kick));
            CHECK(stepArraysEqual(a.drumMotif.clap, b.drumMotif.clap));
            CHECK(stepArraysEqual(a.drumMotif.hatClosed, b.drumMotif.hatClosed));
            CHECK(stepArraysEqual(a.drumMotif.hatOpen, b.drumMotif.hatOpen));
            CHECK(stepArraysEqual(a.drumMotif.percA, b.drumMotif.percA));
            CHECK(stepArraysEqual(a.drumMotif.percB, b.drumMotif.percB));
            CHECK(a.bassMotif == b.bassMotif);
        }
    }

    // ---- Structural: the bar/phase state machine itself ----
    {
        CHECK(totalBreakdownBars() == kBreakdownEntryBars + kBreakdownBodyBars + kPreDropBars);
        CHECK(phaseForBar(0) == BreakdownPhase::Entry);
        CHECK(phaseForBar(kBreakdownEntryBars - 1) == BreakdownPhase::Entry);
        CHECK(phaseForBar(kBreakdownEntryBars) == BreakdownPhase::Body);
        CHECK(phaseForBar(kBreakdownEntryBars + kBreakdownBodyBars - 1) == BreakdownPhase::Body);
        CHECK(phaseForBar(kBreakdownEntryBars + kBreakdownBodyBars) == BreakdownPhase::PreDrop);
        CHECK(phaseForBar(totalBreakdownBars() - 1) == BreakdownPhase::PreDrop);
        CHECK(barsInPhase(BreakdownPhase::Entry) == kBreakdownEntryBars);
        CHECK(barsInPhase(BreakdownPhase::Body) == kBreakdownBodyBars);
        CHECK(barsInPhase(BreakdownPhase::PreDrop) == kPreDropBars);
    }

    // ==================================================================
    // CompactLoop - the actual 16-bar "hear the whole arc on one Generate
    // click" loop. Every property below maps directly to the user's own
    // section-12 test list.
    // ==================================================================

    // ---- compactSectionForBar boundaries ----
    {
        CHECK(compactSectionForBar(0) == CompactSection::Drop);
        CHECK(compactSectionForBar(kCompactDropBars - 1) == CompactSection::Drop);
        CHECK(compactSectionForBar(kCompactDropBars) == CompactSection::BreakEntry);
        CHECK(compactSectionForBar(kCompactDropBars + kCompactEntryBars) == CompactSection::BreakBody);
        CHECK(compactSectionForBar(kCompactDropBars + kCompactEntryBars + kCompactBodyBars - 1) == CompactSection::BreakBody);
        CHECK(compactSectionForBar(kCompactDropBars + kCompactEntryBars + kCompactBodyBars) == CompactSection::PreDrop);
        CHECK(compactSectionForBar(kCompactLoopBars - 1) == CompactSection::PreDrop);
        // Wraps correctly for out-of-range/negative bars (matches the same
        // wrap idiom PluginProcessor's own step math already uses).
        CHECK(compactSectionForBar(kCompactLoopBars) == CompactSection::Drop);
        CHECK(compactSectionForBar(-1) == CompactSection::PreDrop);
    }

    // ---- Property: DROP span composition - the stage-arc-compression fix
    // (MLPipeline/musical_target/sound_and_rhythm_diagnostic_pass4.md
    // section 2/7). Kick/clap/bass are still byte-identical to
    // identity.drumMotif/bassMotif's own literal bars 0-7 (they're
    // structurally bar-invariant already - kick fires every bar
    // identically, clap is one canonical shape tiled across all 16 bars -
    // so remapping which source bar feeds them is a no-op by construction,
    // and this asserts that stays true). hatClosed/hatOpen/percA/percB are
    // NO LONGER identity.drumMotif's raw bars 0-7 - they're now
    // COMPRESSED from all 4 of generateDrop()'s own energy stages, 2 bars
    // per stage, sampled from generateDrop()'s own bars {0,1, 4,5, 8,9,
    // 12,13} (Establish/Develop/Increase/FullDrop respectively) into the
    // compact loop's bars {0,1, 2,3, 4,5, 6,7}. Every value is still
    // literal, real, byte-identical content from generateDrop()'s own
    // output - just re-composed, never a new random/probability decision. ----
    {
        const int stepsPerBar = kBreakdownStepsPerBar; // 16
        // destBar -> sourceBar, matching BreakdownArrangement.cpp's own
        // kCompressedSourceBar table (duplicated here deliberately - a
        // test should assert the OBSERVABLE contract, not import the
        // production constant and trivially agree with itself).
        const int destToSourceBar[kCompactDropBars] = { 0, 1, 4, 5, 8, 9, 12, 13 };

        for (uint32_t seed = 1; seed < 30; ++seed)
        {
            const MusicIdentity id = makeIdentity(seed);
            const CompactLoop loop = generateCompactLoop(id);

            // Kick/clap/bass: still literal bars 0-7, unchanged.
            for (int s = 0; s < kCompactDropBars * stepsPerBar; ++s)
            {
                CHECK(loop.drum.kick[(size_t) s].active == id.drumMotif.kick[(size_t) s].active);
                CHECK(loop.drum.kick[(size_t) s].velocity == id.drumMotif.kick[(size_t) s].velocity);
                CHECK(loop.drum.clap[(size_t) s].active == id.drumMotif.clap[(size_t) s].active);
                CHECK(loop.drum.clap[(size_t) s].velocity == id.drumMotif.clap[(size_t) s].velocity);
                CHECK(loop.bass[(size_t) s] == id.bassMotif[(size_t) s]);
            }

            // hatClosed/hatOpen/percA/percB: compressed - each destination
            // bar equals the mapped source bar of generateDrop()'s own
            // output, exactly.
            for (int destBar = 0; destBar < kCompactDropBars; ++destBar)
            {
                const int srcBar = destToSourceBar[destBar];
                for (int s = 0; s < stepsPerBar; ++s)
                {
                    const int destIdx = destBar * stepsPerBar + s;
                    const int srcIdx  = srcBar  * stepsPerBar + s;
                    CHECK(loop.drum.hatClosed[(size_t) destIdx].active   == id.drumMotif.hatClosed[(size_t) srcIdx].active);
                    CHECK(loop.drum.hatClosed[(size_t) destIdx].velocity == id.drumMotif.hatClosed[(size_t) srcIdx].velocity);
                    CHECK(loop.drum.hatOpen[(size_t) destIdx].active     == id.drumMotif.hatOpen[(size_t) srcIdx].active);
                    CHECK(loop.drum.percA[(size_t) destIdx].active       == id.drumMotif.percA[(size_t) srcIdx].active);
                    CHECK(loop.drum.percB[(size_t) destIdx].active       == id.drumMotif.percB[(size_t) srcIdx].active);
                }
            }
            CHECK(loop.drum.kick.size() == id.drumMotif.kick.size());
        }
    }

    // ==================================================================
    // Section-12 (revised) explicit ask: prove each of the compact loop's
    // 8 Drop bars maps to the correct generateDrop() energy stage, and
    // that FullDrop is measurably denser/stronger than Establish where the
    // existing grammar intends it to be - user-facing bar numbers (1-8),
    // matching the approved "compress into bars 1-8" fix.
    // ==================================================================
    {
        auto barActive = [](const StepArray& s, int bar, int stepsPerBar)
        {
            int n = 0;
            for (int i = 0; i < stepsPerBar; ++i)
                if (s[(size_t) (bar * stepsPerBar + i)].active) ++n;
            return n;
        };

        // 1. Bars 1-2 (compact loop bars 0-1) correspond to Establish -
        //    equal to generateDrop()'s own bars 0-1.
        // 2. Bars 3-4 (compact loop bars 2-3) correspond to Develop -
        //    equal to generateDrop()'s own bars 4-5.
        // 3. Bars 5-6 (compact loop bars 4-5) correspond to Increase -
        //    equal to generateDrop()'s own bars 8-9.
        // 4. Bars 7-8 (compact loop bars 6-7) correspond to FullDrop -
        //    equal to generateDrop()'s own bars 12-13.
        {
            const MusicIdentity id = makeIdentity(42);
            const CompactLoop loop = generateCompactLoop(id);
            const int spb = kBreakdownStepsPerBar;

            auto barEqual = [&](int destBar, int srcBar)
            {
                for (int s = 0; s < spb; ++s)
                {
                    const int d = destBar * spb + s, sBar = srcBar * spb + s;
                    if (loop.drum.hatClosed[(size_t) d].active != id.drumMotif.hatClosed[(size_t) sBar].active) return false;
                    if (loop.drum.percA[(size_t) d].active     != id.drumMotif.percA[(size_t) sBar].active)     return false;
                }
                return true;
            };

            CHECK(barEqual(0, 0) && barEqual(1, 1));   // bars 1-2 = Establish (drumMotif bars 0-1)
            CHECK(barEqual(2, 4) && barEqual(3, 5));   // bars 3-4 = Develop   (drumMotif bars 4-5)
            CHECK(barEqual(4, 8) && barEqual(5, 9));   // bars 5-6 = Increase  (drumMotif bars 8-9)
            CHECK(barEqual(6, 12) && barEqual(7, 13)); // bars 7-8 = FullDrop  (drumMotif bars 12-13)
        }

        // 5. FullDrop (bars 7-8) is measurably denser/stronger than
        //    Establish (bars 1-2) - averaged over many seeds (per-seed
        //    hat/perc content varies, the AVERAGE relationship is what the
        //    StageEnergy table (kFullDrop >> kEstablish for
        //    hatClosed/percA/percB) actually guarantees).
        {
            int64_t establishTotal = 0, fullDropTotal = 0;
            const int seeds = 60;
            for (uint32_t seed = 1; seed <= (uint32_t) seeds; ++seed)
            {
                const MusicIdentity id = makeIdentity(seed);
                const CompactLoop loop = generateCompactLoop(id);
                const int spb = kBreakdownStepsPerBar;

                establishTotal += barActive(loop.drum.hatClosed, 0, spb) + barActive(loop.drum.hatClosed, 1, spb)
                                + barActive(loop.drum.percA,     0, spb) + barActive(loop.drum.percA,     1, spb)
                                + barActive(loop.drum.percB,     0, spb) + barActive(loop.drum.percB,     1, spb);
                fullDropTotal  += barActive(loop.drum.hatClosed, 6, spb) + barActive(loop.drum.hatClosed, 7, spb)
                                + barActive(loop.drum.percA,     6, spb) + barActive(loop.drum.percA,     7, spb)
                                + barActive(loop.drum.percB,     6, spb) + barActive(loop.drum.percB,     7, spb);
            }
            std::printf("Establish (bars1-2) hat+perc total over %d seeds: %lld\n", seeds, (long long) establishTotal);
            std::printf("FullDrop  (bars7-8) hat+perc total over %d seeds: %lld\n", seeds, (long long) fullDropTotal);
            CHECK(fullDropTotal > establishTotal); // the played loop now actually reaches full-drop energy
        }
    }

    // ---- Property: kick/sub remain absent during the breakdown span
    // (bars 8-15) - every muted-role velocity is exactly zero there. ----
    {
        const int breakdownStart = kCompactDropBars * kBreakdownStepsPerBar; // 128
        const int totalSteps = kCompactLoopBars * kBreakdownStepsPerBar;     // 256
        for (uint32_t seed = 1; seed < 30; ++seed)
        {
            const MusicIdentity id = makeIdentity(seed);
            const CompactLoop loop = generateCompactLoop(id);

            for (int s = breakdownStart; s < totalSteps; ++s)
            {
                CHECK(loop.drum.kick[(size_t) s].active == false);
                CHECK(loop.drum.hatClosed[(size_t) s].active == false);
                CHECK(loop.drum.hatOpen[(size_t) s].active == false);
                CHECK(loop.drum.percA[(size_t) s].active == false);
                CHECK(loop.drum.percB[(size_t) s].active == false);
                CHECK(loop.bass[(size_t) s] == kBassOffValue);
            }
        }
    }

    // ---- Property: breakdown introduces dedicated melodic material -
    // the pad has real content ONLY in bars 8-15, silent in bars 0-7
    // (where the Drop groove itself is the focus). ----
    {
        const int breakdownStart = kCompactDropBars * kBreakdownStepsPerBar;
        for (uint32_t seed = 1; seed < 30; ++seed)
        {
            const MusicIdentity id = makeIdentity(seed);
            const CompactLoop loop = generateCompactLoop(id);

            for (int s = 0; s < breakdownStart; ++s)
                CHECK(loop.pad.pitchOffsets[(size_t) s] == kPadOffValue);

            int activeInBreakdown = 0;
            for (int s = breakdownStart; s < (int) loop.pad.pitchOffsets.size(); ++s)
                if (loop.pad.pitchOffsets[(size_t) s] != kPadOffValue)
                    ++activeInBreakdown;
            CHECK(activeInBreakdown > 0);
        }
    }

    // ---- Property: same generated loop is repeatable when the loop
    // restarts - generateCompactLoop called twice on the same identity
    // produces byte-identical output. ----
    {
        for (uint32_t seed = 1; seed < 20; ++seed)
        {
            const MusicIdentity id = makeIdentity(seed);
            const CompactLoop a = generateCompactLoop(id);
            const CompactLoop b = generateCompactLoop(id);
            CHECK(stepArraysEqual(a.drum.kick, b.drum.kick));
            CHECK(stepArraysEqual(a.drum.clap, b.drum.clap));
            CHECK(stepArraysEqual(a.drum.hatClosed, b.drum.hatClosed));
            CHECK(stepArraysEqual(a.drum.hatOpen, b.drum.hatOpen));
            CHECK(stepArraysEqual(a.drum.percA, b.drum.percA));
            CHECK(stepArraysEqual(a.drum.percB, b.drum.percB));
            CHECK(a.bass == b.bass);
            CHECK(a.pad.pitchOffsets == b.pad.pitchOffsets);
            CHECK(a.pad.gateLengthSteps == b.pad.gateLengthSteps);
        }
    }

    // ---- Property: array lengths are exactly 256 steps (16 bars) -
    // matches kLoopTotalSteps, so this drops straight into the existing
    // setGeneratedDrumPattern/setGeneratedMelodyPattern step-wrap math
    // with no length mismatch. ----
    {
        const MusicIdentity id = makeIdentity(7);
        const CompactLoop loop = generateCompactLoop(id);
        CHECK((int) loop.drum.kick.size() == kLoopTotalSteps);
        CHECK((int) loop.bass.size() == kLoopTotalSteps);
        CHECK((int) loop.bassGateLengthSteps.size() == kLoopTotalSteps);
        CHECK((int) loop.pad.pitchOffsets.size() == kLoopTotalSteps);
        CHECK((int) loop.pad.gateLengthSteps.size() == kLoopTotalSteps);
    }

    // ==================================================================
    // Section-12 explicit ask: "calculate DROP density, BREAKDOWN density,
    // PRE_DROP density and verify the musical energy arc" - a real,
    // printed, asserted density-per-section measurement across many
    // seeds, not just the earlier all-zero/non-zero checks.
    // ==================================================================
    {
        auto countActive = [](const StepArray& s, int fromStep, int toStep)
        {
            int n = 0;
            for (int i = fromStep; i < toStep; ++i)
                if (s[(size_t) i].active) ++n;
            return n;
        };
        auto countActiveInt8 = [](const std::vector<int8_t>& s, int8_t off, int fromStep, int toStep)
        {
            int n = 0;
            for (int i = fromStep; i < toStep; ++i)
                if (s[(size_t) i] != off) ++n;
            return n;
        };

        const int dropStart = 0, dropEnd = kCompactDropBars * kBreakdownStepsPerBar;                       // 0-127
        const int breakdownStart = dropEnd, breakdownEnd = breakdownStart + (kCompactEntryBars + kCompactBodyBars) * kBreakdownStepsPerBar; // 128-191
        const int preDropStart = breakdownEnd, preDropEnd = kLoopTotalSteps;                                // 192-255

        int64_t dropTotal = 0, breakdownTotal = 0, preDropTotal = 0;
        int64_t dropPad = 0, breakdownPad = 0, preDropPad = 0;
        const int seeds = 30;

        for (uint32_t seed = 1; seed <= (uint32_t) seeds; ++seed)
        {
            const MusicIdentity id = makeIdentity(seed);
            const CompactLoop loop = generateCompactLoop(id);

            auto rhythmicDensity = [&](int from, int to)
            {
                return countActive(loop.drum.kick, from, to) + countActive(loop.drum.clap, from, to)
                     + countActive(loop.drum.hatClosed, from, to) + countActive(loop.drum.hatOpen, from, to)
                     + countActive(loop.drum.percA, from, to) + countActive(loop.drum.percB, from, to)
                     + countActiveInt8(loop.bass, kBassOffValue, from, to);
            };

            dropTotal      += rhythmicDensity(dropStart, dropEnd);
            breakdownTotal += rhythmicDensity(breakdownStart, breakdownEnd);
            preDropTotal   += rhythmicDensity(preDropStart, preDropEnd);

            dropPad      += countActiveInt8(loop.pad.pitchOffsets, kPadOffValue, dropStart, dropEnd);
            breakdownPad += countActiveInt8(loop.pad.pitchOffsets, kPadOffValue, breakdownStart, breakdownEnd);
            preDropPad   += countActiveInt8(loop.pad.pitchOffsets, kPadOffValue, preDropStart, preDropEnd);
        }

        const double dropMean      = (double) dropTotal / seeds;
        const double breakdownMean = (double) breakdownTotal / seeds;
        const double preDropMean   = (double) preDropTotal / seeds;

        std::printf("Rhythmic density (kick+clap+hat+perc+bass active steps), mean over %d seeds:\n", seeds);
        std::printf("  DROP (bars 1-%d):      %.1f\n", kCompactDropBars, dropMean);
        std::printf("  BREAKDOWN (bars %d-%d): %.1f\n", kCompactDropBars + 1, kCompactDropBars + kCompactEntryBars + kCompactBodyBars, breakdownMean);
        std::printf("  PRE_DROP (bars %d-%d): %.1f\n", kCompactDropBars + kCompactEntryBars + kCompactBodyBars + 1, kCompactLoopBars, preDropMean);
        std::printf("Pad density (active steps), mean over %d seeds: DROP=%.1f BREAKDOWN=%.1f PRE_DROP=%.1f\n",
                     seeds, (double) dropPad / seeds, (double) breakdownPad / seeds, (double) preDropPad / seeds);

        // The actual musical energy arc this whole pass was built to
        // produce: DROP is clearly the densest rhythmic section (real
        // groove), BREAKDOWN and PRE_DROP are both clearly less dense
        // rhythmically (kick/hat/perc/bass muted - clap alone can't close
        // more than a fraction of the gap), while the PAD is exactly
        // inverted (silent in DROP, present only in BREAKDOWN/PRE_DROP).
        CHECK(dropMean > breakdownMean * 2.0);   // "substantially less rhythmic activity"
        CHECK(dropMean > preDropMean * 2.0);
        CHECK(dropPad == 0);                     // pad never sounds during DROP
        CHECK(breakdownPad > 0);
        CHECK(preDropPad > 0);
    }

    TEST_SUMMARY_AND_EXIT();
}
