#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UIStyle.h"
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
    // Shorter window while the manual drum grid/melody grid/Serum panels
    // are hidden (see kShowFullUI) - no point reserving space for a large
    // scrollable area with nothing visible in it. 412 fits the header,
    // Generate Drum Pattern row, the status block (now tall enough for
    // one line per role's actual loaded sample path - see
    // generateDrumPatternClicked), and the generated pattern grid + legend
    // with a little breathing room; flip kShowFullUI back to restore the
    // full 980x760 layout.
    setSize(980, kShowFullUI ? 1044 : 696); // +170 for the drum-decode diagnostic block, +114 for the library-scan diagnostic block, see their setBounds comments

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

    // Phase 1 deterministic engine - AbletonCopilot outputs real MIDI directly
    // to the host, mixed straight into AbletonCopilot's own audio output -
    // no Drum Rack or other instrument required to hear it.
    generateDrumPatternButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    generateDrumPatternButton.setColour(juce::TextButton::textColourOffId, kAccent);
    generateDrumPatternButton.onClick = [this] { generateDrumPatternClicked(); };
    addAndMakeVisible(generateDrumPatternButton);
    updateGenerateButtonAvailability(); // starts disabled - the background sample scan hasn't run yet at this point in the constructor

    libraryScanStatusLabel.setFont(small());
    libraryScanStatusLabel.setJustificationType(juce::Justification::topLeft);
    libraryScanStatusLabel.setColour(juce::Label::textColourId, kTextDim);
    addAndMakeVisible(libraryScanStatusLabel);
    updateLibraryScanStatusLabel(); // initial snapshot - everything "no"/pending until the scan actually progresses

    drumPatternStatusLabel.setFont(small()); // smaller font: room for 4 full sample paths without ballooning the window
    drumPatternStatusLabel.setJustificationType(juce::Justification::topLeft); // long multi-line diagnostic block reads top-down, not vertically centred
    drumPatternStatusLabel.setColour(juce::Label::textColourId, kTextDim);
    drumPatternStatusLabel.setText(
        "Not generated yet.", juce::dontSendNotification);
    addAndMakeVisible(drumPatternStatusLabel);
    addAndMakeVisible(generatedDrumGrid);

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

    // Everything below the fixed header/genre-key row scrolls as one unit —
    // set up the outer viewport before adding any of its children.
    mainViewport.setViewedComponent(&mainContent, false);
    mainViewport.setScrollBarsShown(true, false); // vertical only
    addAndMakeVisible(mainViewport);
    // Manual drum grid, melody grid, and Serum 2 track panels all live
    // inside mainContent/mainViewport - hidden together for the
    // drum-generation milestone (see kShowFullUI). Nothing underneath is
    // disconnected: onStepToggled/onSampleCycled/etc. still push to the
    // processor exactly as before, and the background library/preset
    // scans below still populate drumMachine's rows - there's just no
    // visible surface to see or interact with them on right now.
    mainViewport.setVisible(kShowFullUI);

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
            if (s.rackId == "HIHAT")                        ++hatCandidateCount;
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
    // hidden alongside mainViewport below (see kShowFullUI).
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

    // Phase 1 deterministic engine row(s) - always visible (not gated by
    // kShowExperimentalFeatures). Status label gets its own multi-line
    // block below the button (temporary end-to-end diagnostics - several
    // lines of text, doesn't fit next to the button). This is the focus
    // of the drum-generation milestone, so it's the one section never
    // hidden by kShowFullUI.
    auto drumEngineRow = area.removeFromTop(26).reduced(12, 0);
    generateDrumPatternButton.setBounds(drumEngineRow.removeFromLeft(180));
    area.removeFromTop(4);
    // Temporary (110px): library-scan pipeline diagnostics (see
    // updateLibraryScanStatusLabel) - always visible, not gated behind a
    // Generate click, since the whole point is showing why Generate can't
    // be clicked yet. Shrink/remove once the scan-pipeline milestone
    // is done.
    libraryScanStatusLabel.setBounds(area.removeFromTop(110).reduced(12, 0));
    area.removeFromTop(4);
    // Temporarily tall (320px, up from 150px): the bug-hunting diagnostic
    // block below prints ~4 lines per role (pool size, candidate path,
    // exists/extension/format/decoder/length, final loaded-or-fallback) -
    // shrink this back once the SYNTH FALLBACK root cause is fixed and
    // the status returns to a short summary.
    drumPatternStatusLabel.setBounds(area.removeFromTop(320).reduced(12, 0));
    area.removeFromTop(4);
    generatedDrumGrid.setBounds(area.removeFromTop(GeneratedDrumGridComponent::kRequiredHeight).reduced(12, 0));
    area.removeFromTop(8);

    // Reference-track row(s) - hidden while kShowExperimentalFeatures is
    // off (same reasoning as the tab strip above).
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

    // Everything else — drum grid, melody grid, Serum 2 track panels —
    // scrolls as a single unit, sized to however much content actually
    // exists rather than growing the window itself. Hidden for the
    // drum-generation milestone (see kShowFullUI) - skip laying it out
    // entirely rather than just leaving it invisible with stale bounds.
    if (kShowFullUI)
    {
        mainViewport.setBounds(area);

        const int contentWidth = juce::jmax(200, area.getWidth() - 16); // room for the vertical scrollbar
        const int innerWidth   = contentWidth - 24;
        int y = 12;

        const int drumGridHeight = DrumMachineComponent::kRowHeight * drumMachine.getNumRows();
        drumMachine.setBounds(12, y, innerWidth, drumGridHeight);
        y += drumGridHeight + 16;

        const int melodyGridHeight = MelodyGridComponent::kRowHeight * melodyGrid.getNumTracks();
        melodyGrid.setBounds(12, y, innerWidth, melodyGridHeight);
        y += melodyGridHeight + 16;

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

        mainContent.setSize(contentWidth, y);
    }
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
        generateDrumPatternButton.setEnabled(false);
        generateDrumPatternButton.setButtonText("No Sample Library");
    }
    else if (!sampleIndexReady)
    {
        generateDrumPatternButton.setEnabled(false);
        generateDrumPatternButton.setButtonText("Scanning Library...");
    }
    else
    {
        generateDrumPatternButton.setEnabled(true);
        generateDrumPatternButton.setButtonText("Generate Drum Pattern");
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
      << ", onIndexReady=" << (onIndexReadyFired ? "fired" : "not fired");

    libraryScanStatusLabel.setText(s, juce::dontSendNotification);
}

