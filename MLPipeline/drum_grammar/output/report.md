# Melodic Techno Drum Grammar - Analysis Report

## Corpus
- **KICK**: 17 loop files, packs: PML Mirage, PML Mystique, 68 total onsets, 17 bars analyzed
- **CLAP**: 26 loop files, packs: PML Mirage, PML Mystique, 176 total onsets, 73 bars analyzed
- **HAT**: 94 loop files, packs: Odd Frequency Exo, Odd Frequency Exo2, PML Mirage, PML Mystique, 3058 total onsets, 240 bars analyzed
- **PERC**: 56 loop files, packs: Odd Frequency Exo, Odd Frequency Exo2, PML Mirage, PML Mystique, 1590 total onsets, 163 bars analyzed
- **TOP**: 48 loop files, packs: Odd Frequency Exo, Odd Frequency Exo2, PML Mystique, 1987 total onsets, 144 bars analyzed
- **RIDE**: 19 loop files, packs: Odd Frequency Exo, Odd Frequency Exo2, PML Mirage, 352 total onsets, 47 bars analyzed

## Per-role 16th-step hit probability (folded to one bar, all analyzed bars combined)

### KICK (n=17 files, 68 onsets)
step:     0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
prob:  0.25 0.00 0.00 0.00 0.25 0.00 0.00 0.00 0.25 0.00 0.00 0.00 0.25 0.00 0.00 0.00
vel:   0.99 0.00 0.00 0.00 0.86 0.00 0.00 0.00 0.85 0.00 0.00 0.00 0.86 0.00 0.00 0.00
on-beat (step%4==0): 100.0%   8th-offbeat (step%4==2): 0.0%   weak 16th (step%4 in 1,3): 0.0%
mean onsets/bar: 4.00

### CLAP (n=26 files, 176 onsets)
step:     0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
prob:  0.01 0.01 0.01 0.01 0.41 0.00 0.03 0.01 0.01 0.02 0.02 0.02 0.41 0.00 0.02 0.01
vel:   0.01 0.01 0.01 0.00 0.94 0.00 0.04 0.14 0.01 0.36 0.37 0.45 0.93 0.00 0.32 0.23
on-beat (step%4==0): 84.1%   8th-offbeat (step%4==2): 8.5%   weak 16th (step%4 in 1,3): 7.4%
adjacent bars byte-identical: 80.8% of 47 pairs
mean onsets/bar: 2.63

### HAT (n=94 files, 3058 onsets)
step:     0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
prob:  0.06 0.06 0.08 0.06 0.07 0.06 0.08 0.05 0.06 0.06 0.08 0.06 0.06 0.06 0.08 0.05
vel:   0.41 0.37 0.74 0.37 0.30 0.35 0.78 0.35 0.30 0.37 0.76 0.37 0.33 0.37 0.77 0.40
on-beat (step%4==0): 25.5%   8th-offbeat (step%4==2): 30.5%   weak 16th (step%4 in 1,3): 44.0%
adjacent bars byte-identical: 65.1% of 146 pairs
mean onsets/bar: 12.61

### PERC (n=56 files, 1590 onsets)
step:     0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
prob:  0.06 0.06 0.07 0.07 0.05 0.05 0.07 0.06 0.06 0.06 0.08 0.08 0.05 0.06 0.07 0.06
vel:   0.58 0.36 0.53 0.54 0.30 0.45 0.50 0.45 0.52 0.37 0.50 0.51 0.37 0.43 0.50 0.43
on-beat (step%4==0): 22.5%   8th-offbeat (step%4==2): 28.7%   weak 16th (step%4 in 1,3): 48.7%
adjacent bars byte-identical: 57.0% of 107 pairs
mean onsets/bar: 10.49

### TOP (n=48 files, 1987 onsets)
step:     0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
prob:  0.06 0.06 0.07 0.06 0.07 0.04 0.07 0.06 0.07 0.07 0.07 0.06 0.07 0.04 0.07 0.06
vel:   0.23 0.28 0.71 0.25 0.70 0.26 0.69 0.30 0.19 0.29 0.69 0.27 0.68 0.27 0.67 0.28
on-beat (step%4==0): 27.1%   8th-offbeat (step%4==2): 28.9%   weak 16th (step%4 in 1,3): 44.0%
adjacent bars byte-identical: 45.8% of 96 pairs
mean onsets/bar: 13.65

### RIDE (n=19 files, 352 onsets)
step:     0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
prob:  0.06 0.06 0.12 0.01 0.06 0.05 0.12 0.01 0.05 0.07 0.12 0.01 0.06 0.05 0.12 0.01
vel:   0.31 0.15 0.89 0.15 0.24 0.15 0.88 0.16 0.25 0.16 0.89 0.16 0.25 0.14 0.90 0.16
on-beat (step%4==0): 23.3%   8th-offbeat (step%4==2): 48.9%   weak 16th (step%4 in 1,3): 27.8%
adjacent bars byte-identical: 82.1% of 28 pairs
mean onsets/bar: 7.53

## Cross-role relationships

### Aggregate step-probability-vector correlation (corpus-wide, always available)
- KICK<->CLAP: r = 0.6391
- KICK<->HAT: r = 0.0758
- KICK<->PERC: r = -0.4488
- KICK<->RIDE: r = -0.062
- CLAP<->HAT: r = 0.1509
- CLAP<->PERC: r = -0.425
- CLAP<->RIDE: r = 0.0018
- HAT<->PERC: r = 0.4758
- HAT<->RIDE: r = 0.929
- PERC<->RIDE: r = 0.3379

