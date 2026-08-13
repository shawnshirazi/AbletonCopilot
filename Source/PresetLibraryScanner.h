#pragma once
#include <JuceHeader.h>
#include "MelodyCategory.h"
#include <vector>
#include <memory>
#include <atomic>

// One real preset file on disk from a synth's own library — Serum 2's
// *.SerumPreset or Diva's *.h2p. Both formats are confirmed non-loadable
// programmatically (Serum 2's real plugin state is "VC2!" + XML, unrelated
// to either file format — proven earlier this session via a byte-level
// diagnostic), so this is filename indexing for advisory suggestions only:
// "go load this yourself in that plugin's own browser," same manual
// workflow the Capture feature already assumes.
struct PresetEntry
{
    juce::String   pluginName; // "Serum 2" | "Diva"
    juce::String   name;       // file stem, e.g. "BS Filthy Bassline [Sam Smyers]"
    juce::File     file;
    MelodyCategory category;
};

// Background-thread scan of a sample-library folder for real preset files,
// mirroring RackBrowserComponent's existing scan pattern (Source/
// RackBrowserComponent.cpp) — same juce::Thread + recursive findChildFiles +
// message-thread callback shape, just indexing preset files instead of
// audio samples.
class PresetLibraryScanner : private juce::Thread
{
public:
    PresetLibraryScanner();
    ~PresetLibraryScanner() override;

    void setLibraryDir(const juce::File& dir);

    // Fires on the message thread whenever a scan finishes.
    std::function<void(const std::vector<PresetEntry>&)> onPresetsChanged;

private:
    void run() override;

    juce::File pendingDir;
    std::shared_ptr<std::atomic<bool>> aliveFlag { std::make_shared<std::atomic<bool>>(true) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetLibraryScanner)
};
