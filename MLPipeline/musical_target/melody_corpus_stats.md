# Melody corpus: what real, well-loved leads do

This corpus is the training data behind the learned melody model (`Source/Engine/MelodyModel.h`,
`Source/Engine/MelodyModelData.h`, `Tools/melody_model/train_melody_model.py`). The user asked for
melodies learned from the best, most enjoyable ones rather than melodies that are merely in key.

- **Source:** human transcriptions from the Hooktheory TheoryTab dataset, as published with Sheet
  Sage (Donahue et al., github.com/chrisdonahue/sheetsage-data, **CC BY-NC-SA 3.0**). The entries
  were filtered to instrumental melodic/progressive house, techno and trance leads at 110–140 BPM.
  The scripts that rebuild the corpus are in `Tools/melody_model/corpus/`.
- **What the repo stores:** only aggregate statistics (in `MelodyModelData.h`) and the analysis
  below. It stores no corpus melody, and the generator never reproduces one.
- **License note:** the learned tables are derived from a non-commercial, share-alike dataset.
  Keep that in mind before any commercial distribution of the plugin.
- **Coverage:** the network in the build environment blocked every source for Afterlife-core
  artists (Tale Of Us, Anyma, ARTBAT, Bodzin…). The closest material in the corpus is the
  deadmau5 / Prydz / Sasha / Nathan Fake / Max Cooper / Rodriguez Jr / Kölsch lineage, plus
  classic trance.

## What's in the corpus

`corpus.json` holds 176 entries. Each entry has `title, artist, section, source_url, kind, key_guess, tonic_pc, mode, bpm_if_known, notes [[start_beats, dur_beats, midi_pitch], ...], chords`, plus metadata (`lead_type`, `group`, `core`, `style_auto`, `raw_file`, `dataset_id`).

| subset | n | what |
|---|---|---|
| Hooktheory TheoryTab transcriptions | 146 sections from 117 songs | Human transcriptions of the melody and chords for one section of a real record. They come from the Sheet Sage release of the Hooktheory dataset (`Hooktheory.json.gz` and `Hooktheory_Raw.json.gz`, CC BY-NC-SA 3.0, fetched from raw.githubusercontent.com/chrisdonahue/sheetsage-data). Each one is also written out as a .mid file in `raw/hooktheory_midi/`. |
| **core** (used for the headline stats) | **102 sections from 86 songs** | The instrumental leads (entries I labelled as vocal hooks are excluded) at 110-140 BPM. 50 are from progressive/melodic house and techno artists, 52 from trance/progressive trance. |
| MIDI pack | 30 | These are `Melody 1-18.mid` and `Trance_melody_001-012.mid`, found in `github.com/semedin/Music-CheatSheet/material/`. The original pack is not named, and the files are 138 BPM trance leads/arps. They are reported separately and are **not** in the core stats. |

**Well-known tracks in the core set:** deadmau5 *Strobe*, *Some Chords*, *Clockwork*, *Arguru*, *October*, *Alone With You*, *Imaginary Friends*, *Avaritia*, *Jaded*, *Maths*, *Slip*, *Saved*, *There Might Be Coffee*, *Suite 03*, *Ghosts n Stuff* (intro), *FML/Your Ad Here*, *Notos* (with Armin). Eric Prydz *Opus*, *Generate*, *Pjanoo*, *Tether* (instrumental). Sasha *Xpander*. Nathan Fake *The Sky Was Pink* (Holden mix). Max Cooper *Autumn Haze*. Nora En Pure *Sweet Melody*. Rodriguez Jr. *Shapes I See*. Kölsch *All That Matters* (Kryder rmx). Rüfüs Du Sol *Brighter* (intro) and *Sundream*. Underworld *Luetin*. Chemical Brothers *Star Guitar*, *Saturate*. Röyksopp *What Else Is There* (instrumental). Klangkarussell *Sonnentanz*. Dusky *No More*. Ilan Bluestone *43*. Jaytech *Paradox*, *Pyramid*, *Multiverse*. Avicii *Levels*, *Fade Into Darkness*. Inner City *Good Life*. Faithless *Insomnia*. Paul van Dyk *For An Angel*. Robert Miles *Children*, *Fable*. ATB *9 PM*, *Ecstasy*. Tiësto *Traffic*, *Secrets*, *Jedidja*. Armin van Buuren *Shivers*, *Orbion*, *Pulsar*, *Mirage*, *Hystereo*, *Full Focus*. Gaia *Tuvan*, *Stellar*. Gareth Emery *Exposure*, *Metropolis*, *Mansion*. Mat Zo *The Lost*, *Bipolar*, *Superman*. Above & Beyond *Anjunabeach*. Darude *Sandstorm*. Heatbeat, Ørjan Nilsen and others.

