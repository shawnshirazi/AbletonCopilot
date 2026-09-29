# Progressive arp reference: "Synth Arp Loop Yearn" (D minor, 125 BPM)

The user supplied this loop as the melodic target for the progressive techno style
(`SongStyle::ProgressiveTechno`, `Source/Engine/ProgressiveGenerator.cpp`). The audio comes
from a commercial sample pack (file `BOS_IEPT_125_Synth_Arp_Loop_Yearn_Dm.wav`) and is **not**
stored in this repo. This file keeps only the analysis.

## Method
- The file is 15.36 s, stereo, 24-bit 44.1 kHz. That is exactly **8 bars at 125 BPM**.
- Tuning: `librosa.estimate_tuning` returned −0.5 of a semitone, and spectral peaks give about
  **−45 cents**. The loop is pitched flat, so a naive transcription reads every note as a split
  between two semitones (C#3/D3, E4/F4). The constant-Q transcription was re-run with
  `tuning=-0.45`.
- Each 16th step's note is the constant-Q bin whose energy rises most from just before that step
  to just after it.

## Transcription (16ths, tuning corrected)
```
bar1: D3 A3 D4 E4 F4 D3 A3 D5 E4 F4 D3 A3 D5 E4 F4 D3
bar2: A3 D4 E4 F4 D3 A3 D5 E4 F4 D3 A3 D5 E5 F4 D3 A3
bar3: D4 F4 G4 D3 A3 D5 E4 G4 D3 A3 D4 E4 G4 D3 A3 D5
bar4: E4 A4 D3 A3 D5 E4 A4 D3 A3 D5 F4 A4 E4 Bb2 F3 D4
bar5: E4 A4 Bb2 F3 D4 E5 A4 Bb2 F3 D5 E5 A4 Bb2 F3 D5 E4
bar6: A4 Bb2 F3 D5 E4 A4 Bb2 F3 D4 E4 A4 Bb2 F3 D4 E4 A4
bar7: E4 G2 D3 C5 D5 E5 A4 G2 D3 C4 E4 F4 A4 G2 D3 C4
bar8: E4 F4 A4 G2 D3 C4 D5 F4 A4 G2 D3 C4 E4 F4 A4 G2
```
A few isolated readings were off-scale (D#, A#3, G#). They sit where a neighbouring in-scale
note is expected, so they were treated as detection errors and corrected above.

## What makes it work
1. **A 5-step cell against a 16-step bar (polymeter).** The pitch sequence autocorrelates at
   0.54 at lag 5 and about 0 at lag 4. The low note lands on steps 0/5/10/15, then 4/9/14, then
   3/8/13, so the figure drifts across the bar lines.
2. **Wide, open voicings.** Each cell goes low root, 5th, octave, 9th, top note, spanning two
   octaves or more.
3. **Extended chords, 2 bars each.**

   | Bars | Cell | Chord |
   |---|---|---|
   | 1–2 | D3 A3 D4 E4 F4 | Dm(add9), i |
   | 3–4 | D3 A3 D4 E4 G4, then A4 | D sus pedal; the **top note climbs F, G, A** (the "yearn") |
   | 5–6 | Bb2 F3 D4 E4 A4 | Bbmaj7(#11), VI |
   | 7–8 | G2 D3 C4 E4 F4 A4 | G(11,13), iv/VII colour; **the cell grows to 6 notes** |

4. **Anticipation.** A cell takes the chord that sounds at its last note. The Bb cell starts 3
   steps before bar 5, and the climbing top note also arrives 3 steps early.
5. **Octave shimmer.** The octave note runs low, high, high (D4 D5 D5 …).

## Sound (measured)
| Measure | Value |
|---|---|
| Energy by band | 24% below 150 Hz, 52% 150–500 Hz, 21% 0.5–2 kHz, 2.2% 2–6 kHz, 0.2% above 6 kHz: warm and dark |
| Note envelope | a hold of about 50 ms, then a smooth fall of about 9 dB across the 16th, so notes blur together |
| Stereo width | side/mid RMS 0.48 (wide) |
| Brightness | the spectral centroid moves between 1.25 and 2.9 kHz from bar to bar (filter automation) |
| Echo | onset-envelope autocorrelation peaks at a dotted-8th (3/16) lag (0.96), which suggests a dotted-8th delay |

## How the generator matches it
- The progression "i-i(sus)-VI-iv" reproduces bars 1–4 note for note. The only differences are
  the transcription errors listed above.
- Bars 5–8 have the same notes and cell lengths, shifted by one 16th, because the loop adds one
  extra note just before each chord change (a human-timing detail).
- The generator's other progressions reuse the same cell shapes over other diatonic chords.
- `renderWarmArp` in `SongRenderer.cpp` is tuned to the sound profile above. The rendered
  Breakdown (arp + pad + sub) measures 19/56/24/1.4/0.3% across the same bands, with width 0.44.
