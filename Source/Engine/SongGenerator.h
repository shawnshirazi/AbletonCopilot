#pragma once

#include "Arrangement.h"
#include "Theory.h"
#include <cstdint>
#include <string>
#include <vector>

// Full-track Melodic Techno generator - builds a complete, multi-part,
// ~6.5-minute song from scratch (drums, bass, pad, arp, lead, FX) as plain
// note data, ready to be written out as an Ableton Live Set (AlsWriter.h)
// or a Standard MIDI File (MidiFileWriter.h). Zero JUCE dependency, same
// convention as every other Engine/ file (see Theory.h for why).
//
// Reuses the existing, already-tested generators rather than replacing
// them:
//   - Engine::buildArrangement        - the per-bar MusicState timeline
//   - Engine::generateArrangementDrop - section-aware drums over the whole song
//   - Engine::generateDrop +
//     Engine::generateBassLoop16      - ONE 16-bar bass theme (archetype +
//                                       real gate lengths) re-used across
//                                       every groove section, so the drops
//                                       share a recognisable bassline
//                                       instead of each getting an
//                                       unrelated one
// and adds what did not exist yet: harmony (a diatonic minor progression),
// a sustained pad, a 16th-note arp, a call-and-response lead hook and an
// FX lane (impacts/risers/downlifters).
namespace Engine
{
    // One section of the finished song, with the display name that ends up
    // as the Ableton Arrangement locator / clip name.
    struct SongSectionSpec
    {
        std::string  name;
        SectionSpec  spec; // kind, bars, energy targets - fed to buildArrangement
    };

    // Full-length structure modelled on the three real reference
    // arrangements in MLPipeline/musical_target/melodic_techno_research.md
    // section 3.2/3.4 (INTRO -> BASS IN -> FULL THEME -> DROP 1 (subtle)
    // -> GROOVE / ELEMENT REINTRODUCTION -> PRE-BREAK -> BREAK -> BUILDUP
    // -> DROP 2 -> MELODIC BREAK -> DROP 3 -> OUTRO). Every length is a
    // multiple of 8 bars (research 3.3 #1); 200 bars = ~6:27 at 124 BPM,
    // inside the 160-248-bar range of the references (research 3.3 #6).
    std::vector<SongSectionSpec> fullMelodicTechnoStructure();

    struct SongParams
    {
        uint32_t  seed        = 1;
        double    bpm         = 124.0;
        int       rootNote    = 9; // 0=C ... 9=A (A minor is the genre's most common key)
        ScaleType scale       = ScaleType::Aeolian;

        // research section 6 / arrangement_target.json "harmony": "limit
        // to 3-4 chord changes per 32-bar section" -> 8 bars per chord.
        int       barsPerChord = 8;

        // -1 = choose from the seed; otherwise an index into
        // progressionCatalogue().
        int       progressionIndex = -1;

        std::vector<SongSectionSpec> structure = fullMelodicTechnoStructure();
    };

    struct SongNote
    {
        double startBeat   = 0.0; // relative to the owning clip's start
        double lengthBeats = 0.25;
        int    pitch       = 60;  // MIDI note number
        int    velocity    = 100; // 1..127
    };

    struct SongClip
    {
        std::string           name;
        int                   startBar = 0;
        int                   bars     = 0;
        std::vector<SongNote> notes;
    };

    enum class SongTrackRole { Kick, Clap, Hats, Perc, Bass, Pad, Arp, Lead, Fx };

    struct SongTrack
    {
        std::string           name;
        SongTrackRole         role;
        int                   colorIndex = 0; // Ableton Live clip/track colour palette index (0..69)
        std::vector<SongClip> clips;          // one per section that has notes, in time order

        int noteCount() const;
    };

    struct SongSection
    {
        std::string  name;
        MusicSection kind;
        int          startBar = 0;
        int          bars     = 0;
    };

    struct ChordProgression
    {
        const char* name;       // e.g. "i-VII-VI-VII"
        const char* character;
        int         degrees[4]; // 0-based scale degrees of each chord root
    };

    // The diatonic minor progressions the research documents (section 6)
    // plus two more common genre staples. Deterministic order.
    const std::vector<ChordProgression>& progressionCatalogue();

    struct Song
    {
        std::string title;
        double      bpm      = 124.0;
        int         rootNote = 9;
        ScaleType   scale    = ScaleType::Aeolian;
        uint32_t    seed     = 1;
        int         totalBars = 0;

        ChordProgression         progression {};
        std::vector<int>         chordDegreeByBar; // scale degree of the chord root, one per bar
        std::vector<SongSection> sections;
        std::vector<SongTrack>   tracks;

        std::string keyName() const; // "A minor"
        double lengthSeconds() const { return totalBars * 4.0 * 60.0 / bpm; }
    };

    // Deterministic: same params -> byte-identical song.
    Song generateSong(const SongParams& params);

    // General-MIDI / Ableton-Drum-Rack note numbers the drum lanes use, so
    // the tracks also work if a user merges them into one Drum Rack.
    namespace SongDrumNotes
    {
        constexpr int kKick      = 36; // C1
        constexpr int kRim       = 37; // C#1 (perc A)
        constexpr int kClap      = 39; // D#1
        constexpr int kHatClosed = 42; // F#1
        constexpr int kPercB     = 45; // A1  (perc B / low tom)
        constexpr int kHatOpen   = 46; // A#1
    }

    // FX lane pitches - load an FX Drum Rack / one-shot samples on these.
    namespace SongFxNotes
    {
        constexpr int kImpact     = 36; // C1 - hit on the downbeat of each drop / break
        constexpr int kRiser      = 38; // D1 - held through the build into a drop
        constexpr int kDownlifter = 40; // E1 - start of a break / outro
    }
}
