# Sound & Rhythm Diagnostic — Pass 4

**Status: research/diagnostic only.** No `DrumEngine.cpp`, `BassEngine.cpp`,
`MelodyEngine`, sample-selection code, or Serum2 code was touched to produce
this document. Every claim is tagged **[CODE]** (traced directly from this
repository's source), **[MEASURED]** (a number produced by running real
plugin code — either `DrumSampleSelector`/`DrumSamplePackTier`/
`DrumSampleScoring`/`DrumSampleSelection` against the real, current, on-disk
sample-index cache, or `Engine::generateMusicIdentity`/`generateCompactLoop`
against real seeds), **[LIBRARY]** (from the reference `.als` arrangements /
vendor packs already catalogued in `melodic_techno_research.md`), or
**[INTERPRETATION]** (a judgment call, flagged as such).

This continues directly from `melodic_techno_research.md`,
`melodic_techno_production_grammar_v2.md`, and `youtube_melodic_techno_research.md`
(read in full for this pass) — it does not re-derive that evidence, it applies
it against the **current, post-fix** state of the plugin (this session
already shipped a tier-cascade sample-selection fix, a compact 16-bar loop,
a breakdown pad, and a bass register clamp — see git history) to find out
what's *still* wrong.

---

## 1. Reference findings (what "Melodic Techno" concretely requires)

Already fully documented in the three companion docs above; not repeated in
full here. The load-bearing numbers this pass leans on:

- **KICK**: 4-on-the-floor, 100% on-beat `[M+Y-HIGH]`. Real corpus fingerprint:
  duration 494ms mean, sub-bass fraction (20-100Hz) 0.744, attack 14ms.
- **CLAP**: 84.1% on-beat (steps 4/12), 2.63 onsets/bar, 80.8% adjacent-bar
  repetition `[M]`.
- **HAT (closed)**: 6.86 onsets/bar, dominant bucket is the **offbeat-8th**
  (steps %4==2) at **50.2%**, on-beat 27.1%, weak-16th only 22.8% `[M+Y-HIGH]`.
- **PERC**: 10.49 onsets/bar (dense), dominant bucket is **weak-16th** at
  48.7% (this role, unlike hat, IS weak-16th-dominant), negative correlation
  with kick (-0.45) and clap (-0.43) `[M+Y-HIGH]`.
- **Swing**: 8-12% on hats/perc, kick/bass stay strict `[Y-MEDIUM]` —
  currently **not implemented anywhere** (confirmed again this pass, see §5).
- **Breakdown**: kick+full hat/perc groove silent for the entire Break
  (16-24 bars), a **dedicated pad/atmospheric element** exposed specifically
  during that span (3/3 real reference arrangements), driving hat/perc
  groove reintroduces **gradually across the drop's own first 8-24 bars**,
  not before it `[LIBRARY, HIGH]`.
- **Sound selection tiers**: Tier1 Melodic Techno packs = PML Mirage/
  Mystique, Odd Frequency Exo/Exo2, PML Complete Arrangement Academy, Ekko
  Mirage. Tier4 off-genre = Tech House packs (TechHouseMarket, Sonance
  Sounds Dirty Tech, Odd Frequency Tech House/Danza/Hype bundles) `[LIBRARY]`.

---

## 2. Current-vs-reference differences (headline finding)

**The single biggest, most concrete, newly-confirmed finding this pass:
the generated loop's audible "Drop" span never reaches the drum engine's
own 3rd/4th energy stages.**

Traced via source **[CODE]** and confirmed empirically **[MEASURED]** across
3 deterministic seeds (42, 1234, 999999):

- `generateDrop()` builds a real 4-stage energy arc across all 16 bars of
  `identity.drumMotif`: bars 1-4 = `kEstablish`, bars 5-8 = `kDevelop`,
  bars 9-12 = `kIncrease`, bars 13-15 = `kFullDrop`, bar 16 = thinned
  transition (`DrumEngine.cpp` lines 297-301, 627-661).
- `generateCompactLoop()` copies `identity.drumMotif` bars 0-7 **verbatim**
  into the played loop's "Drop" span, then **zeroes** kick/hatClosed/
  hatOpen/percA/percB for bars 8-15 (the breakdown span) —
  `BreakdownArrangement.cpp` lines 274-294.