**Not obtained: Afterlife-core material.** There is nothing symbolic from Tale Of Us, Anyma, ARTBAT, Stephan Bodzin, Mind Against, Adriatique, Innellea, Massano, Argy, Kevin de Vries, Colyn, Agents Of Time, Mathame, Recondite, Joris Voorn, Boris Brejcha, Monolink, Camelphat, Meduza or Innēr Sense. None of these artists are in the Hooktheory dataset. The sites that might hold them (onlinesequencer, musescore, hooktheory.com, bitmidi, freemidi, nonstop2k, productionmusiclive, cymatics, reddit, wikipedia and others) are blocked by the egress proxy for both curl and WebFetch. The closest real material is the deadmau5, Prydz, Sasha, Nathan Fake, Max Cooper, Nora En Pure, Rodriguez Jr and Kölsch lineage above.

**How the data was processed and its caveats**
- Hooktheory entries are **user transcriptions of one section**, not stems. They are usually faithful but sometimes simplified: one voice only, and the annotator picked the octave.
- Key and mode are the annotator's own label. They are used as the tonic reference, so the analysis is in scale degrees relative to that tonic.
- BPM comes from Hooktheory's audio beat alignment. Entries notated in half time or double time (a raw BPM of 55-72 or 220-280) were rescaled to the club beat, and those entries are marked `beat_rescale`.
- The "vocal hook" labels are my own judgement from knowing the records. The dataset does not reliably mark vocals.
- Statistics are pooled over notes, so long transcriptions carry more weight. Deadmau5 *Clockwork* (384 notes) and Prydz *Opus* (192) are the largest. `per_melody_table.md` gives the per-melody values.
- The melodies split cleanly into two archetypes. **"line"** melodies (71) have fewer than 15% of their intervals at 8 or more semitones. **"arp/pedal"** melodies (31) are figures that alternate with a pedal tone or jump by octaves, e.g. *Opus*, *Insomnia*, *Anjunabeach*. Pooled numbers mix the two, so sections E and F report them separately. A generator should probably model them separately as well.

## Headline numbers (core, n = 102; "line" subset in brackets where it differs a lot)

