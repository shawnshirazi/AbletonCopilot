#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace
{
    // Static, role-based default EQ per melody category - real mixing
    // convention, not adaptive/analyzed correction: bass stays low and
    // loses top-end brightness it doesn't need, leads/plucks/synths get
    // high-passed so they don't compete with the bass, pads get pushed
    // further back so they sit behind the lead. Fx/Other has no strong
    // convention (eqActive = false, see setMelodyTrackCategory).
    struct VoiceEqSetting { double hpHz; double shelfHz; double shelfDb; };
    const VoiceEqSetting kVoiceEqSettings[7] = { // indexed by (int) MelodyCategory
        { 30.0,  4000.0,  -3.0 }, // Bass
        { 150.0, 8000.0,   1.0 }, // Lead
        { 250.0, 6000.0,  -2.0 }, // Pad
        { 200.0, 10000.0,  1.5 }, // Pluck
        { 120.0, 9000.0,   0.5 }, // Synth
        { 0.0,   0.0,      0.0 }, // Fx - eqActive false
        { 0.0,   0.0,      0.0 }, // Other - eqActive false
    };
}

AbletonCopilotAudioProcessor::AbletonCopilotAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
                   #if ! JucePlugin_IsMidiEffect
                    #if ! JucePlugin_IsSynth
                     .withInput("Input",  juce::AudioChannelSet::stereo(), true)
                    #endif
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                   #endif
                     )
#endif
{
    drumFormatManager.registerBasicFormats();

    for (int r = 0; r < kDrumRows; ++r)
    {
        for (int s = 0; s < kDrumSteps; ++s)
            drumSteps[r][s].store(false, std::memory_order_relaxed);
        drumRowMuted[r].store(false, std::memory_order_relaxed);
        drumRowSolo[r].store(false, std::memory_order_relaxed);
    }

    juce::addDefaultFormatsToManager(pluginFormatManager);
    addMelodyTrack(); // track 0 — always present by default (the "Bass" voice)
}

AbletonCopilotAudioProcessor::~AbletonCopilotAudioProcessor()
{
    aliveFlag->store(false, std::memory_order_release);
    serumLoader.stopThread(4000);
}

const juce::String AbletonCopilotAudioProcessor::getName() const { return JucePlugin_Name; }

bool AbletonCopilotAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AbletonCopilotAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AbletonCopilotAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AbletonCopilotAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int  AbletonCopilotAudioProcessor::getNumPrograms()                            { return 1; }
int  AbletonCopilotAudioProcessor::getCurrentProgram()                         { return 0; }
void AbletonCopilotAudioProcessor::setCurrentProgram(int)                      {}
const juce::String AbletonCopilotAudioProcessor::getProgramName(int)           { return {}; }
void AbletonCopilotAudioProcessor::changeProgramName(int, const juce::String&) {}

void AbletonCopilotAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    analyzer.prepare(sampleRate, getTotalNumInputChannels());
    rmsSmoothed   = 0.0f;
    widthSmoothed = 0.0f;
    wasPlaying    = false;

    // Reset generated-drum synth voices on (re)prepare - their phase/filter
    // state is tied to a sample rate, so a stale mid-decay voice from a
    // previous sample rate would render wrong. They just restart cleanly on
    // next trigger.
    generatedDrumScratch.setSize(1, samplesPerBlock);
    for (auto& voice : generatedDrumSynthVoices)
        voice = Engine::DrumVoiceState {};

    const int trackCount = activeMelodyTrackCount.load(std::memory_order_relaxed);
    for (int t = 0; t < trackCount; ++t)
    {
        melodyVoices[t].scratch.setSize(2, samplesPerBlock);
        if (auto* serum = melodyVoices[t].instance.load(std::memory_order_acquire))
            serum->prepareToPlay(sampleRate, samplesPerBlock);
    }

    // Reset correction filter state for the new sample rate
    for (int ch = 0; ch < 2; ++ch)
    {
        corrSub[ch].reset();
        corrMid[ch].reset();
        corrAir[ch].reset();
    }

    // Rebuild correction filters with current params at the new sample rate
    if (corrActive.load())
    {
        CorrectionParams p;
        { juce::ScopedLock sl(corrLock); p = corrPending; }
        rebuildCorrFilters(sampleRate, p);
    }
}

void AbletonCopilotAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AbletonCopilotAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

//==============================================================================

void AbletonCopilotAudioProcessor::setMelodyTrackCategory(int trackIndex, MelodyCategory category)
{
    if (trackIndex < 0 || trackIndex >= kMaxMelodyTracks)
        return;

    auto& voice = melodyVoices[trackIndex];
    const auto& s = kVoiceEqSettings[(int) category];

    const bool active = category != MelodyCategory::Fx && category != MelodyCategory::Other;
    voice.eqActive.store(active, std::memory_order_relaxed);
    if (!active)
        return;

    const double sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;
    auto hp    = FeatureExtractor::makeHighPass(sr, s.hpHz, 0.707);
    auto shelf = FeatureExtractor::makeHighShelf(sr, s.shelfHz, 0.707, s.shelfDb);

    for (int ch = 0; ch < 2; ++ch)
    {
        voice.eqHp[ch]    = hp;    voice.eqHp[ch].reset();
        voice.eqShelf[ch] = shelf; voice.eqShelf[ch].reset();
    }
}

