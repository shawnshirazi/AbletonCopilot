#pragma once
#include <JuceHeader.h>
#include "Analysis/FeedbackEngine.h" // FeedbackItem/Severity — reused as the card model
#include "MusicTheory/MelodyCritic.h"
#include "MusicTheory/MelodicTechnoTheory.h" // SongSection
#include <vector>

// Second tab: "listen to what's already playing, give real melodic-techno
// ideas back" — self-contained, independent of the older hidden mix-EQ
// analyzer widgets still living on PluginEditor (those are untouched). Owner
// (PluginEditor) wires onAnalyzeClicked to the actual analysis call and
// pushes results back in via setResult/setStatus/setBusy.
//
// Three suggestion families:
//  - Analysis cards (setResult): read-only, from listening to rendered audio
//    (key/progression, groove, section, technique, presets - nothing to apply).
//  - Melody-edit cards (setMelodyEditSuggestions): actionable, from the
//    instant/audio-free MelodyCritic pass over the piano roll.
//  - Analysis-tweak cards (setAnalysisTweaks): actionable, from TheoryAdvisor
//    - the subset of what it hears that maps to a real, applicable change
//    (a clashing note, a key mismatch, a sample to assign to a drum row).
// The latter two share one generic card type (ActionableFeedback + ActionableCard)
// so any suggestion source can plug in real Approve/Reject/Approve-All behavior.
class AdvisorPanelComponent : public juce::Component
{
public:
    AdvisorPanelComponent();

    std::function<void()> onAnalyzeClicked;
    std::function<void()> onSuggestMelodyEditsClicked;
    std::function<void()> onArrangementClicked;

    // Fired whenever any card starts/stops previewing, so the owner can
    // force its own audio audible while comparing before/after (see
    // PluginEditor::beginPreview/endPreview) regardless of which tab is
    // showing.
    std::function<void()> onPreviewStarted;
    std::function<void()> onPreviewEnded;

    // Ableton doesn't pass locator/marker text to plugins over VST3, so the
    // current song section can't be auto-detected from the host - the user
    // tells us instead. Read at Analyze time.
    MelodicTechnoTheory::SongSection getSelectedSection() const;

    void setBusy(bool busy);
    void setStatus(const juce::String& text);
    void setResult(const juce::String& keyBpmText, const std::vector<FeedbackItem>& items);

    // Live capture status — currentlyPlaying/bufferedSeconds already exist
    // and update continuously on the processor (see PluginProcessor.cpp's
    // resetCapture()-on-transport-start), this just surfaces them. Call from
    // the owner's timer while this tab is visible.
    void setListeningStatus(bool isPlaying, int bufferedSecs);

    // Generic actionable suggestion: item is what's displayed, applyChange/
    // revertChange are the owner-supplied, symmetric actions that switch
    // the underlying state to "after" and back to "before" (writing a
    // note, changing the key picker, reassigning a drum sample, etc.) - the
    // card itself has no idea what kind of change it is, just that it can
    // be flipped both ways for an A/B preview before committing.
    struct ActionableFeedback
    {
        FeedbackItem           item;
        std::function<void()>  applyChange;
        std::function<void()>  revertChange;
    };
    void setMelodyEditSuggestions(const std::vector<ActionableFeedback>& cards); // "Melody Edits" section
    void setAnalysisTweaks(const std::vector<ActionableFeedback>& cards);        // "Tweaks from Analysis" section
    void setArrangementSuggestions(const std::vector<ActionableFeedback>& cards); // "Arrangement" section

    void resized() override;
    void paint(juce::Graphics&) override;

private:
    // Read-only analysis card — category/headline/detail/fix laid out as a
    // wrapped multi-paragraph block, height computed from actual content
    // (not fixed/truncated) so nothing gets cut off.
    class SuggestionCard : public juce::Component
    {
    public:
        void setItem(const FeedbackItem& item);
        int  getRequiredHeight(int width);
        void resized() override;
        void paint(juce::Graphics&) override;

    protected:
        static constexpr int kPadding = 10;
        FeedbackItem      data;
        juce::TextLayout  layout;
        virtual void rebuildLayout(int width);
    };

    // Same visual body as SuggestionCard, plus a Preview -> A/B toggle ->
    // Keep/Undo flow: Preview applies the change and lets you flip back and
    // forth against the "before" state while it plays, Keep locks it in
    // (same disabled "Applied" look as before), Undo reverts and removes
    // the card (same path as Reject).
    class ActionableCard : public SuggestionCard
    {
    public:
        ActionableCard();
        void setAction(const FeedbackItem& item, std::function<void()> apply, std::function<void()> revert);
        void resized() override;

        // For "Approve All" - applies directly and locks in, skipping the
        // manual preview/compare step. No-ops if already applied.
        void keepDirectly();

        std::function<void()> onRejected;       // fired with no side effect — owner removes the card
        std::function<void()> onPreviewStarted; // fired when Preview is first clicked - owner un-suppresses playback
        std::function<void()> onPreviewEnded;   // fired on Keep or Undo - owner restores suppression

    private:
        enum class State { Idle, Previewing, Applied };
        void enterAppliedState();
        void updateButtonVisibility();

        std::function<void()> applyChange;
        std::function<void()> revertChange;
        State                  state        = State::Idle;
        bool                    hearingAfter = false; // which side the toggle is on, while Previewing

        juce::TextButton previewButton { "Preview" };
        juce::TextButton rejectButton  { "Reject" };
        juce::TextButton toggleButton  { "Hear: Before" };
        juce::TextButton keepButton    { "Keep" };
        juce::TextButton undoButton    { "Undo" };
    };

    juce::Label       titleLabel;
    juce::ComboBox    sectionPicker;
    juce::TextButton  analyzeButton { "Analyze Now" };
    juce::Label       listeningStatusLabel;
    juce::Label       keyBpmLabel;
    juce::Label       statusLabel;
    juce::Viewport    viewport;
    juce::Component   content;
    juce::OwnedArray<SuggestionCard> cards;

    juce::TextButton  suggestEditsButton { "Suggest Melody Edits" };
    juce::TextButton  approveAllButton   { "Approve All" };
    juce::Label       melodyEditsLabel;
    juce::OwnedArray<ActionableCard> melodyEditCards;

    juce::TextButton  analysisApproveAllButton { "Approve All" };
    juce::Label       analysisTweaksLabel;
    juce::OwnedArray<ActionableCard> analysisTweakCards;

    juce::TextButton  arrangementButton         { "Arrangement Ideas" };
    juce::TextButton  arrangementApproveAllButton { "Approve All" };
    juce::Label       arrangementLabel;
    juce::OwnedArray<ActionableCard> arrangementCards;

    void layoutContent();
    void removeMelodyEditCard(ActionableCard* card);
    void removeAnalysisTweakCard(ActionableCard* card);
    void removeArrangementCard(ActionableCard* card);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdvisorPanelComponent)
};
