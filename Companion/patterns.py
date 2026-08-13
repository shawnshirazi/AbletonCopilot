"""
MIDI pattern generation: drums, bass, chords, lead.

All times are in beats (1.0 = 1 quarter note).
Each note is a tuple: (pitch, time, duration, velocity, mute).
MIDI pitches use General MIDI / Ableton Drum Rack defaults:
  36=Kick  38=Snare  39=Clap  42=ClosedHH  46=OpenHH
Bass is placed at octave 2 (~MIDI 36-47).
Chords at octave 3 (~MIDI 48-59).
Lead at octave 4-5 (~MIDI 60-83).
"""

NOTE_MAP = {
    'C': 0, 'C#': 1, 'Db': 1, 'D': 2, 'D#': 3, 'Eb': 3,
    'E': 4, 'F': 5, 'F#': 6, 'Gb': 6, 'G': 7, 'G#': 8,
    'Ab': 8, 'A': 9, 'A#': 10, 'Bb': 10, 'B': 11,
}

MINOR_SCALE = [0, 2, 3, 5, 7, 8, 10]
MAJOR_SCALE = [0, 2, 4, 5, 7, 9, 11]

# (semitone offset from root, 'M'=major triad / 'm'=minor triad)
MINOR_PROG = [(0, 'm'), (8, 'M'), (3, 'M'), (10, 'M')]   # i - VI - III - VII
MAJOR_PROG = [(0, 'M'), (7, 'M'), (9, 'm'),  (5, 'M')]   # I  - V  - vi  - IV

BEATS = 4  # beats per bar


def parse_key(key_str: str) -> tuple:
    """Return (root_semitone 0-11, is_minor bool). Defaults to C major."""
    s = key_str.strip()
    if not s or s in ('--', 'n/a', ''):
        return 0, False
    for length in (2, 1):
        root = s[:length]
        if root in NOTE_MAP:
            suffix = s[length:].strip().lower()
            is_minor = suffix.startswith('m') or 'minor' in suffix
            return NOTE_MAP[root], is_minor
    return 0, False


def _chord_notes(root_semi: int, mode: str, octave: int = 3) -> list:
    base = 12 * (octave + 1) + root_semi
    intervals = [0, 3, 7] if mode == 'm' else [0, 4, 7]
    return [base + i for i in intervals] + [base + 12]  # root + triad + octave


def _scale(root_semi: int, is_minor: bool, octave: int = 4) -> list:
    base = 12 * (octave + 1) + root_semi
    intervals = MINOR_SCALE if is_minor else MAJOR_SCALE
    return [base + i for i in intervals]


# ─────────────────────── DRUM PATTERNS (1-bar templates) ──────────────────────

def _repeat(pattern_1bar: list, bars: int) -> list:
    out = []
    for bar in range(bars):
        for (p, t, d, v, m) in pattern_1bar:
            out.append((p, t + bar * BEATS, d, v, m))
    return out


