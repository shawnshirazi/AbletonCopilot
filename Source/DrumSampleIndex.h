#pragma once
#include <JuceHeader.h>
#include "DrumSampleAnalysis.h"
#include "RackBrowserComponent.h" // Rack, RackEntry - the same scanned-library data already feeding the manual drum grid
#include <functional>
#include <map>
#include <vector>

// One sample already classified into a rack (RackBrowserComponent) plus
// its measured DSP features (DrumSampleAnalysis). Ranking/selection
// (DrumSampleSelector.h) works on these, never on raw files.
struct IndexedSample
{
    juce::File                 file;
    juce::String               rackId; // KICK/SNARE/CLAP/HIHAT/PERC/... (RackBrowserComponent's classification)
    Engine::DrumSampleFeatures features;
};

// Background-thread analysis of the drum-role samples RackBrowserComponent
// already found and classified - never re-classifies anything and never
// scans the filesystem itself (that stays RackBrowserComponent's job).
// Only KICK/SNARE/CLAP/HIHAT/PERC candidates are analyzed - loops, FX,
// vocals, bass, and misc are skipped entirely, keeping the analyzed set
// to the low hundreds/thousands even though the library has 20k+ files.
//
// Results are cached to disk keyed by (path, file size, modification
// time), so a file that hasn't changed since the last scan is never
// re-analyzed - the plugin only pays the real DSP cost once per sample,
// not once per load. Runs entirely on its own thread; never touches the
// audio thread or blocks plugin construction.
class DrumSampleIndex : private juce::Thread
{
public:
    DrumSampleIndex();
    ~DrumSampleIndex() override;

    // Kicks off (or re-kicks-off, cancelling any in-progress scan) a scan
    // of the given racks. Safe to call repeatedly as the library rescans.
    void analyzeRacks(const std::vector<Rack>& racks);

    // Fires on the message thread once a scan completes.
    std::function<void(const std::vector<IndexedSample>&)> onIndexReady;

private:
    void run() override;

    juce::File cacheFile() const;
    void saveCache(const std::vector<IndexedSample>& samples) const;

    std::vector<Rack> pendingRacks;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumSampleIndex)
};
