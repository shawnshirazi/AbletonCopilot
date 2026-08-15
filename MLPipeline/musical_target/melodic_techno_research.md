# Melodic Techno Research — Evidence Base

Status: research only. No `DrumEngine`/`BassEngine` code was touched to produce this
document. Every claim below is either (a) a direct quote/paraphrase from a named
external source, or (b) a number measured directly from files already in
`/Users/shawnshirazi/shawn music stuff` — never invented. Where sources conflict,
that's stated explicitly rather than silently picked.

This document exists to answer, with evidence, the questions the user asked
directly:

- What makes the target sound like melodic techno rather than generic techno?
- What makes the breakdown feel like melodic techno?
- What makes the drop feel like melodic techno?
- What makes the bass feel melodic techno?
- What makes the drums feel melodic techno?
- What role does repetition play? What role does variation play?
- How do bass and kick interact?
- How does the track transition from breakdown to drop?
- What should NOT be generated?

---

## 1. Reference artists — what they actually are

**AFTR:HRS / VER:WEST (Tiësto's alias).** AFTR:HRS Records was created by Tiësto in
2016 as a melodic-house/melodic-techno-focused offshoot of Musical Freedom. In 2021
Tiësto launched **VER:WEST**, an alter-ego specifically for "melodic techno and
house," described as showing "the darker side" with "a deep and brooding,
techno-influenced dance floor style." AFTR:HRS Sessions is a monthly 60-minute
mix series. [Sources: [Beatportal](https://www.beatportal.com/articles/465243-tiesto-drops-melodic-house-techno-infused-elements-of-a-new-life-via-his-verwest-alias), [WeRaveYou](https://weraveyou.com/2021/01/tiesto-alias-verwest-reveals-debut-episode-of-aftrhrs-sessions/), [Dancing Astronaut](https://dancingastronaut.com/2021/01/listen-to-tiestos-new-aftrhrs-sessions-mix-series-with-inaugural-episode-by-verwest/)]

**PRISMATIC.** Tiësto's newer radio show (first episode January 2026, aired on
Radio 538), featuring melodic house/techno and trance-adjacent material weekly.
Full tracklists exist per-episode on 1001Tracklists (not fetchable directly in
this session — blocked by a redirect loop — so specific track-by-track sonic
analysis wasn't possible here; treat "PRISMATIC" as confirming the **genre
placement** — melodic house & techno, not generic EDM — rather than as a source
of track-level audio measurements). [Source: [1001Tracklists episode index](https://www.1001tracklists.com/tracklist/2fwfrjwt/tiesto-prismatic-008-2026-02-21.html)]

**Layton Giordani.** Explicitly categorized by Production Music Live's own
Techno Academy (see §3 below) alongside Adam Beyer, Eli Brown, Amelie Lens,
Charlotte De Witte — i.e. **peak-time/driving techno, not melodic techno** by
this vendor's own genre taxonomy. His style: "expressive melodies, lyrical
synths, and dark, hypnotic techno rhythms," with "intricate arrangements and
unexpected musical textures" building tension and release. Signature technique:
**automating a low-pass filter on the bassline during breakdowns/build-ups**
to create tension and anticipation — this is the single most consistently
repeated concrete technique attributed to him across sources. Peak-time techno
in this style runs 126–132 BPM. [Sources: [Beatportal](https://www.beatportal.com/articles/783088-step-by-step-guide-to-producing-techno-peak-time-driving-in-the-style-of-layton-giordani-eli-brown-and-adam-beyer), [EDM.com](https://edm.com/music-releases/layton-giordani-better-than-u-thought/)]

**Consequence for the target:** the user's brief explicitly blends melodic
techno (AFTR:HRS/PRISMATIC direction) with Giordani's driving-techno DNA. These
are, per the vendor's own genre taxonomy below, two adjacent-but-distinct
buckets. The concrete, reusable thing to borrow from Giordani specifically is
the **filter-automation-driven breakdown/build technique**, not his BPM range
or his more aggressive/darker sound design — the target stays in melodic
techno's 120–125 BPM pocket (see §4), just with Giordani-style filter tension
building layered on top.

---

## 2. Genre boundaries — what melodic techno is NOT

| Dimension | Melodic Techno | Progressive House | Generic Techno | Tech House |
|---|---|---|---|---|
| Tempo | 120–125 BPM | 120–128 BPM | 125–135 BPM | 124–128 BPM |
| Melody | Central but subtle/restrained | Prominent, uplifting, anthemic | Minimal to none | Minimal (bassline plays the "melody") |
| Emotional tone | Melancholic, cinematic, dark/warm | Euphoric | Aggressive, industrial | Groovy, playful |
| Energy curve | Tension → Drop (defined peaks) | Continuous evolution, no sharp resets | Relentless, hypnotic repetition | Groove-driven, subtle changes |
| Drop | Central emotional payoff | Secondary/subtle | Percussive/rhythmic peak | Bass-driven, groove-focused |
| Breakdown | Percussion strips back, melody intensifies, then a defined drop | Gradual, no dramatic reset | Industrial soundscapes, drum filtering | Vocal/FX-focused, brief |
| Harmony | Present but restrained (minor-key, 2–4 chords) | Obvious, frequent | Rare — dark stabs, dissonance | Rare — a single stab/note |

[Sources: [Wikipedia — Melodic Techno](https://en.wikipedia.org/wiki/Melodic_Techno), [Doubleclap Radio](https://doubleclap.dance/melodic-techno-vs-progressive-house/), [Myloops production guide](https://www.myloops.net/melodic-techno-production-complete-guide-from-start-to-finish)]

**What this rules out explicitly (the user's own "do NOT" list, now backed by
sources):**
- Generic EDM "Pop Form" (Intro→Verse→Pre-Chorus→Chorus→Bridge→Drop→Outro) — a
  real, named, *different* structure documented in the user's own PML
  Arrangement Academy guide (§3), explicitly contrasted against the electronic-
  music form. Using it would make the output sound like a festival-EDM
  track, not melodic techno.
- Festival progressive house's anthemic, obvious-melody-forward drops.
- Generic techno's near-absence of melody/harmony.
- "Random procedural drums" — a flat/uniform probability model (which is
  exactly what the pre-fix `DrumRhythmGrammar.h` HAT data amounted to before
  the groove-density-filter fix from the previous milestone).

---

## 3. Arrangement — from the user's own library (primary evidence)

`/Users/shawnshirazi/shawn music stuff` contains **PML - Complete Arrangement
Academy (PML361)**, a full arrangement course whose "Guide to Electronic
Arrangements" PDF explicitly gives **separate, named arrangement guidelines
per genre** — Melodic Techno, Melodic House, Afro House, Techno, Tech House —
each with its own section-length ranges, sound characteristics, drop types,
and tension techniques. This is a *stronger* source than any web article
because it's vendor-authored, purchased, genre-disambiguated reference
material already in the corpus. Full text extracted via `pdftotext` (see
`/tmp/arrangement_guide.txt` from this session, or re-extract from the PDF at
`PML - Complete Arrangement Academy (PML361)/PML - Guide to Electronic
Arrangements (PDF)/PML - Guide to Electronic Arrangements.pdf`).

### 3.1 Melodic Techno arrangement guidelines (verbatim, from the PDF)

> "Melodic Techno focuses on progressive storytelling and atmospheric
> immersion. The energy curve is carefully modeled and creates a hypnotic and
> emotional journey. Melodic Techno thrives on contrasts — epic cinematic
> breakdowns with soaring atmospheres vs. deep, stabby drops with pulsing bass
> and hypnotic grooves. This balance creates its signature emotional impact."

Structure given (inspired by Afterlife, Anyma, Innellea, Colyn, Stephan Bodzin):
- Intro (8–16 bars): atmospheric pads, filtered synths, deep reverb tails
- Build-up (16–32 bars): gradual bass introduction, evolving arpeggios
- First Drop (32–64 bars): subtle impact with full bass and percussion groove
- Breakdown (64–96 bars): cinematic soundscapes, orchestral elements, wide
  stereo imaging
- Main Drop (96–128 bars): expansive peak, fully open leads, layered
  harmonics
- Outro (128+ bars): elements fade gradually, focusing on atmosphere

Sound characteristics: "Deep kicks, spacious percussions, delayed hi-hats" /
"Pulsating, rolling sub-bass" / "Wide, emotional pads, evolving synths,
arpeggiated leads" / "Ethereal vocal textures, long decay reverbs, cinematic
sound design."

Drop types named: **Melodic Drops** (expansive, full-bodied synth moments),
**Fancy drops** (deliberately different from everything before — new sound
design, high impact), **Percussion-Driven Drops** (minimal melodic content,
rhythm-focused), **Call and response drops** (one element "calls," another
"responds").

Tension techniques: gradual high-pass filtering + LFO modulation; delayed
snare fills / randomized rhythmic elements; reversed pads with long reverb
tails; **removing lead synths before a major impact moment**.

### 3.2 Three real, complete reference arrangements (extracted directly, not summarized)

The Arrangement Academy ships three actual Ableton Live projects built by the
vendor specifically to teach melodic-techno arrangement, each with an
accompanying rendered `.mp3`:

- `PML - Melodic Techno - 01 - Arrangement Analysis/PML - Melodic Techno -
  Arrangement Analysis.als` (124 BPM)
- `PML - Melodic Techno - 02 - Practical Session Track 1/PML - Melodic Techno
  - 004 - Final arrangement.als` (124 BPM)
- `PML - Melodic Techno - 03 - Practical Session Track 2/PML - Melodic Techno
  - 006 - Final arrangement.als` (125 BPM)

`.als` files are gzip-compressed XML. Each contains real Arrangement-view
**Locators** (the producer's own section markers) with exact bar positions.
These were decompressed and parsed directly in this session (`gunzip` +
regex over `<Locator>`/`<Time Value>`/`<Name Value>`) — this is not an
approximation or a transcription from listening, it's the literal authored
structure.

**Track A — "Arrangement Analysis" (124 BPM):**

| Bars | Length | Section |
|---|---|---|
| 1–16 | 16 | (opening, unmarked — precedes the first locator) |
| 17–32 | 16 | Loop Intro |
| 33–48 | 16 | Intro |
| 49–80 | 32 | Full Theme |
| 81–96 | 16 | **Drop 1** |
| 97–120 | 24 | Drums Reintroduction |
| 121–128 | 8 | Pre-Break |
| 129–144 | 16 | Break |
| 145–152 | 8 | Buildup |
| 153–184 | 32 | **Drop 2** |
| 185–192 | 8 | Melodic Break |
| 193–224 | 32 | **Drop 3** |
| 225–232 | 8 | Outro |
| 233–240 | 8 | Loop Outro |
| 241+ | — | End |

**Track B — "Practical Session Track 1" (124 BPM):**

| Bars | Length | Section |
|---|---|---|
| 1–32 | 32 | Intro |
| 33–48 | 16 | Break |
| 49–80 | 32 | **Drop** |
| 81–84 | 4 | Pre-Break |
| 85–108 | 24 | Break |
| 109–148 | 40 | **Drop** |
| 149–164 | 16 | Lead section |
| 165–188 | 24 | **Drop** |
| 189–204 | 16 | Outro |
| 205+ | — | Loop Outro |

**Track C — "Practical Session Track 2" (125 BPM):**

| Bars | Length | Section |
|---|---|---|
| 1–16 | 16 | Intro |
| 17–40 | 24 | Bass in |
| 41–64 | 24 | Break |
| 65–80 | 16 | **Drop** |
| 81–104 | 24 | Drums in |
| 105–112 | 8 | Pre-Break |
| 113–128 | 16 | Break |
| 129–152 | 24 | **Drop** |
| 153–160 | 8 | Outro |
| 161+ | — | Outro Loop |

### 3.3 What these three real tracks establish, combined

1. **Section lengths are consistently multiples of 8 bars** (4, 8, 16, 24, 32,
   40 all appear; the one 4-bar section is a "Pre-Break" micro-transition,
   never a full section). This is a real, load-bearing structural constraint
   for any bar-count-based generator, not an assumption.
2. **"Break" is not one monolithic event.** All three tracks show a **short
   transitional sub-section immediately before the main break** ("Pre-Break,"
   4–8 bars) — this is where the track is visibly *thinning out*, not yet
   fully stripped. The break proper is 16–24 bars.
3. **Elements re-enter as their own distinct, named, multi-bar sub-sections**
   — "Drums Reintroduction" (24 bars), "Drums in" (24 bars), "Bass in" (24
   bars) all appear as their own labeled blocks, not as an instant on/off
   switch. This directly supports a **per-role energy ramp**, not a single
   global energy scalar that flips every role at once — exactly the shape of
   the `MusicState` idea the user sketched in §9 of their brief.
4. **Builds are short.** The build immediately before a *later* drop
   ("Buildup," "Pre-Break") is only **4–8 bars** — much shorter than the
   generic-EDM 16–32-bar riser formula. The *first* build (before Drop 1) is
   longer (24–32 bars, "Full Theme"/"Bass in"+"Break"), because it's also
   doing the job of introducing the track, not just building tension into an
   already-established groove. **Concrete implication: build length should
   depend on which drop it's building into, not be a fixed constant.**
5. **Drops are 16–40 bars**, most commonly 16, 24, or 32 — never a single
   fixed length across a whole track. Later drops in Track A (Drop 3, 32
   bars) and Track B (final drop, 24 bars) are comparable in length to
   earlier ones — length doesn't have to monotonically grow, but the earlier
   drop is deliberately marked "subtle impact" per the PDF guide's own text.
6. Full track lengths (~208–248 bars ≈ 6.5–8 minutes at 124–125 BPM) match
   the "5–10 min" range in the general electronic-arrangement section of the
   same PDF, and match every external source's stated melodic-techno track
   length.

### 3.4 State machine implied by the evidence (not invented)

```
INTRO → (BASS/DRUMS GRADUALLY ENTER, still "intro"-weight)
  → FIRST GROOVE / FULL THEME (established groove, no drop yet)
  → DROP 1 ("subtle impact" — the PDF's own words)
  → ELEMENT REINTRODUCTION (a real, separate, multi-bar state — NOT part of the drop, NOT part of the break)
  → PRE-BREAK (short thinning-out transition, 4-8 bars)
  → BREAK (16-24 bars — melody/pads exposed, kick/bass/percussion reduced)
  → BUILD (short, 4-8 bars, when following an already-established groove)
  → DROP 2 (bigger than Drop 1)
  → [optionally: a second, SHORTER "Melodic Break" — 8 bars, a breather, not a full reset]
  → DROP 3 (comparable or bigger)
  → OUTRO
```

This validates the user's own sketch (INTRO → GROOVE → DEVELOPMENT →
BREAKDOWN → BUILD → DROP → DEVELOPMENT → BREAKDOWN → DROP) but adds two real,
evidenced refinements the sketch didn't have: (a) a distinct short
"element-reintroduction" state that isn't the same as the drop or the build,
and (b) breaks come in at least two real varieties (a full 16–24-bar Break
and a shorter ~8-bar "Melodic Break" breather) — not one uniform breakdown
template reused every time.

---

## 4. Drums — what makes them read as melodic techno specifically

Two independent production guides (Myloops "Melodic Techno Production:
Complete Guide," EDMprod's CamelPhat-style guide) and the local
`drum_grammar.json`/groove-filtered `kHatRhythm` analysis (previous
milestone) converge on the same shape, with one real tension worth stating
plainly:

- **Kick**: four-on-the-floor, but with real audible decay — "300–500ms of
  audible decay with real body around 60–80 Hz," long enough that "there is
  simply no room on the downbeat for a low note without turning the low end
  to mud." This is the *causal* reason the bassline sits off the beat (see
  §5) — not an arbitrary stylistic choice. [Myloops bassline guide]
- **Clap/snare**: beats 2 and 4, "short, snappy." Matches the corpus
  measurement directly: `kClapRhythm.onBeatFraction = 0.841`, dominant
  positions 4 and 12 (beats 2 and 4 in a 16-step bar), 2.63 hits/bar. This is
  a case where the local measurement and the external guide fully agree.
- **Hi-hats — the one real point of disagreement in the sources.** EDMprod's
  CamelPhat guide and the Myloops CamelPhat guide both describe "constant
  16th-note hi-hats, with the offbeat hit emphasized" — i.e. a *denser*
  reference point than what the previous milestone's groove-filtered corpus
  measurement produced (6.86 onsets/bar, selective, ~50% of probability mass
  concentrated on the four offbeat-8th positions). This is worth being
  explicit about rather than quietly picking a side: CamelPhat's specific
  hi-hat style is denser/rolling; the target described by the user (driving,
  hypnotic, but explicitly "not filling every available step," matching
  Layton Giordani's more spacious groove-techno feel and the deeper
  AFTR:HRS/VER:WEST direction) is the **sparser, selective** end of what
  "melodic techno hi-hats" can mean. The groove-filtered corpus data is kept
  as the primary target for this reason — it's also literally sourced from
  the same packs (PML Mirage/Mystique, Odd Frequency Exo/Exo2) already
  driving the rest of this generator, not an unrelated reference.
- **Percussion**: measured negative correlation with kick (-0.4488) and clap
  (-0.425), positive with hat (+0.4758) — i.e. percussion genuinely occupies
  negative space around the foundation, confirmed both by the corpus
  measurement (already implemented) and by every production guide's
  "call-and-response," "interlocking" language.
- **Swing — a real, currently-unimplemented gap.** Multiple independent
  sources give a specific, converging number: **8–12% swing on hats/
  percussion while the kick and bass stay on strict quantization**
  (Beatportal ARTBAT/Anyma-style guide: "Apply 8–12% swing to your hi-hats
  and percussion while keeping your kick and bass on strict, straight
  quantisation"; Studio Brootle's techno drum guide references "SP1200 16
  Swing-67 at 10%" as a concrete preset). `DrumEngine.cpp` currently places
  every hit on an exact 16th-note grid with zero timing offset — this is a
  real, measurable, currently-missing feature, not a subjective one.
- **Ghost kicks — a real technique NOT currently modeled, and NOT confirmed
  in the local corpus.** Multiple guides describe a very quiet
  (−15 to −20dB) secondary kick pattern on off-16th positions. This is
  real and specific, but the local KICK corpus (`kKickRhythm`) measured
  **100% on-beat placement with zero off-beat onsets** across 17 real
  loop files — i.e. the user's own library's kick *loops* don't exhibit
  this technique (it may be applied later, in full mixdowns, rather than
  baked into vendor kick loops). Flagged as an external, sourced,
  not-yet-corpus-confirmed technique — a candidate to test in a later
  phase, not to silently add now.
- **Hi-hat choke** — a real technique (open hat cut off by the next closed
  hit, from a 909-style engine) referenced in Studio Brootle's guide, not
  currently modeled in `DrumVoiceSynth`/`DrumEngine`'s independent-voice
  triggering. A real, concrete, currently-missing refinement for later.

## 5. Bass — what makes it feel melodic techno, and why the previous
   `BassEngine` work already agrees with outside sources

The previous milestone's `BassRhythmGrammar.h` (measured from real PML
Mirage/Mystique/Odd-Frequency-Exo2 vendor bassline MIDI, 20-file groove
subset) found: **74.7% root note**, real diatonic intervals when it does move
(-7/-5/-4/-2/+2/+3/+7 semitones), mean 6.52 notes/bar, mean note length 1.32
steps, and **on-kick fraction 21.3% — measurably below the 25% uniform
baseline**.

External sources, found independently in this research pass, **converge on
the same picture without having been asked to**:

> "Notes land on offbeats — 'the and of every beat' — creating a 4-note-per-
> bar skeleton that interlocks with four-on-the-floor kicks... Approximately
> 70% root note with remaining 30% handling harmonic movement... 4-bar
> template uses root repetition plus fifth (harmonic width), walkdowns
> toward chord changes using minor third or flat seventh... Keep range to
> one octave, staying mostly on root and fifth." [Myloops bassline guide]

This is close corroboration from an independent source: ~70–75% root
(measured 74.7%, guide says ~70%), fifth and third movement (measured
offsets include ±7 = fifth, -4/+3 ≈ third-family), offbeat placement
(measured 21.3% on-kick vs. guide's "avoid the downbeat entirely" framing).

**Sidechain / rhythmic gap — a real distinction to keep straight.** The
user's brief asks to "investigate sidechain-style rhythmic gaps even if
actual compressor sidechaining isn't implemented yet." Two separate things
exist here, and the current `BassEngine` only implements one of them:

1. **Sequencing-level kick avoidance** (which positions the bass is *allowed*
   to play on at all) — this is what `BassEngine.cpp`'s
   `kBassKickCorrelation` already does, derived directly from the measured
   `onKickFraction`.
2. **Mix-level sidechain ducking** (an actual gain-reduction envelope applied
   to the bass voice's audio, triggered by the kick, so that even a bass
   note that *does* overlap a kick attack gets pulled down in level for that
   instant) — this is a DSP/audio-processing feature, not a sequencing one,
   and **does not exist anywhere in the current codebase**. Sources give
   concrete numbers: "release 90–140ms at 122–124 BPM," "4:1–6:1 ratio,
   0.1–1ms attack, 150–200ms release... routes to bassline (3–5dB
   reduction)."

These are complementary, not redundant — real melodic techno productions use
both. This is scoped as a distinct future phase (audio-DSP, not
sequencing/grammar work) rather than folded into `BassEngine`.

## 6. Harmony

The user's own library contains `PML_-_Music_Theory_for_Melodic_House_&_
Techno_(PML285)`, whose "Harmony & Chord Progressions" slide deck (extracted
via `pdftotext` this session) establishes: natural-minor scale framework,
diatonic triads/7th chords by roman numeral (i–ii°–III–iv–v–VI–VII in a
minor key), inversions, and chord substitution. This is a generic
theory-fundamentals deck, not melodic-techno-specific — but it confirms the
**minor-key-diatonic-harmony framework** every external melodic-techno guide
also assumes.

External guides give concrete, reusable progressions, notably:

> "i–VII–VI–VII (Am–G–F–G): The workhorse. Dark, cyclical, hypnotic." /
> "i–v–IV–VII (Am–Em–F–G): More introspective." / "Limit to 3–4 chord
> changes per 32-bar section." [Beatportal ARTBAT/Anyma-style guide]

`i–VII–VI–VII` in particular is worth flagging: it's independently described
as "dark, cyclical, hypnotic" — three of the exact adjectives the user used
to describe the target sound.

## 7. Melody

No dedicated melody-analysis work exists yet in this codebase (no
`MelodyEngine`, and the existing prompt/critique-based melody path is hidden
behind `kShowExperimentalFeatures`). Real evidence available for a future
phase: the user's library contains `PML Cercle - Diva Melodic Techno Presets
+ MIDI V1.1/PML Cercle - Diva Melodic Techno MIDI Files/` — **99 real,
explicitly melodic-techno-labeled MIDI files**, role-tagged by filename
prefix: `LD` (lead, 21 files), `BS` (bass, 17), `FX` (13), `DR` (drone, 13),
`ATM` (atmosphere/pad, 11), `ARP` (arpeggio, 10), `PD` (pad, 9), `STB` (stab,
5). This has not been analyzed note-by-note in this pass (out of scope —
"do not implement everything at once") but is a real, ready-to-use corpus
for Phase G.

External guides give concrete numbers worth carrying forward: 2-bar phrase
motifs with call-and-response structure; simple 4-note motifs on scale
degrees 1-3-5-7; "complexity comes from timbral evolution, not harmonic
complexity"; arpeggios at 16th-note grid with 50–75% gate and 3 velocity
tiers (strong-beat 110–120, off-beat 85–95, fills 65–75); "introduce a small
variation every 4–8 bars."

## 8. What should NOT be generated (explicit)

- The generic-EDM "Pop Form" (Intro/Verse/Pre-Chorus/Chorus/Bridge/Drop/
  Outro) — a real, named, different structure than the electronic-music/
  melodic-techno form, per the user's own PML arrangement guide.
- A 16–32-bar riser-into-snare-roll build before *every* drop — the real
  local reference tracks show most builds between drops are 4–8 bars, not a
  long generic buildup.
- Constant, uniform hi-hat 16ths with no negative space — this is exactly
  the bug the previous milestone already found and fixed (the raw,
  un-filtered `HAT` corpus, 62% "roller" loops, produced exactly this
  failure mode).
- A breakdown that removes everything at once with no "Pre-Break" thinning
  transition, and no distinct "elements reintroduce" phase before the next
  drop — the real reference tracks show these as separate, multi-bar states.
- A bassline that plays a note on every kick, or that wanders melodically
  with wide pitch range — measured evidence (74.7% root, ~2.15 semitone mean
  range) says the opposite.
- Sidechain/ducking as pure sequencing (leaving positions empty) mistaken
  for the actual mix-level effect — they're different tools, both real,
  neither implemented yet at the mix level.

---

## 9. Corpus inventory and confidence tiers

`/Users/shawnshirazi/shawn music stuff` contains far more than the four packs
used so far (PML Mirage, PML Mystique, Odd Frequency Exo, Odd Frequency
Exo2). Full pack-level audio-file counts, measured this session
(`find <pack> -iname '*.wav' -o -iname '*.aif' ...`):

**Tier 1 — explicit Melodic Techno (highest confidence):**
- `PML - Melodic Techno - Sound Pack - Mirage (PML341)` — 706 files (already in use)
- `PML - Melodic Techno - Sound Pack - Mystique (PML354)` — 687 files (already in use)
- `Odd Frequency - Modern Melodic Techno Mega Bundle` — 1187 files (Exo/Exo2, already in use)
- `Ekko - Mirage Melodic House & Techno` — 329 files (not yet used)
- `PML Cercle - Diva Melodic Techno Presets + MIDI V1.1` — 99 MIDI files + Diva presets (not yet used)
- `PML - Complete Arrangement Academy (PML361)` — real melodic-techno arrangement/project reference (used for §3 above)
- `PML_-_Music_Theory_for_Melodic_House_&_Techno_(PML285)` — theory reference + melodic-techno MIDI packs (`PML - Melodic Techno - Visions Beyond`, `PML - Melodic Techno & House - Midi Pack`) (not yet used)
- `Voltage Vol.2 for Serum 2` — 366 Serum 2 presets, techno-branded, 35 "BS -" bass presets (inventoried, not loadable — see Appendix)

**Tier 2 — Melodic House & Techno / Progressive Techno (adjacent, compatible):**
- `Odd Frequency - Grid Mega Bundle` — 958 files (also ships its own Serum 2 bass presets)
- `Afterhours - Progressive & Tech` — 384 files
- `Progressive Techno - Sample Tools by Cr2` — 298 files
- `PML Tops & Atmo Loops Pack V1` — 119 files (general top-loop/atmosphere material, genre-compatible)
- `Diva Cercle Sounds - Bonus Loops` — 51 files

**Tier 3 — driving/dark techno (Giordani-adjacent DNA, not melodic-labeled):**
- `PML - Dark Techno Sample Pack 2` — 396 files
- `Toolroom - Essential Techno Vol. 4` — 418 files
- `Acid Techno` — 231 files

**Tier 4 — explicitly deprioritize (different genre, per the user's own
instruction):**
- `PML - Complete Tech House Start to Finish Academy (PML338)` — 1124 files
  (Tech House is explicitly a *different* genre per the same PML arrangement
  guide's own taxonomy — "bassline plays the melody... stays on one note,"
  "avoids chords" — the opposite of what's wanted here)
- `Odd Frequency - Tech House Mega Bundle` — 1010 files
- `Secrets-Tech-House-Vol.2` / `Ekko-Secrets-Tech-House` — 629 + 471 files
- `THM - Heatwave - Tech House Vocals` — 248 files (vocal-centric, off-target)
- `TechHouse` — 228 files
- `Speed Vocal Bass House` — 91 files
- `Future Rave` — 824 files (a different EDM subgenre — explicitly what the
  user said not to target)
- `Trance` — 415 files (different genre)
- `KICK & BASS - ALKEMIST VOL.1` — 3289 files (largest single pack in the
  library by file count; naming convention and scale is consistent with a
  trap/bass-music kit, not melodic techno — needs a closer look before any
  future phase touches it, but starts in this tier)
- `FL studio/Image-Line` — 2621 files (FL Studio's own generic bundled
  factory content, not genre-specific at all)
- `TPS - Selection` / `TPS - Savannah` / `TPS - Spectrum Full Bundle` — 964 +
  366 + 88 files (unclear branding/genre identity, treat cautiously as
  unweighted/generic until inspected further)

This tiering is proposed evidence for the confidence-weighting system in
Phase B of the architecture proposal — **not yet implemented anywhere**. The
current `RackClassification`/`DrumSampleIndex`/`DrumSampleSelector` pipeline
has no concept of pack-level genre confidence at all today; it scores
individual samples by measured audio features and by functional-role
fingerprint distance, blind to which pack a file came from.

---

## 10. Reference-analysis pass 2 — Serum2 diagnosis, clip-level breakdown evidence, Breakdown Grammar

Status: research only, per explicit instruction — no `DrumEngine.cpp`/
`BassEngine.cpp` musical logic was touched to produce this section (a small,
disclosed diagnostic/status-panel change in `PluginEditor.cpp` was made
alongside it — see §10.1, not a generation change). Every claim below is
labeled **[LIBRARY]** (measured directly from files in `/Users/shawnshirazi/
shawn music stuff`), **[WEB]** (a named external source), **[CODE]** (traced
directly from this repository's own source), or **[INTERPRETATION]** (a
judgment call, flagged as such) — never asserted as fact without one of
these tags.

### 10.1 Part 1 — What Serum 2 is actually playing **[CODE]**

Traced directly, not assumed, by reading `PluginProcessor.cpp`'s Serum
loader and trigger paths:

- **Which instance loads**: confirmed via the real, dated
  `~/Library/AbletonCopilot/serum_load_log.txt` —
  `createInstanceFromDescription("Serum 2")` (not "Serum 2 FX") succeeds for
  both track 0 (Bass) and track 1 (Melody).
- **What state it's in immediately after construction**: `PluginProcessor.
  cpp`'s loader callback only ever calls `inst->setStateInformation(...)` when
  `voice.pendingState.getSize() > 0`. `pendingState` is populated in exactly
  two places: `setStateInformation` (the HOST restoring a previously-saved
  Ableton project) and `loadCapturedPreset` (a direct result of the user
  clicking Capture or cycling `</>` in the UI). **Neither has ever happened
  in this environment** — `~/Library/AbletonCopilot/CapturedPresets/` does
  not exist on disk. **Conclusion: every note heard so far has been played
  by Serum 2's own built-in factory "Init" patch, not any Melodic Techno
  preset and not any captured state.** This was previously suspected but not
  proven; it is now proven by code trace + a real absence-of-file check, not
  inferred.
- **Does Generate Bass change Serum's state?** No — confirmed by reading
  every call site of `setStateInformation`/`loadCapturedPreset`; nothing in
  `generateLoopClicked`/`applyRenderMode` touches Serum2 state, only MIDI
  note data.
- **Can Serum 2's own preset library be selected by index/name via the host
  API?** No — re-confirmed, not re-tested, via the existing dated
  `serum_program_list_debug.txt` diagnostic already in this repo:
  `getNumPrograms()` returns a dummy list ("Prog 1".."Prog 128"), and
  `setCurrentProgram(1)` provably does not change `getStateInformation()`'s
  bytes ("dead end"). Capture (real VST3 state bytes, round-tripped exactly)
  remains the only legitimate mechanism — this conclusion is unchanged, just
  re-verified against real evidence rather than assumed to still be true.
- **Where the octave jump actually comes from** — traced through the exact
  pitch computation in `PluginProcessor.cpp`'s melody-voice trigger loop:
  `pitch = jlimit(0, 127, 36 + keyRoot + offset)`. `keyRoot` is
  `getSelectedKey()`'s root semitone, confirmed bounded to **0–11** (`(selId
  - 1) / 2` over a 24-item major/minor picker). `offset` is bounded to
  **-7..+7** semitones — confirmed by reading `BassRhythmGrammar.h`'s
  `kBassPitchOffsets` table directly (root 0, then ±2/±3/±4/±5/±7, never
  ±12) and `BassArchetype.cpp`'s five templates (root, +3, or +7 only,
  matching the same measured table). **Neither the archetype system nor the
  measured interval table can produce a full octave (±12) jump on its own.**
  The real mechanism is different: because `keyRoot` and `offset` both add
  into the SAME fixed base (36), the bass's absolute register drifts by as
  much as `11 + 7 = 18` semitones (1.5 octaves) purely from KEY CHOICE
  interacting with interval choice — e.g. key=C (root 0) keeps the bass
  between MIDI 29–43 (F1–G2), while key=B (root 11) pushes it to 36–54
  (C2–F#3) for the exact same archetype and seed. **This is a real,
  provable register-drift bug, not a generation-randomness issue**: nothing
  currently clamps the bass to a fixed target octave independent of key.
- **Instrumentation added this pass** (`PluginEditor.cpp`, `applyRenderMode`
  - a status-panel change, not a generation change): the Bass/Melody status
  labels now say `"Serum 2 preset: factory Init patch (no capture performed
  yet - ...)"` instead of a vaguer "(no captured sound)", and a new **"Bass
  MIDI range: <low> - <high>"** label computes the real min/max MIDI note of
  whatever was just generated, using `juce::MidiMessage::getMidiNoteName`
  (real output, not estimated) via the exact same `36 + keyRoot + offset`
  formula the audio-thread trigger loop uses.
- **Real preset candidates on disk** (re-confirmed from the previous pass,
  not re-searched): 370 `.fxp` files in PML Mirage alone, real bass-role
  names like `PML BS Rolling Close.fxp`, `PML BS Sub Particles.fxp`, `PML BS
  Reese Fall.fxp`, `PML BS Main Lake.fxp`; Odd Frequency Exo2 adds `BS -
  Artion.fxp`, `BS - Flow.fxp`. `.fxp` is a VST2 chunk format Serum 2 (VST3)
  cannot load through the host API (established in a previous session's
  investigation) — these remain real, named, `Capture`-workflow suggestions,
  never auto-loaded, never claimed as active unless actually captured.

### 10.2 Part 3/4 — Clip-level breakdown evidence from the user's own reference arrangements **[LIBRARY]**

Section 3.2-3.4 (above) already extracted and tabulated the real Locator
structure of the three complete `.als` reference arrangements in `PML -
Complete Arrangement Academy (PML361)`. This pass goes one level deeper:
parsing individual **track clip start/end times** within the "Arrangement
Analysis" project (124 BPM) to see what's actually SOUNDING during each
labeled section, not just the section names.

**Real track roster found in this one project** (53 tracks, grouped) - a
genuine vocabulary of what a real melodic techno arrangement actually
contains, useful evidence on its own for Part 5/6:
`KICK/BASS` (Kick, Tom) · `BASSLINE & STABS` (Sub Bass) · `BASS` (Bass Stab,
Percussion Element) · `BASSY LEAD` (Side Bassy Lead, Pluck, Side Bassy Lead
Layer) · `DRUMS` (Woodblock, Ride, Ride 2, Stick, Fast Hat Bright, Hat, Top)
· `SHAKER GROUP` (Shaker 1, Shaker 2, Shakers) · `INSTRUMENTS` (Synth, Main
Loop Synth ×2) · `PADS/SYNTHS` (**Pad Break**, Serum Lead Atmos, Pad Fast
Tremolo, Pad, Pad 2) · `VOCALS` · `FX` (Long Noise Transition, Noise
Transition Down, Noise Fx, Fx 1-4, Reverse Cymbal, FX Cymbal, Ambience).

**Clip-level activity around the Pre-Break(120)→Break(128)→Buildup(144)→
Drop2(152) region:**

| Track | Active bars | Relative to sections |
|---|---|---|
| **Kick** | ...112–118, **120–128**, *silent 128–152*, 152–160... | Kick plays through Pre-Break, then is fully silent for the ENTIRE Break **and** Buildup (24 bars) - does not return early into the Buildup the way generic EDM formulas assume. Same clip data also shows Kick silent 182–192 - the "Melodic Break" section too. |
| **Hat / Fast Hat Bright** | ...112–120, *silent 120–144*, **144–151**, 152–184... | Hats are silent through Pre-Break **and** Break, then are exactly what comes back at the start of Buildup (bar 144) - hats mark the buildup, not the break. |
| **Pad Break** | *silent elsewhere*, **184–192 only** | A track literally named "Pad Break" has exactly ONE clip in the whole ~240-bar arrangement, and it exactly matches the "Melodic Break" locator (184-192). This is the single clearest, most literal piece of evidence in this whole investigation for "what replaces the kick's energy in a breakdown": a dedicated atmospheric pad, gated to play *only* during the break. |
| Sub Bass / Bass Stab | one continuous clip spans nearly the whole track (bar 48-232/240) | **[INTERPRETATION - inconclusive]**: clip-level data alone can't distinguish "still playing notes, just filtered/automated" from "silent within the clip" - a note-level query this session returned a note count too dense to trust without further validation (see caveat below). No volume/filter automation envelope was found directly on either track. Whether the sub bass is sequenced-silent or filtered-quiet during the Break is **not yet verified** and should not be asserted either way. |

**Caveat, stated plainly**: the Sub Bass/Bass Stab note-level query in this
session returned an implausibly high note count (3001 events for an ~184-bar
part), which does not match a real bassline and was not resolved before
time ran out on this pass - flagged as **not yet verified**, not silently
treated as either "bass continues" or "bass is silent."

### 10.3 Part 2 — External research (bounded, supplementary to §10.2's stronger library evidence) **[WEB]**

Per the user's own instruction to prioritize library evidence, this was a
bounded pass (a handful of searches), not exhaustive - no stems or audio of
copyrighted commercial tracks are accessible to this session by any
legitimate means, so external evidence here is structural/technique-level,
corroborating rather than leading:

- Layton Giordani's "Dragonfly" breakdown: **cuts the low end, introduces
  other synths/melodic lines, and filters out the low end during a 16-bar
  section while the melody plays more frequently** [Beatportal-adjacent
  production coverage] - independently matches this session's own measured
  16-24 bar "Break" length and the kick/bass-reduction pattern in §10.2,
  found without being asked to match it.
- Real Beatport data for Giordani's own catalogue: peak-time/driving techno
  releases run 129-135 BPM, while tracks tagged "Melodic House & Techno"
  specifically (e.g. "Alto" w/ Adam Beyer) sit at 129 BPM - slightly above
  the 120-125 pocket this document's §2 previously assumed as a hard
  ceiling. **[INTERPRETATION]**: treat 120-129 BPM as the real target range,
  not 120-125.
- Reverb-tail automation: **reverb send increased +3 to +6dB over 16-32
  bars** during a breakdown is a named, sourced technique (independent
  production guide) - concrete enough to implement later as a real DSP
  automation curve, not vague "add reverb" advice.
- Breakdown structure independently described elsewhere as **"3x 8-bar
  sections"** - consistent with this session's own measured 16-24 bar Break
  lengths (2-3 concatenated 8-bar units), not contradicting it.
- AFTR:HRS/VER:WEST specific track-level BPM/key data was **not obtainable**
  through this session's search access - Beatport/1001Tracklists list
  releases but didn't surface individual technical metadata for this
  specific alias in the time available. This is a real, disclosed gap, not
  papered over.

### 10.4 Breakdown Grammar - deriving explicit MusicState phases from §10.2's evidence

The existing `MusicSection` enum (`Source/Engine/MusicState.h`) already has
real, evidence-derived section names (`PreDrop`, `Breakdown`,
`BreakdownBuild`, ...) from the *first* research pass - **this is closer to
what the user is asking for in Part 4 than a fresh design would be**. What's
missing, per §10.2's NEW clip-level evidence, is encoding WHAT HAPPENS in
each phase as distinct, named sub-behaviour rather than one
`drumEnergy`/`bassEnergy` scalar per bar:

```
Breakdown Entry   (= "Pre-Break", measured 4-8 bars)
  kick: fading out (still present at entry, gone by the end - §10.2's Kick
        clip data shows it present through bar 120-128, i.e. still active
        for the FIRST few bars of what's labeled Pre-Break, then gone)
  hats: already reduced/gone (measured: hats were already silent by 120)
  bass: [not yet verified - see §10.2 caveat]

Breakdown Body    (= "Break", measured 16-24 bars)
  kick: OFF (measured: zero Kick clip coverage for the full 128-144 span)
  hats: OFF (measured: zero Hat coverage for the same span)
  melody/pad: EXPOSED - a real, dedicated "Pad Break" track activates
              (measured: Pad Break's only clip in the whole project is
              exactly this section)

Breakdown Development / Pre-drop (= "Buildup", measured 4-8 bars)
  kick: still OFF (measured: Kick doesn't return until Drop, not Buildup)
  hats: RETURN here specifically (measured: Fast Hat Bright's first
        post-break clip starts exactly at bar 144, the Buildup boundary)
  tension: this is where §10.3's reverb-tail/filter-automation technique
           would apply (not yet implemented - a DSP feature, not sequencing)

Drop              (kick + bass + full hat/perc groove all return together,
                    matching the existing DROP/BREAKDOWN RenderedLoop model)
```

**Proposed architecture (not yet implemented, for the next pass)**: extend
`MusicState`/`RenderMode` with a third value or a `BreakdownPhase` enum
{Entry, Body, Development} instead of one boolean, so hats and kick can
re-enter at DIFFERENT bars (hats at the Development boundary, kick only at
the Drop) - directly implementing what §10.2 measured, not inventing a new
shape. This is a bigger change than the current two-mode
(Drop/Breakdown) `RenderMode` and needs its own design pass.

### 10.5 Parts 5/6/7 - preliminary findings tied to this evidence (not yet implemented)

- **Percussion vocabulary [LIBRARY]**: the real track roster in §10.2
  confirms Woodblock, Ride (×2), Stick, Shaker (×2 MIDI + 1 audio), "Top" as
  real, currently-used melodic-techno percussion voices - broader than this
  plugin's current PercA/PercB two-voice model. A future pass could
  cross-reference this vocabulary against the user's own pack contents
  (Odd Frequency/PML PERC-classified files) to see how many of these
  specific timbres are actually available.
- **Bass register [CODE + INTERPRETATION]**: §10.1's register-drift finding
  is the highest-confidence, most actionable finding in this whole pass.
  Proposed fix direction (not implemented): clamp the bass's absolute
  register to a fixed target octave (e.g. always resolve `offset` around a
  FIXED anchor pitch class near the root, then transpose the whole pattern
  by octaves - not semitones - to land in a chosen target range like
  C1-C2), so key selection changes which NOTE plays, never which OCTAVE the
  part sits in, unless an archetype explicitly calls for a real octave
  displacement (none currently do - all 5 archetypes stay within ±7
  semitones, confirmed in §10.1).
- **Sample selection [unchanged]**: `DrumSamplePackTier` (previous pass) is
  the general, non-hardcoded genre-confidence mechanism the user is asking
  for in Part 6 - already built, already tested (46/46). No new evidence
  this pass suggests changing it; re-verified as still the right mechanism,
  not re-implemented.

### 10.6 Proposed single cohesive target (Part 8) - for review, not yet built

Per the user's "one convincing style, not five mediocre ones": the
evidence in this section converges on one real, specific, repeatedly-
measured shape, not an invented one:

- **124 BPM** (measured: all three reference tracks are 124-125 BPM; matches
  §10.3's Giordani "Melodic House & Techno"-tagged data at 129 BPM as the
  upper edge of a real range, not a hard ceiling)
- **16-bar Drop** (already the current loop-generator's length)
- **Breakdown Entry (~8 bars) → Break (~16 bars) → Development (~8 bars)**
  as three distinct phases, not one Breakdown boolean (§10.4)
- Kick: silent for the full Break+Development, returns only at the Drop
  (measured, §10.2 - corrects this codebase's current Breakdown design,
  which already mutes the kick for the whole Breakdown render mode, so this
  is actually already consistent - no change needed here)
- Hats: silent through Entry+Break, return specifically at Development
  start (measured, §10.2 - NOT currently implemented; the current
  Breakdown render keeps hatClosed at reduced gain throughout rather than
  fully silent-then-returning)
- A dedicated atmospheric pad exposed specifically during the Break
  (measured, §10.2 - not yet built anywhere in this codebase; melody
  generation is explicitly out of scope again this pass per the user's own
  instruction)
- Bass register clamped to a fixed target octave regardless of key
  (§10.1/9.5 - the highest-confidence actionable fix from this whole pass)

**Explicitly not decided by this report**: exact target register bounds
(needs a specific octave choice, e.g. "C1-C2" vs "C2-C3" - a musical
interpretation, not something the evidence gathered this pass pins down to
one exact answer), the exact `BreakdownPhase` API shape, and whether
percussion vocabulary expansion belongs in this phase or a later one.

---

## 11. Reference-analysis pass 3 — cross-validated Breakdown Grammar (final proposal), bass register, Serum 2 workflow

Status: **research and specification only** — no engine, processor, or UI
code was changed to produce this section. This directly continues §10 by
doing the two things §10 explicitly left undone: (a) checking §10.2's
single-instance clip-level findings against the **other two** real
reference arrangements instead of generalizing from one song, and (b)
pulling **absolute** MIDI register data (not just relative offsets) for the
bass-register question. One finding in §10.4 is **corrected** below based on
this wider evidence — flagged explicitly, not quietly folded in.

### 11.1 Cross-validating §10.2 against Tracks B and C **[LIBRARY]**

§10.2 measured clip-level kick/hat/pad behavior from exactly one reference
arrangement ("Arrangement Analysis"). This pass parsed the same kind of
clip-level data from the other two real, complete `.als` projects in the
same course (`Practical Session Track 1`, 124 BPM; `Practical Session Track
2`, 125 BPM), specifically around each track's **second** Break→Drop
transition (the break between an already-established Drop 1 and Drop 2 —
see §11.2 for why "second break" matters).

**Practical Session Track 1** (Pre-Break 81-84, Break 85-108, Drop2 at 109):

| Track | Measured clip activity | Reading |
|---|---|---|
| Kick | ON through 81-84, OFF 85-104 (20 bars), **ON again 105-108** | Kick returns 4 bars *before* the Drop, not exactly at it |
| Bassline | ON through 81-92, OFF 93-108 (16 bars), returns **exactly at Drop (109)** | Bass does NOT return early — waits for the downbeat |
| Hat / Hats / Off HiHat | OFF 85-116 (32 bars — the whole Break **plus** the first 8 bars of Drop2) | Full hat groove returns *8 bars into the Drop*, not before it |
| Strings / Low Lead Trem | silent elsewhere, **ON exactly 93-109** | A sustained string/lead pad fills exactly the window where bass goes silent, through into the drop's first bar |

**Practical Session Track 2** (Pre-Break 105-112, Break 113-128, Drop2 at 129):

| Track | Measured clip activity | Reading |
|---|---|---|
| Kick | OFF 105-120 (16 bars), **ON again 121-128** | Kick returns 8 bars *before* the Drop — same direction as Track 1, different exact offset |
| Sub Bass | OFF 105-128 (24 bars, the entire Pre-Break+Break), returns **exactly at Drop (129)** | Core/sub bass again waits for the exact downbeat — consistent with Track 1 |
| Bass Pluck (secondary layer) | OFF 105-120, **returns 121** (same bar as kick) | A *secondary* bass layer can return early alongside kick, even while Sub Bass stays out |
| Rolling Bass 1 / Rolling Bass 2 | **never leaves** — continuous 65-169 through the whole Break | A persistent "rolling" texture layer can act as a groove anchor that simply never drops out, distinct from the sub-bass role |
| Rolling Hat / Hat 2 / Ride | OFF 113-137ish (Break **plus** ~8 bars into Drop2) | Same pattern as Track 1: full hat groove returns *into* the drop, not before it |
| Choir Pad 1/2/3 (3-layer stack) | silent elsewhere, **ON exactly 105-129** | Bar-exact match to the full Pre-Break+Break span — the cleanest "dedicated breakdown pad" evidence found this session |
| Riser (MIDI) + Riser (Audio) | **ON exactly 121-129** (MIDI) / overlapping window (Audio) | A riser occupies precisely the same 8-bar window where kick and Bass Pluck return — the actual pre-drop tension device |
| Bass Sustain / Reese (in the FIRST break, 41-64) | ON exactly 41-64.75 | A held/sustained bass texture replaces the rhythmic bassline during the break — direct evidence for "sustained vs rhythmic" |

Both new instances also confirm §10.2's core claims independently: kick and
a full driving-hat groove are both absent for the bulk of a genuine
mid-song break, and a dedicated pad/string/choir element is gated
specifically to that span in **3 of 3** reference songs now checked (Track
A's "Pad Break", Track 1's "Strings"/"Low Lead Trem", Track 2's "Choir
Pad").

Track 2 also has a track **group literally named "BREAK"** containing
Riser, Bass Sustain, Bass Mid, Reese, Strings Ensemble Legato, and Pad
Extreme Pitch — the vendor's own production bundles exactly the role
cluster this document has been assembling from measurement (sustained
texture + riser + pad), which is corroborating, not just coincidental.

### 11.2 Correction to §10.4: the pre-first-drop "Break" is NOT a real breakdown **[LIBRARY, correcting a prior claim]**

Track 1's **first** "Break" (33-48, 16 bars, before Drop 1 at 49) has **kick
and hats fully active throughout** — Fast Hat/Closed Hats fast/Off HiHat all
show unbroken clips through that entire span. This is genuinely different
from every "Break" measured between two established drops. Track A shows
the same pattern from the other direction: its section before Drop 1 is
named "Full Theme," not "Break," and has no kick-off breakdown at all.

**Conclusion**: the kick-silent, pad-exposed breakdown behavior documented
in §10.2/§10.4/§11.1 is specifically a **post-drop phenomenon** — it occurs
in the gap between an already-established Drop and the next one, never
before a track's first drop. A "Break" locator name alone does not imply
breakdown behavior; what matters is whether a drop has already happened.
This matters for whatever eventually decides *when* to enter the Breakdown
render mode, though that decision is out of scope for this document.

### 11.3 Correction to §10.4's hat-timing claim **[LIBRARY, self-correction]**

§10.4 claimed (from the single Track A instance) that hats "return
specifically at the Buildup boundary," i.e. several bars *before* the drop.
With two more instances measured, that turns out to be the **minority**
pattern: in Track A, hats return 8 bars before the drop; in both Track 1
and Track 2, the full hat/percussion groove stays silent through the entire
break and only starts reintroducing **8-32 bars into the drop itself**
(matching §3.3's already-documented "Drums Reintroduction" 24-bar section),
not before it.

The thing that *reliably* signals "the drop is imminent" across all three
instances isn't the hat groove — it's a **riser/tension device** (Track
2's Riser tracks) and, in 2 of 3 instances, an **early kick return** 4-8
bars before the actual drop hit (Tracks 1 and 2; Track A's kick instead
waits for the drop exactly). Bass, by contrast, is consistent 2-for-2 in
the clear cases: the core/sub bass layer does **not** return early — it
waits for the exact downbeat every time it was unambiguously measurable.

This is flagged explicitly as a correction because §10.4's grammar sketch
proposed "hats return at Development" as a rule, and that rule does not
hold up against the wider evidence — the revised grammar in §11.4 reflects
the corrected, majority pattern instead.

### 11.4 Final proposed Breakdown Grammar **[LIBRARY-derived, for approval before any implementation]**

Bar lengths below are the measured range across all three instances
(§3.2-3.4, §10.2, §11.1), not arbitrary choices; a single default is
proposed for each phase, called out as the most common measured value.

```
BREAK_ENTRY        (measured 4-8 bars; default 8)
  kick:            present at entry in 2/3 instances, OFF by phase end in 3/3
  driving hat/perc groove: already OFF or reduced to near-silent by phase end (3/3)
  core/sub bass:   typically still present at entry, fades out during
                   Entry or early Body (3/3, exact fade point varies)
  secondary bass (pluck/rolling layer): no requirement to exit - may
                   continue unchanged (Track 2's Rolling Bass never left)
  pad/atmospheric: begins taking over presence

BREAK_BODY         (measured 16-24 bars; default 16)
  kick:            OFF (3/3 confirmed)
  driving hat/perc groove: OFF (3/3 confirmed)
  core/sub bass:   OFF for the bulk of this phase (2/2 unambiguous
                   instances; Track A's sub-bass clip data was
                   inconclusive - see §10.2's caveat, still unresolved)
  dedicated pad/string/choir element: ON - this is its defining phase
                   (3/3 confirmed: "Pad Break", "Strings"/"Low Lead Trem",
                   "Choir Pad" - though the exact pad span sometimes
                   extends across Entry+Body+Development as one continuous
                   presence rather than starting fresh at Body)
  sustained bass texture (Reese/Bass Sustain, when present): may substitute
                   for the silent rhythmic bass (Track 2's first break)

BREAKDOWN_DEVELOPMENT / PRE_DROP   (measured 4-8 bars immediately
                                     preceding the Drop; default 8)
  kick:            EITHER stays off until the Drop itself (1/3 - Track A)
                   OR reintroduces 4-8 bars early as a pre-drop pump (2/3 -
                   Tracks 1 & 2) - both are real, evidenced options; the
                   current engine's Breakdown mode already implements the
                   simpler "stays off" option, which is not wrong per this
                   evidence, just one of two real choices
  core/sub bass:   stays OFF, returns only at the Drop downbeat (2/2
                   unambiguous instances - no early return observed for
                   this specific layer, unlike kick)
  secondary bass layer: may return alongside an early kick return (1
                   instance - Track 2's Bass Pluck)
  driving hat/perc groove: usually does NOT return here (2/3) - more
                   often reintroduces gradually INSIDE the drop's own
                   first 8-24 bars instead (this corrects §10.4 - see §11.3)
  riser/tension device: the reliable pre-drop signal (present in every
                   instance with a clean measurable pre-drop window) -
                   occupies roughly the phase's own bar span

DROP
  kick, core/sub bass: both present (kick may already have been present
                   if it returned early in Development)
  driving hat/perc groove: often layers in gradually across the drop's own
                   first 8-24 bars rather than being complete from bar 1
                   (already documented as "Drums Reintroduction" in §3.3 -
                   not a new claim, now tied directly to breakdown-exit
                   behavior)
  dedicated breakdown-only pad/atmospheric element: drops out (by
                   definition, gated to the breakdown span)
```

**Scope note (§11.2)**: this grammar applies to a break that follows an
already-established Drop. It is not evidenced to apply to whatever precedes
a track's very first drop.

**Proposed architecture, unchanged from §10.4, still not implemented**:
`BreakdownPhase { Entry, Body, Development }` on `MusicState`/`RenderMode`
instead of one boolean, so the driving hat/perc groove and kick/bass can
each follow their own measured re-entry timing instead of moving together.
The single biggest missing *capability* (not just missing sequencing) is a
**dedicated pad/atmospheric role gated to the breakdown span** - the
current engine only ever mutes existing roles, it never adds a new
presence back in, and that's the element confirmed in 3/3 real reference
songs as what actually fills the space kick/bass leave behind. A pre-drop
riser/tension device is the second-highest-leverage missing piece.

**Still explicitly open** (unchanged from §10.4, not resolved by this
pass): the exact `BreakdownPhase` API shape, whether an optional early-kick-
return variant is worth implementing given it's evidenced in only 2/3
instances, and whether percussion-vocabulary expansion belongs in this
phase of work at all.

### 11.5 Bass register **[LIBRARY, absolute pitch data]**

§10.5 flagged the register-drift bug (`36 + keyRoot + offset` lets key
choice slide the whole register by up to 18 semitones) as the highest-
confidence actionable finding but did not pull absolute pitch data to
recommend a concrete target. This pass did:

- **Full vendor bass-MIDI corpus (55 files, 2786 notes), unfiltered**:
  absolute pitch spans D#1(27) to G#5(80), mean 50.3. **Not directly usable**
  as a register target - this folder mixes genuinely different bass
  *styles* (sub-register "Low" parts down at 27-45 alongside "Pluck"/
  "Stab" parts up at 60-80 that read as mid-range lead material, not
  foundation bass), so an unfiltered average is meaningless for "what
  register should the bass role sit in."
- **The 20-file "groove" subset** (the same real-groove filter already used
  to build `BassArchetype`'s five templates, §5/§10.5 - selective rhythmic
  placement, not rolling 16ths or long pads): absolute pitch spans A1(33)
  to C#5(73), median 55 (G3), mean of each file's own most-common ("home")
  pitch = 52.4 (E3). Each individual file's own range is narrow (most span
  well under 10 semitones internally - e.g. "Low Cliff" spans exactly 4
  semitones, A1-C#2) - real basslines commit to one tight register per
  track, they don't range widely within a single part.
- **Attempted** to pull absolute pitch directly from the three real full-
  mix reference arrangements' own Bassline/Sub Bass/Rolling Bass tracks
  (more authoritative than the vendor's isolated demo-preset MIDI, since
  it's what's actually in a finished mix) - this hit the **same class of
  unreliable result already flagged in §10.2's caveat** (single-repeated-
  pitch output, implausible note counts in the tens of thousands for a
  sparse bassline). **Not used as evidence** - the regex-based `.als`
  note-level parse remains unreliable for this specific query and is
  explicitly not being papered over a second time.
- **Recommendation [INTERPRETATION, anchored in the groove-subset's real
  absolute numbers, not picked arbitrarily]**: the current formula's
  absolute bounds at keyRoot=0 (MIDI 29-43, F1-G2) already sit inside the
  real corpus's lower cluster - the bounds themselves aren't badly wrong.
  The actual fix is to stop letting `keyRoot` slide the whole register:
  resolve the tonic's pitch class from `keyRoot`, place it in a **fixed**
  octave (proposed: the octave containing MIDI 36-47, i.e. C2-B2) rather
  than adding `keyRoot` directly onto the base pitch, then apply the
  existing archetype offsets within a clamped band (proposed: MIDI 29-48,
  F1-C3 - wrap by octave, not by re-adding semitones, if an offset would
  exceed it) so that changing key changes which note plays, never which
  octave the part lives in. This band covers the real groove-subset's
  lower/middle cluster (roughly 13 of 20 files sit at or below G3/55) and
  keeps the current archetype's own note choices unchanged.
- **Explicitly not settled**: the exact final octave is a musical choice
  within the evidenced cluster (the corpus's own home-register median, E3/
  52, is a full third higher than the proposed C2-B2 anchor) - this is
  disclosed as an interpretation, not a single number the data forces.

### 11.6 Serum 2 preset workflow — deeper investigation **[CODE + LIBRARY]**

Checked, this pass, whether there's any legitimate mechanism better than
Capture for getting a real preset into Serum 2 programmatically:

- **Serum 2's actual native preset format**: `.SerumPreset` - confirmed via
  `file` (zlib-compressed data, Xfer's own proprietary binary schema), real
  files present at `/Library/Audio/Presets/Xfer Records/Serum 2 Presets/
  Presets/Factory/Bass/{808,Acid,Electric,Hard,Misc,Modulated,Reese,Retro
  Analog,Sub,Synth}` - genre-appropriate subfolders including a `Reese` and
  `Sub` folder directly relevant to melodic techno bass.
- **The user's own purchased bass packs** ("KICK & BASS - ALKEMIST Vol.1-3",
  "KICK & BASS - ELECTRO X TECH") are `.fxp` (legacy VST2 chunk format,
  confirmed already in §10.1) - real files like `KNB Vol.1 - BASS - Classic
  Reese.fxp` still present.
- **Checked specifically for `.vstpreset`** (the one format a JUCE/VST3 host
  *can* parse into a state chunk without touching any proprietary format) -
  **zero exist** anywhere under the Xfer Serum 2 Presets library or the
  user's own sample/MIDI library. This closes off what would have been the
  one clean host-side auto-load path.
- **Checked Serum 2's own prefs** (`~/Library/Preferences/Serum2Prefs.json`)
  for a "last preset"/"default patch" key that could be pre-seeded before
  our plugin instantiates the instance - none exists. The only path-related
  key (`Serum Presets Path`) points Serum 2's *own internal browser* at its
  library; it isn't something our host writes to force-load a specific
  patch into a freshly-created instance.
- **Conclusion, now with the actual reason established rather than assumed**:
  there is no way for this plugin to programmatically select a real preset
  without either (a) reverse-engineering Xfer's proprietary `.SerumPreset`
  zlib schema well enough to synthesize a valid VST3 state chunk - fragile,
  unsupported, high risk of producing an invalid/corrupt DSP state, not
  undertaken - or (b) the user loading the preset through Serum 2's own UI
  (which natively understands both `.SerumPreset` and legacy `.fxp`) and
  this plugin capturing the resulting real state via the existing
  `getStateInformation()` Capture mechanism. **Capture remains the only
  safe mechanism** - this pass didn't just re-confirm that conclusion, it
  closed off the two alternatives that could have replaced it.
- **Concrete, still-undone workflow gap** (design proposal, not built): the
  status label currently gives a generic instruction ("open Serum 2, browse
  to a real preset..."). It could instead name real, specific candidates
  from the folders confirmed above - e.g. for the Bass track, "Try Factory
  → Bass → Reese or Sub, or your own KICK & BASS - ALKEMIST packs" - using
  real folder/pack names rather than a generic suggestion. This still
  requires the user to do the Capture click themselves (there is no way
  around that, per the conclusion above); it only makes the suggestion
  concrete. Not implemented this pass, pending approval alongside the rest
  of this document.

### 11.7 Summary of what's being proposed for approval

1. **§11.4's Breakdown Grammar** - three phases (Entry/Body/Development)
   with measured bar-length ranges and per-role behavior, corrected in two
   places (§11.2, §11.3) versus the previous single-instance draft. This is
   the piece explicitly gated on approval before any `MusicState`/
   `RenderMode`/generator changes.
2. **§11.5's bass-register fix direction** - fixed-octave anchor (proposed
   C2-B2, clamped band F1-C3) instead of `keyRoot`-driven register drift.
   Exact bounds flagged as an interpretation within real data, not a single
   forced number.
3. **§11.6's Serum 2 conclusion** - Capture confirmed as the only safe
   mechanism (not merely re-asserted); a concrete, real-folder-name status-
   label improvement proposed but not built.

Nothing in this section has been implemented. Per the explicit instruction
this pass responds to, DrumEngine.cpp, BassEngine.cpp, and DROP-mode
generation were not touched, and no code changes were made at all in this
pass - this is a specification document awaiting review.

---

## Sources index

- [Beatportal — Tiësto/VER:WEST](https://www.beatportal.com/articles/465243-tiesto-drops-melodic-house-techno-infused-elements-of-a-new-life-via-his-verwest-alias)
- [WeRaveYou — AFTR:HRS Sessions](https://weraveyou.com/2021/01/tiesto-alias-verwest-reveals-debut-episode-of-aftrhrs-sessions/)
- [Dancing Astronaut — AFTR:HRS Sessions](https://dancingastronaut.com/2021/01/listen-to-tiestos-new-aftrhrs-sessions-mix-series-with-inaugural-episode-by-verwest/)
- [1001Tracklists — Tiësto Prismatic 008](https://www.1001tracklists.com/tracklist/2fwfrjwt/tiesto-prismatic-008-2026-02-21.html)
- [Beatportal — Layton Giordani/Eli Brown/Adam Beyer peak-time techno guide](https://www.beatportal.com/articles/783088-step-by-step-guide-to-producing-techno-peak-time-driving-in-the-style-of-layton-giordani-eli-brown-and-adam-beyer)
- [EDM.com — Layton Giordani](https://edm.com/music-releases/layton-giordani-better-than-u-thought/)
- [Wikipedia — Melodic Techno](https://en.wikipedia.org/wiki/Melodic_Techno)
- [Doubleclap Radio — Melodic Techno vs Progressive House](https://doubleclap.dance/melodic-techno-vs-progressive-house/)
- [Myloops — Melodic Techno Production: Complete Guide](https://www.myloops.net/melodic-techno-production-complete-guide-from-start-to-finish)
- [Myloops — How to Make a Melodic Techno Bassline That Actually Grooves](https://www.myloops.net/how-to-make-a-melodic-techno-bassline)
- [EDMprod — How to Make Melodic Techno like CamelPhat](https://www.edmprod.com/how-to-make-melodic-techno/)
- [Beatportal — Anyma/Miss Monique/ARTBAT/Stephan Bodzin step-by-step guide](https://www.beatportal.com/articles/899368-step-by-step-guide-to-creating-a-melodic-house-techno-track-anyma-miss-monique-artbat-stephan-bodzin)
- [Studio Brootle — Techno Drum Patterns and Programming Tips](https://www.studiobrootle.com/techno-drum-patterns-and-drum-programming-tips/)
- Local: `PML - Complete Arrangement Academy (PML361)/PML - Guide to Electronic
  Arrangements (PDF)/PML - Guide to Electronic Arrangements.pdf`
- Local: `PML_-_Music_Theory_for_Melodic_House_&_Techno_(PML285)/Information/PML
  Harmony & Chord Progressions Slides.pdf`
- Local: the three real `.als` arrangement files listed in §3.2 - clip-level
  (not just locator-level) parsing added in §10.2
- Local: `MLPipeline/drum_grammar/output/drum_grammar.json`,
  `MLPipeline/drum_grammar/output/bass_grammar.json` (previous milestone's
  measurements, reused here, not re-derived)
- Local: `~/Library/AbletonCopilot/serum_load_log.txt`,
  `serum_program_list_debug.txt` (real, dated runtime diagnostics - §10.1)
- Local: `Source/Engine/BassRhythmGrammar.h`, `Source/Engine/BassArchetype.cpp`,
  `Source/PluginProcessor.cpp` (code-traced, not assumed - §10.1)
- [Beatport — Layton Giordani track/BPM/key data](https://www.beatport.com/artist/layton-giordani/374036/tracks)
- [Beatport — "Alto" (Adam Beyer, Layton Giordani)](https://www.beatport.com/track/alto/20487769)
- [It's The DJ — Layton Giordani "Dragonfly" breakdown](https://itsthedj.com/layton-giordani/)
- [Myloops — melodic techno pad/reverb-tail automation technique](https://www.myloops.net/melodic-techno-production-complete-guide-from-start-to-finish)
- Local: `PML - Melodic Techno - 02 - Practical Session Track 1/PML -
  Melodic Techno - 004 - Final arrangement.als`, `PML - Melodic Techno - 03
  - Practical Session Track 2/PML - Melodic Techno - 006 - Final
  arrangement.als` - clip-level parsing added in §11.1, cross-validating
  §10.2's single-instance findings
- Local: `MLPipeline/drum_grammar/analyze_bass_grammar.py` (reused its own
  `BASS_MIDI_CORPUS`/groove-subset filter definitions to compute absolute
  pitch statistics in §11.5 - filter thresholds not re-derived, only reused)
- Local: `/Library/Audio/Presets/Xfer Records/Serum 2 Presets/Presets/
  Factory/Bass/*`, `~/Library/Preferences/Serum2Prefs.json` (checked in
  §11.6 for a viable non-Capture preset-loading mechanism - none found)
