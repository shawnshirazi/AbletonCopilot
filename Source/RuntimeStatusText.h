#pragma once
#include <JuceHeader.h>
#include <vector>

// Pure, dependency-free text builder for the structured runtime-status
// block the user explicitly asked for: DRUMS/BASS/MELODY/PAD/SECTION,
// each field only ever showing what the code can actually prove (a real
// decoded sample file, a real confirmed Serum2 capture, a real diagnostic-
// reported active flag) - never a guess, never an assumption. Extracted
// as a pure function (same reasoning as SerumPresetStatus.h) so it's
// unit-testable without constructing the full editor.
namespace RuntimeStatusText
{
    struct DrumRoleLine
    {
        juce::String label;    // "Kick"/"Clap"/"Closed Hat"/"Open Hat"/"Perc A"/"Perc B"
        juce::String fileName; // real decoded sample file name, or "(synth fallback)" if none loaded
    };

    struct VoiceLine
    {
        juce::String label;       // "BASS"/"MELODY"/"PAD"
        juce::String presetLine;  // SerumPresetStatus::compactPresetLine's output
        juce::String extraLine;   // e.g. "MIDI range: C2 - A2" (Bass only) - empty = omitted
        bool         active = false;
    };

    // Everything EXCEPT the SECTION line - built once per Generate click
    // and cached (PluginEditor::structuredStatusPrefix), since SECTION is
    // the only part that needs refreshing every timer tick from the live
    // host playhead (see appendSection below) - rebuilding the drum/voice
    // diagnostics every tick would be wasted work for data that hasn't
    // changed since the last Generate click.
    inline juce::String buildStatusPrefix(const std::vector<DrumRoleLine>& drumRoles,
                                           const std::vector<VoiceLine>& voices)
    {
        juce::String s;
        s << "DRUMS\n";
        for (auto& r : drumRoles)
            s << "  " << r.label << ": " << r.fileName << "\n";

        for (auto& v : voices)
        {
            s << "\n" << v.label << "\n";
            s << "  Serum2: " << v.presetLine << "\n";
            if (v.extraLine.isNotEmpty())
                s << "  " << v.extraLine << "\n";
            s << "  active: " << (v.active ? "YES" : "NO") << "\n";
        }
        return s;
    }

    inline juce::String appendSection(const juce::String& prefix, const juce::String& sectionName)
    {
        return prefix + "\nSECTION\n  " + sectionName + "\n";
    }

    // Human-readable name for Engine::CompactSection - kept here (not in
    // BreakdownArrangement.h, which is zero-JUCE) since this is purely a
    // display concern.
    inline juce::String sectionDisplayName(int compactSectionEnumValue)
    {
        switch (compactSectionEnumValue)
        {
            case 0: return "DROP";
            case 1: return "BREAK_ENTRY";
            case 2: return "BREAK_BODY";
            case 3: return "PRE_DROP";
            default: return "(unknown)";
        }
    }
}
