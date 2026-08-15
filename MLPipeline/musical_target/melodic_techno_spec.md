# Melodic Techno Musical Specification

Companion to `melodic_techno_research.md` (evidence/sources) and
`drum_target.json` / `bass_target.json` / `arrangement_target.json`
(machine-readable versions of the numbers below). This document defines the
target sound in measurable/programmable terms — no "groovy," no "emotional"
without a number attached.

Status: specification only. Nothing in `Source/Engine/DrumEngine.*` or
`Source/Engine/BassEngine.*` has been changed to produce this document.
Where a characteristic is **already implemented and measured**, that's
stated. Where it's a **real, sourced gap**, that's stated too — this is not
a claim that the current generator already does all of this.

---

## A. DRUMS

| Characteristic | Target value | Status | Source |
|---|---|---|---|
| Kick density | 4-on-the-floor, 100% on-beat | Implemented (`kKickRhythm.onBeatFraction == 1.0`) | Local corpus, 17 files |
| Kick decay | 300–500ms audible tail, body 60–80Hz | Not modeled in `DrumVoiceSynth` kick synthesis (real samples used when available — decay is whatever the sample has; the synth fallback's own envelope isn't tuned to this) | External (Myloops) |
| Clap/backbeat probability | 84.1% on-beat, dominant at steps 4/12 (backbeats 2&4), 2.63 hits/bar | Implemented (`kClapRhythm`) | Local corpus, 26 files |
| Hat subdivision | Groove (selective): offbeat-8th (steps 2/6/10/14) carry ~50% of probability mass at velocity 0.72–0.80; remaining 50% spread thinly (0.02–0.07 each) across the other 12 positions; 6.86 onsets/bar | Implemented (`kHatRhythm`, groove-filtered subset, previous milestone) | Local corpus, 33-file groove subset (of 94 raw) |
| Hat subdivision — CamelPhat-dense alternative | "Constant 16th-note hi-hats, offbeat emphasized" | **Not** the current target — documented as a real alternative interpretation, deliberately not chosen (see research §4) | External (EDMprod/Myloops CamelPhat guides) |
| Open-hat frequency | Near-absent early (establish-stage scale 0.10), ramps to a real presence by full-drop (scale 0.75) — a ~7.5x density ratio across the phrase | Implemented (`StageEnergy.hatOpen`, `DrumEngine.cpp`) | Local corpus (`kRideRhythm`) + design |
| Percussion density | ~4.78 hits/bar (accent-style subset), correlated -0.4488 with kick, -0.425 with clap, +0.4758 with hat | Implemented (`kPercRhythm`, `buildEstablishStage`) | Local corpus, 56 files |
| Syncopation | Bias toward weakest 16th of each beat (steps 3/7/11/15), layered multiplicatively on the measured base rate | Implemented (`applySyncopation`, `DrumEngine.cpp`) | Design, consistent with research |
| Repetition length | Bar-level: kick 100% identical (mod. phrase accent), clap 80.8%, hat-groove 66%, perc 57% adjacent-bar-identical. Section-level: 8/16/24/32-bar blocks (see Arrangement) | Bar-level implemented; section-level **not yet implemented** — current `DrumEngine` only knows a fixed 16-bar Drop | Local corpus + `.als` arrangement evidence |
| Variation frequency | 4-stage arc across 16 bars (establish/develop/increase/fullDrop), i.e. variation roughly every 4 bars within a Drop | Implemented | Design (previous milestone), consistent with "change subtly every 4–16 bars" guidance |
| Ghost-hit frequency | Hat-groove secondary hits average roughly half the velocity of pulse hits (0.30–0.41 vs 0.72–0.80) | Implemented (velocity table) | Local corpus |
| Swing | **8–12% swing on hats/percussion, kick and bass stay strictly quantized** | **Not implemented** — every hit lands on an exact 16th-note grid, zero timing offset | External (Beatportal ARTBAT guide, Studio Brootle) — real, sourced, currently-missing gap |
| Velocity hierarchy | Kick beat-1 (0.988) > other beats (0.85–0.86); hat pulse (~0.76) > ghost (~0.35); clap on-beat (~0.93) > ghost (~0.3–0.45) | Implemented | Local corpus |
| Hi-hat choke (open cut off by next closed hit) | Real 909-style technique | **Not implemented** — `DrumVoiceSynth` triggers each role's voice independently, no choke groups | External (Studio Brootle) |
| Ghost kick (very quiet off-16th kick hits, -15 to -20dB) | Described in multiple external guides | **Not confirmed in local corpus** — 17 real kick loop files measured 100% on-beat, zero off-beat onsets. Flagged as untested, not silently added. | External only — explicitly not corpus-backed |

