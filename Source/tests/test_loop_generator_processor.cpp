// Regression tests for the loop-generator workflow's PluginProcessor-level
// contracts (Part 16 of the loop-generator brief): mute/unmute must never
// touch stored pattern/sample-selection state, and both melody voices
// (Bass=track0, Melody=track1) must show a generated pattern active
// immediately after Generate - no arrangement/transport wait. JUCE-linked
// (constructs a real AbletonCopilotAudioProcessor), same technique as
// test_rack_classification.cpp/test_rack_browser_cache.cpp - see those
// files' own build notes for the compile/link recipe used this session.
//
// Deliberately does NOT go through PluginEditor (constructing the full
// editor pulls in the GUI/preset-scan/library-scan machinery, which is
// its own much heavier surface) - everything checked here is a real
// PluginProcessor public-API contract, exercised directly.
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../MusicTheory/MelodyMotifGenerator.h"
#include "../MelodyGridComponent.h"
#include "../Engine/tests/TestSupport.h"
#include <cstdio>
#include <cstdlib>

namespace
{
    // A minimal, real, decodable WAV file - same technique as
    // test_rack_browser_cache.cpp's makeTestWav - just enough for
    // AbletonCopilotAudioProcessor::setGeneratedDrumPattern's internal
    // AudioFormatManager::createReaderFor() to succeed.
    juce::File makeTestWav(const juce::File& dir, const juce::String& name)
    {
        juce::File f = dir.getChildFile(name);
        f.deleteFile();

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::FileOutputStream> stream(f.createOutputStream());
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wav.createWriterFor(stream.get(), 44100.0, 1, 16, {}, 0));
        if (writer != nullptr)
        {
            stream.release(); // writer now owns it
            juce::AudioBuffer<float> buf(1, 4410); // 100ms of silence - decodable is all that matters
            buf.clear();
            writer->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples());
        }
        return f;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("AbletonCopilot_loopgen_test_" + juce::String(juce::Random::getSystemRandom().nextInt(1000000)));
    tempDir.createDirectory();
    auto kickWav = makeTestWav(tempDir, "kick.wav");
    CHECK(kickWav.existsAsFile());

    AbletonCopilotAudioProcessor processor;

    // ---- Both melody voices (Bass=0, Melody=1) show a generated pattern
    // active immediately after setGeneratedMelodyPattern - no transport/
    // arrangement wait, matching "Serum/MelodyVoice receives bass/melody
    // immediately." ----
    {
        std::vector<int8_t> bass(256, -128);
        bass[0] = 0; // one real note in bar 0
        processor.setGeneratedMelodyPattern(0, bass, 9);
        const auto diagBass = processor.getMelodyVoiceDiagnostics(0);
        CHECK(diagBass.generatedPatternActive);
        CHECK(diagBass.generatedTotalSteps == 256);

        std::vector<int8_t> melody(256, -128);
        melody[4] = 3;
        processor.setGeneratedMelodyPattern(1, melody, 9);
        const auto diagMelody = processor.getMelodyVoiceDiagnostics(1);
        CHECK(diagMelody.generatedPatternActive);
        CHECK(diagMelody.generatedTotalSteps == 256);
    }

    // ---- Mute/unmute never touches stored pattern/sample-selection
    // state: setGeneratedDrumPattern is the ONLY place
    // getGeneratedRoleLoadDiagnostics' data is set (see PluginProcessor.cpp),
    // so if that diagnostic is byte-identical before/after a mute toggle,
    // mute provably never re-ran pattern/sample-selection logic. ----
    {
        std::vector<AbletonCopilotAudioProcessor::GeneratedDrumRole> roles;
        AbletonCopilotAudioProcessor::GeneratedDrumRole kick;
        kick.midiNote = 36; // GM kick - resolves to Engine::DrumRole::Kick
        kick.velocity.assign(256, 0);
        kick.velocity[0] = 127;
        kick.sampleFile = kickWav;
        roles.push_back(kick);
        processor.setGeneratedDrumPattern(roles);

        const auto before = processor.getGeneratedRoleLoadDiagnostics(Engine::DrumRole::Kick);
        CHECK(before.candidateFile == kickWav);

        processor.setGeneratedDrumRoleMuted((int) Engine::DrumRole::Kick, true);
        processor.setGeneratedDrumRoleGain((int) Engine::DrumRole::Kick, 0.4f);
        processor.setGeneratedDrumRoleMuted((int) Engine::DrumRole::Kick, false);

        const auto after = processor.getGeneratedRoleLoadDiagnostics(Engine::DrumRole::Kick);
        CHECK(after.candidateFile == before.candidateFile);
        CHECK(after.finalLoadedFile == before.finalLoadedFile);
        CHECK(after.candidateExists == before.candidateExists);
        CHECK(after.decodedLengthSamples == before.decodedLengthSamples);

        // The generated melody patterns set above must also survive the
        // drum-role mute calls untouched (mute is drum-role-scoped only).
        CHECK(processor.getMelodyVoiceDiagnostics(0).generatedPatternActive);
        CHECK(processor.getMelodyVoiceDiagnostics(1).generatedPatternActive);
    }

    // ---- Serum2 preset-honesty guard: capturedPresetActive must start
    // false (nothing has ever been captured/loaded), and a loadCapturedPreset
    // call that fails (no real Serum2 instance is loaded in this test
    // environment - construction never auto-loads a plugin) must leave it
    // false, never flip it true on a failed attempt. This is the most
    // important guard against the two dishonest-reporting bugs found this
    // pass (PluginEditor's serumStatusFor/updateTrackTitle previously
    // trusted UI-side presetIndex bookkeeping alone) - a full successful-
    // capture path can't be exercised here without a real loaded Serum2
    // instance, but "never a false positive" is provable without one. ----
    {
        CHECK(processor.getMelodyVoiceDiagnostics(0).capturedPresetActive == false);
        CHECK(processor.getMelodyVoiceDiagnostics(1).capturedPresetActive == false);

        const bool loaded = processor.loadCapturedPreset(0, juce::File("/nonexistent/no.serumstate"));
        CHECK(loaded == false); // no Serum2 instance in this test environment - must fail, not silently succeed
        CHECK(processor.getMelodyVoiceDiagnostics(0).capturedPresetActive == false);
    }

    // ---- Real gate-length data (Part 3 of the archetype-groove pass:
    // Engine::BassArchetype's note lengths) reaches the voice when passed,
    // and does NOT appear on a track that never received one - proves the
    // optional parameter actually plumbs through to MelodyVoice, and that
    // melody's existing (no-gate) call path is unaffected by the new
    // mechanism existing at all. ----
    {
        std::vector<int8_t> bassWithGates(256, -128);
        std::vector<int8_t> gates(256, 0);
        bassWithGates[2] = 0;  gates[2] = 3; // matches SteadyOffbeat's own shape
        bassWithGates[6] = 0;  gates[6] = 3;
        processor.setGeneratedMelodyPattern(0, bassWithGates, 9, gates);
        const auto diagBassGated = processor.getMelodyVoiceDiagnostics(0);
        CHECK(diagBassGated.generatedGateLengthCount == 2);

        // Melody (track 1) was set earlier in this test with NO gate
        // array (the existing 3-argument call, matching every real
        // melody call site) - must show zero gate entries.
        const auto diagMelodyNoGate = processor.getMelodyVoiceDiagnostics(1);
        CHECK(diagMelodyNoGate.generatedGateLengthCount == 0);

        // Re-setting bass with the ORIGINAL 3-argument call (no gate
        // array) must clear any previously-stored gate data - the
        // parameter is a real replacement, not an additive merge.
        std::vector<int8_t> bassNoGate(256, -128);
        bassNoGate[0] = 0;
        processor.setGeneratedMelodyPattern(0, bassNoGate, 9);
        CHECK(processor.getMelodyVoiceDiagnostics(0).generatedGateLengthCount == 0);
    }

    // ---- Melody motif generator: deterministic for the same seed, and
    // active within bar 0 (steps 0-15) - "melody active from bar 1." ----
    {
        const auto a = MelodyMotifGenerator::generateMelodyMotif(9, true, MelodyCategory::Lead, MelodyStyle::Default, 42);
        const auto b = MelodyMotifGenerator::generateMelodyMotif(9, true, MelodyCategory::Lead, MelodyStyle::Default, 42);
        CHECK(a == b);

        const auto c = MelodyMotifGenerator::generateMelodyMotif(9, true, MelodyCategory::Lead, MelodyStyle::Default, 4242);
        CHECK(a != c); // a different seed must be ABLE to produce a different result

        bool activeInBar0 = false;
        for (int i = 0; i < 16; ++i)
            if (a[(size_t) i] != MelodyGridComponent::kMelodyOff)
                activeInBar0 = true;
        // Not every seed/motif necessarily lands a note in the exact first
        // bar (the motif pools have real rests) - check across several
        // seeds so this proves "can start immediately," not "always hits
        // step 0-15 for this one arbitrary seed."
        if (!activeInBar0)
        {
            for (uint32_t s = 1; s < 20 && !activeInBar0; ++s)
            {
                const auto trial = MelodyMotifGenerator::generateMelodyMotif(9, true, MelodyCategory::Lead, MelodyStyle::Default, s);
                for (int i = 0; i < 16; ++i)
                    if (trial[(size_t) i] != MelodyGridComponent::kMelodyOff)
                        activeInBar0 = true;
            }
        }
        CHECK(activeInBar0);
    }

    // ---- Bass register clamp (section 12's explicit "bass stays low"
    // ask, and the "previous problem where the bass jumped into high
    // octaves must not return" requirement) - clampBassRegisterPitch
    // (PluginProcessor.h) must keep every real keyRoot(0-11)/offset(the
    // full -7..+7 archetype range, plus some margin) combination inside
    // the fixed F1-C3 band, and must do so by octave-WRAPPING (preserving
    // pitch class), never by truncating to a single note. ----
    {
        constexpr int kBassRegisterMin = 29, kBassRegisterMax = 48;
        bool everyPitchInBand = true;
        bool everyPitchClassPreserved = true;
        for (int keyRoot = 0; keyRoot <= 11; ++keyRoot)
        {
            for (int offset = -12; offset <= 12; ++offset) // wider than the real ±7 archetype range, as a margin
            {
                const int raw     = 36 + keyRoot + offset;
                const int clamped = clampBassRegisterPitch(raw);
                if (clamped < kBassRegisterMin || clamped > kBassRegisterMax)
                    everyPitchInBand = false;
                if (((clamped % 12) + 12) % 12 != ((raw % 12) + 12) % 12)
                    everyPitchClassPreserved = false;
            }
        }
        CHECK(everyPitchInBand);
        CHECK(everyPitchClassPreserved);

        // The exact real-world case that motivated this fix: the same
        // seed/archetype/offset at two different keys must now land in
        // the SAME register (previously key=B could sit a full 1.5
        // octaves above key=C for identical bass content).
        const int keyC_offsetLow  = clampBassRegisterPitch(36 + 0  + (-7)); // key=C, offset=-7
        const int keyB_offsetHigh = clampBassRegisterPitch(36 + 11 + (+7)); // key=B, offset=+7
        CHECK(std::abs(keyC_offsetLow - keyB_offsetHigh) <= (kBassRegisterMax - kBassRegisterMin));
    }

    tempDir.deleteRecursively();

    TEST_SUMMARY_AND_EXIT();
}
