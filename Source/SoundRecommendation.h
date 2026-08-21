#pragma once
#include <JuceHeader.h>
#include <vector>

// Pure, dependency-free "what kind of Serum 2 sound would fit this role"
// recommendation engine - the foundation of the sound-design workflow:
// Generate -> analyze the role's already-generated MIDI -> recommend a
// CHARACTER (not a specific preset - see the honesty contract below) ->
// audition it against the real generated material -> Capture whatever the
// user settles on.
//
// HONESTY CONTRACT (matches SerumPresetStatus.h's own contract, extended
// here): this file NEVER claims to have selected, loaded, or activated a
// Serum 2 preset. It does two things only:
//   1. Recommends a CHARACTER/description in words (e.g. "Deep / Analog /
//      Reese / Melodic Techno"), derived from the role and the actual
//      measured density/hold-length of that role's already-generated
//      pattern - never random text, never a fixed string independent of
//      the real MIDI.
//   2. Optionally lists REAL, VERIFIED-TO-EXIST-RIGHT-NOW `.SerumPreset`
//      filenames found by directly listing a real folder on disk (Xfer's
//      own Factory preset library) - filesystem enumeration, not Serum 2's
//      VST3 program-list API (confirmed unusable - see below) and not any
//      parsing of the proprietary `.SerumPreset` binary format. If that
//      folder doesn't exist on a given machine, the candidate list is
//      empty - never fabricated, never a guess.
//
// WHY NOT AUTO-SELECT A PRESET: investigated and answered before this
// header was written (musical_target/melodic_techno_research.md section
// 11.6, re-confirmed in sound_and_rhythm_diagnostic_pass4.md section 6):
// Serum 2's VST3 `getNumPrograms()`/`setCurrentProgram()` returns a dummy
// "Prog 1".."Prog 128" list that provably does not change
// `getStateInformation()`'s bytes (a dead end, re-verified via the real,
// dated `~/Library/AbletonCopilot/serum_program_list_debug.txt` this
// project already produced); no `.vstpreset` files (the one format a JUCE
// host could load directly) exist anywhere in the user's library;
// `Serum2Prefs.json` has no pre-seedable "default patch" key. The ONLY
// legitimate mechanism to get a real preset into a hosted Serum 2 instance
// is the existing Capture workflow: the user loads a sound through Serum
// 2's OWN browser, then this plugin captures the real resulting VST3
// state bytes. This header does not change that conclusion - it makes the
// recommendation step that precedes it useful and evidence-grounded.
//
// RESEARCH GROUNDING: every character rule below cites the specific claim
// it's drawn from, in MLPipeline/musical_target/. Confidence tags
// ([HIGH]/[MEDIUM]/[LOW]/[INTERPRETATION]) are carried over from those
// documents' own tagging, not invented here - most sound-design claims in
// that corpus are honestly tagged LOW (single web source, no local
// MIDI-corpus cross-validation, since this project's corpus is MIDI-only
// and has no synthesis-parameter dimension). This is disclosed, not hidden.
namespace SoundRecommendation
{
    enum class Role { Bass, Melody, Pad };

    // Real, measured statistics about a role's ALREADY-GENERATED pattern -
    // computed by the caller (PluginEditor) from the actual stored offsets
    // array, never re-derived, guessed, or based on a fresh/different
    // pattern. This is what makes the recommendation "understand the
    // musical role" per the user's own request, rather than being a fixed
    // string.
    struct PatternStats
    {
        bool  hasNotes     = false; // false = totally empty pattern (e.g. Pad this phase) - character falls back to role-only defaults
        float density      = 0.0f;  // fraction of steps that are onsets, 0..1
        float avgHoldSteps = 0.0f;  // mean steps a note rings before the next onset (or loop end) - only meaningful if hasNotes
    };