| metric | value |
|---|---|
| Key/mode | 90 of 102 are minor-type (81 aeolian, 9 dorian); 8 major, 3 lydian, 1 mixolydian |
| Tempo | median 128 BPM (these are the audio-aligned values) |
| Repeated notes | 34% of intervals (line: **48%**) |
| Steps (1-2 st) | 20% of intervals; 30% of moving intervals (line: **27% / 51%**) |
| Step : leap (3+ st) | 0.43 (line: **1.06**; arp: 0.10) |
| Most common moves | 0, ±2, −3, ±7, +3, ±1, ±5, ±12 (line: 0, ±2, ±3, ±1, ±5, ±7) |
| Leap recovery (next move after a leap of 5+ st goes the other way) | 75% (line: 54%, where 19% is a step back and 35% repeats the landing note. Line melodies usually *land and sit*; arps zig-zag) |
| Note lengths | 8th 41%, 16th 27%, dotted 8th 18%, quarter 7%, longer 5% (line: 8th 45%, dotted 8th 26%, 16th 14%) |
| Notes per bar | median 5 (IQR 3-7); 6 is the modal count; 8 (straight 8ths) is 12% of bars; 16ths are 4% |
| Onsets off the beat | 62% of onsets fall off the quarter beat; 26% on the e/a 16ths; 17% are true anticipations, i.e. an off-beat onset held across the next beat (line: 21%) |
| Most-hit 16th positions | 0 (11%), **6** (10.6%), **14** (10.0%), 12 (9.6%), 4 (8.1%), 8 (7.8%), 2 (7.7%), 10 (6.9%). The "and" of 2 and the "and" of 4 are hit more often than beats 2 and 3 |
| Range | median 12 semitones (IQR 9-17); line melodies median 12 (IQR 7-15); arps median 19 |
| Pitch vocabulary | median 6 pitch classes per melody and 5 distinct pitches per 4-bar phrase; 74% use 6 or fewer pitch classes |
| Scale-degree share (all notes) | 1: 24%, 5: 19%, b3: 14%, 2: 10%, 4: 10%, b7: 9%, b6: 5.5%, 7: 2.6%, 3: 2.4%, 6: 2.2% |
| Downbeat (beat 1) degree | 1: 27%, 5: 16%, b3: 15%, 2: 11%, 4: 9%, b7: 7%, b6: 6% |
| Minor-type pitch usage | 70% fit natural minor exactly; 50% fit dorian (because they avoid the 6th entirely); b6 appears in 36% of melodies and natural 6 in 12%; the leading tone (natural 7) appears in only 14%; b2 in 4%. Pure minor pentatonic accounts for 13% |
| Phrase endings (last note of each 4-bar phrase) | 1: 27%, 5: 20%, 2: 13%, b7: 10%, b3: 9%. Final note of the section: 1 31%, 5 20%, b7 14%, 2 10% |
| Where the phrase-final note sits | 37% start on 16th #62 of the 4-bar phrase (the "and" of beat 4 in bar 4), 16% on #63, 12% on beat 4. Phrases usually end on an **anticipation** into the next downbeat rather than on a long note on beat 1 |
| Repetition (4-bar groups) | exact pitch+rhythm: bar 3 = bar 1 15.5%, bar 2 = bar 1 7%, bars 3-4 = bars 1-2 8%. Transposition-invariant: bar 3 = bar 1 29%. **Rhythm only: bar 3 = bar 1 63%, bars 3-4 = 1-2 50%, all 4 bars share one rhythm 33%** |
| 8-bar groups | bars 5-8 = 1-4 exactly 19%; bars 5-7 repeat with bar 8 varied 12%; same rhythm with new pitches 34% |
| Strict repeating period | 4-bar 28%, 2-bar 7%, 8-bar 6%, 1-bar 2%; 58% have no strict period (the pitch follows the chord changes while the rhythm repeats) |
| Top 1-bar rhythms | `x.x. x.x. x.x. x.x.` (straight 8ths, 7%); `x..x ..x. .x.. x..x` (dotted-8th chain 3-3-3-3-4, 6%); `x... .... .... ....` (one held note, 5%); 16ths (4%); `x..x ..x. .x.. x.x.`; `..x. .x.. x..x ..x.` (the same chain displaced by an 8th); `x... x... x... x...` |
| Top half-bar cells | `x..x ..x.` (3-3-2 tresillo, 14% line) ≈ `x.x. x.x.` (13%) > rest > `x... ....` > `.x.. x..x` > `..x. .x..` |
| Harmony under the melodies | Chord-root time: i 35%, bVI 17%, iv 9%, bVII 8%, bIII 7%, V 6%, v 5%. Common openings: i-iv-bIII-bVII, i-bVII-bVI-i, bVI-iv-i-bVI, i-bVI-iv-V |

## Qualitative observations: what the best ones share

