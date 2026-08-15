#pragma once
#include <JuceHeader.h>
#include "RackClassification.h"
#include "StackBrowserComponent.h"
#include <map>
#include <vector>

// One sample library entry, classified into a rack (KICK, SNARE, ...).
struct RackEntry
{
    juce::String name;
    juce::File   file;
};

struct Rack
{
    juce::String          id;        // e.g. "KICK"
    juce::String          display;   // e.g. "Kicks"
    std::vector<RackEntry> samples;
};

//==============================================================================
// Left-hand sidebar: vertical list of racks (categories), XO-style.
class RackListComponent : public juce::Component
{
public:
    std::function<void(int)> onSelect;

    void setRacks(const std::vector<Rack>& newRacks);
    void setSelected(int index);

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    std::vector<Rack> racks;
    int selectedIndex = 0;
    static constexpr int kRowHeight = 32;
};

//==============================================================================
// Right-hand pane: draggable sample rows for the currently selected rack.
class RackContentComponent : public juce::Component
{
public:
    RackContentComponent();

    void setSamples(juce::String rackDisplayName, const std::vector<RackEntry>& entries);
    void resized() override;
    void paint(juce::Graphics&) override;

private:
    void rebuildRows();

    juce::String                       rackName;
    std::vector<RackEntry>             samples;
    juce::Viewport                     viewport;
    juce::Component                    content;
    juce::OwnedArray<StackRowComponent> rows;
};

//==============================================================================
// Combines the sidebar + content pane and scans a library folder on a
// background thread, grouping every sample into a rack by filename keyword.
class RackBrowserComponent : public juce::Component,
                              private juce::Thread
{
public:
    RackBrowserComponent();
    ~RackBrowserComponent() override;

    void setLibraryDir(const juce::File& dir);

    // Fires on the message thread whenever a library scan finishes.
    std::function<void(const std::vector<Rack>&)> onRacksChanged;

    // Fires on the message thread right after the recursive directory
    // walk finds its file list, BEFORE the (much slower, one
    // AudioFormatManager::createReaderFor() open per file) classification
    // pass starts - proves the scan is actually progressing rather than
    // stuck, and separates "the file-find step never returned" from "it's
    // still classifying" as two distinguishable, debuggable states.
    std::function<void(int)> onFilesDiscovered;

    void resized() override;
    void paint(juce::Graphics&) override;

    // Diagnostics for the (temporary) library-scan status label - true
    // once this scan's classification pass was served entirely from the
    // cache (see cacheFile()/run()), false if any file needed a fresh
    // classifySample() call (a first-ever scan, or the library changed).
    // Message-thread read only; written just before onRacksChanged fires.
    bool lastScanWasFullyCached() const noexcept { return lastScanFullyCached; }
    int  lastScanCachedFileCount() const noexcept { return lastScanCachedCount; }
    int  lastScanChangedFileCount() const noexcept { return lastScanChangedCount; }

private:
    void run() override;
    void applyRacks(std::vector<Rack> newRacks);
    void selectRack(int index);

    // Path+size+mtime-keyed cache of (rackId, duration) so a file that
    // hasn't changed since the last scan skips classifySample()'s real
    // cost entirely (a decoder open per file - the dominant cost of a
    // ~21k-file library scan, confirmed by inspection: this stage had NO
    // caching at all before, unlike DrumSampleIndex's own already-cached
    // deeper-analysis stage downstream of it) - mirrors
    // DrumSampleIndex::cacheFile()/saveCache()'s exact convention.
    juce::File cacheFile() const;
    struct CachedClassification { juce::int64 fileSize; juce::int64 mtimeMs; juce::String rackId; double durationSecs; };
    void saveCache(const std::map<juce::String, CachedClassification>& entries) const;

    RackListComponent    rackList;
    RackContentComponent rackContent;

    juce::File           pendingDir;
    std::vector<Rack>    racks;
    int                  selected = 0;

    bool lastScanFullyCached = false;
    int  lastScanCachedCount = 0;
    int  lastScanChangedCount = 0;

    static constexpr int kSidebarWidth = 130;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RackBrowserComponent)
};
