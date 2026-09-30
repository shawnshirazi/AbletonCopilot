"""Extract genre-relevant melodies from the Hooktheory (TheoryTab) dataset released with Sheet Sage
(Donahue et al., CC BY-NC-SA 3.0; https://github.com/chrisdonahue/sheetsage-data).
Writes one .mid per entry to raw/hooktheory_midi/ and returns corpus entries."""
import gzip, json, re, os, statistics
import pretty_midi

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW = os.path.join(BASE, 'raw')

# artist -> None (all songs) or set of song slugs.  group: which bucket it belongs to
SEL = {
 # progressive / melodic house & techno and adjacent electronica (closest to target taste)
 'deadmau5': None, 'deadmau5---armin-van-buuren': None, 'deadmau5-and-kaskade': None,
 'eric-prydz': None, 'sasha': None, 'moderat': None, 'max-cooper': None, 'jon-hopkins': None,
 'nathan-fake': None, 'kolsch': None, 'nora-en-pure': None, 'rodriguez-jr': None, 'dusky': None,
 'ilan-bluestone': None, 'ilan-bluestone-and-jerome-isma-ae': None, 'rufus-du-sol': None,
 'underworld': None, 'klangkarussell': None, 'maribou-state': None, 'boys-noize': None,
 'royksopp': {'eple', 'only-this-moment', 'what-else-is-there', 'the-girl-and-the-robot---spencer-and-hill-remix', 'remind-me'},
 'jaytech': None, 'zombie-nation-and-david-whittaker': None, 'inner-city': None,
 'the-chemical-brothers': {'star-guitar', 'swoon', 'no-geography', 'galvanize'}, 'chemical-brothers': None,
 'avicii': {'levels', 'fade-into-darkness'},
 # trance / progressive trance classics (emotional-lead lineage of melodic techno)
 'faithless': None, 'paul-van-dyk': None, 'robert-miles': None, 'ferry-corsten': {'the-orange-theme-by-cygnus-x'},
 'atb': None, 'chicane-and-ferry-corsten': None, 'tiesto': {'traffic', 'secrets', 'jedidja---dancing-water', 'adagio-for-strings--original-by-samuel-barber-'},
 'gareth-emery': None, 'gareth-emery-and-ashley-wallbridge': None, 'above-and-beyond': None,
 'mat-zo': None, 'gaia---armin-van-buuren': None,
 'armin-van-buuren': {'orbion', 'pulsar', 'shivers', 'mirage', 'full-focus', 'hystereo'},
 'matt-lange-vs-gareth-emery': None, 'orjan-nilsen': None, 'heatbeat': None, 'darude': None,
 'cosmic-gate-and-cary-brothers': None, 'dash-berlin-and-jay-cosmic': None,
}
PC = ['C','C#','D','Eb','E','F','F#','G','Ab','A','Bb','B']
MODES = {'221222':'major','212212':'minor','212221':'dorian','122122':'phrygian','222122':'lydian','221221':'mixolydian','122212':'locrian'}

def lyrics_present(xml):
    return bool(re.search(r'<lyrics>\s*[^<\s]', xml or ''))

def section_of(raw):
    try: return raw['json_api'].get('section')
    except Exception: return None

def bpm_of(v, raw):
    al = v.get('alignment') or {}
    ref = (al.get('refined') or {})
    t, b = ref.get('times'), ref.get('beats')
    if t and b and len(t) > 4:
        diffs = [(t[i+1]-t[i])/(b[i+1]-b[i]) for i in range(len(t)-1) if b[i+1] > b[i]]
        return round(60/statistics.median(diffs), 1), 'audio-aligned'
    if raw:
        m = re.search(r'<BPM>([\d.]+)</BPM>', raw['json_api'].get('xmlData') or '')
        if m: return float(m.group(1)), 'theorytab-meta'
    return None, None

