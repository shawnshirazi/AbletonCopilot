"""
Builds a full arrangement in Ableton's Session View via AbletonOSC.

Creates 4 MIDI tracks (Drums / Bass / Chords / Lead) and fills clip slots
with genre-appropriate patterns for each section of the song.  Scene names
label each section so the producer can launch or record-to-arrangement.

Track colours (Ableton integer values):
  Drums  = 14 (orange-red)  Bass = 9 (yellow)
  Chords = 6  (teal)        Lead = 1 (blue)
"""

from osc_client import AbletonOSC
import patterns as P
from typing import Optional

# ─── Section definitions ─────────────────────────────────────────────────────
# Each row: (scene_name, bars, drum_variant, bass_variant, chord_variant, lead_variant)
# None means skip that track for this section (empty clip slot).

_ARRANGEMENTS = {
    'house': [
        ('Intro',    8,  'sparse', 'sparse', None,    None   ),
        ('Verse',   16,  'full',   'full',   'slow',  None   ),
        ('Build',    8,  'build',  'full',   'slow',  'high' ),
        ('Drop',    16,  'full',   'full',   'stab',  'high' ),
        ('Break',    8,  None,     None,     'slow',  'high' ),
        ('Build 2',  8,  'build',  'full',   'slow',  'high' ),
        ('Drop 2',  16,  'full',   'full',   'stab',  'high' ),
        ('Outro',    8,  'sparse', 'sparse', None,    None   ),
    ],
    'tech_house': [
        ('Intro',    8,  'sparse', 'sparse', None,    None   ),
        ('Groove',  16,  'full',   'full',   'stab',  None   ),
        ('Build',    8,  'build',  'full',   'stab',  'high' ),
        ('Drop',    16,  'full',   'full',   'stab',  'high' ),
        ('Break',    8,  'sparse', None,     'slow',  'high' ),
        ('Build 2',  8,  'build',  'full',   'stab',  'high' ),
        ('Drop 2',  16,  'full',   'full',   'stab',  'high' ),
        ('Outro',    8,  'sparse', 'sparse', None,    None   ),
    ],
    'melodic_techno': [
        ('Intro',   16,  'sparse', None,     'slow',  None   ),
        ('A',       16,  'full',   'sparse', 'slow',  None   ),
        ('B',       16,  'full',   'full',   'slow',  'high' ),
        ('Peak',    16,  'build',  'full',   'high',  'high' ),
        ('Break',   16,  None,     None,     'slow',  'high' ),
        ('Build',    8,  'build',  'full',   'slow',  'high' ),
        ('Peak 2',  16,  'full',   'full',   'high',  'high' ),
        ('Outro',   16,  'sparse', None,     'slow',  None   ),
    ],
    'techno': [
        ('Intro',   16,  'full',   None,     None,    None   ),
        ('A',       16,  'full',   'sparse', None,    None   ),
        ('B',       16,  'full',   'full',   'stab',  None   ),
        ('Peak',    16,  'build',  'full',   'stab',  'high' ),
        ('Break',   16,  'sparse', None,     'slow',  'high' ),
        ('Build',    8,  'build',  'full',   'stab',  'high' ),
        ('Peak 2',  16,  'full',   'full',   'stab',  'high' ),
        ('Outro',   16,  'full',   None,     None,    None   ),
    ],
    'trap': [
        ('Intro',    8,  'sparse', 'sparse', None,    None   ),
        ('Verse',   16,  'full',   'full',   'slow',  None   ),
        ('Pre',      8,  'build',  'full',   'slow',  'high' ),
        ('Hook',    16,  'full',   'full',   'stab',  'high' ),
        ('Verse 2', 16,  'full',   'full',   'slow',  None   ),
        ('Pre 2',    8,  'build',  'full',   'slow',  'high' ),
        ('Hook 2',  16,  'full',   'full',   'stab',  'high' ),
        ('Outro',    8,  'sparse', 'sparse', None,    None   ),
    ],
    'future_bass': [
        ('Intro',    8,  'sparse', 'sparse', 'slow',  None   ),
        ('Verse',   16,  'full',   'full',   'slow',  None   ),
        ('Pre',      8,  'build',  'full',   'high',  'high' ),
        ('Drop',    16,  'full',   'full',   'stab',  'high' ),
        ('Break',    8,  None,     None,     'slow',  'high' ),
        ('Pre 2',    8,  'build',  'full',   'high',  'high' ),
        ('Drop 2',  16,  'full',   'full',   'stab',  'high' ),
        ('Outro',    8,  'sparse', 'sparse', 'slow',  None   ),
    ],
    'dnb': [
        ('Intro',    8,  'sparse', 'sparse', None,    None   ),
        ('Intro 2',  8,  'full',   'sparse', None,    None   ),
        ('Verse',   16,  'full',   'full',   'slow',  None   ),
        ('Drop',    16,  'full',   'full',   'stab',  'high' ),
        ('Break',    8,  'sparse', None,     'slow',  'high' ),
        ('Build',    8,  'build',  'full',   'stab',  'high' ),
        ('Drop 2',  16,  'full',   'full',   'stab',  'high' ),
        ('Outro',    8,  'sparse', 'sparse', None,    None   ),
    ],
}