- **Bars 0-7 = `kEstablish` + `kDevelop` only.** `kIncrease` and `kFullDrop`
  — the two stages with the highest hat/perc/percB energy multipliers — are
  generated (real CPU work) and then **discarded every single time**, never
  audible in any loop repeat.

This is why the drop reads as thin rather than "convincing melodic techno
energy," independent of sample quality:

| Role | Played density (bars0-7), mean of 3 seeds | Measured corpus mean | Ratio |
|---|---|---|---|
| KICK | 4.00/bar | 4.00/bar | 100% — correct |
| CLAP | 2.17/bar | 2.63/bar | 82% — close |
| HAT CLOSED | 2.88/bar | 6.86/bar | **42%** |
| PERC A | 2.12/bar | — (see below) | — |
| PERC B | 0.25/bar | — | — |
| PERC A+B combined | 2.38/bar | 10.49/bar (whole PERC role) | **23%** |

`[MEASURED]`, raw numbers in `/private/tmp/.../scratchpad/stemtest/rhythm_output.txt`
this session (standalone tool, `Engine::generateMusicIdentity` +
`Engine::generateCompactLoop`, zero product code touched).

percB's near-silence (flat ~0.25/bar across all 3 seeds) is not noise — it's
by design: `kEstablish.percB = 0.00f`, `kDevelop.percB = 0.30f`
(`DrumEngine.cpp` line 298-299). percB was explicitly written to "enter
later, especially bars 9-15" (the engine's own comment, line 279) — but bars
9-15 is exactly the span `generateCompactLoop` always discards. percB's
intended entrance **never happens** in the current architecture.

**A second, independent rhythm-shape problem**, also confirmed across all 3
seeds: HAT CLOSED's *position* distribution is inverted from the reference.

| | on-beat | offbeat-8th | weak-16th |
|---|---|---|---|
| Corpus `[M]` | 27.1% | **50.2%** (dominant) | 22.8% |
| Played, seed 42 | 3.7% | 29.6% | **66.7%** |
| Played, seed 1234 | 8.7% | 13.0% | **78.3%** |
| Played, seed 999999 | 0.0% | 36.8% | **63.2%** |

Visually (raw ASCII, seed 42, `..........#o...o` repeated almost every bar):
the strongest hit (`#`) correctly lands on step 10 (offbeat-8th), but a
consistent secondary hit (`o`) lands on step 15 (weak-16th, the position
right before the bar wraps) in nearly every single bar — this secondary hit
is far more frequent/regular than the reference's own much sparser,
lower-velocity ghost hits at that position (corpus: step 15 probability
0.03, velocity 0.42 — a rare, quiet ghost note, not a near-constant one).
**This is what makes the hat groove read as "busy/generic 16th-pattern"
rather than "selective offbeat pulse"** even though average density is
actually *below* the reference, not above it — density and *placement* are
two separate problems here, both real. `[MEASURED, INTERPRETATION on cause]`

**Net conclusion**: the user's own prior instinct ("don't add more hats just
because the corpus has lots of hat hits") already correctly steered away
from raw-density matching — but the resulting engine is now **under-dense
by roughly 2-4x on hat/perc** relative even to the *already-selective*
groove-filtered corpus, largely because of the stage-arc-truncation bug
above, not because the per-stage multipliers themselves are wrong. Fixing
the truncation (letting the played Drop span actually reach `kIncrease`/
`kFullDrop`, or re-scaling the 4 stages to fit inside the 8 bars that are
actually audible) is higher-leverage than touching the multipliers again.

---

## 3. Sample-selection problems

**Good news, confirmed by re-running the real selection algorithm against
today's real, current sample-index cache (5699 analyzed samples,
`~/Library/AbletonCopilot/drum_sample_index_cache.json`, dated today) — not
a stale report:**

