// Regression tests for Engine::MusicIdentity/renderMode - the shared
// "generate once, render two ways" core of the 16-bar loop workflow.
// Covers the structural (non-audio) half of the section-16 test list from
// the loop-generator brief: loop length, determinism, per-mode kick mute,
// and the "switching modes never touches the underlying pattern" guarantee
// (checked here as literal byte-identity, not just "looks unchanged").
#include "../MusicIdentity.h"
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

    TEST_SUMMARY_AND_EXIT();
}
