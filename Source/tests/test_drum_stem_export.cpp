// Regression tests for the drum-stem-export workflow (Source/DrumStemExporter.h) -
// JUCE-linked (constructs a real AbletonCopilotAudioProcessor for the
// snapshot/no-mutation tests, same technique as
// test_loop_generator_processor.cpp - see that file's own build notes for
// the compile/link recipe used this session).
//
// Every property the user explicitly asked to have proven: 6 stems
// produced; all identical duration; all start-aligned (same time basis);
// a muted role's stem is silent; per-role isolation (a role's stem only
// ever contains that role's own onsets); exporting never mutates stored
// pattern/sample-selection state; exporting twice produces byte-identical
// audio; exact sample count for 16 bars at the given bpm/sampleRate;
// correct loop boundary (no truncation/padding); and a reconstruction
// test (sum of the 6 stems matches the combined-mix reference within a
// small tolerance).
#include <JuceHeader.h>
#include "../DrumStemExporter.h"
#include "../PluginProcessor.h"
#include "../Engine/Grid.h"
#include "../Engine/tests/TestSupport.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{
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
            stream.release();
            juce::AudioBuffer<float> buf(1, 4410);
            buf.clear();
            writer->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples());
        }
        return f;
    }

    // A constant-value mono sample buffer, long enough that it would
    // normally extend well past a second nearby onset if not clamped -
    // used to prove the monophonic-per-role hard-retrigger-cutoff
    // behaviour.
    std::shared_ptr<juce::AudioBuffer<float>> makeConstantBuffer(int numSamples, float value)
    {
        auto buf = std::make_shared<juce::AudioBuffer<float>>(1, numSamples);
        buf->clear();
        for (int i = 0; i < numSamples; ++i)
            buf->setSample(0, i, value);
        return buf;
    }

    DrumStemExporter::Input makeBaseInput(int totalSteps, double bpm, double sampleRate)
    {
        DrumStemExporter::Input input;
        input.totalSteps = totalSteps;
        input.bpm         = bpm;
        input.sampleRate  = sampleRate;
        for (auto& r : input.roles)
        {
            r.midiNote = -1;
            r.velocity.assign((size_t) totalSteps, 0);
        }
        return input;
    }

    float peakAbs(const juce::AudioBuffer<float>& b, int startSample, int numSamples)
    {
        float peak = 0.0f;
        startSample = juce::jmax(0, startSample);
        numSamples  = juce::jmin(numSamples, b.getNumSamples() - startSample);
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < numSamples; ++i)
                peak = juce::jmax(peak, std::abs(b.getSample(ch, startSample + i)));
        return peak;
    }

    bool isSilent(const juce::AudioBuffer<float>& b)
    {
        return peakAbs(b, 0, b.getNumSamples()) == 0.0f;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // ---- computeTotalSamples: exact, clean-integer bpm/sampleRate so the
    // expected value can be hand-derived instead of duplicating the
    // production formula in the test. bpm=120 -> secPerBar=2.0s ->
    // 16 bars = 32.0s -> 32.0 * 44100 = 1,411,200 samples exactly. ----
    {
        const int samples = DrumStemExporter::computeTotalSamples(256, 120.0, 44100.0);
        CHECK(samples == 1411200);
    }

    // ---- 6 stems produced, all with identical duration == the computed
    // total-sample count (this IS "all start at the same time" too, since
    // every stem's buffer index 0 represents the same instant, step 0). ----
    {
        auto input = makeBaseInput(256, 120.0, 44100.0);
        input.roles[0].midiNote = 36; // Kick - unmuted, but no hits -> silent, still full-length
        const auto stems = DrumStemExporter::renderDrumStems(input);
        CHECK(stems.size() == 6);
        for (auto& s : stems)
        {
            CHECK(s.audio.getNumSamples() == 1411200);
            CHECK(s.audio.getNumChannels() == 2);
        }
    }

    // ---- Loop boundary is correct: a hit exactly at the LAST step (255)
    // is still placed inside the buffer, not silently dropped/truncated -
    // and the buffer length itself is exactly the 16-bar total, no padding
    // beyond it. ----
    {
        auto input = makeBaseInput(256, 120.0, 44100.0);
        input.roles[0].midiNote = 36;
        input.roles[0].velocity[255] = 100;
        input.roles[0].sampleBuffer  = makeConstantBuffer(2000, 1.0f);
        const auto stems = DrumStemExporter::renderDrumStems(input);
        const auto& kick = stems[0].audio;
        const double lastStepSec = Engine::stepTimeSeconds(255, 120.0, Engine::StepGridConfig{});
        const int    lastStepSample = (int) std::lround(lastStepSec * 44100.0);
        CHECK(lastStepSample < kick.getNumSamples());
        CHECK(peakAbs(kick, lastStepSample, 10) > 0.0f); // the hit is really there, not dropped
    }

    // ---- Muted role produces a fully silent stem, even though it has
    // real hits in its velocity array (proves mute is honoured, not just
    // "nothing would have played anyway"). A sanity check right after
    // confirms the SAME pattern, unmuted, is genuinely audible - so the
    // silence above is really caused by mute, not by an unrelated bug. ----
    {
        auto input = makeBaseInput(256, 120.0, 44100.0);
        input.roles[0].midiNote = 36; // Kick, synth fallback (no sampleBuffer)
        input.roles[0].velocity[0] = 127;
        input.roles[0].muted = true;
        const auto mutedStems = DrumStemExporter::renderDrumStems(input);
        CHECK(isSilent(mutedStems[0].audio));

        input.roles[0].muted = false;
        const auto unmutedStems = DrumStemExporter::renderDrumStems(input);
        CHECK(!isSilent(unmutedStems[0].audio));
    }

    // ---- Per-role isolation: Kick has a hit only at step 0, Clap only at
    // step 32 (bar 2) - each role's stem must show energy ONLY in its own
    // hit's window, and every OTHER role's stem (including the other of
    // this pair) must be silent at that same instant. ----
    {
        auto input = makeBaseInput(256, 120.0, 44100.0);
        input.roles[0].midiNote = 36; // Kick
        input.roles[0].velocity[0] = 127;
        input.roles[0].sampleBuffer = makeConstantBuffer(2000, 1.0f);

        input.roles[1].midiNote = 39; // Clap
        input.roles[1].velocity[32] = 127;
        input.roles[1].sampleBuffer = makeConstantBuffer(2000, 1.0f);

        const auto stems = DrumStemExporter::renderDrumStems(input);
        const int clapStepSample = (int) std::lround(
            Engine::stepTimeSeconds(32, 120.0, Engine::StepGridConfig{}) * 44100.0);

        CHECK(peakAbs(stems[0].audio, 0, 100) > 0.0f);              // Kick stem has its own hit
        CHECK(peakAbs(stems[0].audio, clapStepSample, 100) == 0.0f); // Kick stem silent where only Clap fired
        CHECK(peakAbs(stems[1].audio, clapStepSample, 100) > 0.0f);  // Clap stem has its own hit
        CHECK(peakAbs(stems[1].audio, 0, 100) == 0.0f);              // Clap stem silent where only Kick fired
        for (int r = 2; r < 6; ++r)
            CHECK(isSilent(stems[(size_t) r].audio)); // roles with no midiNote/hits at all stay silent
    }

    // ---- Monophonic-per-role hard-retrigger cutoff: two onsets of the
    // SAME role close together must not overlap - the first hit's audio
    // must stop exactly where the second hit's begins (mirrors live
    // playback's readPos=0 hard reset on retrigger), not sum/crossfade. ----
    {
        auto input = makeBaseInput(256, 120.0, 44100.0);
        input.roles[0].midiNote = 36;
        input.roles[0].velocity[0] = 127; // step 0
        input.roles[0].velocity[1] = 127; // step 1, immediately after - forces a short cutoff on step 0's hit
        input.roles[0].sampleBuffer = makeConstantBuffer(50000, 1.0f); // much longer than one step

        const auto stems = DrumStemExporter::renderDrumStems(input);
        const auto& kick = stems[0].audio;

        const int start0 = (int) std::lround(Engine::stepTimeSeconds(0, 120.0, Engine::StepGridConfig{}) * 44100.0);
        const int start1 = (int) std::lround(Engine::stepTimeSeconds(1, 120.0, Engine::StepGridConfig{}) * 44100.0);
        CHECK(start1 > start0);

        // Right up to (but not including) start1, hit 0's audio is present.
        CHECK(std::abs(kick.getSample(0, start1 - 1) - 1.0f) < 1e-6f);
        // From start1 onward, hit 0 must NOT still be sounding underneath
        // hit 1 (both hits share the same constant value, so this can only
        // be checked structurally: hit 0's contribution to this region is
        // clamped to zero length, so total energy in [start1, start1+N)
        // must equal exactly ONE hit's worth, not two summed/doubled).
        CHECK(std::abs(kick.getSample(0, start1) - 1.0f) < 1e-6f);      // hit 1 sounding, not silence
        CHECK(std::abs(kick.getSample(0, start1) - 2.0f) > 1e-3f);      // NOT hit0+hit1 summed (would be 2.0)
    }

    // ---- Determinism: rendering the exact same input twice must produce
    // byte-identical (sample-for-sample equal) audio - "exporting twice
    // with the same pattern produces identical audio." ----
    {
        auto input = makeBaseInput(256, 120.0, 44100.0);
        input.roles[0].midiNote = 36;
        input.roles[0].velocity[0] = 100;
        input.roles[0].sampleBuffer = makeConstantBuffer(2000, 0.7f);
        input.roles[2].midiNote = 42; // HatClosed - exercises the swing path too
        for (int s = 1; s < 256; s += 2) input.roles[2].velocity[s] = 90;

        const auto a = DrumStemExporter::renderDrumStems(input);
        const auto b = DrumStemExporter::renderDrumStems(input);
        for (size_t r = 0; r < a.size(); ++r)
        {
            CHECK(a[r].audio.getNumSamples() == b[r].audio.getNumSamples());
            bool identical = true;
            for (int ch = 0; ch < a[r].audio.getNumChannels() && identical; ++ch)
                for (int i = 0; i < a[r].audio.getNumSamples() && identical; ++i)
                    if (a[r].audio.getSample(ch, i) != b[r].audio.getSample(ch, i))
                        identical = false;
            CHECK(identical);
        }
    }

    // ---- Reconstruction: summing all 6 independently-rendered stems must
    // match renderDrumMixReference (the same onset-placement code,
    // accumulated into one buffer) within a tiny float tolerance. ----
    {
        auto input = makeBaseInput(256, 120.0, 44100.0);
        input.roles[0].midiNote = 36; input.roles[0].velocity[0]  = 127; input.roles[0].sampleBuffer = makeConstantBuffer(2000, 0.5f);
        input.roles[1].midiNote = 39; input.roles[1].velocity[16] = 110; input.roles[1].sampleBuffer = makeConstantBuffer(1500, 0.3f);
        input.roles[2].midiNote = 42; for (int s = 1; s < 256; s += 4) input.roles[2].velocity[s] = 80; // HatClosed, swung
        input.roles[4].midiNote = 37; input.roles[4].velocity[48] = 100; // PercA, synth fallback

        const auto stems = DrumStemExporter::renderDrumStems(input);
        const auto mixRef = DrumStemExporter::renderDrumMixReference(input);

        CHECK(mixRef.getNumSamples() == DrumStemExporter::computeTotalSamples(256, 120.0, 44100.0));

        juce::AudioBuffer<float> summed(2, mixRef.getNumSamples());
        summed.clear();
        for (auto& s : stems)
            for (int ch = 0; ch < summed.getNumChannels(); ++ch)
                summed.addFrom(ch, 0, s.audio, ch, 0, s.audio.getNumSamples());

        float maxDiff = 0.0f;
        for (int ch = 0; ch < summed.getNumChannels(); ++ch)
            for (int i = 0; i < summed.getNumSamples(); ++i)
                maxDiff = juce::jmax(maxDiff, std::abs(summed.getSample(ch, i) - mixRef.getSample(ch, i)));
        CHECK(maxDiff < 1e-5f);
    }

    // ---- Processor integration: getGeneratedDrumStemSnapshot + export
    // must never mutate stored pattern/sample-selection state (the exact
    // same before/after diagnostic-equality technique
    // test_loop_generator_processor.cpp uses for mute/unmute), and must
    // never change what setGeneratedDrumPattern originally stored. ----
    {
        auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                           .getChildFile("AbletonCopilot_stemexport_test_" + juce::String(juce::Random::getSystemRandom().nextInt(1000000)));
        tempDir.createDirectory();
        auto kickWav = makeTestWav(tempDir, "kick.wav");
        CHECK(kickWav.existsAsFile());

        AbletonCopilotAudioProcessor processor;

        std::vector<AbletonCopilotAudioProcessor::GeneratedDrumRole> roles;
        AbletonCopilotAudioProcessor::GeneratedDrumRole kick;
        kick.midiNote = 36;
        kick.velocity.assign(256, 0);
        kick.velocity[0]  = 127;
        kick.velocity[64] = 100;
        kick.sampleFile   = kickWav;
        roles.push_back(kick);
        processor.setGeneratedDrumPattern(roles);

        const auto diagBefore = processor.getGeneratedRoleLoadDiagnostics(Engine::DrumRole::Kick);
        CHECK(diagBefore.finalLoadedFile == kickWav);

        const auto snapshot = processor.getGeneratedDrumStemSnapshot(124.0);
        CHECK(snapshot.bpm == 124.0);
        CHECK(snapshot.totalSteps == 256);
        CHECK(snapshot.roles[0].midiNote == 36);
        CHECK(snapshot.roles[0].velocity[0]  == 127);
        CHECK(snapshot.roles[0].velocity[64] == 100);
        CHECK(snapshot.roles[0].loadedFile == kickWav);

        const auto stems = DrumStemExporter::renderDrumStems(snapshot);
        const auto writeResult = DrumStemExporter::writeStemsToWav(stems, tempDir, snapshot.sampleRate);
        CHECK(writeResult.allSucceeded);
        for (auto& f : writeResult.writtenFiles)
            CHECK(f.existsAsFile());

        const auto diagAfter = processor.getGeneratedRoleLoadDiagnostics(Engine::DrumRole::Kick);
        CHECK(diagAfter.candidateFile      == diagBefore.candidateFile);
        CHECK(diagAfter.finalLoadedFile    == diagBefore.finalLoadedFile);
        CHECK(diagAfter.candidateExists    == diagBefore.candidateExists);
        CHECK(diagAfter.decodedLengthSamples == diagBefore.decodedLengthSamples);

        // Every written WAV must have the exact same length in samples -
        // "all six start at the same time" and "all six identical
        // duration," verified by actually decoding the files, not just
        // trusting the in-memory buffers.
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        juce::int64 firstLength = -1;
        for (auto& f : writeResult.writtenFiles)
        {
            std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(f));
            CHECK(reader != nullptr);
            if (reader != nullptr)
            {
                if (firstLength < 0) firstLength = reader->lengthInSamples;
                CHECK(reader->lengthInSamples == firstLength);
                CHECK(reader->sampleRate == snapshot.sampleRate);
            }
        }

        // Exporting twice (a second call, into the same base folder - a
        // new timestamped subfolder each time) must produce byte-identical
        // audio content, decoded back from disk.
        const auto writeResult2 = DrumStemExporter::writeStemsToWav(stems, tempDir, snapshot.sampleRate);
        CHECK(writeResult2.allSucceeded);
        CHECK(writeResult2.outputDirectory != writeResult.outputDirectory); // never silently overwrites

        std::unique_ptr<juce::AudioFormatReader> readerA(fm.createReaderFor(writeResult.writtenFiles[0]));
        std::unique_ptr<juce::AudioFormatReader> readerB(fm.createReaderFor(writeResult2.writtenFiles[0]));
        CHECK(readerA != nullptr && readerB != nullptr);
        if (readerA != nullptr && readerB != nullptr)
        {
            CHECK(readerA->lengthInSamples == readerB->lengthInSamples);
            juce::AudioBuffer<float> bufA(1, (int) readerA->lengthInSamples);
            juce::AudioBuffer<float> bufB(1, (int) readerB->lengthInSamples);
            readerA->read(&bufA, 0, bufA.getNumSamples(), 0, true, false);
            readerB->read(&bufB, 0, bufB.getNumSamples(), 0, true, false);
            bool identical = true;
            for (int i = 0; i < bufA.getNumSamples() && identical; ++i)
                if (std::abs(bufA.getSample(0, i) - bufB.getSample(0, i)) > 1e-6f)
                    identical = false;
            CHECK(identical);
        }

        tempDir.deleteRecursively();
    }

    TEST_SUMMARY_AND_EXIT();
}
