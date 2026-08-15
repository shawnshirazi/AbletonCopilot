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
#include "GeneratedDrumGridComponent.h"
#include "DrumSampleSelector.h"
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
#include "Engine/BassEngine.h"
#include "Engine/MusicIdentity.h"
#include "MusicTheory/MelodyMotifGenerator.h"
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

    // The single "Generate" workflow: builds ONE shared Engine::MusicIdentity
    // (Source/Engine/MusicIdentity.h - drums via the already-existing native
    // 16-bar Engine::generateDrop(), bass via Engine::generateBassPattern()
    // tiled x2) plus a melody motif (MusicTheory/MelodyMotifGenerator.h,
    // same tiling), stores both as editor state, then renders the currently
    // selected mode via applyRenderMode(). Drums+bass+melody all become
    // audible from bar 1 of one immediately-looping 16-bar idea - see
    // MusicIdentity.h's own header comment for why the long-form
    // Arrangement/MusicState machinery is no longer what this button drives.
    void generateLoopClicked();

    // DROP/BREAKDOWN mode buttons - re-renders the STORED currentIdentity
    // via applyRenderMode() with no new seed drawn and no call into
    // generateLoopClicked(), so switching modes can never regenerate the
    // underlying motifs (see Engine::renderMode's own guarantee).
    void modeClicked(Engine::RenderMode mode);

    // Sends currentIdentity/currentMelodyMotif's ALWAYS-unchanged pattern
    // data to setGeneratedDrumPattern/setGeneratedMelodyPattern only when
    // called from generateLoopClicked() (a real new pattern to store) and
    // applies that mode's RoleMix mute/gain suggestions
    // (setGeneratedDrumRoleMuted/setGeneratedDrumRoleGain/
    // setMelodyTrackMuted) every time, including on a bare mode switch.
    void applyRenderMode(Engine::RenderMode mode, bool alsoSendPatternData);

    // Part D of the bass-runtime-bug + sample-selection investigation:
    // "why did the selector think this was a good Melodic Techno
    // percussion sound?" - writes the full PERC candidate pool (every
    // field DrumSampleSelector.cpp's ranking actually used: path, pack/
    // source tier, DSP features, and the individual score components),
    // ranked, top 10 only, to
    // ~/Library/AbletonCopilot/percussion_selection_debug.txt so the
    // real ranking behind percA/percB's actual choice can be inspected
    // after every Generate Drum Pattern click - not a one-off report,
    // always current. sel/pool/seed/bpm are exactly what
    // generateLoopClicked() already computed for this click - see
    // its own call to selectDrumSamples().
    static void logPercussionCandidateDiagnostics(const std::vector<IndexedSample>& indexed,
                                                    const DrumSampleSelection& sel,
                                                    double bpm);

    // Keeps generateLoopButton's enabled state and label in sync
    // with sampleIndexReady/libraryDir - called once at construction and
    // again whenever sampleIndex.onIndexReady fires. Disabled (not just
    // silently ineffective) is the fix for the confirmed race where
    // clicking Generate before the first background scan completes made
    // every role's candidate pool 0, so every role fell back to
    // DrumVoiceSynth with no indication why.
    void updateGenerateButtonAvailability();

    // Formats rackScanStarted/rackFilesDiscovered/.../onIndexReadyFired
    // (see their declarations below) into libraryScanStatusLabel's text -
    // called once at construction and again every timerCallback() tick,
    // plus immediately whenever one of those fields changes.
    void updateLibraryScanStatusLabel();

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

    // Drum-generation milestone UI cleanup: the manual/sample-based drum
    // grid, melody grid, Serum 2 track panels, and the genre/key/EQ row
    // (all melody/manual-grid controls, not part of the deterministic
    // drum-generation focus) are hidden so the visible editor is just
    // Generate Drum Pattern + status + the generated pattern grid. Nothing
    // is deleted or disconnected - drumMachine/melodyGrid/melodyPanels and
    // their processor wiring, and the background library/preset scans,
    // all keep running exactly as before; only setVisible()/layout are
    // gated by this flag. Flip back to true to restore the full editor.
    static constexpr bool kShowFullUI = false;

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
    // The loop-generator workflow (Source/Engine/MusicIdentity.h) - synthesizes
    // and plays real audio directly from AbletonCopilot itself, see
    // generateLoopClicked(). Not gated by kShowExperimentalFeatures - this
    // is the new engine, not the hidden heuristic/AI layer.
    juce::TextButton  generateLoopButton { "Generate" };

    // DROP/BREAKDOWN - two mixes of the SAME generated MusicIdentity, see
    // modeClicked()/applyRenderMode(). currentRenderMode tracks which one
    // is active so a mode button click can re-render without regenerating.
    juce::TextButton  dropModeButton      { "DROP" };
    juce::TextButton  breakdownModeButton { "BREAKDOWN" };
    Engine::RenderMode currentRenderMode = Engine::RenderMode::Drop;

    // The identity/melody generated by the last generateLoopClicked() -
    // std::nullopt until the first Generate click. modeClicked() reads
    // this (never regenerates it) to switch mixes.
    std::optional<Engine::MusicIdentity> currentIdentity;
    std::vector<int8_t>                  currentMelodyMotif; // 256 steps once generated

    // Temporary "trace the library-scan pipeline" diagnostic (separate from
    // drumPatternStatusLabel, which only ever shows post-Generate-click
    // decode diagnostics) - always visible, refreshed every timerCallback()
    // tick from real state set at each real pipeline step below, so a
    // stuck scan is visibly stuck at a specific stage instead of a single
    // static "Scanning..." button label with no progress signal at all.
    juce::Label       libraryScanStatusLabel;
    juce::Label       drumPatternStatusLabel;
    // Read-only display of the generated pattern, fed the exact same data
    // as processor.setGeneratedDrumPattern() - see generateLoopClicked().
    GeneratedDrumGridComponent generatedDrumGrid;

    // Loop-generator mute row: independent per-role MIX control (Part 10 of
    // the loop-generator brief) - never touches the stored pattern, see
    // PluginProcessor::setGeneratedDrumRoleMuted. One control per
    // Engine::DrumRole ordinal (Kick/Clap/HatClosed/HatOpen/PercA/PercB, in
    // that order - see the array init in the constructor).
    struct DrumRoleMuteControl
    {
        juce::Label      roleLabel;
        juce::TextButton muteButton { "M" };
    };
    std::array<DrumRoleMuteControl, 6> drumRoleMutes;

    // Serum2 status for the loop workflow's two always-present voices
    // (track 0 = Bass, track 1 = Melody) - reuses the existing, already-
    // honest processor.getMelodyTrackStatus() (never fabricates a preset
    // name) for both.
    juce::Label serumBassStatusLabel;
    juce::Label serumMelodyStatusLabel;
    juce::Label bassMidiRangeLabel; // real, computed-not-assumed MIDI register of the just-generated bass - see the reference-analysis report's Part 1
    juce::Label loopLengthLabel;

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

    // Analyzed+cached drum-role samples (see Source/DrumSampleIndex.h) -
    // rebuilt in the background whenever rackBrowser rescans, off the
    // audio thread. generateLoopClicked() ranks/selects from
    // whatever's here at click time (see Source/DrumSampleSelector.h).
    // Confirmed root cause of "every role falls back to DrumVoiceSynth
    // even though the library has real candidates": the FIRST scan can
    // take several seconds (a real, measured background DSP-analysis
    // cost, not a bug in the analysis itself), and clicking Generate
    // before it completes used to silently proceed with an empty index -
    // every role's candidate pool was 0, so every role fell back, with
    // nothing telling the user why. Generate is now disabled
    // (updateGenerateButtonAvailability()) until sampleIndexReady is true.
    DrumSampleIndex            sampleIndex;
    std::vector<IndexedSample> latestSampleIndex;
    bool                       sampleIndexReady = false; // true once sampleIndex.onIndexReady has fired at least once

    // Library-scan pipeline diagnostics (temporary, "isolate the stuck
    // Scanning Library... problem" milestone) - each field is set at the
    // exact real step it names, never inferred/reconstructed, so
    // libraryScanStatusLabel can show precisely which stage the pipeline
    // is actually at right now.
    bool   rackScanStarted        = false; // rackBrowser.setLibraryDir() has been called
    double scanPipelineStartSecs  = 0.0;   // juce::Time::getMillisecondCounterHiRes()*0.001 at the moment rackScanStarted flips true - lets the label show real elapsed seconds, proving the timer/UI is alive even mid-scan, not just a static label
    int  rackFilesDiscovered    = -1;    // -1 = not yet reported; set from RackBrowserComponent::onFilesDiscovered
    bool rackScanCompleted      = false; // rackBrowser.onRacksChanged has fired
    bool onRacksChangedFired    = false; // same signal as rackScanCompleted, tracked separately so the label can show the callback explicitly
    bool sampleIndexScanStarted = false; // sampleIndex.analyzeRacks() has been called
    bool onIndexReadyFired      = false; // same signal as sampleIndexReady, tracked separately for the same reason as onRacksChangedFired
    int  kickCandidateCount = 0, clapCandidateCount = 0, hatCandidateCount = 0, percCandidateCount = 0;

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