_TRACK_NAMES  = ['Drums', 'Bass', 'Chords', 'Lead']

_FALLBACK_GENRE = 'house'


def get_arrangement(genre_id: str):
    return _ARRANGEMENTS.get(genre_id, _ARRANGEMENTS[_FALLBACK_GENRE])


def decide_tracks(data: dict) -> dict:
    """
    Analyse the mix data and return which tracks are actually needed.

    Returns a dict of {track_name: bool} for Drums, Bass, Chords, Lead.
    Logic: if the mix is already rich in a frequency band, skip that element;
    if it's thin, include it.
    """
    sub_pct      = float(data.get('sub_pct',      0.0))
    low_pct      = float(data.get('low_pct',      0.0))
    air_pct      = float(data.get('air_pct',      0.0))
    low_mid_pct  = float(data.get('low_mid_pct',  0.0))
    high_mid_pct = float(data.get('high_mid_pct', 0.0))
    width        = float(data.get('stereo_width',  0.0))
    lufs         = float(data.get('lufs',        -99.0))

    # Drums: add if sub+low energy is thin (kick is sub/low heavy)
    needs_drums  = (sub_pct + low_pct) < 0.30

    # Bass: add if sub energy is thin and overall level is low
    needs_bass   = sub_pct < 0.18 or lufs < -18.0

    # Chords: add if mid-range is thin or mix is very mono
    needs_chords = (low_mid_pct + high_mid_pct) < 0.40 or width < 0.25

    # Lead: add if air/presence is thin (leads sit in high-mid/air)
    needs_lead   = (air_pct + high_mid_pct) < 0.20

    result = {
        'Drums':  needs_drums,
        'Bass':   needs_bass,
        'Chords': needs_chords,
        'Lead':   needs_lead,
    }
    return result


def build(
    osc: AbletonOSC,
    genre_id: str,
    bpm: float,
    key_str: str,
    *,
    tracks_needed: Optional[dict] = None,
    dry_run: bool = False,
    verbose: bool = True,
) -> None:
    """
    Build a full arrangement in Ableton's session view.

    Parameters
    ----------
    osc           : connected AbletonOSC client
    genre_id      : e.g. 'melodic_techno'
    bpm           : session BPM (0 = skip tempo change)
    key_str       : key string like 'Am', 'F#m', 'C'
    tracks_needed : dict of {track_name: bool}; None means all tracks
    dry_run       : if True, print actions but do not send OSC
    verbose       : print progress
    """
    if tracks_needed is None:
        tracks_needed = {t: True for t in _TRACK_NAMES}
    root, is_minor = P.parse_key(key_str)
    arrangement    = get_arrangement(genre_id)
    num_sections   = len(arrangement)

    def log(*args):
        if verbose:
            print(*args)

    def act(label, fn, *args, **kwargs):
        if verbose:
            print(f'  {label}')
        if not dry_run:
            fn(*args, **kwargs)

    # ── Set session tempo ────────────────────────────────────────────────────
    if bpm > 0:
        act(f'Set tempo: {bpm} BPM', osc.set_tempo, bpm)

    # ── Create only the needed MIDI tracks ──────────────────────────────────
    active_tracks = [n for n in _TRACK_NAMES if tracks_needed.get(n, True)]
    log(f'\nAdding tracks: {", ".join(active_tracks)}')

    track_map: dict[str, int] = {}  # name -> ableton track index
    for name in active_tracks:
        if dry_run:
            idx = len(track_map)
        else:
            idx = osc.create_midi_track()
        track_map[name] = idx
        log(f'  {name} -> track index {idx}')
        if not dry_run:
            osc.set_track_name(idx, name)

    drum_t  = track_map.get('Drums',  -1)
    bass_t  = track_map.get('Bass',   -1)
    chord_t = track_map.get('Chords', -1)
    lead_t  = track_map.get('Lead',   -1)

    # ── Label scenes ─────────────────────────────────────────────────────────
    log('\nNaming scenes...')
    for slot, (name, bars, *_) in enumerate(arrangement):
        act(f'Scene {slot}: {name}', osc.set_scene_name, slot, name)

    # ── Create clips and fill with patterns ──────────────────────────────────
    log(f'\nFilling {num_sections} sections...')

    for slot, (section, bars, d_var, b_var, c_var, l_var) in enumerate(arrangement):
        beats = bars * P.BEATS
        log(f'\n  [{slot}] {section}  ({bars} bars)')

        if drum_t >= 0 and d_var is not None:
            drum_notes = P.make_drums(genre_id, d_var, bars)
            act(f'    Drums ({d_var})', _create_clip_with_notes,
                osc, drum_t, slot, beats, drum_notes, dry_run)

        if bass_t >= 0 and b_var is not None:
            bass_notes = P.make_bass(root, is_minor, bars, genre_id, b_var)
            act(f'    Bass  ({b_var})', _create_clip_with_notes,
                osc, bass_t, slot, beats, bass_notes, dry_run)

        if chord_t >= 0 and c_var is not None:
            chord_notes = P.make_chords(root, is_minor, bars, genre_id, c_var)
            act(f'    Chords ({c_var})', _create_clip_with_notes,
                osc, chord_t, slot, beats, chord_notes, dry_run)

        if lead_t >= 0 and l_var is not None:
            lead_notes = P.make_lead(root, is_minor, bars, genre_id, l_var)
            act(f'    Lead  ({l_var})', _create_clip_with_notes,
                osc, lead_t, slot, beats, lead_notes, dry_run)

    log('\nDone.')