### Direct co-occurrence on matched same-BPM/same-theme companion loops (n=40 matched pairs total)
- KICK->CLAP: 8 pairs, 32/32 of role B's onsets land on a role A step (100.0%)
- KICK->HAT: 18 pairs, 128/496 of role B's onsets land on a role A step (25.8%)
- KICK->PERC: 1 pairs, 9/33 of role B's onsets land on a role A step (27.3%)
- CLAP->HAT: 13 pairs, 50/374 of role B's onsets land on a role A step (13.4%)

## Sound fingerprints (one-shot samples, Phase 4)

### KICK (n=83)
- duration_ms: mean=493.575, stddev=185.957
- attack_ms: mean=14.15, stddev=16.623
- decay_ms_to_minus40db: mean=2.848, stddev=1.863
- peak: mean=0.915, stddev=0.086
- rms: mean=0.301, stddev=0.075
- spectral_centroid_hz: mean=205.179, stddev=278.316
- zero_crossing_rate_hz: mean=736.74, stddev=1557.704
- estimated_pitch_hz: mean=32.398, stddev=30.277
- sub_bass_fraction_20_100hz: mean=0.744, stddev=0.109

### CLAP (n=61)
- duration_ms: mean=525.989, stddev=249.142
- attack_ms: mean=10.812, stddev=12.316
- decay_ms_to_minus40db: mean=1.094, stddev=0.872
- peak: mean=0.87, stddev=0.093
- rms: mean=0.087, stddev=0.035
- spectral_centroid_hz: mean=4722.579, stddev=1215.538
- zero_crossing_rate_hz: mean=3150.432, stddev=2420.049
- estimated_pitch_hz: mean=0.0, stddev=0.0
- sub_bass_fraction_20_100hz: mean=0.002, stddev=0.003

### HAT (n=85)
- duration_ms: mean=353.639, stddev=253.443
- attack_ms: mean=5.448, stddev=5.333
- decay_ms_to_minus40db: mean=1.124, stddev=1.155
- peak: mean=0.84, stddev=0.144
- rms: mean=0.094, stddev=0.054
- spectral_centroid_hz: mean=9070.796, stddev=1450.53
- zero_crossing_rate_hz: mean=7202.553, stddev=3130.177
- estimated_pitch_hz: mean=6.492, stddev=34.122
- sub_bass_fraction_20_100hz: mean=0.0, stddev=0.0

### PERC (n=86)
- duration_ms: mean=534.889, stddev=308.194
- attack_ms: mean=11.364, stddev=23.946
- decay_ms_to_minus40db: mean=1.726, stddev=1.872
- peak: mean=0.784, stddev=0.195
- rms: mean=0.077, stddev=0.041
- spectral_centroid_hz: mean=3053.164, stddev=2106.133
- zero_crossing_rate_hz: mean=2048.114, stddev=2109.665
- estimated_pitch_hz: mean=77.981, stddev=111.846
- sub_bass_fraction_20_100hz: mean=0.024, stddev=0.074

### OPEN_HAT (n=56)
- duration_ms: mean=580.236, stddev=392.865
- attack_ms: mean=5.177, stddev=4.878
- decay_ms_to_minus40db: mean=0.987, stddev=0.939
- peak: mean=0.811, stddev=0.144
- rms: mean=0.089, stddev=0.035
- spectral_centroid_hz: mean=8260.986, stddev=1395.271
- zero_crossing_rate_hz: mean=5998.18, stddev=2008.427
- estimated_pitch_hz: mean=10.196, stddev=38.412
- sub_bass_fraction_20_100hz: mean=0.0, stddev=0.0

## Representative real patterns (ASCII, from actual analyzed files)

KICK - PML_MTM2_Kick_Loop_001_125BPM_F_Touch.wav (125.0 BPM, 1 bar(s)):
  K...K...K...K...

KICK - PML_MTM2_Kick_Loop_002_125BPM_F#_Believe.wav (125.0 BPM, 1 bar(s)):
  K...K...K...K...

CLAP - PML_MTM2_Clap_Loop_001_126BPM_Dream.wav (126.0 BPM, 2 bar(s)):
  ....C.......C...
  ....C.......C...

CLAP - PML_MTM2_Clap_Loop_002_122BPM_Ocean.wav (122.0 BPM, 2 bar(s)):
  ....C.......C...
  ....C.......C...

HAT - Odd Frequency - Exo - Hat Loop 1 (125 BPM).wav (125.0 BPM, 4 bar(s)):
  .HHHHHHHHHHHHHHH
  HHHHHHHHHHHHHHHH
  HHHHHHHHHHHHHHHH
  HHHHHHHHHHHHHHHH

HAT - Odd Frequency - Exo - Hat Loop 10 (125 BPM).wav (125.0 BPM, 2 bar(s)):
  HHH.HHH.HHH.HHH.
  HHH.HHH.HHH.HHH.

PERC - Odd Frequency - Exo - Perc Loop 1 (126 BPM).wav (126.0 BPM, 4 bar(s)):
  PPPPPPPPPPPPPPPP
  PPPPPPPPPPPPPPPP
  PPPPPPPPPPPPPPPP
  PPPPPPPPPPPPPPPP

PERC - Odd Frequency - Exo - Perc Loop 10 (126 BPM).wav (126.0 BPM, 2 bar(s)):
  P.PP.PPPPPPP.PPP
  PPPP.PPPPPPPPPPP