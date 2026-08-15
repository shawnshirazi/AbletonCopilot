#!/usr/bin/env python3
"""
Offline Melodic Techno drum-analysis pipeline ("drum grammar" milestone).

Lives entirely outside the JUCE plugin. Produces a real, inspectable
dataset + statistics - not a black box, not hardcoded genre lore, no
neural network. Every number in the output is counted from real audio.

Corpus: real, already-licensed Melodic Techno sample-pack material from
the user's own library (/Users/shawnshirazi/shawn music stuff) -
specifically the per-role LOOP folders from four Melodic-Techno-branded
packs (PML Mirage PML341, PML Mystique PML354, Odd Frequency Exo, Odd
Frequency Exo 2). Each loop file is already a single isolated drum role
(the pack vendor split them that way for production use), not a mixed
multi-instrument recording - so onset detection runs directly on the raw
waveform. This is a materially cleaner signal than source-separating a
mixed "drums" bus (compare MLPipeline/fragment_lib.py's
extract_drum_patterns, built for a single user-supplied MIXED reference
track where Demucs + frequency-band heuristics are the only option).

Honesty notes (read before trusting any single number):
  - This is real audio evidence, not a controlled listening-test corpus.
    Different packs mix differently; onset detection can miss quiet ghost
    hits or double-trigger a long, ringy transient.
  - Cross-role relationships are measured TWO ways: (a) aggregate 16-step
    probability-vector correlation across each role's whole corpus
    (always available, corpus-wide, not tied to any one song), and
    (b) direct co-occurrence on same-BPM/same-theme file PAIRS across
    role folders within one pack (only available where the vendor
    happened to name companion loops consistently - report the actual
    pair count, which is small; treat it as a spot-check, not the primary
    evidence).
  - No kick+hat+clap+perc file exists together in one waveform anywhere
    in this corpus, so no measurement here required source separation.
"""

import json
import re
import sys
from collections import defaultdict
from pathlib import Path

import numpy as np
import soundfile as sf
import librosa

LIB_ROOT = Path("/Users/shawnshirazi/shawn music stuff")
OUT_DIR = Path(__file__).parent / "output"

STEPS_PER_BAR = 16

# ---------------------------------------------------------------------------
# Corpus manifest - every directory analyzed, named explicitly so the
# dataset's provenance is auditable at a glance, not hidden in glob logic.
# ---------------------------------------------------------------------------

MIRAGE = LIB_ROOT / "FL studio/Techno/PML - Melodic Techno - Sound Pack - Mirage (PML341)/PML - Melodic Techno - Sample Pack"
MYSTIQUE = LIB_ROOT / "FL studio/Techno/PML - Melodic Techno - Sound Pack - Mystique (PML354)/PML - Melodic Techno - Sample Pack"
EXO = LIB_ROOT / "FL studio/Techno/Odd Frequency - Modern Melodic Techno Mega Bundle/Odd Frequency - Exo - Full Bundle/Odd Frequency - Exo - Sample Pack"
EXO2 = LIB_ROOT / "FL studio/Techno/Odd Frequency - Modern Melodic Techno Mega Bundle/Odd Frequency - Exo 2 - Full Bundle/Odd Frequency - Exo 2 - Sample Pack"

