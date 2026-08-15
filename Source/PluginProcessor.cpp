#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Engine/Grid.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace
{
    // Real swing (Part 4 of the groove-improvement pass): sourced from two
    // independent production guides that converge on the same range -
    // Beatportal's ARTBAT/Anyma-style guide ("apply 8-12% swing to your
    // hi-hats and percussion while keeping your kick and bass on strict,
    // straight quantisation") and Studio Brootle's techno drum guide
    // ("SP1200 16 Swing-67 at 10%"). 10% is the midpoint of the sourced
    // range, applied only to hatClosed/percA/percB - kick/clap/bass keep
    // triggering exactly on the step-boundary detection below, unchanged.
    constexpr float kSwingAmount = 0.10f;

    bool synthRoleIsSwung(int midiNote)
    {
        Engine::DrumRole role;
        if (!Engine::drumRoleForGmNote(midiNote, role))
            return false;
        return role == Engine::DrumRole::HatClosed
            || role == Engine::DrumRole::PercA
            || role == Engine::DrumRole::PercB;
    }

    // Extra delay (in samples) swing adds to this step vs. a straight grid -
    // Engine::stepTimeSeconds (Source/Engine/Grid.h) already computes the
    // real swing-aware onset time (delays only odd/off-beat 16ths); this
    // is just the DIFFERENCE between the swung and straight timing for the
    // one step in question, converted to samples. wrappedStep/totalSteps
    // determine bars-per-loop implicitly (stepsPerBar is always 16 in this
    // codebase's step-grid convention - see kDrumSteps/kMelodySteps/
    // Engine::kLoopStepsPerBar, all 16), bpm comes from the host transport.
    int swingDelaySamples(int wrappedStep, double bpm, double sampleRate)
    {
        if (bpm <= 0.0 || sampleRate <= 0.0)
            return 0;
        Engine::StepGridConfig straight; straight.swing = 0.0f;
        Engine::StepGridConfig swung;    swung.swing    = kSwingAmount;
        const double deltaSec = Engine::stepTimeSeconds(wrappedStep, bpm, swung)
                               - Engine::stepTimeSeconds(wrappedStep, bpm, straight);
        return juce::jmax(0, (int) std::lround(deltaSec * sampleRate));
    }
}

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
    StartupTiming::reset(); // earliest point in a load cycle - fresh log for this load
    StartupTiming::mark("AudioProcessor ctor start");

    drumFormatManager.registerBasicFormats();

    for (int r = 0; r < kDrumRows; ++r)
    {
        for (int s = 0; s < kDrumSteps; ++s)
            drumSteps[r][s].store(false, std::memory_order_relaxed);
        drumRowMuted[r].store(false, std::memory_order_relaxed);
        drumRowSolo[r].store(false, std::memory_order_relaxed);
    }

    juce::addDefaultFormatsToManager(pluginFormatManager);
    addMelodyTrack(); // track 0 — always present by default (the "Bass" voice) - starts loadSerum() on a background thread, does not block here
    addMelodyTrack(); // track 1 — always present by default (the "Melody" voice, loop-generator workflow) - so its Serum2 instance is already loading before the user ever clicks Generate, per "the generated bass workflow should use the captured/known Serum 2 state immediately rather than waiting"

    for (auto& muted : generatedDrumRoleMuted)
        muted.store(false, std::memory_order_relaxed);
    for (auto& gain : generatedDrumRoleGain)
        gain.store(1.0f, std::memory_order_relaxed);

    StartupTiming::mark("AudioProcessor ctor end");
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
    for (auto& voice : generatedSampleVoices)
        voice = GeneratedSampleVoiceState {};

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

void AbletonCopilotAudioProcessor::setGeneratedMelodyPattern(int trackIndex, const std::vector<int8_t>& offsets,
                                                                int keyRoot, const std::vector<int8_t>& gateLengthSteps)
{
    if (trackIndex < 0 || trackIndex >= kMaxMelodyTracks)
        return;

    auto& voice = melodyVoices[trackIndex];
    juce::ScopedLock sl(voice.lock);
    voice.generatedOffsets         = offsets;
    voice.generatedTotalSteps      = (int) offsets.size();
    voice.keyRoot                  = keyRoot;
    voice.generatedGateLengthSteps = gateLengthSteps; // empty for every existing caller except the new bass path - zero behaviour change for them
}

