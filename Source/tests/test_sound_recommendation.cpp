// Regression tests for Source/SoundRecommendation.h - the evidence-
// grounded "what kind of Serum 2 sound fits this role" recommendation
// engine. JUCE-dependent (juce::String/juce::File), same build technique
// as test_serum_preset_status.cpp/test_rack_classification.cpp (link
// against compiled juce_core/juce_audio_basics translation units - no
// full AbletonCopilotAudioProcessor needed, unlike test_independent_
// tracks.cpp's own recipe).
#include "../SoundRecommendation.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstdint>

static int g_checksRun = 0;
static int g_checksFailed = 0;

#define CHECK(cond) \
    do { \
        ++g_checksRun; \
        if (!(cond)) { \
            ++g_checksFailed; \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

int main()
{
    using namespace SoundRecommendation;

    // ---- analyzePattern: known synthetic arrays produce the expected
    // density/avgHoldSteps - the foundation everything else depends on. ----
    {
        // Dense: onset every step (16/16) - hold length must be exactly 1.
        std::vector<int8_t> everyStep(16, 0);
        const auto s1 = analyzePattern(everyStep, (int8_t) -128);
        CHECK(s1.hasNotes);
        CHECK(s1.density > 0.99f && s1.density < 1.01f);
        CHECK(s1.avgHoldSteps > 0.99f && s1.avgHoldSteps < 1.01f);

        // Sparse: exactly 2 onsets in a 16-step loop, evenly spaced (steps
        // 0 and 8) - hold length must average exactly 8.
        std::vector<int8_t> sparse(16, -128);
        sparse[0] = 0; sparse[8] = 3;
        const auto s2 = analyzePattern(sparse, (int8_t) -128);
        CHECK(s2.hasNotes);
        CHECK(s2.density > 0.12f && s2.density < 0.13f); // 2/16
        CHECK(s2.avgHoldSteps > 7.99f && s2.avgHoldSteps < 8.01f);

        // Totally empty - hasNotes must be false, not a divide-by-zero or
        // a fabricated non-zero density.
        std::vector<int8_t> empty(16, -128);
        const auto s3 = analyzePattern(empty, (int8_t) -128);
        CHECK(!s3.hasNotes);
        CHECK(s3.density == 0.0f);

        // Empty container entirely (size 0) - must not crash, must report
        // no notes.
        std::vector<int8_t> zeroLen;
        const auto s4 = analyzePattern(zeroLen, (int8_t) -128);
        CHECK(!s4.hasNotes);
    }

    // ---- listRealPresetCandidates never fabricates: a nonexistent folder
    // returns an empty list, not invented names. ----
    {
        auto names = listRealPresetCandidates(juce::File("/definitely/does/not/exist/anywhere"));
        CHECK(names.isEmpty());
    }

    // ---- listRealPresetCandidates against a real, populated fixture
    // folder this test creates itself - deterministic regardless of
    // whether Xfer's own Serum 2 library happens to be installed on the
    // machine running this test. ----
    juce::File fixtureRoot;
    {
        fixtureRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("AbletonCopilot_sound_recommendation_test_"
                                        + juce::String(juce::Random::getSystemRandom().nextInt(1000000)));
        auto bassReese = fixtureRoot.getChildFile("Bass").getChildFile("Reese");
        bassReese.createDirectory();
        bassReese.getChildFile("BA - Fake Reese One.SerumPreset").create();
        bassReese.getChildFile("BA - Fake Reese Two.SerumPreset").create();
        bassReese.getChildFile("not_a_preset.txt").create(); // must be excluded (wrong extension)

        auto names = listRealPresetCandidates(bassReese);
        CHECK(names.size() == 2);
        CHECK(names.contains("BA - Fake Reese One"));
        CHECK(names.contains("BA - Fake Reese Two"));
        for (auto& n : names)
            CHECK(!n.contains(".SerumPreset")); // extension stripped for display

        // Cap respected - maxCount=1 must return exactly 1, not 2.
        CHECK(listRealPresetCandidates(bassReese, 1).size() == 1);
    }

    // ---- 2/3. Bass recommendation != Melody recommendation for the
    // IDENTICAL PatternStats input - proves ROLE (not data) drives the
    // difference, exactly what "recommendation understands the musical
    // role" requires. ----
    {
        PatternStats sharedStats;
        sharedStats.hasNotes     = true;
        sharedStats.density      = 0.25f;
        sharedStats.avgHoldSteps = 4.0f;

        const auto bassRec   = recommend(Role::Bass,   sharedStats, fixtureRoot);
        const auto melodyRec = recommend(Role::Melody, sharedStats, fixtureRoot);
        CHECK(bassRec.character != melodyRec.character);
        CHECK(bassRec.roleText  != melodyRec.roleText);
        // Melody must NOT default to a bass-type patch (explicit
        // requirement) - its character/candidate folder must never
        // mention "Bass".
        CHECK(!melodyRec.character.containsIgnoreCase("bass"));
        CHECK(!melodyRec.candidateFolderHint.containsIgnoreCase("bass"));
    }

    // ---- Recommendation changes appropriately based on measured MIDI
    // characteristics - dense/short-hold vs sparse/long-hold produce
    // genuinely different character text for the SAME role. ----
    {
        PatternStats dense;
        dense.hasNotes     = true;
        dense.density      = 0.5f;
        dense.avgHoldSteps = 1.5f;

        PatternStats sparse;
        sparse.hasNotes     = true;
        sparse.density      = 0.1f;
        sparse.avgHoldSteps = 10.0f;

        const auto bassDense  = recommend(Role::Bass, dense, fixtureRoot);
        const auto bassSparse = recommend(Role::Bass, sparse, fixtureRoot);
        CHECK(bassDense.character != bassSparse.character);
        CHECK(bassDense.why != bassSparse.why);

        const auto melodyDense  = recommend(Role::Melody, dense, fixtureRoot);
        const auto melodySparse = recommend(Role::Melody, sparse, fixtureRoot);
        CHECK(melodyDense.character != melodySparse.character);
        CHECK(melodyDense.why != melodySparse.why);
    }

    // ---- Pad's recommendation is static this phase regardless of stats -
    // explicit, documented behaviour (no pad generator/sound-generation
    // work this pass), not an oversight. ----
    {
        PatternStats a; a.hasNotes = false;
        PatternStats b; b.hasNotes = true; b.density = 0.9f; b.avgHoldSteps = 1.0f;
        const auto padA = recommend(Role::Pad, a, fixtureRoot);
        const auto padB = recommend(Role::Pad, b, fixtureRoot);
        CHECK(padA.character == padB.character);
        CHECK(padA.roleText  == padB.roleText);
    }

    // ---- Candidates are real (from the fixture folder) when the folder
    // exists, and the folder hint is always present even when it doesn't -
    // never a blank, never a fabricated name. ----
    {
        PatternStats stats; // hasNotes=false -> base character branch
        const auto bassRec = recommend(Role::Bass, stats, fixtureRoot);
        CHECK(bassRec.candidateFolderHint.isNotEmpty());
        CHECK(bassRec.candidateNames.size() == 2); // the 2 real fixture files created above
        for (auto& n : bassRec.candidateNames)
            CHECK(n.startsWith("BA - Fake Reese")); // genuinely the fixture's own real names, not invented

        // A root with nothing under it at all - candidates empty, hint
        // still present (honest manual-browse guidance).
        const auto emptyRootRec = recommend(Role::Melody, stats, juce::File("/definitely/does/not/exist/anywhere"));
        CHECK(emptyRootRec.candidateNames.isEmpty());
        CHECK(emptyRootRec.candidateFolderHint.isNotEmpty());
    }

    fixtureRoot.deleteRecursively();

    std::printf("%d/%d checks passed\n", g_checksRun - g_checksFailed, g_checksRun);
    return g_checksFailed == 0 ? 0 : 1;
}
