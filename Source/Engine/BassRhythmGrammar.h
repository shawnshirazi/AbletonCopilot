#pragma once

// GENERATED FILE - do not hand-edit.
//
// Regenerate with:
//   MLPipeline/venv/bin/python3 MLPipeline/drum_grammar/generate_bass_grammar_header.py
// which reads MLPipeline/drum_grammar/output/bass_grammar.json's bass_groove_stats
// block - a real per-16th-step bass-note-placement distribution measured from
// vendor-authored bassline MIDI files in four Melodic-Techno-branded sample
// packs already in the user's library (PML Mirage, PML Mystique, Odd Frequency
// Exo 2) - see MLPipeline/drum_grammar/analyze_bass_grammar.py for the
// measurement methodology. Not hand-tuned guesses - if a number here looks
// wrong, fix the analysis or the corpus and regenerate, don't edit this file.
// Generated: 2026-08-14
// n=20 groove-subset bass MIDI files, 856 notes, packs: Odd Frequency Exo2, PML Mirage, PML Mystique
// The raw BS-named MIDI corpus (55 files) was filtered down to this 20-file
// groove subset - excluded files were either continuous "rolling" 16th-note
// basslines (>11 notes/bar, a different production tool/texture, the bass
// equivalent of the HAT corpus's excluded rollers - see DrumRhythmGrammar.h)
// or long sustained/pad-style basslines (mean note length > 2.5 steps - a
// harmonic-pad role, not a rhythmic groove).

namespace Engine
{
    // One dimension per real, measurable characteristic of a groove-style
    // melodic-techno bassline (see analyze_bass_grammar.py). Unlike
    // RoleRhythmStats (DrumRhythmGrammar.h) there is no per-position velocity
    // table - the source MIDI files use a single constant velocity value
    // throughout (verified directly, not assumed), so it carries no real
    // dynamics signal to measure; BassEngine.cpp's own velocity convention is
    // a disclosed design choice, not measured data.
    struct BassRhythmStats
    {
        float step16Probability[16]; // measured fraction of groove-subset bass onsets landing at each position (sums to 1.0)
        float onKickFraction;          // fraction of onsets landing on a kick position (step % 4 == 0) - measured BELOW the 25% uniform baseline: bass genuinely syncopates around the kick
        float meanNotesPerBar;         // real measured average note count per bar (groove subset)
        float meanNoteLenSteps;        // real measured average note length in 16th-note steps - short/punchy, not sustained
        float meanPitchRangeSemitones; // real measured average span between a file's lowest and highest note
        float meanUniquePitches;       // real measured average count of distinct pitches used - close to 2: mostly a repeated root note with an occasional nearby passing tone, not a wandering melody
    };

    // One entry per pitch offset (in semitones, relative to each source
    // file's own modal/most-common pitch - i.e. relative to "the root", for a
    // bassline that's mostly one note) that actually occurred in the
    // groove-subset corpus, most-common first - real evidence for HOW a
    // melodic techno bassline moves off its root: overwhelmingly stays on it
    // (~75%), and when it moves, moves by real diatonic bass intervals (a
    // 3rd/4th/5th away), not an arbitrary chromatic walk.
    struct BassPitchOffset { int offsetSemitones; float probability; };
    constexpr int kNumBassPitchOffsets = 8;
    constexpr BassPitchOffset kBassPitchOffsets[kNumBassPitchOffsets] = {
        { 0, 0.746500f },
        { 3, 0.051400f },
        { -2, 0.049100f },
        { 7, 0.037400f },
        { -4, 0.036200f },
        { -5, 0.028000f },
        { -7, 0.028000f },
        { 2, 0.023400f },
    };

    constexpr BassRhythmStats kBassGrooveRhythm {
        { 0.099300f, 0.061900f, 0.072400f, 0.106300f, 0.045600f, 0.023400f, 0.140200f, 0.052600f, 0.043200f, 0.050200f, 0.058400f, 0.053700f, 0.024500f, 0.043200f, 0.099300f, 0.025700f },
        0.212600f, 6.522300f, 1.317900f, 2.150000f, 1.900000f
    };
}