def record_to_arrangement(
    osc: AbletonOSC,
    genre_id: str,
    bpm: float,
    *,
    tracks_needed: Optional[dict] = None,
    dry_run: bool = False,
) -> None:
    """
    Play through each session-view scene while Ableton's Arrangement Overdub
    is active, so all clips land on the arrangement timeline automatically.
    """
    import time

    if tracks_needed is None:
        tracks_needed = {t: True for t in _TRACK_NAMES}

    arrangement = get_arrangement(genre_id)
    beats_per_bar = P.BEATS

    def _has_content(d_var, b_var, c_var, l_var) -> bool:
        return (
            (tracks_needed.get('Drums')  and d_var is not None) or
            (tracks_needed.get('Bass')   and b_var is not None) or
            (tracks_needed.get('Chords') and c_var is not None) or
            (tracks_needed.get('Lead')   and l_var is not None)
        )

    if not dry_run:
        osc.send('/live/song/set/current_song_time', 0.0)
        time.sleep(0.15)
        osc.send('/live/song/set/arrangement_overdub', 1)
        time.sleep(0.1)

    total = sum(bars for _, bars, *_ in arrangement)
    print(f'  Playing {len(arrangement)} sections ({total} bars) at {bpm:.0f} BPM...')

    beat_cursor = 0.0
    for scene_idx, row in enumerate(arrangement):
        section, bars = row[0], row[1]
        d_var, b_var, c_var, l_var = row[2], row[3], row[4], row[5]
        section_beats = float(bars * beats_per_bar)
        duration = section_beats / bpm * 60.0

        if not _has_content(d_var, b_var, c_var, l_var):
            print(f'  [{scene_idx}] {section}: no patterns, skipping')
            beat_cursor += section_beats
        else:
            print(f'  [{scene_idx}] {section}: {bars} bars ({duration:.0f}s)')
            if not dry_run:
                osc.send('/live/song/set/current_song_time', beat_cursor)
                time.sleep(0.1)
                osc.fire_scene(scene_idx)
                time.sleep(duration + 0.3)
            beat_cursor += section_beats

    if not dry_run:
        osc.send('/live/song/stop_playing')
        time.sleep(0.2)
        osc.send('/live/song/set/arrangement_overdub', 0)

    print('  Done — switch to Arrangement View in Ableton to see the patterns.')


def _create_clip_with_notes(
    osc: AbletonOSC,
    track_idx: int,
    clip_slot: int,
    beats: float,
    notes: list,
    dry_run: bool,
):
    if dry_run:
        return
    osc.create_clip(track_idx, clip_slot, beats)
    ready = osc.wait_for_clip(track_idx, clip_slot)
    if not ready:
        print(f'      WARNING: clip not ready at track={track_idx} slot={clip_slot}')
        return
    chunk = 50
    for i in range(0, len(notes), chunk):
        osc.add_notes(track_idx, clip_slot, notes[i:i + chunk])
    count = osc.get_note_count(track_idx, clip_slot)
    if count < 0:
        print(f'      WARNING: could not verify notes (endpoint may be unsupported)')
    elif count == 0:
        print(f'      WARNING: clip exists but has 0 notes — add_notes may be failing')
    else:
        print(f'      OK: {count} notes in clip')
