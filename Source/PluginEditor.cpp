#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UIStyle.h"
#include "Engine/DrumSamplePackTier.h"
#include "Engine/BreakdownArrangement.h"
#include "RuntimeStatusText.h"
#include "SerumPresetStatus.h"
#include <algorithm>
#include <cmath>
#include <numeric>

using namespace UIStyle;

//==============================================================================
// MelodyTrackPanel — one per melody track, shown in a scrollable list below
// the drum grid. Prompt-driven generation ("Generate rolling bass melody"),
// preset cycling, and that track's own Serum2 GUI window.

AbletonCopilotAudioProcessorEditor::MelodyTrackPanel::MelodyTrackPanel(
    AbletonCopilotAudioProcessorEditor& o, int trackIndexIn)
    : trackIndex(trackIndexIn), owner(o)
{
    titleLabel.setColour(juce::Label::textColourId, kTextPrimary);
    titleLabel.setFont(heading());
    addAndMakeVisible(titleLabel);

    prevPresetButton.setColour(juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
    prevPresetButton.setColour(juce::TextButton::textColourOffId, kTextDim);
    prevPresetButton.onClick = [this] { owner.cyclePresetForTrack(*this, -1); };
    addAndMakeVisible(prevPresetButton);

    nextPresetButton.setColour(juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
    nextPresetButton.setColour(juce::TextButton::textColourOffId, kTextDim);
    nextPresetButton.onClick = [this] { owner.cyclePresetForTrack(*this, 1); };
    addAndMakeVisible(nextPresetButton);

    captureButton.setColour(juce::TextButton::buttonColourId,  kPanelAlt);
    captureButton.setColour(juce::TextButton::textColourOffId, kAccent);
    captureButton.onClick = [this] { owner.captureCurrentSound(*this); };
    addAndMakeVisible(captureButton);

    openSerumButton.setColour(juce::TextButton::buttonColourId,  kPanelAlt);
    openSerumButton.setColour(juce::TextButton::textColourOffId, kAccent);
    openSerumButton.onClick = [this] { owner.openSerumWindowForTrack(*this); };
    addAndMakeVisible(openSerumButton);

    statusLabel.setText("Loading Serum 2...", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, kTextDim);
    statusLabel.setFont(small());
    addAndMakeVisible(statusLabel);

    promptBox.setColour(juce::TextEditor::backgroundColourId, kPanelAlt);
    promptBox.setColour(juce::TextEditor::textColourId, kTextPrimary);
    promptBox.setColour(juce::TextEditor::outlineColourId, kBorder);
    promptBox.setColour(juce::TextEditor::focusedOutlineColourId, kAccent);
    promptBox.setFont(body());
    promptBox.setTextToShowWhenEmpty("Generate rolling bass melody...", kTextDim);
    promptBox.onReturnKey = [this] { owner.generateForTrack(*this); };
    addAndMakeVisible(promptBox);

    generateButton.setColour(juce::TextButton::buttonColourId,  kAccent.withAlpha(0.18f));
    generateButton.setColour(juce::TextButton::textColourOffId, kAccent);
    generateButton.onClick = [this] { owner.generateForTrack(*this); };
    addAndMakeVisible(generateButton);

    // Fragment/motif-based generation is hidden while the deterministic
    // engine rebuild is in progress (see kShowExperimentalFeatures) - the
    // prompt box only exists to feed it, so it's hidden alongside it.
    generateButton.setVisible(kShowExperimentalFeatures);
    promptBox.setVisible(kShowExperimentalFeatures);
}

void AbletonCopilotAudioProcessorEditor::MelodyTrackPanel::paint(juce::Graphics& g)
{
    g.setColour(kPanel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
}

void AbletonCopilotAudioProcessorEditor::MelodyTrackPanel::resized()
{
    auto area = getLocalBounds().reduced(10, 8);

    auto titleRow = area.removeFromTop(22);
    openSerumButton.setBounds(titleRow.removeFromRight(100));
    titleRow.removeFromRight(6);
    captureButton.setBounds(titleRow.removeFromRight(70));
    titleRow.removeFromRight(6);
    nextPresetButton.setBounds(titleRow.removeFromRight(22));
    prevPresetButton.setBounds(titleRow.removeFromLeft(22));
    titleRow.removeFromLeft(6);
    titleLabel.setBounds(titleRow);

    area.removeFromTop(2);
    statusLabel.setBounds(area.removeFromTop(14));

    area.removeFromTop(6);
    auto promptRow = area.removeFromTop(26);
    generateButton.setBounds(promptRow.removeFromRight(90));
    promptRow.removeFromRight(6);
    promptBox.setBounds(promptRow);
}

//==============================================================================

AbletonCopilotAudioProcessorEditor::AbletonCopilotAudioProcessorEditor(
    AbletonCopilotAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    StartupTiming::mark("Editor ctor start");
    // A sensible default that fits the fixed header, both mute rows, the
    // compact STATUS block, and the full pattern grid without scrolling
    // in the common case - NOT sized to fit literally every diagnostic
    // block unscrolled (that's what mainViewport's real scroll is for -
    // see resized()). Real resizing is now enabled (setResizable/
    // setResizeLimits below), so this is a starting point, not the only
    // usable size - previously the editor could never be resized at all.
    setSize(980, 700);
    setResizable(true, true);
    setResizeLimits(640, 420, 1600, 1400);

    // Genre picker
    const auto& profiles = GenreProfiles::getInstance();
    auto ids   = profiles.getIds();
    auto names = profiles.getDisplayNames();
    for (int i = 0; i < ids.size(); ++i)
        genrePicker.addItem(names[i], i + 1);
    genrePicker.setSelectedId(1, juce::dontSendNotification);
    genrePicker.setColour(juce::ComboBox::backgroundColourId, kPanel);
    genrePicker.setColour(juce::ComboBox::textColourId,       kTextPrimary);
    genrePicker.setColour(juce::ComboBox::outlineColourId,    kBorder);
    addAndMakeVisible(genrePicker);

    // Manual trigger button (fallback — primary trigger is transport stop)
    analyzeNowButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    analyzeNowButton.setColour(juce::TextButton::textColourOffId, kAccent);
    analyzeNowButton.onClick = [this] { runAnalysis(); };
    addAndMakeVisible(analyzeNowButton);

    // Apply / bypass toggle button
    applyButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    applyButton.setColour(juce::TextButton::textColourOffId, kTextDim);
    applyButton.setEnabled(false);
    applyButton.onClick = [this] {
        if (processor.correctionsEnabled())
            processor.clearCorrections();
        else if (hasPendingCorrections)
            processor.applyCorrections(pendingCorrections);
    };
    addAndMakeVisible(applyButton);

    // Status label
    statusLabel.setText("Place on master bus. Press Play in Ableton.", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, kTextDim);
    statusLabel.setFont(juce::FontOptions(11.5f));
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);

    // Sample library path (persisted between sessions)
    auto libFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("AbletonCopilot/library_path.txt");
    if (libFile.existsAsFile())
        libraryDir = juce::File(libFile.loadFileAsString().trim());

    // First run, nothing chosen yet — default straight to the known library
    // so the grid loads with real samples with no clicking required.
    if (!libraryDir.isDirectory())
    {
        auto fallback = juce::File("/Users/shawnshirazi/shawn music stuff");
        if (fallback.isDirectory())
            libraryDir = fallback;
    }

    // Compact — shown as a small clickable path in the header, not a full
    // row, since it's not one of the two focus areas (drums, Serum 2/MIDI).
    libraryPathLabel.setText(libraryDir.isDirectory() ? libraryDir.getFullPathName()
                                                       : "No sample library chosen",
                              juce::dontSendNotification);
    libraryPathLabel.setColour(juce::Label::textColourId, kTextDim);
    libraryPathLabel.setFont(body());
    libraryPathLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(libraryPathLabel);
    // Sample-library path is only relevant to the manual drum grid/Serum
    // presets, both hidden for the drum-generation milestone - see
    // kShowFullUI. The background library scan itself still runs
    // regardless (see the deferred setLibraryDir call near the end of this
    // constructor), just with no UI to change it while hidden.
    libraryPathLabel.setVisible(kShowFullUI);

    chooseLibraryButton.setButtonText("Change...");
    chooseLibraryButton.setColour(juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
    chooseLibraryButton.setColour(juce::TextButton::textColourOffId, kAccent);
    chooseLibraryButton.onClick = [this] { chooseLibrary(); };
    addAndMakeVisible(chooseLibraryButton);
    chooseLibraryButton.setVisible(kShowFullUI);

    // Render/drag-out flow is hidden for now — the two focus areas are the
    // drum grid and the melody/Serum 2 tracks below it. The rendering code
    // itself (DrumPatternRenderer, StackRenderer) is untouched, just not
    // reachable from the UI at the moment.
    generateStackButton.setButtonText("Render Pattern Audio");
    generateStackButton.onClick = [this] { runDrumPatternGeneration(); };
    addAndMakeVisible(generateStackButton);
    generateStackButton.setVisible(false);

    stackStatusLabel.setText("Pick a sample library, then Render Pattern Audio.", juce::dontSendNotification);
    addAndMakeVisible(stackStatusLabel);
    stackStatusLabel.setVisible(false);

    addAndMakeVisible(stackBrowser);
    stackBrowser.setVisible(false);
    // rackBrowser is never added as a visible child now — it runs headless,
    // just scanning the library on a background thread and feeding rows into
    // drumMachine via onRacksChanged below. The Drum Machine is the only view.

    // Analyzer feature stays hidden (see kShowAnalyzer).
    genrePicker.setVisible(kShowAnalyzer);
    analyzeNowButton.setVisible(kShowAnalyzer);
    applyButton.setVisible(kShowAnalyzer);
    statusLabel.setVisible(kShowAnalyzer);

    // Genre selector — only Melodic Techno is implemented right now.
    drumGenrePicker.addItem("Melodic Techno", 1);
    drumGenrePicker.setSelectedId(1, juce::dontSendNotification);
    drumGenrePicker.setEnabled(false);
    drumGenrePicker.setColour(juce::ComboBox::backgroundColourId, kPanel);
    drumGenrePicker.setColour(juce::ComboBox::textColourId,       kTextPrimary);
    drumGenrePicker.setColour(juce::ComboBox::outlineColourId,    kBorder);
    addAndMakeVisible(drumGenrePicker);
    drumGenrePicker.setVisible(kShowFullUI); // melody/manual-grid genre selector, not part of the drum-generation focus (see kShowFullUI)

    // Key selector — shared by every melody track (they play together, so
    // they stay in the same key); each Generate re-reads whatever's selected.
    {
        static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F",
                                            "F#", "G", "G#", "A", "A#", "B" };
        int itemId = 1;
        for (auto* name : noteNames)
        {
            keyPicker.addItem(juce::String(name) + " major", itemId++);
            keyPicker.addItem(juce::String(name) + " minor", itemId++);
        }
    }
    keyPicker.setSelectedId(2, juce::dontSendNotification); // C minor default
    keyPicker.setColour(juce::ComboBox::backgroundColourId, kPanel);
    keyPicker.setColour(juce::ComboBox::textColourId,       kTextPrimary);
    keyPicker.setColour(juce::ComboBox::outlineColourId,    kBorder);
    keyPicker.onChange = [this] { exportMelodyPattern(); };
    addAndMakeVisible(keyPicker);
    keyPicker.setVisible(kShowFullUI); // melody-track key selector, hidden alongside the melody grid

    genreEqToggle.setToggleState(processor.isGenreEqEnabled(), juce::dontSendNotification);
    genreEqToggle.setColour(juce::ToggleButton::textColourId, kTextDim);
    genreEqToggle.onClick = [this] { processor.setGenreEqEnabled(genreEqToggle.getToggleState()); };
    addAndMakeVisible(genreEqToggle);
    genreEqToggle.setVisible(kShowFullUI); // melody-track EQ toggle, hidden alongside the melody grid

    // Loop-generator workflow (Source/Engine/MusicIdentity.h +
    // Source/Engine/BreakdownArrangement.h) - one click builds a shared
    // MusicIdentity, stitches it into a compact 16-bar loop that plays the
    // whole DROP->BREAKDOWN->PRE_DROP->DROP arc every repeat, and
    // immediately plays drums+bass+melody+pad together, mixed straight
    // into AbletonCopilot's own audio output - no Drum Rack or other
    // instrument required to hear it. No separate DROP/BREAKDOWN mode
    // toggle anymore - the loop always plays the full arc.
    generateLoopButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    generateLoopButton.setColour(juce::TextButton::textColourOffId, kAccent);
    generateLoopButton.onClick = [this] { generateLoopClicked(); };
    addAndMakeVisible(generateLoopButton);
    updateGenerateButtonAvailability(); // starts disabled - the background sample scan hasn't run yet at this point in the constructor

    // From here down: everything is a child of mainContent (scrolled as
    // one unit by mainViewport, see resized()) rather than a direct child
    // of the editor - this is the fix for the vertical-overflow problem
    // (previously this whole stack had no scroll container at all).
    libraryScanStatusLabel.setFont(small());
    libraryScanStatusLabel.setJustificationType(juce::Justification::topLeft);
    libraryScanStatusLabel.setColour(juce::Label::textColourId, kTextDim);
    mainContent.addAndMakeVisible(libraryScanStatusLabel);
    updateLibraryScanStatusLabel(); // initial snapshot - everything "no"/pending until the scan actually progresses

    structuredStatusLabel.setFont(small());
    structuredStatusLabel.setJustificationType(juce::Justification::topLeft);
    structuredStatusLabel.setColour(juce::Label::textColourId, kTextDim);
    structuredStatusLabel.setText("DRUMS  (not generated yet)\n", juce::dontSendNotification);
    mainContent.addAndMakeVisible(structuredStatusLabel);

    drumPatternStatusLabel.setFont(small()); // smaller font: room for 4 full sample paths without ballooning the window
    drumPatternStatusLabel.setJustificationType(juce::Justification::topLeft); // long multi-line diagnostic block reads top-down, not vertically centred
    drumPatternStatusLabel.setColour(juce::Label::textColourId, kTextDim);
    drumPatternStatusLabel.setText(
        "Not generated yet.", juce::dontSendNotification);
    mainContent.addAndMakeVisible(drumPatternStatusLabel);
    // Sized to its own real (dynamic, 9-row) content height in resized()
    // below and positioned directly in mainContent's layout flow -
    // mainViewport provides the vertical scroll for this along with
    // everything else, no separate viewport just for the grid.
    mainContent.addAndMakeVisible(generatedDrumGrid);

    // "Export Drum Stems" (Source/DrumStemExporter.h) - offline, per-role
    // WAV render of exactly what's currently generated, no regeneration.
    exportDrumStemsButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    exportDrumStemsButton.setColour(juce::TextButton::textColourOffId, kAccent);
    exportDrumStemsButton.setEnabled(false); // enabled once currentDrumGroove exists - see refreshLoopDisplay()
    exportDrumStemsButton.onClick = [this] { exportDrumStemsClicked(); };
    mainContent.addAndMakeVisible(exportDrumStemsButton);

    exportDrumStemsStatusLabel.setFont(small());
    exportDrumStemsStatusLabel.setJustificationType(juce::Justification::topLeft);
    exportDrumStemsStatusLabel.setColour(juce::Label::textColourId, kTextDim);
    exportDrumStemsStatusLabel.setText("Generate a loop first.", juce::dontSendNotification);
    mainContent.addAndMakeVisible(exportDrumStemsStatusLabel);

    // Per-role mute row (Part 10) - a MIX control, independent of
    // generation. See PluginProcessor::setGeneratedDrumRoleMuted; role
    // order matches Engine::DrumRole (Kick, Clap, HatClosed, HatOpen,
    // PercA, PercB).
    {
        static const char* const kRoleNames[6] = { "Kick", "Clap", "Closed Hat", "Open Hat", "Perc A", "Perc B" };
        for (int r = 0; r < 6; ++r)
        {
            auto& ctrl = drumRoleMutes[(size_t) r];
            ctrl.roleLabel.setText(kRoleNames[r], juce::dontSendNotification);
            ctrl.roleLabel.setFont(small());
            ctrl.roleLabel.setColour(juce::Label::textColourId, kTextDim);
            mainContent.addAndMakeVisible(ctrl.roleLabel);

            ctrl.muteButton.setClickingTogglesState(true);
            ctrl.muteButton.setColour(juce::TextButton::buttonColourId,   kPanel);
            ctrl.muteButton.setColour(juce::TextButton::buttonOnColourId, UIStyle::kMuteActive);
            ctrl.muteButton.onClick = [this, r]
            {
                processor.setGeneratedDrumRoleMuted(r, drumRoleMutes[(size_t) r].muteButton.getToggleState());
            };
            mainContent.addAndMakeVisible(ctrl.muteButton);
        }
    }

    // Per-voice mute row (Bass/Melody/Pad) - same independent, playback-
    // only mute contract as the drum-role row above, calling
    // PluginProcessor::setMelodyTrackMuted instead of
    // setGeneratedDrumRoleMuted. Track order matches the loop-generator's
    // own fixed convention (Bass=0, Melody=1, Pad=2).
    {
        static const char* const kVoiceNames[3] = { "Bass", "Melody", "Pad" };
        for (int v = 0; v < 3; ++v)
        {
            auto& ctrl = voiceMutes[(size_t) v];
            ctrl.roleLabel.setText(kVoiceNames[v], juce::dontSendNotification);
            ctrl.roleLabel.setFont(small());
            ctrl.roleLabel.setColour(juce::Label::textColourId, kTextDim);
            mainContent.addAndMakeVisible(ctrl.roleLabel);

            ctrl.muteButton.setClickingTogglesState(true);
            ctrl.muteButton.setColour(juce::TextButton::buttonColourId,   kPanel);
            ctrl.muteButton.setColour(juce::TextButton::buttonOnColourId, UIStyle::kMuteActive);
            ctrl.muteButton.onClick = [this, v]
            {
                processor.setMelodyTrackMuted(v, voiceMutes[(size_t) v].muteButton.getToggleState());
            };
            mainContent.addAndMakeVisible(ctrl.muteButton);
        }
    }

    // loopLengthLabel stays a direct child of the editor (not mainContent)
    // - it moves into the fixed header row next to Generate, see
    // resized() - always reachable without scrolling.
    loopLengthLabel.setFont(small());
    loopLengthLabel.setColour(juce::Label::textColourId, kTextDim);
    loopLengthLabel.setText(juce::String("Loop: ") + juce::String(Engine::kGrooveLoopBars) + " bars",
                             juce::dontSendNotification);
    addAndMakeVisible(loopLengthLabel);

    // Reference-track controls - Studio, not Advisor (see PluginEditor.h).
    loadReferenceButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    loadReferenceButton.setColour(juce::TextButton::textColourOffId, kAccent);
    loadReferenceButton.onClick = [this] { loadReferenceTrackClicked(); };
    addAndMakeVisible(loadReferenceButton);

    clearReferenceButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    clearReferenceButton.setColour(juce::TextButton::textColourOffId, kTextDim);
    clearReferenceButton.onClick = [this] { clearReferenceTrack(); };
    clearReferenceButton.setVisible(false);
    addAndMakeVisible(clearReferenceButton);

    referenceStatusLabel.setFont(body());
    referenceStatusLabel.setColour(juce::Label::textColourId, kTextDim);
    referenceStatusLabel.setText("No reference track loaded", juce::dontSendNotification);
    addAndMakeVisible(referenceStatusLabel);

    extractPatternsButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    extractPatternsButton.setColour(juce::TextButton::textColourOffId, kAccent);
    extractPatternsButton.onClick = [this] { extractPatternsClicked(); };
    extractPatternsButton.setVisible(false);
    addAndMakeVisible(extractPatternsButton);

    applyReferenceDrumsButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    applyReferenceDrumsButton.setColour(juce::TextButton::textColourOffId, kGood);
    applyReferenceDrumsButton.onClick = [this] { applyReferenceDrumsClicked(); };
    applyReferenceDrumsButton.setVisible(false);
    addAndMakeVisible(applyReferenceDrumsButton);

    extractPatternsStatusLabel.setFont(body());
    extractPatternsStatusLabel.setColour(juce::Label::textColourId, kTextDim);
    extractPatternsStatusLabel.setVisible(false);
    addAndMakeVisible(extractPatternsStatusLabel);

    // Reference-track feature hidden while the deterministic engine
    // rebuild is in progress (see kShowExperimentalFeatures) - clear/
    // extract/apply stay individually gated by their own state (hasResult
    // etc.) on TOP of this, so simply reasserting false here is enough;
    // they never get re-shown by later state changes while the flag is off.
    loadReferenceButton.setVisible(kShowExperimentalFeatures);
    referenceStatusLabel.setVisible(kShowExperimentalFeatures);

    // Everything below the fixed header/Generate row scrolls as one unit —
    // set up the outer viewport before adding any of its children.
    // Always visible now (previously hidden unless kShowFullUI, back when
    // it wrapped only the hidden manual-grid subtree - it now wraps the
    // real, always-visible Studio content too, so it must always be
    // visible; setShowingAdvisor still hides it while the Advisor tab is
    // showing, see mainViewport.setVisible(!showAdvisor) below).
    mainViewport.setViewedComponent(&mainContent, false);
    mainViewport.setScrollBarsShown(true, false); // vertical only
    addAndMakeVisible(mainViewport);
    // Manual drum grid, melody grid, and Serum 2 track panels still live
    // inside mainContent - individually hidden for the drum-generation
    // milestone (see kShowFullUI). Nothing underneath is disconnected:
    // onStepToggled/onSampleCycled/etc. still push to the processor
    // exactly as before, and the background library/preset scans below
    // still populate drumMachine's rows - there's just no visible surface
    // to see or interact with them on right now.

    mainContent.addAndMakeVisible(drumMachine);

    drumMachine.onStepToggled = [this](int rowIndex, int step, bool isOn)
    {
        processor.setDrumRowStep(rowIndex, step, isOn);
    };
    drumMachine.onSampleCycled = [this](int rowIndex)
    {
        processor.setDrumRowSample(rowIndex, drumMachine.getSampleForRow(rowIndex));
    };
    drumMachine.onMuteToggled = [this](int rowIndex, bool muted)
    {
        processor.setDrumRowMuted(rowIndex, muted);
    };
    drumMachine.onSoloToggled = [this](int rowIndex, bool solo)
    {
        processor.setDrumRowSolo(rowIndex, solo);
    };
    mainContent.addAndMakeVisible(melodyGrid);

    melodyGrid.onTrackChanged = [this](int)
    {
        exportMelodyPattern();
    };
    melodyGrid.onTrackMuteToggled = [this](int trackIndex, bool muted)
    {
        processor.setMelodyTrackMuted(trackIndex, muted);
    };
    melodyGrid.onTrackSoloToggled = [this](int trackIndex, bool solo)
    {
        processor.setMelodyTrackSolo(trackIndex, solo);
    };
    melodyGrid.onTrackPresetCycle = [this](int trackIndex, int dir)
    {
        if (trackIndex >= 0 && trackIndex < melodyPanels.size())
            cyclePresetForTrack(*melodyPanels[trackIndex], dir);
    };

    serumTracksHeadingLabel.setText("SERUM 2 TRACKS", juce::dontSendNotification);
    serumTracksHeadingLabel.setColour(juce::Label::textColourId, kTextDim);
    serumTracksHeadingLabel.setFont(heading());
    mainContent.addAndMakeVisible(serumTracksHeadingLabel);

    addMelodyTrackButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    addMelodyTrackButton.setColour(juce::TextButton::textColourOffId, kAccent);
    addMelodyTrackButton.onClick = [this] { addMelodyTrackClicked(); };
    mainContent.addAndMakeVisible(addMelodyTrackButton);

    // Track 0 — always present ("Bass" by default; Generate re-detects the
    // category from whatever prompt is typed). The processor's own
    // constructor already allocated this slot (so audio works even if the
    // editor UI is never opened, same as the old single-instance behaviour)
    // — reference it directly rather than calling addMelodyTrack() again,
    // which would allocate a second, orphaned slot.
    {
        const int trackIndex = 0;
        melodyGrid.addTrack(categoryDisplayName(MelodyCategory::Bass), categoryColour(MelodyCategory::Bass));
        processor.setMelodyTrackCategory(trackIndex, MelodyCategory::Bass);
        auto* panel = melodyPanels.add(new MelodyTrackPanel(*this, trackIndex));
        mainContent.addAndMakeVisible(panel);
        scanPresetsForTrack(*panel);
        updateTrackTitle(*panel);
    }

    // Track 1 — always present ("Melody" by default, Lead category) for
    // the loop-generator workflow's third simultaneous voice (see
    // generateLoopClicked()). Same "reference the processor's own
    // already-allocated slot directly" pattern as track 0 above - the
    // processor constructor already called addMelodyTrack() a second time
    // for this. Reuses the existing MelodyTrackPanel/Capture/preset-cycle
    // machinery unchanged, so capturing a real melodic-techno lead sound
    // for this track works exactly the same way it already does for Bass.
    {
        const int trackIndex = 1;
        melodyGrid.addTrack(categoryDisplayName(MelodyCategory::Lead), categoryColour(MelodyCategory::Lead));
        auto* panel = melodyPanels.add(new MelodyTrackPanel(*this, trackIndex));
        panel->category = MelodyCategory::Lead;
        processor.setMelodyTrackCategory(trackIndex, MelodyCategory::Lead);
        mainContent.addAndMakeVisible(panel);
        scanPresetsForTrack(*panel);
        updateTrackTitle(*panel);
    }

    // Track 2 — always present (Pad category) for Engine::BreakdownArrangement's
    // dedicated breakdown melodic/harmonic material (see generateCompactLoop()
    // in Source/Engine/BreakdownArrangement.h) - the single highest-leverage
    // missing capability the reference research identified: previously the
    // "breakdown" was only ever the drop with roles muted, never a real new
    // musical presence. Same pattern as tracks 0/1 - references the
    // processor's own already-allocated slot (third addMelodyTrack() call
    // in the processor constructor) rather than allocating a new one here.
    {
        const int trackIndex = 2;
        melodyGrid.addTrack(categoryDisplayName(MelodyCategory::Pad), categoryColour(MelodyCategory::Pad));
        auto* panel = melodyPanels.add(new MelodyTrackPanel(*this, trackIndex));
        panel->category = MelodyCategory::Pad;
        processor.setMelodyTrackCategory(trackIndex, MelodyCategory::Pad);
        mainContent.addAndMakeVisible(panel);
        scanPresetsForTrack(*panel);
        updateTrackTitle(*panel);
    }

    rackBrowser.onFilesDiscovered = [this](int count)
    {
        rackFilesDiscovered = count;
        updateLibraryScanStatusLabel();
    };

    rackBrowser.onRacksChanged = [this](const std::vector<Rack>& racks)
    {
        latestRacks = racks;
        drumMachine.refreshFromRacks(racks);
        pushAllDrumRowsToProcessor();
        exportMelodyPattern();
        resized();

        rackScanCompleted   = true;
        onRacksChangedFired = true;

        // Re-analyze the drum-role samples in the background whenever the
        // library rescans (a fresh library path, or samples added/removed/
        // changed on disk) - never on the message/audio thread, and the
        // disk cache (Source/DrumSampleIndex.h) means unchanged files
        // aren't re-decoded.
        sampleIndexScanStarted = true;
        sampleIndex.analyzeRacks(racks);
        updateLibraryScanStatusLabel();
    };

    sampleIndex.onIndexReady = [this](const std::vector<IndexedSample>& indexed)
    {
        latestSampleIndex = indexed;
        sampleIndexReady  = true;
        onIndexReadyFired = true;

        // Candidate counts per role, mirroring DrumSampleSelector's own
        // role->rack mapping exactly (clap draws from CLAP+SNARE) - a
        // direct, real count of what the selector will actually see,
        // not a guess.
        kickCandidateCount = clapCandidateCount = hatCandidateCount = percCandidateCount = 0;
        for (auto& s : indexed)
        {
            if (!s.features.valid)
                continue;
            if (s.rackId == "KICK")                        ++kickCandidateCount;
            if (s.rackId == "CLAP" || s.rackId == "SNARE")  ++clapCandidateCount;
            if (s.rackId == "HIHAT" || s.rackId == "OPEN_HAT") ++hatCandidateCount;
            if (s.rackId == "PERC")                         ++percCandidateCount;
        }

        updateGenerateButtonAvailability();
        updateLibraryScanStatusLabel();
    };

    // Studio / Advisor tab strip
    studioTabButton.setClickingTogglesState(false);
    advisorTabButton.setClickingTogglesState(false);
    studioTabButton.onClick  = [this] { setShowingAdvisor(false); };
    advisorTabButton.onClick = [this] { setShowingAdvisor(true); };
    // Advisor is hidden while the deterministic engine rebuild is in
    // progress - Studio is the only reachable surface, so the tab strip
    // itself (there's nothing left to switch between) is hidden too.
    studioTabButton.setVisible(kShowExperimentalFeatures);
    advisorTabButton.setVisible(kShowExperimentalFeatures);
    addAndMakeVisible(studioTabButton);
    addAndMakeVisible(advisorTabButton);

    addChildComponent(advisorPanel); // hidden until the Advisor tab is picked
    advisorPanel.onAnalyzeClicked = [this] { runTheoryAdvisorAnalysis(); };
    advisorPanel.onSuggestMelodyEditsClicked = [this] { buildMelodyEditSuggestions(); };
    advisorPanel.onArrangementClicked = [this] { buildArrangementSuggestions(); };
    advisorPanel.onPreviewStarted = [this] { beginPreview(); };
    advisorPanel.onPreviewEnded   = [this] { endPreview(); };
    setShowingAdvisor(false);

    pushAllDrumRowsToProcessor(); // starts empty until a library is scanned
    exportMelodyPattern();

    presetScanner.onPresetsChanged = [this](const std::vector<PresetEntry>& presets)
    {
        latestPresets = presets;
    };

    // Kick off the (already background-threaded, see RackBrowserComponent::
    // run()/PresetLibraryScanner::run()) sample-library scans on the next
    // message-loop iteration rather than inline here - the grid/UI this
    // constructor is building should be fully constructed and handed back
    // to the host before we even start the (tiny, but non-zero)
    // std::thread-creation work each setLibraryDir() call does. Not a
    // timer - a single deferred call, not a repeating one used to paper
    // over anything.
    if (libraryDir.isDirectory())
    {
        juce::Component::SafePointer<AbletonCopilotAudioProcessorEditor> safeThis(this);
        juce::MessageManager::callAsync([safeThis]
        {
            if (auto* self = safeThis.getComponent())
            {
                StartupTiming::mark("Library scan kickoff (deferred)");
                self->rackScanStarted       = true;
                self->scanPipelineStartSecs = juce::Time::getMillisecondCounterHiRes() * 0.001;
                self->updateLibraryScanStatusLabel();
                self->rackBrowser.setLibraryDir(self->libraryDir);
                self->presetScanner.setLibraryDir(self->libraryDir);
            }
        });
    }

    startTimerHz(30);
    StartupTiming::mark("Editor ctor end");
}

AbletonCopilotAudioProcessorEditor::~AbletonCopilotAudioProcessorEditor()
{
    // AudioProcessor only holds a non-owning raw pointer to its active
    // editor (see activeEditor in juce_AudioProcessor.h) and expects whoever
    // owns/destroys the editor to call editorBeingDeleted() first, or it's
    // left dangling. Each panel's serumWindow owns that track's Serum2
    // editor component, so tear them all down explicitly here rather than
    // relying on member destruction order.
    for (auto* panel : melodyPanels)
    {
        if (panel->serumWindow == nullptr)
            continue;

        if (auto* serum = processor.getHostedSerumInstance(panel->trackIndex))
            if (auto* activeEd = serum->getActiveEditor())
                serum->editorBeingDeleted(activeEd);
        panel->serumWindow.reset();
    }
}

//==============================================================================

void AbletonCopilotAudioProcessorEditor::resized()
{
    static bool loggedFirstResized = false;
    if (!loggedFirstResized)
    {
        loggedFirstResized = true;
        StartupTiming::mark("Editor first resized()");
    }

    auto area = getLocalBounds();

    // Header: title on the left, a compact sample-library path on the right.
    auto headerArea  = area.removeFromTop(48);
    auto headerInner = headerArea.reduced(14, 0);
    headerInner.removeFromLeft(170); // room for the "Ableton Copilot" title
    chooseLibraryButton.setBounds(headerInner.removeFromRight(64));
    headerInner.removeFromRight(6);
    libraryPathLabel.setBounds(headerInner);

    if (kShowAnalyzer)
    {
        // Genre picker + analyze button row
        auto ctrlRow = area.removeFromTop(30).reduced(12, 0);
        genrePicker.setBounds(ctrlRow.removeFromLeft(300));
        ctrlRow.removeFromLeft(8);
        analyzeNowButton.setBounds(ctrlRow.removeFromLeft(120));

        area.removeFromTop(4);

        // Apply / bypass button (full width)
        applyButton.setBounds(area.removeFromTop(28).reduced(12, 0));

        area.removeFromTop(6);

        // Status label
        statusLabel.setBounds(area.removeFromTop(18).reduced(12, 0));
        area.removeFromTop(6);

        // Transport bar (painted)
        area.removeFromTop(28);
        area.removeFromTop(6);

        // Live meters (painted)
        area.removeFromTop(48);
        area.removeFromTop(10);
    }

    area.removeFromTop(12);

    // Studio / Advisor tab strip — fixed, above whichever view is showing.
    // Hidden entirely while kShowExperimentalFeatures is off (see its
    // declaration) - Studio is the only reachable surface, no strip needed.
    if (kShowExperimentalFeatures)
    {
        auto tabRow = area.removeFromTop(28).reduced(12, 0);
        studioTabButton.setBounds(tabRow.removeFromLeft(90));
        tabRow.removeFromLeft(6);
        advisorTabButton.setBounds(tabRow.removeFromLeft(90));
        area.removeFromTop(8);
    }

    advisorPanel.setBounds(area);

    if (showingAdvisor)
        return;

    // Genre + key selector (shared across every melody track) - melody/
    // manual-grid infrastructure, not part of the drum-generation focus,
    // hidden alongside the kShowFullUI subtree below.
    if (kShowFullUI)
    {
        auto genreRow = area.removeFromTop(26).reduced(12, 0);
        drumGenrePicker.setBounds(genreRow.removeFromLeft(160));
        genreRow.removeFromLeft(8);
        keyPicker.setBounds(genreRow.removeFromLeft(160));
        genreRow.removeFromLeft(8);
        genreEqToggle.setBounds(genreRow.removeFromLeft(110));
        area.removeFromTop(12);
    }

    // HEADER's Generate row (goal: Generate always reachable without
    // scrolling) - the loop-generator's one unified Generate button (no
    // separate DROP/BREAKDOWN mode buttons anymore - the compact loop
    // always plays the whole arc) plus the loop-length label, both moved
    // up from the scrollable body into this fixed strip.
    auto generateRow = area.removeFromTop(34).reduced(12, 0);
    generateLoopButton.setBounds(generateRow.removeFromLeft(140));
    generateRow.removeFromLeft(10);
    loopLengthLabel.setBounds(generateRow.removeFromLeft(110));
    area.removeFromTop(8);

    // Reference-track row(s) - hidden while kShowExperimentalFeatures is
    // off (same reasoning as the tab strip above). Left exactly as-is -
    // out of scope for this UI cleanup, still dead/invisible.
    if (kShowExperimentalFeatures)
    {
    auto refRow = area.removeFromTop(26).reduced(12, 0);
    loadReferenceButton.setBounds(refRow.removeFromLeft(160));
    refRow.removeFromLeft(8);
    if (clearReferenceButton.isVisible())
    {
        clearReferenceButton.setBounds(refRow.removeFromLeft(60));
        refRow.removeFromLeft(8);
    }
    referenceStatusLabel.setBounds(refRow);
    area.removeFromTop(6);

    if (extractPatternsButton.isVisible())
    {
        auto extractRow = area.removeFromTop(26).reduced(12, 0);
        extractPatternsButton.setBounds(extractRow.removeFromLeft(260));
        if (applyReferenceDrumsButton.isVisible())
        {
            extractRow.removeFromLeft(8);
            applyReferenceDrumsButton.setBounds(extractRow.removeFromLeft(180));
        }
        area.removeFromTop(4);

        if (extractPatternsStatusLabel.isVisible())
        {
            extractPatternsStatusLabel.setBounds(area.removeFromTop(20).reduced(12, 0));
            area.removeFromTop(4);
        }
    }
    area.removeFromTop(6);
    } // kShowExperimentalFeatures

    // ---- Everything else - mute rows, the compact STATUS summary, the
    // PATTERN grid (main visual focus), the detailed diagnostics, and -
    // when kShowFullUI is on - the manual drum/melody grid and Serum 2
    // panels - scrolls as ONE unit inside mainViewport/mainContent. This
    // is the fix for the vertical-overflow problem: previously this
    // content had no scroll container at all (mainViewport wrapped only
    // the kShowFullUI subtree, which is hidden by default), so the
    // window's own fixed height had to be tall enough to show literally
    // everything unscrolled. mainContent is now sized every call to its
    // real summed content height (never a guess), so mainViewport's
    // scrollbar appears only when content genuinely exceeds whatever
    // height the host/user has given the window. ----
    mainViewport.setBounds(area);
    const int contentWidth = juce::jmax(200, area.getWidth() - mainViewport.getScrollBarThickness());
    int y = 8;

    // Per-role mute row - one compact column per role.
    {
        auto muteRow = juce::Rectangle<int>(0, y, contentWidth, 48).reduced(12, 0);
        const int colWidth = muteRow.getWidth() / 6;
        for (auto& ctrl : drumRoleMutes)
        {
            auto col = muteRow.removeFromLeft(colWidth);
            ctrl.roleLabel.setBounds(col.removeFromTop(20));
            ctrl.muteButton.setBounds(col.removeFromTop(24).withWidth(40));
        }
    }
    y += 48 + 4;

    // Per-voice mute row (Bass/Melody/Pad) - same compact-column layout,
    // narrower row since there are only 3.
    {
        auto voiceMuteRow = juce::Rectangle<int>(0, y, contentWidth, 48).reduced(12, 0);
        const int colWidth = voiceMuteRow.getWidth() / 3;
        for (auto& ctrl : voiceMutes)
        {
            auto col = voiceMuteRow.removeFromLeft(colWidth);
            ctrl.roleLabel.setBounds(col.removeFromTop(20));
            ctrl.muteButton.setBounds(col.removeFromTop(24).withWidth(40));
        }
    }
    y += 48 + 8;

    // STATUS - the compact DRUMS / BASS+MELODY+PAD / SECTION block (see
    // RuntimeStatusText.h) - 3 dense lines, not the ~23-line block this
    // used to be; the 4 separate Serum/MIDI-range labels it duplicated
    // are retired (same information, no longer shown twice).
    {
        constexpr int kStatusHeight = 58; // ~3 lines at small() + padding
        structuredStatusLabel.setBounds(juce::Rectangle<int>(0, y, contentWidth, kStatusHeight).reduced(12, 0));
        y += kStatusHeight + 8;
    }

    // PATTERN - the main visual focus (goal 4), right after STATUS -
    // previously buried below two more diagnostic blocks. Sized to its
    // own real (dynamic, 9-row) content height; its own internal
    // horizontal-only viewport still handles the 256-step width
    // independently of this outer vertical scroll (a legitimately
    // necessary nested viewport - see GeneratedDrumGridComponent).
    {
        const int gridWidth = juce::jmax(1, contentWidth - 24);
        generatedDrumGrid.setBounds(12, y, gridWidth, generatedDrumGrid.getRequiredHeight());
        y += generatedDrumGrid.getHeight() + 10;
    }

    // Export Drum Stems - button + status, directly below the pattern
    // grid it exports (no regeneration involved, purely a render of what's
    // already shown above).
    {
        auto exportRow = juce::Rectangle<int>(0, y, contentWidth, 28).reduced(12, 0);
        exportDrumStemsButton.setBounds(exportRow.removeFromLeft(160));
        y += 28 + 4;

        constexpr int kExportStatusHeight = 96; // room for the 6 role lines + Length/BPM/Sample Rate + Exported confirmation
        exportDrumStemsStatusLabel.setBounds(juce::Rectangle<int>(0, y, contentWidth, kExportStatusHeight).reduced(12, 0));
        y += kExportStatusHeight + 8;
    }

    // Detailed diagnostics - kept, not deleted (goal 9), just below the
    // grid now instead of above it - lower-priority developer detail
    // than the compact STATUS summary, still fully reachable by
    // scrolling, same content as before.
    {
        constexpr int kLibraryScanHeight = 90;
        libraryScanStatusLabel.setBounds(juce::Rectangle<int>(0, y, contentWidth, kLibraryScanHeight).reduced(12, 0));
        y += kLibraryScanHeight + 4;

        constexpr int kDrumPatternHeight = 150;
        drumPatternStatusLabel.setBounds(juce::Rectangle<int>(0, y, contentWidth, kDrumPatternHeight).reduced(12, 0));
        y += kDrumPatternHeight + 8;
    }

    // The old manual drum grid/melody grid - still hidden behind
    // kShowFullUI, unchanged content/sizing logic, not requested by this
    // pass, not touched.
    if (kShowFullUI)
    {
        const int innerWidth = contentWidth - 24;

        const int drumGridHeight = DrumMachineComponent::kRowHeight * drumMachine.getNumRows();
        drumMachine.setBounds(12, y, innerWidth, drumGridHeight);
        y += drumGridHeight + 16;

        const int melodyGridHeight = MelodyGridComponent::kRowHeight * melodyGrid.getNumTracks();
        melodyGrid.setBounds(12, y, innerWidth, melodyGridHeight);
        y += melodyGridHeight + 16;
    }

    // Per-track Serum2 panels (Capture/Open Serum2/preset-cycle/status,
    // one per Bass/Melody/Pad track) - ALWAYS visible now, no longer
    // gated behind kShowFullUI. This was a real bug: a Component never
    // given real bounds stays at its default zero size (not clickable,
    // not visible), so the Capture workflow - the ONLY way to get a real
    // captured Serum2 preset playing instead of the factory Init patch -
    // was completely unreachable in the shipped UI. See the plan this was
    // built from for the full investigation. Independent from the
    // kShowFullUI-gated manual grids above - this is a targeted
    // visibility fix, not a UI redesign.
    {
        const int innerWidth = contentWidth - 24;

        serumTracksHeadingLabel.setBounds(12, y, innerWidth, 22);
        y += 22 + 6;

        for (auto* panel : melodyPanels)
        {
            panel->setBounds(12, y, innerWidth, MelodyTrackPanel::kHeight);
            y += MelodyTrackPanel::kHeight + 8;
        }
        y += 8;

        addMelodyTrackButton.setBounds(12, y, 220, 26);
        y += 26 + 20;
    }

    mainContent.setSize(contentWidth, y);
}

//==============================================================================

void AbletonCopilotAudioProcessorEditor::setShowingAdvisor(bool showAdvisor)
{
    showingAdvisor = showAdvisor;
    updatePlaybackSuppression();

    advisorPanel.setVisible(showAdvisor);

    mainViewport.setVisible(!showAdvisor);
    drumGenrePicker.setVisible(!showAdvisor);
    keyPicker.setVisible(!showAdvisor);
    genreEqToggle.setVisible(!showAdvisor);

    loadReferenceButton.setVisible(!showAdvisor);
    clearReferenceButton.setVisible(!showAdvisor && loadedReference.has_value());
    referenceStatusLabel.setVisible(!showAdvisor);
    extractPatternsButton.setVisible(!showAdvisor && loadedReference.has_value());
    applyReferenceDrumsButton.setVisible(!showAdvisor && hasReferenceDrumPatterns);
    extractPatternsStatusLabel.setVisible(!showAdvisor && extractPatternsStatusLabel.getText().isNotEmpty());

    resized();
    repaint();
}

void AbletonCopilotAudioProcessorEditor::updatePlaybackSuppression()
{
    // Suppress our own drum/melody output while listening on the Advisor
    // tab (so audio analysis doesn't hear itself) - EXCEPT while a
    // suggestion preview is active, since the whole point of previewing is
    // to actually hear the change.
    processor.setSuppressOwnPlayback(showingAdvisor && activePreviewCount == 0);
}

void AbletonCopilotAudioProcessorEditor::beginPreview()
{
    ++activePreviewCount;
    updatePlaybackSuppression();
}

void AbletonCopilotAudioProcessorEditor::endPreview()
{
    jassert(activePreviewCount > 0);
    activePreviewCount = juce::jmax(0, activePreviewCount - 1);
    updatePlaybackSuppression();
}

static FeedbackItem feedbackItemForMelodyEdit(const MelodyEditSuggestion& s)
{
    FeedbackItem item;
    item.severity = Severity::Warning;
    item.category = s.trackLabel + " - STEP " + juce::String(s.step);
    item.headline = "Change " + juce::String((int) s.oldOffset) + " -> " + juce::String((int) s.newOffset)
                    + " semitones from root";
    item.detail   = s.reason;
    return item;
}

void AbletonCopilotAudioProcessorEditor::runTheoryAdvisorAnalysis()
{
    if (processor.getAnalyzer().isAnalyzing())
        return;

    int secs = processor.getAnalyzer().bufferedSeconds();
    if (secs < 5)
    {
        advisorPanel.setStatus("Need at least 5 seconds of audio (captured " +
                                juce::String(secs) + "s). Play your track first.");
        return;
    }

    advisorPanel.setBusy(true);

    float dawBpm = processor.currentBpm.load(std::memory_order_relaxed);
    processor.getAnalyzer().triggerAnalysis("melodic_techno", dawBpm, [this](const AnalysisResult& r)
    {
        advisorPanel.setBusy(false);

        if (!r.valid)
        {
            advisorPanel.setStatus("Analysis failed - capture more audio and try again.");
            return;
        }

        auto [keyRoot, isMinor] = getSelectedKey();

        AdvisorContext ctx;
        ctx.features        = r.features;
        ctx.racks           = latestRacks;
        ctx.presets         = latestPresets;
        ctx.keyRootSemitone = keyRoot;
        ctx.isMinor          = isMinor;
        ctx.section          = advisorPanel.getSelectedSection();
        ctx.profileId        = loadedReference.has_value() ? kReferenceProfileId
                                                             : juce::String("melodic_techno");

        for (int t = 0; t < melodyGrid.getNumTracks(); ++t)
        {
            MelodyTrackContext track;
            track.trackIndex = t;
            track.label      = (t < melodyPanels.size()) ? categoryDisplayName(melodyPanels[t]->category)
                                                           : juce::String("TRACK ") + juce::String(t);
            track.category   = (t < melodyPanels.size()) ? melodyPanels[t]->category : MelodyCategory::Bass;
            track.offsets    = melodyGrid.getTrackOffsets(t);
            ctx.tracks.push_back(std::move(track));
        }

        AdvisorResult result = theoryAdvisor.generate(ctx);

        juce::String keyBpmText = (r.features.key.isNotEmpty() ? r.features.key : juce::String("Key: unknown"))
                                 + "  |  "
                                 + (r.features.bpm > 0.0f ? juce::String(r.features.bpm, 1) + " BPM"
                                                           : juce::String("BPM n/a"));
        advisorPanel.setResult(keyBpmText, result.informational);

        std::vector<AdvisorPanelComponent::ActionableFeedback> tweaks;

        for (auto& fix : result.clashFixes)
        {
            AdvisorPanelComponent::ActionableFeedback card;
            card.item = feedbackItemForMelodyEdit(fix);
            card.applyChange = [this, trackIndex = fix.trackIndex, step = fix.step, newOffset = fix.newOffset]
            {
                melodyGrid.setStepOffset(trackIndex, step, newOffset);
            };
            card.revertChange = [this, trackIndex = fix.trackIndex, step = fix.step, oldOffset = fix.oldOffset]
            {
                melodyGrid.setStepOffset(trackIndex, step, oldOffset);
            };
            tweaks.push_back(std::move(card));
        }

        if (result.keyChange)
        {
            auto kc = *result.keyChange;
            const int prevId = keyPicker.getSelectedId();
            FeedbackItem item;
            item.severity = Severity::Warning;
            item.category = "KEY";
            item.headline = "Set key picker to detected key";
            AdvisorPanelComponent::ActionableFeedback card;
            card.item = item;
            card.applyChange = [this, kc]
            {
                // keyPicker items: id = root*2 + (isMinor?2:1), see the
                // constructor's noteNames loop (major then minor per root).
                const int id = kc.rootSemitone * 2 + (kc.isMinor ? 2 : 1);
                keyPicker.setSelectedId(id, juce::sendNotification);
            };
            card.revertChange = [this, prevId]
            {
                keyPicker.setSelectedId(prevId, juce::sendNotification);
            };
            tweaks.push_back(std::move(card));
        }

        for (auto& sa : result.sampleAssigns)
        {
            const juce::File prevFile = drumMachine.getSampleForRackId(sa.rackId);
            FeedbackItem item;
            item.severity = Severity::Good;
            item.category = "SAMPLE";
            item.headline = "Assign " + sa.file.getFileNameWithoutExtension() + " to " + sa.rackId;
            AdvisorPanelComponent::ActionableFeedback card;
            card.item = item;
            card.applyChange = [this, sa]
            {
                drumMachine.setRowSampleByFile(sa.rackId, sa.file);
            };
            card.revertChange = [this, rackId = sa.rackId, prevFile]
            {
                if (prevFile.existsAsFile())
                    drumMachine.setRowSampleByFile(rackId, prevFile);
            };
            tweaks.push_back(std::move(card));
        }

        lastAdvisorTweaks = std::move(tweaks);
        refreshAnalysisTweakCards();
    });
}

void AbletonCopilotAudioProcessorEditor::refreshAnalysisTweakCards()
{
    auto combined = lastAdvisorTweaks;
    combined.insert(combined.end(), pendingReferenceTweaks.begin(), pendingReferenceTweaks.end());
    advisorPanel.setAnalysisTweaks(combined);
}

void AbletonCopilotAudioProcessorEditor::setReferenceStatusText(const juce::String& text, bool hasReference)
{
    referenceStatusLabel.setText(text, juce::dontSendNotification);
    referenceStatusLabel.setColour(juce::Label::textColourId, hasReference ? kGood : kTextDim);
    clearReferenceButton.setVisible(hasReference);
    extractPatternsButton.setVisible(hasReference);
    if (!hasReference)
        extractPatternsStatusLabel.setVisible(false);
    resized();
}

void AbletonCopilotAudioProcessorEditor::setExtractPatternsStatusText(const juce::String& text, bool busy)
{
    extractPatternsButton.setEnabled(!busy);
    extractPatternsButton.setButtonText(busy ? "Extracting..." : "Extract Bass & Drum Patterns From This Track");
    extractPatternsStatusLabel.setText(text, juce::dontSendNotification);
    extractPatternsStatusLabel.setVisible(text.isNotEmpty());
    resized();
}

void AbletonCopilotAudioProcessorEditor::loadReferenceTrackClicked()
{
    if (referenceAnalyzer.isAnalyzing())
        return;

    referenceFileChooser = std::make_unique<juce::FileChooser>(
        "Choose a reference track", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");

    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    referenceFileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (!file.existsAsFile())
            return;

        setReferenceStatusText("Analyzing " + file.getFileNameWithoutExtension() + "...", false);

        referenceAnalyzer.analyzeFile(file, [this](const ReferenceAnalysisResult& result)
        {
            onReferenceAnalyzed(result);
        });
    });
}

void AbletonCopilotAudioProcessorEditor::onReferenceAnalyzed(const ReferenceAnalysisResult& result)
{
    if (!result.valid)
    {
        setReferenceStatusText("Couldn't analyze " + result.displayName + " - try a different file.", false);
        return;
    }

    loadedReference = result;

    auto profile = buildProfileFromReference(result.displayName, result.features);
    GenreProfiles::getInstance().setReferenceProfile(profile);

    juce::String bpmStr = result.features.bpm > 0.0f
                          ? juce::String(result.features.bpm, 1) + " BPM"
                          : juce::String("BPM n/a");
    juce::String summary = "Reference: " + result.displayName + "  |  " + result.features.key +
                            "  |  " + bpmStr + "  |  " + juce::String(result.features.lufs, 1) + " LUFS";
    setReferenceStatusText(summary, true);

    // Detected key becomes a real Approve action, same pattern as
    // TheoryAdvisor's own KeyChangeSuggestion card above - never silently
    // overwritten.
    pendingReferenceTweaks.clear();

    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F",
                                        "F#", "G", "G#", "A", "A#", "B" };
    int detectedRoot = -1;
    const bool detectedMinor = result.features.key.endsWithIgnoreCase("minor");
    for (int i = 0; i < 12; ++i)
    {
        const juce::String name = noteNames[i];
        // Boundary check (next char must be the space before "major"/"minor")
        // disambiguates "C" from "C#" - a plain startsWith would match both.
        if (result.features.key.startsWithIgnoreCase(name)
            && result.features.key.length() > name.length()
            && result.features.key[name.length()] == ' ')
        {
            detectedRoot = i;
            break;
        }
    }

    if (detectedRoot >= 0)
    {
        const int prevId = keyPicker.getSelectedId();
        FeedbackItem item;
        item.severity = Severity::Good;
        item.category = "REFERENCE";
        item.headline = "Set key picker to " + juce::String(noteNames[detectedRoot]) +
                         (detectedMinor ? " minor" : " major") + " (from reference track)";
        AdvisorPanelComponent::ActionableFeedback card;
        card.item = item;
        card.applyChange = [this, detectedRoot, detectedMinor]
        {
            const int id = detectedRoot * 2 + (detectedMinor ? 2 : 1);
            keyPicker.setSelectedId(id, juce::sendNotification);
        };
        card.revertChange = [this, prevId]
        {
            keyPicker.setSelectedId(prevId, juce::sendNotification);
        };
        pendingReferenceTweaks.push_back(std::move(card));
    }

    refreshAnalysisTweakCards();
}

