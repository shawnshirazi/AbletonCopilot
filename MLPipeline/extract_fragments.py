#!/usr/bin/env python3
"""
Phase 1 + 2: transcribe bass stems from the "melodic" family of tracks
(Melodic Techno / Melodic Tech House / Melodic House) into real 2-bar
fragments the AbletonCopilot Drum Machine can pick from and transpose,
instead of the hand-invented motifs it uses today.

For every matching track's segments, runs basic-pitch on bass.wav, quantizes
the resulting notes onto the 16-steps-per-bar grid the plugin already uses,
slices into 2-bar (32-step) windows, and stores each window as a fragment:
step -> semitone offset from the track's own key root (or null = no note) —
the exact same representation DrumMachineComponent::melodyOffsets uses, so
this can be dropped straight into the plugin later with no format work.
"""

import json
import os
import sys
from pathlib import Path

from fragment_lib import transcribe, notes_to_fragments

STEMS_ROOT = Path("/Volumes/Seagate Desktop Drive/shawn-stuff/STEP 2 Split Tracks - COMPLETE")
OUT_DIR = Path(__file__).parent / "fragments"
OUT_FILE = OUT_DIR / "melodic_family_bass_fragments.json"

MELODIC_SUBGENRES = {
    "melodic techno", "melodic tech house", "melodic house", "meldodic tech house",
}

NOTE_MAP = {
    "C": 0, "C#": 1, "DB": 1, "D": 2, "D#": 3, "EB": 3, "E": 4, "F": 5, "F#": 6,
    "GB": 6, "G": 7, "G#": 8, "AB": 8, "A": 9, "A#": 10, "BB": 10, "B": 11,
}


def parse_key(key_str: str):
    """'Bb Minor' -> (10, True)"""
    parts = key_str.strip().split()
    if not parts:
        return 0, False
    root = parts[0].upper()
    is_minor = len(parts) > 1 and parts[1].lower().startswith("min")
    return NOTE_MAP.get(root, 0), is_minor


def find_melodic_segments():
    """Yield (track_title, seg_id, bass_wav_path, bpm, key_root, is_minor)."""
    for folder in sorted(os.listdir(STEMS_ROOT)):
        track_dir = STEMS_ROOT / folder
        meta_path = track_dir / "metadata.json.txt"
        if not meta_path.is_file():
            continue
        try:
            meta = json.loads(meta_path.read_text())
        except Exception:
            continue

        subgenre = (meta.get("subgenre") or "").strip().lower()
        if subgenre not in MELODIC_SUBGENRES:
            continue

        segments_dir = track_dir / "segments"
        if not segments_dir.is_dir():
            continue

        for seg_folder in sorted(os.listdir(segments_dir)):
            seg_dir = segments_dir / seg_folder
            bass_wav = seg_dir / "bass.wav"
            seg_meta_path = seg_dir / "meta.json"
            if not bass_wav.is_file() or not seg_meta_path.is_file():
                continue
            try:
                seg_meta = json.loads(seg_meta_path.read_text())
            except Exception:
                continue

            bpm = seg_meta.get("bpm") or meta.get("bpm")
            key_str = seg_meta.get("key") or meta.get("key") or "C Major"
            if not bpm:
                continue
            root, is_minor = parse_key(key_str)
            yield meta.get("title", folder), seg_folder, bass_wav, float(bpm), root, is_minor


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    segments = list(find_melodic_segments())
    print(f"Found {len(segments)} segments across the melodic family.")

    all_fragments = []
    for i, (title, seg_id, bass_wav, bpm, key_root, is_minor) in enumerate(segments, 1):
        print(f"[{i}/{len(segments)}] {title} / {seg_id} (bpm={bpm:.0f})")
        midi_data = transcribe(bass_wav)
        if midi_data is None:
            continue
        frags = notes_to_fragments(midi_data, bpm, key_root)
        for f in frags:
            all_fragments.append({
                "source": title,
                "segment": seg_id,
                "bpm": bpm,
                "isMinor": is_minor,
                "steps": f,
            })
        print(f"    -> {len(frags)} usable 2-bar fragment(s)")

        # Checkpoint every 25 segments — a late failure (or a Ctrl-C) shouldn't
        # throw away 10+ minutes of already-completed transcription.
        if i % 25 == 0:
            OUT_FILE.write_text(json.dumps(all_fragments, indent=1, default=int))

    OUT_FILE.write_text(json.dumps(all_fragments, indent=1, default=int))
    print(f"\nWrote {len(all_fragments)} total fragments to {OUT_FILE}")


if __name__ == "__main__":
    main()
