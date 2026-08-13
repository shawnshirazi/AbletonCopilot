#pragma once
#include <JuceHeader.h>
#include "MelodyCategory.h"
#include <array>
#include <vector>

// One melody track: an independent Serum2-hosted voice with its own 8-bar
// pitch pattern, preset name (display only — actual preset capture/load
// lives in PluginEditor/PluginProcessor), and mute/solo.
struct MelodyRow
{
    juce::String              elementName; // badge label, e.g. "BASS"
    juce::Colour               colour;
    juce::String               presetName;  // editor-set display text
    std::array<int8_t, 128>    offsets {};
    bool                       muted = false;
    bool                       solo  = false;
};

// Dedicated melody/MIDI note-editing surface — deliberately its own
// component (not squeezed into DrumMachineComponent's rows) so a note has
// real vertical room to be legible: kRowHeight is more than 2.5x the drum
// grid's. Sized to its natural full height (like DrumMachineComponent);
// vertical scrolling for the whole plugin happens once, at the top level
// (PluginEditor's outer viewport), not independently per section.
class MelodyGridComponent : public juce::Component
{
public:
    MelodyGridComponent();

    int  addTrack(const juce::String& badgeLabel, juce::Colour colour); // returns new track index
    int  getNumTracks() const noexcept { return (int) tracks.size(); }
    std::array<int8_t, 128> getTrackOffsets(int trackIndex) const;
    bool isTrackMuted(int trackIndex) const;
    bool isTrackSolo(int trackIndex) const;
    void setTrackPresetName(int trackIndex, const juce::String& name);
    void setTrackIdentity(int trackIndex, const juce::String& badgeLabel, juce::Colour colour);

    // Writes a single step directly — same effect as a manual drag (mutates,
    // repaints, fires onTrackChanged so the export/Serum-2 pipeline picks it
    // up). Used by the Advisor tab to apply an approved melody-edit suggestion.
    void setStepOffset(int trackIndex, int step, int8_t newOffset);

    // Sets mute state directly (vs. toggleMute, only reachable via the row's
    // own M button) - used by the Advisor tab to mute/unmute a track as an
    // A/B preview mechanism.
    void setTrackMuted(int trackIndex, bool muted);

    static constexpr int8_t kMelodyOff = -128;

    // Writes a fresh 8-bar pattern for one track. Bass uses the real 2-bar
    // transcribed-fragment library (see MLPipeline/extract_fragments.py),
    // bucketed by note density so a style word actually changes the output
    // (Rolling -> busiest third of the real library, Atmospheric -> sparsest
    // third). Every other category uses a generalized motif generator built
    // from real house/techno bassline idioms, picked from a dense or sparse
    // pool by the same style. Call again for a different variation — it's
    // randomized each time.
    void generateForTrack(int trackIndex, int keyRootSemitone, bool isMinor,
                           MelodyCategory category, MelodyStyle style);

    void setPlayheadStep(int step, bool visible);

    // Tier 2 reference-track fragments (see Analysis/ReferenceFragmentJob and
    // MLPipeline/extract_reference_fragments.py) - real transcribed 2-bar
    // bass patterns from one specific user-supplied track, kept in a
    // separate pool from the generic corpus so clearing a reference cleanly
    // reverts generation. When non-empty, generateForTrack's Bass path draws
    // from this pool exclusively instead of the generic library.
    void loadReferenceFragments(const juce::File& jsonFile);
    void clearReferenceFragments();

    void resized() override;

    static constexpr int kRowHeight    = 110; // real note visibility, vs. the drum grid's 42
    static constexpr int kStepsPerBar  = 16;
    static constexpr int kNumBars      = 8;
    static constexpr int kNumSteps     = kStepsPerBar * kNumBars; // 128

    std::function<void(int trackIndex)>             onTrackChanged;
    std::function<void(int trackIndex, bool muted)> onTrackMuteToggled;
    std::function<void(int trackIndex, bool solo)>  onTrackSoloToggled;
    std::function<void(int trackIndex, int dir)>    onTrackPresetCycle;

private:
    void toggleMute(int trackIndex);
    void toggleSolo(int trackIndex);
    void setStepFromY(int trackIndex, int step, int yWithinRow);
    void toggleStep(int trackIndex, int step, int yWithinRow);

    static constexpr int kFragmentSteps = kNumSteps / 4; // 32 = 2 bars

    void loadBassFragmentsIfNeeded();
    void generateBassFromFragments(int trackIndex, int keyRootSemitone, bool isMinor, MelodyStyle style);
    void generateMelodyFromMotifShapes(int trackIndex, int keyRootSemitone, bool isMinor,
                                        MelodyCategory category, MelodyStyle style);

    // Shared by loadBassFragmentsIfNeeded() and loadReferenceFragments() -
    // parses a fragments JSON file (array of {steps:[...]}, see
    // MLPipeline/extract_fragments.py's output shape) into outFragments.
    // Returns false if the file doesn't exist or isn't a valid fragments array.
    bool parseFragmentsFile(const juce::File& file,
                             std::vector<std::array<int8_t, kFragmentSteps>>& outFragments) const;

    std::vector<std::array<int8_t, kFragmentSteps>> bassFragments;
    // Indices into bassFragments, bucketed by note density — see
    // DrumMachineComponent's earlier version of this comment (moved here
    // unchanged): thresholds picked from the real library's distribution
    // (median 8, p25 6, p75 11 across 3184 fragments).
    std::vector<int> sparseBassFragmentIndices; // density <= 6
    std::vector<int> denseBassFragmentIndices;  // density >= 11
    bool              fragmentsLoaded = false;

    // Reference-track-specific fragments (Tier 2) - no density bucketing,
    // see loadReferenceFragments()'s doc comment in the header above for why.
    std::vector<std::array<int8_t, kFragmentSteps>> referenceBassFragments;

    static constexpr int kHeaderWidth   = 195;
    static constexpr int kArrowWidth    = 18;
    static constexpr int kStepWidth     = 20;
    static constexpr int kMuteWidth     = 20;
    static constexpr int kBadgeDiameter = 38;

    class HeaderColumn : public juce::Component
    {
    public:
        explicit HeaderColumn(MelodyGridComponent& o) : owner(o) {}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
    private:
        MelodyGridComponent& owner;
    };

    class StepGridContent : public juce::Component
    {
    public:
        explicit StepGridContent(MelodyGridComponent& o) : owner(o) {}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
    private:
        MelodyGridComponent& owner;
        bool dragStarted = false;
    };

    std::vector<MelodyRow> tracks;

    HeaderColumn    headerColumn   { *this };
    StepGridContent stepGridContent{ *this };
    juce::Viewport  stepViewport; // horizontal-only, same pattern as DrumMachineComponent

    int  playheadStep    = -1;
    bool playheadVisible = false;

    friend class HeaderColumn;
    friend class StepGridContent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MelodyGridComponent)
};
