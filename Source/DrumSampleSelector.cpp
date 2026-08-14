#include "DrumSampleSelector.h"

namespace
{
    const std::vector<RackEntry>* findRackSamples(const std::vector<Rack>& racks, const juce::String& id)
    {
        for (auto& r : racks)
            if (r.id == id)
                return &r.samples;
        return nullptr;
    }

    DrumSampleChoice pick(Engine::DrumRole role, std::initializer_list<const std::vector<RackEntry>*> pools,
                           uint32_t seed)
    {
        // Concatenate every matching pool (e.g. CLAP + SNARE for Clap)
        // into one candidate list, then hand the count to the
        // deterministic picker - no candidate ever gets special treatment
        // over another (no genre/quality scoring, per the "avoid
        // destructive filtering or arbitrary EQ assumptions" instruction).
        std::vector<const RackEntry*> candidates;
        for (auto* pool : pools)
            if (pool != nullptr)
                for (auto& entry : *pool)
                    candidates.push_back(&entry);

        DrumSampleChoice choice;
        choice.poolSize = (int) candidates.size();

        const int idx = Engine::selectSampleIndex(role, seed, choice.poolSize);
        if (idx >= 0)
            choice.file = candidates[(size_t) idx]->file;

        return choice;
    }
}

DrumSampleSelection selectDrumSamples(const std::vector<Rack>& racks, uint32_t seed)
{
    DrumSampleSelection sel;
    sel.kick = pick(Engine::DrumRole::Kick, { findRackSamples(racks, "KICK") }, seed);
    sel.clap = pick(Engine::DrumRole::Clap, { findRackSamples(racks, "CLAP"), findRackSamples(racks, "SNARE") }, seed);
    sel.hat  = pick(Engine::DrumRole::Hat,  { findRackSamples(racks, "HIHAT") }, seed);
    sel.perc = pick(Engine::DrumRole::Perc, { findRackSamples(racks, "PERC") }, seed);
    return sel;
}