# role -> [(pack label, loop directory), ...]. KICK/CLAP/HAT/PERC are the
# four roles DrumEngine actually generates for; TOP/RIDE are auxiliary
# "supporting texture" evidence, reported separately, never blended into
# the four primary roles' statistics.
LOOP_CORPUS = {
    "KICK": [
        ("PML Mirage",   MIRAGE / "LOOPS/KICKS"),
        ("PML Mystique", MYSTIQUE / "Loops/Kick Loops"),
    ],
    "CLAP": [
        ("PML Mirage",   MIRAGE / "LOOPS/CLAPS SNARES"),
        ("PML Mystique", MYSTIQUE / "Loops/Clap & Snare Loops"),
    ],
    "HAT": [
        ("PML Mirage",   MIRAGE / "LOOPS/HIHATS"),
        ("PML Mystique", MYSTIQUE / "Loops/Hat & Shaker Loops"),
        ("Odd Frequency Exo",  EXO / "Drum Loops/Hat Loop"),
        ("Odd Frequency Exo2", EXO2 / "Drum Loops/Hat Loops"),
    ],
    "PERC": [
        ("PML Mirage",   MIRAGE / "LOOPS/MISC PERCS TOMS"),
        ("PML Mystique", MYSTIQUE / "Loops/Percussion Loops"),
        ("Odd Frequency Exo",  EXO / "Drum Loops/Perc Loops"),
        ("Odd Frequency Exo2", EXO2 / "Drum Loops/Perc Loops"),
    ],
    "TOP": [
        ("PML Mystique", MYSTIQUE / "Loops/Top Loops"),
        ("Odd Frequency Exo",  EXO / "Drum Loops/Top Loops"),
        ("Odd Frequency Exo2", EXO2 / "Drum Loops/Top Loops"),
    ],
    "RIDE": [
        ("PML Mirage",   MIRAGE / "LOOPS/RIDES"),
        ("Odd Frequency Exo",  EXO / "Drum Loops/Ride Loops"),
        ("Odd Frequency Exo2", EXO2 / "Drum Loops/Ride Loops"),
    ],
}

# role -> [(pack label, one-shot directory), ...] for the Phase 4 sound
# fingerprint - completely separate corpus from the loop corpus above.
ONESHOT_CORPUS = {
    "KICK": [
        ("PML Mirage",   MIRAGE / "ONESHOTS/KICKS"),
        ("PML Mystique", MYSTIQUE / "One Shots/Kicks"),
        ("Odd Frequency Exo",  EXO / "Drums/Kicks"),
        ("Odd Frequency Exo2", EXO2 / "Drums/Kicks"),
    ],
    "CLAP": [
        ("PML Mirage",   MIRAGE / "ONESHOTS/CLAPS"),
        ("PML Mystique", MYSTIQUE / "One Shots/Claps"),
        ("Odd Frequency Exo",  EXO / "Drums/Claps"),
        ("Odd Frequency Exo2", EXO2 / "Drums/Claps"),
    ],
    "HAT": [
        ("PML Mirage",   MIRAGE / "ONESHOTS/HIHATS"),
        ("PML Mystique", MYSTIQUE / "One Shots/Hats & Shaker"),
        ("Odd Frequency Exo",  EXO / "Drums/Closed Hats"),
        ("Odd Frequency Exo2", EXO2 / "Drums/Closed Hats"),
    ],
    "PERC": [
        ("PML Mirage",   MIRAGE / "ONESHOTS/PERCUSSIONS"),
        ("PML Mystique", MYSTIQUE / "One Shots/Percussions"),
        ("Odd Frequency Exo",  EXO / "Drums/Percs"),
        ("Odd Frequency Exo2", EXO2 / "Drums/Percs"),
    ],
    # Added for the layered hat-hierarchy milestone (OPEN HAT is now its
    # own DrumRole, distinct from closed HAT - see Source/Engine/
    # DrumVoiceSynth.h). Odd Frequency Exo/Exo2 both have a dedicated
    # "Open Hats" one-shot folder; PML Mirage/Mystique don't split open
    # hats out separately, but their RIDES one-shot folders are real,
    # genre-relevant "open, ringing, longer-decay high-frequency
    # percussion" material - the same acoustic character an open hat/ride
    # accent needs, so they're included here as a legitimate stand-in
    # rather than leaving OPEN_HAT with zero measured target data.
    "OPEN_HAT": [
        ("Odd Frequency Exo",  EXO / "Drums/Open Hats"),
        ("Odd Frequency Exo2", EXO2 / "Drums/Open Hats"),
        ("PML Mirage",   MIRAGE / "ONESHOTS/RIDES"),
        ("PML Mystique", MYSTIQUE / "One Shots/Rides"),
    ],
}

BPM_RE = re.compile(r"(\d{2,3})\s*bpm", re.IGNORECASE)


def parse_bpm(name: str):
    m = BPM_RE.search(name)
    return float(m.group(1)) if m else None


