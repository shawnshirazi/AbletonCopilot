#!/usr/bin/env python3
"""Learn a melody model from a corpus of real lead melodies and write it as a
C++ header (Source/Engine/MelodyModelData.h).

  python3 Tools/melody_model/train_melody_model.py corpus.json [out.h]

corpus.json entries: {"title","artist","core":bool,"style_auto":"line"|"arp","mode","tonic_pc",
                      "notes":[[start_beats, dur_beats, midi], ...],
                      "chords":[[start_beats, dur_beats, "Root", [intervals], inversion], ...]}

The corpus used is the Hooktheory TheoryTab data published with Sheet Sage
(github.com/chrisdonahue/sheetsage-data, CC BY-NC-SA 3.0) filtered to
instrumental melodic/progressive house, techno and trance leads at 110-140 BPM
- see MLPipeline/musical_target/melody_corpus_stats.md. Only AGGREGATE
statistics are written; no melody from the corpus is stored or reproduced.

Learned per archetype ("line" melodies and "arp"/pedal figures):
  - P(next interval | previous interval), in scale steps -7..+7
  - P(degree relative to the sounding chord's root | strong / weak position)
  - P(key degree | phrase-final note)
  - one-bar rhythm templates (16th-grid onsets + lengths), weighted by count
  - rhythm/pitch repetition rates inside 4-bar groups, anticipation rate
"""
import json
import math
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE.parent.parent / "Source" / "Engine" / "MelodyModelData.h"
MINOR = [0, 2, 3, 5, 7, 8, 10]
MAX_IV = 7
NOTE_PC = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def root_pc(name):
    pc = NOTE_PC[name[0].upper()]
    for ch in name[1:]:
        pc += 1 if ch == "#" else -1 if ch == "b" else 0
    return pc % 12


def degree(pitch, tonic):
    """MIDI pitch -> absolute natural-minor scale step from the tonic (chromatic notes snap down)."""
    octave, pc = divmod(pitch - tonic, 12)
    below = max(x for x in MINOR if x <= pc)
    return octave * 7 + MINOR.index(below)


class Stats:
    def __init__(self):
        self.trans = defaultdict(Counter)
        self.strong = Counter()
        self.weak = Counter()
        self.final = Counter()
        self.rhythms = Counter()
        self.rep_rhythm = Counter()  # bar index (1..3) -> repeats bar 0 rhythm
        self.rep_both = Counter()    # ... with the same relative pitches
        self.groups = 0
        self.anticip = 0
        self.onsets = 0
        self.melodies = 0
        self.steps = self.moves = 0
        self.ranges = []
        self.profiles = []  # per 8-bar window: maxrun, meanrun, range, distinct, dirchange, leaps


def chord_at(chords, beat):
    for s, d, name, ivs, inv in chords:
        if s <= beat < s + d:
            return root_pc(name)
    return None