void AbletonCopilotAudioProcessor::rebuildCorrFilters(double sr, const CorrectionParams& p)
{
    auto sub = FeatureExtractor::makePeakingEQ(sr,  60.0,   1.0,  p.subBoostDb);
    auto mid = FeatureExtractor::makePeakingEQ(sr,  400.0,  1.0,  p.midBoostDb);
    auto air = FeatureExtractor::makeHighShelf( sr, 10000.0, 0.707, p.airBoostDb);

    for (int ch = 0; ch < 2; ++ch)
    {
        corrSub[ch] = sub;  corrSub[ch].reset();
        corrMid[ch] = mid;  corrMid[ch].reset();
        corrAir[ch] = air;  corrAir[ch].reset();
    }

    corrGainLin  = juce::Decibels::decibelsToGain(p.outputGainDb);
    corrWidthMul = p.widthScale;
}

void AbletonCopilotAudioProcessor::applyCorrections(const CorrectionParams& p)
{
    { juce::ScopedLock sl(corrLock); corrPending = p; }
    corrActive.store(true,  std::memory_order_release);
    corrDirty.store(true,   std::memory_order_release);
}

void AbletonCopilotAudioProcessor::clearCorrections()
{
    corrActive.store(false, std::memory_order_release);
}

CorrectionParams AbletonCopilotAudioProcessor::getCorrections() const
{
    juce::ScopedLock sl(corrLock);
    return corrPending;
}

//==============================================================================

void AbletonCopilotAudioProcessor::setDrumRowSample(int rowIndex, const juce::File& file)
{
    if (rowIndex < 0 || rowIndex >= kDrumRows)
        return;

    if (!file.existsAsFile())
    {
        juce::ScopedLock sl(drumBufferLock);
        drumRowBuffers[rowIndex].reset();
        return;
    }

    std::unique_ptr<juce::AudioFormatReader> reader(drumFormatManager.createReaderFor(file));
    if (reader == nullptr)
        return;

    auto buf = std::make_shared<juce::AudioBuffer<float>>(
        juce::jmax(1, (int) reader->numChannels), (int) reader->lengthInSamples);
    reader->read(buf.get(), 0, (int) reader->lengthInSamples, 0, true, true);

    juce::ScopedLock sl(drumBufferLock);
    drumRowBuffers[rowIndex] = buf;
}

void AbletonCopilotAudioProcessor::setDrumRowStep(int rowIndex, int step, bool isOn)
{
    if (rowIndex < 0 || rowIndex >= kDrumRows || step < 0 || step >= kDrumSteps)
        return;

    drumSteps[rowIndex][step].store(isOn, std::memory_order_relaxed);
}

void AbletonCopilotAudioProcessor::setDrumRowMuted(int rowIndex, bool muted)
{
    if (rowIndex < 0 || rowIndex >= kDrumRows)
        return;

    drumRowMuted[rowIndex].store(muted, std::memory_order_relaxed);
}

void AbletonCopilotAudioProcessor::setDrumRowSolo(int rowIndex, bool solo)
{
    if (rowIndex < 0 || rowIndex >= kDrumRows)
        return;

    // Guard against double-counting a redundant set-to-the-same-value call
    // so soloedRowCount stays an accurate O(1) "is anything soloed" check
    // for the audio thread.
    const bool wasSolo = drumRowSolo[rowIndex].exchange(solo, std::memory_order_relaxed);
    if (wasSolo == solo)
        return;

    if (solo)
        soloedRowCount.fetch_add(1, std::memory_order_relaxed);
    else
        soloedRowCount.fetch_sub(1, std::memory_order_relaxed);
}

void AbletonCopilotAudioProcessor::setMelodyPattern(int trackIndex,
                                                      const std::array<int8_t, kMelodySteps>& offsets,
                                                      int keyRoot)
{
    if (trackIndex < 0 || trackIndex >= kMaxMelodyTracks)
        return;

    auto& voice = melodyVoices[trackIndex];
    juce::ScopedLock sl(voice.lock);
    voice.offsets = offsets;
    voice.keyRoot = keyRoot;
}

void AbletonCopilotAudioProcessor::setMelodyTrackMuted(int trackIndex, bool muted)
{
    if (trackIndex < 0 || trackIndex >= kMaxMelodyTracks)
        return;

    melodyVoices[trackIndex].muted.store(muted, std::memory_order_relaxed);
}

void AbletonCopilotAudioProcessor::setMelodyTrackSolo(int trackIndex, bool solo)
{
    if (trackIndex < 0 || trackIndex >= kMaxMelodyTracks)
        return;

    // Guard against double-counting a redundant set-to-the-same-value call,
    // same pattern as setDrumRowSolo.
    const bool wasSolo = melodyVoices[trackIndex].solo.exchange(solo, std::memory_order_relaxed);
    if (wasSolo == solo)
        return;

    if (solo)
        soloedMelodyTrackCount.fetch_add(1, std::memory_order_relaxed);
    else
        soloedMelodyTrackCount.fetch_sub(1, std::memory_order_relaxed);
}