void AbletonCopilotAudioProcessorEditor::clearReferenceTrack()
{
    loadedReference.reset();
    pendingReferenceTweaks.clear();
    hasReferenceDrumPatterns = false;
    pendingDrumPatterns.clear();
    GenreProfiles::getInstance().setReferenceProfile(std::nullopt);
    melodyGrid.clearReferenceFragments();
    applyReferenceDrumsButton.setVisible(false);
    setReferenceStatusText("No reference track loaded", false);
    setExtractPatternsStatusText("", false);
    refreshAnalysisTweakCards();
}

void AbletonCopilotAudioProcessorEditor::extractPatternsClicked()
{
    if (!loadedReference.has_value() || referenceFragmentJob.isRunning())
        return;

    setExtractPatternsStatusText("Extracting patterns - this can take a couple minutes (isolating bass + drums with "
                                  "Demucs, then transcribing/analyzing them)...", true);

    auto [keyRoot, isMinor] = getSelectedKey();
    const float bpm = loadedReference->features.bpm > 0.0f ? loadedReference->features.bpm : 126.0f;

    referenceFragmentJob.extract(loadedReference->sourceFile, bpm, keyRoot, isMinor,
        [this](const ReferenceFragmentResult& result)
        {
            if (!result.success)
            {
                setExtractPatternsStatusText("Extraction failed: " + result.errorMessage, false);
                return;
            }

            melodyGrid.loadReferenceFragments(result.outputJson);

            // Loading the fragments only fills the candidate pool - nothing
            // visibly changes until something actually generates from it.
            // Regenerate every Bass track right away so real notes show up
            // immediately instead of requiring a separate manual Generate
            // click the user has no reason to expect after "Extract".
            if (result.fragmentCount > 0)
                for (auto* panel : melodyPanels)
                    if (panel->category == MelodyCategory::Bass)
                        generateForTrack(*panel);

            pendingDrumPatterns = result.drumPatterns;
            hasReferenceDrumPatterns = !pendingDrumPatterns.empty();
            applyReferenceDrumsButton.setVisible(hasReferenceDrumPatterns);

            juce::String bassMsg = result.fragmentCount == 0
                ? juce::String("no usable bass patterns")
                : juce::String(result.fragmentCount) + " bass pattern(s)";

            juce::String drumMsg;
            if (hasReferenceDrumPatterns)
            {
                juce::StringArray roles;
                for (auto& p : pendingDrumPatterns) roles.add(p.rackId);
                drumMsg = " and drum hits for " + roles.joinIntoString(", ") +
                          " (approximated from frequency-band onset detection - click Apply Reference Drums to use them)";
            }
            else
            {
                drumMsg = " - no drum patterns detected";
            }

            setExtractPatternsStatusText("Extracted " + bassMsg + drumMsg + ".", false);
            resized();
        });
}

