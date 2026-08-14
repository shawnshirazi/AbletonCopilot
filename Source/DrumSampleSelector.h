#pragma once
#include <JuceHeader.h>
#include "DrumSampleIndex.h" // IndexedSample
#include "Engine/DrumSampleScoring.h" // Engine::scoreForRole
#include <vector>

// JUCE-side ranking/selection wrapper around Engine::scoreForRole and
// Engine::selectSampleIndex. Deliberately dumb about musical timing (no
// idea what a step, a bar, or a pattern is) and about sound analysis (no
// idea how a sample got measured) - it only combines "how good is this
// candidate for this role" (Engine::scoreForRole) with "which of the
// good ones, deterministically" (Engine::selectSampleIndex).
struct DrumSampleChoice
{
    juce::File file;            // invalid (File()) if no candidates existed for this role
    int        poolSize     = 0; // candidates considered for this role (post role-filter, pre-ranking)
    int        shortlistSize = 0; // size of the top-ranked shortlist actually sampled from
    float      score        = 0.0f; // the chosen sample's own score, for diagnostics/reporting
};

struct DrumSampleSelection
{
    DrumSampleChoice kick, clap, hat, perc;
};

// indexed = whatever DrumSampleIndex last analyzed (PluginEditor's
// latestSampleIndex). Clap draws from the CLAP and SNARE racks combined -
// melodic techno productions use them close to interchangeably for this
// role. Kick/Hat/Perc each draw from their own single rack only, never
// borrowing from an unrelated category. bpm is passed straight to
// Engine::scoreForRole (Kick only actually uses it - see
// Engine/DrumSampleScoring.h).
DrumSampleSelection selectDrumSamples(const std::vector<IndexedSample>& indexed, uint32_t seed, double bpm);
