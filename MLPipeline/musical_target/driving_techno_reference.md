# Driving techno reference: Innēr Sense, "People Can Fly" (Spectrum, 2025)

The user chose this as the target for `SongStyle::DrivingTechno`
(`Source/Engine/DrivingTechnoGenerator.cpp`). Beatport lists it as Techno (Peak Time / Driving),
128 BPM, "A major", 6:06, released 2025-12-19 on Joris Voorn's label Spectrum. The user uploaded
a 4:07 edit, which was analysed here. The audio is a commercial release and is **not** stored in
this repo; only the analysis is.

## Method
- Beat tracking: a linear fit over the beats gives **127.996 BPM**; the downbeat is at 0.033 s.
  The edit is 132 bars.
- Arrangement: per 4-bar block, the RMS level, six energy bands, a kick score and the chroma.
- Groove: onset strength in four frequency bands, folded over a 16-step bar and averaged across
  the drops.
- Pitch: a harmonic/percussive split, then high-pass/low-pass filtering, then **basic-pitch**
  (Spotify's polyphonic transcription model, ONNX, run offline). Notes were quantised to the
  bar grid. Long-window FFTs measured the sub pitch.
- Source separation (Demucs) was not possible because the environment's network blocks the model
  download.

## Arrangement (the 4:07 edit)
| Bars | Section | Evidence |
|---|---|---|
| 1–28 | Intro/groove | 80% of the energy below 120 Hz; sparse tonic pulses; hook fragments from bar 22 |
| 29–32 | Break | the sub drops out (12%), the mids come up |
| 33–64 | Drop | full; the hook; from bar 49 the roots alternate every 4 bars |
| 65–80 | Breakdown | no kick, sub about 6%; **Am ↔ F**, 4 bars each; the pulse melody |
| 81–96 | Build | **Bb (bII)** at bars 82–84, then a chromatic climb; kick back for bars 85–92, then a 4-bar hole |
| 97–128 | Drop 2 | the hook an octave down, then the pulse melody inside the drop (C → E → F) |

## Groove (drop, folded 16-step bar)
- Kick on every beat.
- **The lowest band (the rumble) hits steps 0, 2 and 3 of every beat, never step 1.** That is
  kick, a gap, then bass on the 3rd and 4th 16ths: the classic rolling "k . b b".
- The high band peaks on the 8th offbeats (the open hat) and on 2 and 4 (the clap/snare top).
- The 300 Hz–1 kHz band has onsets on **every 16th except the beats**: a pumping, sidechained
  16th stab.

## Harmony and melody (A minor)
- **Key:** transcribed notes weight A, E, C most; the breakdown chroma is A/C/E then F/A/C/E.
  That is A minor. Beatport's "A major" is wrong.
- **Low end:** a rumble with energy spread across about 45–60 Hz and no single clean pitch.
- **Drop hook:** a sparse descending figure over the tonic pedal.

  | Bar | Notes (16th step) |
  |---|---|
  | A | F4 (1), E4 (2, held), D4 (5–7), A3 (8) |
  | B | pickup E4 (13), F4 (15) |

  That is **♭6–5–4–1**.
- **Breakdown pulse:** a repeated-note 16th pulse (a gated pluck) that changes pitch once per
  bar.

  | Bars | Chord | Pulse pitches |
  |---|---|---|
  | 73–76 | Am | C5, A4, A4, C5 |
  | 77–80 | F | B4→G4 (split bar), A4, A4, B4 |

  A is the tone both chords share. B over F is a **Lydian ♯11**, and G is the 9th.
- **Build:** the pulse goes C5, then A4, then B♭4 (over the ♭II chord), then climbs **one
  semitone per bar**: B4 C5 C#5 D5 D#5 E5 F5 F#5 G5 …, up to the octave at the drop.

## Measured sound (reference vs the generator's rendered preview, seed 1, A minor)
| Section | Source | RMS dB | 20–60 | 60–120 | 120–400 | 0.4–2k | 2–6k | 6–16k | Width |
|---|---|---|---|---|---|---|---|---|---|
| Drop | reference | −7.3 | 45.2 | 23.0 | 15.2 | 10.4 | 3.4 | 2.7 | 0.35 |
| Drop | generated | −7.8 | 46.9 | 17.6 | 16.4 | 11.0 | 2.7 | 3.3 | 0.22 |
| Breakdown | reference | −13.5 | 6.5 | 0.2 | 27.7 | 46.8 | 12.7 | 6.1 | 0.66 |
| Breakdown | generated | −11.5 | 12.2 | 0.0 | 38.0 | 43.4 | 4.7 | 1.6 | 0.38 |

Band columns are percent of energy. The generated breakdown is still narrower and darker than the
reference.

## How the generator uses it
Techniques, not notes:
- **Two-chord harmony**, 4 bars each: i–VI as in the reference, most often; i–VII, i–iv or i–III
  otherwise.
- **Hook:** one of five descending shapes, all ending on the tonic, and one of four rhythms,
  with an optional pickup.
- **Pulse melody:** built from the chords' shared tone, an upper neighbour for each chord (a
  triad tone on the home chord, a colour tone on the second chord for the lift) and a turn.
  There are three pattern variants.
- **Build:** the ♭II chord, then the chromatic climb.
- **Key:** random per seed unless one is given.
