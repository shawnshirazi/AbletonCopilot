#!/usr/bin/env python3
"""
Generates Source/Engine/BassRhythmGrammar.h from
MLPipeline/drum_grammar/output/bass_grammar.json's bass_groove_stats block -
the ONLY place the measured per-16th-step bass-note-placement distribution
used by Engine::generateBassPattern (Source/Engine/BassEngine.cpp) comes
from. See analyze_bass_grammar.py for the measurement methodology (real
vendor-authored bassline MIDI files from the user's own Melodic Techno
sample-pack library).

Run this whenever bass_grammar.json is regenerated (analyze_bass_grammar.py)
to keep the C++ generator's grammar in sync with the real measurement.
Never hand-edit the generated header - if a number there looks wrong, fix
the analysis or the corpus, then regenerate.
"""

import datetime
import json
from pathlib import Path

JSON_PATH = Path(__file__).parent / "output" / "bass_grammar.json"
HEADER_PATH = Path(__file__).parents[2] / "Source" / "Engine" / "BassRhythmGrammar.h"


def fmt16(values):
    return "{ " + ", ".join(f"{v:.6f}f" for v in values) + " }"


def main():
    data = json.loads(JSON_PATH.read_text())
    s = data["bass_groove_stats"]

    lines = []
    lines.append("#pragma once")
    lines.append("")
    lines.append("// GENERATED FILE - do not hand-edit.")
    lines.append("//")
    lines.append("// Regenerate with:")
    lines.append("//   MLPipeline/venv/bin/python3 MLPipeline/drum_grammar/generate_bass_grammar_header.py")
    lines.append("// which reads MLPipeline/drum_grammar/output/bass_grammar.json's bass_groove_stats")
    lines.append("// block - a real per-16th-step bass-note-placement distribution measured from")
    lines.append("// vendor-authored bassline MIDI files in four Melodic-Techno-branded sample")
    lines.append("// packs already in the user's library (PML Mirage, PML Mystique, Odd Frequency")
    lines.append("// Exo 2) - see MLPipeline/drum_grammar/analyze_bass_grammar.py for the")
    lines.append("// measurement methodology. Not hand-tuned guesses - if a number here looks")
    lines.append("// wrong, fix the analysis or the corpus and regenerate, don't edit this file.")
    lines.append(f"// Generated: {datetime.date.today().isoformat()}")
    lines.append(f"// n={s['num_files']} groove-subset bass MIDI files, {s['total_notes']} notes, "
                  f"packs: {', '.join(s['packs'])}")
    lines.append("// The raw BS-named MIDI corpus (55 files) was filtered down to this 20-file")
    lines.append("// groove subset - excluded files were either continuous \"rolling\" 16th-note")
    lines.append("// basslines (>11 notes/bar, a different production tool/texture, the bass")
    lines.append("// equivalent of the HAT corpus's excluded rollers - see DrumRhythmGrammar.h)")
    lines.append("// or long sustained/pad-style basslines (mean note length > 2.5 steps - a")
    lines.append("// harmonic-pad role, not a rhythmic groove).")
    lines.append("")
    lines.append("namespace Engine")
    lines.append("{")
    lines.append("    // One dimension per real, measurable characteristic of a groove-style")
    lines.append("    // melodic-techno bassline (see analyze_bass_grammar.py). Unlike")
    lines.append("    // RoleRhythmStats (DrumRhythmGrammar.h) there is no per-position velocity")
    lines.append("    // table - the source MIDI files use a single constant velocity value")
    lines.append("    // throughout (verified directly, not assumed), so it carries no real")
    lines.append("    // dynamics signal to measure; BassEngine.cpp's own velocity convention is")
    lines.append("    // a disclosed design choice, not measured data.")
    lines.append("    struct BassRhythmStats")
    lines.append("    {")
    lines.append("        float step16Probability[16]; // measured fraction of groove-subset bass onsets landing at each position (sums to 1.0)")
    lines.append("        float onKickFraction;          // fraction of onsets landing on a kick position (step % 4 == 0) - measured BELOW the 25% uniform baseline: bass genuinely syncopates around the kick")
    lines.append("        float meanNotesPerBar;         // real measured average note count per bar (groove subset)")
    lines.append("        float meanNoteLenSteps;        // real measured average note length in 16th-note steps - short/punchy, not sustained")
    lines.append("        float meanPitchRangeSemitones; // real measured average span between a file's lowest and highest note")
    lines.append("        float meanUniquePitches;       // real measured average count of distinct pitches used - close to 2: mostly a repeated root note with an occasional nearby passing tone, not a wandering melody")
    lines.append("    };")
    lines.append("")
    lines.append("    // One entry per pitch offset (in semitones, relative to each source")
    lines.append("    // file's own modal/most-common pitch - i.e. relative to \"the root\", for a")
    lines.append("    // bassline that's mostly one note) that actually occurred in the")
    lines.append("    // groove-subset corpus, most-common first - real evidence for HOW a")
    lines.append("    // melodic techno bassline moves off its root: overwhelmingly stays on it")
    lines.append("    // (~75%), and when it moves, moves by real diatonic bass intervals (a")
    lines.append("    // 3rd/4th/5th away), not an arbitrary chromatic walk.")
    lines.append("    struct BassPitchOffset { int offsetSemitones; float probability; };")
    offsets = s["pitch_offset_distribution"]
    lines.append(f"    constexpr int kNumBassPitchOffsets = {len(offsets)};")
    lines.append(f"    constexpr BassPitchOffset kBassPitchOffsets[kNumBassPitchOffsets] = {{")
    for entry in offsets:
        lines.append(f"        {{ {entry['offsetSemitones']}, {entry['probability']:.6f}f }},")
    lines.append("    };")
    lines.append("")
    lines.append(f"    constexpr BassRhythmStats kBassGrooveRhythm {{")
    lines.append(f"        {fmt16(s['step16_hit_probability'])},")
    lines.append(f"        {s['on_kick_fraction']:.6f}f, {s['mean_notes_per_bar']:.6f}f, "
                  f"{s['mean_note_len_steps']:.6f}f, {s['mean_pitch_range_semitones']:.6f}f, "
                  f"{s['mean_unique_pitches']:.6f}f")
    lines.append("    };")
    lines.append("}")
    lines.append("")

    HEADER_PATH.write_text("\n".join(lines))
    print(f"Wrote {HEADER_PATH}")


if __name__ == "__main__":
    main()
