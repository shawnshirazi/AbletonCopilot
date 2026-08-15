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
- Local: the three real `.als` arrangement files listed in §3.2
- Local: `MLPipeline/drum_grammar/output/drum_grammar.json`,
  `MLPipeline/drum_grammar/output/bass_grammar.json` (previous milestone's
  measurements, reused here, not re-derived)