The tier-cascade fix already shipped this session (`preferBestAvailableTier`,
`DrumSampleSelector.cpp`) is working. **Every top-10 candidate for all 6
roles is now Tier1-MelodicTechno** (Odd Frequency Exo/Exo2, PML Mirage/
Mystique, PML Complete Arrangement Academy, Ekko Mirage) `[MEASURED]`. This
is a real, large improvement over the **pre-fix** state recorded in
`MLPipeline/drum_grammar/output/sample_ranking_report.md` (dated before this
session's fix), where KICK's #1-ranked candidate was
`TechHouseMarket - Kick 32.wav` and HAT's entire top-5 was Tech House/generic
FL Studio packs. **The "wrong pack tier" problem is substantially resolved**
for all 6 roles as of right now — this is a real before/after, not an
assumption.

Per-role detail (full top-10 with acoustic fingerprints in
`/private/tmp/.../scratchpad/stemtest/audit_output.txt` this session):

| Role | Raw pool → post-tier-cascade pool | #1 candidate | Pack |
|---|---|---|---|
| KICK | 1433 → 178 | `PML_DPV7_BD_Kick_001.wav` | PML Complete Arrangement Academy |
| CLAP | 1672 → 353 | `Odd Frequency - Exo 2 - Clap 9.wav` | Odd Frequency Exo/Exo2 |
| CLOSED HAT | 1038 → 175 | `Odd Frequency - Exo - Closed Hat 4.wav` | Odd Frequency Exo/Exo2 |
| OPEN HAT | 539 → 123 | `PML_DPV2_OHH_1_ANALOG_011.wav` | PML Mystique |
| PERC A | 1017 → 164 | `Odd Frequency - Exo 2 - Perc 1.wav` | Odd Frequency Exo/Exo2 |
| PERC B | 1016 → 163 | (same pool, A's pick excluded) | Odd Frequency Exo/Exo2 |

**Percussion, specifically** — the user's suspicion ("technically valid
percussion that is stylistically too Tech House") does **not** hold at the
pack-tier level anymore: every one of PercA/PercB's top-10 candidates is a
Tier1 pack, confirmed by both this pass's audit and a real, dated,
plugin-generated diagnostic (`~/Library/AbletonCopilot/
percussion_selection_debug.txt`, today's timestamp) that shows the actual
last real selection (percA = `Odd Frequency - Exo 2 - Perc 20.wav`, percB =
`Odd Frequency - Exo - Perc 6.wav`, both Tier1).

**What the pack-tier fix can't see, and is the more likely real remaining
issue**: this project's PERC "rack" classification (`RackClassification.h`,
not touched by the tier fix) is still a **single, generic PERC bucket** —
every one-shot in an "Odd Frequency Exo2/Percs" or "PML Mystique/
Percussions" folder is one undifferentiated pool, regardless of whether the
individual sample is a bright metallic click/rim, a low tom-like hit, or a
shaker/maraca texture. The real reference arrangements (§10.2 of
`melodic_techno_research.md`) use a **much broader, named vocabulary**
(Woodblock, Ride, Stick, Shaker×2-3, "Top") that this engine's 2-voice
PercA/PercB model structurally cannot reproduce, no matter how well the
pack-tier is weighted — **this is a rhythm-vocabulary/pool-granularity gap,
not a wrong-sample bug**, and matches
`melodic_techno_production_grammar_v2.md` §1's own already-flagged note
("broader than this engine's current 2-voice PercA/PercB model").
`[LIBRARY+CODE, INTERPRETATION]`

**One real, minor, non-blocking data-quality note**: `DrumSampleFeatures::
spectralCentroidHz` is defined in the struct and populated by the Python
research pipeline's offline analysis (real numbers in `report.md`), but is
**never computed anywhere in the runtime C++ analysis path**
(`DrumSampleAnalysis.cpp`) — it's always 0.0 in the live cache. Confirmed
this field is **not used by `Engine::scoreForRole`'s weighted scoring**
(only duration/attack/zcr/pitch are weighted — `DrumSampleScoring.cpp` lines
44-47), so this is dead data, not a selection bug — flagged only because a
future scoring-weight change might assume it's populated. `[CODE]`

---

## 4. Processing opportunities (proposed chain, NOT implemented)

Traced the real signal path **[CODE]**: both `generatedSampleVoices` (real
samples) and `generatedDrumSynthVoices` (synth fallback) in
`PluginProcessor.cpp`'s `processBlock()` do **pure `addFrom`/
`renderDrumVoice` with only a scalar gain** — zero EQ, saturation,
compression, reverb, delay, or stereo processing exists anywhere in the
drum signal path today. (Contrast: melody voices already have a working,
tested precedent — `MelodyVoice::eqHp`/`eqShelf`, a static per-category
high-pass+shelf `FeatureExtractor::Biquad` pair, gated by `eqActive`/
`genreEqEnabled`, applied per-voice before mixing — `PluginProcessor.cpp`
lines 218-238, 1357-1364. This is the direct architectural pattern to reuse
for drum roles, not a new design.)

Per role, separating "wrong sample, EQ won't fix it" from "good sample,
processing gets you closer" — grounded in the measured fingerprints from
§3 and `report.md`:

| Role | Verdict | Reasoning |
|---|---|---|
| **KICK** | Good sample, processing helps | Selected candidates run 360-420ms decay vs. the corpus's fuller 494ms mean — on the shorter/tighter end, not wrong, but a touch shy of the "300-500ms audible tail, 60-80Hz body" spec. A short saturation/low-end exciter stage would add perceived weight/length without needing a different sample; genuinely lengthening a short one-shot's tail convincingly needs a synthesized/layered sub-tail, not simple EQ. |
| **CLAP** | Good sample, minor polish only | Selected candidates (0.42-0.6s, ZCR 3000-4700Hz) already sit close to the corpus fingerprint. A light high-shelf + transient click could tighten the snap; not a priority. |
| **CLOSED HAT** | Good sample; the problem is rhythm, not the sample | Acoustic fingerprint (duration 0.23-0.5s, ZCR 7000-9000Hz) matches the corpus hat fingerprint well. The "still sounds Tech House" complaint for hats is much better explained by §2's density/placement findings than by sample character. |
| **OPEN HAT** | Good sample | Candidates match the corpus fingerprint closely (duration ~0.37-0.49s, ZCR 4700-9300Hz). No action needed. |
| **PERC A/B** | Good samples, but the vocabulary itself is too narrow for EQ to fix | Individually well-matched to the generic PERC fingerprint, but see §3 — differentiating a "click/rim-like" PercA from a more "low/tom-like" PercB via a resonant EQ bump/notch on each could add distinctiveness *within* the current 2-voice budget, but won't reproduce the real reference's 5-7-voice percussion vocabulary. Treat as a partial mitigation, not a fix. |
| **BASS↔KICK interaction** | DSP gap, sourced, real | No sidechain-style gain-reduction envelope exists anywhere (confirmed again this pass — only the *sequencing*-level kick-avoidance in `BassEngine.cpp` exists). ~90-140ms release at 122-124 BPM is a concrete, externally-sourced, currently-missing DSP feature. |

**Genre-character opportunities that apply across multiple roles** (all
`[Y-MEDIUM]`, none corpus-verifiable since they're audio-domain, not
MIDI-domain):

- **Reverb send on hats/perc**: real PML arrangement-guide text explicitly
  says "spacious percussions, delayed hi-hats" and "wide stereo imaging" —
  a short-to-medium reverb send + a rhythmic (dotted-8th/16th) delay on
  hat/perc specifically is one of the more plausible "this is genuinely why
  it reads as dry/Tech-House rather than spacious/Melodic-Techno" factors,
  independent of the rhythm/density issue in §2.
- **Stereo widening on hats/perc**: consistent with the same "wide stereo
  imaging" language; Tech House grooves tend to sit more centered.
- **Swing (8-12%, hats/perc only)**: a sequencing/timing fix, not audio
  processing, but belongs in the same "why does this read as the wrong
  genre" bucket — currently zero swing exists anywhere (`DrumEngine.cpp`
  places every hit on the exact 16th grid).

**Explicitly not proposed as implementation-ready this pass**: exact filter
frequencies, compressor ratios, or reverb decay times — none of that is
measurable from this project's own corpus (MIDI/onset data has no audio-DSP
dimension), so any specific numbers would be picked, not evidenced. A future
implementation pass should pick conservative starting values and let the
user's own ears (not a metric) validate them, consistent with the
"statistics are evidence, references are the target" instruction.

