# Melodic Techno Production Grammar v2 (proposed)

**Status: proposal only. Nothing in this document has been implemented.**
No `DrumEngine.cpp`, `BassEngine.cpp`, `MelodyEngine`, `MusicState`,
`PluginEditor`, or other engine/UI code has been touched to produce it. This
is meant to become the reviewed foundation for the *next* version of those
systems — implementation should only begin after the user approves this
document (or a revised version of it), exactly as agreed for the previous
Breakdown Grammar proposal.

## 0. What this combines

Three independent evidence streams, cross-referenced against each other
wherever they overlap:

1. **This project's own measured corpus** — real `.als` arrangements (3 full
   songs, clip-level timing), `drum_grammar.json` (real vendor drum MIDI,
   multiple roles), `bass_grammar.json`/`BassRhythmGrammar.h` (20-file,
   856-note real bassline groove-subset), `BassArchetype.cpp` (5 corpus-
   derived rhythmic templates). See `melodic_techno_research.md` sections
   1-11.
2. **YouTube/production-technique research** — 41 claims from real tutorial
   videos, technique-breakdown articles, and producer interviews. See
   `youtube_melodic_techno_research.md` and `youtube_production_knowledge.json`.
3. Nothing here is invented. Every rule below cites which of the two
   documents above it comes from, and its confidence tier.

**Reading key**: `[M]` = from this project's own measurement.
`[Y-HIGH/MEDIUM/LOW]` = from the YouTube corpus at that confidence tier.
`[M+Y]` = independently confirmed by both. A rule with no `[M]` tag has
never been checked against this project's own corpus and should be treated
as more speculative than one that has.

---

## 1. DRUMS grammar v2