void AbletonCopilotAudioProcessorEditor::applyReferenceDrumsClicked()
{
    if (pendingDrumPatterns.empty())
        return;

    juce::StringArray applied, skipped;
    for (auto& p : pendingDrumPatterns)
    {
        if (drumMachine.applyReferenceDrumPattern(p.rackId, p.steps, p.sampleFile))
            applied.add(p.rackId);
        else
            skipped.add(p.rackId);
    }

    pushAllDrumRowsToProcessor();

    juce::String msg = "Applied reference drums to " + applied.joinIntoString(", ") + ".";
    if (!skipped.isEmpty())
        msg += " No row for " + skipped.joinIntoString(", ") + " in your library - skipped.";

    setExtractPatternsStatusText(msg, false);
}

void AbletonCopilotAudioProcessorEditor::updateGenerateButtonAvailability()
{
    if (!libraryDir.isDirectory())
    {
        generateLoopButton.setEnabled(false);
        generateLoopButton.setButtonText("No Sample Library");
    }
    else if (!sampleIndexReady)
    {
        generateLoopButton.setEnabled(false);
        generateLoopButton.setButtonText("Scanning Library...");
    }
    else
    {
        generateLoopButton.setEnabled(true);
        generateLoopButton.setButtonText("Generate");
    }
}

