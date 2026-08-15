# Melodic Techno Bass Grammar - Analysis Report

## Corpus
Real vendor-authored bass MIDI files from PML Mirage/Mystique and Odd Frequency Exo 2 (the same packs used for the drum grammar analysis).
- Total bass MIDI files found: 55
- Groove subset (used for BassRhythmGrammar.h): 20 files, packs: Odd Frequency Exo2, PML Mirage, PML Mystique
- Excluded as continuous/rolling (>11.0 notes/bar): 18 files
- Excluded as sustained/pad-style (long notes or too sparse): 17 files

## Groove-subset 16th-step hit probability (one bar, all groove files combined)
step:      0     1     2     3     4     5     6     7     8     9    10    11    12    13    14    15
prob:  0.099 0.062 0.072 0.106 0.046 0.023 0.140 0.053 0.043 0.050 0.058 0.054 0.025 0.043 0.099 0.026

on-kick fraction (steps 0/4/8/12): 21.3% (below the 25% uniform baseline - bass genuinely syncopates around the kick, doesn't double it)
mean notes/bar: 6.52
mean note length: 1.32 steps (short/punchy, not sustained)
mean pitch range: 2.1 semitones, mean unique pitches: 1.90 (mostly a repeated root note with occasional nearby passing tones, not a wandering melody)

## Pitch offset from each file's own modal (root) pitch
- +0 semitones: 74.7%
- +3 semitones: 5.1%
- -2 semitones: 4.9%
- +7 semitones: 3.7%
- -4 semitones: 3.6%
- -5 semitones: 2.8%
- -7 semitones: 2.8%
- +2 semitones: 2.3%