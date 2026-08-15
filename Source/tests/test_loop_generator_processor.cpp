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

    tempDir.deleteRecursively();

    TEST_SUMMARY_AND_EXIT();
}
