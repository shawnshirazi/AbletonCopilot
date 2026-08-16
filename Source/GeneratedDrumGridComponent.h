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
// Rows are dynamic (see setPattern) - originally always exactly the 6
// Engine::DrumRole rows (Kick/Clap/HatClosed/HatOpen/PercA/PercB); now
// also fed Bass/Melody/Pad rows (their offset/gate arrays converted to a
// simple active-step "velocity" by the caller - see PluginEditor) so the
// whole generated loop is visible in one place. The caller is responsible
// for sizing this component to getRequiredHeight() and placing it inside
// a vertical-scrolling juce::Viewport (see PluginEditor::
// generatedDrumGridViewport) - "additional rows caused scrolling
// problems" previously because this component always claimed a FIXED
// 6-row height with nowhere to scroll to see more. Velocity is shown as
// cell shade: darker = stronger, lighter = softer (see GridContent::
// paint) - a small legend along the bottom explains the scale.
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

    // Total height needed for however many rows are CURRENTLY displayed
    // (see setPattern) plus the legend - dynamic, not a fixed constant,
    // since the row count is no longer fixed at 6. The owner sizes this
    // component to this height and places it inside its own
    // vertical-scrolling juce::Viewport.
    int getRequiredHeight() const noexcept { return kRowHeight * (int) rows.size() + kLegendHeight; }

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

    // 6px/step (was 8) - real width reduction so the 256-step/16-bar grid
    // is 1536px instead of 2048px, less horizontal scrolling in a compact
    // plugin window while cells stay legibly visible (still an even,
    // clean pixel width, same shading logic) - full elimination of
    // horizontal scroll isn't realistic at this step resolution without
    // making cells illegibly thin, same tradeoff most DAW step-sequencer/
    // piano-roll UIs make.
    static constexpr int kStepWidth = 6;

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
