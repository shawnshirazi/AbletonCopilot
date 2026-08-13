#pragma once
#include <JuceHeader.h>
#include "StackRenderer.h"

// One row: either a section header ("Intro") or a draggable stem ("Kick").
class StackRowComponent : public juce::Component
{
public:
    StackRowComponent(juce::String label, juce::File file, bool isHeader);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    juce::String labelText;
    juce::File   fileToDrag;
    bool         header;
    bool         dragStarted = false;
};

// Scrollable list of sections, each containing draggable stem rows.
// Drag a stem row straight onto a track in Ableton's Arrangement View.
class StackBrowserComponent : public juce::Component
{
public:
    StackBrowserComponent();

    void setSections(std::vector<StackSection> newSections);
    void resized() override;
    void paint(juce::Graphics&) override;

private:
    void rebuildRows();

    static constexpr int kHeaderRowHeight = 26;
    static constexpr int kStemRowHeight   = 24;

    std::vector<StackSection>        sections;
    juce::Viewport                    viewport;
    juce::Component                   content;
    juce::OwnedArray<StackRowComponent> rows;
};