def load_mono(path: Path):
    y, sr = sf.read(str(path), always_2d=False)
    if y.ndim > 1:
        y = y.mean(axis=1)
    return y.astype(np.float64), sr


def analyze_loop_file(path: Path, role: str):
    """One loop WAV -> a per-file record of quantized onset steps, or None
    if it can't be analyzed (no BPM in filename, unreadable, or silent)."""
    bpm = parse_bpm(path.name)
    if bpm is None or bpm <= 0:
        return None

    try:
        y, sr = load_mono(path)
    except Exception as e:
        print(f"    [skip] {path.name}: {e}", file=sys.stderr)
        return None

    if len(y) < sr * 0.05 or np.max(np.abs(y)) < 1e-6:
        return None

    duration = len(y) / sr
    sec_per_step = (60.0 / bpm) / 4.0
    bar_secs = sec_per_step * STEPS_PER_BAR
    num_bars = max(1, int(round(duration / bar_secs)))
    total_steps = num_bars * STEPS_PER_BAR

    onset_times = librosa.onset.onset_detect(
        y=y.astype(np.float32), sr=sr, units="time", backtrack=False,
        wait=max(1, int(round(0.035 * sr / 512))), hop_length=512,
    )

    # librosa's peak-picking onset detector structurally can't flag a hit
    # that starts AT sample 0 with no lead-in silence - peak-picking needs
    # a preceding lower-energy frame to register a "rise", and frame 0 has
    # none. Loop packs routinely start exactly on the downbeat transient
    # (verified directly against several kick loops in this corpus: peak
    # amplitude within the first 20ms is >90% of the file's overall peak),
    # so without this fix every loop would silently lose its very first
    # hit - not a real musical absence, a detector blind spot.
    first_ms = y[: int(sr * 0.015)]
    if len(first_ms) and np.max(np.abs(first_ms)) >= 0.4 * np.max(np.abs(y)):
        if len(onset_times) == 0 or onset_times[0] > 0.03:
            onset_times = np.concatenate([[0.0], onset_times])

    if len(onset_times) == 0:
        return None

    def amp_at(t):
        chunk = y[int(t * sr):int(t * sr) + int(0.02 * sr)]
        return float(np.max(np.abs(chunk))) if len(chunk) else 0.0

    amps = np.array([amp_at(t) for t in onset_times])
    peak = float(np.max(amps)) if len(amps) else 1.0
    rel_vel = amps / peak if peak > 0 else amps

    steps, vels = [], []
    for t, v in zip(onset_times, rel_vel):
        step = int(round(t / sec_per_step))
        if 0 <= step < total_steps:
            steps.append(step)
            vels.append(float(v))

    if not steps:
        return None

    return {
        "path": str(path),
        "pack": None,  # filled by caller
        "bpm": bpm,
        "duration": duration,
        "num_bars": num_bars,
        "total_steps": total_steps,
        "steps": steps,
        "velocities": vels,
    }


def fold_to_16(step, total_steps):
    return step % STEPS_PER_BAR


def analyze_role_corpus(role: str, dirs):
    records = []
    for pack_label, d in dirs:
        if not d.is_dir():
            print(f"  [WARN] missing directory for {role}/{pack_label}: {d}", file=sys.stderr)
            continue
        wavs = sorted(p for p in d.glob("*.wav"))
        for p in wavs:
            rec = analyze_loop_file(p, role)
            if rec is None:
                continue
            rec["pack"] = pack_label
            records.append(rec)
    return records


def bar_pattern(rec):
    """List[List[bool]] - one list of 16 booleans per bar in this file."""
    bars = [[False] * STEPS_PER_BAR for _ in range(rec["num_bars"])]
    for s in rec["steps"]:
        bar = s // STEPS_PER_BAR
        pos = s % STEPS_PER_BAR
        if bar < len(bars):
            bars[bar][pos] = True
    return bars


