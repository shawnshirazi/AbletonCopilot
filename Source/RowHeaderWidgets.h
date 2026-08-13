#pragma once
#include <JuceHeader.h>
#include "UIStyle.h"

// Shared row-header painting widgets — the ring badge, and the stacked M/S
// toggle pair — used identically by DrumMachineComponent's drum rows and
// MelodyGridComponent's melody tracks, so the two stay visually consistent
// without duplicating this painting code in both files.
namespace RowHeaderWidgets
{
    // Ring badge: outlined circle in the row's accent colour with its
    // abbreviated name centred inside — XO's primary row-identity element.
    inline void paintRingBadge(juce::Graphics& g, juce::Rectangle<float> bounds,
                                const juce::Colour& colour, const juce::String& label, bool dimmed)
    {
        auto c = dimmed ? colour.withMultipliedAlpha(0.4f) : colour;
        g.setColour(c);
        g.drawEllipse(bounds.reduced(1.0f), 1.6f);
        g.setFont(UIStyle::badge());
        g.drawText(label, bounds, juce::Justification::centred);
    }

    // Small stacked M / S toggle pair, sized to sit beside the ring badge.
    struct MuteSoloRects
    {
        juce::Rectangle<int> mute, solo;
    };

    inline MuteSoloRects layoutMuteSolo(juce::Rectangle<int> column)
    {
        MuteSoloRects r;
        r.mute = column.removeFromTop(column.getHeight() / 2 - 1);
        column.removeFromTop(2);
        r.solo = column;
        return r;
    }

    inline void paintToggleButton(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label,
                                   bool active, const juce::Colour& activeColour)
    {
        static const juce::Colour kOffBg { 0xff26272b };
        g.setColour(active ? activeColour : kOffBg);
        g.fillRoundedRectangle(area.toFloat(), 3.0f);
        g.setColour(active ? juce::Colours::black : UIStyle::kTextDim);
        g.setFont(UIStyle::small().withStyle("Bold"));
        g.drawText(label, area, juce::Justification::centred);
    }
}
