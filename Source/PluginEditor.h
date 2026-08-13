#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Analysis/AudioAnalyzer.h"
#include "Analysis/GenreProfiles.h"
#include "StackRenderer.h"
#include "StackBrowserComponent.h"
#include "RackBrowserComponent.h"
#include "DrumMachineComponent.h"
#include "DrumPatternRenderer.h"
#include "MelodyCategory.h"
#include "MelodyGridComponent.h"
#include "AdvisorPanelComponent.h"
#include "Analysis/TheoryAdvisor.h"
#include "Analysis/ArrangementAdvisor.h"
#include "Analysis/ReferenceTrackAnalyzer.h"
#include "Analysis/ReferenceFragmentJob.h"
#include "MusicTheory/MelodyCritic.h"
#include "PresetLibraryScanner.h"
#include "Engine/Grid.h"
#include "Engine/DrumEngine.h"
#include <optional>

class AbletonCopilotAudioProcessorEditor
    : public juce::AudioProcessorEditor,
      private juce::Timer
{
public:
    explicit AbletonCopilotAudioProcessorEditor(AbletonCopilotAudioProcessor&);
    ~AbletonCopilotAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void runAnalysis();
    void applyResult(const AnalysisResult& result);
    void showStatus(const juce::String& msg);

    void chooseLibrary();
    void runStackGeneration();
    void showStackStatus(const juce::String& msg);

    void runDrumPatternGeneration();
    void pushAllDrumRowsToProcessor();
    void exportMelodyPattern();
    std::pair<int, bool> getSelectedKey() const; // {rootSemitone, isMinor}

    // Studio / Advisor tab switch — see advisorPanel below.
    void setShowingAdvisor(bool showAdvisor);
    void runTheoryAdvisorAnalysis();
    void buildArrangementSuggestions();

    // Reference-track import (Tier 1) - see Analysis/ReferenceTrackAnalyzer.h.
    // Real analysis of a user-supplied audio file, used as the comparison
    // target instead of a generic genre archetype.
    void loadReferenceTrackClicked();
    void onReferenceAnalyzed(const ReferenceAnalysisResult& result);
    void clearReferenceTrack();
    void refreshAnalysisTweakCards(); // combines lastAdvisorTweaks + pendingReferenceTweaks

    // Tier 2 - see Analysis/ReferenceFragmentJob.h.
    void extractPatternsClicked();
    void applyReferenceDrumsClicked();

    // Phase 1 deterministic engine (Source/Engine/) - synthesizes and plays
    // real audio directly from AbletonCopilot itself (see
    // setGeneratedDrumPattern/DrumVoiceSynth rendering in
    // PluginProcessor.cpp), no Drum Rack or companion plugin required. Also
    // emits the pattern as MIDI, kept only as an optional secondary output.
    void generateDrumPatternClicked();

    // Small local helpers so the reference-track status UI (now Studio-tab
    // members, not AdvisorPanelComponent's) has one place each that updates
    // text/visibility/enabled-state - mirrors the setReferenceStatus/
    // setExtractPatternsStatus methods AdvisorPanelComponent used to expose.
    void setReferenceStatusText(const juce::String& text, bool hasReference);
    void setExtractPatternsStatusText(const juce::String& text, bool busy);

    // A suggestion preview forces our own audio audible even while the
    // Advisor tab would normally suppress it (see updatePlaybackSuppression).
    // Reference-counted since more than one card could theoretically be
    // previewing at once.
    void updatePlaybackSuppression();
    void beginPreview();
    void endPreview();
    void buildMelodyEditSuggestions();

    void paintHeader(juce::Graphics&, juce::Rectangle<int>);
    void paintTransportBar(juce::Graphics&, juce::Rectangle<int>);
    void paintLiveMeters(juce::Graphics&, juce::Rectangle<int>);

    static CorrectionParams computeCorrections(const AudioFeatures& f, const GenreProfile* p);
    static void             exportAnalysisJSON(const AnalysisResult& r);

    //==========================================================================
    // Per-track Serum2 panel: prompt-driven generation ("Generate rolling
    // bass melody") + captured-preset cycling + Capture + that track's own
    // Serum2 GUI window. One of these per melody track in melodyGrid, shown
    // in a scrollable list below the melody note grid so kMaxMelodyTracks
    // fits without growing the window. Talks back to the owner editor
    // directly (same pattern as DrumMachineComponent's HeaderColumn/
    // StepGridContent).
    class MelodyTrackPanel : public juce::Component
    {
    public:
        MelodyTrackPanel(AbletonCopilotAudioProcessorEditor& o, int trackIndexIn);

        void paint(juce::Graphics&) override;
        void resized() override;

        int trackIndex;
        MelodyCategory category = MelodyCategory::Bass;
        // Captured Serum2 sounds for this track's category — real state
        // snapshots (see PluginProcessor::captureMelodyTrackState), not
        // .fxp files (that approach never actually worked — Serum2's VST3
        // state is its own proprietary format, confirmed via a diagnostic
        // dump, nothing to do with VST2 .fxp chunk bytes).
        juce::Array<juce::File> presetFiles;
        int presetIndex = -1;
        std::unique_ptr<juce::DocumentWindow> serumWindow;

        juce::Label       titleLabel;
        juce::TextButton  prevPresetButton { "<" };
        juce::TextButton  nextPresetButton { ">" };
        juce::TextButton  captureButton    { "Capture" };
        juce::TextButton  openSerumButton  { "Open Serum 2" };
        juce::Label       statusLabel;
        juce::TextEditor  promptBox;
        juce::TextButton  generateButton { "Generate" };

        static constexpr int kHeight = 92;

    private:
        AbletonCopilotAudioProcessorEditor& owner;
    };
    friend class MelodyTrackPanel;

    void addMelodyTrackClicked();
    void scanPresetsForTrack(MelodyTrackPanel& panel);
    void cyclePresetForTrack(MelodyTrackPanel& panel, int direction);
    void captureCurrentSound(MelodyTrackPanel& panel);
    void generateForTrack(MelodyTrackPanel& panel);
    void openSerumWindowForTrack(MelodyTrackPanel& panel);
    void updateTrackTitle(MelodyTrackPanel& panel);

    AbletonCopilotAudioProcessor& processor;

    juce::ComboBox   genrePicker;
    juce::TextButton analyzeNowButton { "Analyze Now" };
    juce::TextButton applyButton      { "Apply Corrections" };
    juce::Label      statusLabel;

    juce::Label       libraryPathLabel;
    juce::TextButton  chooseLibraryButton { "Choose Sample Library..." };
    juce::TextButton  generateStackButton { "Generate Drum Stack" };
    juce::Label       stackStatusLabel;
    StackBrowserComponent stackBrowser;
    StackRenderer         stackRenderer;
    RackBrowserComponent  rackBrowser;
    juce::File            libraryDir;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Analyzer feature is hidden for now — the plugin is being repurposed as a
    // sample loader / drum machine first; corrections/analysis come back later.
    static constexpr bool kShowAnalyzer = false;

    // Everything built on hand-authored heuristics / approximate signal
    // analysis (Advisor tab critique, reference-track load/extract/apply,
    // fragment/motif-based Generate) is hidden while the plugin is rebuilt
    // on a deterministic engine foundation (see Source/Engine/). Nothing
    // is deleted - flip this back to true to restore it exactly as it was.
    // Manual step editing, key/genre pickers, and Serum2 capture/preset
    // controls are NOT gated by this - they're basic infrastructure, not
    // the experimental generation/critique layer.
    static constexpr bool kShowExperimentalFeatures = false;

    // rackBrowser now runs headless — just the background scanner feeding
    // drumMachine's rows. Its own browsing UI (sidebar + sample list) is no
    // longer shown; the Drum Machine is the only view.
    juce::ComboBox        drumGenrePicker;   // locked to "Melodic Techno" for now
    juce::ComboBox        keyPicker;         // key shared by every melody track
    juce::ToggleButton    genreEqToggle { "Genre EQ" }; // per-voice role-based default EQ on/off

    // Reference-track controls (Studio, not Advisor - Advisor is for
    // critiquing existing/already-played audio; a reference track is an
    // input to generation, so it lives here). See loadReferenceTrackClicked/
    // onReferenceAnalyzed/clearReferenceTrack/extractPatternsClicked below.
    // Phase 1 deterministic engine (Source/Engine/) - synthesizes and plays
    // real audio directly from AbletonCopilot itself, see
    // generateDrumPatternClicked(). Not gated by kShowExperimentalFeatures -
    // this is the new engine, not the hidden heuristic/AI layer.
    juce::TextButton  generateDrumPatternButton { "Generate Drum Pattern" };
    juce::Label       drumPatternStatusLabel;

    juce::TextButton  loadReferenceButton  { "Load Reference Track..." };
    juce::TextButton  clearReferenceButton { "Clear" };
    juce::Label       referenceStatusLabel;
    juce::TextButton  extractPatternsButton { "Extract Bass & Drum Patterns From This Track" };
    juce::TextButton  applyReferenceDrumsButton { "Apply Reference Drums" };
    juce::Label       extractPatternsStatusLabel;
    DrumMachineComponent  drumMachine;
    DrumPatternRenderer   drumPatternRenderer;

    // Melody/MIDI note editing — its own section below the drum grid (not
    // squeezed into drumMachine's rows).
    MelodyGridComponent   melodyGrid;

    // Per-track Serum2 panels — one per melody track (track 0/"Bass" exists
    // from construction; more via addMelodyTrackButton, up to
    // AbletonCopilotAudioProcessor::kMaxMelodyTracks).
    juce::OwnedArray<MelodyTrackPanel> melodyPanels;
    juce::TextButton                   addMelodyTrackButton { "+ Add Serum 2 Track" };
    // A real child Label (not manually painted) so it scrolls correctly as
    // part of mainContent inside mainViewport.
    juce::Label serumTracksHeadingLabel;

    // Everything below the fixed header/genre-key row lives in mainContent,
    // sized to its actual needed height and scrolled as one unit by
    // mainViewport — a single vertical scroll for the whole plugin rather
    // than several independent nested ones, and it keeps the window itself
    // at a fixed, reasonable size regardless of how much content exists
    // (drum row count, melody track count, Serum 2 panel count).
    juce::Viewport  mainViewport;
    juce::Component mainContent;

    // Studio / Advisor tab switch. Advisor is a self-contained second view
    // (melodic-techno theory + listening suggestions) occupying the same
    // area as mainViewport; only one is visible at a time. latestRacks
    // caches whatever rackBrowser last scanned (previously discarded after
    // forwarding to drumMachine) so the advisor can suggest real filenames.
    juce::TextButton      studioTabButton   { "Studio" };
    juce::TextButton      advisorTabButton  { "Advisor" };
    bool                  showingAdvisor = false;
    int                   activePreviewCount = 0;
    AdvisorPanelComponent advisorPanel;
    TheoryAdvisor          theoryAdvisor;
    MelodyCritic            melodyCritic;
    ArrangementAdvisor      arrangementAdvisor;
    std::vector<Rack>      latestRacks;
    PresetLibraryScanner    presetScanner;
    std::vector<PresetEntry> latestPresets;

    bool   wasPlayheadRunning = false;
    double playheadStartTime  = 0.0;

    AnalysisResult    lastResult;
    CorrectionParams  pendingCorrections;
    bool              hasResult            = false;
    bool              hasPendingCorrections = false;
    bool              wasAnalyzing         = false;

    // Reference track (Tier 1) — the Advisor tab's own analysis-tweak cards
    // section is shared between what TheoryAdvisor finds (lastAdvisorTweaks,
    // rebuilt each time runTheoryAdvisorAnalysis completes) and what loading
    // a reference track proposes (pendingReferenceTweaks, e.g. the detected-
    // key Approve card); refreshAnalysisTweakCards() combines both so
    // neither source clobbers the other's cards.
    std::unique_ptr<juce::FileChooser>                referenceFileChooser;
    ReferenceTrackAnalyzer                            referenceAnalyzer;
    ReferenceFragmentJob                              referenceFragmentJob;
    std::optional<ReferenceAnalysisResult>            loadedReference;
    std::vector<AdvisorPanelComponent::ActionableFeedback> lastAdvisorTweaks;
    std::vector<AdvisorPanelComponent::ActionableFeedback> pendingReferenceTweaks;

    // Tier 2 drums (Part B) - set once extractPatternsClicked()'s job
    // reports drum patterns, cleared on clearReferenceTrack(). Held until
    // the user explicitly clicks Apply Reference Drums (never auto-applied -
    // it overwrites existing drum-row patterns/samples).
    bool hasReferenceDrumPatterns = false;
    std::vector<DrumRolePattern> pendingDrumPatterns;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AbletonCopilotAudioProcessorEditor)
};