1. **Natural minor with a small palette.** Almost every lead is aeolian, and most use only 5-6 pitch classes. Tonic, fifth and minor third carry about 57% of the notes and about 58% of the downbeats. Colour comes from **b6 and 2 over the bVI and iv chords**, not from chromaticism. The leading tone is rare (14%), so a generator should default to natural minor and treat the 6th and 7th as rare, chord-driven exceptions.
2. **Repetition of pitch is a feature.** A third of all moves are repeated notes (half of them in line melodies). The lead pulses on one note in a syncopated rhythm and moves only at a few points, often by a step or a 3rd. *Levels*, *For An Angel*, *Children* and *Full Focus* are clear examples.
3. **The rhythm is fixed and the pitches follow the harmony.** Bar 3 repeats bar 1's rhythm in 63% of 4-bar groups, but its exact pitches in only 15%. Over 8 bars the most common pattern is the same rhythm with new pitches (34%). The hook is really a *rhythmic cell* that gets re-voiced over i-bVI-iv-bVII style progressions, with the melody taking the nearest chord tone.
4. **Dotted-8th and tresillo syncopation.** The 3-3-2 cell (`x..x ..x.`) and the 3-3-3-3-4 dotted-8th chain (Strobe, Clockwork, Opus-type figures) are the signature. The "and" of 2 and the "and" of 4 are hit more often than beats 2 and 3. About one onset in six is an anticipation held over the beat.
5. **Phrases end on an anticipation, and are open more often than closed.** The last note of a 4-bar phrase usually starts on the final 8th or 16th of bar 4 and ties into the next downbeat. It lands on 1 or 5 about 45% of the time. It lands on 2, b7 or b3 about a third of the time, which leaves the loop open so it can cycle.
6. **Two archetypes:** (a) *line* leads span about an octave, with a 1:1 step-to-leap ratio, lots of repeated notes, and leaps that land and sit; (b) *arp/pedal* leads span about 1.5 octaves and alternate a pedal or root with moving upper notes. In the arps, 81% of leaps reverse direction immediately (a zig-zag), e.g. *Opus*, *Insomnia*, *Anjunabeach*, *Tuvan*. Both are 16th- or 8th-driven. Long sustained notes are rare (about 5% of notes are longer than a beat) and mostly mark phrase ends.
7. **Small interval vocabulary.** ±2, ±3, ±5, ±7 and ±12 account for almost all moves. Tritones (0.3%) and chromatic semitones outside the scale are essentially absent.
8. **Density.** Line leads have 4-8 notes per bar, typically 5-6. Bars with 1-3 notes (about 20%) are the held phrase ends and breaths. Dense 16th bars belong to the arp archetype.

The detailed tables follow. Section A is the core set. B is core, minor keys only. C is core, prog/melodic house and techno artists only. D is the MIDI pack (for contrast). E is the core "line" melodies. F is the core "arp/pedal" melodies. The last table is harmony. Per-melody numbers are in `per_melody_table.md`.


## How the generator uses it
- **The trainer learns separate models** for "line" melodies (65, minor/dorian) and "arp/pedal"
  figures (25):
  - interval-to-interval transitions
  - the note's position relative to the sounding chord's root, on strong and weak beats
  - phrase-final degrees
  - 178 real one-bar rhythms
  - bar-3 and bar-5 rhythm and pitch repetition rates
  - an anticipation rate
  - an 8-bar shape profile: repeat runs, range, distinct pitches, direction changes and leaps
    (p25, p50 and p75)
- **The composer builds each phrase in three steps:**
  1. It plans the rhythm the way the corpus does: bar 3 restates bar 1's rhythm and often not its
     pitches, and phrase endings land early and ring.
  2. It samples the notes from the learned tables, under the arrangement's hard constraints (no
     sustained semitone against the pedal or the chord).
  3. It keeps the best of about 1500 candidates. A candidate's score is its likelihood under the
     model, minus its distance from the corpus's shape profile.
- **The shape target is the more melodic half of the corpus.** Repeat runs are held between p25
  and p50, and distinct pitches and direction changes between p50 and p75. This follows the
  user's repeated rejection of static, stuttering lines.
- **Check:** over 40 sampled phrases the output averaged 4.6 notes per bar, 34% repeated notes,
  67% steps and 63% off-beat onsets, against the corpus's 5, 48% (line), about 70% and 62%.