def compute_role_stats(role: str, records):
    step16_counts = np.zeros(STEPS_PER_BAR)
    step16_vel_sum = np.zeros(STEPS_PER_BAR)
    step16_vel_n = np.zeros(STEPS_PER_BAR)
    total_onsets = 0
    total_bars = 0
    identical_adjacent_bars = 0
    adjacent_bar_pairs = 0
    per_file_density = []

    for rec in records:
        for s, v in zip(rec["steps"], rec["velocities"]):
            pos = fold_to_16(s, rec["total_steps"])
            step16_counts[pos] += 1
            step16_vel_sum[pos] += v
            step16_vel_n[pos] += 1
            total_onsets += 1

        bars = bar_pattern(rec)
        total_bars += len(bars)
        per_file_density.append(len(rec["steps"]) / max(1, rec["num_bars"]))
        for i in range(len(bars) - 1):
            adjacent_bar_pairs += 1
            if bars[i] == bars[i + 1]:
                identical_adjacent_bars += 1

    step16_prob = (step16_counts / total_onsets).tolist() if total_onsets else [0.0] * STEPS_PER_BAR
    step16_vel = [
        (step16_vel_sum[i] / step16_vel_n[i]) if step16_vel_n[i] else 0.0
        for i in range(STEPS_PER_BAR)
    ]

    strong = sum(step16_counts[i] for i in range(STEPS_PER_BAR) if i % 4 == 0)
    eighth_off = sum(step16_counts[i] for i in range(STEPS_PER_BAR) if i % 4 == 2)
    weak16 = sum(step16_counts[i] for i in range(STEPS_PER_BAR) if i % 4 in (1, 3))

    return {
        "num_files": len(records),
        "packs": sorted({r["pack"] for r in records}),
        "total_onsets": int(total_onsets),
        "total_bars_analyzed": int(total_bars),
        "mean_onsets_per_bar": float(np.mean(per_file_density)) if per_file_density else 0.0,
        "step16_hit_probability": [round(x, 4) for x in step16_prob],
        "step16_mean_relative_velocity": [round(x, 4) for x in step16_vel],
        "on_beat_fraction": round(strong / total_onsets, 4) if total_onsets else 0.0,
        "eighth_offbeat_fraction": round(eighth_off / total_onsets, 4) if total_onsets else 0.0,
        "weak_16th_fraction": round(weak16 / total_onsets, 4) if total_onsets else 0.0,
        "adjacent_bar_pairs_analyzed": int(adjacent_bar_pairs),
        "adjacent_bars_identical_fraction": round(identical_adjacent_bars / adjacent_bar_pairs, 4) if adjacent_bar_pairs else None,
    }


def extract_theme(name: str):
    """Best-effort 'theme' token from a PML loop filename, used only to
    pair companion loops released together (same theme + BPM) across role
    folders for the direct co-occurrence check. Returns None if no theme
    token is present (many hat/perc loops are just numbered)."""
    stem = Path(name).stem
    # Strip a leading pack/role/number/BPM/key run, keep whatever's left.
    parts = re.split(r"_\d+BPM_?", stem, flags=re.IGNORECASE)
    if len(parts) < 2:
        return None
    tail = parts[-1]
    tail = re.sub(r"^[A-G]#?_", "", tail)  # drop a leading musical key token like "F#_"
    tail = tail.strip("_ ")
    return tail.lower() if tail else None


def cross_role_pairs(records_by_role):
    """Direct co-occurrence from same-BPM/same-theme companion loops."""
    pairs_found = []
    roles = ["KICK", "CLAP", "HAT", "PERC"]
    indexed = {
        role: [
            (r, extract_theme(Path(r["path"]).name))
            for r in records_by_role.get(role, [])
        ]
        for role in roles
    }
    for i, role_a in enumerate(roles):
        for role_b in roles[i + 1:]:
            for rec_a, theme_a in indexed[role_a]:
                if theme_a is None:
                    continue
                for rec_b, theme_b in indexed[role_b]:
                    if theme_b is None or theme_a != theme_b:
                        continue
                    if abs(rec_a["bpm"] - rec_b["bpm"]) > 0.5:
                        continue
                    pairs_found.append((role_a, role_b, rec_a, rec_b, theme_a))
    return pairs_found


