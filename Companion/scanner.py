"""
Sample library scanner.

Walks a root directory, indexes audio files, and matches them to
a given BPM / key / genre by parsing filenames for embedded metadata.
"""

import os
import re
from dataclasses import dataclass, field
from typing import Optional

AUDIO_EXTS = {'.wav', '.aif', '.aiff', '.flac', '.mp3', '.ogg'}

# Keywords that map filename tokens to instrument type
TYPE_KEYWORDS = {
    'kick': 'KICK',   'kck': 'KICK',
    'snare': 'SNARE', 'snr': 'SNARE',
    'clap': 'CLAP',   'clp': 'CLAP',
    'hihat': 'HIHAT', 'hh': 'HIHAT',  'hat': 'HIHAT',
    'cymbal': 'CYMBAL',
    'percussion': 'PERC', 'perc': 'PERC', 'shaker': 'PERC',
    'bass': 'BASS',   'sub': 'BASS',    '808': 'BASS',
    'chord': 'CHORD', 'chords': 'CHORD',
    'pad': 'PAD',
    'lead': 'LEAD',   'synth': 'LEAD',  'pluck': 'LEAD',
    'arp': 'ARP',     'arpeggio': 'ARP',
    'melody': 'MELODY', 'mel': 'MELODY',
    'vocal': 'VOCAL',  'vox': 'VOCAL',  'vocals': 'VOCAL',
    'fx': 'FX',        'riser': 'FX',   'impact': 'FX',   'sweep': 'FX',
    'loop': 'LOOP',
    'drum': 'DRUM',   'drums': 'DRUM',  'break': 'DRUM',
}

NOTE_MAP = {
    'C': 0, 'C#': 1, 'Db': 1, 'D': 2, 'D#': 3, 'Eb': 3,
    'E': 4, 'F': 5, 'F#': 6, 'Gb': 6, 'G': 7, 'G#': 8,
    'Ab': 8, 'A': 9, 'A#': 10, 'Bb': 10, 'B': 11,
}

_DRUM_TYPES = {'KICK', 'SNARE', 'HIHAT', 'CLAP', 'PERC', 'DRUM', 'CYMBAL'}


@dataclass
class SampleEntry:
    path: str
    name: str
    sample_type: str = 'UNKNOWN'
    bpm: Optional[float] = None
    key_root: Optional[int] = None   # semitone 0-11, None = unknown
    is_minor: Optional[bool] = None


def _parse_filename(stem: str) -> tuple:
    """Return (sample_type, bpm, key_root, is_minor)."""
    lower = stem.lower().replace('_', ' ').replace('-', ' ')
    tokens = lower.split()

    # BPM: find a number adjacent to 'bpm' or a standalone 2-3 digit number
    bpm = None
    bpm_m = re.search(r'(\d{2,3})\s*bpm', lower)
    if bpm_m:
        bpm = float(bpm_m.group(1))
    else:
        # fallback: standalone 2-3 digit number that looks like a BPM
        nums = re.findall(r'\b(\d{2,3})\b', lower)
        candidates = [float(n) for n in nums if 60 <= float(n) <= 220]
        bpm = candidates[0] if candidates else None

    # Key: look for note+mode patterns like "Am", "F#m", "Cmaj", "Bb"
    key_root = None
    is_minor = None
    key_m = re.search(
        r'\b([A-Ga-g][#b]?)\s*(min(?:or)?|maj(?:or)?|m(?!aj))?\b', stem
    )
    if key_m:
        note_str = key_m.group(1)
        # Capitalize correctly (e.g. "f#" -> "F#")
        note_str = note_str[0].upper() + note_str[1:]
        if note_str in NOTE_MAP:
            key_root = NOTE_MAP[note_str]
            mode_str = (key_m.group(2) or '').lower()
            if mode_str and 'maj' not in mode_str:
                is_minor = True   # 'm' or 'min' or 'minor'
            elif 'maj' in mode_str:
                is_minor = False
            # else unknown; leave None

    # Type: first matching keyword wins
    sample_type = 'UNKNOWN'
    for token in tokens:
        if token in TYPE_KEYWORDS:
            sample_type = TYPE_KEYWORDS[token]
            break
    if sample_type == 'UNKNOWN':
        # Try substring match in full stem
        for kw, typ in TYPE_KEYWORDS.items():
            if kw in lower:
                sample_type = typ
                break

    return sample_type, bpm, key_root, is_minor


def _parse_track_key(key_str: str) -> tuple[int | None, bool | None]:
    """Parse VST key string like 'C# major' into (key_root, is_minor)."""
    if not key_str:
        return None, None
    m = re.match(r'([A-G][#b]?)\s*(major|minor)', key_str.strip(), re.IGNORECASE)
    if not m:
        return None, None
    root = NOTE_MAP.get(m.group(1))
    is_minor = m.group(2).lower() == 'minor'
    return root, is_minor


def _filename_score(e: SampleEntry, bpm: float, key_root: int, is_minor: bool) -> int:
    """Replicate the original scoring logic for filename metadata."""
    BPM_TOL = 0.05
    s = 0
    if e.bpm is not None:
        if abs(e.bpm - bpm) / max(bpm, 1) <= BPM_TOL:
            s += 10
        elif abs(e.bpm - bpm * 0.5) / max(bpm, 1) <= BPM_TOL:
            s += 7
        elif abs(e.bpm - bpm * 2) / max(bpm, 1) <= BPM_TOL:
            s += 7
    else:
        s += 3

    if e.key_root is not None:
        if e.key_root == key_root:
            s += (5 if e.is_minor == is_minor else 3)
        elif e.key_root == (key_root + 7) % 12:
            s += 2
    else:
        s += 2

    return s


