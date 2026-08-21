#pragma once
#include <JuceHeader.h>

// Pure, dependency-free decision logic for what a melody track's Serum2
// status label is allowed to say. Extracted out of PluginEditor's
// serumStatusFor/updateTrackTitle lambdas specifically so it's unit-
// testable without constructing the full editor (which the rest of this
// project's test suite deliberately avoids - see
// Source/tests/test_loop_generator_processor.cpp's own header comment).
//
// The honesty contract this exists to enforce: a preset name may ONLY be
// displayed as loaded/captured when BOTH (a) local UI bookkeeping
// (lastConfirmedPresetName) says a specific named preset was successfully
// applied, AND (b) the processor confirms Serum2's instance actually has
// that captured state active right now
// (MelodyVoiceDiagnostics::capturedPresetActive). Either condition failing
// falls through to the same honest "factory Init patch" message the app
// has always shown when nothing was captured - never a guess, never
// inferred from presetIndex/presetFiles alone.
namespace SerumPresetStatus
{
    // trackLabel: "Bass"/"Melody". confirmedName: MelodyTrackPanel::
    // lastConfirmedPresetName (empty if nothing confirmed loaded).
    // capturedPresetActive: PluginProcessor::MelodyVoiceDiagnostics's own
    // field for this track. suggestions/fallbackStatus: the same real,
    // named candidate text and live processor status the existing
    // fallback message already used.
    inline juce::String statusText(const juce::String& trackLabel,
                                    const juce::String& confirmedName,
                                    bool capturedPresetActive,
                                    const juce::String& suggestions,
                                    const juce::String& fallbackStatus)
    {
        if (confirmedName.isNotEmpty() && capturedPresetActive)
            return trackLabel + " - Serum 2 preset: " + confirmedName + " (captured)";

        return trackLabel + " - Serum 2 preset: factory Init patch (no capture performed yet - "
             + "open Serum 2, browse to a real " + suggestions + " preset, click Capture) - "
             + fallbackStatus;
    }

    // Same honesty gate, terse form for the structured DRUMS/BASS/MELODY/
    // PAD/SECTION status block (RuntimeStatusText.h) - "Preset: <name>
    // Captured [checkmark]" / "Preset: Factory Init (not captured)", the
    // exact wording requested for both this side-by-side block AND the
    // per-track panel's own status line (see panelStatusLine below - same
    // wording, kept as a separate function since callers need it in a
    // different loading-state context, not because the text itself should
    // ever differ). The explicit "Captured" checkmark only ever appears
    // alongside a real confirmed name under the same two-condition gate as
    // everything else in this file - never shown on its own.
    inline juce::String compactPresetLine(const juce::String& confirmedName, bool capturedPresetActive)
    {
        if (confirmedName.isNotEmpty() && capturedPresetActive)
            return "Preset: " + confirmedName + "  Captured " + juce::String(juce::CharPointer_UTF8("\xe2\x9c\x93"));
        return juce::String("Preset: Factory Init (not captured)");
    }

    // Same honesty gate, for MelodyTrackPanel's own title label (which has
    // no per-track "suggestions"/fallback text - just a plain name-or-
    // placeholder).
    inline juce::String titleText(const juce::String& categoryDisplayName,
                                   const juce::String& confirmedName,
                                   bool capturedPresetActive)
    {
        // "\xe2\x80\x94" = em-dash, same escaped-UTF8-byte convention the
        // rest of PluginEditor.cpp already uses (see updateTrackTitle's
        // prior version) rather than a literal non-ASCII source character,
        // which triggers juce::String's 8-bit-data assertion.
        const juce::String name = (confirmedName.isNotEmpty() && capturedPresetActive)
            ? confirmedName
            : juce::String("(no captured sounds yet \xe2\x80\x94 click Capture)");
        return categoryDisplayName + "  \xe2\x80\x94  " + name;
    }

    // Same honesty gate, for the per-track panel's own "Preset: ..."
    // status line (MelodyTrackPanel::statusLabel, once that track's
    // Serum2 instance has finished loading) - "Preset: <name>" /
    // "Preset: Factory Init (not captured)", the exact wording requested
    // for that specific display. Callers must only use this once
    // MelodyVoiceDiagnostics::serumInstanceLoaded is true for the track -
    // while still loading, the real load-status text (PluginProcessor::
    // getMelodyTrackStatus) is what belongs there instead, since
    // "Factory Init" would be premature (there's no instance yet to be
    // at any patch, factory or otherwise).
    inline juce::String panelStatusLine(const juce::String& confirmedName, bool capturedPresetActive)
    {
        return compactPresetLine(confirmedName, capturedPresetActive);
    }
}
