#include "PluginProcessor.h"
#include "PluginEditor.h"

static const juce::Colour kBg      { 0xff141414 };
static const juce::Colour kPanel   { 0xff1f1f1f };
static const juce::Colour kAccent  { 0xfffc8c00 };
static const juce::Colour kText    { 0xffeeeeee };
static const juce::Colour kTextDim { 0xff777777 };

AbletonCopilotMidiAudioProcessorEditor::AbletonCopilotMidiAudioProcessorEditor(
    AbletonCopilotMidiAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(340, 220);

    statusLabel.setFont(juce::FontOptions(15.0f).withStyle("Bold"));
    statusLabel.setColour(juce::Label::textColourId, kAccent);
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);

    detailLabel.setFont(juce::FontOptions(11.5f));
    detailLabel.setColour(juce::Label::textColourId, kTextDim);
    detailLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(detailLabel);

    drumStatusLabel.setFont(juce::FontOptions(11.0f));
    drumStatusLabel.setColour(juce::Label::textColourId, kTextDim);
    drumStatusLabel.setJustificationType(juce::Justification::centredLeft);
    drumStatusLabel.setMinimumHorizontalScale(1.0f);
    addAndMakeVisible(drumStatusLabel);

    startTimerHz(4);
    timerCallback();
}

AbletonCopilotMidiAudioProcessorEditor::~AbletonCopilotMidiAudioProcessorEditor() {}

void AbletonCopilotMidiAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBg);
    g.setColour(kPanel);
    g.fillRoundedRectangle(getLocalBounds().reduced(8).toFloat(), 6.0f);
}

void AbletonCopilotMidiAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(16);
    statusLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(6);
    detailLabel.setBounds(area.removeFromTop(32));
    area.removeFromTop(6);
    drumStatusLabel.setBounds(area); // multi-line diagnostic block, see timerCallback()
}

void AbletonCopilotMidiAudioProcessorEditor::timerCallback()
{
    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    const bool found = processor.patternFileFound.load(std::memory_order_relaxed);
    const int  steps  = processor.activeStepCount.load(std::memory_order_relaxed);
    const int  root    = processor.keyRootForUI.load(std::memory_order_relaxed);

    statusLabel.setText(found ? "AbletonCopilot MIDI" : "Waiting for pattern...",
                         juce::dontSendNotification);

    juce::String detail = found
        ? "Key: " + juce::String(noteNames[juce::jlimit(0, 11, root)])
              + "   |   " + juce::String(steps) + " active steps"
        : "Open the Drum Machine plugin and choose a\nsample library to start generating a bassline.";
    detailLabel.setText(detail, juce::dontSendNotification);

    // Temporary diagnostics (end-to-end drum path debugging) - one line per
    // stage of the pipeline, so it's visible exactly where the chain breaks
    // without needing a debugger attached to Ableton.
    const bool drumsFound  = processor.drumPatternFileFound.load(std::memory_order_relaxed);
    const int  drumSteps   = processor.drumActiveStepCount.load(std::memory_order_relaxed);
    const bool drumsParsed = processor.drumRolesParsedOk.load(std::memory_order_relaxed);
    const int  pollCount   = processor.drumPollCount.load(std::memory_order_relaxed);
    const bool playing     = processor.transportPlaying.load(std::memory_order_relaxed);
    const int  noteOnCount = processor.drumNoteOnCount.load(std::memory_order_relaxed);

    juce::String drumDetail;
    drumDetail << "File found: " << (drumsFound ? "YES" : "no") << "\n";
    drumDetail << "Roles parsed: " << (drumsParsed ? "YES" : "no")
               << "  |  hits: " << drumSteps << "\n";
    drumDetail << "Poll #" << pollCount << " (timer alive)\n";
    drumDetail << "Transport: " << (playing ? "PLAYING" : "stopped - notes only trigger while playing") << "\n";
    drumDetail << "Drum noteOn sent: " << noteOnCount;
    drumStatusLabel.setText(drumDetail, juce::dontSendNotification);
}