## B. BASS

| Characteristic | Target value | Status | Source |
|---|---|---|---|
| Notes/bar | 6.52 (groove subset mean) | Implemented (`kBassGrooveRhythm.meanNotesPerBar`) | Local corpus, 20-file groove subset (of 55 raw) |
| Note lengths | Mean 1.32 sixteenth-note steps (short/punchy) | Implemented as "one attack per active step" (the melody-voice architecture retriggers every step, so effective note length is inherently ≤1 step — see `BassEngine.cpp` comments) | Local corpus |
| Root-note percentage | 74.7% (offset 0) | Implemented (`kBassPitchOffsets[0]`) | Local corpus, 856 notes |
| Interval distribution | -7 (2.8%), -5 (2.8%), -4 (3.6%), -2 (4.9%), +2 (2.3%), +3 (5.1%), +7 (3.7%) semitones from each file's own modal pitch | Implemented (`kBassPitchOffsets`, full measured table) | Local corpus |
| Syncopation / kick overlap | on-kick fraction 21.3%, measurably below the 25% uniform baseline | Implemented (`kBassKickCorrelation`, derived from measured `onKickFraction`) | Local corpus |
| Octave/register | Not directly measured as an absolute register (source files span many keys/octaves); relative movement kept narrow (mean pitch range 2.15 semitones) | Register itself deferred to the loaded Serum patch's own octave/transpose (see §F below) | Local corpus |
| Motif length | 4-bar block (`kBlockBars`), matching real evidence of both 1-bar-repeating and up-to-4-bar-varying patterns in the source MIDI | Implemented (`buildBassBlock`) | Local corpus (mixed 1-bar/4-bar evidence, no single dominant length) |
| Repetition | Bars 0–3 built once, ≥75% shared content bar-to-bar within the block | Implemented | Design, matching corpus |
| Variation | Bars 4–7 derive from bars 0–3 via `params.variation`-scaled touches, kick-avoidance re-checked per touch | Implemented (`deriveBassBlock`) | Design |
| Silence / rests | Real "negative space" between notes — 128-step array, only ~6.5×8=52 of 128 steps active at density 0.5 by design | Implemented | Local corpus |
| Sidechain-style rhythmic gap (sequencing) | Positions correlated away from kick | Implemented (see above) | Local + external corroboration |
| Sidechain-style ducking (mix/DSP) | Real gain-reduction envelope on the bass voice triggered by kick, ~90–140ms release at 122–124 BPM | **Not implemented** — no DSP sidechain exists anywhere in the plugin | External — real, sourced, scoped as a future audio-DSP phase, not sequencing |

## C. MELODY

No `MelodyEngine` exists yet. Numbers below are targets for Phase G, not
current state.

