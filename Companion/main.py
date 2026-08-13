#!/usr/bin/env python3
"""
AbletonCopilot Companion

On each new analysis from the VST3:
  1. Scans your sample library and matches files by BPM / key / genre.
  2. Renders a synced drum stack (one stem per element, per section) into
     ~/Library/AbletonCopilot/Session/DrumStack — drag the stems onto
     separate tracks in Ableton and they play back in sync.
  3. Optionally lays down matching MIDI drum clips in Session View via
     AbletonOSC, if Ableton is running with the AbletonOSC control surface.

Usage:
  python main.py --library ~/Music/Samples
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

import patterns as P
import midi_writer

ANALYSIS_PATH = Path.home() / 'Library' / 'AbletonCopilot' / 'analysis.json'
SESSION_DIR   = Path.home() / 'Library' / 'AbletonCopilot' / 'Session'

# Sections to generate MIDI for (name, bars, drum_var, bass_var, chord_var, lead_var)
SECTIONS = {
    'house':          [('Intro',8,'sparse','sparse',None,None),('Drop',16,'full','full','stab','high'),('Break',8,None,None,'slow','high'),('Outro',8,'sparse','sparse',None,None)],
    'tech_house':     [('Intro',8,'sparse','sparse',None,None),('Groove',16,'full','full','stab',None),('Drop',16,'full','full','stab','high'),('Outro',8,'sparse',None,None,None)],
    'melodic_techno': [('Intro',16,'sparse',None,'slow',None),('A',16,'full','sparse','slow',None),('Peak',16,'build','full','high','high'),('Break',16,None,None,'slow','high'),('Outro',16,'sparse',None,'slow',None)],
    'techno':         [('Intro',16,'full',None,None,None),('A',16,'full','sparse','stab',None),('Peak',16,'build','full','stab','high'),('Outro',16,'full',None,None,None)],
    'trap':           [('Intro',8,'sparse','sparse',None,None),('Verse',16,'full','full','slow',None),('Hook',16,'full','full','stab','high'),('Outro',8,'sparse','sparse',None,None)],
    'future_bass':    [('Intro',8,'sparse','sparse','slow',None),('Drop',16,'full','full','stab','high'),('Break',8,None,None,'slow','high'),('Outro',8,'sparse','sparse','slow',None)],
    'dnb':            [('Intro',8,'sparse','sparse',None,None),('Verse',16,'full','full','slow',None),('Drop',16,'full','full','stab','high'),('Outro',8,'sparse','sparse',None,None)],
}


def _load_analysis(path: Path):
    try:
        with open(path) as f:
            return json.load(f)
    except Exception:
        return None


def _set_tempo(host: str, bpm: float):
    try:
        from osc_client import AbletonOSC
        osc = AbletonOSC(host=host)
        time.sleep(0.3)
        if osc.ping():
            osc.set_tempo(bpm)
            print(f'  Set Ableton tempo: {bpm} BPM')
        osc.close()
    except Exception as e:
        print(f'  (AbletonOSC not available: {e})')


def _build_session(data: dict, library_root: str | None):
    genre_id = data.get('genre_id', 'house')
    bpm      = float(data.get('bpm', 128))
    key_str  = data.get('key', 'C')

    root, is_minor = P.parse_key(key_str)
    sections = SECTIONS.get(genre_id, SECTIONS['house'])

    # Fresh session folder
    if SESSION_DIR.exists():
        shutil.rmtree(SESSION_DIR)
    SESSION_DIR.mkdir(parents=True)

    # ── MIDI patterns ─────────────────────────────────────────────────────────
    midi_dir = SESSION_DIR / 'MIDI_Patterns'
    midi_dir.mkdir()

    track_makers = {
        'Drums':  lambda var, bars: P.make_drums(genre_id, var, bars),
        'Bass':   lambda var, bars: P.make_bass(root, is_minor, bars, genre_id, var),
        'Chords': lambda var, bars: P.make_chords(root, is_minor, bars, genre_id, var),
        'Lead':   lambda var, bars: P.make_lead(root, is_minor, bars, genre_id, var),
    }
    track_vars = ['Drums', 'Bass', 'Chords', 'Lead']

    for i, row in enumerate(sections):
        name, bars, d_var, b_var, c_var, l_var = row
        for track, var in zip(track_vars, [d_var, b_var, c_var, l_var]):
            if var is None:
                continue
            notes = track_makers[track](var, bars)
            fname = f'{i+1:02d}_{name.replace(" ","_")}_{track}.mid'
            midi_writer.write(str(midi_dir / fname), notes, bpm)

    print(f'  MIDI patterns: {len(list(midi_dir.iterdir()))} files')

    # ── Sample matching ───────────────────────────────────────────────────────
    if library_root and os.path.isdir(library_root):
        from scanner import SampleLibrary
        lib = SampleLibrary(library_root)
        count = lib.scan()
        matches = lib.find_matches(bpm, root, is_minor, genre_id)

        TYPE_TO_FOLDER = {
            'KICK':   'Drums', 'SNARE':  'Drums', 'CLAP':   'Drums',
            'HIHAT':  'Drums', 'CYMBAL': 'Drums', 'PERC':   'Drums', 'DRUM': 'Drums',
            'BASS':   'Bass',
            'CHORD':  'Chords', 'PAD': 'Chords',
            'LEAD':   'Lead', 'ARP': 'Lead', 'MELODY': 'Lead', 'PLUCK': 'Lead',
            'VOCAL':  'Vocals',
            'FX':     'FX',
            'LOOP':   'Loops',
            'UNKNOWN':'Misc',
        }

        placed = 0
        for sample_type, entries in matches.items():
            folder_name = TYPE_TO_FOLDER.get(sample_type, 'Misc')
            dest = SESSION_DIR / 'Samples' / folder_name / sample_type
            dest.mkdir(parents=True, exist_ok=True)
            for e in entries:
                try:
                    os.symlink(e.path, dest / e.name)
                    placed += 1
                except FileExistsError:
                    pass

        print(f'  Samples indexed: {count}  |  matched & linked: {placed}')
        _print_sample_summary(matches)
    else:
        print('  No library specified — MIDI patterns only.')

    return SESSION_DIR


def _print_sample_summary(matches: dict):
    priority = ['KICK','SNARE','HIHAT','BASS','CHORD','PAD','LEAD','ARP','MELODY','VOCAL','FX']
    print('\n  Top matched samples:')
    shown = set()
    for typ in priority + sorted(matches.keys()):
        if typ in shown or typ not in matches:
            continue
        shown.add(typ)
        entries = matches[typ][:2]
        for e in entries:
            bpm_s = f' {e.bpm:.0f}bpm' if e.bpm else ''
            key_s = f' {e.key_root}' if e.key_root is not None else ''
            print(f'    [{typ:<7}] {e.name}{bpm_s}{key_s}')


def run_once(data: dict, library_root: str | None, host: str, dry_run: bool,
             audio_cache=None):
    import arrangement
    import stack_renderer

    genre_id = data.get('genre_id', 'house')
    bpm      = float(data.get('bpm', 0))
    key_str  = data.get('key', 'C')
    genre    = data.get('genre_name', '?')
    print(f'\n  Genre: {genre}  |  BPM: {bpm:.1f}  |  Key: {key_str}')

    # Match samples and render a synced drum stack (stems per section)
    if library_root and os.path.isdir(library_root):
        from scanner import SampleLibrary
        import patterns as P_inner
        root, is_minor = P_inner.parse_key(key_str)
        lib = SampleLibrary(library_root)
        lib.scan()
        matches = lib.find_matches(bpm, root, is_minor, genre_id,
                                   track_data=data, audio_cache=audio_cache)
        _print_sample_summary(matches)
        if not dry_run:
            print('\nRendering drum stack...')
            stack_dir = stack_renderer.build(matches, genre_id, bpm, SESSION_DIR)
            print(f'Done. Drag the stems from {stack_dir} onto separate tracks in Ableton.')

    # Also lay down MIDI drum clips in Session View via AbletonOSC (optional —
    # requires Ableton running with the AbletonOSC control surface installed)
    from osc_client import AbletonOSC
    tracks_needed = {'Drums': True, 'Bass': False, 'Chords': False, 'Lead': False}

    print('\nConnecting to Ableton...')
    osc = AbletonOSC(host=host)
    time.sleep(0.3)

    if not dry_run and not osc.ping():
        print('  (AbletonOSC not responding — skipping Session View MIDI clips)')
        osc.close()
        return
    print('Connected.')

    print('\nGenerating drum patterns in Session View...')
    arrangement.build(osc, genre_id, bpm, key_str,
                      tracks_needed=tracks_needed, dry_run=dry_run)
    print('Done. Switch to Session View in Ableton (press Tab) to see the drum clips.')

    osc.close()


COOLDOWN_SECS = 90  # ignore any new analysis within 90s of the last run

def watch_loop(library_root: str | None, host: str, dry_run: bool, audio_cache=None):
    print(f'\nWatching: {ANALYSIS_PATH}')
    print('Play your track in Ableton, then stop — the plugin will auto-analyze.')
    print('Press Ctrl-C to quit.\n')

    last_mtime  = ANALYSIS_PATH.stat().st_mtime if ANALYSIS_PATH.exists() else None
    last_run_at = 0.0

    while True:
        try:
            if ANALYSIS_PATH.exists():
                mtime = ANALYSIS_PATH.stat().st_mtime
                if mtime != last_mtime:
                    last_mtime = mtime
                    since_last = time.time() - last_run_at
                    if since_last < COOLDOWN_SECS:
                        remaining = int(COOLDOWN_SECS - since_last)
                        print(f'  (cooldown — ignoring, {remaining}s remaining)')
                    else:
                        data = _load_analysis(ANALYSIS_PATH)
                        if data:
                            ts = datetime.now().strftime('%H:%M:%S')
                            print(f'[{ts}] New analysis detected:')
                            last_run_at = time.time()
                            run_once(data, library_root, host, dry_run,
                                     audio_cache=audio_cache)
            time.sleep(1.0)
        except KeyboardInterrupt:
            print('\nExiting.')
            break
        except Exception as e:
            print(f'Error: {e}')
            time.sleep(2.0)


def _banner():
    print('=' * 56)
    print('  AbletonCopilot Companion  v0.2')
    print('=' * 56)


def main():
    _banner()
    ap = argparse.ArgumentParser()
    ap.add_argument('--library', metavar='DIR', default=str(Path.home()),
                    help='Sample library root')
    ap.add_argument('--host', default='127.0.0.1')
    ap.add_argument('--no-watch', action='store_true')
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('--index', action='store_true',
                    help='Analyze entire sample library and build audio cache, then exit')
    ap.add_argument('--genre', metavar='ID',
                    help='Skip the plugin/analysis.json — render a drum stack directly '
                         'for this genre (house, tech_house, melodic_techno, techno, '
                         'trap, future_bass, dnb). Requires --bpm.')
    ap.add_argument('--bpm', type=float, help='BPM to use with --genre')
    ap.add_argument('--key', default='C', help='Key to use with --genre (e.g. Am, F#m, C)')
    args = ap.parse_args()

    if args.dry_run:
        print('  [DRY RUN]')

    # Open (or create) the audio feature cache
    audio_cache = None
    try:
        from audio_analyzer import SampleCache
        audio_cache = SampleCache()
    except Exception as e:
        print(f'  (audio cache unavailable: {e})')

    if args.index:
        if not args.library:
            print('--index requires --library')
            sys.exit(1)
        if audio_cache is None:
            print('librosa is required for --index. Run: pip install librosa')
            sys.exit(1)
        from scanner import SampleLibrary
        print(f'\nIndexing {args.library} ...')
        lib = SampleLibrary(args.library)
        lib.scan_and_analyze(audio_cache, max_workers=4)
        print('Indexing complete.')
        audio_cache.close()
        return

    if args.genre:
        if not args.bpm:
            print('--genre requires --bpm')
            sys.exit(1)
        data = {'genre_id': args.genre, 'genre_name': args.genre,
                'bpm': args.bpm, 'key': args.key}
        run_once(data, args.library, args.host, args.dry_run, audio_cache=audio_cache)
    elif args.no_watch:
        data = _load_analysis(ANALYSIS_PATH)
        if not data:
            print(f'No analysis at {ANALYSIS_PATH}')
            sys.exit(1)
        run_once(data, args.library, args.host, args.dry_run, audio_cache=audio_cache)
    else:
        watch_loop(args.library, args.host, args.dry_run, audio_cache=audio_cache)


if __name__ == '__main__':
    main()