---

## 5. Rhythm problems

Summarized from §2, plus items not yet covered:

1. **Stage-arc truncation** (the headline finding, §2) — `kIncrease`/
   `kFullDrop` are generated and discarded every loop repeat. **Highest
   leverage fix in this whole document.**
2. **HAT CLOSED under-density**: 42% of the corpus mean in the audible span,
   confirmed across 3 seeds — a direct consequence of #1, not a separately
   broken multiplier.
3. **PERC A+B under-density**: 23% of the corpus mean combined — same root
   cause, compounded by percB's `kEstablish` value being intentionally 0.
4. **HAT CLOSED weak-16th-dominant instead of offbeat-8th-dominant** — a
   real, separate, position-level (not density-level) mismatch: a
   near-constant secondary "ghost" hit at step 15 is far more frequent than
   the reference's own sparse ghost notes at that position. Worth a
   dedicated look independent of the density fix.
5. **Swing**: still 0% everywhere (kick/clap/bass AND hat/perc all land on
   the exact grid) — the 8-12%-on-hats-only target from `[Y-MEDIUM]`
   evidence remains unimplemented.
6. **What's already correct and should NOT be touched**: KICK (100% match to
   corpus density/position), CLAP (82-100% on-beat across seeds, close
   density match, matches the "stable backbeat, don't vary it" finding).
   Any future implementation pass should be careful not to regress these
   while fixing hat/perc.