void AbletonCopilotAudioProcessorEditor::updateLibraryScanStatusLabel()
{
    juce::String s;
    s << "Library path: " << (libraryDir.isDirectory() ? libraryDir.getFullPathName() : juce::String("(none - not a valid directory)")) << "\n";
    s << "Scan started: " << (rackScanStarted ? "yes" : "no");
    if (rackScanStarted && !onIndexReadyFired)
    {
        const double elapsed = juce::Time::getMillisecondCounterHiRes() * 0.001 - scanPipelineStartSecs;
        s << " (" << juce::String(elapsed, 1) << "s ago - still running if this number is climbing)";
    }
    else if (onIndexReadyFired)
    {
        const double elapsed = juce::Time::getMillisecondCounterHiRes() * 0.001 - scanPipelineStartSecs;
        s << " (completed in " << juce::String(elapsed, 1) << "s)";
    }
    s << "\n";
    s << "Files discovered: " << (rackFilesDiscovered < 0 ? juce::String("pending") : juce::String(rackFilesDiscovered)) << "\n";
    s << "Audio files: " << (rackFilesDiscovered < 0 ? juce::String("pending") : juce::String(rackFilesDiscovered))
      << " (findChildFiles already filters to wav/aif/aiff/flac/mp3/ogg)\n";
    s << "Kick candidates: " << kickCandidateCount << "   Clap candidates: " << clapCandidateCount
      << "   Hat candidates: " << hatCandidateCount << "   Perc candidates: " << percCandidateCount << "\n";
    s << "Scan completed: " << (rackScanCompleted ? "yes" : "no") << "\n";
    s << "Completion callback: onRacksChanged=" << (onRacksChangedFired ? "fired" : "not fired")
      << ", onIndexReady=" << (onIndexReadyFired ? "fired" : "not fired") << "\n";

    // Rack-classification cache diagnostics (path+size+mtime-keyed, see
    // RackBrowserComponent::cacheFile) - the fix for the "reopening the
    // plugin rescans the whole library" issue. "Incremental" means every
    // file WAS in the cache but at least one had changed since; "Full
    // Scan" means the cache was empty (first run, or the cache file was
    // deleted); "Cached" means every single file was served from cache.
    if (rackScanCompleted)
    {
        const int cachedN  = rackBrowser.lastScanCachedFileCount();
        const int changedN = rackBrowser.lastScanChangedFileCount();
        const juce::String mode = rackBrowser.lastScanWasFullyCached() ? "Cached"
                                 : (cachedN > 0 ? "Incremental" : "Full Scan");
        s << "Library: " << mode << "  (Files: " << (cachedN + changedN)
          << "  Changed: " << changedN << ")";
    }

    libraryScanStatusLabel.setText(s, juce::dontSendNotification);
}

namespace
{
    // General MIDI drum map note numbers - a real, recognized convention
    // (Bass Drum 1, Hand Clap, Closed Hi-Hat, Open Hi-Hat, Side Stick,
    // Open Hi Conga) so this lines up with most drum racks/instruments by
    // default. Order matches Engine::DrumRole (Kick, Clap, HatClosed,
    // HatOpen, PercA, PercB). Shared by generateDrumsData() (which needs
    // just name/midiNote/steps) and refreshLoopDisplay() (which also
    // needs colour) - built fresh in both places from whatever
    // currentDrumGroove currently holds, never stored itself.
    struct DrumRoleExport { const char* name; int midiNote; juce::Colour colour; Engine::StepArray steps; };

    std::vector<DrumRoleExport> drumRoleExports(const Engine::DropPattern& drum)
    {
        return {
            { "KICK",       36, UIStyle::kKick,      drum.kick      },
            { "CLAP",       39, UIStyle::kClap,       drum.clap      },
            { "HAT_CLOSED", 42, UIStyle::kHihat,      drum.hatClosed },
            { "HAT_OPEN",   46, UIStyle::kHihatOpen,  drum.hatOpen   },
            { "PERC_A",     37, UIStyle::kPerc,       drum.percA     },
            { "PERC_B",     63, UIStyle::kPercAlt,    drum.percB     },
        };
    }

    const DrumSampleChoice& choiceForRoleName(const DrumSampleSelection& sel, const char* name)
    {
        if (juce::String(name) == "KICK")       return sel.kick;
        if (juce::String(name) == "CLAP")       return sel.clap;
        if (juce::String(name) == "HAT_CLOSED") return sel.hatClosed;
        if (juce::String(name) == "HAT_OPEN")   return sel.hatOpen;
        if (juce::String(name) == "PERC_A")     return sel.percA;
        return sel.percB;
    }

    // Bass/Melody rows - converted from their real offset arrays to a
    // simple active-step "velocity" (100 = a note is really there, 0 =
    // off) since this grid's cell shading is velocity-based and these
    // voices are monophonic pitched data, not drum hit strength - not a
    // claim about note dynamics, just a visibility convention.
    std::vector<int> offsetsToDisplayVelocity(const std::vector<int8_t>& offsets, int8_t offValue)
    {
        std::vector<int> v;
        v.reserve(offsets.size());
        for (auto o : offsets)
            v.push_back(o != offValue ? 100 : 0);
        return v;
    }
}