    // Computes PatternStats from a raw per-step offset array using
    // `offValue` as the "no note here" sentinel (Engine::kBassOffValue ==
    // MelodyGridComponent::kMelodyOff == -128, but passed explicitly so
    // this header stays decoupled from those types/includes). Pure,
    // deterministic - the exact same function PluginEditor uses for
    // display and tests use to verify recommendation behavior, so there's
    // only one definition of "density"/"hold length" anywhere in this
    // codebase.
    template <typename OffsetContainer>
    PatternStats analyzePattern(const OffsetContainer& offsets, int8_t offValue)
    {
        PatternStats stats;
        const int totalSteps = (int) offsets.size();
        if (totalSteps <= 0)
            return stats;

        std::vector<int> onsetSteps;
        for (int i = 0; i < totalSteps; ++i)
            if (offsets[(size_t) i] != offValue)
                onsetSteps.push_back(i);

        if (onsetSteps.empty())
            return stats;

        stats.hasNotes = true;
        stats.density  = (float) onsetSteps.size() / (float) totalSteps;

        float totalHold = 0.0f;
        for (size_t i = 0; i < onsetSteps.size(); ++i)
        {
            const int nextOnset = (i + 1 < onsetSteps.size()) ? onsetSteps[i + 1]
                                                                : (onsetSteps[0] + totalSteps); // wraps to the loop's own first onset
            totalHold += (float) (nextOnset - onsetSteps[i]);
        }
        stats.avgHoldSteps = totalHold / (float) onsetSteps.size();
        return stats;
    }

    struct Recommendation
    {
        juce::String character;   // e.g. "Deep / Analog / Reese / Melodic Techno" - the headline recommendation
        juce::String roleText;    // e.g. "Low-register foundation" - one-line role summary
        juce::String why;         // grounded explanation referencing the real measured stats + a citation tag
        // Real, verified-to-exist-right-now .SerumPreset file names (no
        // extension, for display) found under candidateFolderHint - empty
        // if that folder doesn't exist on this machine. NEVER claims these
        // are loaded/active; the user must still browse to and load one
        // through Serum 2's own UI, then Capture it.
        juce::StringArray candidateNames;
        juce::String candidateFolderHint; // e.g. "Factory -> Bass -> Reese" - shown even when candidateNames is empty, as manual-browse guidance
    };

    // The real, on-disk root of Xfer's own factory Serum 2 preset library -
    // confirmed to exist on this development machine (ls'd directly, not
    // assumed) at the time this header was written. A different machine
    // without Serum 2/Xfer's presets installed here will simply find
    // nothing below it - handled honestly (empty candidateNames), not as
    // an error.
    inline juce::File defaultSerumFactoryPresetsRoot()
    {
        return juce::File("/Library/Audio/Presets/Xfer Records/Serum 2 Presets/Presets/Factory");
    }

    // Lists real .SerumPreset file names (without extension) directly
    // under `folder`, sorted, capped at `maxCount` - a plain filesystem
    // listing (juce::File::findChildFiles), NOT Serum 2's VST3 program-
    // list API (confirmed unusable, see this header's own top comment) and
    // NOT any parsing of the proprietary .SerumPreset binary format itself.
    // Empty if the folder doesn't exist - never a fabricated name.
    inline juce::StringArray listRealPresetCandidates(const juce::File& folder, int maxCount = 5)
    {
        juce::StringArray names;
        if (!folder.isDirectory())
            return names;

        juce::Array<juce::File> files;
        folder.findChildFiles(files, juce::File::findFiles, false, "*.SerumPreset");
        files.sort();
        for (auto& f : files)
        {
            if (names.size() >= maxCount)
                break;
            names.add(f.getFileNameWithoutExtension());
        }
        return names;
    }

