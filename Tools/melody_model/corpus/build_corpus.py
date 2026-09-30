"""Build corpus.json from (1) Hooktheory TheoryTab transcriptions and (2) MIDI-pack files found on GitHub."""
import json, os, glob, shutil, re
import pretty_midi
from build_hooktheory import build as build_ht, BASE

# Sections the (human) annotators transcribed from a sung vocal line, per my knowledge of the records.
# Only used to separate vocal hooks from instrumental leads; notes themselves are untouched.
VOCAL = {
 ('above and beyond', 'sun and moon'), ('above and beyond', 'always'), ('above and beyond', 'tightrope feat marty longstaff'),
 ('above and beyond', 'good for me   ambient mix'), ('deadmau5 and kaskade', 'i remember'), ('deadmau5', 'professional griefers'),
 ('deadmau5', 'the veldt'), ('deadmau5', 'raise your weapon   madeon remix'), ('deadmau5', 'monophobia'),
 ('eric prydz', 'call on me'), ('royksopp', 'what else is there'), ('royksopp', 'remind me'), ('royksopp', 'only this moment'),
 ('royksopp', 'the girl and the robot   spencer and hill remix'), ('gareth emery', 'yesterday'),
 ('matt lange vs gareth emery', 'another you another me'), ('heatbeat', 'secret feat quilla'),
 ('cosmic gate and cary brothers', 'wake your mind   tritonal remix'), ('dash berlin and jay cosmic', 'here tonight   lush and simon remix'),
 ('maribou state', 'nervous tics'), ('the chemical brothers', 'swoon'), ('the chemical brothers', 'no geography'),
 ('gareth emery', 'eye of the storm   stadiumx remix'), ('mat zo', 'lucid dreams'),
}
VOCAL_SECTIONS = {('rufus du sol', 'brighter', 'Chorus')}

def tempo_fix(e):
    """Hooktheory annotators sometimes notate in half- or double-time. Rescale beats so the grid is the club beat."""
    b = e['bpm_if_known']
    f = 1.0
    if b:
        if 55 <= b < 72: f = 2.0
        elif 220 <= b <= 280: f = 0.5
    if f != 1.0:
        e['notes'] = [[o*f, d*f, p] for o, d, p in e['notes']]
        e['chords'] = [[c[0]*f, c[1]*f] + c[2:] for c in e['chords']]
        e['bpm_if_known'] = round(b*f, 1)
        e['beat_rescale'] = f
    b = e['bpm_if_known']
    e['in_tempo_range_110_140'] = bool(b and 110 <= b <= 140)
    return e

def skyline(notes):
    """keep highest note per onset (for polyphonic/interleaved pack MIDI)"""
    by = {}
    for o, d, p in notes:
        k = round(o, 3)
        if k not in by or p > by[k][2]: by[k] = [k, d, p]
    return [by[k] for k in sorted(by)]

def midi_notes(path):
    pm = pretty_midi.PrettyMIDI(path)
    tpb = pm.resolution
    out = []
    for inst in pm.instruments:
        if inst.is_drum: continue
        for n in inst.notes:
            s = pm.time_to_tick(n.start)/tpb; e = pm.time_to_tick(n.end)/tpb
            out.append([round(s*4)/4 if abs(s*4-round(s*4)) < 0.1 else round(s, 3), round(e-s, 3), n.pitch])
    tempos = pm.get_tempo_changes()[1]
    return sorted(out), (round(float(tempos[0]), 1) if len(tempos) else None)

PC = ['C','C#','D','Eb','E','F','F#','G','Ab','A','Bb','B']
MAJ = [0,2,4,5,7,9,11]; MIN = [0,2,3,5,7,8,10]
def key_guess(notes):
    """Krumhansl-free simple guess: best diatonic fit, tie broken by weight of candidate tonic on long/first/last notes."""
    import collections
    w = collections.Counter()
    for o, d, p in notes: w[p % 12] += d
    best = None
    for t in range(12):
        for name, sc in (('minor', MIN), ('major', MAJ)):
            fit = sum(w[(t+s) % 12] for s in sc)
            tonic_w = w[t] + (2 if notes[-1][2] % 12 == t else 0) + (1 if notes[0][2] % 12 == t else 0)
            sc_ = (fit, tonic_w + (0.5 if name == 'minor' else 0))
            if best is None or sc_ > best[0]: best = (sc_, t, name)
    return best[1], best[2]

def build_packs():
    entries = []
    src = os.path.join(BASE, 'repos', 'semedin_Music-CheatSheet', 'material')
    dst = os.path.join(BASE, 'raw', 'midi_pack_semedin'); os.makedirs(dst, exist_ok=True)
    for f in sorted(glob.glob(os.path.join(src, '*.mid'))):
        name = os.path.basename(f)
        shutil.copy(f, os.path.join(dst, name))
        notes, bpm = midi_notes(f)
        lead = skyline(notes)
        t, mode = key_guess(lead)
        entries.append({
            'title': name[:-4], 'artist': 'unknown pack author',
            'source_url': 'https://github.com/semedin/Music-CheatSheet/tree/main/material',
            'dataset': 'MIDI files shipped in semedin/Music-CheatSheet material/ (origin pack not stated; trance lead/arp MIDI)',
            'kind': 'midi_pack', 'lead_type': 'pack_lead_arp', 'group': 'midi_pack_trance',
            'key_guess': f'{PC[t]} {mode} (auto)', 'tonic_pc': t, 'mode': mode, 'bpm_if_known': bpm if name.startswith('Melody') else None,
            'in_tempo_range_110_140': False,
            'raw_file': os.path.relpath(os.path.join(dst, name), BASE),
            'notes': lead, 'polyphonic_source': len(lead) != len(notes),
        })
    return entries

if __name__ == '__main__':
    ht = [tempo_fix(e) for e in build_ht()]
    for e in ht:
        vocal = ((e['artist'], e['title']) in VOCAL and e['section'] != 'Instrumental') or (e['artist'], e['title'], e['section']) in VOCAL_SECTIONS
        e['lead_type'] = 'vocal_hook' if vocal else 'instrumental_lead'
        e['core'] = (not vocal) and (e['in_tempo_range_110_140'] or (e['bpm_if_known'] is None and (e['artist'], e['title']) == ('deadmau5', 'strobe')))
    packs = build_packs()
    for e in packs: e['core'] = False
    corpus = ht + packs
    json.dump(corpus, open(os.path.join(BASE, 'corpus.json'), 'w'), indent=1)
    print('total', len(corpus), 'hooktheory', len(ht), 'core', sum(e['core'] for e in corpus), 'packs', len(packs))
    print('unique songs', len({(e['artist'], e['title']) for e in ht}), 'core unique songs', len({(e['artist'], e['title']) for e in ht if e['core']}))
    for e in ht:
        if not e['core']: print('non-core:', e['artist'], '|', e['title'], '|', e['section'], e['bpm_if_known'], e['lead_type'])
