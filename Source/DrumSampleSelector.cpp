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

    // exclude: when non-null, drops any candidate whose file matches it
    // before ranking - how percB (see selectDrumSamples) avoids landing on
    // the exact same file percA already claimed, without needing a second
    // pass or touching Engine::selectSampleIndex's own seeded-pick logic.
    // If excluding would empty the pool entirely (a single-candidate
    // pool), the exclusion is skipped rather than returning no candidate -
    // a real second sample, even if it has to be the same one, beats
    // silence.
    DrumSampleChoice pick(Engine::DrumRole role, std::vector<const IndexedSample*> candidates,
                           uint32_t seed, double bpm, const juce::File* exclude = nullptr)
    {
        DrumSampleChoice choice;

        if (exclude != nullptr && candidates.size() > 1)
        {
            std::vector<const IndexedSample*> filtered;
            filtered.reserve(candidates.size());
            for (auto* c : candidates)
                if (c->file != *exclude)
                    filtered.push_back(c);
            if (!filtered.empty())
                candidates = std::move(filtered);
        }

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
    sel.kick      = pick(Engine::DrumRole::Kick,      candidatesForRackIds(indexed, { "KICK" }), seed, bpm);
    sel.clap      = pick(Engine::DrumRole::Clap,      candidatesForRackIds(indexed, { "CLAP", "SNARE" }), seed, bpm);
    sel.hatClosed = pick(Engine::DrumRole::HatClosed, candidatesForRackIds(indexed, { "HIHAT" }), seed, bpm);
    sel.hatOpen   = pick(Engine::DrumRole::HatOpen,   candidatesForRackIds(indexed, { "OPEN_HAT", "RIDE" }), seed, bpm);

    sel.percA = pick(Engine::DrumRole::PercA, candidatesForRackIds(indexed, { "PERC" }), seed, bpm);
    // percB draws from the SAME pool as percA but is explicitly forced
    // away from percA's exact file (see pick's `exclude` parameter) - a
    // different salt (DrumSampleSelection.cpp's roleSalt) already usually
    // picks a different index, but "usually" isn't good enough for "two
    // complementary percussion voices, not one role silently duplicated
    // onto two" (see DrumEngine.cpp's percB motif, which assumes a real
    // second sample).
    sel.percB = pick(Engine::DrumRole::PercB, candidatesForRackIds(indexed, { "PERC" }), seed, bpm, &sel.percA.file);
    return sel;
}