void AbletonCopilotAudioProcessorEditor::generateDrumPatternClicked()
{
    const Engine::StepGridConfig grid; // defaults: 16 steps/bar, 16 bars = 256 steps
    juce::Random rng;

    auto freshParams = [&rng]
    {
        Engine::DrumPatternParams p;
        p.density     = 0.5f;
        p.syncopation = 0.3f;
        p.variation   = 0.2f;
        p.seed        = (uint32_t) rng.nextInt();
        return p;
    };

    struct RoleExport { const char* name; int midiNote; juce::Colour colour; Engine::StepArray steps; };
    // General MIDI drum map note numbers - a real, recognized convention
    // (Bass Drum 1, Hand Clap, Closed Hi-Hat, Side Stick) so this lines up
    // with most drum racks/instruments by default.
    std::vector<RoleExport> roles;
    roles.push_back({ "KICK", 36, UIStyle::kKick,  Engine::generateKick(grid, freshParams()) });
    roles.push_back({ "CLAP", 39, UIStyle::kClap,  Engine::generateClap(grid, freshParams()) });
    roles.push_back({ "HAT",  42, UIStyle::kHihat, Engine::generateHat (grid, freshParams()) });
    roles.push_back({ "PERC", 37, UIStyle::kPerc,  Engine::generatePerc(grid, freshParams()) });

    // Hand the pattern straight to the processor - it plays it back as real
    // audio (DrumVoiceSynth, see PluginProcessor.cpp) and, secondarily, as
    // MIDI. Also feed generatedDrumGrid so the UI shows exactly the same
    // pattern - both consumers are built from the SAME toVelocityArray()
    // result per role (Engine::toVelocityArray, DrumEngine.h), never two
    // independently-derived patterns.
    std::vector<AbletonCopilotAudioProcessor::GeneratedDrumRole> processorRoles;
    std::vector<GeneratedDrumGridComponent::RowDisplay> displayRows;
    juce::StringArray summaryParts;
    int totalHits = 0;

    // Sample-selection layer (Source/DrumSampleSelector.h): decides WHICH
    // FILE, never WHEN/WHERE/how loud - that stays entirely DrumEngine's
    // job above. One seed drawn here (same pattern as each role's own
    // DrumPatternParams::seed) so the same seed always picks the same
    // samples; a fresh Generate click still explores new choices, same as
    // it already explores new patterns.
    const uint32_t sampleSeed = (uint32_t) rng.nextInt();
    const double   bpmForSelection = (double) processor.currentBpm.load(std::memory_order_relaxed);
    const DrumSampleSelection sampleSel = selectDrumSamples(latestSampleIndex, sampleSeed, bpmForSelection);

    auto choiceForRole = [&](const char* name) -> const DrumSampleChoice&
    {
        if (juce::String(name) == "KICK") return sampleSel.kick;
        if (juce::String(name) == "CLAP") return sampleSel.clap;
        if (juce::String(name) == "HAT")  return sampleSel.hat;
        return sampleSel.perc;
    };

    for (auto& role : roles)
    {
        std::vector<int> velocity = Engine::toVelocityArray(role.steps);
        const int activeCount = (int) std::count_if(velocity.begin(), velocity.end(),
                                                      [](int v) { return v > 0; });

        const DrumSampleChoice& choice = choiceForRole(role.name);

        AbletonCopilotAudioProcessor::GeneratedDrumRole pr;
        pr.midiNote   = role.midiNote;
        pr.velocity   = velocity;
        pr.sampleFile = choice.file; // invalid File() -> PluginProcessor falls back to DrumVoiceSynth for this role
        processorRoles.push_back(std::move(pr));

        GeneratedDrumGridComponent::RowDisplay dr;
        dr.name     = role.name;
        dr.colour   = role.colour;
        dr.velocity = std::move(velocity);
        displayRows.push_back(std::move(dr));

        totalHits += activeCount;
        summaryParts.add(juce::String(activeCount) + " " + juce::String(role.name).toLowerCase());
    }

    processor.setGeneratedDrumPattern(processorRoles);
    generatedDrumGrid.setPattern(std::move(displayRows), grid.stepsPerBar, grid.numBars);

    juce::String status;
    status << "Generated - " << summaryParts.joinIntoString(" / ") << " hits (" << totalHits
           << " total) over " << grid.numBars << " bars.\n";
    status << "Playing directly from AbletonCopilot - start Ableton's transport to hear it. "
              "No Drum Rack or other instrument required.\n";

    // Bug-hunting diagnostic block (temporary, "trace the runtime audio
    // path" milestone): for each role, show every step actually checked -
    // the selector's pool size (0 here means the sample index had no
    // candidates for this role AT ALL, which is a completely different
    // failure than "a candidate was chosen but wouldn't decode" below),
    // then the exact file/extension/format/decoder/length checks
    // PluginProcessor::setGeneratedDrumPattern() performed, queried back
    // via getGeneratedRoleLoadDiagnostics() - not reconstructed here, so
    // this can't drift from what setGeneratedDrumPattern() actually did.
    // This call is synchronous with setGeneratedDrumPattern() just above.
    status << "Sample index: " << (int) latestSampleIndex.size() << " analyzed samples total.\n";
    for (auto& role : roles)
    {
        Engine::DrumRole resolvedRole;
        if (!Engine::drumRoleForGmNote(role.midiNote, resolvedRole))
            continue;

        const DrumSampleChoice& choice = choiceForRole(role.name);
        const auto d = processor.getGeneratedRoleLoadDiagnostics(resolvedRole);

        const juce::String label = juce::String(role.name).toLowerCase().substring(0, 1).toUpperCase()
                                  + juce::String(role.name).toLowerCase().substring(1);
        status << label << ": pool=" << choice.poolSize << " shortlist=" << choice.shortlistSize << "\n";
        status << "  candidate: " << (d.candidateFile.getFullPathName().isEmpty() ? "(none)" : d.candidateFile.getFullPathName()) << "\n";
        status << "  exists=" << (d.candidateExists ? "yes" : "no")
               << " ext=" << (d.extension.isEmpty() ? "(none)" : d.extension)
               << " formatRecognized=" << (d.formatRecognized ? d.recognizedFormatName : juce::String("no"))
               << " readerCreated=" << (d.readerCreated ? "yes" : "no")
               << " decodedLength=" << (juce::int64) d.decodedLengthSamples << " samples\n";
        status << "  " << label << ": ";
        if (d.finalLoadedFile.existsAsFile())
            status << d.finalLoadedFile.getFullPathName();
        else
            status << "SYNTH FALLBACK";
        status << "\n";
    }

    drumPatternStatusLabel.setText(status, juce::dontSendNotification);
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
    panel.presetIndex = ((panel.presetIndex + direction) % n + n) % n;

    if (!processor.loadCapturedPreset(panel.trackIndex, panel.presetFiles.getReference(panel.presetIndex)))
    {
        panel.statusLabel.setText("Couldn't load preset (is Serum 2 loaded yet?)", juce::dontSendNotification);
        return;
    }

    auto name = panel.presetFiles.getReference(panel.presetIndex).getFileNameWithoutExtension();
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

            melodyGrid.setTrackPresetName(panel.trackIndex, safeName);
            updateTrackTitle(panel);
        }), false);
}