//==============================================================================
// Hosted Serum2 — hardcoded search-by-name for now (see chat: general
// instrument picker is a bigger, separate build). Search + instantiate run
// on a background thread since plugin creation can be slow; the result is
// handed to the message thread, which is the only thing that ever owns or
// destroys the AudioPluginInstance. The audio thread only ever reads the
// raw atomic pointer and never deletes it.
namespace
{
    juce::File findSerum2Vst3()
    {
        juce::Array<juce::File> searchDirs {
            juce::File("/Library/Audio/Plug-Ins/VST3"),
            juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                .getChildFile("Library/Audio/Plug-Ins/VST3"),
        };

        for (auto& dir : searchDirs)
        {
            if (!dir.isDirectory())
                continue;

            for (auto& f : dir.findChildFiles(juce::File::findDirectories, false, "*.vst3"))
                if (f.getFileNameWithoutExtension().containsIgnoreCase("serum"))
                    return f;
        }

        return {};
    }
}

juce::String AbletonCopilotAudioProcessor::getMelodyTrackStatus(int trackIndex) const
{
    if (trackIndex < 0 || trackIndex >= kMaxMelodyTracks)
        return {};

    auto& voice = melodyVoices[trackIndex];
    juce::ScopedLock sl(voice.statusLock);
    return voice.statusMessage;
}

bool AbletonCopilotAudioProcessor::isMelodyTrackLoaded(int trackIndex) const
{
    if (trackIndex < 0 || trackIndex >= kMaxMelodyTracks)
        return false;
    return melodyVoices[trackIndex].loaded.load(std::memory_order_relaxed);
}

void AbletonCopilotAudioProcessor::setGeneratedDrumPattern(const std::vector<GeneratedDrumRole>& roles)
{
    // Every role's velocity array is sized to the same grid (Engine::totalSteps
    // of whatever StepGridConfig the caller generated with) - use the first
    // non-empty one found as the step count the audio thread wraps against.
    int totalSteps = 0;
    for (const auto& role : roles)
    {
        if (!role.velocity.empty())
        {
            totalSteps = (int) role.velocity.size();
            break;
        }
    }

    juce::ScopedLock sl(generatedDrumLock);
    generatedDrumRoles      = roles;
    generatedDrumTotalSteps = totalSteps;
}

int AbletonCopilotAudioProcessor::addMelodyTrack()
{
    const int index = activeMelodyTrackCount.load(std::memory_order_relaxed);
    if (index >= kMaxMelodyTracks)
        return -1;

    melodyVoices[index].active.store(true, std::memory_order_relaxed);
    activeMelodyTrackCount.store(index + 1, std::memory_order_relaxed);
    loadSerum();
    return index;
}

void AbletonCopilotAudioProcessor::loadSerum()
{
    if (serumLoader.isThreadRunning())
        return;

    serumLoader.startThread();
}

