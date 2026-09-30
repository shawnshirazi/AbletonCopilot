"""Compute melody statistics over corpus.json and write STATS.md + stats.json."""
import json, os, collections, statistics, math, sys

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
corpus = json.load(open(os.path.join(BASE, 'corpus.json')))

SCALES = {'minor': [0,2,3,5,7,8,10], 'major': [0,2,4,5,7,9,11], 'dorian': [0,2,3,5,7,9,10],
          'phrygian': [0,1,3,5,7,8,10], 'lydian': [0,2,4,6,7,9,11], 'mixolydian': [0,2,4,5,7,9,10], 'locrian': [0,1,3,5,6,8,10]}
DEG_NAME = {0:'1',1:'b2',2:'2',3:'b3',4:'3',5:'4',6:'#4/b5',7:'5',8:'b6',9:'6',10:'b7',11:'7'}
MINORISH = {'minor', 'dorian', 'phrygian'}

def mono(notes):
    """ensure monophonic line: sort, keep highest pitch per onset, clip overlaps"""
    by = {}
    for o, d, p in notes:
        k = round(o, 4)
        if k not in by or p > by[k][2]: by[k] = [k, d, p]
    ns = [by[k] for k in sorted(by)]
    for i in range(len(ns)-1):
        ns[i][1] = min(ns[i][1], ns[i+1][0]-ns[i][0])
    return [n for n in ns if n[1] > 0]

def diatonic_index(rel, scale):
    """semitone-rel-to-tonic (can be any int) -> diatonic step index (float for chromatic)"""
    octv, pc = divmod(rel, 12)
    if pc in scale: return octv*7 + scale.index(pc)
    lo = max(i for i, s in enumerate(scale) if s < pc)
    return octv*7 + lo + 0.5

def pct(c, total):
    return f'{100*c/total:.1f}%' if total else '-'

def hist_table(counter, total, keyfmt=str, top=None, header=('value','count','share')):
    items = counter.most_common(top) if top else sorted(counter.items())
    lines = [f'| {header[0]} | {header[1]} | {header[2]} |', '|---|---|---|']
    for k, v in items: lines.append(f'| {keyfmt(k)} | {v} | {pct(v,total)} |')
    return '\n'.join(lines)

