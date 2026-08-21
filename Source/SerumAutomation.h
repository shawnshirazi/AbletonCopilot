#pragma once
#include <JuceHeader.h>

// macOS-only, OS-level Serum 2 UI automation - the ONLY mechanism that
// can advance Serum 2's own preset browser to a DIFFERENT real factory
// preset. No VST3 API exists for this (re-confirmed this project's own
// prior investigations: melodic_techno_research.md section 11.6,
// sound_and_rhythm_diagnostic_pass4.md section 6, and a real live AX-tree
// dump of Serum 2's editor showing its entire content canvas exposes ZERO
// accessible sub-elements - a coordinate-based click is the only
// mechanism, not a choice this code made lightly).
//
// HONESTY CONTRACT: this can genuinely fail - no Accessibility permission
// granted to this process, the Serum 2 window not currently open/found,
// or a click that simply didn't land on the right control. Every function
// here reports an explicit, checkable result - never silently assumes
// success. Callers MUST verify the effect independently afterward (e.g.
// diffing real getStateInformation() bytes before/after - this module has
// no access to Serum 2's state bytes itself and cannot do that
// verification on its own).
namespace SerumAutomation
{
    // True if THIS process has been granted macOS Accessibility
    // permission (System Settings -> Privacy & Security -> Accessibility)
    // - required for both window inspection and synthetic input. A whole
    // batch of candidates should check this ONCE up front and report one
    // clear, honest reason for every candidate rather than attempting
    // (and separately failing) once per candidate.
    bool isAutomationAvailable();

    // The real, CURRENT on-screen bounds of one of THIS process's own
    // windows whose title contains `windowTitleHint` (e.g. the exact
    // title PluginEditor::openSerumWindowForTrack sets - "Serum 2 —
    // BASS") - never a hardcoded/assumed position or size. `found` is
    // false if automation isn't available or no matching window is
    // currently open.
    struct WindowBounds
    {
        bool  found = false;
        float x = 0, y = 0, w = 0, h = 0;
    };
    WindowBounds findOwnWindow(const juce::String& windowTitleHint);

    // Clicks the next-preset arrow, at a position expressed as a FRACTION
    // of the window's own current live width/height (see this file's .cpp
    // for the exact measured fraction and its provenance) - adapts to the
    // window's real current position/size, but is still fundamentally
    // dependent on Serum 2's own internal layout proportions staying the
    // same; a real, disclosed limitation, not hidden. Returns true only
    // if a click was actually dispatched (automation available AND the
    // window was found) - this is NOT proof the preset actually changed;
    // callers must verify that independently.
    bool clickNextPreset(const juce::String& windowTitleHint);
}