7. **Breakdown span itself**: kick/hatClosed/hatOpen/percA/percB correctly
   silent for bars 8-15 in all 3 seeds (0 onsets, matches the 3/3
   reference-arrangement finding). Clap stays present (16-18 onsets across
   8 bars) — a deliberate, disclosed judgment call from the prior session,
   not re-litigated here. Pad content is present but sparse (~14 onsets
   across 128 steps) — consistent with the "sustained notes, intentional
   rests" design goal, not obviously wrong.

Full ASCII grids for all 6 roles × 16 bars × 3 seeds (raw `generateDrop()`
output AND the actual played `CompactLoop`) are in
`/private/tmp/claude-501/.../scratchpad/stemtest/rhythm_output.txt` from this
session — not reproduced in full here for length, but available for direct
inspection; re-run `audit_rhythm.cpp` (same directory) against any future
code changes to re-verify.

---

## 6. Bass-vs-Melody Serum2 problem

Traced **[CODE]**, then confirmed against real, dated, on-disk evidence from
**today's session** (not a stale prior finding):

1. **Architecture**: `PluginProcessor`'s `melodyVoices[trackIndex]` — Bass
   (track 0), Melody/Lead (track 1), and Pad (track 2, added this session)
   each own a **fully independent** `juce::AudioPluginInstance` (a separate
   Serum2 VST3 instance per track — confirmed by today's
   `serum_load_log.txt` showing 3 separate `createInstanceFromDescription`
   calls, one per track index). `captureMelodyTrackState`/
   `loadCapturedPreset` are strictly indexed by `trackIndex`
   (`PluginProcessor.cpp` lines 1551-1589) — **zero shared state exists
   between tracks anywhere in the code.**
