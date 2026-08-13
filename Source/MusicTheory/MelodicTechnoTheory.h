#pragma once
#include <JuceHeader.h>
#include "../MelodyCategory.h"

// Static music-theory knowledge for melodic techno specifically (no other
// genres — deliberately scoped, per the actual ask: a real foundation of
// what makes melodic techno sound like melodic techno, not a generic
// "electronic music" grab-bag). Pure data, no DSP and no UI — consumed by
// TheoryAdvisor (Source/Analysis/TheoryAdvisor.h) today, and available for
// the melody/bass generator to read from later instead of its own ad hoc
// constants.
namespace MelodicTechnoTheory
{
    // Interval tables in semitones from the root, 7 degrees.
    // Aeolian (natural minor) is melodic techno's default dark/minor sound.
    // Dorian is the genre's signature "hopeful minor" lift — same as Aeolian
    // but with a raised 6th (index 5: 8 -> 9).
    extern const int kAeolianScale[7];
    extern const int kDorianScale[7];
    extern const int kIonianScale[7]; // plain major - for tracks on a major key

    struct ChordMove
    {
        const char* romanNumerals; // e.g. "i - VI - VII"
        int         degrees[4];    // scale-degree offsets, i = 0
        const char* feel;          // one-line description of when to use it
    };
    extern const ChordMove kProgressions[];
    extern const int       kNumProgressions;

    struct KeyGuidance
    {
        int         rootSemitone; // 0 = C, 1 = C#, ...
        const char* name;         // e.g. "A minor"
    };
    // Genre-typical dark minor keys real melodic techno leans on.
    extern const KeyGuidance kCommonKeys[];
    extern const int         kNumCommonKeys;

    // Descriptive guidance only — surfaced as text by the advisor, never
    // applied to audio automatically.
    extern const char* kSwingGuidance;

    extern const char* kDrumIdiomNotes[];
    extern const int   kNumDrumIdiomNotes;

    // Arrangement section — set manually by the user on the Advisor tab
    // (Ableton doesn't pass locator/marker text to plugins over VST3, so
    // this can't be auto-detected from the host; see AdvisorPanelComponent).
    enum class SongSection { Intro, Build, Drop, Breakdown, Outro };
    constexpr int kNumSongSections = 5;

    struct SectionGuidance
    {
        SongSection section;
        const char* label; // e.g. "Build"
        const char* note;  // what should be happening in this part of the track
    };
    extern const SectionGuidance kSectionGuidance[kNumSongSections]; // indexed by (int) SongSection

    // Each tip tagged with which section(s) it's actually relevant to -
    // sectionMask bit i = (1u << (int) SongSection). Lets the advisor pick
    // from a section-appropriate subset instead of the full pool regardless
    // of what part of the track is playing.
    struct TechniqueTip { const char* text; unsigned sectionMask; };
    extern const TechniqueTip kHiddenTechniques[];
    extern const int          kNumHiddenTechniques;
    inline unsigned sectionBit(SongSection s) { return 1u << (unsigned) s; }
    constexpr unsigned kAllSections = 0x1Fu; // all 5 bits

    // Real genre convention for where each instrument role sits, root-relative
    // semitones (same units as MelodyGridComponent's track offsets) - a hard
    // constraint applied at generation time and checked against existing
    // content, not just advisory text. Bass stays low, leads/plucks sit
    // above it, pads are wide/centered; Fx/Other has no strong convention
    // so it's left effectively unconstrained.
    struct RegisterRange { int minSemitone; int maxSemitone; };
    extern const RegisterRange kCategoryRegister[7]; // indexed by (int) MelodyCategory

    // Semitone-degree -> semitone-offset helper, same convention already
    // used in MelodyGridComponent.cpp's generateMelodyFromMotifShapes:
    // octave-wraps degrees outside 0-6 rather than clamping.
    int degreeToSemitone(int degree, bool useDorian);
}