void AbletonCopilotAudioProcessorEditor::generateForTrack(MelodyTrackPanel& panel)
{
    auto prompt   = panel.promptBox.getText().trim();
    auto category = parseCategoryFromPrompt(prompt);
    auto style    = parseStyleFromPrompt(prompt);

    if (category != panel.category)
    {
        panel.category = category;
        processor.setMelodyTrackCategory(panel.trackIndex, category);
        scanPresetsForTrack(panel);
    }

    const auto [root, isMinor] = getSelectedKey();
    melodyGrid.generateForTrack(panel.trackIndex, root, isMinor, category, style);
    melodyGrid.setTrackIdentity(panel.trackIndex, categoryDisplayName(category), categoryColour(category));

    if (!panel.presetFiles.isEmpty())
    {
        if (panel.presetIndex < 0)
            panel.presetIndex = 0;

        if (processor.loadCapturedPreset(panel.trackIndex, panel.presetFiles.getReference(panel.presetIndex)))
            melodyGrid.setTrackPresetName(panel.trackIndex,
                panel.presetFiles.getReference(panel.presetIndex).getFileNameWithoutExtension());
    }

    updateTrackTitle(panel);
    exportMelodyPattern();
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
    juce::String name = (panel.presetIndex >= 0 && panel.presetIndex < panel.presetFiles.size())
        ? panel.presetFiles.getReference(panel.presetIndex).getFileNameWithoutExtension()
        : "(no captured sounds yet — click Capture)";
    panel.titleLabel.setText(categoryDisplayName(panel.category) + "  \xe2\x80\x94  " + name,
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