2. **What's actually loaded right now**: `~/Library/AbletonCopilot/
   CapturedPresets/` **does not exist on disk** — confirmed by directory
   listing this session. Since `pendingState` (the only mechanism that
   changes a voice away from Serum2's own default state) is populated
   exclusively by `loadCapturedPreset` or a host-restore, and neither has
   ever happened for any of the 3 tracks, **all three voices — Bass,
   Melody, Pad — are currently playing Serum2's own default factory "Init"
   patch, identically.**
3. **Why Bass and Melody sound the same**: not a code bug. It's the direct,
   expected consequence of "3 independent instances, 0 captures ever
   performed" — three separate copies of the same default patch sound
   identical by definition, regardless of how correct the hosting
   architecture is.
4. **Can the existing Capture workflow support genuinely separate Bass/
   Melody/Pad states?** **Yes, already, today, with zero code changes** —
   confirmed by the per-`trackIndex` independence in point 1. The workflow
   already works exactly as needed; it has just never been exercised.
5. **What real preset candidates exist for a genuinely different Bass vs.
   Melody/Pad timbre** (per the user's explicit "not just an octave shift,
   a genuinely different sound" ask): Serum 2's real factory library at
   `/Library/Audio/Presets/Xfer Records/Serum 2 Presets/Presets/Factory/`
   has dedicated `Bass/` (with `Reese`/`Sub`/`808`/`Acid` subfolders — see
   `melodic_techno_research.md` §11.6), `Lead/`, `Pluck/`, and `Pad/`
   folders — real, concrete, genre-neutral but structurally correct
   starting points for the 3 tracks' 3 different roles. `.SerumPreset`
   files in this library are real (confirmed to exist, zlib-compressed
   proprietary format) but **not programmatically loadable** — re-confirmed
   this pass, no new mechanism found (see point 6).
6. **Safest real workflow** (re-confirmed, not newly discovered): the user
   opens Serum 2's own GUI for a given track (the existing "Open Serum 2"
   button), browses to a real preset (e.g. `Factory → Bass → Reese` for
   track 0, `Factory → Lead` or `Factory → Pluck` for track 1, `Factory →
   Pad` for track 2 — or their own purchased `Voltage Vol.2`/`Odd Frequency
   Exo2 Serum 2 Presets` libraries, browsable the same way), then clicks
   the existing **Capture** button. This is proven, working, and requires
   no code changes — it requires the user to actually do it, three times,
   with three genuinely different presets. There is no way to automate the
   preset *choice* (re-confirmed: `getNumPrograms()`/`setCurrentProgram()`
   is a dead end, no `.vstpreset` files exist anywhere in the library, no
   Serum2Prefs.json hook exists — `melodic_techno_research.md` §11.6,
   re-verified not re-litigated this pass).
7. **One concrete, still-not-built UI improvement** (carried over from
   §11.6 of the research doc, re-confirmed still relevant and now
   trivially actionable given the real folder names in point 5): the
   status/suggestion text shown per track could name real, specific,
   category-matched folders (`Factory → Bass → Reese or Sub` for track 0,
   `Factory → Lead` or `Factory → Pluck` for track 1, `Factory → Pad` for
   track 2) instead of a generic "open Serum 2 and browse" instruction —
   small, UI-only, no musical-generation risk.

---

## 7. Proposed implementation order

Per the user's own explicit rule ("use actual reference evidence first,
don't touch the rhythm engine just because a metric looks wrong") — ordered
by evidence strength and blast radius, smallest/safest first:

1. **Fix the stage-arc truncation** (§2, §5.1) — the single highest-leverage
   fix, and arguably not even a "musical" change: it's making the already-
   designed `kIncrease`/`kFullDrop` stages actually reachable within the
   compact loop's 8 played bars, rather than changing any measured
   probability table or multiplier. Two real options exist (not decided
   here): (a) compress the existing 4-stage arc to fit 8 bars (2 bars per
   stage instead of 4), or (b) widen the compact loop's audible "Drop" span
   itself so more of the original 16-bar arc survives. This needs its own
   design decision before implementation — flagging both options, not
   picking one.
2. **percB's `kEstablish=0.0` interacting with #1** — once #1 is decided,
   re-check whether percB's "enters late" design intent still makes sense
   inside whatever the new audible span turns out to be.
3. **HAT CLOSED's weak-16th ghost-hit over-frequency** (§2, §5.4) — a
   narrower, more surgical fix than #1; likely addressable by reducing how
   often the transition-bar/ghost-hit logic places a hit at the pre-wrap
   weak-16th position, without touching the main offbeat-8th pulse logic at
   all.
4. **Bass-vs-Melody Serum2**: zero code changes needed (§6) — this is a
   **user workflow item** (perform 3 real Captures), optionally paired with
   the small, low-risk UI text improvement in §6 point 7 naming real preset
   folders per track.
5. **Swing** (§1, §5.5): a real, sourced, currently-zero feature — additive
   (a timing offset applied at trigger time for hat/perc roles only),
   doesn't change any existing probability/density logic, but does touch
   the audio-thread trigger path (`PluginProcessor.cpp`), so it should be
   scoped and tested carefully.
6. **Drum-role processing chain** (§4): reuse the existing `MelodyVoice`
   per-category static-EQ pattern (`eqHp`/`eqShelf`/`eqActive`) as the
   architecture, applied per drum role instead of per melody category.
   Reverb/delay sends and stereo widening for hat/perc are real,
   `[Y-MEDIUM]`-sourced opportunities but need their own design pass (no
   reverb/delay bus exists anywhere in the plugin today) — larger scope
   than the EQ precedent alone.
7. **Percussion vocabulary expansion** (§3): the lowest-leverage-per-effort
   item here — genuinely fixing "PercA/PercB can't reproduce a 5-7-voice
   real percussion arrangement" requires either a 3rd+ percussion voice or
   a smarter within-PERC sub-classification (organic vs. metallic), both
   real scope increases. Recommend deferring until 1-3 are validated by
   ear, since it's the most speculative/highest-effort item on this list.
8. **Sidechain-style kick↔bass ducking DSP** (§4): real, sourced, but a new
   DSP feature (gain-reduction envelope) with no existing precedent in the
   codebase at all — larger, separately-scoped work, not a quick add.

**Nothing above has been implemented.** Per the explicit instruction this
document responds to, `DrumEngine.cpp`, `BassEngine.cpp`, sample-selection
code, and Serum2 code were not touched to produce it.
