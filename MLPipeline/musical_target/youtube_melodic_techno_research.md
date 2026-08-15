# YouTube / Production-Knowledge Melodic Techno Research

A companion to `melodic_techno_research.md` (the primary-source document built
from the user's own sample library, MIDI corpus, and real Ableton
arrangements). This document adds a second, independent evidence stream:
publicly available production-tutorial, technique-breakdown, and interview
content, with an emphasis on YouTube. Every claim is machine-readable in
`youtube_production_knowledge.json` (41 claims); this document is the human-
readable narrative and methodology write-up for that file.

**Status: research only.** No `DrumEngine.cpp`, `BassEngine.cpp`,
`MelodyEngine`, `MusicState`, `PluginEditor`, or any other engine/UI code was
touched to produce this document or the accompanying JSON. Nothing here has
been implemented.

---

## 0. Methodology, tools used, and access limitations (read this first)

**Tools available this session**: `WebSearch` (a search engine, returns
titles/URLs/synthesized snippets) and `WebFetch` (fetches a URL and converts
its HTML to text/markdown for a model to read). Both are legitimate,
publicly-available access methods - no login bypass, no rate-limit
circumvention, no scraping tool beyond a standard HTTP GET.

**What was tested and what actually works, established empirically before
committing to a methodology (not assumed):**

1. **Fetching a YouTube watch page directly** (`youtube.com/watch?v=...`) via
   `WebFetch` was tested and **returns only page chrome** (footer navigation,
   legal links) - no title, description, chapters, or captions. YouTube's
   watch page is a JavaScript-rendered single-page app; a plain HTTP GET
   (which is all `WebFetch` does) cannot see anything the JS would have
   rendered. **Confirmed empirically, not assumed.**
2. **A third-party YouTube-transcript site** was tried once
   (`youtubetotranscript.com`) and returned **HTTP 403 Forbidden** - the site
   itself is blocking automated access. Per the explicit instruction for this
   pass ("do not scrape/download YouTube content in a way that violates
   YouTube's terms... if a transcript isn't legitimately accessible, record
   that rather than inventing information"), **this was not retried against
   other similar sites** - a 403 is the site declining automated access, and
   working around that would cross from "fetching a public page" into
   "circumventing an access restriction," which this pass explicitly avoids.
3. **YouTube's own official oEmbed endpoint** (`noembed.com/embed?url=...`,
   itself just a public proxy for YouTube's documented oEmbed API - the same
   mechanism any website uses to embed a YouTube video preview) reliably
   returns a video's **title, channel name, and channel URL**. This is
   legitimate, YouTube-sanctioned public metadata, explicitly designed for
   third-party consumption. **This works and was used throughout** to confirm
   real channel names.

**Conclusion, applied consistently to every claim in the JSON**: this session
could not legitimately obtain YouTube video descriptions, chapters, or
caption/transcript text directly. Every YouTube-video claim in this corpus is
therefore one of:

- **`video_metadata_only`** - the video's real title/channel is confirmed
  (via oEmbed), but no content claim is made about what's actually said in
  it. Used when a video is cited as evidence that a topic/technique/producer
  pairing is being actively taught, without claiming to know the specifics.
- **`search_snippet_synthesis`** - a WebSearch call returned a synthesized
  answer that appears to draw on indexed page content (Google's own
  crawl/index, which may include a video's description or an article
  discussing it) - attributed to the underlying source, not presented as a
  verbatim transcript quote.
- **`article_direct_text`** / **`article_direct_quote`** - `WebFetch` against
  a normal (non-YouTube, server-rendered) article or blog post succeeded
  fully, and the claim is taken directly from that page's real text. Several
  of these accompany or summarize a specific YouTube video (noted per claim).

**No claim in this corpus is presented as a verbatim YouTube transcript
quote.** Where a real quote appears, it is quoted by a written article that
did the interview/attribution itself (e.g. MusicRadar's Layton Giordani
interview), not lifted from a video's captions.

