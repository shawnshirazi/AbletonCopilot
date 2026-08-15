#pragma once
#include <JuceHeader.h>
#include "RackClassification.h"
#include "StackBrowserComponent.h"
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

private:
    void run() override;
    void applyRacks(std::vector<Rack> newRacks);
    void selectRack(int index);

    RackListComponent    rackList;
    RackContentComponent rackContent;

    juce::File           pendingDir;
    std::vector<Rack>    racks;
    int                  selected = 0;

    static constexpr int kSidebarWidth = 130;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RackBrowserComponent)
};