def build():
    d = json.load(gzip.open(os.path.join(RAW, 'Hooktheory.json.gz')))
    rawd = json.load(gzip.open(os.path.join(RAW, 'Hooktheory_Raw.json.gz')))
    outdir = os.path.join(RAW, 'hooktheory_midi'); os.makedirs(outdir, exist_ok=True)
    cands = {}
    for k, v in d.items():
        h = v['hooktheory']; a = h['artist']; s = h['song']
        if a not in SEL or (SEL[a] is not None and s not in SEL[a]): continue
        ann = v['annotations']; mel = ann.get('melody') or []
        if len(mel) < 8: continue
        if any(m.get('beats_per_bar') != 4 for m in ann['meters']): continue
        raw = rawd.get(k)
        sec = section_of(raw)
        key = (a, s, (sec or '').lower())
        # keep the most complete transcription per (artist, song, section)
        if key in cands and len(cands[key][1]['annotations']['melody']) >= len(mel): continue
        cands[key] = (k, v, raw, sec)
    entries = []
    for (a, s, _), (k, v, raw, sec) in sorted(cands.items()):
        ann = v['annotations']; key0 = ann['keys'][0]
        tonic = key0['tonic_pitch_class']; mode = MODES.get(''.join(map(str, key0['scale_degree_intervals'])), 'other')
        notes = [[float(n['onset']), float(n['offset'] - n['onset']), 60 + 12*int(n['octave']) + int(n['pitch_class'])] for n in ann['melody']]
        # Hooktheory octave 0 is the octave containing middle C; transpose up an octave for a typical lead register
        notes = [[o, du, p + 12] for o, du, p in notes]
        chords = [[float(c['onset']), float(c['offset']-c['onset']), c['root_pitch_class'], c['root_position_intervals'], c['inversion']] for c in (ann.get('harmony') or [])]
        bpm, bpm_src = bpm_of(v, raw)
        xml = raw['json_api'].get('xmlData') if raw else ''
        vocal = lyrics_present(xml)
        pm = pretty_midi.PrettyMIDI(initial_tempo=bpm or 120)
        inst = pretty_midi.Instrument(program=81, name='lead')
        spb = 60/(bpm or 120)
        for o, du, p in notes: inst.notes.append(pretty_midi.Note(100, p, o*spb, (o+du)*spb))
        pm.instruments.append(inst)
        ch = pretty_midi.Instrument(program=89, name='chords')
        for o, du, r, ivs, inv in chords:
            pcs = [r]; acc = r
            for iv in ivs: acc += iv; pcs.append(acc)
            for p in pcs: ch.notes.append(pretty_midi.Note(70, 48 + p, o*spb, (o+du)*spb))
        pm.instruments.append(ch)
        fn = f'{a}__{s}__{(sec or "section").lower().replace(" ", "-")}__{k}.mid'
        fn = re.sub(r'[^A-Za-z0-9_.-]', '', fn)
        pm.write(os.path.join(outdir, fn))
        entries.append({
            'title': s.replace('-', ' ').strip(), 'artist': a.replace('-', ' '), 'section': sec,
            'source_url': v['hooktheory']['urls']['song'], 'dataset': 'Hooktheory TheoryTab via sheetsage-data (Hooktheory.json.gz)',
            'dataset_id': k, 'kind': 'transcription', 'lead_type': 'vocal' if vocal else 'instrumental_or_unlabelled',
            'group': 'trance_prog' if a in {'faithless','paul-van-dyk','robert-miles','ferry-corsten','atb','chicane-and-ferry-corsten','tiesto','gareth-emery','gareth-emery-and-ashley-wallbridge','above-and-beyond','mat-zo','gaia---armin-van-buuren','armin-van-buuren','matt-lange-vs-gareth-emery','orjan-nilsen','heatbeat','darude','cosmic-gate-and-cary-brothers','dash-berlin-and-jay-cosmic'} else 'prog_melodic_house_techno',
            'key_guess': f'{PC[tonic]} {mode}', 'tonic_pc': tonic, 'mode': mode,
            'key_changes': len(ann['keys']) > 1,
            'bpm_if_known': bpm, 'bpm_source': bpm_src,
            'raw_file': os.path.relpath(os.path.join(outdir, fn), BASE),
            'notes': notes,
            'chords': [[o, du, PC[r], ivs, inv] for o, du, r, ivs, inv in chords],
        })
    return entries

if __name__ == '__main__':
    e = build()
    print(len(e))
    for x in e: print(x['artist'], '|', x['title'], '|', x['section'], x['key_guess'], x['bpm_if_known'], x['lead_type'], len(x['notes']))