void AbletonCopilotAudioProcessorEditor::generateDrumsData()
{
    // Flat, non-arc, non-arrangement 8-bar drum groove (Engine::GrooveLoop.h,
    // now drum-ONLY - bass was split out, see that header's own comment
    // for why). Fresh seed every call, independent of bass/melody's own
    // seeds - this is what makes "Generate Drums doesn't overwrite bass/
    // melody" concretely true: this function never touches
    // currentBassMotif/currentMelodyMotif or calls
    // setGeneratedMelodyPattern at all.
    juce::Random rng;
    Engine::GrooveLoopParams grooveParams;
    grooveParams.seed = (uint32_t) rng.nextInt();
    currentDrumGroove = Engine::generateGrooveLoop(grooveParams);

    // Sample selection (Source/DrumSampleSelector.h): decides WHICH FILE
    // plays each drum role - genuinely part of "drum generation" (unlike
    // bass/melody, which never touch sample selection at all), so it
    // belongs here, stored into currentDrumSampleSelection (see that
    // member's own comment for why it must be stored, not recomputed by
    // refreshLoopDisplay on every bass/melody regeneration).
    const uint32_t sampleSeed = (uint32_t) rng.nextInt();
    currentDrumSampleSelection = selectDrumSamples(latestSampleIndex, sampleSeed, currentLoopBpm);
    logPercussionCandidateDiagnostics(latestSampleIndex, currentDrumSampleSelection, currentLoopBpm);

    std::vector<AbletonCopilotAudioProcessor::GeneratedDrumRole> processorRoles;
    for (auto& role : drumRoleExports(currentDrumGroove->drum))
    {
        AbletonCopilotAudioProcessor::GeneratedDrumRole pr;
        pr.midiNote   = role.midiNote;
        pr.velocity   = Engine::toVelocityArray(role.steps);
        pr.sampleFile = choiceForRoleName(currentDrumSampleSelection, role.name).file; // invalid File() -> PluginProcessor falls back to DrumVoiceSynth for this role
        processorRoles.push_back(std::move(pr));
    }
    processor.setGeneratedDrumPattern(processorRoles);
}

void AbletonCopilotAudioProcessorEditor::generateBassData()
{
    // Engine::generateBassPattern (BassEngine.h) - completely unmodified,
    // already a self-contained 8-bar/128-step generator with real
    // measured kick-avoidance. Fresh seed every call, independent of
    // drums/melody's own seeds. Never touches currentDrumGroove/
    // currentMelodyMotif or calls setGeneratedDrumPattern/track1's
    // setGeneratedMelodyPattern at all - this is what makes "Generate
    // Bass doesn't overwrite drums/melody" concretely true.
    juce::Random rng;
    Engine::BassPatternParams bassParams;
    bassParams.seed = (uint32_t) rng.nextInt();
    const auto bassPattern = Engine::generateBassPattern(bassParams);
    currentBassMotif.assign(bassPattern.begin(), bassPattern.end());

    const auto [keyRoot, isMinor] = getSelectedKey();
    juce::ignoreUnused(isMinor);
    processor.setGeneratedMelodyPattern(0, currentBassMotif, keyRoot); // track 0 = Bass
}

void AbletonCopilotAudioProcessorEditor::generateMelodyData()
{
    // MelodyMotifGenerator (MusicTheory/MelodyMotifGenerator.h) - the SAME
    // real motif generator this codebase already has, unmodified. Fresh
    // seed every call, independent of drums/bass's own seeds. Never
    // touches currentDrumGroove/currentBassMotif or track0's
    // setGeneratedMelodyPattern at all - this is what makes "Generate
    // Melody doesn't overwrite drums/bass" concretely true.
    juce::Random rng;
    const auto [keyRoot, isMinor] = getSelectedKey();
    const uint32_t seed = (uint32_t) rng.nextInt();
    const auto eightBarMelody = MelodyMotifGenerator::generateMelodyMotif(
        keyRoot, isMinor, MelodyCategory::Lead, MelodyStyle::Default, seed);
    currentMelodyMotif.assign(eightBarMelody.begin(), eightBarMelody.end());
    processor.setGeneratedMelodyPattern(1, currentMelodyMotif, keyRoot); // track 1 = Melody
}

void AbletonCopilotAudioProcessorEditor::generateDrumsClicked()
{
    refreshLoopBpm();
    generateDrumsData();
    refreshLoopDisplay();
}

void AbletonCopilotAudioProcessorEditor::generateBassClicked()
{
    refreshLoopBpm();
    generateBassData();
    refreshLoopDisplay();
}

void AbletonCopilotAudioProcessorEditor::generateMelodyClicked()
{
    refreshLoopBpm();
    generateMelodyData();
    refreshLoopDisplay();
}

void AbletonCopilotAudioProcessorEditor::generateLoopClicked()
{
    // Convenience wrapper: three independently-seeded generation steps,
    // ONE shared display refresh at the end (not three) - "one click,
    // hear a full loop," while the underlying state is genuinely three
    // independent slots (see generateDrumsClicked/generateBassClicked/
    // generateMelodyClicked above, each reachable independently via the
    // per-track Serum2 panels' own Generate buttons - see
    // PluginEditor::generateForTrack).
    refreshLoopBpm();
    generateDrumsData();
    generateBassData();
    generateMelodyData();
    refreshLoopDisplay();
}

void AbletonCopilotAudioProcessorEditor::refreshLoopBpm()
{
    // Shared across all three categories (drums/bass/melody all "share
    // the same BPM") - refreshed on every generate action, not just the
    // first, so it always reflects the live host tempo if one exists.
    currentLoopBpm = (double) processor.currentBpm.load(std::memory_order_relaxed);
    if (currentLoopBpm <= 0.0)
        currentLoopBpm = 124.0; // matches the real reference arrangements' own tempo - see melodic_techno_research.md section 3.2
}

void AbletonCopilotAudioProcessorEditor::refreshLoopDisplay()
{
    // Rebuilds the pattern grid + structured status text from whatever is
    // CURRENTLY STORED in currentDrumGroove/currentBassMotif/
    // currentMelodyMotif - each independently, only the ones that
    // actually exist. Never regenerates anything, never overwrites
    // stored data - this is the single place all three generate-click
    // handlers converge on for display, so none of them needs to know
    // about the other two categories' data at all.
    const auto [keyRoot, isMinor] = getSelectedKey();
    juce::ignoreUnused(isMinor);

    exportDrumStemsButton.setEnabled(currentDrumGroove.has_value()); // Export Drum Stems only needs drum roles

    std::vector<GeneratedDrumGridComponent::RowDisplay> displayRows;
    juce::StringArray summaryParts;
    int totalHits = 0;

    if (currentDrumGroove.has_value())
    {
        for (auto& role : drumRoleExports(currentDrumGroove->drum))
        {
            std::vector<int> velocity = Engine::toVelocityArray(role.steps);
            const int activeCount = (int) std::count_if(velocity.begin(), velocity.end(),
                                                          [](int v) { return v > 0; });
            GeneratedDrumGridComponent::RowDisplay dr;
            dr.name     = role.name;
            dr.colour   = role.colour;
            dr.velocity = std::move(velocity);
            displayRows.push_back(std::move(dr));

            totalHits += activeCount;
            summaryParts.add(juce::String(activeCount) + " " + juce::String(role.name).toLowerCase());
        }
    }

    {
        GeneratedDrumGridComponent::RowDisplay bassRow;
        bassRow.name     = "BASS";
        bassRow.colour   = UIStyle::kBass;
        bassRow.velocity = offsetsToDisplayVelocity(currentBassMotif, Engine::kBassOffValue);
        displayRows.push_back(std::move(bassRow));

        GeneratedDrumGridComponent::RowDisplay melodyRow;
        melodyRow.name     = "MELODY";
        melodyRow.colour   = UIStyle::kLead;
        melodyRow.velocity = offsetsToDisplayVelocity(currentMelodyMotif, MelodyGridComponent::kMelodyOff);
        displayRows.push_back(std::move(melodyRow));

        // PAD - explicitly all-off. There is no pad generator this phase
        // (see GrooveLoop.h) - the Pad voice, its mute control, and its
        // Serum2 hosting all stay fully intact and functional, simply
        // idle, rather than deleted.
        GeneratedDrumGridComponent::RowDisplay padRow;
        padRow.name     = "PAD";
        padRow.colour   = UIStyle::kPad;
        padRow.velocity = std::vector<int>((size_t) Engine::kGrooveLoopTotalSteps, 0);
        displayRows.push_back(std::move(padRow));
    }

    generatedDrumGrid.setPattern(std::move(displayRows), Engine::kGrooveLoopStepsPerBar, Engine::kGrooveLoopBars);
    // setPattern() can change the row count - re-run the real layout
    // (resized()) immediately so mainContent's height and
    // mainViewport's scroll range are correct right away, not just after
    // the next window resize (the same single source of truth as every
    // other layout change, not a bespoke partial resize).
    resized();

    // Pad (track 2) always gets an explicit all-off pattern - no pad
    // generator exists this phase. Bass/Melody (tracks 0/1) are NOT
    // re-sent here - generateBassData()/generateMelodyData() already sent
    // whatever's currently stored to the processor when they last ran;
    // re-sending unconditionally here would be harmless (same data) but
    // is deliberately not done, to keep "which function actually owns
    // sending track N's pattern" unambiguous.
    const std::vector<int8_t> padAllOff((size_t) Engine::kGrooveLoopTotalSteps, Engine::kPadOffValue);
    processor.setGeneratedMelodyPattern(2, padAllOff, keyRoot);

    int bassActiveCount = 0;
    for (auto v : currentBassMotif)
        if (v != Engine::kBassOffValue)
            ++bassActiveCount;
    int melodyActiveCount = 0;
    for (auto v : currentMelodyMotif)
        if (v != MelodyGridComponent::kMelodyOff)
            ++melodyActiveCount;
    const int padActiveCount = 0; // always silent this phase - no pad generator, see above

    juce::String status;
    status << "Generated - " << (summaryParts.isEmpty() ? juce::String("(no drums yet)") : summaryParts.joinIntoString(" / "))
           << " hits (" << totalHits
           << " total), bass " << bassActiveCount << " notes, melody " << melodyActiveCount
           << " notes, pad " << padActiveCount << " notes - an " << Engine::kGrooveLoopBars
           << "-bar loop that repeats indefinitely (no breakdown/arrangement this phase).\n";
    status << "Playing directly from AbletonCopilot - start Ableton's transport to hear it. "
              "No Drum Rack or other instrument required.\n";
    status << "Sample index: " << (int) latestSampleIndex.size() << " analyzed samples total.\n";

    // Structured DRUMS lines (RuntimeStatusText.h) - only the 5 rhythmic
    // roles the user's own requested format names (Kick/Closed Hat/Open
    // Hat/Perc A/Perc B - Clap's full diagnostic stays available below in
    // the detailed block, not removed, just not duplicated up here).
    // "(synth fallback)" is shown honestly whenever no real sample file
    // was actually loaded - same d.finalLoadedFile check the detailed
    // diagnostic line already uses, never a separate/optimistic claim.
    std::vector<RuntimeStatusText::DrumRoleLine> drumRoleLines;
    auto structuredLabelFor = [](const char* roleName) -> const char*
    {
        if (juce::String(roleName) == "KICK")       return "Kick";
        if (juce::String(roleName) == "HAT_CLOSED") return "Closed Hat";
        if (juce::String(roleName) == "HAT_OPEN")   return "Open Hat";
        if (juce::String(roleName) == "PERC_A")     return "Perc A";
        if (juce::String(roleName) == "PERC_B")     return "Perc B";
        return nullptr; // CLAP - intentionally not in this compact block
    };

    if (currentDrumGroove.has_value())
    {
        for (auto& role : drumRoleExports(currentDrumGroove->drum))
        {
            Engine::DrumRole resolvedRole;
            if (!Engine::drumRoleForGmNote(role.midiNote, resolvedRole))
                continue;

            const DrumSampleChoice& choice = choiceForRoleName(currentDrumSampleSelection, role.name);
            const auto d = processor.getGeneratedRoleLoadDiagnostics(resolvedRole);

            const juce::String label = juce::String(role.name).toLowerCase().substring(0, 1).toUpperCase()
                                      + juce::String(role.name).toLowerCase().substring(1);
            status << label << ": pool=" << choice.poolSize << "/" << choice.shortlistSize
                   << "  cand=" << (d.candidateFile.getFileName().isEmpty() ? juce::String("(none)") : d.candidateFile.getFileName())
                   << " [" << (d.candidateExists ? juce::String("found") : juce::String("MISSING"))
                   << (d.formatRecognized ? (", " + d.recognizedFormatName) : juce::String(", unrecognized"))
                   << (d.readerCreated ? (", " + juce::String((juce::int64) d.decodedLengthSamples) + "smp") : juce::String(", no-reader"))
                   << "]  -> " << (d.finalLoadedFile.existsAsFile() ? d.finalLoadedFile.getFileName() : juce::String("SYNTH FALLBACK"))
                   << "\n";

            if (const char* structuredLabel = structuredLabelFor(role.name))
            {
                RuntimeStatusText::DrumRoleLine line;
                line.label    = structuredLabel;
                line.fileName = d.finalLoadedFile.existsAsFile() ? d.finalLoadedFile.getFileName() : juce::String("(synth fallback)");
                drumRoleLines.push_back(line);
            }
        }
    }

    drumPatternStatusLabel.setText(status, juce::dontSendNotification);

    // Real MIDI register actually sent for the bass, computed from the
    // EXACT array just handed to setGeneratedMelodyPattern
    // (currentBassMotif) using the SAME clampBassRegisterPitch()
    // PluginProcessor's real trigger loop applies - not estimated, not
    // assumed. Feeds into the compact BASS line below (structuredStatusLabel).
    juce::String bassRangeText;
    {
        int bassMin = 200, bassMax = -1;
        for (int8_t off : currentBassMotif)
        {
            if (off == Engine::kBassOffValue)
                continue;
            const int pitch = juce::jlimit(0, 127, clampBassRegisterPitch(36 + keyRoot + (int) off));
            bassMin = juce::jmin(bassMin, pitch);
            bassMax = juce::jmax(bassMax, pitch);
        }
        bassRangeText = bassMax >= 0
            ? juce::MidiMessage::getMidiNoteName(bassMin, true, true, 3) + " - "
              + juce::MidiMessage::getMidiNoteName(bassMax, true, true, 3)
            : juce::String("(no notes)");
    }

    // Same idea for melody's own register (clampMelodyRegisterPitch,
    // PluginProcessor.h) - proves in the UI, not just in code, that
    // melody now has its own real, independently-clamped range distinct
    // from bass's.
    juce::String melodyRangeText;
    {
        int melMin = 200, melMax = -1;
        for (int8_t off : currentMelodyMotif)
        {
            if (off == MelodyGridComponent::kMelodyOff)
                continue;
            const int pitch = juce::jlimit(0, 127, clampMelodyRegisterPitch(36 + keyRoot + (int) off));
            melMin = juce::jmin(melMin, pitch);
            melMax = juce::jmax(melMax, pitch);
        }
        melodyRangeText = melMax >= 0
            ? juce::MidiMessage::getMidiNoteName(melMin, true, true, 3) + " - "
              + juce::MidiMessage::getMidiNoteName(melMax, true, true, 3)
            : juce::String("(no notes)");
    }

    // The structured DRUMS/BASS/MELODY/PAD/SECTION block itself
    // (RuntimeStatusText.h) - built from exactly the same real diagnostic
    // calls the labels above already used (getMelodyVoiceDiagnostics,
    // SerumPresetStatus::compactPresetLine's honesty gate), never a
    // separate/optimistic re-derivation. There is no DROP/BREAKDOWN/
    // PRE_DROP section in this flat 8-bar phase - the line reads "LOOP"
    // right after Generate; timerCallback() upgrades it to a live
    // "LOOP (bar N/8)" bar counter once the transport is actually
    // playing (see that function).
    {
        std::vector<RuntimeStatusText::VoiceLine> voices;

        auto confirmedNameFor = [&](int trackIndex) -> juce::String
        {
            return (trackIndex >= 0 && trackIndex < melodyPanels.size())
                ? melodyPanels[trackIndex]->lastConfirmedPresetName
                : juce::String();
        };

        RuntimeStatusText::VoiceLine bassLine;
        bassLine.label      = "BASS";
        bassLine.presetLine = SerumPresetStatus::compactPresetLine(confirmedNameFor(0),
                                   processor.getMelodyVoiceDiagnostics(0).capturedPresetActive);
        bassLine.extraLine  = "MIDI " + bassRangeText;
        bassLine.active     = processor.getMelodyVoiceDiagnostics(0).generatedPatternActive;
        voices.push_back(bassLine);

        RuntimeStatusText::VoiceLine melodyLine;
        melodyLine.label      = "MELODY";
        melodyLine.presetLine = SerumPresetStatus::compactPresetLine(confirmedNameFor(1),
                                     processor.getMelodyVoiceDiagnostics(1).capturedPresetActive);
        melodyLine.extraLine  = "MIDI " + melodyRangeText;
        melodyLine.active     = processor.getMelodyVoiceDiagnostics(1).generatedPatternActive;
        voices.push_back(melodyLine);

        RuntimeStatusText::VoiceLine padLine;
        padLine.label      = "PAD";
        padLine.presetLine = SerumPresetStatus::compactPresetLine(confirmedNameFor(2),
                                  processor.getMelodyVoiceDiagnostics(2).capturedPresetActive);
        padLine.active     = processor.getMelodyVoiceDiagnostics(2).generatedPatternActive;
        voices.push_back(padLine);

        structuredStatusPrefix = RuntimeStatusText::buildStatusPrefix(drumRoleLines, voices);
        structuredStatusLabel.setText(
            RuntimeStatusText::appendSection(structuredStatusPrefix, "LOOP"),
            juce::dontSendNotification);
    }
}

