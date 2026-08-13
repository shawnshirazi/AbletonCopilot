#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <optional>
#include "FeatureExtractor.h"
#include "FeedbackEngine.h" // reuses FeedbackItem/Severity - same card model, no new display type needed
#include "../RackBrowserComponent.h" // Rack/RackEntry
#include "../PresetLibraryScanner.h" // PresetEntry
#include "../MelodyCategory.h"
#include "../MusicTheory/MelodicTechnoTheory.h"
#include "../MusicTheory/MelodyCritic.h" // MelodyEditSuggestion - reused for clash fixes, same action shape

// One melody track's real, current state — exact ground truth (not audio
// inference) for what this plugin's own tracks are actually playing, so the
// advisor can cross-reference what it hears against what's actually in the
// grids. Assembled by PluginEditor the same way buildMelodyEditSuggestions()
// already does.
struct MelodyTrackContext
{
    int                     trackIndex = -1;
    juce::String            label;
    MelodyCategory          category = MelodyCategory::Bass;
    std::array<int8_t, 128> offsets {};
};

struct AdvisorContext
{
    AudioFeatures                   features;
    std::vector<Rack>               racks;
    std::vector<PresetEntry>        presets;
    std::vector<MelodyTrackContext> tracks;
    int  keyRootSemitone = 0;
    bool isMinor          = true;
    MelodicTechnoTheory::SongSection section = MelodicTechnoTheory::SongSection::Drop;

    // Which GenreProfiles id to compare spectral targets against
    // (suggestSamples' sub/air targets) - "melodic_techno" normally, or
    // GenreProfiles::kReferenceProfileId when the user has loaded a
    // reference track (see PluginEditor::loadReferenceTrackClicked).
    juce::String profileId = "melodic_techno";
};

// This is a real, actionable target for the key picker - not just text.
struct KeyChangeSuggestion { int rootSemitone; bool isMinor; };

// Names a real file the owner can hand straight to
// DrumMachineComponent::setRowSampleByFile.
struct SampleAssignSuggestion { juce::String rackId; juce::File file; };

// Split so the owner can tell informational suggestions (nothing to do but
// read) apart from ones with a concrete, applicable action attached.
struct AdvisorResult
{
    std::vector<FeedbackItem>           informational;
    std::vector<MelodyEditSuggestion>   clashFixes;
    std::optional<KeyChangeSuggestion>  keyChange;
    std::vector<SampleAssignSuggestion> sampleAssigns;
};

// Melodic-techno-only listening advisor: turns already-detected AudioFeatures
// (key/BPM/spectral balance - see AudioAnalyzer/FeatureExtractor, already
// built and working) plus the user's real, already-scanned sample/preset
// libraries AND the plugin's own grid state into concrete suggestions - what
// key/chord move to try, groove/swing direction, specific samples/presets to
// reach for, cross-track note clashes, and a rotating production technique.
// Some of these are purely informational; others (a clashing note, a key
// mismatch, a sample to assign) carry enough data for the owner to build a
// real Approve action - this class only ever describes/computes the change,
// never applies it itself. Deliberately not genre-parameterized (unlike
// FeedbackEngine, which stays genre-general) - this class only knows melodic
// techno, per explicit scope for this feature.
class TheoryAdvisor
{
public:
    AdvisorResult generate(const AdvisorContext& ctx) const;

private:
    void suggestKeyAndProgression(const AdvisorContext&, AdvisorResult&) const;
    void suggestSection          (const AdvisorContext&, AdvisorResult&) const;
    void suggestGroove           (const AdvisorContext&, AdvisorResult&) const;
    void suggestSamples          (const AdvisorContext&, AdvisorResult&) const;
    void suggestPresets          (const AdvisorContext&, AdvisorResult&) const;
    void checkTrackClashes       (const AdvisorContext&, AdvisorResult&) const;
    void suggestTechnique        (const AdvisorContext&, AdvisorResult&) const;

    static FeedbackItem idea(const juce::String& category, const juce::String& headline,
                              const juce::String& detail, const juce::String& fix);
};
