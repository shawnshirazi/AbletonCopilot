#!/usr/bin/env python3
"""
Tier 2: extract real transcribed bass fragments from a single user-supplied
reference track (a normal mixed file, not a pre-isolated stem).

Unlike extract_fragments.py (which walks a whole pre-separated stem library
and expects bass.wav to already exist per segment), this script takes one
mixed audio file, runs Demucs locally to isolate the bass stem itself, then
reuses the exact same transcription/windowing logic from fragment_lib.py so
the output JSON is in the same shape the C++ side already parses
(MelodyGridComponent::loadReferenceFragments).

Usage:
    python extract_reference_fragments.py --input <reference.wav> \\
        --bpm <float> --key-root <int 0-11> --minor <true|false> \\
        --output <fragments.json>
"""

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from fragment_lib import transcribe, notes_to_fragments, extract_drum_patterns


def separate_stems(input_file: Path, work_dir: Path) -> dict[str, Path] | None:
    """Run Demucs (full 4-stem htdemucs separation) on input_file, return a
    dict of {stem_name: path} for whichever of bass/drums/other/vocals were
    produced inside work_dir, or None if separation failed outright.

    Full separation (no --two-stems) so one pass produces both bass.wav
    (for melody transcription) and drums.wav (for pattern/sample
    extraction) - --two-stems still runs the full separation internally
    and just discards the other stems, so doing it once here is strictly
    cheaper than the two separate --two-stems calls this used to need."""
    cmd = [
        sys.executable, "-m", "demucs.separate",
        "-n", "htdemucs",
        "-o", str(work_dir),
        str(input_file),
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Demucs separation failed:\n{result.stderr}", file=sys.stderr)
        return None

    stem_dir = work_dir / "htdemucs" / input_file.stem
    stems = {}
    for name in ("bass", "drums", "other", "vocals"):
        p = stem_dir / f"{name}.wav"
        if p.is_file():
            stems[name] = p
    return stems if stems else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="Reference audio file (mixed, any format Demucs/ffmpeg reads)")
    parser.add_argument("--bpm", required=True, type=float)
    parser.add_argument("--key-root", required=True, type=int, help="0=C .. 11=B")
    parser.add_argument("--minor", required=True, type=str, help="true/false")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--drums-output", required=False, type=Path,
                         help="Optional: also extract approximate drum patterns/one-shots (see fragment_lib.extract_drum_patterns)")
    args = parser.parse_args()

    is_minor = args.minor.strip().lower() in ("true", "1", "yes")

    work_dir = Path(tempfile.mkdtemp(prefix="abletoncopilot_reference_"))
    try:
        print(f"Separating stems from {args.input.name} (this can take a minute or two)...")
        stems = separate_stems(args.input, work_dir)
        if stems is None or "bass" not in stems:
            print("Could not isolate a bass stem - no fragments extracted.", file=sys.stderr)
            args.output.write_text(json.dumps([]))
            if args.drums_output is not None:
                args.drums_output.write_text(json.dumps({}))
            sys.exit(1)

        print("Transcribing bass stem...")
        midi_data = transcribe(stems["bass"])
        frags = notes_to_fragments(midi_data, args.bpm, args.key_root) if midi_data is not None else []

        all_fragments = [
            {
                "source": args.input.stem,
                "segment": f"seg_{i:03d}",
                "bpm": args.bpm,
                "isMinor": is_minor,
                "steps": f,
            }
            for i, f in enumerate(frags)
        ]

        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(all_fragments, indent=1, default=int))
        print(f"Wrote {len(all_fragments)} bass fragment(s) to {args.output}")

        if args.drums_output is not None:
            if "drums" in stems:
                print("Analyzing drum stem (frequency-band onset detection)...")
                drum_patterns = extract_drum_patterns(stems["drums"], args.bpm, args.drums_output.parent)
                args.drums_output.parent.mkdir(parents=True, exist_ok=True)
                args.drums_output.write_text(json.dumps(drum_patterns, indent=1, default=int))
                print(f"Wrote drum patterns for {list(drum_patterns.keys())} to {args.drums_output}")
            else:
                args.drums_output.write_text(json.dumps({}))
                print("No drum stem produced - skipping drum pattern extraction.", file=sys.stderr)
    finally:
        shutil.rmtree(work_dir, ignore_errors=True)


if __name__ == "__main__":
    main()