void AbletonCopilotAudioProcessorEditor::exportDrumStemsClicked()
{
    if (!currentDrumGroove.has_value())
        return; // button is disabled in this case (see refreshLoopDisplay); guard anyway - never export nothing

    const juce::File startFolder = lastStemExportFolder.isDirectory()
        ? lastStemExportFolder
        : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    stemExportFileChooser = std::make_unique<juce::FileChooser>(
        "Choose a folder for the exported drum stems", startFolder);

    auto flags = juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectDirectories;

    stemExportFileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        const juce::File chosen = fc.getResult();
        if (!chosen.isDirectory())
            return; // cancelled

        lastStemExportFolder = chosen;

        // Snapshot only, via PluginProcessor::getGeneratedDrumStemSnapshot -
        // no regeneration, no change to sample selection/mute/pattern
        // state. bpm comes from currentLoopBpm (the actual generation
        // bpm), not the live-transport cache, so export works whether or
        // not the host transport has ever run.
        const double bpm = currentDrumGroove.has_value() ? currentLoopBpm : 124.0;
        const auto   input = processor.getGeneratedDrumStemSnapshot(bpm);

        const DrumStemExporter::Result stems       = DrumStemExporter::renderDrumStems(input);
        const auto                     writeResult = DrumStemExporter::writeStemsToWav(stems, chosen, input.sampleRate);

        std::vector<RuntimeStatusText::StemExportRoleStatus> roleStatuses;
        for (auto& r : stems)
        {
            RuntimeStatusText::StemExportRoleStatus rs;
            rs.label    = r.roleLabel;
            rs.fileName = r.loadedFile.existsAsFile() ? r.loadedFile.getFileName() : juce::String("(synth fallback)");
            roleStatuses.push_back(rs);
        }

        juce::String text = RuntimeStatusText::buildDrumStemExportStatus(
            roleStatuses, Engine::kGrooveLoopBars, input.bpm, input.sampleRate);

        std::array<bool, 6> succeeded {};
        for (size_t i = 0; i < succeeded.size(); ++i)
            succeeded[i] = writeResult.writtenFiles[i].existsAsFile();

        text << RuntimeStatusText::buildDrumStemExportedConfirmation(succeeded);
        if (!writeResult.allSucceeded)
            text << "\n" << writeResult.errorMessage;
        text << "\nOutput: " << writeResult.outputDirectory.getFullPathName();

        exportDrumStemsStatusLabel.setText(text, juce::dontSendNotification);
        resized();
    });
}

void AbletonCopilotAudioProcessorEditor::logPercussionCandidateDiagnostics(
    const std::vector<IndexedSample>& indexed, const DrumSampleSelection& sel, double bpm)
{
    // Same pool percA/percB actually draw from - see
    // DrumSampleSelector.cpp's candidatesForRackIds(indexed, { "PERC" }).
    // Deliberately re-filtered here rather than threaded through from
    // selectDrumSamples() so this stays a read-only diagnostic with zero
    // chance of influencing the real selection.
    struct Row
    {
        const IndexedSample*        sample;
        float                       dspScore;
        Engine::PackClassification  pack;
        float                       tierWeight;
        float                       combined;
    };

    std::vector<Row> rows;
    for (auto& s : indexed)
    {
        if (s.rackId != "PERC" || !s.features.valid)
            continue;
        Row r;
        r.sample     = &s;
        r.dspScore   = Engine::scoreForRole(Engine::DrumRole::PercA, s.features, bpm);
        r.pack       = Engine::classifyPackTier(s.file.getFullPathName().toStdString());
        r.tierWeight = Engine::tierWeight(r.pack.tier);
        r.combined   = r.dspScore * r.tierWeight;
        rows.push_back(r);
    }

    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.combined > b.combined; });

    auto tierName = [](Engine::PackTier t) -> const char*
    {
        switch (t)
        {
            case Engine::PackTier::MelodicTechno:    return "Tier1-MelodicTechno";
            case Engine::PackTier::AdjacentTechno:   return "Tier2-AdjacentTechno";
            case Engine::PackTier::OtherElectronic:  return "Tier3-OtherElectronic";
            case Engine::PackTier::OffGenreFallback: return "Tier4-OffGenreFallback";
        }
        return "?";
    };

    juce::String report;
    report << "=== Percussion (PERC) candidate diagnostics - " << juce::Time::getCurrentTime().toString(true, true) << " ===\n";
    report << "Pool size: " << (int) rows.size() << " analyzed PERC candidates (out of "
           << (int) indexed.size() << " total analyzed samples).\n";
    report << "Chosen percA: " << (sel.percA.file.existsAsFile() ? sel.percA.file.getFullPathName() : juce::String("(none)")) << "\n";
    report << "Chosen percB: " << (sel.percB.file.existsAsFile() ? sel.percB.file.getFullPathName() : juce::String("(none)")) << "\n\n";
    report << "Top " << juce::jmin(10, (int) rows.size()) << " candidates, ranked by combined score "
              "(dspScore x packTierWeight):\n";

    const int topN = juce::jmin(10, (int) rows.size());
    for (int i = 0; i < topN; ++i)
    {
        const Row& r = rows[(size_t) i];
        const auto& f = r.sample->features;
        const bool isPercA = r.sample->file == sel.percA.file;
        const bool isPercB = r.sample->file == sel.percB.file;

        report << (i + 1) << ". " << r.sample->file.getFullPathName()
               << (isPercA ? "  [SELECTED percA]" : "") << (isPercB ? "  [SELECTED percB]" : "") << "\n";
        report << "   pack=" << (r.pack.packName.empty() ? std::string("(unclassified)") : r.pack.packName)
               << "  tier=" << tierName(r.pack.tier) << "  classification=" << r.sample->rackId << "\n";
        report << "   duration=" << juce::String(f.durationSec, 3) << "s"
               << "  peak=" << juce::String(f.peakAmplitude, 3)
               << "  rms=" << juce::String(f.rms, 3)
               << "  attack=" << juce::String(f.attackTimeMs, 1) << "ms"
               << "  spectralCentroid=" << juce::String(f.spectralCentroidHz, 0) << "Hz"
               << "  zcr=" << juce::String(f.zeroCrossingRate, 0) << "/s"
               << "  pitch=" << juce::String(f.estimatedPitchHz, 0) << "Hz\n";
        report << "   dspScore=" << juce::String(r.dspScore, 4)
               << "  packTierWeight=" << juce::String(r.tierWeight, 2)
               << "  combinedScore=" << juce::String(r.combined, 4) << "\n";
    }

    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("AbletonCopilot");
    dir.createDirectory();
    dir.getChildFile("percussion_selection_debug.txt").replaceWithText(report);
}

void AbletonCopilotAudioProcessorEditor::buildMelodyEditSuggestions()
{
    auto [keyRoot, isMinor] = getSelectedKey();

    std::vector<AdvisorPanelComponent::ActionableFeedback> out;
    for (int t = 0; t < melodyGrid.getNumTracks(); ++t)
    {
        auto offsets = melodyGrid.getTrackOffsets(t);
        juce::String trackLabel = (t < melodyPanels.size())
            ? categoryDisplayName(melodyPanels[t]->category)
            : juce::String("TRACK ") + juce::String(t);
        MelodyCategory category = (t < melodyPanels.size()) ? melodyPanels[t]->category : MelodyCategory::Bass;
        auto suggestions = melodyCritic.critique(t, trackLabel, offsets, keyRoot, isMinor, category);
        for (auto& s : suggestions)
        {
            AdvisorPanelComponent::ActionableFeedback card;
            card.item = feedbackItemForMelodyEdit(s);
            card.applyChange = [this, trackIndex = s.trackIndex, step = s.step, newOffset = s.newOffset]
            {
                melodyGrid.setStepOffset(trackIndex, step, newOffset);
            };
            card.revertChange = [this, trackIndex = s.trackIndex, step = s.step, oldOffset = s.oldOffset]
            {
                melodyGrid.setStepOffset(trackIndex, step, oldOffset);
            };
            out.push_back(std::move(card));
        }

        auto layering = melodyCritic.suggestLayering(t, trackLabel, category, offsets,
                                                       melodyGrid.getNumTracks(),
                                                       AbletonCopilotAudioProcessor::kMaxMelodyTracks);
        if (layering)
        {
            auto l = *layering;
            FeedbackItem item;
            item.severity = Severity::Good;
            item.category = "LAYERING";
            item.headline = "Layer a " + categoryDisplayName(l.suggestedCategory) + " track on " + l.sourceLabel;
            item.detail   = l.reason;

            // No track-removal capability exists yet, so "before"/"after"
            // for an added track is mute state, not add/remove: the first
            // apply creates + generates the track once, then both
            // apply/revert just flip its mute - Keep leaves it unmuted (the
            // actual "add"), Undo leaves it muted rather than deleting it.
            auto createdIndex = std::make_shared<int>(-1);

            AdvisorPanelComponent::ActionableFeedback card;
            card.item = item;
            card.applyChange = [this, l, createdIndex]
            {
                if (*createdIndex < 0)
                {
                    if (melodyPanels.size() >= AbletonCopilotAudioProcessor::kMaxMelodyTracks)
                        return; // room ran out since this suggestion was built

                    addMelodyTrackClicked();
                    const int newIndex = melodyGrid.getNumTracks() - 1;
                    if (newIndex < 0 || newIndex >= melodyPanels.size())
                        return;

                    melodyGrid.setTrackIdentity(newIndex, categoryDisplayName(l.suggestedCategory),
                                                 categoryColour(l.suggestedCategory));
                    melodyPanels[newIndex]->category = l.suggestedCategory;
                    processor.setMelodyTrackCategory(newIndex, l.suggestedCategory);
                    updateTrackTitle(*melodyPanels[newIndex]);

                    auto sourceOffsets = melodyGrid.getTrackOffsets(l.sourceTrackIndex);
                    for (int step = 0; step < (int) sourceOffsets.size(); ++step)
                    {
                        if (sourceOffsets[(size_t) step] == MelodyGridComponent::kMelodyOff)
                            continue;
                        const int8_t transposed = (int8_t) juce::jlimit(-24, 24,
                            (int) sourceOffsets[(size_t) step] + l.transposeSemitones);
                        melodyGrid.setStepOffset(newIndex, step, transposed);
                    }
                    *createdIndex = newIndex;
                }
                else
                {
                    melodyGrid.setTrackMuted(*createdIndex, false);
                }
            };
            card.revertChange = [this, createdIndex]
            {
                if (*createdIndex >= 0)
                    melodyGrid.setTrackMuted(*createdIndex, true);
            };
            out.push_back(std::move(card));
        }
    }

    advisorPanel.setMelodyEditSuggestions(out);
}

void AbletonCopilotAudioProcessorEditor::buildArrangementSuggestions()
{
    std::vector<MelodyTrackContext> tracks;
    for (int t = 0; t < melodyGrid.getNumTracks(); ++t)
    {
        MelodyTrackContext track;
        track.trackIndex = t;
        track.label      = (t < melodyPanels.size()) ? categoryDisplayName(melodyPanels[t]->category)
                                                       : juce::String("TRACK ") + juce::String(t);
        track.category   = (t < melodyPanels.size()) ? melodyPanels[t]->category : MelodyCategory::Bass;
        track.offsets    = melodyGrid.getTrackOffsets(t);
        tracks.push_back(std::move(track));
    }

    std::vector<DrumRowContext> drumRows;
    for (int r = 0; r < drumMachine.getNumRows(); ++r)
    {
        DrumRowContext row;
        row.rackId = drumMachine.getRowRackId(r);
        row.steps  = drumMachine.getRowSteps(r);
        drumRows.push_back(std::move(row));
    }

    ArrangementResult result = arrangementAdvisor.analyze(tracks, drumRows, advisorPanel.getSelectedSection());

    juce::String status;
    for (auto& item : result.informational)
        status << item.headline << (item.detail.isNotEmpty() ? " - " + item.detail : juce::String()) << "\n";
    if (status.isNotEmpty())
        advisorPanel.setStatus(status.trim());

    std::vector<AdvisorPanelComponent::ActionableFeedback> out;

    if (result.needsHook)
    {
        FeedbackItem item;
        item.severity = Severity::Warning;
        item.category = "ARRANGEMENT";
        item.headline = "Add a Lead hook to this Drop";
        item.detail   = "No melodic track is carrying a hook in this section.";

        // Same mute-based before/after as the layering suggestion in
        // buildMelodyEditSuggestions() - no track-removal capability exists
        // yet, so "before" is muted rather than truly absent.
        auto createdIndex = std::make_shared<int>(-1);

        AdvisorPanelComponent::ActionableFeedback card;
        card.item = item;
        card.applyChange = [this, createdIndex]
        {
            if (*createdIndex < 0)
            {
                if (melodyPanels.size() >= AbletonCopilotAudioProcessor::kMaxMelodyTracks)
                    return;

                addMelodyTrackClicked();
                const int newIndex = melodyGrid.getNumTracks() - 1;
                if (newIndex < 0 || newIndex >= melodyPanels.size())
                    return;

                melodyGrid.setTrackIdentity(newIndex, categoryDisplayName(MelodyCategory::Lead),
                                             categoryColour(MelodyCategory::Lead));
                melodyPanels[newIndex]->category = MelodyCategory::Lead;
                processor.setMelodyTrackCategory(newIndex, MelodyCategory::Lead);
                updateTrackTitle(*melodyPanels[newIndex]);

                const auto [root, minor] = getSelectedKey();
                melodyGrid.generateForTrack(newIndex, root, minor, MelodyCategory::Lead, MelodyStyle::Default);
                exportMelodyPattern();
                *createdIndex = newIndex;
            }
            else
            {
                melodyGrid.setTrackMuted(*createdIndex, false);
            }
        };
        card.revertChange = [this, createdIndex]
        {
            if (*createdIndex >= 0)
                melodyGrid.setTrackMuted(*createdIndex, true);
        };
        out.push_back(std::move(card));
    }

    advisorPanel.setArrangementSuggestions(out);
}

//==============================================================================

