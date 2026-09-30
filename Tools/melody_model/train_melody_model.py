#!/usr/bin/env python3
"""Learn a melody model from a corpus of real melodic-techno lead melodies
and write it as a C++ header (Source/Engine/MelodyModelData.h).

  python3 Tools/melody_model/train_melody_model.py corpus.json [out.h]

corpus.json: [{"title","artist","key_guess","notes":[[start_beats, dur_beats, midi], ...]}, ...]

Only aggregate statistics go into the header - no melody from the corpus is
stored or reproduced:
  - 2-bar rhythm templates (16th-grid onsets + lengths), weighted by count
  - scale-step interval transition matrix P(next | previous), -7..+7 steps
  - scale-degree distribution on strong beats / on phrase-final notes
  - 4-bar repetition schemes (which bars restate bar 1)
  - summary stats (steps vs leaps, range, notes per bar, syncopation)
"""
import json
import math
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE.parent.parent / "Source" / "Engine" / "MelodyModelData.h"

NAMES = {"C": 0, "C#": 1, "DB": 1, "D": 2, "D#": 3, "EB": 3, "E": 4, "F": 5, "F#": 6, "GB": 6, "G": 7,
         "G#": 8, "AB": 8, "A": 9, "A#": 10, "BB": 10, "B": 11}
MINOR = [0, 2, 3, 5, 7, 8, 10]
MAX_IV = 7


def parse_key(k):
    """'A minor', 'Am', 'F#m', 'C major' -> (root_pc, is_minor) or None."""
    if not k:
        return None
    m = re.match(r"\s*([A-Ga-g])([#b]?)\s*(m|min|minor|maj|major)?", str(k))
    if not m:
        return None
    root = NAMES[(m.group(1) + m.group(2)).upper()]
    minor = (m.group(3) or "m").lower().startswith("m") and not (m.group(3) or "").lower().startswith("maj")
    return root, minor


def estimate_key(notes):
    """Krumhansl-style fit against natural-minor/major scale membership, weighted by duration."""
    w = Counter()
    for s, d, p in notes:
        w[p % 12] += d
    best = None
    for root in range(12):
        for minor in (True, False):
            scale = MINOR if minor else [0, 2, 4, 5, 7, 9, 11]
            inscale = sum(w[(root + x) % 12] for x in scale)
            tonic = w[root] + 0.5 * w[(root + 7) % 12]
            score = inscale + 0.3 * tonic
            if best is None or score > best[0]:
                best = (score, root, minor)
    return best[1], best[2]


def to_minor_degree(p, root, minor):
    """MIDI pitch -> (absolute scale step, in_scale) relative to the minor tonic (relative minor for major keys)."""
    tonic = root if minor else (root + 9) % 12
    rel = p - tonic
    octave, pc = divmod(rel, 12)
    if pc in MINOR:
        return octave * 7 + MINOR.index(pc), True
    # chromatic: snap down to the scale tone below
    below = max(x for x in MINOR if x < pc)
    return octave * 7 + MINOR.index(below), False