void AbletonCopilotAudioProcessor::SerumLoaderThread::run()
{
    // Detailed step-by-step log for diagnosing load failures without needing
    // to relay UI text back and forth — read straight from disk after a run.
    auto logFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("AbletonCopilot/serum_load_log.txt");
    logFile.getParentDirectory().createDirectory();
    logFile.deleteFile();
    auto logLine = [&logFile](const juce::String& line)
    {
        logFile.appendText(juce::Time::getCurrentTime().toString(false, true, true, true)
                            + "  " + line + "\n");
    };

    auto setVoiceStatus = [](auto& voice, const juce::String& msg)
    {
        juce::ScopedLock sl(voice.statusLock);
        voice.statusMessage = msg;
    };

    logLine("=== Serum load attempt starting ===");

    auto path = findSerum2Vst3();
    logLine("Search result: " + (path.exists() ? path.getFullPathName() : juce::String("NOT FOUND")));

    const int trackCount = owner.activeMelodyTrackCount.load(std::memory_order_relaxed);

    if (!path.exists())
    {
        for (int i = 0; i < trackCount; ++i)
        {
            auto& voice = owner.melodyVoices[i];
            if (voice.loadAttempted.exchange(true, std::memory_order_relaxed))
                continue;
            setVoiceStatus(voice, "Serum 2 not found in ~/Library/Audio/Plug-Ins/VST3 or /Library/Audio/Plug-Ins/VST3");
        }
        return;
    }

    if (threadShouldExit())
        return;

    // Find the plugin description(s) once — reused to instantiate every
    // pending voice below, since they're all the same underlying plugin file.
    juce::AudioPluginFormat* foundFormat = nullptr;
    juce::Array<juce::PluginDescription> foundDescs;
    int totalDescriptionsFound = 0;

    logLine("Registered formats: " + juce::String(owner.pluginFormatManager.getNumFormats()));

    for (auto* format : owner.pluginFormatManager.getFormats())
    {
        if (threadShouldExit())
            return;

        logLine("Trying format: " + format->getName()
                + "  fileMightContainThisPluginType=" + (format->fileMightContainThisPluginType(path.getFullPathName()) ? "yes" : "no"));

        juce::OwnedArray<juce::PluginDescription> found;
        format->findAllTypesForFile(found, path.getFullPathName());
        logLine("  findAllTypesForFile -> " + juce::String(found.size()) + " description(s)");
        totalDescriptionsFound += found.size();

        for (auto* desc : found)
        {
            logLine("    - \"" + desc->name + "\" isInstrument=" + (desc->isInstrument ? "yes" : "no")
                    + " numInputChannels=" + juce::String(desc->numInputChannels)
                    + " numOutputChannels=" + juce::String(desc->numOutputChannels)
                    + " uniqueId=" + juce::String(desc->uniqueId));
            foundDescs.add(*desc);
        }

        if (!found.isEmpty())
        {
            foundFormat = format;
            break;
        }
    }

    if (foundFormat == nullptr)
    {
        juce::String reason = "no plugin types found in " + path.getFullPathName();
        logLine("=== FAILED: " + reason + " ===");
        for (int i = 0; i < trackCount; ++i)
        {
            auto& voice = owner.melodyVoices[i];
            if (voice.loadAttempted.exchange(true, std::memory_order_relaxed))
                continue;
            setVoiceStatus(voice, "Could not load Serum 2: " + reason);
        }
        return;
    }

    // Instantiate one independent AudioPluginInstance per pending voice — we
    // already know this exact file is Serum2, so (as before) we don't filter
    // candidate descriptions by isInstrument; just try them in order.
    for (int i = 0; i < trackCount; ++i)
    {
        if (threadShouldExit())
            return;

        auto& voice = owner.melodyVoices[i];
        if (voice.loadAttempted.exchange(true, std::memory_order_relaxed))
            continue; // already loaded (or attempted) in an earlier run of this thread

        std::unique_ptr<juce::AudioPluginInstance> instance;
        juce::String errorMessage;

        for (auto& desc : foundDescs)
        {
            if (threadShouldExit())
                return;

            errorMessage.clear();
            instance = foundFormat->createInstanceFromDescription(
                desc, owner.getSampleRate() > 0 ? owner.getSampleRate() : 44100.0,
                owner.getBlockSize() > 0 ? owner.getBlockSize() : 512, errorMessage);
            logLine("    [track " + juce::String(i) + "] createInstanceFromDescription(\"" + desc.name + "\") -> "
                    + (instance != nullptr ? "SUCCESS" : "failed, error=\"" + errorMessage + "\""));
            if (instance != nullptr)
                break;
        }

        if (instance == nullptr)
        {
            juce::String reason = errorMessage.isNotEmpty()
                ? errorMessage
                : "found " + juce::String(totalDescriptionsFound)
                      + " plugin type(s) but none could be instantiated";
            logLine("=== FAILED for track " + juce::String(i) + ": " + reason + " ===");
            setVoiceStatus(voice, "Could not load Serum 2: " + reason);
            continue;
        }

        instance->prepareToPlay(owner.getSampleRate() > 0 ? owner.getSampleRate() : 44100.0,
                                owner.getBlockSize() > 0 ? owner.getBlockSize() : 512);

        if (threadShouldExit())
            return;

        logLine("=== SUCCESS for track " + juce::String(i) + " ===");
        setVoiceStatus(voice, "Serum 2 loaded");

        auto flag = owner.aliveFlag;
        juce::MessageManager::callAsync([&voice, flag, inst = std::move(instance)]() mutable
        {
            if (!flag->load(std::memory_order_acquire))
                return; // processor was destroyed before this landed

            if (voice.pendingState.getSize() > 0)
                inst->setStateInformation(voice.pendingState.getData(),
                                          (int) voice.pendingState.getSize());

            // One-off diagnostic: does Serum 2 expose its own preset library
            // through the standard VST program-list API? If getNumPrograms()
            // is >1 and setCurrentProgram() actually changes getStateInformation()'s
            // bytes, we can drive Serum 2's real preset browser directly by
            // index instead of round-tripping captured state — the "filter
            // through the bass sounds like before" behaviour the user wants,
            // but working this time. If it reports 1 program, Serum 2 doesn't
            // bridge its browser to the host at all and this path is a dead end.
            {
                auto* raw = inst.get();
                auto dbgFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                   .getChildFile("AbletonCopilot/serum_program_list_debug.txt");
                dbgFile.getParentDirectory().createDirectory();
                juce::String out;
                out << "=== " << juce::Time::getCurrentTime().toString(true, true, true, true) << " ===\n";
                const int numPrograms = raw->getNumPrograms();
                out << "getNumPrograms() = " << numPrograms << "\n";
                out << "getCurrentProgram() = " << raw->getCurrentProgram() << "\n";
                const int toList = juce::jmin(numPrograms, 40);
                for (int p = 0; p < toList; ++p)
                    out << "  [" << p << "] " << raw->getProgramName(p) << "\n";

                juce::MemoryBlock before;
                raw->getStateInformation(before);
                if (numPrograms > 1)
                {
                    raw->setCurrentProgram(numPrograms > 1 ? 1 : 0);
                    juce::MemoryBlock after;
                    raw->getStateInformation(after);
                    out << "setCurrentProgram(1) changed state bytes? "
                        << (before == after ? "NO (identical - dead end)" : "YES (state actually changed!)") << "\n";
                    raw->setCurrentProgram(0); // restore
                }
                dbgFile.appendText(out);
            }

            voice.ownerInstance = std::move(inst);
            voice.instance.store(voice.ownerInstance.get(), std::memory_order_release);
            voice.loaded.store(true, std::memory_order_relaxed);
        });
    }
}

