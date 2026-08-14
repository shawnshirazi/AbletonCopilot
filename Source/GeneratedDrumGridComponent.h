#pragma once
#include <JuceHeader.h>
#include <vector>

// Read-only visual display of the most recently generated drum pattern -
// fed the exact same per-role velocity data (see Engine::toVelocityArray,
// Source/Engine/DrumEngine.h) already handed to
// AbletonCopilotAudioProcessor::setGeneratedDrumPattern(), never a second,
// independently generated pattern. Kept fully separate from
// DrumMachineComponent (the manual/sample-based grid): that grid's rows
// only exist if the user's own sample library has matching samples, and
// its cells drive a different, sample-based playback engine - reusing it
// here would either have nowhere to show anything (no local Kick/Clap/Hat/
// Perc samples) or make a cell mean two different things at once.
//
// Four fixed rows (Kick/Clap/Hat/Perc, matching Engine::DrumRole /
// DrumVoiceSynth 1:1) are always shown regardless of the user's sample
// library. Velocity is shown as cell opacity.
class GeneratedDrumGridComponent : public juce::Component
{
public:
    struct RowDisplay
    {
        juce::String     name;
        juce::Colour     colour;
        std::vector<int> velocity; // 0-127, 0 = no hit - same data as GeneratedDrumRole::velocity
    };

    GeneratedDrumGridComponent();

    // Replaces the displayed pattern. Call this alongside
    // processor.setGeneratedDrumPattern() with rows built from the SAME
    // Engine::StepArray data (via Engine::toVelocityArray) - see
    // PluginEditor::generateDrumPatternClicked().
    void setPattern(std::vector<RowDisplay> newRows, int stepsPerBarIn, int numBarsIn);

    void resized() override;

    static constexpr int kRowHeight   = 26;
    static constexpr int kHeaderWidth = 62;

private:
    class GridContent : public juce::Component
    {
    public:
        explicit GridContent(GeneratedDrumGridComponent& o) : owner(o) {}
        void paint(juce::Graphics&) override;
    private:
        GeneratedDrumGridComponent& owner;
    };

    class HeaderColumn : public juce::Component
    {
    public:
        explicit HeaderColumn(GeneratedDrumGridComponent& o) : owner(o) {}
        void paint(juce::Graphics&) override;
    private:
        GeneratedDrumGridComponent& owner;
    };

    static constexpr int kStepWidth = 8;

    std::vector<RowDisplay> rows;
    int stepsPerBar = 16;
    int numBars     = 16;

    HeaderColumn   headerColumn{ *this };
    GridContent    gridContent{ *this };
    juce::Viewport gridViewport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GeneratedDrumGridComponent)
};