| Element | Rule | Evidence |
|---|---|---|
| Kick | Four-on-the-floor, every beat, no exceptions found in either corpus | `[M+Y-HIGH]` — `drum_grammar.json` KICK on_beat_fraction=1.0; Myloops states the same pattern |
| Kick tail | Real melodic-techno kicks favor a longer decay (~300-500ms, energy ~60-80Hz) than tech-house; this is *why* the bass has to duck around it, not incidental | `[M+Y-HIGH]` — explains the independently measured `bass_grammar.json` on_kick_fraction=0.2126 |
| Closed/driving hat | Dominant real placement is the 8th-note offbeat (~50% of onsets), NOT uniform — 27% land on-beat, 23% land on a weak 16th. Reduce velocity specifically on offbeat hits for a more organic feel | `[M+Y-HIGH]` — `drum_grammar.json` HAT stats; Attack Magazine's Beat Dissected |
| Open hat/ride | A second, distinctly-processed (often heavily compressed) open hat layered on the offbeat, separate from the closed/driving layer, with subtle variation every couple of bars | `[Y-MEDIUM]` — not separately broken out in `drum_grammar.json` today (HAT role doesn't currently distinguish open/closed statistically) |
| Clap/snare | **Default should be on-beat** (beats 2 and 4 or equivalent) — `[M]` measured `CLAP.on_beat_fraction=0.841`, `adjacent_bars_identical_fraction=0.808`. A syncopated variant (single hit every 2 bars + double hit every 4, or clap-on-second-kick-only) is real and genre-appropriate but is a **named minority technique (~16% of the real corpus)**, not a default | `[M, corrects Y-MEDIUM]` |
| Percussion | Dense (~10-11 onsets/bar), dominant placement is syncopated/weak-16th (49%), not on-beat. Real vocabulary includes conga/tom/maracas/rim/shaker/ride/woodblock (per the arrangement roster in `melodic_techno_research.md` §10.2) — broader than this engine's current 2-voice PercA/PercB model | `[M+Y-HIGH]` |
| Ghost notes | Sit around velocity 35-50 in general (non-genre-specific) electronic drum programming; darkening tone (not just lowering volume) reads more realistic than volume reduction alone | `[Y-LOW]` — not melodic-techno-specific, not independently re-measured this pass |
| Swing | No melodic-techno-specific number available. One adjacent-genre (dark Berlin techno) data point: 50-55% | `[Y-LOW]` — flagged explicitly as NOT melodic techno |

**Implementation priority**: kick/hat/perc density-and-placement rules are
`HIGH`-confidence and ready to inform `DrumEngine.cpp` changes once
approved. The clap-syncopation-as-minority-variant correction is
particularly actionable: today's engine should not treat a syncopated clap
pattern as the default shape. Swing and ghost-note velocity need a
dedicated measurement pass before they're implementation-ready.

## 2. BASS grammar v2

| Element | Rule | Evidence |
|---|---|---|
| Placement vs. kick | Leave the kick-aligned 16th steps empty/sparse — measured `on_kick_fraction=0.2126`, well under the 25% uniform baseline | `[M+Y-HIGH]` — 2 independent tutorials state the identical rule |
| Note length | Short and punchy: `mean_note_len_steps=1.32` sixteenth-steps. Fast attack/decay, low sustain, slight release increase on longer notes only | `[M+Y-HIGH]` — 3 independent sources, the single best-evidenced claim in this whole document |
| Register/octave | Tight, single-octave-or-narrower range (`mean_pitch_range_semitones=2.15` measured); root dominates at 74.65% of onsets. **Register should be a fixed target independent of key** (already proposed in `melodic_techno_research.md` §11.5 — anchor tonic pitch class to a fixed octave, e.g. C2-B2, clamp band F1-C3, instead of `36 + keyRoot + offset`) | `[M+Y-HIGH]` |
| Root/fifth/third — **correction** | Real corpus is root-then-minor-third-forward, NOT root-and-fifth as generic guidance claims: root 74.65%, +3 semitones (minor third) 5.14%, -2 semitones 4.91%, +7 semitones (fifth) only 3.74%. **The fifth is less common than the minor third in the real groove-subset corpus** | `[M, corrects Y-MEDIUM]` — a genuine finding, not a restatement |
| Syncopation | Dropping alternating/offbeat notes an octave lower is a real, specific technique with no equivalent dimension in `BassRhythmGrammar.h`/`BassArchetype.cpp` today | `[Y-LOW]` — candidate for a future archetype dimension, not yet cross-validated |
| Repetition vs. development | Small melodic/passing-note changes across a 4-bar structure rather than exact repetition | `[Y-MEDIUM, consistent with M]` — already the shape of the existing "bar0 canonical → bars1-3 repeat-or-touch" logic in `BassArchetype.cpp`, independently derived, now corroborated rather than contradicted |
| Sidechain | ~80-150ms release at 122-124 BPM, described as "non-negotiable" | `[Y-MEDIUM]` — audio-domain, not checkable against a MIDI-only corpus |
| Layering | Sub layer (sine/triangle) + mid/character layer (saw/square+filter), optional higher "top bass" layer with independent rhythm | `[M+Y-HIGH]` — a real reference arrangement (Track 2) has exactly this structure: Rolling Bass 1/2 (persistent), Sub Bass, Bass Pluck (secondary, different re-entry timing) |
| Sound design (Serum) | Two detailed but single-source oscillator/filter/envelope recipes exist (Mind Flux) — real, specific, usable as a starting point, not a verified genre rule | `[Y-LOW]` |

**Implementation priority**: kick-avoidance, note-length, and register-
clamping are `HIGH`-confidence and directly actionable — register-clamping
in particular was already independently proposed in the prior pass and is
now further corroborated. The root/fifth/third correction should inform
`BassRhythmGrammar.h`/`kBassPitchOffsets` interpretation (that table
already reflects the corrected reality — root/±3/±2 dominate over ±7 — so
no *change* is needed there, but any future melody/harmony-writing logic
that assumes "root and fifth" as the go-to interval pair should be
corrected). Octave-drop syncopation is a real candidate for a 6th
`BassArchetype`, but needs its own measurement pass first.

## 3. BREAKDOWN grammar v2

This extends the 3-phase grammar already proposed and approved-pending in
`melodic_techno_research.md` §11.4 (`BREAK_ENTRY → BREAK_BODY →
BREAKDOWN_DEVELOPMENT/PRE_DROP → DROP`). YouTube evidence corroborates most
of it and adds two new, explicitly-flagged open questions.

**Confirmed/strengthened by YouTube evidence:**
- Removal order (kick first, bass reduces, pad/lead exposed) — `[M+Y-HIGH]`
- Breakdown length 16-32 bars — `[M+Y-HIGH]`, matches measured Entry+Body+
  Development ranges
- Riser as a pre-drop tension device — `[M+Y-HIGH]`, a real Riser track
  (MIDI + audio) was found in the exact 8-bar pre-drop window in one
  reference arrangement
- Dedicated pad/atmospheric element exposed during the break body — `[M]`,
  now the single highest-leverage missing *capability* in the current
  engine (it only mutes existing roles, never adds a new presence)

**New, explicitly open questions raised by the YouTube evidence, NOT yet
resolved:**
- **Pre-drop pause/silence**: 2 independent sources describe cutting drums
  or inserting a short silence right before the drop. This was **not
  observed** in any of the 3 measured reference arrangements (kick either
  stays off until the drop or returns 4-8 bars early — never a hard gap).
  Open question: is this a real technique this project's specific
  references don't happen to use, or a finer-grained effect than bar-level
  clip analysis can see? Not implemented either way until resolved.
- **Filter automation during breakdown**: two flavors described (mix-bus
  high-pass sweeping up 20Hz→200-400Hz over 16-32 bars; bassline-specific
  low-pass automation, attributed to Layton Giordani specifically as a
  signature technique). Neither is observable from this project's
  clip-timing/MIDI data. Real, plausible, `MEDIUM` confidence, not yet
  implementable without a DSP automation feature that doesn't exist in the
  engine today.
- **"Keep some percussion during the breakdown" vs. full silence**: YouTube
  guidance suggests keeping subtle rhythmic elements going; this project's
  own measurement shows the full hat/perc groove going **completely
  silent** in all 3 reference arrangements. The measured, primary-source
  finding should win here — full silence for the driving groove during
  Body, not a reduced-but-present version.

**Net effect on the §11.4 grammar**: no phase boundaries or bar-length
ranges change. The pad/atmospheric-role gap and riser gap identified in
§11.4 are now doubly-confirmed (both primary measurement and independent
YouTube sources agree they're the two biggest missing pieces). The
pre-drop-pause and filter-automation questions are recorded as open
research threads, not folded into the grammar as rules, because they
aren't yet resolved against this project's own evidence.

## 4. DROP grammar v2

| Element | Rule | Evidence |
|---|---|---|
| First-bar density | The drop's first bar is NOT necessarily the fullest version of the arrangement. In 2 of 3 measured reference arrangements, kick+bass are present but the full hat/percussion groove is still silent or partial | `[M+Y-HIGH]` — independently corroborated by generic "gradual reintroduction" guidance |
| Drum reintroduction | The hat/percussion groove reintroduces gradually across the drop's own first 8-24 bars (matches the already-documented "Drums Reintroduction" named section from §3.3) | `[M+Y-HIGH]` |
| Bass re-entry | Core/sub bass lands exactly on the drop's downbeat — no early return observed in either of the 2 unambiguous measured instances | `[M]` — internal finding only, not found or contradicted by any external source this pass |
| Kick re-entry | Either waits for the exact drop (simpler, matches current engine behavior) or returns 4-8 bars early during Development as a pre-drop pump — both are real, evidenced options | `[M]` (§11.4) |
| Melodic re-entry / variation from breakdown | The breakdown's dedicated pad/atmospheric element drops out at the Drop by definition (gated to the breakdown span) | `[M]` |

**Implementation priority**: the gradual hat/perc reintroduction is the
single most actionable, best-evidenced structural change this document
proposes for Drop rendering — today's engine (per the existing two-mode
Drop/Breakdown `RenderMode`) has no concept of a drop "assembling itself"
over its own first section; this would be new capability, not a parameter
tweak.

## 5. SOUND DESIGN grammar v2 (explicitly the weakest-evidenced tier)

Every sound-design claim gathered this pass is single-source (`LOW`
confidence) — none have any corresponding measurable dimension in this
project's MIDI/audio-onset corpus. Recorded as real, usable starting points
for whenever Serum preset-design/capture work resumes, not as validated
rules:

- **Lead**: free-running LFOs for motion, saw+noise layering, macro-mapped
  filter/LFO/reverb (Tunecraft Sounds)
- **Pluck**: short attack + fast decay, single envelope drives both
  amplitude and filter cutoff (MusicRadar/Future Music)
- **Pad**: 2-3 layered pads (bright/mid/low), filter cutoff automated over
  ~16 bars (Samplesound) — the 16-bar figure coincidentally matches the
  measured Break-body length, not evidence of a causal link
- **Bass**: two detailed Serum 2 oscillator/filter/envelope recipes (Mind
  Flux) — concrete enough to try as literal starting patches
- **Kick**: no concrete synthesis-parameter claims were gathered this
  pass (only sample-based/mix-processing claims, already in
  `melodic_techno_research.md` §4)

**This category should NOT be treated as implementation-ready.** Its
connection to the eventual Serum-preset-selection work
(`melodic_techno_research.md` §11.6, Capture-based workflow) is real but
indirect: these are technique *descriptions*, not presets, and this project
has no mechanism to synthesize a Serum preset from a text description of
oscillator settings — the Capture workflow (loading a real preset through
Serum's own UI, then capturing its actual state bytes) remains the only
concrete path, unchanged by this pass.

## 6. What is explicitly NOT decided by this document

- No `MusicState`/`RenderMode`/`BreakdownPhase` API shape — unchanged from
  the still-pending §11.4 proposal.
- No resolution on the pre-drop-pause discrepancy (§3 above).
- No filter-automation implementation (no such DSP capability exists in the
  engine today; this document only notes it as evidenced technique).
- No percussion-vocabulary-expansion decision (whether/when to move beyond
  PercA/PercB).
- No sound-design/Serum implementation of any kind — that tier is
  explicitly the weakest-evidenced and least actionable.
- No BassArchetype changes (the root/fifth/third correction describes
  what's *already* correctly reflected in `BassRhythmGrammar.h`'s measured
  table; the octave-drop-syncopation idea is a candidate for a future
  archetype, not proposed for immediate addition).

## 7. Suggested next step

Per the user's own priority ordering across this whole research effort:
review this document plus its two companions
(`youtube_melodic_techno_research.md`, `youtube_production_knowledge.json`)
and indicate which of the `HIGH`-confidence, `[M+Y]`-tagged rules above
should become the actual basis for the next `DrumEngine`/`BassEngine`/
Breakdown implementation pass. Nothing here should be built without that
sign-off.