//==============================================================================

void AbletonCopilotAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Manual triggering: incoming note-ons at our GM drum-map notes hit the
    // same synth voices the generated pattern uses below (shared
    // triggerDrumVoice - see Engine/DrumVoiceSynth.h), on any channel. Read
    // before we clear/replace midiMessages, since we're a generator, not a
    // pass-through.
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (!msg.isNoteOn())
            continue;

        Engine::DrumRole role;
        if (Engine::drumRoleForGmNote(msg.getNoteNumber(), role))
            Engine::triggerDrumVoice(generatedDrumSynthVoices[(int) role], role,
                                      msg.getFloatVelocity(), getSampleRate());
    }

    midiMessages.clear(); // we're a generator, not a pass-through

    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // --- Transport detection ---
    bool isPlayingNow = false;
    if (auto* head = getPlayHead())
    {
        if (auto pos = head->getPosition())
        {
            isPlayingNow = pos->getIsPlaying();
            if (auto bpmVal = pos->getBpm())
                currentBpm.store((float)*bpmVal, std::memory_order_relaxed);
        }
    }

    if (isPlayingNow && !wasPlaying)
        analyzer.resetCapture();
    else if (!isPlayingNow && wasPlaying)
        playbackJustStopped.store(true, std::memory_order_release);

    currentlyPlaying.store(isPlayingNow, std::memory_order_relaxed);
    wasPlaying = isPlayingNow;

    // --- Drum machine: step-trigger against host PPQ, then mix one-shots in ---
    // Step boundaries are only checked once per block (fine as long as a block
    // is shorter than a 16th note, true for any normal block size/tempo), and
    // a step fires from the block that contains its boundary rather than at
    // the exact sample — a few ms of quantization, not sample-accurate.
    {
        if (isPlayingNow)
        {
            double ppq = 0.0;
            if (auto* head = getPlayHead())
                if (auto pos = head->getPosition())
                    if (auto ppqOpt = pos->getPpqPosition())
                        ppq = *ppqOpt;

            const int stepFloor   = (int) std::floor(ppq * 4.0); // 16th notes
            const int wrappedStep = ((stepFloor % kDrumSteps) + kDrumSteps) % kDrumSteps;

            const int  soloCount = soloedRowCount.load(std::memory_order_relaxed);

            for (int row = 0; row < kDrumRows; ++row)
            {
                auto& voice = drumVoices[row];
                if (wrappedStep != voice.lastStepIndex)
                {
                    voice.lastStepIndex = wrappedStep;
                    const bool audible = !drumRowMuted[row].load(std::memory_order_relaxed)
                                       && (soloCount == 0 || drumRowSolo[row].load(std::memory_order_relaxed));
                    if (audible && drumSteps[row][wrappedStep].load(std::memory_order_relaxed))
                        voice.readPos = 0;
                }
            }
        }
        else
        {
            for (auto& voice : drumVoices)
                voice.lastStepIndex = -1;
        }

        const int nSamplesDrum = buffer.getNumSamples();
        const int nChDrum      = std::min(2, buffer.getNumChannels());

        for (int row = 0; row < kDrumRows; ++row)
        {
            auto& voice = drumVoices[row];
            if (voice.readPos < 0)
                continue;

            std::shared_ptr<juce::AudioBuffer<float>> buf;
            {
                juce::ScopedLock sl(drumBufferLock);
                buf = drumRowBuffers[row];
            }
            if (buf == nullptr || buf->getNumSamples() == 0)
            {
                voice.readPos = -1;
                continue;
            }

            const int remaining = buf->getNumSamples() - voice.readPos;
            const int n         = std::min(nSamplesDrum, remaining);
            const int srcCh     = buf->getNumChannels();

            if (!suppressOwnPlayback.load(std::memory_order_relaxed))
                for (int ch = 0; ch < nChDrum; ++ch)
                    buffer.addFrom(ch, 0, *buf, std::min(ch, srcCh - 1), voice.readPos, n);

            voice.readPos += n;
            if (voice.readPos >= buf->getNumSamples())
                voice.readPos = -1;
        }
    }

    // --- Deterministic drum-MIDI output (Generate Drum Pattern button, see
    // Source/Engine/DrumEngine.h) -- emits real MIDI notes to the host,
    // independent of the sample-based drum machine above. Same step-
    // trigger-against-host-PPQ approach used there and previously proven in
    // the AbletonCopilotMIDI companion plugin, now ported in-process so
    // AbletonCopilot itself can drive a downstream instrument without a
    // separate plugin. Each role gets its own short-gated voice (~40ms), not
    // held-until-next-event, since drum hits are strikes, not sustained notes.
    {
        std::vector<GeneratedDrumRole> localRoles;
        int localTotalSteps = 0;
        {
            juce::ScopedLock sl(generatedDrumLock);
            localRoles      = generatedDrumRoles;
            localTotalSteps = generatedDrumTotalSteps;
        }

        if (localTotalSteps > 0)
        {
            if (isPlayingNow)
            {
                double ppq = 0.0;
                if (auto* head = getPlayHead())
                    if (auto pos = head->getPosition())
                        if (auto ppqOpt = pos->getPpqPosition())
                            ppq = *ppqOpt;

                const int stepFloor   = (int) std::floor(ppq * 4.0);
                const int wrappedStep = ((stepFloor % localTotalSteps) + localTotalSteps) % localTotalSteps;

                if (wrappedStep != generatedDrumLastStepIndex)
                {
                    generatedDrumLastStepIndex = wrappedStep;

                    for (size_t r = 0; r < localRoles.size() && r < (size_t) kMaxGeneratedDrumRoles; ++r)
                    {
                        const auto& role = localRoles[r];
                        if (role.midiNote < 0 || wrappedStep >= (int) role.velocity.size())
                            continue;

                        const int vel = role.velocity[(size_t) wrappedStep];
                        if (vel <= 0)
                            continue;

                        // Primary output: trigger the internal synth voice
                        // for this hit - same trigger function, same voice
                        // array, as the incoming-MIDI path above, keyed by
                        // GM note so this doesn't depend on localRoles'
                        // array order.
                        Engine::DrumRole synthRole;
                        if (Engine::drumRoleForGmNote(role.midiNote, synthRole))
                            Engine::triggerDrumVoice(generatedDrumSynthVoices[(int) synthRole], synthRole,
                                                      (float) vel / 127.0f, getSampleRate());

                        // Optional secondary output: real MIDI note to the
                        // host, kept for downstream routing but not
                        // required to hear anything (see the render/mix
                        // pass below).
                        auto& voice = generatedDrumVoices[r];
                        if (voice.noteOn)
                        {
                            midiMessages.addEvent(juce::MidiMessage::noteOff(10, voice.pitch), 0);
                            voice.noteOn = false;
                        }

                        midiMessages.addEvent(juce::MidiMessage::noteOn(10, role.midiNote, (juce::uint8) vel), 0);
                        voice.noteOn          = true;
                        voice.pitch           = role.midiNote;
                        voice.samplesUntilOff = juce::jmax(1, (int) (0.04 * getSampleRate()));
                    }
                }
            }
            else
            {
                generatedDrumLastStepIndex = -1;
            }
        }

        const int numSamplesForGate = buffer.getNumSamples();
        for (int r = 0; r < kMaxGeneratedDrumRoles; ++r)
        {
            auto& voice = generatedDrumVoices[r];
            if (!voice.noteOn)
                continue;

            voice.samplesUntilOff -= numSamplesForGate;
            if (voice.samplesUntilOff <= 0)
            {
                const int offset = juce::jlimit(0, juce::jmax(0, numSamplesForGate - 1),
                                                 numSamplesForGate + voice.samplesUntilOff);
                midiMessages.addEvent(juce::MidiMessage::noteOff(10, voice.pitch), offset);
                voice.noteOn = false;
            }
        }
    }

    // --- Generated-drum synth rendering: mixes generatedDrumSynthVoices
    // (Engine::DrumVoiceSynth) into our own output. Deliberately outside
    // and independent of the pattern/transport gating above - a voice
    // triggered by incoming MIDI (or still decaying from a step a few
    // blocks ago) must keep rendering even with no pattern generated yet or
    // the transport stopped. This is the primary way the generated pattern
    // is heard; the MIDI events above are optional and not required.
    {
        auto*     scratch = generatedDrumScratch.getWritePointer(0);
        const int n       = buffer.getNumSamples();
        const int nCh     = std::min(2, buffer.getNumChannels());

        for (auto& voice : generatedDrumSynthVoices)
        {
            if (!voice.active)
                continue;

            Engine::renderDrumVoice(voice, scratch, n);

            if (!suppressOwnPlayback.load(std::memory_order_relaxed))
                for (int ch = 0; ch < nCh; ++ch)
                    buffer.addFrom(ch, 0, scratch, n);
        }
    }

    // --- Melody voices -> their own hosted Serum2 instances, each mixed
    // straight into our own output. Same PPQ step-trigger idea as the drum
    // voices above, just targeting a hosted instrument's MIDI input instead
    // of a one-shot sample buffer. All active voices share one host PPQ
    // read per block (no behaviour change vs. a single voice, just avoids
    // redundant getPosition() calls).
    {
        double ppq = 0.0;
        if (auto* head = getPlayHead())
            if (auto pos = head->getPosition())
                if (auto ppqOpt = pos->getPpqPosition())
                    ppq = *ppqOpt;

        const int trackCount    = activeMelodyTrackCount.load(std::memory_order_relaxed);
        const int soloTrackCount = soloedMelodyTrackCount.load(std::memory_order_relaxed);

        for (int t = 0; t < trackCount; ++t)
        {
            auto& voice = melodyVoices[t];
            auto* serum = voice.instance.load(std::memory_order_acquire);
            if (serum == nullptr)
                continue;

            std::array<int8_t, kMelodySteps> localMelody;
            int localMelodyRoot;
            {
                juce::ScopedLock sl(voice.lock);
                localMelody     = voice.offsets;
                localMelodyRoot = voice.keyRoot;
            }

            // Mute/solo gate only the triggering of new notes — same
            // convention as the drum rows: a note already sounding keeps
            // ringing out via its own note-off below rather than being cut
            // off abruptly.
            const bool audible = !voice.muted.load(std::memory_order_relaxed)
                               && (soloTrackCount == 0 || voice.solo.load(std::memory_order_relaxed));

            juce::MidiBuffer serumMidi;

            if (isPlayingNow)
            {
                const int stepFloor   = (int) std::floor(ppq * 4.0);
                const int wrappedStep = ((stepFloor % kMelodySteps) + kMelodySteps) % kMelodySteps;

                if (wrappedStep != voice.lastStepIndex)
                {
                    voice.lastStepIndex = wrappedStep;

                    if (voice.noteOn)
                    {
                        serumMidi.addEvent(juce::MidiMessage::noteOff(1, voice.soundingPitch), 0);
                        voice.noteOn = false;
                    }

                    const int8_t offset = localMelody[(size_t) wrappedStep];
                    if (offset != kMelodyOffValue && audible)
                    {
                        const int pitch = juce::jlimit(0, 127, 36 + localMelodyRoot + offset);
                        serumMidi.addEvent(juce::MidiMessage::noteOn(1, pitch, (juce::uint8) 100), 0);
                        voice.noteOn        = true;
                        voice.soundingPitch = pitch;
                    }
                }
            }
            else if (voice.noteOn)
            {
                serumMidi.addEvent(juce::MidiMessage::noteOff(1, voice.soundingPitch), 0);
                voice.noteOn        = false;
                voice.lastStepIndex = -1;
            }

            const int numSamples = buffer.getNumSamples();
            voice.scratch.setSize(2, numSamples, false, false, true);
            voice.scratch.clear();
            serum->processBlock(voice.scratch, serumMidi);

            // Static, role-based default EQ - shapes this voice's own tone
            // before it's mixed with anything else, regardless of whether
            // suppression means this block ends up audible.
            if (genreEqEnabled.load(std::memory_order_relaxed) && voice.eqActive.load(std::memory_order_relaxed))
            {
                const int nChEq = std::min(2, voice.scratch.getNumChannels());
                for (int ch = 0; ch < nChEq; ++ch)
                {
                    auto* data = voice.scratch.getWritePointer(ch);
                    for (int n = 0; n < numSamples; ++n)
                        data[n] = voice.eqShelf[ch].process(voice.eqHp[ch].process(data[n]));
                }
            }

            if (!suppressOwnPlayback.load(std::memory_order_relaxed))
            {
                const int nChSerum = std::min({ 2, buffer.getNumChannels(), voice.scratch.getNumChannels() });
                for (int ch = 0; ch < nChSerum; ++ch)
                    buffer.addFrom(ch, 0, voice.scratch, ch, 0, numSamples);
            }
        }
    }

    // --- Rebuild correction filters if params changed (cheap path most blocks) ---
    if (corrDirty.load(std::memory_order_acquire))
    {
        CorrectionParams p;
        {
            juce::ScopedLock sl(corrLock);
            p = corrPending;
        }
        rebuildCorrFilters(getSampleRate(), p);
        corrDirty.store(false, std::memory_order_release);
    }

    // --- Capture raw input for analysis (before any DSP) ---
    if (isPlayingNow)
        analyzer.pushSamples(buffer);

    // --- Apply correction chain ---
    if (corrActive.load(std::memory_order_relaxed))
    {
        const int nSamples = buffer.getNumSamples();
        const int nCh      = buffer.getNumChannels();

        // Per-channel EQ
        for (int ch = 0; ch < nCh; ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            int    idx  = std::min(ch, 1);
            for (int i = 0; i < nSamples; ++i)
            {
                float s = corrSub[idx].process(data[i]);
                s = corrMid[idx].process(s);
                s = corrAir[idx].process(s);
                data[i] = s;
            }
        }

        // M/S stereo width
        if (nCh >= 2 && corrWidthMul != 1.0f)
        {
            float* L = buffer.getWritePointer(0);
            float* R = buffer.getWritePointer(1);
            for (int i = 0; i < nSamples; ++i)
            {
                float m = (L[i] + R[i]) * 0.5f;
                float s = (L[i] - R[i]) * 0.5f * corrWidthMul;
                L[i] = m + s;
                R[i] = m - s;
            }
        }

        // Output gain
        if (corrGainLin != 1.0f)
            buffer.applyGain(corrGainLin);

        // Brick-wall limiter at -0.3 dBFS to prevent post-gain clipping
        const float ceiling = juce::Decibels::decibelsToGain(-0.3f);
        for (int ch = 0; ch < nCh; ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < nSamples; ++i)
                data[i] = std::max(-ceiling, std::min(ceiling, data[i]));
        }
    }

    // --- Live meters (reflect corrected output) ---
    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();
    float peak = 0.0f;

    double blockMeanSq = 0.0;
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* data = buffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float s = data[i];
            peak = std::max(peak, std::abs(s));
            blockMeanSq += (double)s * s;
        }
    }
    blockMeanSq /= (numCh * numSamples);

    double widthVal = 0.0;
    if (numCh >= 2)
    {
        const float* L = buffer.getReadPointer(0);
        const float* R = buffer.getReadPointer(1);
        double midSq = 0.0, sideSq = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            double m = (L[i] + R[i]) * 0.5;
            double s = (L[i] - R[i]) * 0.5;
            midSq  += m * m;
            sideSq += s * s;
        }
        double total = midSq + sideSq;
        if (total > 0.0)
            widthVal = std::sqrt(sideSq / total);
    }

    const float alphaSlow = 0.05f;
    const float alphaFast = 0.15f;
    rmsSmoothed   = alphaSlow * (float)blockMeanSq + (1.0f - alphaSlow) * rmsSmoothed;
    widthSmoothed = alphaFast * (float)widthVal    + (1.0f - alphaFast) * widthSmoothed;

    float rmsDb = rmsSmoothed > 1e-10f
                  ? juce::Decibels::gainToDecibels(std::sqrt(rmsSmoothed))
                  : -99.0f;

    liveRmsDb.store(rmsDb, std::memory_order_relaxed);
    liveStereoWidth.store(widthSmoothed, std::memory_order_relaxed);
    livePeakDb.store(peak > 0.0f ? juce::Decibels::gainToDecibels(peak) : -99.0f,
                     std::memory_order_relaxed);
}

