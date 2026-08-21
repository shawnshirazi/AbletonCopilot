// Regression tests for Source/SoundDnaLibrary.h/.cpp and
// Source/SoundLibraryLearner.h's real, on-disk discoverCandidates - the
// Sound DNA library's own pure/deterministic logic (JSON round-trip,
// fingerprinting, analysis, label derivation). JUCE-dependent, same
// lightweight build technique as test_sound_recommendation.cpp/
// test_candidate_evaluator.cpp.
#include "../SoundDnaLibrary.h"
#include "../SoundLibraryLearner.h"
#include "../SerumAutomation.h"
#include "../SoundRecommendation.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>

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
    // ---- fingerprintForFile: deterministic (same file -> same
    // fingerprint every call) and changes if the file's content/size
    // changes - the exact "invalidate if source changed" contract. ----
    {
        auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                           .getChildFile("AbletonCopilot_sound_dna_test_" + juce::String(juce::Random::getSystemRandom().nextInt(1000000)));
        tempDir.createDirectory();
        auto f = tempDir.getChildFile("fake.SerumPreset");
        f.replaceWithText("hello world");

        const auto fp1 = SoundDna::fingerprintForFile(f);
        const auto fp2 = SoundDna::fingerprintForFile(f);
        CHECK(fp1 == fp2);
        CHECK(fp1.isNotEmpty());

        f.replaceWithText("different content entirely");
        const auto fp3 = SoundDna::fingerprintForFile(f);
        CHECK(fp3 != fp1);

        // makeId is a deterministic, shorter derivation of the fingerprint.
        CHECK(SoundDna::makeId(fp1) == SoundDna::makeId(fp1));
        CHECK(SoundDna::makeId(fp1).isNotEmpty());

        tempDir.deleteRecursively();
    }

    // ---- fingerprintForCapturedState: deterministic for identical bytes,
    // different for different bytes. ----
    {
        juce::MemoryBlock a; a.append("abc", 3);
        juce::MemoryBlock b; b.append("abc", 3);
        juce::MemoryBlock c; c.append("xyz", 3);
        CHECK(SoundDna::fingerprintForCapturedState(a) == SoundDna::fingerprintForCapturedState(b));
        CHECK(SoundDna::fingerprintForCapturedState(a) != SoundDna::fingerprintForCapturedState(c));
    }

    // ---- toVar/fromVar: full round trip preserves every field,
    // including nested octave/noteLength/velocity results and dnaLabels -
    // this is what makes persistence + resumability (section 15) work at
    // all. ----
    {
        SoundDna::LearnedSound s;
        s.id = "abc123";
        s.displayName = "unknown";
        s.role = "Bass";
        s.sourceHint = "Factory/Bass/Reese/BA - Test.SerumPreset";
        s.sourceFingerprint = "deadbeef";
        s.analysisVersion = 1;
        s.dateLearnedIso = "2026-01-01T00:00:00Z";
        s.capturedStateFile = "abc123.serumstate";
        s.status = SoundDna::LearnStatus::Success;
        s.statusReason = "verified";
        s.dnaLabels.add("dark");
        s.dnaLabels.add("sub-heavy");

        SoundDna::OctaveResult oc;
        oc.label = "C2"; oc.midiPitch = 36;
        oc.measurement.valid = true;
        oc.measurement.spectralCentroidHz = 500.0f;
        oc.measurement.subPct = 0.4f;
        oc.measurement.rms = 0.1f;
        oc.measurement.stereoCorrelation = 0.9f;
        s.octaves.push_back(oc);

        SoundDna::NoteLengthResult nl;
        nl.label = "sustained"; nl.lengthSteps = 16;
        nl.measurement.valid = true;
        nl.releaseTimeMs = 250.5f;
        s.noteLengths.push_back(nl);

        SoundDna::VelocityResult vel;
        vel.label = "hard"; vel.velocity = 110;
        vel.measurement.valid = true;
        vel.measurement.peakDb = -6.0f;
        s.velocities.push_back(vel);

        const auto v = SoundDna::toVar(s);
        const auto back = SoundDna::fromVar(v);

        CHECK(back.id == s.id);
        CHECK(back.displayName == s.displayName);
        CHECK(back.role == s.role);
        CHECK(back.sourceHint == s.sourceHint);
        CHECK(back.sourceFingerprint == s.sourceFingerprint);
        CHECK(back.capturedStateFile == s.capturedStateFile);
        CHECK(back.status == SoundDna::LearnStatus::Success);
        CHECK(back.statusReason == s.statusReason);
        CHECK(back.dnaLabels.size() == 2);
        CHECK(back.dnaLabels.contains("dark"));
        CHECK(back.dnaLabels.contains("sub-heavy"));

        CHECK(back.octaves.size() == 1);
        CHECK(back.octaves[0].label == "C2");
        CHECK(back.octaves[0].midiPitch == 36);
        CHECK(back.octaves[0].measurement.valid);
        CHECK(std::abs(back.octaves[0].measurement.spectralCentroidHz - 500.0f) < 0.01f);
        CHECK(std::abs(back.octaves[0].measurement.subPct - 0.4f) < 0.001f);

        CHECK(back.noteLengths.size() == 1);
        CHECK(back.noteLengths[0].label == "sustained");
        CHECK(std::abs(back.noteLengths[0].releaseTimeMs - 250.5f) < 0.01f);

        CHECK(back.velocities.size() == 1);
        CHECK(back.velocities[0].velocity == 110);
        CHECK(std::abs(back.velocities[0].measurement.peakDb - (-6.0f)) < 0.01f);
    }

    // ---- toString/statusFromString round trip for all three states. ----
    {
        CHECK(SoundDna::statusFromString(SoundDna::toString(SoundDna::LearnStatus::Success)) == SoundDna::LearnStatus::Success);
        CHECK(SoundDna::statusFromString(SoundDna::toString(SoundDna::LearnStatus::Failed)) == SoundDna::LearnStatus::Failed);
        CHECK(SoundDna::statusFromString(SoundDna::toString(SoundDna::LearnStatus::Unknown)) == SoundDna::LearnStatus::Unknown);
        CHECK(SoundDna::statusFromString("garbage") == SoundDna::LearnStatus::Unknown); // never silently promoted to Success
    }

    // ---- saveLibraryIndex/loadLibraryIndex: real file round trip -
    // proves persistence actually works, using a real temp override
    // location is not supported (libraryDir() is fixed), so this test
    // saves real entries, loads them back, and cleans up afterward,
    // restoring whatever was there before (none, in a fresh test env). ----
    {
        std::vector<SoundDna::LearnedSound> before;
        SoundDna::loadLibraryIndex(before); // whatever's really there (possibly empty)

        SoundDna::LearnedSound s;
        s.id = "test_roundtrip_entry";
        s.role = "Bass";
        s.displayName = "unknown";
        s.status = SoundDna::LearnStatus::Failed;
        s.statusReason = "test entry - not a real learned sound";

        std::vector<SoundDna::LearnedSound> toSave = before;
        toSave.push_back(s);
        CHECK(SoundDna::saveLibraryIndex(toSave));

        std::vector<SoundDna::LearnedSound> loaded;
        CHECK(SoundDna::loadLibraryIndex(loaded));
        bool found = false;
        for (auto& l : loaded)
            if (l.id == "test_roundtrip_entry") { found = true; CHECK(l.statusReason == s.statusReason); }
        CHECK(found);

        // Restore the real library to what it was before this test ran -
        // never leave test data behind in the real library.
        SoundDna::saveLibraryIndex(before);
    }

    // ---- analyzeBuffer: empty buffer -> valid=false, never a fabricated
    // measurement; a real synthetic tone -> valid=true with sane values. ----
    {
        juce::AudioBuffer<float> empty(2, 0);
        const auto m0 = SoundDna::analyzeBuffer(empty, 44100.0);
        CHECK(m0.valid == false);

        const double sr = 44100.0;
        const int n = (int) sr; // 1 second
        juce::AudioBuffer<float> buf(2, n);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buf.getWritePointer(ch);
            for (int i = 0; i < n; ++i)
                d[i] = 0.4f * std::sin(2.0 * juce::MathConstants<double>::pi * 220.0 * (double) i / sr);
        }
        const auto m1 = SoundDna::analyzeBuffer(buf, sr);
        CHECK(m1.valid == true);
        CHECK(m1.rms > 0.01f); // a real 0.4-amplitude tone must have a real, non-trivial RMS
        CHECK(m1.rms < 1.0f);
        // Identical L/R channels -> correlation must read as fully
        // correlated (close to +1), a real, checkable DSP sanity test.
        CHECK(m1.stereoCorrelation > 0.99f);
    }

    // ---- analyzeBuffer stereo correlation: independent (uncorrelated)
    // noise on each channel must read as near-zero correlation, not
    // fabricated agreement. ----
    {
        const double sr = 44100.0;
        const int n = (int) sr / 4;
        juce::AudioBuffer<float> buf(2, n);
        juce::Random rngL(12345), rngR(67890);
        for (int i = 0; i < n; ++i)
        {
            buf.setSample(0, i, rngL.nextFloat() * 2.0f - 1.0f);
            buf.setSample(1, i, rngR.nextFloat() * 2.0f - 1.0f);
        }
        const auto m = SoundDna::analyzeBuffer(buf, sr);
        CHECK(m.valid);
        CHECK(std::abs(m.stereoCorrelation) < 0.3f); // independent random noise should not read as strongly correlated
    }

    // ---- measureReleaseTimeMs: a buffer that's silent immediately after
    // note-off measures a near-zero release time; a buffer that stays
    // loud measures -1 (never decayed within the render) - the honest
    // "genuinely long release, not clamped to a fake number" contract. ----
    {
        const double sr = 44100.0;
        const int sustainedSamples = (int) sr / 2; // 0.5s "held"
        const int tailSamples = (int) sr / 2;      // 0.5s tail
        const int noteOffIndex = sustainedSamples;

        // Case 1: tail drops to silence immediately.
        {
            juce::AudioBuffer<float> buf(1, sustainedSamples + tailSamples);
            auto* d = buf.getWritePointer(0);
            for (int i = 0; i < sustainedSamples; ++i) d[i] = 0.5f; // held
            for (int i = sustainedSamples; i < buf.getNumSamples(); ++i) d[i] = 0.0f; // silent immediately after note-off
            const float release = SoundDna::measureReleaseTimeMs(buf, sr, noteOffIndex);
            CHECK(release >= 0.0f);
            CHECK(release < 50.0f); // near-instant
        }

        // Case 2: tail stays just as loud as the sustained portion - never
        // decays -> must report -1, not a fabricated finite number.
        {
            juce::AudioBuffer<float> buf(1, sustainedSamples + tailSamples);
            auto* d = buf.getWritePointer(0);
            for (int i = 0; i < buf.getNumSamples(); ++i) d[i] = 0.5f;
            const float release = SoundDna::measureReleaseTimeMs(buf, sr, noteOffIndex);
            CHECK(release == -1.0f);
        }
    }

    // ---- deriveDnaLabels: grounded only in real measurements - a
    // synthetic LearnedSound with known sub-heavy/dark values must
    // produce the corresponding labels; one with no valid measurements
    // produces no labels at all (never guessed). ----
    {
        SoundDna::LearnedSound empty;
        CHECK(SoundDna::deriveDnaLabels(empty).isEmpty());

        SoundDna::LearnedSound s;
        SoundDna::NoteLengthResult medium;
        medium.label = "medium";
        medium.measurement.valid = true;
        medium.measurement.spectralCentroidHz = 300.0f; // dark
        medium.measurement.subPct = 0.5f;                // sub-heavy
        medium.measurement.stereoWidth = 0.05f;          // narrow
        medium.measurement.rms = 0.3f;
        s.noteLengths.push_back(medium);

        const auto labels = SoundDna::deriveDnaLabels(s);
        CHECK(labels.contains("dark"));
        CHECK(labels.contains("sub-heavy"));
        CHECK(labels.contains("narrow"));
        CHECK(!labels.contains("bright"));
        CHECK(!labels.contains("wide"));
    }

    // ---- discoverCandidates: real, on-disk discovery - never fabricates
    // a candidate. An unrecognized role returns empty; a nonexistent
    // factory root returns empty for every role (honest degradation,
    // matching this project's own established convention for a missing
    // Xfer library on a different machine). ----
    {
        auto fake = SoundLibraryLearner::discoverCandidates("Bass", 5, juce::File("/definitely/does/not/exist"));
        CHECK(fake.empty());

        auto unknownRole = SoundLibraryLearner::discoverCandidates("NotARole", 5, SoundRecommendation::defaultSerumFactoryPresetsRoot());
        CHECK(unknownRole.empty());
    }

    // ---- discoverCandidates against the REAL Xfer factory library (if
    // present on this machine - the same honest-degrade convention as
    // SoundRecommendation's own tests): real filenames, correct role
    // tagging, and the cap is respected. ----
    {
        const auto factoryRoot = SoundRecommendation::defaultSerumFactoryPresetsRoot();
        if (factoryRoot.isDirectory())
        {
            auto bass = SoundLibraryLearner::discoverCandidates("Bass", 3, factoryRoot);
            CHECK(bass.size() <= 3);
            for (auto& c : bass)
            {
                CHECK(c.file.existsAsFile());
                CHECK(c.file.getFileExtension() == ".SerumPreset");
                CHECK(c.role == "Bass");
                CHECK(c.sourceHint.contains("Bass"));
            }

            auto melody = SoundLibraryLearner::discoverCandidates("Melody", 3, factoryRoot);
            for (auto& c : melody)
                CHECK(c.role == "Melody");

            auto pad = SoundLibraryLearner::discoverCandidates("Pad", 3, factoryRoot);
            for (auto& c : pad)
                CHECK(c.role == "Pad");
        }
        else
        {
            std::fprintf(stderr, "test_sound_dna_library: Xfer Serum 2 factory preset library not found on "
                                  "this machine - skipping the real-file discovery sub-checks (honest degrade, "
                                  "same convention as SoundRecommendation's own tests).\n");
        }
    }

    // ---- SerumAutomation::isAutomationAvailable: just a real, checkable
    // boolean call (no crash) - this test process's own trust status is
    // whatever it is; not asserted either way, since it's an environment
    // fact, not something this test controls. ----
    {
        const bool avail = SerumAutomation::isAutomationAvailable();
        juce::ignoreUnused(avail);
        CHECK(true); // reaching here without crashing is the actual check
    }

    std::printf("%d/%d checks passed\n", g_checksRun - g_checksFailed, g_checksRun);
    return g_checksFailed == 0 ? 0 : 1;
}
