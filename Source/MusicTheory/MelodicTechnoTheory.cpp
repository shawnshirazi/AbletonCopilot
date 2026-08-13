#include "MelodicTechnoTheory.h"

namespace MelodicTechnoTheory
{
    const int kAeolianScale[7] = { 0, 2, 3, 5, 7, 8, 10 };
    const int kDorianScale[7]  = { 0, 2, 3, 5, 7, 9, 10 }; // raised 6th vs Aeolian
    const int kIonianScale[7]  = { 0, 2, 4, 5, 7, 9, 11 }; // plain major

    const ChordMove kProgressions[] = {
        { "i - VI - III - VII", { 0, 5, 2, 6 },
          "Rolling, forward-moving loop - good under a driving bassline." },
        { "i - v - VI - iv", { 0, 4, 5, 3 },
          "Darker, more minor-key tension - good for a moodier lead." },
        { "i - VII - VI - v", { 0, 6, 5, 4 },
          "Descending, hypnotic - classic slow-build melodic techno feel." },
        { "i - i - VI - VII", { 0, 0, 5, 6 },
          "Static/minimal with a late lift - good when the bass should stay put." },
    };
    const int kNumProgressions = (int) (sizeof(kProgressions) / sizeof(kProgressions[0]));

    const KeyGuidance kCommonKeys[] = {
        { 9,  "A minor" },
        { 5,  "F minor" },
        { 7,  "G minor" },
        { 0,  "C minor" },
        { 2,  "D minor" },
        { 4,  "E minor" },
    };
    const int kNumCommonKeys = (int) (sizeof(kCommonKeys) / sizeof(kCommonKeys[0]));

    const char* kSwingGuidance =
        "Swing is measured Ableton-style: 50% = dead straight, 66% = full triplet feel. "
        "Tech house typically runs 10-30%; melodic techno stays noticeably lighter - a real "
        "deep/melodic progressive techno reference track measured at just 10% swing (\"Swing "
        "Maschine\"), enough to humanize the groove without pushing it toward house.";

    const char* kDrumIdiomNotes[] = {
        "Kick: four-on-the-floor, short and snappy (300-500ms decay, 60-80Hz body) - it's the "
        "anchor, not the feature, and its decay is exactly why the bass avoids the downbeat.",
        "Clap/snare: sparse and asymmetric, not a steady 2-and-4 backbeat - a real reference "
        "breakdown used a single hit at the 2-bar mark and a double hit at the 4-bar mark. "
        "Predictable backbeat placement reads as house, not melodic techno.",
        "Hi-hats: closed-hat pulse on the off-beats, layered with a second closed-hat pass at "
        "reduced velocity for organic dynamics, plus an off-beat open-hat accent - and sidechain "
        "the hats to the kick too, not just the bass, for separation.",
        "Percussion: real production leans on tuned/detuned one-shots for character - a conga "
        "pitched down ~2 semitones, a mid tom pitched up ~7 semitones, maracas as steady 16ths "
        "with velocity variation, and rim hits nudged slightly off-grid rather than quantized dead-on.",
    };
    const int kNumDrumIdiomNotes = (int) (sizeof(kDrumIdiomNotes) / sizeof(kDrumIdiomNotes[0]));

    const SectionGuidance kSectionGuidance[kNumSongSections] = {
        { SongSection::Intro,     "Intro",
          "8-16 bars, minimal - one or two elements easing in (usually kick + a hint of the main "
          "groove element). Don't judge sub/air balance here, it's meant to be thin." },
        { SongSection::Build,     "Build",
          "~16 bars (two 8-bar phrases) - thin out the low end, automate a filter sweep opening "
          "over the full build, add rising percussion/riser energy toward the drop." },
        { SongSection::Drop,      "Drop",
          "Structured in 8-bar segments (A/B/C/D-style) - full arrangement, kick/bass/hats locked "
          "in. This is the section where the sub-bass and top-end spectral targets actually apply." },
        { SongSection::Breakdown, "Breakdown",
          "~24 bars (three 8-bar phrases) - pull the kick out or go half-time, give the lead/pad "
          "room to breathe. Sparse is the point here, not a problem to fix." },
        { SongSection::Outro,     "Outro",
          "Strip back the same way the intro built up, in 8-bar steps - let elements drop out one "
          "at a time rather than cutting hard." },
    };

    const TechniqueTip kHiddenTechniques[] = {
        { "Call-and-response: keep the bass off the kick's downbeat (rest where "
          "the kick hits) so the two lock together instead of fighting.", kAllSections },
        { "Root-fifth pedal: let the bass sit mostly on the root and fifth, with "
          "occasional passing tones for tension - constant movement reads as busy, not melodic.",
          kAllSections },
        { "Filter-sweep builds: automate a lowpass cutoff opening over 8-16 bars "
          "on a pad or lead into a section change instead of adding new elements abruptly.",
          sectionBit(SongSection::Build) },
        { "Motif + variation: take a short 3-5 note phrase and develop it (transpose, "
          "invert, stretch the rhythm) across the arrangement rather than writing new "
          "melodies for every section - repetition with small changes is what makes it hypnotic.",
          kAllSections },
        { "Restraint: space is a tool - a bar of near-silence before a phrase lands "
          "makes the next hit read as more melodic, not less.",
          sectionBit(SongSection::Intro) | sectionBit(SongSection::Breakdown) | sectionBit(SongSection::Outro) },
        { "Resolve phrases to the root or fifth - a melody that always drifts and "
          "never lands reads as random even when every note is in key.", kAllSections },
    };
    const int kNumHiddenTechniques = (int) (sizeof(kHiddenTechniques) / sizeof(kHiddenTechniques[0]));

    int degreeToSemitone(int degree, bool useDorian)
    {
        const int* scale  = useDorian ? kDorianScale : kAeolianScale;
        const int  octave = degree >= 0 ? degree / 7 : (degree - 6) / 7;
        const int  idx    = ((degree % 7) + 7) % 7;
        return octave * 12 + scale[idx];
    }

    // Indexed by (int) MelodyCategory: Bass, Lead, Pad, Pluck, Synth, Fx, Other.
    const RegisterRange kCategoryRegister[7] = {
        { -12, 10 },  // Bass - root octave down to the flat-7 above root (needs the fifth/flat-7
                      // reachable - real basslines lean on them, per Myloops' construction guide)
        { -2,  14 },  // Lead - mostly above root
        { -7,  12 },  // Pad - wide, centered
        { 0,   19 },  // Pluck - bright, upper register
        { -5,  15 },  // Synth - flexible mid-high
        { -24, 24 },  // Fx - no strong convention, left wide
        { -24, 24 },  // Other - no strong convention, left wide
    };
}
