#include "StackBrowserComponent.h"

static const juce::Colour kHeaderBg { 0xff262626 };
static const juce::Colour kRowBg    { 0xff1c1c1c };
static const juce::Colour kRowHover { 0xff2a2a2a };
static const juce::Colour kAccent   { 0xfffc8c00 };
static const juce::Colour kText     { 0xffeeeeee };
static const juce::Colour kTextDim  { 0xff777777 };

static juce::Colour stemColour(const juce::String& name)
{
    auto n = name.toUpperCase();
    if (n.contains("KICK"))  return juce::Colour(0xfff35940);
    if (n.contains("SNARE")) return juce::Colour(0xff40a0f3);
    if (n.contains("CLAP"))  return juce::Colour(0xfff3b333);
    if (n.contains("HIHAT")) return juce::Colour(0xff4dcc73);
    return juce::Colour(0xff8c8c99);
}

//==============================================================================

StackRowComponent::StackRowComponent(juce::String label, juce::File file, bool isHeader)
    : labelText(std::move(label)), fileToDrag(std::move(file)), header(isHeader)
{
    setInterceptsMouseClicks(!header, false);
}

void StackRowComponent::paint(juce::Graphics& g)
{
    auto r = getLocalBounds();

    if (header)
    {
        g.setColour(kHeaderBg);
        g.fillRect(r);
        g.setColour(kAccent);
        g.setFont(juce::FontOptions(11.5f).withStyle("Bold"));
        g.drawText(labelText, r.reduced(10, 0), juce::Justification::centredLeft);
        return;
    }

    g.setColour(isMouseOver() ? kRowHover : kRowBg);
    g.fillRect(r);

    auto inner = r.reduced(10, 0);
    auto tag = inner.removeFromLeft(8);
    g.setColour(stemColour(labelText));
    g.fillRoundedRectangle(tag.reduced(0, 6).toFloat(), 2.0f);

    inner.removeFromLeft(8);
    g.setColour(kText);
    g.setFont(juce::FontOptions(12.5f));
    g.drawText(labelText, inner, juce::Justification::centredLeft);

    g.setColour(kTextDim);
    g.setFont(juce::FontOptions(14.0f));
    g.drawText("::", r.removeFromRight(24), juce::Justification::centred);
}

void StackRowComponent::mouseDown(const juce::MouseEvent&)
{
    dragStarted = false;
}

void StackRowComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (header || dragStarted) return;
    if (e.getDistanceFromDragStart() < 6) return;
    if (!fileToDrag.existsAsFile()) return;

    dragStarted = true;
    juce::StringArray files;
    files.add(fileToDrag.getFullPathName());
    juce::DragAndDropContainer::performExternalDragDropOfFiles(files, false, this);
}

void StackRowComponent::mouseUp(const juce::MouseEvent&)
{
    dragStarted = false;
}

//==============================================================================

StackBrowserComponent::StackBrowserComponent()
{
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
}

void StackBrowserComponent::setSections(std::vector<StackSection> newSections)
{
    sections = std::move(newSections);
    rebuildRows();
}

void StackBrowserComponent::rebuildRows()
{
    rows.clear();

    int y = 0;
    const int w = juce::jmax(200, viewport.getWidth() - viewport.getScrollBarThickness());

    for (auto& section : sections)
    {
        auto* header = rows.add(new StackRowComponent(section.display, {}, true));
        header->setBounds(0, y, w, kHeaderRowHeight);
        content.addAndMakeVisible(header);
        y += kHeaderRowHeight;

        for (auto& stem : section.stems)
        {
            auto* row = rows.add(new StackRowComponent(stem.name, stem.file, false));
            row->setBounds(0, y, w, kStemRowHeight);
            content.addAndMakeVisible(row);
            y += kStemRowHeight;
        }
    }

    content.setSize(w, juce::jmax(y, viewport.getHeight()));
    viewport.repaint();
}

void StackBrowserComponent::resized()
{
    viewport.setBounds(getLocalBounds());
    rebuildRows();
}

void StackBrowserComponent::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff151515));
    g.fillRect(getLocalBounds());

    if (sections.empty())
    {
        g.setColour(kTextDim);
        g.setFont(juce::FontOptions(12.0f));
        g.drawText("No stack rendered yet.", getLocalBounds(), juce::Justification::centred);
    }
}
