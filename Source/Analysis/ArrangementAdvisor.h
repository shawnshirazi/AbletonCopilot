#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <optional>
#include "FeedbackEngine.h" // FeedbackItem/Severity
#include "TheoryAdvisor.h"  // MelodyTrackContext
#include "../MusicTheory/MelodicTechnoTheory.h"

// One drum row's actual step pattern — real ground truth (not audio
// inference), same spirit as MelodyTrackContext. Assembled by PluginEditor
// from DrumMachineComponent::getRowRackId/getRowSteps.
struct DrumRowContext
{
    juce::String           rackId; // e.g. "KICK"
    std::array<bool, 128>  steps {};
};

struct ArrangementResult
{
    std::vector<FeedbackItem>       informational;
    bool                            needsHook = false; // Drop section, no melodic hook present
};

// Cross-track, whole-arrangement analysis — the piece neither TheoryAdvisor
// (audio + library, per-track) nor MelodyCritic (one track at a time) does:
// looking at the melody grids AND the real drum step patterns together, and
// noticing what's missing across the arrangement as a whole. Purely
// symbolic (no audio needed), same instant/no-wait category as MelodyCritic.
// Read-only: describes issues and enough data to act on them, never applies
// anything itself.
class ArrangementAdvisor
{
public:
    ArrangementResult analyze(const std::vector<MelodyTrackContext>& tracks,
                               const std::vector<DrumRowContext>& drumRows,
                               MelodicTechnoTheory::SongSection section) const;

private:
    void checkMissingHook(const std::vector<MelodyTrackContext>&, MelodicTechnoTheory::SongSection,
                           ArrangementResult&) const;
};