void AbletonCopilotAudioProcessorEditor::paint(juce::Graphics& g)
{
    static bool loggedFirstPaint = false;
    if (!loggedFirstPaint)
    {
        loggedFirstPaint = true;
        StartupTiming::mark("Editor first paint()");
    }

    g.fillAll(kBg);

    auto full = getLocalBounds();

    // Header
    paintHeader(g, full.removeFromTop(48));

    if (!kShowAnalyzer)
        return;

    auto area = full.reduced(12, 0);

    // Skip genre picker row + apply button row + status
    area.removeFromTop(30 + 4 + 28 + 6 + 18 + 6);

    // Transport / capture bar
    paintTransportBar(g, area.removeFromTop(28));
    area.removeFromTop(6);

    // Live meters
    paintLiveMeters(g, area.removeFromTop(48));
}

//==============================================================================

void AbletonCopilotAudioProcessorEditor::paintHeader(juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour(kPanel);
    g.fillRect(r);

    g.setColour(kBorder);
    g.drawHorizontalLine(r.getBottom() - 1, 0.0f, (float) getWidth());

    auto inner = r.reduced(14, 0);
    g.setColour(kTextPrimary);
    g.setFont(title());
    g.drawText("Ableton Copilot", inner, juce::Justification::centredLeft);
}

//==============================================================================

void AbletonCopilotAudioProcessorEditor::paintTransportBar(juce::Graphics& g,
                                                            juce::Rectangle<int> r)
{
    bool playing  = processor.currentlyPlaying.load(std::memory_order_relaxed);
    bool analyzing = processor.getAnalyzer().isAnalyzing();
    int  buffSecs = processor.getAnalyzer().bufferedSeconds();

    g.setColour(kPanel);
    g.fillRoundedRectangle(r.toFloat(), 4.0f);

    // State dot
    juce::Colour dotCol = analyzing ? kWarn
                        : playing   ? kPlaying
                                    : kStopped;
    g.setColour(dotCol);
    auto dot = r.reduced(0, 8).removeFromLeft(8);
    dot.setX(dot.getX() + 10);
    g.fillEllipse(dot.toFloat());

    // State text
    juce::String stateText = analyzing ? "Analyzing..."
                           : playing   ? "Listening  |  " + juce::String(buffSecs) + "s captured"
                                       : (buffSecs > 0 ? "Stopped  |  " + juce::String(buffSecs) + "s captured"
                                                       : "Stopped  |  Press Play in Ableton");

    g.setColour(kTextPrimary);
    g.setFont(juce::FontOptions(12.0f));
    g.drawText(stateText, r.reduced(26, 0), juce::Justification::centredLeft);

    // Capture progress bar (right side)
    if (buffSecs > 0 && !analyzing)
    {
        float pct = std::min(1.0f, buffSecs / 60.0f);
        auto barArea = r.reduced(0, 10).removeFromRight(80).withTrimmedRight(10);
        g.setColour(kBorder);
        g.fillRoundedRectangle(barArea.toFloat(), 2.0f);
        auto filled = barArea.withWidth((int)(barArea.getWidth() * pct));
        g.setColour(kAccent.withAlpha(0.6f));
        g.fillRoundedRectangle(filled.toFloat(), 2.0f);
    }
}

//==============================================================================

void AbletonCopilotAudioProcessorEditor::paintLiveMeters(juce::Graphics& g,
                                                          juce::Rectangle<int> r)
{
    float rmsDb  = processor.liveRmsDb.load(std::memory_order_relaxed);
    float width  = processor.liveStereoWidth.load(std::memory_order_relaxed);
    float peakDb = processor.livePeakDb.load(std::memory_order_relaxed);

    g.setColour(kPanel);
    g.fillRoundedRectangle(r.toFloat(), 4.0f);

    auto inner = r.reduced(10, 6);

    // Three columns: Level | Width | Peak
    int colW = inner.getWidth() / 3;

    auto drawMeter = [&](juce::Rectangle<int> cell,
                         const juce::String& label,
                         const juce::String& value,
                         float normalised) // 0-1 fill
    {
        g.setColour(kTextDim);
        g.setFont(juce::FontOptions(9.5f));
        g.drawText(label, cell.removeFromTop(13), juce::Justification::centredLeft);

        auto barRow = cell.removeFromBottom(6);
        g.setColour(kBorder);
        g.fillRoundedRectangle(barRow.toFloat(), 2.0f);
        if (normalised > 0.0f)
        {
            auto filled = barRow.withWidth((int)(barRow.getWidth() * normalised));
            juce::Colour barCol = normalised > 0.85f ? kCrit
                                : normalised > 0.65f ? kWarn
                                                     : kAccent.withAlpha(0.8f);
            g.setColour(barCol);
            g.fillRoundedRectangle(filled.toFloat(), 2.0f);
        }

        g.setColour(kTextPrimary);
        g.setFont(juce::FontOptions(15.0f).withStyle("Bold"));
        g.drawText(value, cell, juce::Justification::centredLeft);
    };

    // Level: map -60 to 0 dBFS -> 0 to 1
    float levelNorm = rmsDb > -60.0f ? (rmsDb + 60.0f) / 60.0f : 0.0f;
    drawMeter(inner.removeFromLeft(colW), "LEVEL (RMS)",
              rmsDb > -60.0f ? juce::String(rmsDb, 1) + " dB" : "---", levelNorm);

    inner.removeFromLeft(4);

    // Stereo width: 0 to 1
    drawMeter(inner.removeFromLeft(colW - 4), "STEREO WIDTH",
              juce::String(width, 2), width);

    inner.removeFromLeft(4);

    // Peak: map -60 to 0 dBFS -> 0 to 1
    float peakNorm = peakDb > -60.0f ? (peakDb + 60.0f) / 60.0f : 0.0f;
    drawMeter(inner, "PEAK",
              peakDb > -60.0f ? juce::String(peakDb, 1) + " dB" : "---", peakNorm);
}

//==============================================================================

void AbletonCopilotAudioProcessorEditor::timerCallback()
{
    // Keeps the "Xs ago" elapsed counter climbing while the library scan
    // is in flight - a real, ticking number is what actually distinguishes
    // "still scanning" from "frozen", which a static label can't.
    if (rackScanStarted && !onIndexReadyFired)
        updateLibraryScanStatusLabel();

    if (kShowAnalyzer)
    {
        // Auto-trigger: fires when Ableton transport stops
        if (processor.playbackJustStopped.exchange(false, std::memory_order_acq_rel))
        {
            int secs = processor.getAnalyzer().bufferedSeconds();
            if (secs >= 5 && !processor.getAnalyzer().isAnalyzing())
            {
                showStatus("Playback stopped - analyzing " + juce::String(secs) + "s of audio...");
                runAnalysis();
            }
            else if (secs < 5)
            {
                showStatus("Playback stopped - need at least 5s (captured " +
                           juce::String(secs) + "s).");
            }
        }

        // Show "Analyzing..." state in the button while running
        bool analyzing = processor.getAnalyzer().isAnalyzing();
        analyzeNowButton.setEnabled(!analyzing);
        analyzeNowButton.setButtonText(analyzing ? "Analyzing..." : "Analyze Now");

        // Reflect correction on/off state in the apply button
        bool corrOn = processor.correctionsEnabled();
        applyButton.setEnabled(hasPendingCorrections);
        applyButton.setButtonText(corrOn ? "Clear Corrections  (Active)" : "Apply Corrections");
        applyButton.setColour(juce::TextButton::buttonColourId,
                              corrOn ? kAccent.withAlpha(0.25f) : kPanel);
        applyButton.setColour(juce::TextButton::textColourOffId,
                              corrOn ? kAccent : (hasPendingCorrections ? kTextPrimary : kTextDim));
    }
    else
    {
        // Analyzer's hidden — just drop the flag so it doesn't pile up.
        processor.playbackJustStopped.exchange(false, std::memory_order_acq_rel);
    }

    {
        // Approximate playhead: phase-locked to host BPM from the moment
        // playback starts, not sample-accurate host PPQ (the processor
        // doesn't currently expose that) — good enough as a visual guide.
        bool  playing = processor.currentlyPlaying.load(std::memory_order_relaxed);
        float bpm     = processor.currentBpm.load(std::memory_order_relaxed);

        if (playing && bpm > 0.0f)
        {
            double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
            if (!wasPlayheadRunning)
                playheadStartTime = now;

            double stepsPerSec = (bpm / 60.0) * 4.0; // 16th notes
            int    step        = ((int) ((now - playheadStartTime) * stepsPerSec)) % DrumMachineComponent::kNumSteps;
            drumMachine.setPlayheadStep(step, true);
            melodyGrid.setPlayheadStep(step, true);
            wasPlayheadRunning = true;
        }
        else
        {
            wasPlayheadRunning = false;
            drumMachine.setPlayheadStep(-1, false);
            melodyGrid.setPlayheadStep(-1, false);
        }

        if (showingAdvisor)
            advisorPanel.setListeningStatus(playing, processor.getAnalyzer().bufferedSeconds());
    }

    {
        // Generated-pattern playhead: reads the host's actual PPQ position
        // (juce::AudioPlayHead::getPosition() is a plain snapshot, safe to
        // read from any thread - the same interface processBlock's own
        // step-trigger logic already uses on the audio thread) rather than
        // approximating from wall-clock time like the block above. Same
        // stepFloor/wrap math as the generated-drum trigger logic in
        // PluginProcessor::processBlock, just reading, never driving
        // anything - this is visualization only, not a second clock.
        const int totalSteps = generatedDrumGrid.getTotalSteps();
        bool      playing    = false;
        int       step       = -1;

        if (totalSteps > 0)
        {
            if (auto* head = processor.getPlayHead())
            {
                if (auto pos = head->getPosition())
                {
                    playing = pos->getIsPlaying();
                    if (playing)
                    {
                        if (auto ppqOpt = pos->getPpqPosition())
                        {
                            const double ppq       = *ppqOpt;
                            const int    stepFloor  = (int) std::floor(ppq * 4.0);
                            step = ((stepFloor % totalSteps) + totalSteps) % totalSteps;
                        }
                    }
                }
            }
        }

        generatedDrumGrid.setPlayheadStep(step, playing && step >= 0);

        // Live "LOOP (bar N/8)" line (RuntimeStatusText.h) - the same
        // step the playhead above already computed, mapped to a 1-based
        // bar within the flat 8-bar groove. There is no DROP/BREAKDOWN/
        // PRE_DROP section to report in this phase (see GrooveLoop.h) -
        // only refreshes when at least one category has actually been
        // generated (drums, bass, or melody - independently, since any
        // one of them can now exist without the others) and the transport
        // is actually playing, otherwise stays at whatever it last said
        // ("LOOP", right after Generate), never a guess.
        const bool anyLoopGenerated = currentDrumGroove.has_value()
                                     || !currentBassMotif.empty()
                                     || !currentMelodyMotif.empty();
        if (anyLoopGenerated && playing && step >= 0)
        {
            const int bar = (step / Engine::kGrooveLoopStepsPerBar) % Engine::kGrooveLoopBars;
            structuredStatusLabel.setText(
                RuntimeStatusText::appendSection(structuredStatusPrefix,
                    "LOOP (bar " + juce::String(bar + 1) + "/" + juce::String(Engine::kGrooveLoopBars) + ")"),
                juce::dontSendNotification);
        }
    }

    for (auto* panel : melodyPanels)
    {
        panel->statusLabel.setText(processor.getMelodyTrackStatus(panel->trackIndex), juce::dontSendNotification);
        panel->openSerumButton.setEnabled(processor.getHostedSerumInstance(panel->trackIndex) != nullptr);
    }

    repaint();
}

//==============================================================================

void AbletonCopilotAudioProcessorEditor::runAnalysis()
{
    if (processor.getAnalyzer().isAnalyzing()) return;

    int secs = processor.getAnalyzer().bufferedSeconds();
    if (secs < 5)
    {
        showStatus("Need at least 5 seconds of audio (captured " +
                   juce::String(secs) + "s). Play your track first.");
        return;
    }

    const auto& profiles = GenreProfiles::getInstance();
    auto ids = profiles.getIds();
    int  sel = genrePicker.getSelectedId() - 1;
    if (sel < 0 || sel >= ids.size()) return;

    float dawBpm = processor.currentBpm.load(std::memory_order_relaxed);

    processor.getAnalyzer().triggerAnalysis(ids[sel], dawBpm, [this](const AnalysisResult& r)
    {
        applyResult(r);
    });
}

void AbletonCopilotAudioProcessorEditor::applyResult(const AnalysisResult& result)
{
    analyzeNowButton.setEnabled(true);
    analyzeNowButton.setButtonText("Analyze Now");

    if (!result.valid)
    {
        showStatus("Analysis failed - capture more audio and try again.");
        return;
    }

    lastResult = result;
    hasResult  = true;

    // Compute and store correction params for this result
    const auto* profile = GenreProfiles::getInstance().getProfile(result.genreId);
    pendingCorrections    = computeCorrections(result.features, profile);
    hasPendingCorrections = true;
    // If corrections were already active, refresh them with the new analysis
    if (processor.correctionsEnabled())
        processor.applyCorrections(pendingCorrections);

    exportAnalysisJSON(result);

    juce::String bpmStr = result.features.bpm > 0.0f
                          ? juce::String(result.features.bpm, 1) + " BPM"
                          : "BPM n/a";
    showStatus("Done  |  " + result.genreDisplayName + "  |  " + bpmStr + "  |  " + result.features.key);

    // Feed the detected genre/bpm/key straight into the stack renderer.
    runStackGeneration();

    resized();
    repaint();
}

void AbletonCopilotAudioProcessorEditor::exportAnalysisJSON(const AnalysisResult& r)
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("AbletonCopilot");
    if (!dir.exists())
        dir.createDirectory();

    auto q = [](const juce::String& s) { return "\"" + s + "\""; };

    juce::String j;
    j << "{\n"
      << "  " << q("genre_id")      << ": " << q(r.genreId)                                  << ",\n"
      << "  " << q("genre_name")    << ": " << q(r.genreDisplayName)                         << ",\n"
      << "  " << q("bpm")           << ": " << juce::String(r.features.bpm, 2)               << ",\n"
      << "  " << q("key")           << ": " << q(r.features.key)                             << ",\n"
      << "  " << q("lufs")          << ": " << juce::String(r.features.lufs, 2)              << ",\n"
      << "  " << q("peak_db")       << ": " << juce::String(r.features.peakDb, 2)            << ",\n"
      << "  " << q("stereo_width")  << ": " << juce::String(r.features.stereoWidth, 3)       << ",\n"
      << "  " << q("sub_pct")       << ": " << juce::String(r.features.subPct, 4)            << ",\n"
      << "  " << q("low_pct")       << ": " << juce::String(r.features.lowPct, 4)            << ",\n"
      << "  " << q("low_mid_pct")   << ": " << juce::String(r.features.lowMidPct, 4)         << ",\n"
      << "  " << q("high_mid_pct")  << ": " << juce::String(r.features.highMidPct, 4)        << ",\n"
      << "  " << q("air_pct")       << ": " << juce::String(r.features.airPct, 4)            << ",\n"
      << "  " << q("dynamic_range") << ": " << juce::String(r.features.dynamicRangeDb, 2)    << "\n"
      << "}\n";

    dir.getChildFile("analysis.json").replaceWithText(j);
}

CorrectionParams AbletonCopilotAudioProcessorEditor::computeCorrections(
    const AudioFeatures& f, const GenreProfile* p)
{
    CorrectionParams c;

    // Output gain to hit genre LUFS target (clamped to safe range)
    float targetLufs = p ? p->lufsTarget : -9.0f;
    c.outputGainDb = juce::jlimit(-6.0f, 12.0f, targetLufs - f.lufs);

    // Spectral band corrections — 1 dB per ~5% of deviation from target
    float subTarget = p ? p->subPctTarget    : 0.20f;
    float midTarget = p ? p->lowMidPctTarget : 0.34f;
    float airTarget = p ? p->airPctTarget    : 0.07f;

    c.subBoostDb = juce::jlimit(-3.0f, 3.0f, (subTarget - f.subPct)    * 20.0f);
    c.midBoostDb = juce::jlimit(-2.5f, 2.5f, (midTarget - f.lowMidPct) * 12.0f);
    c.airBoostDb = juce::jlimit(-3.0f, 3.0f, (airTarget - f.airPct)    * 20.0f);

    // Stereo width correction
    float widthMin = p ? p->stereoWidthMin : 0.40f;
    if (f.stereoWidth < widthMin * 0.75f)
        c.widthScale = 1.25f;   // clearly too narrow — widen
    else if (f.stereoWidth > 0.75f)
        c.widthScale = 0.85f;   // too wide — narrow slightly

    return c;
}

