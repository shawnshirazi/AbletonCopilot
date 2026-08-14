#include "GeneratedDrumGridComponent.h"
#include "UIStyle.h"

GeneratedDrumGridComponent::GeneratedDrumGridComponent()
{
    addAndMakeVisible(headerColumn);
    gridViewport.setViewedComponent(&gridContent, false);
    gridViewport.setScrollBarsShown(false, true); // horizontal only
    addAndMakeVisible(gridViewport);
}

void GeneratedDrumGridComponent::setPattern(std::vector<RowDisplay> newRows, int stepsPerBarIn, int numBarsIn)
{
    rows        = std::move(newRows);
    stepsPerBar = stepsPerBarIn > 0 ? stepsPerBarIn : 16;
    numBars     = numBarsIn > 0 ? numBarsIn : 1;

    resized();
    headerColumn.repaint();
    gridContent.repaint();
}

void GeneratedDrumGridComponent::resized()
{
    auto area = getLocalBounds();
    headerColumn.setBounds(area.removeFromLeft(kHeaderWidth));
    gridViewport.setBounds(area);

    const int rowCount   = (int) rows.size();
    const int totalSteps = stepsPerBar * numBars;
    gridContent.setSize(juce::jmax(1, totalSteps * kStepWidth), kRowHeight * rowCount);
    headerColumn.setSize(kHeaderWidth, kRowHeight * rowCount);
}

void GeneratedDrumGridComponent::HeaderColumn::paint(juce::Graphics& g)
{
    g.fillAll(UIStyle::kBg);

    for (int i = 0; i < (int) owner.rows.size(); ++i)
    {
        auto& row = owner.rows[(size_t) i];
        const int rowTop = i * GeneratedDrumGridComponent::kRowHeight;
        auto rowArea = juce::Rectangle<int>(0, rowTop, getWidth(), GeneratedDrumGridComponent::kRowHeight);

        g.setColour(UIStyle::kBorder);
        g.drawHorizontalLine(rowTop, 0.0f, (float) getWidth());

        g.setColour(row.colour);
        g.fillEllipse(rowArea.reduced(0, 8).removeFromLeft(10).toFloat());

        g.setColour(UIStyle::kTextDim);
        g.setFont(UIStyle::small());
        g.drawText(row.name, rowArea.reduced(16, 0), juce::Justification::centredLeft);
    }
}

void GeneratedDrumGridComponent::GridContent::paint(juce::Graphics& g)
{
    static const juce::Colour kCellOff { 0xff2a2b2f };
    static const juce::Colour kBarLine { 0xff333438 };

    const int w = getWidth();

    for (int i = 0; i < (int) owner.rows.size(); ++i)
    {
        auto& row = owner.rows[(size_t) i];
        const int rowTop = i * GeneratedDrumGridComponent::kRowHeight;

        g.setColour(i % 2 == 0 ? UIStyle::kPanelAlt : UIStyle::kRowAlt);
        g.fillRect(juce::Rectangle<int>(0, rowTop, w, GeneratedDrumGridComponent::kRowHeight));

        for (int s = 0; s < (int) row.velocity.size(); ++s)
        {
            auto cell = juce::Rectangle<float>((float) (s * GeneratedDrumGridComponent::kStepWidth),
                                               (float) rowTop,
                                               (float) GeneratedDrumGridComponent::kStepWidth,
                                               (float) GeneratedDrumGridComponent::kRowHeight)
                            .reduced(0.5f, 5.0f);

            const int vel = row.velocity[(size_t) s];
            if (vel > 0)
            {
                const float alpha = juce::jlimit(0.25f, 1.0f, (float) vel / 127.0f);
                g.setColour(row.colour.withAlpha(alpha));
                g.fillRect(cell);
            }
            else
            {
                g.setColour(kCellOff);
                g.fillRect(cell);
            }
        }
    }

    // Bar dividers.
    g.setColour(kBarLine);
    for (int bar = 1; bar < owner.numBars; ++bar)
        g.drawVerticalLine(bar * owner.stepsPerBar * GeneratedDrumGridComponent::kStepWidth,
                           0.0f, (float) getHeight());
}
