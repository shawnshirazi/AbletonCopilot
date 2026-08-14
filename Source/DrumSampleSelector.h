#pragma once
#include <JuceHeader.h>
#include "RackBrowserComponent.h" // Rack, RackEntry - the same scanned-library data already feeding the manual drum grid
#include "Engine/DrumSampleSelection.h" // Engine::selectSampleIndex, DrumRole
#include <vector>

// Thin JUCE-side wrapper around Engine::selectSampleIndex - resolves the
// pure index it returns into an actual sample file from the user's own
// scanned library. No new sample-discovery mechanism: this reuses exactly
// the Rack/RackEntry data RackBrowserComponent already produces (the same
// background scan that feeds DrumMachineComponent's rows).
//
// Deliberately dumb on purpose - it has no idea what a step, a bar, or a
// velocity is. DrumEngine decides WHEN/WHERE/how loud; this decides WHICH
// FILE. See Engine/DrumSampleSelection.h for why that split matters.
struct DrumSampleChoice
{
    juce::File file;         // invalid (File()) if no candidates existed for this role
    int        poolSize = 0; // candidate count actually considered - 0 means the library has none for this role
};

struct DrumSampleSelection
{
    DrumSampleChoice kick, clap, hat, perc;
};

// racks = whatever RackBrowserComponent last scanned (PluginEditor's
// latestRacks). Clap draws from the CLAP and SNARE racks combined -
// melodic techno productions use them close to interchangeably for this
// role, and the manual grid's own kRowTemplates already treats clap/snare
// as adjacent categories. Kick/Hat/Perc each draw from their own single
// rack only, never borrowing from an unrelated category.
DrumSampleSelection selectDrumSamples(const std::vector<Rack>& racks, uint32_t seed);
