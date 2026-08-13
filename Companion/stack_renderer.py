"""
Renders each arrangement section's drum pattern as a synced stack of stems.

For every section (Intro, Drop, Break, ...) and every drum element that
hits in it (Kick, Snare, Clap, Hihat), writes one full-length WAV with the
matched sample placed at each beat position and silence elsewhere. Every
stem in a section shares the same length and beat grid, so dragging them
onto separate Ableton tracks keeps them in sync automatically — no manual
nudging needed (same idea as a Splice Stack).
"""

import shutil
from pathlib import Path

import numpy as np
import soundfile as sf

import patterns as P
from arrangement import get_arrangement

SAMPLE_RATE = 44100

# MIDI pitch (see patterns.py's drum templates) -> sample library type
_PITCH_TO_TYPE = {
    36: 'KICK',
    38: 'SNARE',
    39: 'CLAP',
    42: 'HIHAT',
    46: 'HIHAT',
}

# If no sample was matched for a type, borrow the nearest substitute
_FALLBACK_TYPE = {'CLAP': 'SNARE'}


def _load_stereo(path: str, sr: int = SAMPLE_RATE) -> "np.ndarray":
    import librosa
    y, _ = librosa.load(path, sr=sr, mono=False)
    if y.ndim == 1:
        y = np.stack([y, y])
    return y


def _best_sample_path(matches: dict, sample_type: str) -> "str | None":
    entries = matches.get(sample_type) or []
    if not entries:
        fallback = _FALLBACK_TYPE.get(sample_type)
        if fallback:
            entries = matches.get(fallback) or []
    return entries[0].path if entries else None


def _render_stem(sample_path: str, hit_beats: list, bars: int, bpm: float,
                  sr: int = SAMPLE_RATE) -> "np.ndarray":
    total_seconds = bars * P.BEATS * 60.0 / bpm
    total_samples = int(total_seconds * sr)
    buf = np.zeros((2, total_samples), dtype=np.float32)

    y = _load_stereo(sample_path, sr)
    for beat in hit_beats:
        start = int(beat * 60.0 / bpm * sr)
        if start >= total_samples:
            continue
        seg_len = min(y.shape[1], total_samples - start)
        buf[:, start:start + seg_len] += y[:, :seg_len]

    peak = float(np.abs(buf).max())
    if peak > 0.98:
        buf *= 0.98 / peak
    return buf


def build(matches: dict, genre_id: str, bpm: float, out_dir: Path, *,
          verbose: bool = True) -> Path:
    """
    Render a synced drum stack (one WAV per element, per section) for the
    genre's whole arrangement. Returns the output directory.
    """
    stack_dir = Path(out_dir) / 'DrumStack'
    if stack_dir.exists():
        shutil.rmtree(stack_dir)
    stack_dir.mkdir(parents=True)

    arrangement = get_arrangement(genre_id)
    written = 0

    for idx, (section, bars, d_var, *_rest) in enumerate(arrangement):
        if d_var is None:
            continue

        notes = P.make_drums(genre_id, d_var, bars)
        hits_by_type: dict = {}
        for (pitch, t, _dur, _vel, _mute) in notes:
            typ = _PITCH_TO_TYPE.get(pitch)
            if typ:
                hits_by_type.setdefault(typ, []).append(t)
        if not hits_by_type:
            continue

        section_dir = stack_dir / f'{idx + 1:02d}_{section.replace(" ", "_")}'

        for typ, beats in hits_by_type.items():
            sample_path = _best_sample_path(matches, typ)
            if not sample_path:
                if verbose:
                    print(f'    (no {typ} sample matched — skipping {section}/{typ})')
                continue
            section_dir.mkdir(parents=True, exist_ok=True)
            buf = _render_stem(sample_path, sorted(beats), bars, bpm)
            out_path = section_dir / f'{typ.title()}.wav'
            sf.write(str(out_path), buf.T, SAMPLE_RATE, subtype='PCM_24')
            written += 1
            if verbose:
                print(f'    [{section}] {typ.title()}.wav  ({len(beats)} hits, {bars} bars)')

    if verbose:
        print(f'  Drum stack: {written} stems written to {stack_dir}')
    return stack_dir
