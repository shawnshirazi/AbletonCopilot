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

void GeneratedDrumGridComponent::setPlayheadStep(int step, bool visible)
{
    if (playheadStep == step && playheadVisible == visible)
        return;

    playheadStep    = step;
    playheadVisible = visible;
    gridContent.repaint();

    // Auto-scroll so the moving playhead stays on screen - the pattern is
    // 256 steps wide at 8px/step (2048px), well past the visible viewport
    // width, so without this the playhead would scroll off-screen on
    // every loop.
    if (visible && step >= 0)
    {
        const int x = step * kStepWidth;
        auto visibleArea = gridViewport.getViewArea();
        if (x < visibleArea.getX() || x >= visibleArea.getRight())
            gridViewport.setViewPosition(juce::jmax(0, x - visibleArea.getWidth() / 2), 0);
    }
}

void GeneratedDrumGridComponent::resized()
{
    auto area = getLocalBounds();
    area.removeFromBottom(kLegendHeight); // reserved for the legend, painted directly (see paint())

    headerColumn.setBounds(area.removeFromLeft(kHeaderWidth));
    gridViewport.setBounds(area);

    const int rowCount   = (int) rows.size();
    const int totalSteps = stepsPerBar * numBars;
    gridContent.setSize(juce::jmax(1, totalSteps * kStepWidth), kRowHeight * rowCount);
    headerColumn.setSize(kHeaderWidth, kRowHeight * rowCount);
}

void GeneratedDrumGridComponent::paint(juce::Graphics& g)
{
    // Velocity legend: same brighter()-based mapping GridContent::paint
    // uses per cell (darker = stronger, lighter = softer), shown as a
    // small gradient swatch so the shading isn't left unexplained.
    auto legend = getLocalBounds().removeFromBottom(kLegendHeight);
    legend.removeFromLeft(kHeaderWidth);
    legend = legend.reduced(4, 3);

    g.setColour(UIStyle::kTextDim);
    g.setFont(UIStyle::small());

    auto softLabel = legend.removeFromLeft(30);
    g.drawText("Soft", softLabel, juce::Justification::centredLeft);

    auto strongLabel = legend.removeFromRight(42);
    g.drawText("Strong", strongLabel, juce::Justification::centredRight);

    auto bar = legend.reduced(6, 6).toFloat();
    if (bar.getWidth() > 0.0f)
    {
        juce::ColourGradient grad(UIStyle::kAccent.brighter(1.0f), bar.getX(), bar.getY(),
                                   UIStyle::kAccent,                bar.getRight(), bar.getY(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bar, 3.0f);
    }
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
                // Darker = stronger, lighter = softer: brighter(0) at max
                // velocity leaves the base (darker/saturated) row colour
                // unchanged; brighter(~1) at minimum velocity shifts it
                // most of the way toward white. Opaque, not alpha-blended
                // against the panel background - a soft hit stays a
                // visibly light colour rather than fading into the
                // background.
                const float t = juce::jlimit(0.0f, 1.0f, (float) vel / 127.0f);
                g.setColour(row.colour.brighter(1.0f - t));
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

    // Playhead - visualization only, position comes from the owner
    // reading the host's real PPQ each timer tick (see
    // PluginEditor::timerCallback), not from any clock owned by this
    // component.
    if (owner.playheadVisible && owner.playheadStep >= 0)
    {
        const float x = (float) (owner.playheadStep * GeneratedDrumGridComponent::kStepWidth);
        g.setColour(UIStyle::kAccent.withAlpha(0.55f));
        g.fillRect(juce::Rectangle<float>(x, 0.0f, (float) GeneratedDrumGridComponent::kStepWidth, (float) getHeight()));
    }
}
