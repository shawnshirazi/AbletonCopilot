#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

AbletonCopilotMidiAudioProcessor::AbletonCopilotMidiAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    // Ableton's VST3 host requires a valid audio input bus for anything
    // tagged with an "Fx" category (VST3 has no dedicated MIDI-effect
    // category, so JUCE/Projucer fall back to "Fx" for this project) — a
    // zero-audio-bus MIDI-only plugin gets flatly rejected with "no valid
    // input bus could be found". This bus is otherwise unused; audio just
    // passes through untouched in processBlock.
    : AudioProcessor(BusesProperties()
                     .withInput("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
#endif
{
    patternFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                      .getChildFile("AbletonCopilot/melody_pattern.json");
    pattern.fill(kNoNote);

    drumPatternFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                           .getChildFile("AbletonCopilot/drum_pattern.json");

    startTimer(500); // poll for pattern-file changes off the audio thread
}

AbletonCopilotMidiAudioProcessor::~AbletonCopilotMidiAudioProcessor() { stopTimer(); }

const juce::String AbletonCopilotMidiAudioProcessor::getName() const { return JucePlugin_Name; }
bool   AbletonCopilotMidiAudioProcessor::acceptsMidi() const  { return true; }
bool   AbletonCopilotMidiAudioProcessor::producesMidi() const { return true; }
bool   AbletonCopilotMidiAudioProcessor::isMidiEffect() const { return true; }
double AbletonCopilotMidiAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int  AbletonCopilotMidiAudioProcessor::getNumPrograms()                            { return 1; }
int  AbletonCopilotMidiAudioProcessor::getCurrentProgram()                         { return 0; }
void AbletonCopilotMidiAudioProcessor::setCurrentProgram(int)                      {}
const juce::String AbletonCopilotMidiAudioProcessor::getProgramName(int)           { return {}; }
void AbletonCopilotMidiAudioProcessor::changeProgramName(int, const juce::String&) {}

void AbletonCopilotMidiAudioProcessor::prepareToPlay(double, int)
{
    lastStepIndex   = -1;
    noteCurrentlyOn = false;
    soundingPitch   = -1;

    drumVoices.fill(DrumVoiceState {});
    drumLastStepIndex = -1;
}

void AbletonCopilotMidiAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AbletonCopilotMidiAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}
#endif

//==============================================================================
// Runs on the message thread (juce::Timer), never the audio thread — file I/O
// and JSON parsing have no business in processBlock.
//
// BUG FIX (found while debugging the drum path end to end): this used to be
// one function whose body was the melody-pattern read, with
// readDrumPatternFile() tacked on at the very end, AFTER three early-return
// points that all belong to the MELODY file's own state (not found / mtime
// unchanged / invalid JSON). Since the melody "Generate" UI is currently
// hidden (kShowExperimentalFeatures), melody_pattern.json either doesn't
// exist or never changes anymore — so those early returns fired on
// essentially every poll, and readDrumPatternFile() was structurally
// unreachable. Split into two fully independent reads, both always run.
void AbletonCopilotMidiAudioProcessor::timerCallback()
{
    drumPollCount.fetch_add(1, std::memory_order_relaxed); // proves this timer is actually firing

    readMelodyPatternFile();
    readDrumPatternFile();
}

void AbletonCopilotMidiAudioProcessor::readMelodyPatternFile()
{
    if (!patternFile.existsAsFile())
    {
        patternFileFound.store(false, std::memory_order_relaxed);
        return;
    }

    auto mtimeMs = patternFile.getLastModificationTime().toMilliseconds();
    if (mtimeMs == lastPatternFileMTime)
        return;

    lastPatternFileMTime = mtimeMs;
    patternFileFound.store(true, std::memory_order_relaxed);

    auto json = juce::JSON::parse(patternFile);
    if (!json.isObject())
        return;

    const int root = (int) json.getProperty("keyRoot", 0);

    std::array<int, kNumSteps> newPattern;
    newPattern.fill(kNoNote);
    int active = 0;
    if (auto* arr = json.getProperty("steps", juce::var()).getArray())
    {
        for (int i = 0; i < juce::jmin((int) arr->size(), kNumSteps); ++i)
        {
            const auto& element = (*arr)[i];
            if (element.isVoid())
                continue;

            newPattern[(size_t) i] = (int) element;
            ++active;
        }
    }

    {
        juce::ScopedLock sl(patternLock);
        pattern        = newPattern;
        patternKeyRoot = root;
    }

    keyRootForUI.store(root, std::memory_order_relaxed);
    activeStepCount.store(active, std::memory_order_relaxed);
}

//==============================================================================
// Also runs on the message thread, same reasoning as the melody-pattern read
// above. Independent handoff file from the main AbletonCopilot plugin's new
// "Generate Drum Pattern" button (Phase 1 deterministic engine, Source/Engine/).
void AbletonCopilotMidiAudioProcessor::readDrumPatternFile()
{
    if (!drumPatternFile.existsAsFile())
    {
        drumPatternFileFound.store(false, std::memory_order_relaxed);
        return;
    }

    auto mtimeMs = drumPatternFile.getLastModificationTime().toMilliseconds();
    if (mtimeMs == lastDrumPatternFileMTime)
        return;

    lastDrumPatternFileMTime = mtimeMs;
    drumPatternFileFound.store(true, std::memory_order_relaxed);

    auto json = juce::JSON::parse(drumPatternFile);
    if (!json.isObject())
    {
        drumRolesParsedOk.store(false, std::memory_order_relaxed);
        return;
    }

    auto* rolesArr = json.getProperty("roles", juce::var()).getArray();
    if (rolesArr == nullptr)
    {
        drumRolesParsedOk.store(false, std::memory_order_relaxed);
        return;
    }

    std::array<DrumRoleData, kMaxDrumRoles> newRoles;
    int active = 0;
    int rolesWithValidNote = 0;

    for (int r = 0; r < juce::jmin((int) rolesArr->size(), kMaxDrumRoles); ++r)
    {
        const auto& roleVar = (*rolesArr)[r];
        if (!roleVar.isObject())
            continue;

        newRoles[(size_t) r].midiNote = (int) roleVar.getProperty("midiNote", -1);
        if (newRoles[(size_t) r].midiNote >= 0)
            ++rolesWithValidNote;

        auto* stepsArr = roleVar.getProperty("steps", juce::var()).getArray();
        if (stepsArr == nullptr)
            continue;

        for (int i = 0; i < juce::jmin((int) stepsArr->size(), kNumSteps); ++i)
        {
            const auto& element = (*stepsArr)[i];
            if (element.isVoid())
                continue;

            newRoles[(size_t) r].velocity[(size_t) i] = juce::jlimit(1, 127, (int) element);
            ++active;
        }
    }

    {
        juce::ScopedLock sl(drumPatternLock);
        drumRoles = newRoles;
    }

    drumActiveStepCount.store(active, std::memory_order_relaxed);
    drumRolesParsedOk.store(rolesWithValidNote > 0, std::memory_order_relaxed);
}

//==============================================================================
void AbletonCopilotMidiAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                      juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(buffer);

    midiMessages.clear(); // we're a generator, not a pass-through

    bool   isPlayingNow = false;
    double ppq          = 0.0;
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
        {
            isPlayingNow = pos->getIsPlaying();
            if (auto ppqOpt = pos->getPpqPosition())
                ppq = *ppqOpt;
        }

    transportPlaying.store(isPlayingNow, std::memory_order_relaxed); // diagnostic - notes only trigger while this is true

    std::array<int, kNumSteps> localPattern;
    int localRoot;
    {
        juce::ScopedLock sl(patternLock);
        localPattern = pattern;
        localRoot    = patternKeyRoot;
    }

    if (isPlayingNow)
    {
        // Step boundaries are checked once per block, same simplification as
        // the Drum Machine's engine — fine as long as a block is shorter than
        // a 16th note, true for any normal block size/tempo.
        const int stepFloor   = (int) std::floor(ppq * 4.0);
        const int wrappedStep = ((stepFloor % kNumSteps) + kNumSteps) % kNumSteps;

        if (wrappedStep != lastStepIndex)
        {
            lastStepIndex = wrappedStep;

            if (noteCurrentlyOn)
            {
                midiMessages.addEvent(juce::MidiMessage::noteOff(1, soundingPitch), 0);
                noteCurrentlyOn = false;
                soundingPitch   = -1;
            }

            const int offset = localPattern[(size_t) wrappedStep];
            if (offset != kNoNote)
            {
                // C2 + key root + the step's chord-tone/hand-tuned offset.
                const int pitch = juce::jlimit(0, 127, 36 + localRoot + offset);
                midiMessages.addEvent(juce::MidiMessage::noteOn(1, pitch, (juce::uint8) 100), 0);
                noteCurrentlyOn = true;
                soundingPitch   = pitch;
            }
        }
    }
    else if (noteCurrentlyOn)
    {
        midiMessages.addEvent(juce::MidiMessage::noteOff(1, soundingPitch), 0);
        noteCurrentlyOn = false;
        soundingPitch   = -1;
        lastStepIndex   = -1;
    }

    // --- Drum roles (Phase 1 deterministic engine, Source/Engine/) ---
    // Independent short-gated hits, not the melody voice's held-until-next-
    // event style - a real drum hit is a brief strike, not a sustained note.
    // Reuses the SAME ppq/step position computed above (one shared grid),
    // just gated separately since a step can trigger several roles at once
    // (e.g. kick+hat together), unlike the melody voice's single note.
    std::array<DrumRoleData, kMaxDrumRoles> localDrumRoles;
    {
        juce::ScopedLock sl(drumPatternLock);
        localDrumRoles = drumRoles;
    }

    const int numSamples  = buffer.getNumSamples();
    const int gateSamples = juce::jmax(1, (int) (0.04 * getSampleRate())); // ~40ms drum-hit gate

    if (isPlayingNow)
    {
        const int stepFloor   = (int) std::floor(ppq * 4.0);
        const int wrappedStep = ((stepFloor % kNumSteps) + kNumSteps) % kNumSteps;

        if (wrappedStep != drumLastStepIndex)
        {
            drumLastStepIndex = wrappedStep;

            for (int r = 0; r < kMaxDrumRoles; ++r)
            {
                const auto& role = localDrumRoles[(size_t) r];
                if (role.midiNote < 0)
                    continue;

                const int vel = role.velocity[(size_t) wrappedStep];
                if (vel <= 0)
                    continue;

                auto& voice = drumVoices[(size_t) r];
                if (voice.noteOn) // very fast retrigger - close the previous hit first, never leave a stuck note
                {
                    midiMessages.addEvent(juce::MidiMessage::noteOff(10, voice.pitch), 0);
                    voice.noteOn = false;
                }

                // Channel 10 - the General MIDI drum-channel convention,
                // pairing with the GM drum-map note numbers the main
                // plugin's export already uses.
                midiMessages.addEvent(juce::MidiMessage::noteOn(10, role.midiNote, (juce::uint8) vel), 0);
                voice.noteOn          = true;
                voice.pitch           = role.midiNote;
                voice.samplesUntilOff = gateSamples;
                drumNoteOnCount.fetch_add(1, std::memory_order_relaxed); // diagnostic - proves MIDI is actually being emitted
            }
        }
    }
    else
    {
        drumLastStepIndex = -1;
    }

    // Gate-off countdown runs regardless of transport state, so a hit still
    // releases cleanly if playback stops mid-gate.
    for (int r = 0; r < kMaxDrumRoles; ++r)
    {
        auto& voice = drumVoices[(size_t) r];
        if (!voice.noteOn)
            continue;

        voice.samplesUntilOff -= numSamples;
        if (voice.samplesUntilOff <= 0)
        {
            const int offset = juce::jlimit(0, juce::jmax(0, numSamples - 1), numSamples + voice.samplesUntilOff);
            midiMessages.addEvent(juce::MidiMessage::noteOff(10, voice.pitch), offset);
            voice.noteOn = false;
        }
    }
}

bool AbletonCopilotMidiAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* AbletonCopilotMidiAudioProcessor::createEditor()
{
    return new AbletonCopilotMidiAudioProcessorEditor(*this);
}

void AbletonCopilotMidiAudioProcessor::getStateInformation(juce::MemoryBlock&) {}
void AbletonCopilotMidiAudioProcessor::setStateInformation(const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AbletonCopilotMidiAudioProcessor();
}