This is a real constraint on this pass's depth versus what the user asked
for ("captions/transcripts... if available") - transcripts were not
available through any method this session could confirm as legitimate. The
corpus below is built from real production-technique articles (several of
which are themselves companion pieces to specific tutorial videos or use
identical language to what's being taught in them), real interview
quotes, and confirmed-real video titles/channels/topics - a genuine
production-knowledge corpus, just not a transcript-level one.

---

## 1. Confidence tiers (as requested, applied per-claim in the JSON)

- **HIGH** — appears across multiple independent production sources **and**
  is corroborated by this project's own measured corpus (the real `.als`
  arrangements, `drum_grammar.json`, `bass_grammar.json`/
  `BassRhythmGrammar.h`, or the MIDI corpus already analyzed in
  `melodic_techno_research.md`).
- **MEDIUM** — appears across 2+ independent sources but isn't confirmed by
  this project's own corpus (either the corpus has no data on that
  dimension, or it wasn't checked this pass).
- **LOW** — a single producer's/channel's stated technique or opinion, or a
  single tutorial's specific implementation choice — not cross-validated.

Of the 41 claims recorded: **14 HIGH, 12 MEDIUM, 15 LOW.** The HIGH tier is
concentrated almost entirely in claims that could be checked against real
numbers already sitting in this repository (`bass_grammar.json`,
`drum_grammar.json`, the `.als` clip-timing data) — which is exactly the
"don't treat one YouTuber's opinion as a genre rule" bar the user asked for:
nothing reaches HIGH on source-count alone, only on source-count **plus**
independent primary-source measurement.

---

## 2. Drums — findings

**Kick** (HIGH): standard pattern is four-on-the-floor, kick on every beat.
This is not a debatable point in any source found, and it is an exact match
to `drum_grammar.json`'s `KICK.on_beat_fraction = 1.0`,
`mean_onsets_per_bar = 4.0` (17 real files, 68 onsets). A secondary,
genre-flavor point (Myloops, [MEDIUM/observation]): melodic techno kicks
often carry a longer tail (300-500ms, energy around 60-80Hz) than tech-house
kicks, and that tail is described as the *reason* the bassline has to work
around it rhythmically — this is a plausible causal explanation for an
effect this project independently measured (`bass_grammar.json`'s
`on_kick_fraction = 0.2126`, well under the 25% uniform baseline), so it's
tagged HIGH as a corroborated *pattern* even though the causal mechanism
itself isn't independently testable from MIDI data alone.

**Hi-hats** (HIGH): Attack Magazine's "Beat Dissected" breakdown of a real
reference track states primary hats are programmed on the offbeat, with
velocity reduced specifically on offbeat hits for a more organic feel. This
matches `drum_grammar.json`'s `HAT.eighth_offbeat_fraction = 0.502` almost
exactly — the single largest bucket in the real corpus, though not an
absolute majority (27% land on-beat, 23% land on a weak 16th) — so "hats are
mostly on the offbeat, not exclusively" is the corrected, more precise
version of the generic claim.

**Claps/snares** (MEDIUM/LOW, and one real correction): several sources
describe *syncopated* clap placement (a single hit every 2 bars, a double
hit every 4; or a clap only on the second kick of the bar for darker
styles) as a defining melodic-techno/dark-techno technique. Checked against
`drum_grammar.json`: the real corpus's `CLAP.on_beat_fraction = 0.841` shows
the **on-beat pattern is dominant** (26 files, 176 onsets), with
`adjacent_bars_identical_fraction = 0.808` (claps mostly repeat every bar).
The syncopated variants described in the tutorials are real, named, and
worth keeping as an option, but they are a **named minority technique
(~16%)**, not the corpus default — this is exactly the kind of thing the
user asked to catch ("don't treat one YouTuber's opinion as a genre rule").

