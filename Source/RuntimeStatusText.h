#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>

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
    //
    // Deliberately dense (2 lines, not the ~22 a one-field-per-line layout
    // would take) - same exact facts as before (every role's real sample
    // file, every voice's real captured-preset-or-FACTORY-INIT state, MIDI
    // range, active flag), just packed onto one line per category instead
    // of one line per field. Nothing here is summarized or dropped.
    inline juce::String buildStatusPrefix(const std::vector<DrumRoleLine>& drumRoles,
                                           const std::vector<VoiceLine>& voices)
    {
        juce::String s;
        s << "DRUMS  ";
        for (auto& r : drumRoles)
            s << r.label << ": " << r.fileName << "   ";
        s = s.trimEnd() + "\n";

        for (size_t i = 0; i < voices.size(); ++i)
        {
            auto& v = voices[i];
            s << v.label << ": " << v.presetLine;
            if (v.extraLine.isNotEmpty())
                s << " (" << v.extraLine << ")";
            s << ", active: " << (v.active ? "YES" : "NO");
            s << (i + 1 < voices.size() ? "   |   " : "\n");
        }
        return s;
    }

    inline juce::String appendSection(const juce::String& prefix, const juce::String& sectionName)
    {
        return prefix + "SECTION: " + sectionName + "\n";
    }

    struct StemExportRoleStatus
    {
        juce::String label;    // "Kick"/"Clap"/"Closed Hat"/"Open Hat"/"Perc A"/"Perc B"
        juce::String fileName; // real LOADED file name (never a candidate/suggested path) - "(synth fallback)" if none loaded
    };

    // Pre-export status block, matching the user's exact requested format:
    // one "<role> = <file>" line per role (the REAL loaded sample, not a
    // candidate), then Length/BPM/Sample Rate.
    inline juce::String buildDrumStemExportStatus(const std::vector<StemExportRoleStatus>& roles,
                                                   int lengthBars, double bpm, double sampleRate)
    {
        juce::String s = "DRUM STEM EXPORT\n";
        for (auto& r : roles)
            s << r.label << " = " << r.fileName << "\n";
        s << "Length: " << lengthBars << " bars\n";
        s << "BPM: " << juce::String(bpm, 1) << "\n";
        s << "Sample Rate: " << (int) juce::roundToInt(sampleRate) << " Hz\n";
        return s;
    }

    // Post-export confirmation line, one checkmark per role in fixed
    // Kick/Clap/ClosedHat/OpenHat/PercA/PercB order, matching the user's
    // exact requested "Exported: [checkmark] Kick.wav [checkmark] Clap.wav ..." format.
    inline juce::String buildDrumStemExportedConfirmation(const std::array<bool, 6>& succeededPerRole)
    {
        static const char* const kFileNames[6] =
            { "Kick.wav", "Clap.wav", "ClosedHat.wav", "OpenHat.wav", "PercA.wav", "PercB.wav" };
        const juce::String check = juce::String(juce::CharPointer_UTF8("\xe2\x9c\x93"));

        juce::String s = "Exported: ";
        for (int i = 0; i < 6; ++i)
        {
            if (succeededPerRole[(size_t) i])
                s << check << " " << kFileNames[i];
            else
                s << "x " << kFileNames[i];
            if (i < 5)
                s << "  ";
        }
        return s;
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