    // The actual recommendation logic. `presetsRoot` is a parameter (not
    // hardcoded inside) so tests can point it at a fixture folder instead
    // of the real Xfer library.
    inline Recommendation recommend(Role role, const PatternStats& stats,
                                     const juce::File& presetsRoot = defaultSerumFactoryPresetsRoot())
    {
        Recommendation r;

        if (role == Role::Bass)
        {
            r.roleText = "Low-register foundation";
            // Base character grounded in: bass-009 [HIGH, youtube_production_
            // knowledge.json] - two-layer sub+mid/character bass structure,
            // independently corroborated by a real reference arrangement's
            // own "Rolling Bass"/"Sub Bass" tracks; bass-011 [LOW] - a warm,
            // ~200-400Hz-cutoff low-pass character; the existing
            // clampBassRegisterPitch band (F1-C3, PluginProcessor.h) already
            // structurally guarantees this role sits low, so "deep/sub-
            // focused" doesn't need to be re-detected from the MIDI here.
            if (!stats.hasNotes)
            {
                r.character = "Deep / Analog / Melodic Techno Bass";
                r.why = "No bass pattern generated yet - a general deep/analog "
                        "starting character, grounded in the measured two-layer "
                        "sub+mid bass structure common to melodic techno "
                        "[bass-009, HIGH confidence].";
            }
            else if (stats.avgHoldSteps >= 8.0f)
            {
                // Sustained/held notes -> Reese-leaning. Grounded in the
                // measured "Bass Sustain/Reese" texture that replaces a
                // rhythmic bassline during a real reference breakdown
                // (melodic_techno_research.md section 11.1/11.4) - extended
                // here as an [INTERPRETATION] to "a bass phrase that itself
                // holds long notes suits the same sustained/Reese character",
                // not a literal restatement of that breakdown-specific
                // finding.
                r.character = "Deep / Analog / Reese / Melodic Techno";
                r.why = juce::String("This bass phrase holds notes for extended "
                        "durations (avg ") + juce::String(stats.avgHoldSteps, 1)
                        + " steps between onsets) rather than rapid rearticulation - "
                          "a sustained, Reese-style character suits held basslines "
                          "better than a plucky/rolling one [INTERPRETATION, "
                          "extending the sustained-bass/Reese texture measured in "
                          "melodic_techno_research.md 11.1/11.4].";
            }
            else if (stats.avgHoldSteps <= 3.0f && stats.density >= 0.3f)
            {
                // Dense, short notes -> rolling/tight. Grounded in bass-009's
                // own "top bass layer... for rhythm and groove" (HIGH) and
                // the user's own explicit "relatively short/tight notes when
                // the MIDI calls for it" ask.
                r.character = "Rolling / Tight Analog Bass";
                r.why = juce::String("This bass phrase is dense (") + juce::String(juce::roundToInt(stats.density * 100.0f))
                        + "% of steps are onsets) with short note lengths (avg "
                        + juce::String(stats.avgHoldSteps, 1) + " steps) - a tight, "
                          "rolling analog character fits rhythmic movement better "
                          "than a sustained pad-like bass [bass-009, HIGH confidence "
                          "for the rolling/rhythmic bass-layer role].";
            }
            else
            {
                r.character = "Deep / Analog / Reese / Melodic Techno";
                r.why = "A balanced density/hold-length bass phrase - the general "
                        "deep, warm, analog character (with Reese-style movement "
                        "available if the sound calls for it) fits most melodic "
                        "techno basslines [bass-009 HIGH, bass-011 LOW confidence].";
            }
            r.candidateFolderHint = "Factory -> Bass -> Reese";
            r.candidateNames = listRealPresetCandidates(presetsRoot.getChildFile("Bass").getChildFile("Reese"));
            if (r.candidateNames.isEmpty())
            {
                // Fall back to the Sub folder's real listing before giving up
                // entirely - still real, still on-disk, still never fabricated.
                r.candidateFolderHint = "Factory -> Bass -> Sub";
                r.candidateNames = listRealPresetCandidates(presetsRoot.getChildFile("Bass").getChildFile("Sub"));
            }
        }
        else if (role == Role::Melody)
        {
            r.roleText = "Upper-register melodic movement";
            if (!stats.hasNotes)
            {
                r.character = "Analog Pluck / Melodic Techno Lead";
                r.why = "No melody pattern generated yet - a general pluck/lead "
                        "starting character, grounded in the measured pluck "
                        "envelope shape (short attack, fast decay) and Tunecraft's "
                        "melodic-techno lead technique [sound-design-001/002, LOW "
                        "confidence - single-source, no local corpus cross-"
                        "validation].";
            }
            else if (stats.avgHoldSteps <= 2.0f && stats.density >= 0.35f)
            {
                // Dense, short notes -> arpeggiated/pluck texture. Grounded
                // in the user's own explicit "arpeggiated texture" ask and
                // the pluck envelope claim (short attack/fast decay suits
                // rapid rearticulation).
                r.character = "Arpeggiated Pluck Texture";
                r.why = juce::String("This melody phrase is dense (") + juce::String(juce::roundToInt(stats.density * 100.0f))
                        + "% of steps are onsets) with very short note lengths (avg "
                        + juce::String(stats.avgHoldSteps, 1) + " steps) - an "
                          "arpeggiated pluck texture (short attack, fast decay - "
                          "one envelope driving both amplitude and filter cutoff) "
                          "articulates rapid movement better than a sustained lead "
                          "[sound-design-002, LOW confidence].";
            }
            else if (stats.avgHoldSteps >= 6.0f)
            {
                // Long holds -> restrained/evolving lead, not a percussive
                // pluck. Grounded in the free-running-LFO/macro-mapped
                // "real-time expression" lead technique.
                r.character = "Restrained Analog Lead";
                r.why = juce::String("This melody phrase holds notes for extended "
                        "durations (avg ") + juce::String(stats.avgHoldSteps, 1)
                        + " steps) - a restrained, slower-evolving lead character "
                          "(free-running LFOs for motion rather than a fast pluck "
                          "envelope) fits sustained melodic phrasing better "
                          "[sound-design-001, LOW confidence].";
            }
            else
            {
                r.character = "Analog Pluck / Melodic Techno Lead";
                r.why = "A balanced density/hold-length melody phrase - a pluck-"
                        "leaning melodic techno lead (distinct upper-mid/high "
                        "content, real transient definition) fits most generated "
                        "melody phrases [sound-design-001/002, LOW confidence].";
            }
            // Melody must NOT default to a bass-type patch (explicit user
            // requirement) - candidates always come from Pluck/Lead, never
            // Bass, regardless of which character branch above was chosen.
            const bool pluckLeaning = r.character.contains("Pluck") || r.character.contains("Arpeggiated");
            r.candidateFolderHint = pluckLeaning ? "Factory -> Pluck" : "Factory -> Lead";
            r.candidateNames = listRealPresetCandidates(
                presetsRoot.getChildFile(pluckLeaning ? "Pluck" : "Lead"));
        }
        else // Role::Pad
        {
            // Static this pass - Pad has no real generator yet
            // (GrooveLoop.h's own header comment), and the user explicitly
            // asked not to implement pad sound-generation changes here.
            // Grounded in: sound-design-003 [LOW] layered-pad technique, and
            // breakdown-001 [HIGH] - a dedicated atmospheric pad element
            // measured in 3/3 real reference arrangements, exposed
            // specifically during the breakdown while kick/bass recede.
            r.roleText  = "Atmospheric support (breakdown-oriented)";
            r.character = "Atmospheric / Wide / Evolving / Breakdown";
            r.why = "Pad has no generated pattern this phase. The recommended "
                    "character is grounded in real reference arrangements, where "
                    "a dedicated pad/atmospheric element (measured as \"Pad "
                    "Break\"/\"Choir Pad\"/\"Strings\") is exposed specifically "
                    "during the breakdown, supporting rather than competing with "
                    "the bass [breakdown-001, HIGH confidence; sound-design-003, "
                    "LOW confidence for the layering/automation detail].";
            r.candidateFolderHint = "Factory -> Pad";
            r.candidateNames = listRealPresetCandidates(presetsRoot.getChildFile("Pad"));
        }

        return r;
    }
}