bool AbletonCopilotAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* AbletonCopilotAudioProcessor::createEditor()
{
    return new AbletonCopilotAudioProcessorEditor(*this);
}

void AbletonCopilotAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Each active voice's raw Serum2 state, length-prefixed sequentially —
    // we don't have any other persistent parameters yet. Prompt text / track
    // category labels aren't restored by this (cosmetic only); the audio
    // itself (preset + pattern + key, all baked into each voice's own
    // state) is what matters and does round-trip.
    juce::MemoryOutputStream stream(destData, false);

    const int trackCount = activeMelodyTrackCount.load(std::memory_order_relaxed);
    stream.writeInt(trackCount);

    for (int t = 0; t < trackCount; ++t)
    {
        juce::MemoryBlock voiceState;
        if (auto* serum = melodyVoices[t].instance.load(std::memory_order_acquire))
            serum->getStateInformation(voiceState);

        stream.writeInt((int) voiceState.getSize());
        if (voiceState.getSize() > 0)
            stream.write(voiceState.getData(), voiceState.getSize());
    }
}

void AbletonCopilotAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, (size_t) sizeInBytes, false);

    const int trackCount = stream.readInt();
    for (int t = 0; t < trackCount && t < kMaxMelodyTracks; ++t)
    {
        const int stateSize = stream.readInt();
        if (stateSize <= 0)
            continue;

        juce::MemoryBlock voiceState((size_t) stateSize);
        stream.read(voiceState.getData(), stateSize);

        // Serum2 may not have finished loading yet when the host calls this
        // (e.g. right after construction, on project load) — stash it and
        // the loader thread's completion callback will apply it once ready.
        melodyVoices[t].pendingState = voiceState;

        if (auto* serum = melodyVoices[t].instance.load(std::memory_order_acquire))
            serum->setStateInformation(voiceState.getData(), (int) voiceState.getSize());
    }
}