def _audio_score(
    feat: dict,
    sample_type: str,
    track_bpm: float,
    track_key_root: int | None,
    track_is_minor: bool | None,
    track_data: dict,
) -> int:
    s = 0

    # BPM match
    audio_bpm = feat.get("bpm")
    if audio_bpm and track_bpm:
        ratio = abs(audio_bpm - track_bpm) / max(track_bpm, 1)
        half_ratio = abs(audio_bpm - track_bpm * 0.5) / max(track_bpm, 1)
        double_ratio = abs(audio_bpm - track_bpm * 2) / max(track_bpm, 1)
        if ratio <= 0.02:
            s += 15
        elif ratio <= 0.05:
            s += 8
        elif half_ratio <= 0.02 or double_ratio <= 0.02:
            s += 10

    # Key match
    audio_root = feat.get("key_root")
    audio_minor = feat.get("is_minor")
    if audio_root is not None and track_key_root is not None:
        if audio_root == track_key_root:
            if audio_minor == track_is_minor:
                s += 12
            else:
                s += 6   # same root, different mode
        elif audio_root == (track_key_root + 7) % 12:
            s += 4       # perfect fifth

    # Spectral scores for drum types
    if sample_type in _DRUM_TYPES and track_data:
        sub_e = feat.get("sub_energy", 0.0)
        transient = feat.get("transient_density", 0.0)
        centroid = feat.get("spectral_centroid", 0.0)
        mfcc = feat.get("mfcc")

        if sample_type == 'KICK':
            track_sub = track_data.get("sub_pct", 0.25)
            if track_sub < 0.20:
                # Track is light on sub — reward a sub-heavy kick
                s += int(10 * sub_e / max(sub_e, 0.01))
            elif track_sub > 0.30:
                # Track already saturated in sub — prefer mid-punch kick
                s += 5 if sub_e < 0.3 else 0

        elif sample_type in ('SNARE', 'CLAP'):
            # Transient density is normalized per-file so we reward high values
            # relative to a heuristic threshold derived from typical onset strengths
            if transient > 2.0:
                s += 8

        elif sample_type in ('HIHAT', 'CYMBAL'):
            # Centroid > 8 kHz is bright / airy — ideal for hi-hats
            if centroid > 8000:
                s += 8

        # MFCC norm tiebreaker: percussive samples tend to have high-energy MFCCs
        if mfcc:
            mfcc_norm = sum(v * v for v in mfcc) ** 0.5
            if mfcc_norm > 50:
                s += 3

    return s


class SampleLibrary:
    def __init__(self, root: str):
        self.root = root
        self.entries: list[SampleEntry] = []

    def scan(self) -> int:
        self.entries.clear()
        for dirpath, _, filenames in os.walk(self.root):
            for fn in filenames:
                ext = os.path.splitext(fn)[1].lower()
                if ext not in AUDIO_EXTS:
                    continue
                stem = os.path.splitext(fn)[0]
                sample_type, bpm, key_root, is_minor = _parse_filename(stem)
                self.entries.append(SampleEntry(
                    path=os.path.join(dirpath, fn),
                    name=fn,
                    sample_type=sample_type,
                    bpm=bpm,
                    key_root=key_root,
                    is_minor=is_minor,
                ))
        return len(self.entries)

    def find_matches(
        self,
        bpm: float,
        key_root: int,
        is_minor: bool,
        genre: str,
        max_per_type: int = 5,
        track_data: dict = None,
        audio_cache=None,          # SampleCache | None
    ) -> dict:
        """
        Return dict mapping sample_type -> list[SampleEntry], ranked by score.

        When track_data and audio_cache are provided, each type's top-50
        filename-scored candidates are re-ranked with audio feature scores.
        """
        track_key_root, track_is_minor = (None, None)
        if track_data:
            track_key_root, track_is_minor = _parse_track_key(track_data.get("key", ""))

        by_type: dict[str, list[SampleEntry]] = {}
        for e in self.entries:
            by_type.setdefault(e.sample_type, []).append(e)

        result = {}
        for typ, entries in by_type.items():
            # First pass: filename score
            scored = sorted(
                entries,
                key=lambda e: _filename_score(e, bpm, key_root, is_minor),
                reverse=True,
            )
            candidates = scored[:50]

            if audio_cache is not None and track_data is not None:
                def total_score(e: SampleEntry) -> int:
                    fn_s = _filename_score(e, bpm, key_root, is_minor)
                    feat = audio_cache.get(e.path)   # cache-only; no on-the-fly analysis
                    if feat is None:
                        return fn_s
                    return fn_s + _audio_score(
                        feat, typ,
                        bpm, track_key_root, track_is_minor,
                        track_data,
                    )

                candidates = sorted(candidates, key=total_score, reverse=True)

            result[typ] = candidates[:max_per_type]

        return result

    def scan_and_analyze(self, cache, max_workers: int = 4) -> int:
        """Scan library then warm the audio cache for all discovered files."""
        from audio_analyzer import analyze_batch

        count = self.scan()
        if count == 0:
            print(f'  ERROR: no audio files found in {self.root!r}')
            print('  Check the --library path is correct.')
            return 0
        print(f'  Found {count} audio files')
        paths = [e.path for e in self.entries]

        # Check which paths are already cached before we run analysis.
        pre_cached = {p for p in paths if cache.get(p) is not None}
        already_cached = len(pre_cached)

        analyze_batch(paths, cache, max_workers=max_workers)

        # Newly analyzed = paths that now have a cache entry but didn't before.
        newly_analyzed = sum(
            1 for p in paths
            if p not in pre_cached and cache.get(p) is not None
        )

        print(f"  Background audio analysis: {newly_analyzed} new files analyzed, {already_cached} already cached")
        return newly_analyzed
