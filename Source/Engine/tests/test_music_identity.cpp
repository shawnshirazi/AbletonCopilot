// Regression tests for Engine::MusicIdentity/renderMode - the shared
// "generate once, render two ways" core of the 16-bar loop workflow.
// Covers the structural (non-audio) half of the section-16 test list from
// the loop-generator brief: loop length, determinism, per-mode kick mute,
// and the "switching modes never touches the underlying pattern" guarantee
// (checked here as literal byte-identity, not just "looks unchanged").
#include "../MusicIdentity.h"
#include "../BassArchetype.h"
#include "TestSupport.h"

using namespace Engine;

namespace
{
    bool stepArraysEqual(const StepArray& a, const StepArray& b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (a[i].active != b[i].active || a[i].velocity != b[i].velocity)
                return false;
        return true;
    }

    bool dropPatternsEqual(const DropPattern& a, const DropPattern& b)
    {
        return stepArraysEqual(a.kick, b.kick) && stepArraysEqual(a.clap, b.clap)
            && stepArraysEqual(a.hatClosed, b.hatClosed) && stepArraysEqual(a.hatOpen, b.hatOpen)
            && stepArraysEqual(a.percA, b.percA) && stepArraysEqual(a.percB, b.percB);
    }

    int countActive(const StepArray& s)
    {
        int n = 0;
        for (auto& h : s)
            if (h.active) ++n;
        return n;
    }
}

