#include "DrumStemExporter.h"
#include "Engine/Grid.h"
#include <cmath>

namespace DrumStemExporter
{
    namespace
    {
        // Mirrors PluginProcessor.cpp's own `constexpr float kSwingAmount
        // = 0.10f` exactly - deliberately duplicated rather than shared,
        // to avoid touching that live-playback file at all for this
        // feature (see the plan this was built from). If that value is
        // ever changed, this one must be updated to match for stems to
        // stay faithful to live playback.
        constexpr float kHatSwingAmount = 0.10f;

        bool isSwingRole(Engine::DrumRole role)
        {
            return role == Engine::DrumRole::HatClosed
                || role == Engine::DrumRole::PercA
                || role == Engine::DrumRole::PercB;
        }

        const char* roleLabel(int r)
        {
            static const char* const kLabels[6] = { "Kick", "Clap", "Closed Hat", "Open Hat", "Perc A", "Perc B" };
            return (r >= 0 && r < 6) ? kLabels[r] : "";
        }

        const char* roleFileBaseName(int r)
        {
            static const char* const kNames[6] = { "Kick", "Clap", "ClosedHat", "OpenHat", "PercA", "PercB" };
            return (r >= 0 && r < 6) ? kNames[r] : "";
        }

        // The core placement algorithm, shared by renderDrumStems (target
        // = that role's own buffer) and renderDrumMixReference (target =
        // one shared buffer, called once per role) - same code path both
        // times, which is what makes the reconstruction property
        // (sum of stems == mix reference) hold by construction.
        void renderRoleOnsetsInto(juce::AudioBuffer<float>& target, Engine::DrumRole role,
                                   const RoleInput& roleInput, int totalSteps, double bpm,
                                   double sampleRate, int totalSamples)
        {
            if (roleInput.muted || roleInput.midiNote < 0 || roleInput.velocity.empty())
                return;

            Engine::StepGridConfig config;
            config.swing = isSwingRole(role) ? kHatSwingAmount : 0.0f;

            struct Onset { int sampleStart; float velocity01; };
            std::vector<Onset> onsets;
            const int stepLimit = juce::jmin(totalSteps, (int) roleInput.velocity.size());
            for (int step = 0; step < stepLimit; ++step)
            {
                const int vel = roleInput.velocity[(size_t) step];
                if (vel <= 0)
                    continue;

                // Same gain-scaling formula as processBlock's generated-
                // drum-role trigger block: gatedVel = clamp(round(vel *
                // roleGain)), skipped entirely if it rounds to <= 0.
                const int gatedVel = juce::jlimit(0, 127, (int) std::lround((float) vel * roleInput.gain));
                if (gatedVel <= 0)
                    continue;

                // Exact continuous-time position (see this file's own
                // header comment on the block-quantization simplification)
                // - Engine::stepTimeSeconds already gates swing to odd
                // steps internally, so passing a nonzero config.swing for
                // every step of a swing-eligible role is correct, not an
                // over-application.
                const double t = Engine::stepTimeSeconds(step, bpm, config);
                const int sampleStart = (int) std::lround(t * sampleRate);
                if (sampleStart >= totalSamples)
                    continue; // shouldn't happen for step < totalSteps, guarded anyway

                onsets.push_back({ sampleStart, (float) gatedVel / 127.0f });
            }

            if (onsets.empty())
                return;

            const bool hasSample = (roleInput.sampleBuffer != nullptr && roleInput.sampleBuffer->getNumSamples() > 0);

            // Persistent across all onsets of this role, same as the live
            // generatedDrumSynthVoices[r] slot - only meaningful when
            // !hasSample (synth fallback).
            Engine::DrumVoiceState synthVoice;
            std::vector<float> scratch;

            for (size_t i = 0; i < onsets.size(); ++i)
            {
                const int start  = onsets[i].sampleStart;
                const int cutoff = (i + 1 < onsets.size()) ? onsets[i + 1].sampleStart : totalSamples;
                // Monophonic-per-role hard retrigger (see this file's own
                // header comment) - a hit is never rendered past the
                // point the NEXT onset of the same role starts, exactly
                // mirroring triggerGeneratedRole's readPos=0 hard reset.
                const int available = juce::jmax(0, juce::jmin(totalSamples, cutoff) - start);
                if (available <= 0)
                    continue;

                if (hasSample)
                {
                    auto&     buf   = *roleInput.sampleBuffer;
                    const int n     = juce::jmin(available, buf.getNumSamples());
                    const int srcCh = buf.getNumChannels();
                    for (int ch = 0; ch < target.getNumChannels(); ++ch)
                        target.addFrom(ch, start, buf, juce::jmin(ch, srcCh - 1), 0, n, onsets[i].velocity01);
                }
                else
                {
                    Engine::triggerDrumVoice(synthVoice, role, onsets[i].velocity01, sampleRate);
                    scratch.assign((size_t) available, 0.0f);
                    Engine::renderDrumVoice(synthVoice, scratch.data(), available);
                    for (int ch = 0; ch < target.getNumChannels(); ++ch)
                        target.addFrom(ch, start, scratch.data(), available);
                }
            }
        }
    }