void AbletonCopilotAudioProcessor::setGeneratedDrumRoleMuted(int role, bool muted)
{
    if (role < 0 || role >= kMaxGeneratedDrumRoles)
        return;
    generatedDrumRoleMuted[role].store(muted, std::memory_order_relaxed);
}

void AbletonCopilotAudioProcessor::setGeneratedDrumRoleGain(int role, float gain)
{
    if (role < 0 || role >= kMaxGeneratedDrumRoles)
        return;
    generatedDrumRoleGain[role].store(gain, std::memory_order_relaxed);
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

AbletonCopilotAudioProcessor::MelodyVoiceDiagnostics
    AbletonCopilotAudioProcessor::getMelodyVoiceDiagnostics(int trackIndex) const
{
    MelodyVoiceDiagnostics d;
    if (trackIndex < 0 || trackIndex >= kMaxMelodyTracks)
        return d;

    auto& voice = melodyVoices[trackIndex];
    d.serumInstanceLoaded  = voice.instance.load(std::memory_order_acquire) != nullptr;
    d.capturedPresetActive = voice.capturedPresetActive.load(std::memory_order_acquire);
    d.noteOnEventsSent    = voice.noteOnEventsSent.load(std::memory_order_relaxed);
    d.lastBlockPeakOut    = voice.lastBlockPeakOut.load(std::memory_order_relaxed);
    d.suppressOwnPlayback = suppressOwnPlayback.load(std::memory_order_relaxed);
    {
        juce::ScopedLock sl(voice.lock);
        d.generatedTotalSteps    = voice.generatedTotalSteps;
        d.generatedPatternActive = !voice.generatedOffsets.empty() && voice.generatedTotalSteps > 0;
        d.generatedGateLengthCount = (int) std::count_if(voice.generatedGateLengthSteps.begin(),
                                                            voice.generatedGateLengthSteps.end(),
                                                            [](int8_t v) { return v != 0; });
    }
    return d;
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

    // Load (or clear) each role's chosen sample, same
    // read-file-into-a-shared-buffer approach as setDrumRowSample() below
    // - reusing drumFormatManager, no second sample-loading system. A role
    // with no sample (empty/nonexistent File, i.e. the sample-selection
    // layer found nothing suitable) gets its buffer cleared, which is what
    // tells processBlock to fall back to DrumVoiceSynth for that role.
    {
        std::shared_ptr<juce::AudioBuffer<float>> loaded[kMaxGeneratedDrumRoles];
        juce::File                                loadedFile[kMaxGeneratedDrumRoles]; // stays invalid File() unless the load below actually succeeds
        GeneratedRoleLoadDiagnostics               diag[kMaxGeneratedDrumRoles]; // captured at each real check below, never reconstructed after the fact

        for (const auto& role : roles)
        {
            Engine::DrumRole resolvedRole;
            if (!Engine::drumRoleForGmNote(role.midiNote, resolvedRole))
                continue;

            GeneratedRoleLoadDiagnostics d;
            d.candidateFile = role.sampleFile;
            d.extension     = role.sampleFile.getFileExtension();

            for (int fmt = 0; fmt < drumFormatManager.getNumKnownFormats(); ++fmt)
            {
                auto* format = drumFormatManager.getKnownFormat(fmt);
                if (format != nullptr && format->getFileExtensions().contains(d.extension, true))
                {
                    d.formatRecognized     = true;
                    d.recognizedFormatName = format->getFormatName();
                    break;
                }
            }

            d.candidateExists = role.sampleFile.existsAsFile();
            if (!d.candidateExists)
            {
                diag[(int) resolvedRole] = d;
                continue;
            }

            std::unique_ptr<juce::AudioFormatReader> reader(drumFormatManager.createReaderFor(role.sampleFile));
            d.readerCreated = (reader != nullptr);
            if (reader == nullptr)
            {
                diag[(int) resolvedRole] = d; // selector suggested a file, but it couldn't actually be decoded - stays a synth fallback, not a silent lie
                continue;
            }

            d.decodedLengthSamples = reader->lengthInSamples;

            auto buf = std::make_shared<juce::AudioBuffer<float>>(
                juce::jmax(1, (int) reader->numChannels), (int) reader->lengthInSamples);
            reader->read(buf.get(), 0, (int) reader->lengthInSamples, 0, true, true);

            loaded[(int) resolvedRole]     = buf;
            loadedFile[(int) resolvedRole] = role.sampleFile; // only set on an actual successful decode - this IS what's now in generatedRoleSampleBuffers
            d.finalLoadedFile              = role.sampleFile;
            diag[(int) resolvedRole]       = d;
        }

        juce::ScopedLock sampleLock(generatedSampleLock);
        for (int r = 0; r < kMaxGeneratedDrumRoles; ++r)
        {
            generatedRoleSampleBuffers[r]    = loaded[r];
            generatedRoleLoadedFile[r]       = loadedFile[r];
            generatedRoleLoadDiagnostics[r]  = diag[r];
        }
    }

    juce::ScopedLock sl(generatedDrumLock);
    generatedDrumRoles      = roles;
    generatedDrumTotalSteps = totalSteps;
}

juce::File AbletonCopilotAudioProcessor::getGeneratedRoleLoadedFile(Engine::DrumRole role) const
{
    juce::ScopedLock sl(generatedSampleLock);
    return generatedRoleLoadedFile[(int) role];
}

AbletonCopilotAudioProcessor::GeneratedRoleLoadDiagnostics
    AbletonCopilotAudioProcessor::getGeneratedRoleLoadDiagnostics(Engine::DrumRole role) const
{
    juce::ScopedLock sl(generatedSampleLock);
    return generatedRoleLoadDiagnostics[(int) role];
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

void AbletonCopilotAudioProcessor::triggerGeneratedRole(Engine::DrumRole role, float velocity01)
{
    std::shared_ptr<juce::AudioBuffer<float>> buf;
    {
        juce::ScopedLock sl(generatedSampleLock);
        buf = generatedRoleSampleBuffers[(int) role];
    }

    if (buf != nullptr && buf->getNumSamples() > 0)
    {
        auto& voice   = generatedSampleVoices[(int) role];
        voice.readPos = 0;
        voice.gain    = juce::jlimit(0.0f, 1.0f, velocity01);
    }
    else
    {
        Engine::triggerDrumVoice(generatedDrumSynthVoices[(int) role], role, velocity01, getSampleRate());
    }
}

void AbletonCopilotAudioProcessor::fireGeneratedRoleNote(int r, int midiNote, float velocity01,
                                                            juce::MidiBuffer& midiMessages)
{
    // Primary output: trigger this role's real sample if one was
    // selected/loaded, otherwise DrumVoiceSynth (see triggerGeneratedRole).
    Engine::DrumRole synthRole;
    if (Engine::drumRoleForGmNote(midiNote, synthRole))
        triggerGeneratedRole(synthRole, velocity01);

    // Optional secondary output: real MIDI note to the host, kept for
    // downstream routing but not required to hear anything.
    if (r < 0 || r >= kMaxGeneratedDrumRoles)
        return;
    auto& voice = generatedDrumVoices[r];
    if (voice.noteOn)
    {
        midiMessages.addEvent(juce::MidiMessage::noteOff(10, voice.pitch), 0);
        voice.noteOn = false;
    }

    const juce::uint8 midiVel = (juce::uint8) juce::jlimit(1, 127, (int) std::lround(velocity01 * 127.0f));
    midiMessages.addEvent(juce::MidiMessage::noteOn(10, midiNote, midiVel), 0);
    voice.noteOn          = true;
    voice.pitch           = midiNote;
    voice.samplesUntilOff = juce::jmax(1, (int) (0.04 * getSampleRate()));
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
            triggerGeneratedRole(role, msg.getFloatVelocity());
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

                        // Per-role MIX gate (see setGeneratedDrumRoleMuted/
                        // setGeneratedDrumRoleGain) - mute/solo-style: only
                        // gates the TRIGGERING of a new note here, same as
                        // drumRowMuted/MelodyVoice::muted elsewhere in this
                        // function. Never touches role.velocity/localRoles
                        // (the stored pattern) - an already-sounding voice
                        // from before a mute finishes decaying naturally,
                        // and unmuting takes effect on the very next hit
                        // with no regeneration of any kind.
                        if (generatedDrumRoleMuted[r].load(std::memory_order_relaxed))
                            continue;
                        const float roleGain = generatedDrumRoleGain[r].load(std::memory_order_relaxed);
                        const int   gatedVel = juce::jlimit(0, 127, (int) std::lround((float) vel * roleGain));
                        if (gatedVel <= 0)
                            continue;

                        // Real swing (Grid.h's stepTimeSeconds already
                        // computed this timing math - see kSwingAmount's
                        // own comment below for the source): hatClosed/
                        // percA/percB's off-beat 16th positions land a
                        // small, deterministic amount late; kick/clap/bass
                        // stay exactly on this block-boundary detection.
                        // A swung hit is queued (generatedSwingTriggers)
                        // instead of fired immediately - see its own
                        // countdown loop below, the same
                        // decrement-until-zero idiom this file already
                        // uses for note-off scheduling.
                        const bool isSwingRole = synthRoleIsSwung(role.midiNote);
                        if (isSwingRole && (wrappedStep % 2) != 0)
                        {
                            const double swingBpm = (double) currentBpm.load(std::memory_order_relaxed);
                            const int delaySamples = swingDelaySamples(wrappedStep, swingBpm, getSampleRate());
                            generatedSwingTriggers[r] = { true, role.midiNote, (float) gatedVel / 127.0f, delaySamples };
                        }
                        else
                        {
                            fireGeneratedRoleNote((int) r, role.midiNote, (float) gatedVel / 127.0f, midiMessages);
                        }
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

        // Fires any swing-delayed hits whose countdown has now reached
        // this block - see generatedSwingTriggers' own declaration for why
        // this exists instead of firing immediately.
        for (int r = 0; r < kMaxGeneratedDrumRoles; ++r)
        {
            auto& pending = generatedSwingTriggers[r];
            if (!pending.pending)
                continue;

            pending.samplesRemaining -= numSamplesForGate;
            if (pending.samplesRemaining <= 0)
            {
                pending.pending = false;
                fireGeneratedRoleNote(r, pending.midiNote, pending.velocity01, midiMessages);
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

    // --- Generated-drum SAMPLE rendering: mixes generatedRoleSampleBuffers
    // (Source/DrumSampleSelector.h) into our own output, for whichever
    // roles triggerGeneratedRole chose real samples for. Same
    // deliberately-unconditional placement as the synth block above - a
    // triggered voice keeps playing out its sample regardless of pattern/
    // transport state.
    {
        const int nSamplesGen = buffer.getNumSamples();
        const int nChGen      = std::min(2, buffer.getNumChannels());

        for (int r = 0; r < kMaxGeneratedDrumRoles; ++r)
        {
            auto& voice = generatedSampleVoices[r];
            if (voice.readPos < 0)
                continue;

            std::shared_ptr<juce::AudioBuffer<float>> buf;
            {
                juce::ScopedLock sl(generatedSampleLock);
                buf = generatedRoleSampleBuffers[r];
            }
            if (buf == nullptr || buf->getNumSamples() == 0)
            {
                voice.readPos = -1;
                continue;
            }

            const int remaining = buf->getNumSamples() - voice.readPos;
            const int n         = std::min(nSamplesGen, remaining);
            const int srcCh     = buf->getNumChannels();

            if (!suppressOwnPlayback.load(std::memory_order_relaxed))
                for (int ch = 0; ch < nChGen; ++ch)
                    buffer.addFrom(ch, 0, *buf, std::min(ch, srcCh - 1), voice.readPos, n, voice.gain);

            voice.readPos += n;
            if (voice.readPos >= buf->getNumSamples())
                voice.readPos = -1;
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
            std::vector<int8_t> localGeneratedOffsets; // empty unless setGeneratedMelodyPattern is active for this track
            std::vector<int8_t> localGeneratedGateLengthSteps; // empty unless a gate-length array was passed to setGeneratedMelodyPattern
            int localGeneratedTotalSteps = 0;
            {
                juce::ScopedLock sl(voice.lock);
                localMelody             = voice.offsets;
                localMelodyRoot         = voice.keyRoot;
                localGeneratedOffsets   = voice.generatedOffsets;
                localGeneratedGateLengthSteps = voice.generatedGateLengthSteps;
                localGeneratedTotalSteps = voice.generatedTotalSteps;
            }
            const bool useGeneratedPattern = !localGeneratedOffsets.empty() && localGeneratedTotalSteps > 0;

            // Mute/solo gate only the triggering of new notes — same
            // convention as the drum rows: a note already sounding keeps
            // ringing out via its own note-off below rather than being cut
            // off abruptly.
            const bool audible = !voice.muted.load(std::memory_order_relaxed)
                               && (soloTrackCount == 0 || voice.solo.load(std::memory_order_relaxed));

            juce::MidiBuffer serumMidi;

            if (isPlayingNow)
            {
                // Arrangement-driven generated patterns are typically much
                // longer than kMelodySteps (a full multi-section
                // arrangement vs. a fixed 8 bars) - use their own real
                // length for the wrap so the whole arrangement plays
                // through once per loop instead of being truncated to 8
                // bars. Falls back to the normal offsets/kMelodySteps
                // behaviour for any track that never called
                // setGeneratedMelodyPattern (unchanged from before).
                const int totalStepsForWrap = useGeneratedPattern ? localGeneratedTotalSteps : kMelodySteps;
                const int stepFloor   = (int) std::floor(ppq * 4.0);
                const int wrappedStep = ((stepFloor % totalStepsForWrap) + totalStepsForWrap) % totalStepsForWrap;

                if (wrappedStep != voice.lastStepIndex)
                {
                    voice.lastStepIndex = wrappedStep;

                    if (voice.noteOn)
                    {
                        serumMidi.addEvent(juce::MidiMessage::noteOff(1, voice.soundingPitch), 0);
                        voice.noteOn = false;
                    }
                    voice.gateSamplesRemaining = -1; // a new step always cancels any still-armed gate from the previous note

                    const int8_t offset = useGeneratedPattern ? localGeneratedOffsets[(size_t) wrappedStep]
                                                               : localMelody[(size_t) wrappedStep];
                    if (offset != kMelodyOffValue && audible)
                    {
                        const int pitch = juce::jlimit(0, 127, 36 + localMelodyRoot + offset);
                        serumMidi.addEvent(juce::MidiMessage::noteOn(1, pitch, (juce::uint8) 100), 0);
                        voice.noteOn        = true;
                        voice.soundingPitch = pitch;
                        voice.noteOnEventsSent.fetch_add(1, std::memory_order_relaxed);

                        // Real gate-length (Source/Engine/BassArchetype.h) -
                        // arms an early note-off countdown if this onset has
                        // an explicit hold duration shorter than however long
                        // it ends up ringing until the next onset. Empty
                        // array or a 0 entry (every existing melody/manual-
                        // editing call site) leaves this at -1, i.e.
                        // unchanged "ring until the next onset" behaviour.
                        const int8_t gateSteps = (wrappedStep < (int) localGeneratedGateLengthSteps.size())
                                                      ? localGeneratedGateLengthSteps[(size_t) wrappedStep] : 0;
                        if (gateSteps > 0)
                        {
                            const double bpmForGate = (double) currentBpm.load(std::memory_order_relaxed);
                            if (bpmForGate > 0.0 && getSampleRate() > 0.0)
                            {
                                const double secPerStep = (60.0 / bpmForGate) / 4.0;
                                voice.gateSamplesRemaining = juce::jmax(1, (int) std::lround(gateSteps * secPerStep * getSampleRate()));
                            }
                        }
                    }
                }
            }
            else if (voice.noteOn)
            {
                serumMidi.addEvent(juce::MidiMessage::noteOff(1, voice.soundingPitch), 0);
                voice.noteOn        = false;
                voice.lastStepIndex = -1;
                voice.gateSamplesRemaining = -1;
            }

            // Early gate-off: if a gate is armed and its countdown reaches
            // zero before the next onset naturally retriggers the voice,
            // cut the note short here - the same decrement-to-zero idiom
            // already used for generated-drum note-off/swing scheduling
            // (PluginProcessor.cpp's other trigger blocks), applied to a
            // melody voice for the first time. Never touches
            // generatedOffsets/the stored pattern - purely a playback-time
            // gate.
            if (voice.noteOn && voice.gateSamplesRemaining > 0)
            {
                const int numSamplesForGate = buffer.getNumSamples();
                voice.gateSamplesRemaining -= numSamplesForGate;
                if (voice.gateSamplesRemaining <= 0)
                {
                    const int offset = juce::jlimit(0, juce::jmax(0, numSamplesForGate - 1),
                                                     numSamplesForGate + voice.gateSamplesRemaining);
                    serumMidi.addEvent(juce::MidiMessage::noteOff(1, voice.soundingPitch), offset);
                    voice.noteOn = false;
                    voice.gateSamplesRemaining = -1;
                }
            }

            const int numSamples = buffer.getNumSamples();
            voice.scratch.setSize(2, numSamples, false, false, true);
            voice.scratch.clear();
            serum->processBlock(voice.scratch, serumMidi);

            // Runtime proof for getMelodyVoiceDiagnostics(): the peak
            // level of Serum2's OWN raw output for this block, measured
            // BEFORE the EQ/suppression/mix stage below - this is what
            // actually distinguishes "Serum2 is receiving MIDI but its
            // loaded patch is genuinely silent" from "Serum2 is producing
            // real signal that isn't reaching the main output" (a
            // suppression/mixing bug) - the two have identical symptoms
            // ("I hear nothing") but completely different fixes.
            {
                float blockPeak = 0.0f;
                for (int ch = 0; ch < voice.scratch.getNumChannels(); ++ch)
                {
                    const float* data = voice.scratch.getReadPointer(ch);
                    for (int n = 0; n < numSamples; ++n)
                        blockPeak = std::max(blockPeak, std::abs(data[n]));
                }
                voice.lastBlockPeakOut.store(blockPeak, std::memory_order_relaxed);
            }

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
        // A host-restored blob has no known on-disk preset name/file - the
        // UI must not report a captured-preset name for it (see
        // MelodyVoiceDiagnostics::capturedPresetActive's own comment).
        melodyVoices[t].capturedPresetActive.store(false, std::memory_order_release);

        if (auto* serum = melodyVoices[t].instance.load(std::memory_order_acquire))
            serum->setStateInformation(voiceState.getData(), (int) voiceState.getSize());
    }
}

bool AbletonCopilotAudioProcessor::captureMelodyTrackState(int trackIndex, juce::MemoryBlock& outState)
{
    if (trackIndex < 0 || trackIndex >= kMaxMelodyTracks)
        return false;

    auto* serum = melodyVoices[trackIndex].instance.load(std::memory_order_acquire);
    if (serum == nullptr)
        return false;

    outState.reset();
    serum->getStateInformation(outState);
    // We just read Serum2's own CURRENT live state directly - it's real,
    // nameable state as of right now (the caller, PluginEditor's
    // captureCurrentSound, still gates display on the user actually
    // confirming a name for it - see lastConfirmedPresetName).
    melodyVoices[trackIndex].capturedPresetActive.store(true, std::memory_order_release);
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
    melodyVoices[trackIndex].capturedPresetActive.store(true, std::memory_order_release);
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