def learn(entry, st):
    tonic = entry["tonic_pc"] % 12
    notes = sorted((float(s), float(d), int(p)) for s, d, p in entry["notes"] if float(d) > 0)
    if len(notes) < 6:
        return
    st.melodies += 1
    q = [(int(round(s * 4)), max(1, int(round(d * 4))), p) for s, d, p in notes]
    degs = [degree(p, tonic + 12 * (min(x[2] for x in q) // 12 - 1)) for _, _, p in q]
    st.ranges.append(max(p for _, _, p in q) - min(p for _, _, p in q))
    ivs = [max(-MAX_IV, min(MAX_IV, degs[i + 1] - degs[i])) for i in range(len(degs) - 1)]
    for i in range(1, len(ivs)):
        st.trans[ivs[i - 1]][ivs[i]] += 1
    for iv in ivs:
        st.moves += 1
        st.steps += abs(iv) <= 1
    for i, (on, ln, p) in enumerate(q):
        st.onsets += 1
        if on % 4 != 0 and (on % 4) + ln > 4:
            st.anticip += 1
        croot = chord_at(entry.get("chords", []), on / 4.0)
        if croot is not None:
            rel = (degree(p, tonic) - degree(croot + 12 * 10, tonic)) % 7
            (st.strong if (on % 8 == 0 or ln >= 6) else st.weak)[rel] += 1
        end = on + ln
        if i == len(q) - 1 or q[i + 1][0] - end >= 4:
            st.final[degs[i] % 7] += 1
    # phrase-level shape profile per 8-bar window
    maxbar = max(on for on, _, _ in q) // 16
    for w0 in range(0, maxbar + 1, 8):
        ph = [p for on, _, p in q if w0 * 16 <= on < (w0 + 8) * 16]
        if len(ph) < 10:
            continue
        runs, r = [], 1
        for i in range(1, len(ph)):
            if ph[i] == ph[i - 1]:
                r += 1
            else:
                runs.append(r)
                r = 1
        runs.append(r)
        moves = [ph[i + 1] - ph[i] for i in range(len(ph) - 1)]
        nz = [m for m in moves if m != 0]
        dirch = sum(1 for i in range(1, len(nz)) if (nz[i] > 0) != (nz[i - 1] > 0)) / max(1, len(nz) - 1)
        st.profiles.append((max(runs), sum(runs) / len(runs), max(ph) - min(ph), len(set(ph)), dirch,
                            sum(1 for m in moves if abs(m) >= 5) / len(moves)))
    # bars
    bars = defaultdict(list)
    for (on, ln, p), d in zip(q, degs):
        bars[on // 16].append((on % 16, min(ln, 32 - on % 16), d))
    for b, ev in bars.items():
        if 1 <= len(ev) <= 12:
            st.rhythms[tuple((o, l) for o, l, _ in ev)] += 1
    first = min(bars)
    last = max(bars)
    for g in range(first, last - 2, 4):
        if not bars.get(g):
            continue
        st.groups += 1
        r0 = [(o, l) for o, l, _ in bars[g]]
        p0 = [d - bars[g][0][2] for _, _, d in bars[g]]
        for k in (1, 2, 3):
            ev = bars.get(g + k, [])
            if ev and [(o, l) for o, l, _ in ev] == r0:
                st.rep_rhythm[k] += 1
                if [d - ev[0][2] for _, _, d in ev] == p0:
                    st.rep_both[k] += 1


def logp(counter, keys, alpha=0.5):
    tot = sum(counter[k] for k in keys) + alpha * len(keys)
    return [math.log((counter[k] + alpha) / tot) for k in keys]


def emit(name, st, lines):
    ivr = list(range(-MAX_IV, MAX_IV + 1))
    top = st.rhythms.most_common(40)
    lines.append(f"    // ---- {name}: {st.melodies} melodies")
    lines.append(f"    constexpr ArchetypeModel k{name} = {{")
    lines.append(f"        {st.melodies}, {st.steps / max(1, st.moves):.4f}, {st.anticip / max(1, st.onsets):.4f}, "
                 f"{sorted(st.ranges)[len(st.ranges) // 2] if st.ranges else 12},")
    lines.append("        { " + ", ".join(f"{st.rep_rhythm[k] / max(1, st.groups):.3f}" for k in (1, 2, 3)) + " },")
    lines.append("        { " + ", ".join(f"{st.rep_both[k] / max(1, st.rep_rhythm[k]):.3f}" for k in (1, 2, 3)) + " },")
    lines.append("        {")
    for prev in ivr:
        lines.append("            { " + ", ".join(f"{v:.3f}" for v in logp(st.trans[prev], ivr)) + " },")
    lines.append("        },")
    lines.append("        { " + ", ".join(f"{v:.3f}" for v in logp(st.strong, range(7))) + " },")
    lines.append("        { " + ", ".join(f"{v:.3f}" for v in logp(st.weak, range(7))) + " },")
    lines.append("        { " + ", ".join(f"{v:.3f}" for v in logp(st.final, range(7))) + " },")
    def quant(k, qv):
        v = sorted(pr[k] for pr in st.profiles) or [0.0]
        return v[min(len(v) - 1, int(qv * len(v)))]
    lines.append("        { " + ", ".join(f"{quant(k, 0.25):.3f}" for k in range(6)) + " }, // profile p25")
    lines.append("        { " + ", ".join(f"{quant(k, 0.5):.3f}" for k in range(6)) + " }, // profile p50")
    lines.append("        { " + ", ".join(f"{quant(k, 0.75):.3f}" for k in range(6)) + " }, // profile p75")
    lines.append(f"        {len(top)},")
    lines.append("        {")
    for pat, cnt in top:
        pad = list(pat) + [(0, 0)] * (12 - len(pat))
        lines.append(f"            {{ {cnt}, {len(pat)}, {{ {', '.join(str(o) for o, _ in pad)} }}, {{ {', '.join(str(l) for _, l in pad)} }} }},")
    lines.append("        },")
    lines.append("    };")


def main():
    corpus = json.load(open(sys.argv[1]))
    line, arp = Stats(), Stats()
    for e in corpus:
        if not e.get("core") or e.get("mode") not in ("minor", "dorian"):
            continue
        learn(e, arp if e.get("style_auto") == "arp" else line)
    lines = ["#pragma once", "", "// GENERATED by Tools/melody_model/train_melody_model.py - do not edit by hand.",
             "//",
             f"// Aggregate statistics learned from {line.melodies + arp.melodies} real instrumental lead melodies",
             "// (melodic/progressive house, techno and trance, 110-140 BPM, minor/dorian) transcribed in the",
             "// Hooktheory TheoryTab dataset as published with Sheet Sage (github.com/chrisdonahue/sheetsage-data,",
             "// CC BY-NC-SA 3.0). No corpus melody is stored here. See",
             "// MLPipeline/musical_target/melody_corpus_stats.md.", "",
             "namespace Engine", "{", "namespace MelodyModelData", "{",
             "    constexpr int kMaxInterval = 7; // scale steps", "",
             "    struct BarRhythm { int count; int notes; int onset[12]; int len[12]; }; // 16th grid, len may tie past the bar",
             "",
             "    struct ArchetypeModel",
             "    {",
             "        int    melodies;",
             "        double stepRatio;        // |interval| <= 1 scale step, incl. repeats",
             "        double anticipation;     // off-beat onsets held across the next beat",
             "        int    medianRange;      // semitones",
             "        double rhythmRepeat[3];  // P(bar k of a 4-bar group has bar 1's rhythm), k = 2,3,4",
             "        double pitchRepeatGivenRhythm[3]; // ... and also bar 1's relative pitches",
             "        double intervalLogProb[15][15];   // [prev + 7][next + 7]",
             "        double strongChordDegLogProb[7];  // degree above the sounding chord root, strong positions",
             "        double weakChordDegLogProb[7];",
             "        double finalDegreeLogProb[7];     // key degree of phrase-final notes",
             "        // 8-bar phrase shape, interquartile range over the corpus:",
             "        // longest repeated-note run, mean run, range (st), distinct pitches, direction-change rate, leap rate",
             "        double profileP25[6];",
             "        double profileP50[6];",
             "        double profileP75[6];",
             "        int    numRhythms;",
             "        BarRhythm rhythms[40];",
             "    };", ""]
    emit("Line", line, lines)
    emit("Arp", arp, lines)
    lines += ["}", "}", ""]
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else OUT
    out.write_text("\n".join(lines))
    print(f"wrote {out}: line {line.melodies} melodies, step ratio {line.steps / max(1, line.moves):.2f}, "
          f"rhythms {len(line.rhythms)}, rhythm repeat {[round(line.rep_rhythm[k] / max(1, line.groups), 2) for k in (1, 2, 3)]}; "
          f"arp {arp.melodies}")


if __name__ == "__main__":
    main()
