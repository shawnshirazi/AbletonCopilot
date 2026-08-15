#include "DrumSampleSelector.h"
#include "Engine/DrumSampleSelection.h" // Engine::selectSampleIndex
#include <algorithm>

namespace
{
    std::vector<const IndexedSample*> candidatesForRackIds(const std::vector<IndexedSample>& indexed,
                                                             std::initializer_list<const char*> rackIds)
    {
        std::vector<const IndexedSample*> out;
        for (auto& s : indexed)
        {
            if (!s.features.valid)
                continue; // couldn't be decoded/measured - never a candidate

            for (auto* id : rackIds)
                if (s.rackId == id)
                {
                    out.push_back(&s);
                    break;
                }
        }
        return out;
    }

    DrumSampleChoice pick(Engine::DrumRole role, std::vector<const IndexedSample*> candidates,
                           uint32_t seed, double bpm)
    {
        DrumSampleChoice choice;
        choice.poolSize = (int) candidates.size();
        if (candidates.empty())
            return choice;

        std::sort(candidates.begin(), candidates.end(), [&](const IndexedSample* a, const IndexedSample* b)
        {
            return Engine::scoreForRole(role, a->features, bpm) > Engine::scoreForRole(role, b->features, bpm);
        });

        // Top-ranked shortlist: at least 1, at most 5, and never more
        // than ~40% of the pool - keeps genuinely weak candidates out of
        // rotation without collapsing to literally one "best" file every
        // time (still want seed-driven variety among the candidates that
        // are actually good fits).
        const int shortlistSize = std::max(1, std::min({ 5, (int) candidates.size(),
                                                           (int) candidates.size() * 2 / 5 + 1 }));
        choice.shortlistSize = shortlistSize;

        const int idx = Engine::selectSampleIndex(role, seed, shortlistSize);
        choice.file  = candidates[(size_t) idx]->file;
        choice.score = Engine::scoreForRole(role, candidates[(size_t) idx]->features, bpm);
        return choice;
    }
}

DrumSampleSelection selectDrumSamples(const std::vector<IndexedSample>& indexed, uint32_t seed, double bpm)
{
    DrumSampleSelection sel;
    sel.kick = pick(Engine::DrumRole::Kick, candidatesForRackIds(indexed, { "KICK" }), seed, bpm);
    sel.clap = pick(Engine::DrumRole::Clap, candidatesForRackIds(indexed, { "CLAP", "SNARE" }), seed, bpm);
    sel.hat  = pick(Engine::DrumRole::Hat,  candidatesForRackIds(indexed, { "HIHAT", "OPEN_HAT" }), seed, bpm);
    sel.perc = pick(Engine::DrumRole::Perc, candidatesForRackIds(indexed, { "PERC" }), seed, bpm);
    return sel;
}