def co_occurrence_for_pairs(pairs):
    """For each role-pair with at least one matched file pair, what
    fraction of role_b's onsets land on the SAME folded 16-step position
    as a role_a onset in the paired file (both files aligned at loop
    start = bar 1 beat 1, the standard sample-pack convention)."""
    results = defaultdict(lambda: {"n_pairs": 0, "b_onsets": 0, "co_occurring": 0})
    for role_a, role_b, rec_a, rec_b, theme in pairs:
        key = f"{role_a}->{role_b}"
        a_positions = {fold_to_16(s, rec_a["total_steps"]) for s in rec_a["steps"]}
        b_positions = [fold_to_16(s, rec_b["total_steps"]) for s in rec_b["steps"]]
        results[key]["n_pairs"] += 1
        results[key]["b_onsets"] += len(b_positions)
        results[key]["co_occurring"] += sum(1 for p in b_positions if p in a_positions)
    out = {}
    for key, v in results.items():
        out[key] = {
            "matched_file_pairs": v["n_pairs"],
            "role_b_onsets_total": v["b_onsets"],
            "role_b_onsets_landing_on_role_a_step": v["co_occurring"],
            "co_occurrence_fraction": round(v["co_occurring"] / v["b_onsets"], 4) if v["b_onsets"] else None,
        }
    return out


def aggregate_vector_correlation(role_stats):
    # RIDE included (added for the layered hat-hierarchy milestone) so the
    # HatOpen role - whose position/velocity shape comes from RIDE, see
    # generate_rhythm_grammar_header.py - has real measured cross-role
    # correlation against kick/clap/hat/perc too, not just its own
    # position shape in isolation.
    roles = [r for r in ("KICK", "CLAP", "HAT", "PERC", "RIDE") if role_stats.get(r, {}).get("total_onsets")]
    out = {}
    for i, a in enumerate(roles):
        for b in roles[i + 1:]:
            va = np.array(role_stats[a]["step16_hit_probability"])
            vb = np.array(role_stats[b]["step16_hit_probability"])
            if np.std(va) == 0 or np.std(vb) == 0:
                corr = None
            else:
                corr = float(np.corrcoef(va, vb)[0, 1])
            out[f"{a}<->{b}"] = round(corr, 4) if corr is not None else None
    return out


# ---------------------------------------------------------------------------
# Phase 4 - sound fingerprint from real one-shot samples
# ---------------------------------------------------------------------------

def spectral_centroid(y, sr):
    if len(y) < 64:
        return 0.0
    mag = np.abs(np.fft.rfft(y * np.hanning(len(y))))
    freqs = np.fft.rfftfreq(len(y), d=1.0 / sr)
    total = np.sum(mag)
    return float(np.sum(mag * freqs) / total) if total > 0 else 0.0


def estimate_pitch(y, sr, max_window=0.1, min_hz=30.0, max_hz=300.0):
    window = y[: int(sr * max_window)]
    if len(window) < 64:
        return 0.0
    min_lag = max(1, int(sr / max_hz))
    max_lag = min(len(window) - 1, int(sr / min_hz))
    if max_lag <= min_lag:
        return 0.0
    energy0 = float(np.sum(window ** 2))
    if energy0 <= 1e-9:
        return 0.0
    best_lag, best_corr = -1, 0.0
    for lag in range(min_lag, max_lag + 1):
        corr = float(np.sum(window[:-lag] * window[lag:])) / energy0
        if corr > best_corr:
            best_corr, best_lag = corr, lag
    if best_lag <= 0 or best_corr < 0.35:
        return 0.0
    return sr / best_lag


