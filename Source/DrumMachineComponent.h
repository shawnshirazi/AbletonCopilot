#pragma once
#include <JuceHeader.h>
#include "RackBrowserComponent.h"
#include <array>
#include <vector>

// One drum-machine row: a rack category (Kick, Snare, Vocal, ...) that had
// samples in the last library scan, its own 8-bar pattern, and the sample
// currently assigned (cyclable with the < / > arrows). Rows only ever pull
// from their own rack — no cross-category fallback, so a row is always
// correctly labeled for what it's actually playing. A category can spawn
// more than one row (e.g. "KICK"/"KIK2") when the rack has enough distinct
// samples to layer — see kRowTemplates in the .cpp.
struct DrumRow
{
    juce::String            elementName; // display name, e.g. "Kick"
    juce::String            badgeLabel;  // short label inside the row's ring badge, e.g. "KICK"
    juce::String            rackId;      // rack this row is locked to, e.g. "KICK"
    juce::Colour            colour;
    std::vector<RackEntry>  available;   // candidate samples from the rack scan
    int                     sampleIndex = -1;
    std::array<bool, 128>   steps {};    // 8 bars x 16th notes
    bool                    muted = false;
    bool                    solo  = false;
};

// XO-style step sequencer: one row per available sample category (plus
// layered variants like KICK/KIK2), 8 bars (128 steps) each, with the
// assigned sample name + cycle arrows and a moving playhead. The step grid
// scrolls horizontally; the header column (badge/M-S/arrows) stays fixed on
// the left. Melody/MIDI tracks live in the separate MelodyGridComponent,
// not here.
class DrumMachineComponent : public juce::Component
{
public:
    DrumMachineComponent();

    // Rebuilds the row list from a fresh library scan — one row per non-empty,
    // non-loop rack (two for categories with a layered variant template and
    // enough distinct samples to justify it). Manual step edits are
    // preserved across rescans of a rack that was already present.
    void refreshFromRacks(const std::vector<Rack>& racks);

    void setPlayheadStep(int step, bool visible);

    int                    getNumRows() const noexcept { return (int) rows.size(); }
    juce::String           getRowName(int rowIndex) const;
    juce::String           getRowRackId(int rowIndex) const;
    std::array<bool, 128>  getRowSteps(int rowIndex) const;
    juce::File             getSampleForRow(int rowIndex) const;
    bool                   isRowMuted(int rowIndex) const;
    bool                   isRowSolo(int rowIndex) const;

    // Assigns a specific file to whichever row is locked to rackId (e.g.
    // "BASS"), if that file is actually one of the row's available samples.
    // Fires onSampleCycled the same as manually cycling, so the owner's
    // existing wiring pushes it to playback unchanged. Used by the Advisor
    // tab to apply a sample suggestion. Returns false if no matching row/file.
    bool setRowSampleByFile(const juce::String& rackId, const juce::File& file);

    // Current sample file for whichever row is locked to rackId - used to
    // capture a "before" snapshot for the Advisor tab's A/B preview.
    // Returns an invalid File if no such row or no sample assigned.
    juce::File getSampleForRackId(const juce::String& rackId) const;

    // Tier 2 reference-driven drums (see Analysis/ReferenceFragmentJob.h):
    // overwrites whichever row is locked to rackId with a real, reference-
    // extracted step pattern and a real one-shot sliced from that same
    // track, inserted as a synthetic "From Reference" entry in the row's
    // available samples (selected immediately). Only rewrites the pattern -
    // does not create a new row if rackId has none (the role isn't
    // available in the user's own scanned library). Returns false if no
    // matching row exists.
    bool applyReferenceDrumPattern(const juce::String& rackId, const std::array<bool, 128>& steps,
                                    const juce::File& sampleFile);

    void resized() override;

    static constexpr int kRowHeight    = 42;
    static constexpr int kStepsPerBar  = 16;
    static constexpr int kNumBars      = 8;
    static constexpr int kNumSteps     = kStepsPerBar * kNumBars; // 128

    // Fire on the message thread so the owner can mirror live edits into the
    // real-time playback engine (see AbletonCopilotAudioProcessor).
    std::function<void(int rowIndex, int step, bool isOn)> onStepToggled;
    std::function<void(int rowIndex)>                      onSampleCycled;
    std::function<void(int rowIndex, bool muted)>           onMuteToggled;
    std::function<void(int rowIndex, bool solo)>            onSoloToggled;

private:
    void cycleSample(int rowIndex, int direction);
    void toggleStep(int rowIndex, int step);
    void toggleMute(int rowIndex);
    void toggleSolo(int rowIndex);

    static constexpr int kHeaderWidth   = 195;
    static constexpr int kArrowWidth    = 18;
    static constexpr int kStepWidth     = 20;
    static constexpr int kMuteWidth     = 20; // shared column width for the stacked M/S toggles
    static constexpr int kBadgeDiameter = 38;

    // Fixed-left header: ring badge, sample name, M/S, cycle arrows.
    class HeaderColumn : public juce::Component
    {
    public:
        explicit HeaderColumn(DrumMachineComponent& o) : owner(o) {}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
    private:
        DrumMachineComponent& owner;
    };

    // Scrollable step grid, drawn wide (kNumSteps * kStepWidth) inside a
    // horizontal-only Viewport.
    class StepGridContent : public juce::Component
    {
    public:
        explicit StepGridContent(DrumMachineComponent& o) : owner(o) {}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
    private:
        DrumMachineComponent& owner;
    };

    std::vector<DrumRow> rows;

    HeaderColumn    headerColumn   { *this };
    StepGridContent stepGridContent{ *this };
    juce::Viewport  stepViewport;

    int  playheadStep    = -1;
    bool playheadVisible = false;

    friend class HeaderColumn;
    friend class StepGridContent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumMachineComponent)
};