**Percussion** (HIGH): Attack Magazine's conga/tom/maracas/rim breakdown
(syncopated 2-bar cycles, dense 16th-note maracas with velocity variation,
rim shots nudged forward off the kick's transient) matches
`drum_grammar.json`'s `PERC` role closely: `mean_onsets_per_bar = 10.487`
(dense), `weak_16th_fraction = 0.487` (the dominant bucket — mostly
syncopated placement, not on-beat), `adjacent_bars_identical_fraction =
0.57` (bars do vary, confirming the "some variation between velocities/
hits" language rather than a static loop).

**Ghost notes / velocity** (LOW): a generic (not melodic-techno-specific)
claim that ghost notes sit around velocity 35-50 and that darkening their
tone (not just lowering volume) reads as more realistic. Not
melodic-techno-specific, and this project hasn't independently re-measured
a ghost-note velocity range from its own corpus this pass — kept as a
single-source LOW note for a future velocity-focused pass.

**Swing** (LOW): one real data point — 50-55% swing at 120-130 BPM — comes
from Attack Magazine's *Dark Berlin Techno* piece, which is an adjacent
genre, not melodic techno specifically, and this project has never measured
swing from its own corpus. Flagged explicitly as adjacent-genre evidence,
not a melodic-techno data point.

## 3. Bass — findings

This is where the cross-referencing paid off most clearly, because
`bass_grammar.json`/`BassRhythmGrammar.h` already contains hard numbers from
a real 20-file, 856-note groove-subset corpus.

**Kick avoidance** (HIGH, 2 independent sources): both Attack Magazine
("leave the first 16th of every beat empty") and Mind Flux ("leave out
notes where the kick hits") state the same rule in different words. This
project's own corpus: `on_kick_fraction = 0.2126`, meaningfully below the
25% a uniform-random placement would produce. Two independent tutorials and
one independent measurement, all agreeing — the strongest-supported claim
in the whole corpus.

**Register/octave** (HIGH): Attack Magazine's explicit instruction to keep
the bass in one tight octave with the root dominant matches
`bass_grammar.json` almost exactly: `mean_pitch_range_semitones = 2.15`
(the real groove-subset's own internal range is *even tighter* than "one
octave"), and the root (0 semitones) accounts for `74.65%` of all real
onsets. This also matches this project's own prior finding
(`melodic_techno_research.md` §11.5) that individual real basslines commit
to one narrow register regardless of key.

**Root/fifth usage — a real correction, not a confirmation** (MEDIUM):
Myloops states the bassline should be "mostly root and fifth." Checked
against the actual measured `pitch_offset_distribution`: after the root
(74.65%), the next most common intervals are **+3 semitones/minor third
(5.14%)** and **-2 semitones (4.91%)** — the fifth (+7 semitones) is only
**3.74%**, *less* common than the minor third. The generic "root and fifth"
framing undersells how minor-third-forward the real corpus actually is.
This is recorded as a correction the same way two claims in the previous
research pass were self-corrected against wider evidence — the point of
cross-referencing is to catch exactly this kind of thing.

**Note length** (HIGH, 3 independent sources): Attack Magazine and two Mind
Flux articles all independently describe short, punchy bass notes (low
sustain, fast attack/decay, audible transient). `bass_grammar.json`'s
`mean_note_len_steps = 1.3179` (roughly 1.3 sixteenth-note-steps average) is
a direct, unambiguous match. Three sources plus a direct measurement is the
strongest-evidenced single claim in this entire document.

**Sidechain** (MEDIUM): described as "non-negotiable," ~80-150ms release at
122-124 BPM. This is an audio-domain mix technique invisible to a MIDI-only
corpus, so it can't be cross-checked with the tools available — real,
specific, but unverifiable against this project's own data.

**Layering** (HIGH): the generic "sub layer + mid/character layer, optional
higher 'top bass'" description matches a real structure this project
already found in a primary source — Track 2's actual reference arrangement
has "Rolling Bass 1/2" (a persistent layer), "Sub Bass," and "Bass Pluck"
coexisting with independently different re-entry timing (§11.1 of the main
research doc) — a genuine multi-layer bass architecture in a real song, not
just tutorial advice.

**Octave-drop syncopation** (LOW): "drop every offbeat note an octave
lower" is a specific, real, single-source technique with no equivalent
dimension in `BassRhythmGrammar.h` today (the archetype system has no
per-position octave-displacement concept). Worth a future dedicated
measurement pass, not implemented or assumed correct here.

## 4. Breakdown — findings

**Removal order** (HIGH): "kick first, then bass reduces, pad/lead comes
up" is stated generically by Myloops and is **directly and independently
confirmed** by this project's own primary-source clip-level measurement
across three real arrangements (kick fully silent through the entire
measured break in all 3; a dedicated pad/atmospheric element exposed
specifically during that span in all 3 — see `melodic_techno_research.md`
§10.2/§11.1). This is the single most solidly evidenced claim in the
breakdown category.

**Breakdown length** (HIGH): Yuval Miller's stated 16-32 bar range for
breakdowns/build-ups lines up with this project's own measured phase
lengths (Entry 4-8, Body 16-24, Development 4-8 bars — §11.4 of the main
doc) — generic guidance and primary-source measurement agree.

**Reverb/filter automation** (MEDIUM): the "+3 to +6dB reverb as the kick
drops" claim (Myloops) was already in the prior research pass and is
re-surfaced here from the same single source, not newly corroborated. The
"high-pass automate the whole mix 20Hz→200-400Hz over 16-32 bars" claim
(also Myloops) is a second technique from the same source. Both are
consistent *in spirit* with the independently-sourced Layton Giordani claim
about low-pass-filtering the bassline specifically during breakdowns
(different technique, same general "filter automation = tension" idea) —
kept at MEDIUM because reverb/filter automation isn't observable from this
project's clip-timing/MIDI data, so it can't be pushed to HIGH.

**Riser** (HIGH): the generic "riser = pitched-up synth marking a section
change" description is directly confirmed by this project's own §11.1
finding — a real "Riser" MIDI track AND a separate "Riser" audio track both
appear in the actual reference arrangement, active in exactly the same
8-bar pre-drop window where kick and a secondary bass layer also return.

**Pre-drop pause — flagged as NOT observed in the reference arrangements**
(MEDIUM): two independent sources (Yuval Miller, Breve Music Studios)
describe cutting drums or inserting a short silence/pause right before the
drop as a tension technique. This project's own bar-level clip analysis of
three real arrangements did **not** show a hard pause — kick either stays
off until the exact drop or returns 4-8 bars early as a pump, never a
silent gap. This may be a real technique operating at a finer time
resolution than bar-level clip boundaries can show, or it may simply not be
used in this project's specific three reference songs. Recorded honestly as
an open discrepancy, not resolved either way.

**"Keep subtle percussion during the breakdown" — partially contradicted**
(MEDIUM): Yuval Miller suggests keeping some rhythmic elements going during
a breakdown rather than going fully silent. All three of this project's own
measured reference arrangements show the *full* driving hat/percussion
groove going **completely silent** during the break body, not reduced. The
"expose a melodic/pad element" half of the claim is confirmed; the "keep
some percussion" half is not, in this project's own evidence.

## 5. Drop — findings

**Gradual reintroduction, not full-density-from-bar-1** (HIGH): this is the
headline finding of this whole document, because it comes from BOTH an
independent generic source and this project's own primary measurement,
agreeing with each other. Generic guidance describes elements being
reintroduced gradually after a drop hits rather than everything being
active immediately. This project's own §11.1/§11.3 finding is far more
specific and was itself a *correction* to an earlier single-instance
assumption: in 2 of 3 real reference arrangements, the full hat/percussion
groove doesn't return before the drop at all — it reintroduces gradually
**across the drop's own first 8-24 bars**. The drop's first bar, in the
majority of measured real cases, is NOT the fullest version of the groove —
it's kick + bass (+ whatever returned early in the pre-drop window), with
the hat/perc groove still assembling itself over the section that follows.

**Core bass returns exactly on the downbeat** (MEDIUM, primary-measurement-
only): recorded here as an internal finding not independently found or
contradicted by any external source searched this pass — flagged
explicitly so it isn't mistaken for something YouTube/article research
confirmed, when actually only this project's own arrangement analysis did.

## 6. Sound design — findings

All sound-design claims found this pass are **LOW confidence** — none of
them have any corresponding dimension in this project's own corpus (which
is MIDI/audio-onset data, not synthesizer-parameter data), and each comes
from a single source:

- **Lead** (Tunecraft Sounds): free-running LFOs for motion, saw + subtle
  noise layering, macro-mapped filter/LFO/reverb for real-time expression.
- **Pluck** (MusicRadar/Future Music): short attack + fast decay, one
  envelope driving both amplitude and filter cutoff together.
- **Pad** (Samplesound): layer 2-3 pads (bright/mid/low), automate filter
  cutoff over roughly 16 bars — the "16 bars" figure happens to line up with
  this project's own measured Break-body length, but that's a coincidental
  scale match, not evidence the two phenomena are related.
- **Bass synthesis specifics** (Mind Flux, 2 detailed articles): concrete
  Serum 2 oscillator/filter/envelope settings for a rolling bass patch —
  useful as a real, specific starting point if/when Serum preset-design
  work resumes, but a single source's implementation choice, not a
  cross-validated genre rule.

This category is honestly the weakest-evidenced in the whole document, and
that's a fair reflection of reality: synthesis technique is much harder to
verify or cross-reference than rhythm/timing, which can be measured
directly from MIDI.

## 7. Named producers/shows — findings

**Layton Giordani**: two independent sources (this pass's MusicRadar search
result, and the prior pass's It's The DJ "Dragonfly" breakdown citation)
both describe him automating a low-pass filter on the bassline during
breakdowns/buildups as a signature technique — independent corroboration
for the same producer, pushed to HIGH. Separately, MusicRadar's interview
provides real, directly-quoted workflow/gear context (Moog Voyager + TR-909
+ Eventide reverbs; "whether it's a kick or melody, it's always random")
— useful color, not a testable production rule. A third-party YouTube
tutorial (channel: Sam Smyers, confirmed via oEmbed) exists recreating the
lead synth from Giordani's "Act of God" — recorded as **evidence the
technique is being actively studied by other producers**, explicitly
labeled as a fan reconstruction, not Giordani's own stated method (its
actual content wasn't accessible — see §0).

**AFTR:HRS / Tiësto (VER:WEST)**: found a genuine direct quote from Tiësto
himself describing VER:WEST's identity ("melodic house music... a lot
deeper and more chill, a very different energy than Tiesto") — useful for
genre/energy positioning, not a specific production technique.

**Selected**: **explicit gap.** "Selected" is a common word and every
search attempt surfaced unrelated results (other artists being "selected"
by labels, etc.) rather than the specific producer. No disambiguated,
attributable content was found this session. Recorded honestly rather than
guessed at, per the user's own standing instruction throughout this
project.

**Tiësto's Prismatic Radio Show**: confirmed to be real (launched
2026-01-02, 30+ real YouTube episodes found), but it is a mix/radio show,
not a tutorial series — no production-technique content exists to extract
from it. Recorded as a gap, same as the previous research pass's identical
finding for this same show.

---

## 8. What this document explicitly does NOT do

- Does not implement anything. No engine, processor, or UI file was
  touched.
- Does not treat any single-source claim as a genre rule — see the
  confidence tiers and the two explicit corrections in §3/§4.
- Does not claim transcript-level access to any YouTube video's actual
  spoken content — see §0's disclosed tooling limitation.
- Does not fabricate a "Selected" or "Prismatic production technique"
  finding where none was legitimately found.

## 9. Grammar v2

See `melodic_techno_production_grammar_v2.md` for the proposed synthesis of
this document, the prior YouTube-independent research pass
(`melodic_techno_research.md`), and this project's own measured corpora,
into one combined grammar proposal — also not implemented, presented for
review.
