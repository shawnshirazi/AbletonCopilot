#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Minimal status display — this plugin has no controls of its own (yet);
// everything comes from the Drum Machine plugin's pattern file. Just enough
// UI to confirm it found the file and see what it's currently playing.
class AbletonCopilotMidiAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                private juce::Timer
{
public:
    explicit AbletonCopilotMidiAudioProcessorEditor(AbletonCopilotMidiAudioProcessor&);
    ~AbletonCopilotMidiAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    AbletonCopilotMidiAudioProcessor& processor;
    juce::Label statusLabel;
    juce::Label detailLabel;
    juce::Label drumStatusLabel; // separate handoff file, separate status line

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AbletonCopilotMidiAudioProcessorEditor)
};
