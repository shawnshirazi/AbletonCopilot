#include "DrumSampleSelector.h"
#include "Engine/DrumSampleSelection.h" // Engine::selectSampleIndex
#include "Engine/DrumSamplePackTier.h"  // Engine::classifyPackTier - Part C genre-aware ranking
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

    // Combines the sample's own measured-character score
    // (Engine::scoreForRole) with a confidence multiplier for the PACK it
    // came from (Engine::classifyPackTier) - two samples can measure
    // almost identically on duration/attack/ZCR/etc. while one is real
    // Melodic Techno material and the other is Tech House that happens to
    // share those numbers; pack identity is real information the DSP
    // features alone can't see. See Source/Engine/DrumSamplePackTier.h
    // for the tier weights and the actual pack-name evidence they're
    // drawn from.
    float combinedScore(Engine::DrumRole role, const IndexedSample& sample, double bpm)
    {
        const float dspScore  = Engine::scoreForRole(role, sample.features, bpm);
        const float tierScore = Engine::tierWeight(Engine::classifyPackTier(sample.file.getFullPathName().toStdString()).tier);
        return dspScore * tierScore;
    }

    // A real library audit (this session) found the OLD "multiply dspScore
    // by a tier weight, rank the whole pool together" approach wasn't
    // strong enough on its own: for the roles that matter most, real
    // Tier1/2 candidates are heavily outnumbered by Tier3/4 in the raw
    // pool (e.g. HIHAT: 160 Tier1+2 vs 741 Tier3+4; KICK: 172 vs 1033;
    // PERC: 133 vs 665 - measured via MLPipeline-style direct classifyPackTier
    // enumeration of the real library, not assumed) - a single Tier4
    // sample with an unusually good dspScore match could still land in the
    // top-5 shortlist purely on volume, no matter the multiplier. This
    // directly implements the requested fix instead: a real tier CASCADE
    // - try Tier1 alone first, then Tier1+2, then Tier1+2+3 (excluding
    // only confirmed off-genre Tier4), only falling through to the full
    // pool (Tier4 included) if nothing better exists at all. combinedScore
    // (acoustic fingerprint scoring) still decides the pick WITHIN
    // whichever tier tranche was selected - this doesn't replace that,
    // it just decides which population it's allowed to rank within first,
    // per "TIER 3 should only be used if Tier 1/2 has no viable candidate
    // ... within a tier, THEN use the existing acoustic fingerprint
    // scoring."
    constexpr int kMinTierPoolSize = 3; // enough for real shortlist variety, not just one "safe" pick

    std::vector<const IndexedSample*> preferBestAvailableTier(const std::vector<const IndexedSample*>& candidates)
    {
        auto tierOf = [](const IndexedSample* s)
        {
            return Engine::classifyPackTier(s->file.getFullPathName().toStdString()).tier;
        };
        auto poolAtOrBetterThan = [&](Engine::PackTier worstAllowed)
        {
            std::vector<const IndexedSample*> out;
            for (auto* c : candidates)
                if ((int) tierOf(c) <= (int) worstAllowed) // lower enum value = higher confidence tier
                    out.push_back(c);
            return out;
        };

        auto tier1 = poolAtOrBetterThan(Engine::PackTier::MelodicTechno);
        if ((int) tier1.size() >= kMinTierPoolSize)
            return tier1;

        auto tier12 = poolAtOrBetterThan(Engine::PackTier::AdjacentTechno);
        if ((int) tier12.size() >= kMinTierPoolSize)
            return tier12;

        auto tier123 = poolAtOrBetterThan(Engine::PackTier::OtherElectronic);
        if (!tier123.empty())
            return tier123;

        return candidates; // last resort - never leave a role with zero candidates
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

        // Tier-cascade FIRST (see preferBestAvailableTier's own comment) -
        // poolSize/shortlistSize below reflect the tranche actually used
        // for ranking, not the whole library's raw candidate count, so the
        // "pool=X/Y" diagnostic honestly shows what was really competed
        // over.
        candidates = preferBestAvailableTier(candidates);

        choice.poolSize = (int) candidates.size();
        if (candidates.empty())
            return choice;

        std::sort(candidates.begin(), candidates.end(), [&](const IndexedSample* a, const IndexedSample* b)
        {
            return combinedScore(role, *a, bpm) > combinedScore(role, *b, bpm);
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
        choice.score = combinedScore(role, *candidates[(size_t) idx], bpm);
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