void AbletonCopilotAudioProcessorEditor::showStatus(const juce::String& msg)
{
    statusLabel.setText(msg, juce::dontSendNotification);
}

void AbletonCopilotAudioProcessorEditor::showStackStatus(const juce::String& msg)
{
    stackStatusLabel.setText(msg, juce::dontSendNotification);
}

void AbletonCopilotAudioProcessorEditor::chooseLibrary()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Choose your sample library folder",
        libraryDir.isDirectory() ? libraryDir
                                  : juce::File::getSpecialLocation(juce::File::userHomeDirectory));

    auto flags = juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectDirectories;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto result = fc.getResult();
        if (!result.isDirectory())
            return;

        libraryDir = result;
        libraryPathLabel.setText(libraryDir.getFullPathName(), juce::dontSendNotification);
        sampleIndexReady     = false; // the old index belongs to the previous library dir - don't let Generate use it while the new one scans
        rackScanStarted      = true;
        scanPipelineStartSecs = juce::Time::getMillisecondCounterHiRes() * 0.001;
        rackFilesDiscovered  = -1;
        rackScanCompleted    = false;
        onRacksChangedFired  = false;
        sampleIndexScanStarted = false;
        onIndexReadyFired    = false;
        updateGenerateButtonAvailability();
        updateLibraryScanStatusLabel();
        rackBrowser.setLibraryDir(libraryDir);
        presetScanner.setLibraryDir(libraryDir);

        auto libFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                           .getChildFile("AbletonCopilot/library_path.txt");
        libFile.getParentDirectory().createDirectory();
        libFile.replaceWithText(libraryDir.getFullPathName());
    });
}

void AbletonCopilotAudioProcessorEditor::runStackGeneration()
{
    if (!hasResult)
    {
        showStackStatus("Analyze a track first (play + stop, or Analyze Now).");
        return;
    }
    if (!libraryDir.isDirectory())
    {
        showStackStatus("Choose a sample library folder first.");
        return;
    }
    if (stackRenderer.isRendering())
        return;

    generateStackButton.setEnabled(false);
    showStackStatus("Rendering drum stack...");

    stackRenderer.render(lastResult.genreId, lastResult.features.bpm, lastResult.features.key,
                          libraryDir,
        [this](bool ok, const juce::String& log, std::vector<StackSection> sections)
        {
            generateStackButton.setEnabled(true);

            if (!ok)
            {
                showStackStatus("Render failed — check the Companion is set up.");
                DBG(log);
                return;
            }

            stackBrowser.setSections(std::move(sections));
            showStackStatus("Stack ready — drag stems onto tracks in Arrangement View.");
        });
}

void AbletonCopilotAudioProcessorEditor::pushAllDrumRowsToProcessor()
{
    const int activeRows = drumMachine.getNumRows();

    for (int i = 0; i < AbletonCopilotAudioProcessor::kDrumRows; ++i)
    {
        if (i < activeRows)
        {
            processor.setDrumRowSample(i, drumMachine.getSampleForRow(i));
            processor.setDrumRowMuted(i, drumMachine.isRowMuted(i));
            processor.setDrumRowSolo(i, drumMachine.isRowSolo(i));

            auto steps = drumMachine.getRowSteps(i);
            for (int s = 0; s < (int) steps.size(); ++s)
                processor.setDrumRowStep(i, s, steps[(size_t) s]);
        }
        else
        {
            // Clear stale slots — a rescan can shrink the row count, and row
            // identity is positional, so a leftover sample/pattern at an
            // index no longer in use must not keep triggering.
            processor.setDrumRowSample(i, juce::File());
            processor.setDrumRowMuted(i, false);
            processor.setDrumRowSolo(i, false);
            for (int s = 0; s < AbletonCopilotAudioProcessor::kDrumSteps; ++s)
                processor.setDrumRowStep(i, s, false);
        }
    }
}

std::pair<int, bool> AbletonCopilotAudioProcessorEditor::getSelectedKey() const
{
    const int selId = keyPicker.getSelectedId(); // 1-24: alternating major/minor per root
    if (selId <= 0)
        return { 0, false };
    return { (selId - 1) / 2, (selId - 1) % 2 == 1 };
}

void AbletonCopilotAudioProcessorEditor::exportMelodyPattern()
{
    const auto [root, isMinor] = getSelectedKey();

    // Push every melody track's pattern + the shared key root straight into
    // its own hosted Serum2 voice — all in-process.
    const int numTracks = melodyGrid.getNumTracks();
    for (int t = 0; t < numTracks; ++t)
        processor.setMelodyPattern(t, melodyGrid.getTrackOffsets(t), root);

    if (numTracks == 0)
        return;

    // Handoff file for the AbletonCopilotMIDI companion plugin (largely
    // superseded by in-process hosting) — track 0 only, same scope as
    // before this pass introduced multiple tracks.
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("AbletonCopilot");
    if (!dir.exists())
        dir.createDirectory();

    auto offsets = melodyGrid.getTrackOffsets(0);

    juce::String j;
    j << "{\n"
      << "  \"keyRoot\": " << root << ",\n"
      << "  \"isMinor\": " << (isMinor ? "true" : "false") << ",\n"
      << "  \"steps\": [";
    for (int i = 0; i < (int) offsets.size(); ++i)
    {
        if (offsets[(size_t) i] == MelodyGridComponent::kMelodyOff)
            j << "null";
        else
            j << (int) offsets[(size_t) i];
        if (i + 1 < (int) offsets.size())
            j << ",";
    }
    j << "]\n}\n";

    dir.getChildFile("melody_pattern.json").replaceWithText(j);
}

//==============================================================================
// Melody-track panel logic: preset scanning/cycling, prompt-driven
// generation, per-track Serum2 window.

void AbletonCopilotAudioProcessorEditor::addMelodyTrackClicked()
{
    if (melodyPanels.size() >= AbletonCopilotAudioProcessor::kMaxMelodyTracks)
        return;

    const int trackIndex = processor.addMelodyTrack();
    if (trackIndex < 0)
        return;

    melodyGrid.addTrack("NEW", kMisc);

    auto* panel = melodyPanels.add(new MelodyTrackPanel(*this, trackIndex));
    processor.setMelodyTrackCategory(trackIndex, panel->category);
    mainContent.addAndMakeVisible(panel);
    scanPresetsForTrack(*panel);
    updateTrackTitle(*panel);

    resized();
}

namespace
{
    juce::File capturedPresetsDir(MelodyCategory category)
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("AbletonCopilot/CapturedPresets")
                   .getChildFile(categoryDisplayName(category));
    }
}

void AbletonCopilotAudioProcessorEditor::scanPresetsForTrack(MelodyTrackPanel& panel)
{
    panel.presetFiles.clear();
    panel.presetIndex = -1;

    auto dir = capturedPresetsDir(panel.category);
    if (!dir.isDirectory())
        return;

    for (const auto& entry : juce::RangedDirectoryIterator(dir, false, "*.serumstate", juce::File::findFiles))
        panel.presetFiles.add(entry.getFile());

    std::sort(panel.presetFiles.begin(), panel.presetFiles.end(),
               [](const juce::File& a, const juce::File& b)
               {
                   return a.getFileName().compareIgnoreCase(b.getFileName()) < 0;
               });

    panel.presetIndex = panel.presetFiles.isEmpty() ? -1 : 0;
}

void AbletonCopilotAudioProcessorEditor::cyclePresetForTrack(MelodyTrackPanel& panel, int direction)
{
    if (panel.presetFiles.isEmpty())
        return;

    const int n = panel.presetFiles.size();
    const int previousIndex = panel.presetIndex;
    const int candidateIndex = ((panel.presetIndex + direction) % n + n) % n;
    panel.presetIndex = candidateIndex;

    if (!processor.loadCapturedPreset(panel.trackIndex, panel.presetFiles.getReference(candidateIndex)))
    {
        // Revert - the candidate was never actually applied to Serum 2, so
        // presetIndex must not be left pointing at it (that would let a
        // later serumStatusFor/updateTrackTitle call report a preset name
        // that was never loaded). lastConfirmedPresetName is untouched.
        panel.presetIndex = previousIndex;
        panel.statusLabel.setText("Couldn't load preset (is Serum 2 loaded yet?)", juce::dontSendNotification);
        return;
    }

    auto name = panel.presetFiles.getReference(candidateIndex).getFileNameWithoutExtension();
    panel.lastConfirmedPresetName = name;
    melodyGrid.setTrackPresetName(panel.trackIndex, name);
    updateTrackTitle(panel);
}

void AbletonCopilotAudioProcessorEditor::captureCurrentSound(MelodyTrackPanel& panel)
{
    juce::MemoryBlock state;
    if (!processor.captureMelodyTrackState(panel.trackIndex, state))
    {
        panel.statusLabel.setText("Nothing to capture yet (is Serum 2 loaded?)", juce::dontSendNotification);
        return;
    }

    auto* aw = new juce::AlertWindow("Capture Sound",
                                      "Name this sound (open Serum 2 and pick something there first):",
                                      juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor("name", "My " + categoryDisplayName(panel.category), "Name");
    aw->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    aw->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, &panel, aw, state](int result)
        {
            std::unique_ptr<juce::AlertWindow> owned(aw); // deleteWhenDismissed not used — clean up here instead

            if (result != 1)
                return;

            auto name = aw->getTextEditorContents("name").trim();
            if (name.isEmpty())
                return;

            // Strip filesystem-unsafe characters.
            juce::String safeName;
            for (auto c : name)
                safeName << (juce::CharacterFunctions::isLetterOrDigit(c) || c == ' ' || c == '-' || c == '_'
                                 ? juce::String::charToString(c) : juce::String("_"));

            auto dir = capturedPresetsDir(panel.category);
            dir.createDirectory();

            auto file = dir.getChildFile(safeName + ".serumstate");
            file.replaceWithData(state.getData(), state.getSize());

            scanPresetsForTrack(panel);
            auto foundIt = std::find(panel.presetFiles.begin(), panel.presetFiles.end(), file);
            if (foundIt != panel.presetFiles.end())
                panel.presetIndex = (int) std::distance(panel.presetFiles.begin(), foundIt);

            // Real capture, just written to disk and already confirmed
            // active in Serum 2 by processor.captureMelodyTrackState above
            // - safe to confirm.
            panel.lastConfirmedPresetName = safeName;
            melodyGrid.setTrackPresetName(panel.trackIndex, safeName);
            updateTrackTitle(panel);
        }), false);
}

void AbletonCopilotAudioProcessorEditor::generateForTrack(MelodyTrackPanel& panel)
{
    // Re-wired for the independent-generation architecture (see the plan
    // this was built from) - this used to call melodyGrid.generateForTrack
    // (the OLD manual-editing-grid path), which writes into
    // MelodyVoice::offsets. That array has been dead for actual playback
    // ever since the loop-generator workflow shipped: processBlock's
    // trigger loop always prefers non-empty MelodyVoice::generatedOffsets
    // over offsets (see useGeneratedPattern), so the old per-track
    // Generate button silently did nothing audible. It now branches on
    // trackIndex and calls the REAL generator for that category
    // (generateBassClicked/generateMelodyClicked - see those functions'
    // own comments), reusing this already-wired button/click-handler
    // rather than adding new UI elements.
    if (panel.trackIndex == 0)
        generateBassClicked();
    else if (panel.trackIndex == 1)
        generateMelodyClicked();
    else
    {
        // Pad (track 2, or any further track): no generator exists this
        // phase (see GrooveLoop.h's own header comment) - an honest
        // status message, not a silent no-op and not invented content.
        panel.statusLabel.setText("No pad generator yet this phase - Pad plays silent MIDI (see PluginEditor::refreshLoopDisplay).",
                                   juce::dontSendNotification);
        return;
    }

    // Auto-apply whichever REAL captured preset this panel was already
    // cycled to (unrelated to what generates the MIDI pattern above) -
    // preserved unchanged from the old behavior. Never fabricates a
    // preset - loadCapturedPreset only ever applies a real, previously
    // captured Serum2 state.
    if (!panel.presetFiles.isEmpty())
    {
        if (panel.presetIndex < 0)
            panel.presetIndex = 0;

        if (processor.loadCapturedPreset(panel.trackIndex, panel.presetFiles.getReference(panel.presetIndex)))
        {
            auto name = panel.presetFiles.getReference(panel.presetIndex).getFileNameWithoutExtension();
            panel.lastConfirmedPresetName = name;
        }
        // On failure, lastConfirmedPresetName (and whatever it held
        // before) is left untouched - updateTrackTitle below must not
        // claim this candidate loaded just because we tried it.
    }

    updateTrackTitle(panel);
}

void AbletonCopilotAudioProcessorEditor::openSerumWindowForTrack(MelodyTrackPanel& panel)
{
    auto* serum = processor.getHostedSerumInstance(panel.trackIndex);
    if (serum == nullptr)
        return;

    if (panel.serumWindow == nullptr)
    {
        if (!serum->hasEditor())
            return;

        auto* editorComp = serum->createEditorAndMakeActive();
        if (editorComp == nullptr)
            return;

        class SerumWindow : public juce::DocumentWindow
        {
        public:
            using DocumentWindow::DocumentWindow;
            void closeButtonPressed() override { setVisible(false); }
        };

        auto window = std::make_unique<SerumWindow>("Serum 2 \xe2\x80\x94 " + categoryDisplayName(panel.category),
                                                      juce::Colours::black,
                                                      juce::DocumentWindow::closeButton);
        window->setUsingNativeTitleBar(true);
        window->setContentOwned(editorComp, true);
        window->setResizable(true, false);
        window->centreWithSize(editorComp->getWidth(), editorComp->getHeight());
        panel.serumWindow = std::move(window);
    }

    panel.serumWindow->setVisible(true);
    panel.serumWindow->toFront(true);
}

void AbletonCopilotAudioProcessorEditor::updateTrackTitle(MelodyTrackPanel& panel)
{
    const bool capturedActive = processor.getMelodyVoiceDiagnostics(panel.trackIndex).capturedPresetActive;
    panel.titleLabel.setText(
        SerumPresetStatus::titleText(categoryDisplayName(panel.category), panel.lastConfirmedPresetName, capturedActive),
        juce::dontSendNotification);
}

void AbletonCopilotAudioProcessorEditor::runDrumPatternGeneration()
{
    if (drumPatternRenderer.isRendering())
        return;

    std::vector<DrumHitRow> hitRows;
    for (int i = 0; i < drumMachine.getNumRows(); ++i)
    {
        if (drumMachine.isRowMuted(i))
            continue;

        DrumHitRow row;
        row.elementName = drumMachine.getRowName(i);
        row.sampleFile   = drumMachine.getSampleForRow(i);
        row.steps        = drumMachine.getRowSteps(i);
        hitRows.push_back(std::move(row));
    }

    float bpm = processor.currentBpm.load(std::memory_order_relaxed);
    if (bpm <= 0.0f)
        bpm = 124.0f; // typical melodic techno tempo when the host isn't reporting one

    generateStackButton.setEnabled(false);
    showStackStatus("Rendering melodic techno pattern...");

    drumPatternRenderer.render(std::move(hitRows), bpm,
        [this](bool ok, const juce::String& log, std::vector<StackStem> stems)
        {
            generateStackButton.setEnabled(true);

            if (!ok)
            {
                showStackStatus(log.isNotEmpty() ? log
                                                  : "No samples assigned yet — choose a sample library first.");
                return;
            }

            StackSection section;
            section.display = "Melodic Techno";
            section.stems   = std::move(stems);
            stackBrowser.setSections({ std::move(section) });
            showStackStatus("Pattern rendered — drag the stems onto tracks in Ableton.");
        });
}
