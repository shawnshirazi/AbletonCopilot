#include "ArrangementAdvisor.h"
#include <cmath>

namespace
{
    // Matches MelodyGridComponent::kMelodyOff — kept local, same convention
    // already used in MelodyCritic.cpp/TheoryAdvisor.cpp.
    constexpr int8_t kNoNote = -128;

    bool isMelodicHookCategory(MelodyCategory c)
    {
        return c == MelodyCategory::Lead || c == MelodyCategory::Pluck || c == MelodyCategory::Synth;
    }
}

ArrangementResult ArrangementAdvisor::analyze(const std::vector<MelodyTrackContext>& tracks,
                                               const std::vector<DrumRowContext>& drumRows,
                                               MelodicTechnoTheory::SongSection section) const
{
    juce::ignoreUnused(drumRows); // no current check uses the drum grid - kept for future use

    ArrangementResult out;
    checkMissingHook(tracks, section, out);
    return out;
}

void ArrangementAdvisor::checkMissingHook(const std::vector<MelodyTrackContext>& tracks,
                                           MelodicTechnoTheory::SongSection section,
                                           ArrangementResult& out) const
{
    using MelodicTechnoTheory::SongSection;
    if (section != SongSection::Drop)
        return; // a hook is a Drop-section expectation, not universal

    constexpr int kMinActiveSteps = 4;

    for (auto& track : tracks)
    {
        if (!isMelodicHookCategory(track.category))
            continue;

        int active = 0;
        for (auto o : track.offsets)
            if (o != kNoNote)
                ++active;

        if (active >= kMinActiveSteps)
        {
            out.informational.push_back({ Severity::Good, "ARRANGEMENT",
                "Drop has a hook", track.label + " is carrying real melodic content in this section.",
                {} });
            return;
        }
    }

    out.needsHook = true;
    out.informational.push_back({ Severity::Warning, "ARRANGEMENT",
        "Drop has no lead/pluck/synth hook",
        "Only bass and drums appear to have real content for this section - a Drop usually wants "
        "one melodic element carrying the hook.",
        "Approve below to add a new Lead track with a generated part." });
}