| Characteristic | Target value | Status | Source |
|---|---|---|---|
| Motif length | 2-bar phrases | Not implemented | External |
| Phrase structure | Call-and-response between 2-bar phrases | Not implemented | External |
| Note density | Simple 4-note motifs (scale degrees 1-3-5-7) | Not implemented | External |
| Repetition | High — "complexity from timbral evolution, not harmonic complexity" | Not implemented | External |
| Pitch range | Restrained (consistent with the bass's own narrow-range finding) | Not implemented | Design, by analogy |
| Interval size | Small, diatonic | Not implemented | External |
| Stutter/arpeggio gate | 50–75% gate length at 16th-note grid, 3 velocity tiers (110-120 strong / 85-95 off-beat / 65-75 fill) | Not implemented | External |
| Relationship to bass/root | Same key/root as bass and harmony (shared `MusicState`, see Phase C) | Not implemented — no shared state exists yet; melody/bass/drums are three independent code paths today | Design |
| Local reference corpus available | 99 real Melodic-Techno-labeled MIDI files (`PML Cercle - Diva Melodic Techno MIDI Files`), role-tagged: LD(21) BS(17) FX(13) DR(13) ATM(11) ARP(10) PD(9) STB(5) | Cataloged, not yet analyzed | Local corpus |

## D. HARMONY

| Characteristic | Target value | Status | Source |
|---|---|---|---|
| Scale | Natural minor | Design target; not implemented as generation logic | Local (PML285 Harmony slides) + external |
| Progression | `i–VII–VI–VI` family (e.g. Am–G–F–G — "dark, cyclical, hypnotic") as primary; `i–v–iv–VII` as an alternative | Not implemented | External |
| Chord changes per section | 3–4 per 32-bar section | Not implemented | External |
| Chord voicing | Diatonic triads/7ths (i, ii°, III, iv, v, VI, VII), inversions available | Not implemented | Local (PML285) |

## E. BREAKDOWN

See `melodic_techno_research.md` §3 for the full evidence. Measurable
structure (not "less drums"):

| Characteristic | Target value | Status |
|---|---|---|
| Pre-Break (thinning transition) | 4–8 bars, immediately before the full break | Not implemented — current generator has no section states at all |
| Break proper | 16–24 bars | Not implemented |
| Kick | Removed or reduced to a sparse pulse during Break | Not implemented |
| Bass | Reduced — real tracks show a distinct "Bass in"/"Drums in" REintroduction as its own 24-bar section, implying bass is largely absent or minimal during the break itself | Not implemented |
| Percussion | Reduced/removed | Not implemented |
| Melody/harmony | Exposed — becomes the foreground element | Not implemented (no melody engine yet) |
| Filtering | High-pass sweep across the break/build, low-pass automation on bassline specifically (Giordani technique) | Not implemented — no automation/DSP-parameter-over-time system exists |
| Reverb | Increased sends during breakdown ("vast, expanding space") | Not implemented |
| Secondary break variant | "Melodic Break" — ~8 bars, a shorter breather between later drops, not a full reset | Not implemented |

## F. BUILD

| Characteristic | Target value | Status |
|---|---|---|
| Length (into a later drop) | 4–8 bars (short) | Not implemented |
| Length (into the first drop) | 16–32 bars (longer — doing double duty introducing the track) | Not implemented |
| Kick | Gradually returns (real tracks show a distinct "element reintroduction" state) | Not implemented |
| Percussion | Increases | Not implemented |
| Tension technique | Filter automation (HPF sweep), percussion ramps, energy contrast (removing an element right before impact) | Not implemented |

## G. DROP

| Characteristic | Target value | Status |
|---|---|---|
| Length | 16–40 bars, most commonly 16/24/32 — not a fixed constant across drops in the same track | Current `DrumEngine`/`BassEngine` both hardcode a single 16-bar (drum) / 8-bar (bass) container per generation |
| Composition | Kick + bass + closed hat + selective open-hat/ride + clap + percussion + melodic hook, each with a stated purpose (see Relationships, §J) | Drums + bass implemented; melodic hook not implemented |
| Drop type | Melodic / Fancy / Percussion-driven / Call-and-response (see research §3.1) — different drops in the same track can be different types | Not modeled — current generator produces one undifferentiated drop shape |
| Mute-test identity (see §J) | Every role must remain musically coherent with any other single role muted | Design principle, not yet a test |

## H. ENERGY CURVE

Derived from the three real reference arrangements (research §3.2/3.3),
expressed as relative section weight, not absolute loudness (no waveform
loudness analysis was performed in this pass):

```
Intro (low, rising) → Full groove (established, no drop) → Drop 1 (subtle impact)
  → Element reintroduction (rising) → Pre-Break (falling) → Break (low)
  → Build (short, rising) → Drop 2 (bigger than Drop 1)
  → [optional short Melodic Break (brief dip)] → Drop 3 (comparable/bigger) → Outro (falling)
```

Not a smooth monotonic ramp — real drops are NOT strictly increasing in
length or intensity (Drop 3 in Track A is 32 bars vs Drop 2's 32 bars;
Track B's later drops are 24 bars vs. its first drop's 32). The energy curve
has real peaks and valleys, not a single climb to a maximum.

## I. SOUND SELECTION

Ties directly to §11 of the user's brief (confidence weighting) — see the
architecture proposal (Phase B) for the concrete design. Measurable target
per semantic role (examples, not exhaustive):

**DROP_BASS**: strong fundamental (40–100Hz sub layer per external guides),
controlled low end, short/medium decay, rhythmic articulation, mono/centered
below ~200Hz, enough harmonic content in the 150–800Hz range to translate on
small speakers.

**BREAKDOWN_BASS**: softer, filtered (the Giordani low-pass-during-breakdown
technique), longer sustain, less rhythmic density — could be the SAME
underlying sound as DROP_BASS with different filter automation, or a
genuinely different Serum patch; both are legitimate, to be decided at
design time (Phase F), not assumed here.

## J. RELATIONSHIPS BETWEEN PARTS

The user's explicit mute-test requirement, restated as a design constraint:

- **Mute bass** → drum groove (kick/clap/perc/hats) must still read as a
  complete, intentional pattern. Already true today: `DrumEngine.cpp`'s
  drum roles have no dependency on bass.
- **Mute hats** → kick/percussion groove must still make sense. Already true:
  percussion's correlation references are kick/clap/hatClosed/hatOpen, but
  removing hats doesn't remove the kick/clap/perc's own internal logic.
- **Mute percussion** → kick/bass relationship must still work. Already true
  structurally, though there's no melodic hook yet to also check against.
- **Mute melody** → groove works but loses identity. **Cannot be verified
  yet — no melody engine exists.**
- **Mute drums** → bass/melody must still imply the musical phrase. **Cannot
  be verified yet** — bass's own kick-avoidance logic doesn't depend on kick
  being audible, only on knowing where kick WOULD be, so this should already
  hold for bass alone; melody is unverified.

Measured cross-role relationships already driving generation:
`kickClap=+0.64, kickHat=+0.08, kickPerc=-0.45, clapHat=+0.13, clapPerc=-0.43,
hatPerc=+0.51, kickRide=-0.06, clapRide=+0.00, hatRide=+0.91 (very strong),
percRide=+0.34`, plus bass's derived `kBassKickCorrelation ≈ -0.15`.

---

## Appendix: Serum 2 preset identification (investigation, not implementation)

The user's brief asked directly whether the currently-loaded Serum 2 preset
can be identified and shown honestly in the UI. Inspected this session,
without changing any code:

1. **What's hosted**: `AbletonCopilotAudioProcessor::melodyVoices[trackIndex]`
   owns one independent `juce::AudioPluginInstance` per melody track, loaded
   from whatever `Serum2.vst3` is found under `/Library/Audio/Plug-Ins/VST3`
   (`SerumLoaderThread::run()`, `PluginProcessor.cpp`). Track 0 is always
   the default "Bass" voice, loaded automatically at construction.
2. **What preset is loaded right now**: whatever `Serum 2`'s own default
   init patch is, unless the user has used the existing Capture flow — no
   preset file is loaded automatically today. `getMelodyTrackStatus()`
   returns only `voice.statusMessage`, which is set to strings like `"Serum
   2 loaded"` — it has never carried a preset name, because there's never
   been one to carry.
3. **Can the loaded preset be identified programmatically?** Already tested
   in an earlier session and confirmed again by reading the resulting
   diagnostic logs on disk (`~/Library/AbletonCopilot/
   serum_program_list_debug.txt`, `serum_load_log.txt`): Serum 2 reports
   `getNumPrograms() == 128` with generic names (`"Prog 1"`, `"Prog 2"`,
   ...), and calling `setCurrentProgram(1)` does **not** change
   `getStateInformation()`'s output at all (`"changed state bytes? NO
   (identical - dead end)"`, logged across a dozen separate real runs). VST
   program-list access to Serum 2's actual preset browser is a confirmed
   dead end.
4. **Can a preset file be loaded directly?** No. Serum 2's real plugin state
   is a `"VC2!"`-tagged binary blob (not readable as text, not the same as
   the `.SerumPreset` or `.fxp` file formats found in the library) —
   confirmed via a byte-level diagnostic in an earlier session and recorded
   in `PresetLibraryScanner.h`'s own comments.
5. **The only proven-working mechanism**: the existing Capture flow
   (`captureMelodyTrackState`/`loadCapturedPreset`) — the user manually
   browses to a sound inside Serum 2's own GUI, clicks Capture, and the
   plugin grabs that live `"VC2!"` state and can replay it later. This is
   real and working, but requires a manual step per sound; nothing is
   captured on this machine yet.
6. **What the UI should honestly say**: since no preset name is
   recoverable from either the live plugin state or a `.SerumPreset` file,
   the honest minimum is `Bass: Serum 2 / Preset: Unknown (host state)` —
   plus, once a sound has been captured, whatever filename the user gave it
   at capture time (the existing capture UI already asks for a name via an
   `AlertWindow`, per `PluginEditor.cpp`'s `captureCurrentSound`). This is a
   concrete, small UI fix (show the captured file's name if one is loaded,
   else say "Unknown (host state)" instead of just "Serum 2 loaded") —
   scoped as part of Phase F below, not done in this pass.

**Library inventory** (`/Users/shawnshirazi/shawn music stuff`, found this
session): 366 `.SerumPreset`/`.fxp` files total across `Voltage Vol.2 for
Serum 2` (366 files, "BS -"/"LD -"/"Stab -"/"PL -" prefixes), `Odd Frequency
- Exo 2 - Serum 2 Presets` (55 files), `Odd Frequency - Grid 2 - Serum 2
Presets` (51 files), and PML Mirage/Mystique's own `Serum Presets` folders
(zipped `.fxp`, Serum-1-era format, paired 1:1 by name with real bassline
MIDI files — see research §5). None are programmatically loadable per point
4 above — this inventory exists so a future capture-library-building session
knows exactly which files to manually audition and capture first (the
`"BS -"`-prefixed ones), not because they can be loaded automatically.