def main():
    corpus = json.load(open(sys.argv[1]))
    rhythms = Counter()
    trans = defaultdict(Counter)
    strong = Counter()
    finals = Counter()
    schemes = Counter()
    steps = leaps = recovered = leap_total = 0
    ranges, per_bar, sync = [], [], []
    used = 0
    for item in corpus:
        notes = sorted((float(s), float(d), int(p)) for s, d, p in item.get("notes", []) if float(d) > 0)
        if len(notes) < 6:
            continue
        key = parse_key(item.get("key_guess")) or estimate_key(notes)
        root, minor = key
        used += 1
        # 16th grid
        q = [(int(round(s * 4)), max(1, int(round(d * 4))), p) for s, d, p in notes]
        start_bar = q[0][0] // 16
        bars = defaultdict(list)
        for st, ln, p in q:
            bars[st // 16 - start_bar].append((st % 16, ln, p))
        nbars = max(bars) + 1
        # rhythms: every 2-bar window with 3..12 notes
        for b in range(0, nbars - 1):
            pat = tuple((st, min(ln, 32 - st)) for st, ln, p in bars.get(b, [])) + \
                  tuple((st + 16, min(ln, 16 - st)) for st, ln, p in bars.get(b + 1, []))
            if 3 <= len(pat) <= 12:
                rhythms[pat] += 1
        per_bar += [len(v) for v in bars.values()]
        sync += [1 if st % 4 != 0 else 0 for st, ln, p in q]
        degs = [to_minor_degree(p, root, minor)[0] for st, ln, p in q]
        ranges.append(max(p for _, _, p in q) - min(p for _, _, p in q))
        ivs = [max(-MAX_IV, min(MAX_IV, degs[i + 1] - degs[i])) for i in range(len(degs) - 1)]
        for i in range(1, len(ivs)):
            trans[ivs[i - 1]][ivs[i]] += 1
        for i, iv in enumerate(ivs):
            if abs(iv) <= 1:
                steps += 1
            else:
                leaps += 1
            if abs(iv) >= 3:
                leap_total += 1
                if i + 1 < len(ivs) and ivs[i + 1] != 0 and (ivs[i + 1] > 0) != (iv > 0) and abs(ivs[i + 1]) <= 2:
                    recovered += 1
        for (st, ln, p), d in zip(q, degs):
            if st % 8 == 0:
                strong[d % 7] += 1
        # phrase finals: last note before a gap of >= 4 steps, or the last note
        for i in range(len(q)):
            end = q[i][0] + q[i][1]
            if i == len(q) - 1 or q[i + 1][0] - end >= 4:
                finals[degs[i] % 7] += 1
        # 4-bar repetition schemes: label bars by (rhythm, contour) identity
        for b0 in range(0, nbars - 3, 4):
            labels, seen = [], {}
            for b in range(b0, b0 + 4):
                sig = tuple((st, to_minor_degree(p, root, minor)[0] - to_minor_degree(bars[b][0][2], root, minor)[0])
                            for st, ln, p in bars.get(b, [])) if bars.get(b) else ()
                if sig not in seen:
                    seen[sig] = "ABCD"[len(seen)]
                labels.append(seen[sig])
            schemes["".join(labels)] += 1

    if used == 0:
        sys.exit("no usable melodies in corpus")

    # ---- write header ----
    top_rhythms = rhythms.most_common(48)
    ivs_range = list(range(-MAX_IV, MAX_IV + 1))

    def row(prev):
        c = trans[prev]
        tot = sum(c.values()) + 0.5 * len(ivs_range)
        return [math.log((c[n] + 0.5) / tot) for n in ivs_range]

    lines = ["#pragma once", "", "// GENERATED by Tools/melody_model/train_melody_model.py - do not edit.",
             f"// Learned from {used} real melodic-techno lead melodies (aggregate statistics only;",
             "// no corpus melody is stored). See MLPipeline/musical_target/melody_corpus_stats.md.", "",
             "namespace Engine", "{", "namespace MelodyModelData", "{",
             f"    constexpr int kMelodiesUsed = {used};",
             f"    constexpr int kMaxInterval = {MAX_IV}; // scale steps",
             f"    constexpr double kStepRatio = {steps / max(1, steps + leaps):.4f};",
             f"    constexpr double kLeapRecovery = {recovered / max(1, leap_total):.4f};",
             f"    constexpr double kMeanNotesPerBar = {sum(per_bar) / max(1, len(per_bar)):.4f};",
             f"    constexpr double kSyncopation = {sum(sync) / max(1, len(sync)):.4f};",
             f"    constexpr double kMedianRange = {sorted(ranges)[len(ranges) // 2]:.1f}; // semitones", "",
             "    // log P(next interval | previous interval), both in scale steps -7..+7",
             f"    constexpr double kIntervalLogProb[{len(ivs_range)}][{len(ivs_range)}] = {{"]
    for prev in ivs_range:
        lines.append("        { " + ", ".join(f"{v:.4f}" for v in row(prev)) + " },")
    lines.append("    };")
    sd = sum(strong.values())
    lines.append("    // log P(scale degree 0..6 of the minor key | strong beat)")
    lines.append("    constexpr double kStrongDegreeLogProb[7] = { " +
                 ", ".join(f"{math.log((strong[d] + 0.5) / (sd + 3.5)):.4f}" for d in range(7)) + " };")
    fd = sum(finals.values())
    lines.append("    // log P(scale degree | phrase-final note)")
    lines.append("    constexpr double kFinalDegreeLogProb[7] = { " +
                 ", ".join(f"{math.log((finals[d] + 0.5) / (fd + 3.5)):.4f}" for d in range(7)) + " };")
    lines.append("")
    lines.append("    struct RhythmTemplate { int count; int notes; int step[12]; int len[12]; };")
    lines.append(f"    constexpr int kNumRhythms = {len(top_rhythms)};")
    lines.append("    constexpr RhythmTemplate kRhythms[] = {")
    for pat, cnt in top_rhythms:
        stp = list(pat) + [(0, 0)] * (12 - len(pat))
        lines.append(f"        {{ {cnt}, {len(pat)}, {{ {', '.join(str(s) for s, _ in stp)} }}, {{ {', '.join(str(l) for _, l in stp)} }} }},")
    lines.append("    };")
    top_schemes = schemes.most_common(8)
    lines.append("")
    lines.append("    struct Scheme { const char* bars; int count; }; // 4-bar repetition pattern, A = restates bar 1")
    lines.append(f"    constexpr int kNumSchemes = {len(top_schemes)};")
    lines.append("    constexpr Scheme kSchemes[] = { " + ", ".join(f'{{ "{s}", {c} }}' for s, c in top_schemes) + " };")
    lines += ["}", "}", ""]
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else OUT
    out.write_text("\n".join(lines))
    print(f"wrote {out}: {used} melodies, {len(top_rhythms)} rhythms, step ratio {steps / max(1, steps + leaps):.2f}, "
          f"schemes {top_schemes[:4]}")


if __name__ == "__main__":
    main()