def analyze_oneshot(path: Path):
    try:
        y, sr = load_mono(path)
    except Exception:
        return None
    if len(y) < sr * 0.01 or np.max(np.abs(y)) < 1e-6:
        return None

    peak = float(np.max(np.abs(y)))
    rms = float(np.sqrt(np.mean(y ** 2)))
    threshold = peak * 0.9
    attack_idx = next((i for i, v in enumerate(y) if abs(v) >= threshold), len(y))
    attack_ms = attack_idx / sr * 1000.0

    # Duration to -40dB decay from peak (a real, measurable "how long does
    # this one-shot ring out" figure, not just raw file length which often
    # includes trailing silence padding).
    peak_idx = int(np.argmax(np.abs(y)))
    decay_thresh = peak * (10 ** (-40 / 20))
    tail = np.abs(y[peak_idx:])
    below = np.where(tail < decay_thresh)[0]
    decay_samples = below[0] if len(below) else len(tail)
    decay_ms = decay_samples / sr * 1000.0

    centroid = spectral_centroid(y[: int(sr * 0.2)], sr)
    zcr = float(np.mean(np.abs(np.diff(np.sign(y))) > 0)) * sr / 2.0
    pitch = estimate_pitch(y, sr)

    sub_energy = float(np.sum(y ** 2))
    if sub_energy > 0:
        mag = np.abs(np.fft.rfft(y[: int(sr * 0.1)] * np.hanning(min(len(y), int(sr * 0.1)))))
        freqs = np.fft.rfftfreq(min(len(y), int(sr * 0.1)), d=1.0 / sr)
        sub_band = np.sum(mag[(freqs >= 20) & (freqs <= 100)])
        sub_fraction = float(sub_band / np.sum(mag)) if np.sum(mag) > 0 else 0.0
    else:
        sub_fraction = 0.0

    return {
        "duration_ms": len(y) / sr * 1000.0,
        "attack_ms": attack_ms,
        "decay_ms_to_minus40db": decay_ms,
        "peak": peak,
        "rms": rms,
        "spectral_centroid_hz": centroid,
        "zero_crossing_rate_hz": zcr,
        "estimated_pitch_hz": pitch,
        "sub_bass_fraction_20_100hz": sub_fraction,
    }


def compute_fingerprint(role, dirs):
    vals = defaultdict(list)
    n = 0
    for pack_label, d in dirs:
        if not d.is_dir():
            print(f"  [WARN] missing one-shot dir for {role}/{pack_label}: {d}", file=sys.stderr)
            continue
        for p in sorted(d.glob("*.wav")):
            rec = analyze_oneshot(p)
            if rec is None:
                continue
            n += 1
            for k, v in rec.items():
                vals[k].append(v)
    fingerprint = {"num_samples": n}
    for k, arr in vals.items():
        a = np.array(arr)
        fingerprint[k] = {"mean": round(float(np.mean(a)), 3), "stddev": round(float(np.std(a)), 3)}
    return fingerprint


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------