int main()
{
    MusicIdentityParams params;
    params.seed     = 12345;
    params.bpm      = 124.0;
    params.rootNote = 9; // A
    params.isMinor  = true;

    const MusicIdentity identity = generateMusicIdentity(params);

    // ---- Loop length: exactly 16 bars (256 steps) ----
    CHECK((int) identity.drumMotif.kick.size() == kLoopTotalSteps);
    CHECK((int) identity.drumMotif.clap.size() == kLoopTotalSteps);
    CHECK((int) identity.drumMotif.hatClosed.size() == kLoopTotalSteps);
    CHECK((int) identity.drumMotif.hatOpen.size() == kLoopTotalSteps);
    CHECK((int) identity.drumMotif.percA.size() == kLoopTotalSteps);
    CHECK((int) identity.drumMotif.percB.size() == kLoopTotalSteps);
    CHECK((int) identity.bassMotif.size() == kLoopTotalSteps);
    CHECK(kLoopTotalSteps == 256);
    CHECK(kLoopBars == 16);

    // ---- Deterministic: same seed -> identical identity ----
    const MusicIdentity identityAgain = generateMusicIdentity(params);
    CHECK(dropPatternsEqual(identity.drumMotif, identityAgain.drumMotif));
    CHECK(identity.bassMotif == identityAgain.bassMotif);

    // A different seed must be able to produce a different result (not a
    // constant-output bug hiding behind the determinism check above).
    MusicIdentityParams differentSeedParams = params;
    differentSeedParams.seed = 999999;
    const MusicIdentity identityDifferentSeed = generateMusicIdentity(differentSeedParams);
    CHECK(!dropPatternsEqual(identity.drumMotif, identityDifferentSeed.drumMotif)
          || identity.bassMotif != identityDifferentSeed.bassMotif);

    // ---- Bass active from bar 1 (steps 0-15) - "bass should NOT wait" ----
    {
        int activeInBar0 = 0;
        for (int i = 0; i < kLoopStepsPerBar; ++i)
            if (identity.bassMotif[(size_t) i] != kBassOffValue)
                ++activeInBar0;
        CHECK(activeInBar0 > 0);
    }

    // ---- Kick is a real four-on-the-floor pattern (sanity - generateDrop
    // is reused unmodified, this just proves the wiring is correct) ----
    CHECK(countActive(identity.drumMotif.kick) > 0);

    // ---- renderMode: DROP keeps everything unmuted, full gain ----
    const RenderedLoop drop = renderMode(identity, RenderMode::Drop);
    CHECK(!drop.kick.muted && drop.kick.gain == 1.0f);
    CHECK(!drop.clap.muted && !drop.hatClosed.muted && !drop.hatOpen.muted);
    CHECK(!drop.percA.muted && !drop.percB.muted);
    CHECK(!drop.bassMuted);

    // ---- renderMode: BREAKDOWN mutes kick/hatOpen/percB, reduces
    // hatClosed/percA, mutes bass ----
    const RenderedLoop breakdown = renderMode(identity, RenderMode::Breakdown);
    CHECK(breakdown.kick.muted);
    CHECK(breakdown.hatOpen.muted);
    CHECK(breakdown.percB.muted);
    CHECK(!breakdown.hatClosed.muted && breakdown.hatClosed.gain < 1.0f && breakdown.hatClosed.gain > 0.0f);
    CHECK(!breakdown.percA.muted && breakdown.percA.gain < 1.0f && breakdown.percA.gain > 0.0f);
    CHECK(breakdown.bassMuted);

    // ---- Underlying pattern is preserved: switching modes never
    // regenerates or thins the actual drum/bass data, only the RoleMix
    // suggestions differ. This is the literal "switching modes does not
    // regenerate motifs" / "underlying kick pattern is preserved" check. ----
    CHECK(dropPatternsEqual(drop.drum, breakdown.drum));
    CHECK(drop.bass == breakdown.bass);
    CHECK(dropPatternsEqual(drop.drum, identity.drumMotif));
    CHECK(drop.bass == identity.bassMotif);

    // Calling renderMode repeatedly must never mutate identity itself or
    // drift between calls (pure function, no hidden state).
    const RenderedLoop dropAgain = renderMode(identity, RenderMode::Drop);
    CHECK(dropPatternsEqual(drop.drum, dropAgain.drum));
    CHECK(drop.bass == dropAgain.bass);

    // ---- Bass phrase development across the 16-bar loop (Part 5/1 of the
    // groove-improvement pass): bars 9-16 must NOT be a byte-identical
    // copy of bars 1-8 - proves generateBassLoop16's real 4-block chain
    // replaced the old blind "tile the first 8 bars twice" behaviour. ----
    {
        constexpr int kBarSteps = kLoopStepsPerBar * 8; // 128 - first/second half of the 16-bar loop
        bool anySeedDiffers = false;
        for (uint32_t seed = 100; seed < 130; ++seed)
        {
            MusicIdentityParams p;
            p.seed = seed; p.bpm = 124.0; p.rootNote = 9; p.isMinor = true;
            const MusicIdentity id2 = generateMusicIdentity(p);
            bool identicalHalves = true;
            for (int i = 0; i < kBarSteps; ++i)
                if (id2.bassMotif[(size_t) i] != id2.bassMotif[(size_t) (kBarSteps + i)])
                    { identicalHalves = false; break; }
            if (!identicalHalves)
            {
                anySeedDiffers = true;
                break;
            }
        }
        CHECK(anySeedDiffers);
    }

    // ---- Bass density (active-step count) at bars 1-4 vs bars 13-16
    // should, on average across many seeds, be higher in the final block -
    // matches the establish(0.7)->develop(0.85)->increase(1.0)->full(1.0)
    // density ramp generateBassLoop16 applies (mirroring DrumEngine's own
    // already-documented 4-stage energy arc), aggregated because any
    // single seed's random draw can go either way even with a real
    // upward-biased ramp. ----
    {
        constexpr int kBlockSteps = kLoopStepsPerBar * 4; // 64 = 4 bars
        long totalFirstBlockActive = 0, totalLastBlockActive = 0;
        constexpr int kNumSeeds = 40;
        for (uint32_t seed = 200; seed < (uint32_t) (200 + kNumSeeds); ++seed)
        {
            MusicIdentityParams p;
            p.seed = seed; p.bpm = 124.0; p.rootNote = 9; p.isMinor = true;
            const MusicIdentity id2 = generateMusicIdentity(p);
            for (int i = 0; i < kBlockSteps; ++i)
            {
                if (id2.bassMotif[(size_t) i] != kBassOffValue) ++totalFirstBlockActive;
                if (id2.bassMotif[(size_t) (3 * kBlockSteps + i)] != kBassOffValue) ++totalLastBlockActive;
            }
        }
        std::printf("bass active steps: bars1-4 total=%ld bars13-16 total=%ld (over %d seeds)\n",
                     totalFirstBlockActive, totalLastBlockActive, kNumSeeds);
        CHECK(totalLastBlockActive > totalFirstBlockActive);
    }

    // ---- Bass archetype (Part 1 of the archetype-groove pass): bar 0 of
    // generateMusicIdentity's bassMotif must be EXACTLY the archetype
    // selectBassArchetype(seed) picked - proves the real corpus-
    // transcribed shape is actually used to build the identity, not just
    // defined in BassArchetype.h and left unused. Checked across many
    // seeds/archetypes, not one anecdotal example. ----
    {
        for (uint32_t seed = 300; seed < 340; ++seed)
        {
            MusicIdentityParams p;
            p.seed = seed; p.bpm = 124.0; p.rootNote = 9; p.isMinor = true;
            const MusicIdentity id2 = generateMusicIdentity(p);

            const BassArchetype expectedArchetype = selectBassArchetype(seed);
            const ArchetypeBar expectedBar = instantiateArchetypeBar(expectedArchetype);

            for (int s = 0; s < kLoopStepsPerBar; ++s)
            {
                CHECK(id2.bassMotif[(size_t) s] == expectedBar.pitchOffsets[(size_t) s]);
                CHECK(id2.bassGateLengthSteps[(size_t) s] == expectedBar.gateLengthSteps[(size_t) s]);
            }
        }
    }

    // ---- Gate-length data is non-zero only where bassMotif actually has
    // an onset - never a gate value at a rest, never a missing gate at a
    // real onset (checked across the whole 256-step loop, all 4 blocks,
    // not just bar 0). ----
    {
        for (uint32_t seed = 400; seed < 420; ++seed)
        {
            MusicIdentityParams p;
            p.seed = seed; p.bpm = 124.0; p.rootNote = 9; p.isMinor = true;
            const MusicIdentity id2 = generateMusicIdentity(p);
            CHECK(id2.bassGateLengthSteps.size() == id2.bassMotif.size());
            for (size_t i = 0; i < id2.bassMotif.size(); ++i)
            {
                const bool hasOnset = id2.bassMotif[i] != kBassOffValue;
                const bool hasGate  = id2.bassGateLengthSteps[i] != 0;
                CHECK(hasOnset == hasGate);
            }
        }
    }

    // ---- renderMode's bassGateLengthSteps is byte-identical across
    // DROP/BREAKDOWN too - the same "never regenerated by a mode switch"
    // guarantee bass/drum data already has. ----
    {
        MusicIdentityParams p;
        p.seed = 500; p.bpm = 124.0; p.rootNote = 9; p.isMinor = true;
        const MusicIdentity id2 = generateMusicIdentity(p);
        const RenderedLoop drop2      = renderMode(id2, RenderMode::Drop);
        const RenderedLoop breakdown2 = renderMode(id2, RenderMode::Breakdown);
        CHECK(drop2.bassGateLengthSteps == breakdown2.bassGateLengthSteps);
        CHECK(drop2.bassGateLengthSteps == id2.bassGateLengthSteps);
    }

    TEST_SUMMARY_AND_EXIT();
}