    int computeTotalSamples(int totalSteps, double bpm, double sampleRate)
    {
        Engine::StepGridConfig config; // stepsPerBar defaults to 16 - the same grid the shipped product always uses
        const double totalSec = Engine::stepTimeSeconds(totalSteps, bpm, config);
        return juce::jmax(0, (int) std::lround(totalSec * sampleRate));
    }

    Result renderDrumStems(const Input& input)
    {
        Result out;
        const int totalSamples = computeTotalSamples(input.totalSteps, input.bpm, input.sampleRate);

        for (int r = 0; r < (int) Engine::DrumRole::Count; ++r)
        {
            auto& result = out[(size_t) r];
            const auto& roleInput = input.roles[(size_t) r];

            result.roleLabel       = roleLabel(r);
            result.fileBaseName    = roleFileBaseName(r);
            result.loadedFile      = roleInput.loadedFile;
            result.isSynthFallback = (roleInput.sampleBuffer == nullptr || roleInput.sampleBuffer->getNumSamples() == 0);

            result.audio.setSize(2, totalSamples);
            result.audio.clear();

            renderRoleOnsetsInto(result.audio, (Engine::DrumRole) r, roleInput,
                                  input.totalSteps, input.bpm, input.sampleRate, totalSamples);
        }
        return out;
    }

    juce::AudioBuffer<float> renderDrumMixReference(const Input& input)
    {
        const int totalSamples = computeTotalSamples(input.totalSteps, input.bpm, input.sampleRate);
        juce::AudioBuffer<float> mix(2, totalSamples);
        mix.clear();

        for (int r = 0; r < (int) Engine::DrumRole::Count; ++r)
            renderRoleOnsetsInto(mix, (Engine::DrumRole) r, input.roles[(size_t) r],
                                  input.totalSteps, input.bpm, input.sampleRate, totalSamples);
        return mix;
    }

    WriteResult writeStemsToWav(const Result& stems, const juce::File& baseFolder, double sampleRate)
    {
        WriteResult result;

        // Second-resolution timestamp alone can collide on two exports
        // within the same wall-clock second - append a numeric suffix
        // until an unused folder name is found, so "repeated exports
        // never silently overwrite each other" holds even then.
        const auto timestamp = juce::Time::getCurrentTime().formatted("%Y-%m-%d_%H%M%S");
        result.outputDirectory = baseFolder.getChildFile("DrumStems_" + timestamp);
        int suffix = 2;
        while (result.outputDirectory.exists())
            result.outputDirectory = baseFolder.getChildFile("DrumStems_" + timestamp + "_" + juce::String(suffix++));

        if (!result.outputDirectory.createDirectory())
        {
            result.errorMessage = "Could not create output folder: " + result.outputDirectory.getFullPathName();
            return result;
        }

        juce::WavAudioFormat wavFormat;
        bool allOk = true;

        for (size_t i = 0; i < stems.size(); ++i)
        {
            auto outFile = result.outputDirectory.getChildFile(stems[i].fileBaseName + ".wav");
            outFile.deleteFile();

            std::unique_ptr<juce::FileOutputStream> stream(outFile.createOutputStream());
            if (stream == nullptr)
            {
                allOk = false;
                result.errorMessage = "Could not open output stream for " + outFile.getFullPathName();
                continue;
            }

            std::unique_ptr<juce::AudioFormatWriter> writer(
                wavFormat.createWriterFor(stream.get(), sampleRate,
                                           (unsigned int) stems[i].audio.getNumChannels(), 24, {}, 0));
            if (writer == nullptr)
            {
                allOk = false;
                result.errorMessage = "Could not create WAV writer for " + outFile.getFullPathName();
                continue;
            }
            stream.release(); // writer now owns it

            writer->writeFromAudioSampleBuffer(stems[i].audio, 0, stems[i].audio.getNumSamples());
            result.writtenFiles[i] = outFile;
        }

        result.allSucceeded = allOk;
        return result;
    }
}
