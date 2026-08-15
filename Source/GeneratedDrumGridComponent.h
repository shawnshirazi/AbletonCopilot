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
// Six fixed rows (Kick/Clap/HatClosed/HatOpen/PercA/PercB, matching
// Engine::DrumRole / DrumVoiceSynth 1:1) are always shown regardless of
// the user's sample library. Velocity is shown as cell shade: darker =
// stronger, lighter = softer (see GridContent::paint) - a small legend
// along the bottom explains the scale.
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

    // Total steps in the currently displayed pattern (stepsPerBar *
    // numBars from the last setPattern call) - the owner uses this to wrap
    // a real host-PPQ-derived step index before calling setPlayheadStep,
    // so the playhead always matches whatever pattern is actually showing.
    int getTotalSteps() const noexcept { return stepsPerBar * numBars; }

    // Moves the playhead to `step` (0-based, wrapped to getTotalSteps() by
    // the caller) or hides it when `visible` is false. Pure visualization -
    // the caller (PluginEditor's existing timer) is expected to derive
    // `step` from the host's actual PPQ position each call, not from an
    // independent clock; this method just draws whatever it's told and
    // auto-scrolls the viewport to keep it visible.
    void setPlayheadStep(int step, bool visible);

    void paint(juce::Graphics&) override; // legend strip along the bottom
    void resized() override;

    static constexpr int kRowHeight    = 22;
    static constexpr int kHeaderWidth  = 62;
    static constexpr int kLegendHeight = 20;

    // Total height needed for a fixed number of rows (6 - Kick/Clap/
    // HatClosed/HatOpen/PercA/PercB) plus the legend, for the owner's
    // layout code.
    static constexpr int kFixedRowCount     = 6;
    static constexpr int kRequiredHeight    = kRowHeight * kFixedRowCount + kLegendHeight;

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

    int  playheadStep    = -1;
    bool playheadVisible = false;

    HeaderColumn   headerColumn{ *this };
    GridContent    gridContent{ *this };
    juce::Viewport gridViewport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GeneratedDrumGridComponent)
};