def ascii_pattern(rec, role_letter):
    bars = bar_pattern(rec)
    lines = []
    for bar in bars:
        row = "".join(role_letter if b else "." for b in bar)
        lines.append(row)
    return lines


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    records_by_role = {}
    role_stats = {}
    for role, dirs in LOOP_CORPUS.items():
        print(f"Analyzing {role} loop corpus...")
        recs = analyze_role_corpus(role, dirs)
        records_by_role[role] = recs
        role_stats[role] = compute_role_stats(role, recs)
        print(f"  {len(recs)} files analyzed, {role_stats[role]['total_onsets']} onsets, "
              f"{role_stats[role]['adjacent_bar_pairs_analyzed']} adjacent-bar pairs")

    pairs = cross_role_pairs(records_by_role)
    co_occurrence = co_occurrence_for_pairs(pairs)
    vector_corr = aggregate_vector_correlation(role_stats)

    print("\nAnalyzing sound fingerprints (one-shots)...")
    fingerprints = {}
    for role, dirs in ONESHOT_CORPUS.items():
        fp = compute_fingerprint(role, dirs)
        fingerprints[role] = fp
        print(f"  {role}: {fp['num_samples']} one-shots analyzed")

    grammar = {
        "corpus_manifest": {
            role: [{"pack": p, "dir": str(d)} for p, d in dirs]
            for role, dirs in LOOP_CORPUS.items()
        },
        "role_rhythm_stats": role_stats,
        "cross_role_aggregate_vector_correlation": vector_corr,
        "cross_role_matched_pair_co_occurrence": co_occurrence,
        "matched_pair_count": len(pairs),
        "sound_fingerprints": fingerprints,
    }

    (OUT_DIR / "drum_grammar.json").write_text(json.dumps(grammar, indent=2))

    # Human-readable report with real ASCII patterns from real files.
    lines = ["# Melodic Techno Drum Grammar - Analysis Report", ""]
    lines.append("## Corpus")
    for role, dirs in LOOP_CORPUS.items():
        st = role_stats[role]
        lines.append(f"- **{role}**: {st['num_files']} loop files, packs: {', '.join(st['packs']) if st['packs'] else '(none found)'}, "
                      f"{st['total_onsets']} total onsets, {st['total_bars_analyzed']} bars analyzed")
    lines.append("")

    lines.append("## Per-role 16th-step hit probability (folded to one bar, all analyzed bars combined)")
    for role in ("KICK", "CLAP", "HAT", "PERC", "TOP", "RIDE"):
        st = role_stats.get(role)
        if not st or st["total_onsets"] == 0:
            continue
        probs = st["step16_hit_probability"]
        vels = st["step16_mean_relative_velocity"]
        lines.append(f"\n### {role} (n={st['num_files']} files, {st['total_onsets']} onsets)")
        lines.append("step:  " + " ".join(f"{i:>4}" for i in range(16)))
        lines.append("prob:  " + " ".join(f"{p:.2f}" for p in probs))
        lines.append("vel:   " + " ".join(f"{v:.2f}" for v in vels))
        lines.append(f"on-beat (step%4==0): {st['on_beat_fraction']*100:.1f}%   "
                      f"8th-offbeat (step%4==2): {st['eighth_offbeat_fraction']*100:.1f}%   "
                      f"weak 16th (step%4 in 1,3): {st['weak_16th_fraction']*100:.1f}%")
        if st["adjacent_bars_identical_fraction"] is not None:
            lines.append(f"adjacent bars byte-identical: {st['adjacent_bars_identical_fraction']*100:.1f}% "
                          f"of {st['adjacent_bar_pairs_analyzed']} pairs")
        lines.append(f"mean onsets/bar: {st['mean_onsets_per_bar']:.2f}")

    lines.append("\n## Cross-role relationships")
    lines.append("\n### Aggregate step-probability-vector correlation (corpus-wide, always available)")
    for k, v in vector_corr.items():
        lines.append(f"- {k}: r = {v}" if v is not None else f"- {k}: (insufficient variance)")

    lines.append(f"\n### Direct co-occurrence on matched same-BPM/same-theme companion loops (n={len(pairs)} matched pairs total)")
    if co_occurrence:
        for k, v in co_occurrence.items():
            lines.append(f"- {k}: {v['matched_file_pairs']} pairs, "
                          f"{v['role_b_onsets_landing_on_role_a_step']}/{v['role_b_onsets_total']} of role B's onsets "
                          f"land on a role A step ({(v['co_occurrence_fraction'] or 0)*100:.1f}%)")
    else:
        lines.append("- No matched companion-loop pairs found in this corpus.")

    lines.append("\n## Sound fingerprints (one-shot samples, Phase 4)")
    for role, fp in fingerprints.items():
        lines.append(f"\n### {role} (n={fp['num_samples']})")
        for k, v in fp.items():
            if k == "num_samples":
                continue
            lines.append(f"- {k}: mean={v['mean']}, stddev={v['stddev']}")

    lines.append("\n## Representative real patterns (ASCII, from actual analyzed files)")
    letter = {"KICK": "K", "CLAP": "C", "HAT": "H", "PERC": "P", "TOP": "T", "RIDE": "R"}
    for role in ("KICK", "CLAP", "HAT", "PERC"):
        recs = [r for r in records_by_role.get(role, []) if r["num_bars"] <= 4]
        recs.sort(key=lambda r: Path(r["path"]).name)
        for rec in recs[:2]:
            lines.append(f"\n{role} - {Path(rec['path']).name} ({rec['bpm']} BPM, {rec['num_bars']} bar(s)):")
            for row in ascii_pattern(rec, letter[role]):
                lines.append("  " + row)

    (OUT_DIR / "report.md").write_text("\n".join(lines))
    print(f"\nWrote {OUT_DIR / 'drum_grammar.json'}")
    print(f"Wrote {OUT_DIR / 'report.md'}")


if __name__ == "__main__":
    main()
