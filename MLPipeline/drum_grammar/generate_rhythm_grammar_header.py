#!/usr/bin/env python3
"""
Generates Source/Engine/DrumRhythmGrammar.h from
MLPipeline/drum_grammar/output/drum_grammar.json's role_rhythm_stats and
cross_role_aggregate_vector_correlation blocks - the ONLY place the
measured per-16th-step hit-probability/velocity distributions and
cross-role correlation coefficients used by Engine::generateDrop
(Source/Engine/DrumEngine.cpp) come from.

Run this whenever drum_grammar.json is regenerated (analyze_drum_grammar.py)
to keep the C++ generator's grammar in sync with the real measurement.
Never hand-edit the generated header - if a number there looks wrong, fix
the analysis or the corpus, then regenerate.
"""

import datetime
import json
from pathlib import Path

JSON_PATH = Path(__file__).parent / "output" / "drum_grammar.json"
HEADER_PATH = Path(__file__).parents[2] / "Source" / "Engine" / "DrumRhythmGrammar.h"

ROLES = ["KICK", "CLAP", "HAT", "PERC"]
VAR_NAME = {"KICK": "kKickRhythm", "CLAP": "kClapRhythm", "HAT": "kHatRhythm", "PERC": "kPercRhythm"}


def fmt16(values):
    return "{ " + ", ".join(f"{v:.6f}f" for v in values) + " }"


def main():
    data = json.loads(JSON_PATH.read_text())
    stats = data["role_rhythm_stats"]
    corr = data["cross_role_aggregate_vector_correlation"]

    lines = []
    lines.append("#pragma once")
    lines.append("")
    lines.append("// GENERATED FILE - do not hand-edit.")
    lines.append("//")
    lines.append("// Regenerate with:")
    lines.append("//   MLPipeline/venv/bin/python3 MLPipeline/drum_grammar/generate_rhythm_grammar_header.py")
    lines.append("// which reads MLPipeline/drum_grammar/output/drum_grammar.json's")
    lines.append("// role_rhythm_stats and cross_role_aggregate_vector_correlation blocks -")
    lines.append("// real per-16th-step hit-probability/velocity distributions and")
    lines.append("// cross-role correlation coefficients, measured by onset-detecting real")
    lines.append("// audio loops from four Melodic-Techno-branded sample packs already in")
    lines.append("// the user's library (PML Mirage, PML Mystique, Odd Frequency Exo, Odd")
    lines.append("// Frequency Exo 2) - see MLPipeline/drum_grammar/analyze_drum_grammar.py")
    lines.append("// for the measurement methodology. Not hand-tuned guesses - if a number")
    lines.append("// here looks wrong, fix the analysis or the corpus and regenerate, don't")
    lines.append("// edit this file.")
    lines.append(f"// Generated: {datetime.date.today().isoformat()}")
    for role in ROLES:
        s = stats[role]
        lines.append(f"// {role}: n={s['num_files']} loop files, {s['total_onsets']} onsets, "
                      f"{s['total_bars_analyzed']} bars analyzed, packs: {', '.join(s['packs'])}")
    lines.append("")
    lines.append('#include "DrumVoiceSynth.h" // DrumRole')
    lines.append("")
    lines.append("namespace Engine")
    lines.append("{")
    lines.append("    // One dimension per real, measurable rhythmic characteristic (see")
    lines.append("    // analyze_drum_grammar.py's compute_role_stats) - step16Probability and")
    lines.append("    // step16RelativeVelocity are indexed by 16th-note position within one")
    lines.append("    // bar (0-15), folded across every bar in the analyzed corpus.")
    lines.append("    struct RoleRhythmStats")
    lines.append("    {")
    lines.append("        float step16Probability[16];      // measured fraction of this role's onsets landing at each position (sums to 1.0)")
    lines.append("        float step16RelativeVelocity[16];  // measured mean onset amplitude at each position, relative to that file's own loudest hit (0..1)")
    lines.append("        float onBeatFraction;               // fraction of onsets landing exactly on a beat (step % 4 == 0)")
    lines.append("        float eighthOffbeatFraction;        // fraction landing on the 8th-note offbeat (step % 4 == 2)")
    lines.append("        float weakSixteenthFraction;        // fraction landing on the weakest 16ths (step % 4 in {1, 3})")
    lines.append("        float adjacentBarsIdenticalFraction; // -1 = not enough multi-bar material to measure")
    lines.append("        float meanOnsetsPerBar;              // real measured average hit count per bar")
    lines.append("    };")
    lines.append("")
    lines.append("    // Real measured Pearson correlation between each role-pair's aggregate")
    lines.append("    // 16-step hit-probability vectors (corpus-wide, not tied to one song) -")
    lines.append("    // positive means the two roles tend to favor the same positions,")
    lines.append("    // negative means one favors positions the other avoids.")
    lines.append("    struct CrossRoleCorrelation")
    lines.append("    {")
    lines.append("        float kickClap, kickHat, kickPerc, clapHat, clapPerc, hatPerc;")
    lines.append("    };")
    lines.append("")

    for role in ROLES:
        s = stats[role]
        abi = s["adjacent_bars_identical_fraction"]
        abi_val = -1.0 if abi is None else abi
        lines.append(f"    constexpr RoleRhythmStats {VAR_NAME[role]} {{")
        lines.append(f"        {fmt16(s['step16_hit_probability'])},")
        lines.append(f"        {fmt16(s['step16_mean_relative_velocity'])},")
        lines.append(f"        {s['on_beat_fraction']:.6f}f, {s['eighth_offbeat_fraction']:.6f}f, "
                      f"{s['weak_16th_fraction']:.6f}f, {abi_val:.6f}f, {s['mean_onsets_per_bar']:.6f}f")
        lines.append("    };")
        lines.append("")

    lines.append(f"    constexpr CrossRoleCorrelation kCrossRoleCorrelation {{")
    lines.append(f"        {corr['KICK<->CLAP']:.6f}f, {corr['KICK<->HAT']:.6f}f, {corr['KICK<->PERC']:.6f}f, "
                  f"{corr['CLAP<->HAT']:.6f}f, {corr['CLAP<->PERC']:.6f}f, {corr['HAT<->PERC']:.6f}f")
    lines.append("    };")
    lines.append("")

    lines.append("    inline const RoleRhythmStats& rhythmStatsForRole(DrumRole role)")
    lines.append("    {")
    lines.append("        switch (role)")
    lines.append("        {")
    lines.append(f"            case DrumRole::Kick:  return {VAR_NAME['KICK']};")
    lines.append(f"            case DrumRole::Clap:  return {VAR_NAME['CLAP']};")
    lines.append(f"            case DrumRole::Hat:   return {VAR_NAME['HAT']};")
    lines.append(f"            case DrumRole::Perc:  return {VAR_NAME['PERC']};")
    lines.append(f"            case DrumRole::Count: return {VAR_NAME['KICK']};")
    lines.append("        }")
    lines.append(f"        return {VAR_NAME['KICK']};")
    lines.append("    }")
    lines.append("}")
    lines.append("")

    HEADER_PATH.write_text("\n".join(lines))
    print(f"Wrote {HEADER_PATH}")


if __name__ == "__main__":
    main()