def _drum_pattern_1bar(genre: str, variant: str) -> list:
    K, S, CL, CHH, OHH = 36, 38, 39, 42, 46

    if genre in ('house', 'tech_house', 'melodic_techno', 'techno'):
        if variant == 'sparse':
            return [
                (K,  0.0, 0.25, 100, 0),
                (K,  2.0, 0.25, 100, 0),
                (CHH,0.0, 0.25,  55, 0),
                (CHH,1.0, 0.25,  55, 0),
                (CHH,2.0, 0.25,  55, 0),
                (CHH,3.0, 0.25,  55, 0),
            ]
        elif variant == 'build':
            return [
                (K,  0.0, 0.25, 100, 0),
                (K,  1.0, 0.25, 100, 0),
                (K,  1.5, 0.25,  85, 0),
                (K,  2.0, 0.25, 100, 0),
                (K,  3.0, 0.25, 100, 0),
                (K,  3.5, 0.25,  85, 0),
                (CL, 1.0, 0.25,  90, 0),
                (CL, 3.0, 0.25,  90, 0),
                (CHH,0.0, 0.25,  65, 0),
                (CHH,0.5, 0.25,  55, 0),
                (CHH,1.0, 0.25,  65, 0),
                (CHH,1.5, 0.25,  55, 0),
                (CHH,2.0, 0.25,  65, 0),
                (CHH,2.5, 0.25,  55, 0),
                (CHH,3.0, 0.25,  65, 0),
                (CHH,3.5, 0.25,  55, 0),
                (OHH,2.5, 0.25,  70, 0),
            ]
        else:  # full
            return [
                (K,  0.0, 0.25, 100, 0),
                (K,  1.0, 0.25, 100, 0),
                (K,  2.0, 0.25, 100, 0),
                (K,  3.0, 0.25, 100, 0),
                (CL, 1.0, 0.25,  85, 0),
                (CL, 3.0, 0.25,  85, 0),
                (CHH,0.0, 0.25,  65, 0),
                (CHH,0.5, 0.25,  50, 0),
                (CHH,1.0, 0.25,  65, 0),
                (CHH,1.5, 0.25,  50, 0),
                (CHH,2.0, 0.25,  65, 0),
                (CHH,2.5, 0.25,  50, 0),
                (CHH,3.0, 0.25,  65, 0),
                (CHH,3.5, 0.25,  50, 0),
                (OHH,2.5, 0.25,  60, 0),
            ]

    elif genre in ('trap', 'future_bass'):
        if variant == 'sparse':
            return [
                (K,  0.0, 0.25, 100, 0),
                (S,  2.0, 0.25,  90, 0),
                (CHH,0.0, 0.25,  50, 0),
                (CHH,1.0, 0.25,  50, 0),
                (CHH,2.0, 0.25,  50, 0),
                (CHH,3.0, 0.25,  50, 0),
            ]
        elif variant == 'build':
            return [
                (K,  0.0,  0.25, 100, 0),
                (K,  0.75, 0.25,  85, 0),
                (K,  2.0,  0.25, 100, 0),
                (S,  2.0,  0.25,  90, 0),
                (CL, 3.0,  0.25,  80, 0),
                *[(CHH, i * 0.25, 0.20, 45 + (i % 2) * 10, 0) for i in range(16)],
                (OHH,1.5, 0.25,  65, 0),
                (OHH,3.5, 0.25,  65, 0),
            ]
        else:  # full
            return [
                (K,  0.0,  0.25, 100, 0),
                (K,  0.75, 0.25,  85, 0),
                (K,  2.0,  0.25, 100, 0),
                (K,  2.75, 0.25,  80, 0),
                (S,  2.0,  0.25,  90, 0),
                (CL, 1.0,  0.25,  75, 0),
                (CL, 3.0,  0.25,  75, 0),
                *[(CHH, i * 0.25, 0.20, 40 + (i % 4) * 8, 0) for i in range(16)],
                (OHH,0.5, 0.25,  60, 0),
                (OHH,2.5, 0.25,  60, 0),
            ]

    elif genre == 'dnb':
        if variant == 'sparse':
            return [
                (K,  0.0,  0.25, 100, 0),
                (S,  1.0,  0.25,  90, 0),
                (S,  2.75, 0.25,  85, 0),
                (CHH,0.0, 0.25,   60, 0),
                (CHH,0.5, 0.25,   50, 0),
            ]
        else:  # full / build
            return [
                (K,  0.0,  0.25, 100, 0),
                (K,  1.5,  0.25,  80, 0),
                (S,  1.0,  0.25,  90, 0),
                (S,  2.75, 0.25,  85, 0),
                (CL, 3.0,  0.25,  80, 0),
                (CHH,0.0, 0.25,   65, 0),
                (CHH,0.25,0.25,   50, 0),
                (CHH,0.5, 0.25,   60, 0),
                (CHH,0.75,0.25,   45, 0),
                (CHH,1.0, 0.25,   65, 0),
                (CHH,1.25,0.25,   50, 0),
                (CHH,1.5, 0.25,   60, 0),
                (CHH,1.75,0.25,   45, 0),
                (CHH,2.0, 0.25,   65, 0),
                (CHH,2.25,0.25,   50, 0),
                (CHH,2.5, 0.25,   60, 0),
                (CHH,2.75,0.25,   45, 0),
                (CHH,3.0, 0.25,   65, 0),
                (CHH,3.25,0.25,   50, 0),
                (CHH,3.5, 0.25,   60, 0),
                (CHH,3.75,0.25,   45, 0),
                (OHH,0.5, 0.25,   55, 0),
                (OHH,2.5, 0.25,   55, 0),
            ]

    # Fallback: minimal 4/4
    return [
        (K, 0.0, 0.25, 100, 0), (K, 2.0, 0.25, 100, 0),
        (S, 1.0, 0.25,  85, 0), (S, 3.0, 0.25,  85, 0),
    ]


def make_drums(genre: str, variant: str, bars: int) -> list:
    template = _drum_pattern_1bar(genre, variant)
    return _repeat(template, bars)