def analyse(entries):
    S = collections.defaultdict(collections.Counter); V = collections.defaultdict(list)
    for e in entries:
        ns = mono(e['notes'])
        if len(ns) < 4: continue
        tonic = e['tonic_pc']; scale = SCALES.get(e['mode'], SCALES['minor'])
        rel = [p - tonic for _, _, p in ns]
        # reference octave: put the median pitch into octave 0..11 above tonic-octave
        med = statistics.median(rel); shift = 12*math.floor(med/12)
        rel = [r - shift for r in rel]
        di = [diatonic_index(r, scale) for r in rel]
        ons = [o for o, _, _ in ns]; durs = [d for _, d, _ in ns]
        minorish = e['mode'] in MINORISH
        # ---- intervals
        ivs = [rel[i+1]-rel[i] for i in range(len(rel)-1)]
        sivs = [di[i+1]-di[i] for i in range(len(di)-1)]
        for iv in ivs: S['iv_signed'][iv] += 1; S['iv_abs'][abs(iv)] += 1
        for s in sivs: S['steps_abs'][abs(s) if abs(s) < 8 else '8+'] += 1
        for iv in ivs:
            a = abs(iv)
            S['motion']['repeat' if a == 0 else 'step (1-2 st)' if a <= 2 else 'skip (3-4 st)' if a <= 4 else 'leap (5-7 st)' if a <= 7 else 'large leap (8-11 st)' if a <= 11 else 'octave+ (12+ st)'] += 1
            if a: S['direction']['up' if iv > 0 else 'down'] += 1
        # leap recovery: leap >= 5 st followed by motion in the opposite direction (any size) / by step opposite
        for i in range(len(ivs)-1):
            if abs(ivs[i]) >= 5:
                nxt = ivs[i+1]
                S['leap_rec']['total'] += 1
                if nxt != 0 and (nxt > 0) != (ivs[i] > 0): S['leap_rec']['opposite_dir'] += 1
                if nxt != 0 and (nxt > 0) != (ivs[i] > 0) and abs(nxt) <= 2: S['leap_rec']['opposite_step'] += 1
                if nxt == 0: S['leap_rec']['then_repeat'] += 1
            if abs(ivs[i]) >= 3:
                nxt = ivs[i+1]
                S['leap3_rec']['total'] += 1
                if nxt != 0 and (nxt > 0) != (ivs[i] > 0): S['leap3_rec']['opposite_dir'] += 1
        # ---- durations (16th units)
        for d in durs:
            q = round(d*4)
            S['dur16'][q if q <= 16 else '>16'] += 1
        # ---- bars
        nbars = int(max(o for o in ons)//4) + 1
        first_bar = int(min(ons)//4)
        bars = collections.defaultdict(list)
        for (o, d, p), r in zip(ns, rel): bars[int(o//4)].append((round((o % 4)*4, 2), r, round(d*4, 2)))
        for b in range(first_bar, nbars): V['notes_per_bar'].append(len(bars.get(b, [])))
        # ---- onsets within bar (16th grid)
        for o in ons:
            pos = (o % 4)*4
            if abs(pos - round(pos)) < 1e-6: S['onset16'][int(round(pos)) % 16] += 1
            else: S['onset16']['off-grid (triplet etc.)'] += 1
        # syncopation: onset not on an 8th AND/OR notes that start off the beat and sustain across the next beat
        for o, d, p in ns:
            pos = (o % 4)*4
            S['sync']['total'] += 1
            if abs(pos - round(pos)) < 1e-6:
                pi = int(round(pos)) % 16
                if pi % 4 != 0: S['sync']['offbeat_onset'] += 1
                if pi % 2 == 1: S['sync']['odd_16th_onset'] += 1
                nextbeat = math.floor(o) + 1
                if pi % 4 != 0 and o + d > nextbeat + 1e-6: S['sync']['tied_over_beat'] += 1
            else: S['sync']['offgrid'] += 1
        # ---- repetition structure (exact pitch+rhythm, and rhythm-only, and transposed)
        def bar_sig(b, mode='full'):
            x = bars.get(b, [])
            if mode == 'full': return tuple((p, r) for p, r, _ in x)
            if mode == 'rhythm': return tuple(p for p, _, _ in x)
            if mode == 'contour':
                return tuple((x[i][0], x[i][1]-x[0][1]) for i in range(len(x))) if x else ()
        B = list(range(first_bar, nbars))
        if len(B) >= 4:
            for i in range(0, len(B)-3, 4):
                b1, b2, b3, b4 = B[i:i+4]
                if not bars.get(b1): continue
                for mode in ('full', 'rhythm', 'contour'):
                    S['rep_'+mode]['groups'] += 1
                    if bar_sig(b3, mode) == bar_sig(b1, mode): S['rep_'+mode]['bar3==bar1'] += 1
                    if bar_sig(b2, mode) == bar_sig(b1, mode): S['rep_'+mode]['bar2==bar1'] += 1
                    if bar_sig(b3, mode) == bar_sig(b1, mode) and bar_sig(b4, mode) == bar_sig(b2, mode): S['rep_'+mode]['bars3-4==bars1-2 (2-bar motif x2)'] += 1
                    if bar_sig(b3, mode) == bar_sig(b1, mode) and bar_sig(b4, mode) != bar_sig(b2, mode): S['rep_'+mode]['bar3==bar1 but bar4 differs (answer varies)'] += 1
                    if all(bar_sig(b, mode) == bar_sig(b1, mode) for b in (b2, b3, b4)): S['rep_'+mode]['1-bar loop (all 4 equal)'] += 1
            if len(B) >= 8:
                for i in range(0, len(B)-7, 8):
                    S['rep8']['groups'] += 1
                    if all(bar_sig(B[i+j]) == bar_sig(B[i+4+j]) for j in range(4)): S['rep8']['bars5-8==bars1-4 exact'] += 1
                    elif all(bar_sig(B[i+j]) == bar_sig(B[i+4+j]) for j in range(3)): S['rep8']['bars5-7==bars1-3, bar 8 varied'] += 1
                    elif all(bar_sig(B[i+j], 'rhythm') == bar_sig(B[i+4+j], 'rhythm') for j in range(4)): S['rep8']['same rhythm, pitches changed'] += 1
        # motif length: smallest period (in bars) P in {1,2,4,8} with >=75% of bars equal to bar-P
        best = None
        for P in (1, 2, 4, 8):
            pairs = [(b, b-P) for b in B if b-P >= first_bar]
            if len(pairs) < 2: continue
            same = sum(bar_sig(b) == bar_sig(a) for b, a in pairs) / len(pairs)
            if same >= 0.75: best = P; break
        S['motif_period'][f'{best}-bar' if best else 'no strict period (through-composed)'] += 1
        # ---- range, distinct pitches
        V['range'].append(max(rel)-min(rel))
        V['distinct_pc'].append(len({r % 12 for r in rel}))
        V['distinct_pitches'].append(len(set(rel)))
        for i in range(0, len(B), 4):
            chunk = [r for b in B[i:i+4] for _, r, _ in bars.get(b, [])]
            if len(chunk) >= 3: V['distinct_pitches_per_4bar'].append(len(set(chunk)))
        # ---- scale degrees
        for r, d in zip(rel, durs):
            S['deg_all'][r % 12] += 1
            S['deg_weighted'][r % 12] += d
            if minorish: S['deg_minorish'][r % 12] += 1
        for (o, d, p), r in zip(ns, rel):
            pos = (o % 4)*4
            if abs(pos-round(pos)) < 1e-6:
                pi = int(round(pos)) % 16
                if pi == 0: S['deg_downbeat'][r % 12] += 1
                if pi in (0, 8): S['deg_strong'][r % 12] += 1
                if pi in (4, 12): S['deg_weakbeat'][r % 12] += 1
        # ---- phrase endings: last note of each 2-bar and 4-bar unit, and last note of melody
        for size, key in ((4, 'end4'), (2, 'end2')):
            for i in range(0, len(B), size):
                grp = [(o, d, r) for (o, d, _), r in zip(ns, rel) if B[i] <= o//4 < B[i]+size]
                if grp:
                    S[key][grp[-1][2] % 12] += 1
                    S[key+'_pos'][round(((grp[-1][0]) % (4*size))*4)] += 1
        S['end_final'][rel[-1] % 12] += 1
        V['final_dur16'].append(round(durs[-1]*4))
        # start degree
        S['start'][rel[0] % 12] += 1
        # ---- rhythmic motifs: per-bar onset pattern (16 chars) and per-half-bar
        for b in B:
            x = bars.get(b, [])
            if not x or any(abs(p-round(p)) > 1e-6 for p, _, _ in x): continue
            pat = ['.']*16
            for p, _, _ in x: pat[int(round(p)) % 16] = 'x'
            s = ''.join(pat)
            S['bar_rhythm'][s] += 1
            S['half_rhythm'][s[:8]] += 1; S['half_rhythm'][s[8:]] += 1
        # contour of consecutive 3-note figures
        for i in range(len(ivs)-1):
            a, b = ivs[i], ivs[i+1]
            sg = lambda v: 'U' if v > 0 else 'D' if v < 0 else 'R'
            S['contour3'][sg(a)+sg(b)] += 1
        # pitch-class usage (minorish only): which 6th/7th/2nd
        if minorish:
            S['mode_labels'][e['mode']] += 1
            c = collections.Counter(r % 12 for r in rel)
            has = lambda pc: c[pc] > 0
            S['pc_usage']['uses b6 (8)'] += has(8); S['pc_usage']['uses nat 6 (9)'] += has(9)
            S['pc_usage']['uses b7 (10)'] += has(10); S['pc_usage']['uses nat 7 / leading tone (11)'] += has(11)
            S['pc_usage']['uses b2 (1)'] += has(1); S['pc_usage']['uses 2 (2)'] += has(2)
            S['pc_usage']['uses 4 (5)'] += has(5)
            S['pc_usage']['melodies'] += 1
            only_natmin = all(r % 12 in (0,2,3,5,7,8,10) for r in rel)
            S['scale_fit']['fits natural minor exactly'] += only_natmin
            S['scale_fit']['fits minor pentatonic (1 b3 4 5 b7)'] += all(r % 12 in (0,3,5,7,10) for r in rel)
            S['scale_fit']['fits dorian exactly'] += all(r % 12 in (0,2,3,5,7,9,10) for r in rel)
            S['scale_fit']['uses leading tone (harmonic/melodic minor colour)'] += has(11)
            S['scale_fit']['uses 6 or fewer pitch classes'] += len(c) <= 6
            S['scale_fit']['melodies'] += 1
        V['n_notes'].append(len(ns)); V['n_bars'].append(len(B))
        V['density'].append(len(ns)/max(1, len(B)))
    return S, V

def summarise_vals(v):
    if not v: return '-'
    q = statistics.quantiles(v, n=4) if len(v) > 1 else [v[0]]*3
    return f'mean {statistics.mean(v):.2f}, median {statistics.median(v):g}, IQR {q[0]:g}-{q[2]:g}, min {min(v)}, max {max(v)}'

def degfmt(k): return DEG_NAME.get(k, str(k))

def report(name, S, V, n):
    out = [f'## {name} (n = {n} melodies)\n']
    tot = sum(S['iv_abs'].values())
    out.append(f'### Intervals\nConsecutive-note intervals: {tot}.\n')
    out.append('**Motion classes**\n\n' + hist_table(S['motion'], tot, top=10))
    up, dn = S['direction']['up'], S['direction']['down']
    out.append(f'\nDirection of non-repeat moves: up {pct(up, up+dn)}, down {pct(dn, up+dn)}.\n')
    step = S['motion']['step (1-2 st)']; rep = S['motion']['repeat']; leap = tot - step - rep
    out.append(f'**Step : leap ratio** (excluding repeats; leap = 3+ semitones): {step}:{leap} = {step/max(1,leap):.2f}. '
               f'Steps are {pct(step, step+leap)} of moving intervals; repeats are {pct(rep, tot)} of all intervals.\n')
    out.append('**Absolute interval size (semitones)**\n\n' + hist_table(collections.Counter({k: v for k, v in S['iv_abs'].items()}), tot, keyfmt=lambda k: f'{k}'))
    out.append('\n**Signed intervals, top 15**\n\n' + hist_table(S['iv_signed'], tot, keyfmt=lambda k: f'{k:+d}', top=15))
    out.append('\n**Interval size in scale steps (diatonic; chromatic notes count as half-steps)**\n\n' + hist_table(collections.Counter({str(k): v for k, v in S['steps_abs'].items()}), sum(S['steps_abs'].values()), top=12))
    lr = S['leap_rec']; l3 = S['leap3_rec']
    out.append(f"\n**Leap recovery.** After a leap of 5+ semitones (n={lr['total']}): next note moves in the opposite direction {pct(lr['opposite_dir'], lr['total'])}, "
               f"opposite by step {pct(lr['opposite_step'], lr['total'])}, repeats {pct(lr['then_repeat'], lr['total'])}. "
               f"After any 3+ semitone move (n={l3['total']}): opposite direction {pct(l3['opposite_dir'], l3['total'])}.\n")
    ct = sum(S['contour3'].values())
    out.append('**3-note contour shapes** (U=up, D=down, R=repeat)\n\n' + hist_table(S['contour3'], ct, top=9))
    dt = sum(S['dur16'].values())
    out.append('\n### Note lengths (in 16th notes; 4 = one beat)\n\n' + hist_table(collections.Counter({str(k): v for k, v in S['dur16'].items()}), dt, top=14))
    out.append(f"\n### Density\nNotes per bar: {summarise_vals(V['notes_per_bar'])}.  \nNotes per melody: {summarise_vals(V['n_notes'])}; bars per melody: {summarise_vals(V['n_bars'])}.\n")
    npb = collections.Counter(V['notes_per_bar'])
    out.append(hist_table(npb, sum(npb.values()), header=('notes in bar', 'bars', 'share')))
    ot = sum(S['onset16'].values())
    out.append('\n### Onset position within the bar (16th grid, 0 = downbeat, 4 = beat 2, 8 = beat 3, 12 = beat 4)\n\n' +
               hist_table(collections.Counter({(f'{k:02d}' if isinstance(k, int) else k): v for k, v in S['onset16'].items()}), ot))
    sy = S['sync']
    out.append(f"\n**Syncopation.** Of {sy['total']} onsets: off the quarter-note beat {pct(sy['offbeat_onset'], sy['total'])}; "
               f"on an odd 16th (e/a positions) {pct(sy['odd_16th_onset'], sy['total'])}; off-beat onsets that sustain across the next beat (true syncopation/anticipation) {pct(sy['tied_over_beat'], sy['total'])}; off the 16th grid {pct(sy['offgrid'], sy['total'])}.\n")
    out.append('### Repetition / phrase structure (4-bar groups)\n')
    for mode, label in (('full', 'exact pitch+rhythm'), ('contour', 'transposition-invariant (same rhythm + same intervals)'), ('rhythm', 'rhythm only')):
        r = S['rep_'+mode]; g = r['groups']
        out.append(f"- **{label}** (groups={g}): bar2==bar1 {pct(r['bar2==bar1'], g)}, bar3==bar1 {pct(r['bar3==bar1'], g)}, "
                   f"bars3-4==bars1-2 {pct(r['bars3-4==bars1-2 (2-bar motif x2)'], g)}, bar3==bar1 but bar4 differs {pct(r['bar3==bar1 but bar4 differs (answer varies)'], g)}, all four bars equal {pct(r['1-bar loop (all 4 equal)'], g)}")
    r8 = S['rep8']; g8 = r8['groups']
    out.append(f"- **8-bar groups** (n={g8}): bars5-8 == bars1-4 exactly {pct(r8['bars5-8==bars1-4 exact'], g8)}; first 3 bars repeat but bar 8 varied {pct(r8['bars5-7==bars1-3, bar 8 varied'], g8)}; same rhythm with new pitches {pct(r8['same rhythm, pitches changed'], g8)}")
    mp = S['motif_period']
    out.append('\n**Smallest repeating period** (>=75% of bars identical to the bar P bars earlier)\n\n' + hist_table(mp, sum(mp.values()), top=6))
    out.append(f"\n### Range and pitch vocabulary\nRange (semitones, lowest to highest note): {summarise_vals(V['range'])}.  \n"
               f"Distinct pitch classes per melody: {summarise_vals(V['distinct_pc'])}.  \nDistinct pitches per melody: {summarise_vals(V['distinct_pitches'])}.  \n"
               f"Distinct pitches per 4-bar phrase: {summarise_vals(V['distinct_pitches_per_4bar'])}.\n")
    for key, label in (('deg_all', 'Scale-degree usage, all notes (count)'), ('deg_weighted', 'Scale-degree usage weighted by duration (beats)'),
                       ('deg_downbeat', 'Degree of notes starting on the downbeat (beat 1)'), ('deg_strong', 'Degree of notes on strong beats (beats 1 and 3)'),
                       ('deg_weakbeat', 'Degree of notes on beats 2 and 4'), ('start', 'First note of the melody'),
                       ('end2', 'Last note of each 2-bar unit'), ('end4', 'Last note of each 4-bar phrase'), ('end_final', 'Final note of the transcribed section')):
        c = S[key]; t = sum(c.values())
        cc = collections.Counter({degfmt(k): (round(v, 1) if isinstance(v, float) else v) for k, v in c.items()})
        out.append(f'\n**{label}**\n\n' + hist_table(cc, t, top=12, header=('degree (rel. to tonic)', 'count', 'share')))
    ep = S['end4_pos']; et = sum(ep.values())
    out.append('\n**Where the last note of a 4-bar phrase starts** (16th index within the 4-bar phrase, 0-63), top 10\n\n' + hist_table(ep, et, top=10, header=('16th index', 'count', 'share')))
    out.append(f"\nDuration of final note (16ths): {summarise_vals(V['final_dur16'])}.\n")
    if S['pc_usage']['melodies']:
        m = S['pc_usage']['melodies']
        out.append(f"### Pitch-class usage in minor-type melodies (n={m}; key modes labelled by annotators: {dict(S['mode_labels'])})\n")
        out.append('| feature | melodies | share |\n|---|---|---|')
        for k in ['uses b2 (1)', 'uses 2 (2)', 'uses 4 (5)', 'uses b6 (8)', 'uses nat 6 (9)', 'uses b7 (10)', 'uses nat 7 / leading tone (11)']:
            out.append(f'| {k} | {S["pc_usage"][k]} | {pct(S["pc_usage"][k], m)} |')
        for k in ['fits natural minor exactly', 'fits dorian exactly', 'fits minor pentatonic (1 b3 4 5 b7)', 'uses leading tone (harmonic/melodic minor colour)', 'uses 6 or fewer pitch classes']:
            out.append(f'| {k} | {S["scale_fit"][k]} | {pct(S["scale_fit"][k], m)} |')
        c = S['deg_minorish']; t = sum(c.values())
        out.append('\n**Degree histogram, minor-type melodies only (note counts)**\n\n' + hist_table(collections.Counter({degfmt(k): v for k, v in c.items()}), t, top=12, header=('degree', 'count', 'share')))
    br = S['bar_rhythm']; bt = sum(br.values())
    out.append('\n### Recurring rhythmic motifs\nOne-bar onset patterns (x = note onset on that 16th; `.` = no onset), top 15:\n\n' +
               hist_table(collections.Counter({f'`{k[:4]} {k[4:8]} {k[8:12]} {k[12:]}`': v for k, v in br.items()}), bt, top=15, header=('pattern', 'bars', 'share')))
    hr = S['half_rhythm']; ht = sum(hr.values())
    out.append('\nHalf-bar (2-beat) onset cells, top 12:\n\n' + hist_table(collections.Counter({f'`{k[:4]} {k[4:]}`': v for k, v in hr.items()}), ht, top=12, header=('cell', 'count', 'share')))
    return '\n'.join(out), S, V

def chord_stats(entries):
    ROM_MIN = {0:'i',1:'bII',2:'ii',3:'bIII',4:'III',5:'iv',6:'#iv',7:'v',8:'bVI',9:'vi',10:'bVII',11:'vii'}
    PCI = {n: i for i, n in enumerate(['C','C#','D','Eb','E','F','F#','G','Ab','A','Bb','B'])}
    prog = collections.Counter(); roots = collections.Counter()
    for e in entries:
        if e['mode'] not in MINORISH or not e.get('chords'): continue
        seq = []
        for o, d, r, ivs, inv in e['chords']:
            deg = (PCI[r] - e['tonic_pc']) % 12
            minor_q = ivs[:1] == [3]
            base = ROM_MIN[deg]
            lab = (base.lower() if minor_q else base.upper().replace('B', 'b'))
            if ivs[:2] == [3, 3]: lab += 'dim'
            if not seq or seq[-1] != lab: seq.append(lab)
            roots[lab] += d
        # count 4-chord loops
        if len(seq) >= 4: prog[' - '.join(seq[:4])] += 1
    return prog, roots

if __name__ == '__main__':
    core = [e for e in corpus if e.get('core')]
    ht_all = [e for e in corpus if e['kind'] == 'transcription']
    packs = [e for e in corpus if e['kind'] == 'midi_pack']
    minor_core = [e for e in core if e['mode'] in MINORISH]
    parts = []
    txt_core, S, V = report('A. CORE: instrumental leads, 110-140 BPM, prog/melodic house, techno & trance transcriptions', analyse(core)[0], analyse(core)[1], len(core))
    txt_min, S2, V2 = report('B. CORE, minor-type keys only (minor/dorian/phrygian)', *analyse(minor_core), len(minor_core))
    pmh = [e for e in core if e['group'] == 'prog_melodic_house_techno']
    txt_pmh, S3, V3 = report('C. CORE subset: progressive/melodic house & techno artists only (no trance)', *analyse(pmh), len(pmh))
    txt_pack, S4, V4 = report('D. MIDI-pack trance leads/arps (unknown pack, semedin repo) - for contrast only', *analyse(packs), len(packs))
    def style(e):
        ns = mono(e['notes']); iv = [abs(ns[i+1][2]-ns[i][2]) for i in range(len(ns)-1)]
        return 'arp' if sum(x >= 8 for x in iv) / max(1, len(iv)) >= 0.15 else 'line'
    for e in corpus: e['style_auto'] = style(e)
    lines = [e for e in core if e['style_auto'] == 'line']
    arps = [e for e in core if e['style_auto'] == 'arp']
    txt_line, S5, V5 = report('E. CORE "line" melodies (fewer than 15% of intervals are 8+ semitones, i.e. not pedal/octave arps)', *analyse(lines), len(lines))
    txt_arp, S6, V6 = report('F. CORE "arp/pedal" melodies (15%+ of intervals are 8+ semitone jumps)', *analyse(arps), len(arps))
    # per-melody table
    rows = ['| artist | title | section | key | BPM | style | notes | bars | notes/bar | repeat% | step% of moves | range | offbeat onset% | pcs |', '|---|---|---|---|---|---|---|---|---|---|---|---|---|---|']
    for e in sorted(core, key=lambda e: (e['artist'], e['title'])):
        ns = mono(e['notes']); iv = [ns[i+1][2]-ns[i][2] for i in range(len(ns)-1)]
        mv = [x for x in iv if x]; bars = int(max(n[0] for n in ns)//4) - int(min(n[0] for n in ns)//4) + 1
        off = sum(1 for o, _, _ in ns if abs((o % 1)) > 1e-6)
        rows.append(f"| {e['artist']} | {e['title']} | {e['section']} | {e['key_guess']} | {e['bpm_if_known']} | {e['style_auto']} | {len(ns)} | {bars} | {len(ns)/bars:.1f} | {100*iv.count(0)/max(1,len(iv)):.0f} | {100*sum(abs(x)<=2 for x in mv)/max(1,len(mv)):.0f} | {max(n[2] for n in ns)-min(n[2] for n in ns)} | {100*off/len(ns):.0f} | {len({n[2]%12 for n in ns})} |")
    open(os.path.join(BASE, 'per_melody_table.md'), 'w').write('\n'.join(rows))
    json.dump(corpus, open(os.path.join(BASE, 'corpus.json'), 'w'), indent=1)
    extra = '\n\n' + txt_line + '\n\n' + txt_arp
    prog, roots = chord_stats(core)
    json.dump({'core': {k: dict((str(a), b) for a, b in v.items()) for k, v in S.items()}}, open(os.path.join(BASE, 'stats.json'), 'w'), indent=1)
    open(os.path.join(BASE, 'stats_sections.md'), 'w').write('\n\n'.join([txt_core, txt_min, txt_pmh, txt_pack]) + extra +
        '\n\n## Harmony (core, minor-type): first four distinct chords of each transcription\n\n' +
        hist_table(prog, sum(prog.values()), top=15, header=('progression', 'melodies', 'share')) +
        '\n\nChord-root time share (beats):\n\n' + hist_table(collections.Counter({k: round(v, 1) for k, v in roots.items()}), sum(roots.values()), top=12, header=('root', 'beats', 'share')))
    print('ok', len(core), len(minor_core), len(pmh), len(packs), 'line', len(lines), 'arp', len(arps))