bool AbletonCopilotAudioProcessor::captureMelodyTrackState(int trackIndex, juce::MemoryBlock& outState) const
{
    if (trackIndex < 0 || trackIndex >= kMaxMelodyTracks)
        return false;

    auto* serum = melodyVoices[trackIndex].instance.load(std::memory_order_acquire);
    if (serum == nullptr)
        return false;

    outState.reset();
    serum->getStateInformation(outState);
    return outState.getSize() > 0;
}

bool AbletonCopilotAudioProcessor::loadCapturedPreset(int trackIndex, const juce::File& captureFile)
{
    if (trackIndex < 0 || trackIndex >= kMaxMelodyTracks)
        return false;

    auto* serum = melodyVoices[trackIndex].instance.load(std::memory_order_acquire);
    if (serum == nullptr)
        return false;

    juce::MemoryBlock state;
    if (!captureFile.loadFileAsData(state) || state.getSize() == 0)
        return false;

    // No parsing, no format translation — this is genuinely Serum2's own
    // state, captured earlier via captureMelodyTrackState(), fed straight
    // back the same way a DAW session save/reload would.
    serum->setStateInformation(state.getData(), (int) state.getSize());
    return true;
}

juce::AudioPluginInstance* AbletonCopilotAudioProcessor::getHostedSerumInstance(int trackIndex) const noexcept
{
    if (trackIndex < 0 || trackIndex >= kMaxMelodyTracks)
        return nullptr;
    return melodyVoices[trackIndex].instance.load(std::memory_order_acquire);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AbletonCopilotAudioProcessor();
}