# ─────────────────────── BASS PATTERNS ──────────────────────────────────────

def make_bass(root_semi: int, is_minor: bool, bars: int, genre: str, variant: str) -> list:
    prog = MINOR_PROG if is_minor else MAJOR_PROG
    notes = []

    for bar in range(bars):
        prog_idx = bar % len(prog)
        chord_offset, chord_mode = prog[prog_idx]
        chord_root = (root_semi + chord_offset) % 12
        bass_note = 36 + chord_root  # octave 2
        fifth     = bass_note + 7
        offset    = bar * BEATS

        if variant == 'sparse':
            notes += [
                (bass_note, offset + 0.0, 2.0, 90, 0),
                (bass_note, offset + 2.0, 1.8, 80, 0),
            ]
        elif genre in ('trap', 'future_bass'):
            # 808-style long sub hits
            notes += [
                (bass_note, offset + 0.0,  1.5, 95, 0),
                (bass_note, offset + 2.0,  0.5, 85, 0),
                (bass_note, offset + 2.75, 1.0, 80, 0),
            ]
        else:
            # Driving house/techno bass
            notes += [
                (bass_note, offset + 0.0, 0.5, 95, 0),
                (bass_note, offset + 0.5, 0.5, 80, 0),
                (bass_note, offset + 1.0, 0.5, 90, 0),
                (fifth,     offset + 2.0, 0.5, 85, 0),
                (bass_note, offset + 2.5, 0.5, 80, 0),
                (bass_note, offset + 3.0, 0.5, 85, 0),
                (bass_note, offset + 3.5, 0.5, 75, 0),
            ]
    return notes


# ─────────────────────── CHORD PATTERNS ─────────────────────────────────────

def make_chords(root_semi: int, is_minor: bool, bars: int, genre: str, variant: str) -> list:
    prog = MINOR_PROG if is_minor else MAJOR_PROG
    notes = []

    for bar in range(bars):
        prog_idx = bar % len(prog)
        chord_offset, chord_mode = prog[prog_idx]
        chord_root = (root_semi + chord_offset) % 12
        chord = _chord_notes(chord_root, chord_mode, octave=3)
        offset = bar * BEATS

        if variant == 'slow':
            # Pad: whole bar
            for p in chord:
                notes.append((p, offset, BEATS - 0.1, 65, 0))

        elif variant == 'stab':
            # Rhythmic stabs on beats 2 and 4
            for beat in (1.0, 3.0):
                for p in chord:
                    notes.append((p, offset + beat, 0.3, 75, 0))

        elif variant == 'high':
            # Higher register arp feel – chord held with slight velocity variation
            for i, p in enumerate(chord):
                notes.append((p + 12, offset + i * 0.1, BEATS - 0.15, 60 + i * 5, 0))

        else:  # default slow pad
            for p in chord:
                notes.append((p, offset, BEATS - 0.1, 65, 0))

    return notes


# ─────────────────────── LEAD / ARP PATTERNS ────────────────────────────────

def make_lead(root_semi: int, is_minor: bool, bars: int, genre: str, variant: str) -> list:
    prog   = MINOR_PROG if is_minor else MAJOR_PROG
    notes  = []
    step   = 0.25  # 16th notes for house/techno arp
    vel_hi, vel_lo = 85, 65

    for bar in range(bars):
        prog_idx = bar % len(prog)
        chord_offset, chord_mode = prog[prog_idx]
        chord_root = (root_semi + chord_offset) % 12
        arp = _chord_notes(chord_root, chord_mode, octave=4)  # higher register
        offset = bar * BEATS

        if genre in ('trap', 'future_bass'):
            # Slower, emotional arp in 8th notes
            step = 0.5
            for i, beat in enumerate([b * step for b in range(int(BEATS / step))]):
                p = arp[i % len(arp)]
                notes.append((p, offset + beat, step - 0.05,
                               vel_hi if i % 2 == 0 else vel_lo, 0))
        elif variant == 'high':
            # 16th-note arp
            for i, beat in enumerate([b * step for b in range(int(BEATS / step))]):
                p = arp[i % len(arp)]
                notes.append((p, offset + beat, step - 0.05,
                               vel_hi if i % 4 == 0 else vel_lo, 0))
        else:
            # 8th-note arp
            step = 0.5
            for i, beat in enumerate([b * step for b in range(int(BEATS / step))]):
                p = arp[i % len(arp)]
                notes.append((p, offset + beat, step - 0.05,
                               vel_hi if i % 2 == 0 else vel_lo, 0))

    return notes
