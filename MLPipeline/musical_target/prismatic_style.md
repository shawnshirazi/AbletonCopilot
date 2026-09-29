# PRISMATIC by Tiësto: tracklist and style analysis (as of 2026-09-29)

## 0. How the data was collected (read this first)
- The sandbox egress proxy **blocks every music site** (1001tracklists, Beatport, Tunebat, Mixcloud, Apple Podcasts, Spotify, Wikipedia, tranceattack, trance-family and others). Both curl and WebFetch returned 403 or EGRESS_BLOCKED.
- **All data came from the WebSearch tool.** It returns a model-written summary of indexed pages (mostly 1001tracklists.com, tranceattack.net, beatport.com, mixgraph.io, shazam). Each value was therefore seen second-hand. Treat single values as "probably right" (roughly 90%), not verified. Spot checks were consistent: chart positions, labels and release dates matched across results.
- Files:
  - `tracklists.json`: 39 episodes and 691 track entries. 30 tracklists are full and 9 are partial.
  - `tracks_features.csv`: 71 rows. 66 are usable for statistics. Five are "original-version reference" rows with `version_match=no` and are excluded.
  - `analysis_output.txt`: raw output of `analyze.py`.

## 1. The show
- **Name:** `PRISMATIC` (styled "PRISMATIC by Tiësto"). It is Tiësto's weekly one-hour radio show and podcast.
- **Start:** Episode 001 aired on **2 January 2026** (Fri/Sat). It replaced **CLUBLIFE**, which ran for about 18 years. The show airs weekly on **Radio 538** (Saturday midnight) and is syndicated, for example on AH.FM. It is published as a podcast (the Apple Podcasts "PRISMATIC" feed, id 251507798, appears to be the renamed Club Life feed), on Mixcloud (/Tiesto/prismatic-by-tiësto-NNN) and on YouTube. Tiësto also keeps a "PRISMATIC by Tiësto" Spotify playlist.
- **Name origin:** PRISMATIC was first his live show concept (Mexico City in June 2025, Pyramids of Giza in December 2025). That concept marks his "return to trance". Beatportal describes it as "a mix of all the different Tiësto styles… trance, progressive house, slap house, bigroom…".
- **Episode count:** **039 is the latest (2026-09-26)**, so 39 episodes exist, not 37–38. Two are specials: **021** (a 29-track "Mainstage/Trance" set that looks like a live recording) and **031** (Ibiza Special).
- **Format:** about 58–60 minutes, **about 20 tracks per episode, so roughly 3 minutes per track**. Tracks are heavily mixed and edited, not played in full.
- **Sources:** edmidentity.com (2026-01-07), edmtunes.com (2026-01), weraveyou.com (2026-01), edmli.com (2026-01-05), beatportal.com ("Tiësto Embraces His Trance Roots…"), and the 1001tracklists source page `/source/4hd977c/prismatic/`.

## 2. Tracklist coverage
- **Full lists (30):** 001, 002, 003, 005, 006, 007, 008, 009, 010, 012, 014, 015, 017, 018, 019, 020, 024, 025, 027, 028, 029, 030, 031 (17 of 18), 032, 033, 034, 035, 036, 037, 038.
- **Partial lists (9):**
  - 004: 7 of 21
  - 011: 3 of 20
  - 013: 14 of about 20
  - 016: 15 of 21
  - 021: 5 of 29
  - 022: 7 of 19
  - 023: 12 of 20
  - 026: 16 of 20
  - 039: 10 of 19
- Labels were only shown for some episodes (003, 005, 009, 020, 024, 034).

## 3. Results

### 3.1 Genre (1001tracklists episode tags)
Every episode is tagged **Trance**. The co-tags are:

| Co-tags | Episodes |
|---|---|
| "Dance/Electro Pop, Trance" | 19 |
| "Trance" only | 9 |
| "Trance, Melodic House & Techno" | 5 |
| "Dance/Electro Pop, Trance, MH&T" | 2 |
| "Trance, Techno" | 1 |
| "Trance, Techno, MH&T" | 1 |
| House | 1 |
| Mainstage | 1 |

### 3.2 Per-track genre (66 tracks with data)
| Genre | Tracks | Share |
|---|---|---|
| **Trance** (Beatport "Trance (Main Floor)" reported or inferred; includes Hard Trance, Uplifting, Progressive and neo-trance) | **43** | **65%** |
| **Melodic House & Techno** | **12** | **18%** |
| Techno (Peak Time) | 3 | 5% |
| Other: Mainstage, Hard Dance/Neo Rave, Bass House, Indie Dance, House, Latin Electronic, Dance/Pop | 8 | 12% |

- 29 of the 43 trance genres are Beatport-reported. The rest are inferred from label, artist and description, and the CSV marks which is which.
- The sample leans slightly toward well-indexed trance releases. The "Dance/Electro Pop" tag on 1001TL mostly marks vocal Musical Freedom/Spinnin' trance singles, not pop.

### 3.3 BPM (n = 66, excluding one doubtful 90-BPM value)
- **Mean 137.7, median 138.5, range 121–155.** The distribution is **bimodal**:

| BPM range | Tracks |
|---|---|
| ≤129 | 16 (almost all MH&T, house or indie) |
| 130–135 | 8 |
| 136–139 | 12 |
| **140–144** | **14** |
| **145–149** | **12** |
| 150+ | 4 |

- **Trance subset (n = 43): median 140, mean 141.6.** There are two trance clusters:
  - **138–144:** "classic/uplifting/progressive" trance (Cosmic Gate, Thrillseekers, Durand, Daxson, Ottaviani, Ferry Corsten, Bryan Kearney, CIElll, Gareth Emery, KE-YEN, York).
  - **145–150:** "neo-rave / hard trance / hard-house trance" (KI/KI 148, Funk Tribu 148, Benleh 146, Ben Gold "Blackcard" 146, Pegassi 146, DEVERAUX 145, CIS 147, Marlon Hoffstadt 147, Olly James 147, NYCO about 150, Lilly Palmer 150, Tiësto's "Voicemail" remix 150).
- **MH&T subset (n = 12): median 126** (121–133).
- Tiësto's own current singles: "Bring Me To Life" 131, "Don't Lose Your Head" 134, "Echo Sax Finale" 134, "Voicemail" remix 150.

### 3.4 Key (n = 63 with key)
- **Minor 43 (68%), major 18 (29%)**, and 2 with the mode unknown.
- The most common minor tonics are **G minor (7) and F minor (7)**, then D minor (5), then B♭, A and C♯ minor (4 each).
- The minor-key share is typical of trance.

### 3.5 Most-played tracks (normalised artist + title, all versions)
- **4 plays:**
  - Tiësto & Brieanna Grace – *Beautiful Places*
  - Tiësto ft. Olivia Sebastianelli – *Don't Lose Your Head*
- **3 plays:**
  - KUKO – *Voicemail* (the original once, then the Tiësto Remix twice)
  - Benleh – *Keep Going*
  - Bryan Kearney & Modeā – *Ready To Fly*
  - Tiësto ft. BT – *Love Comes Again* (three different 2026 remixes)
  - Tiësto & Caleb Arredondo – *Echo Sax Finale*
- **2 plays (about 35 tracks), for example:**
  - Faithless – *Insomnia* (Disclosure 2025 Edit)
  - Ben Gold – *Blackcard*
  - KI/KI – *5AM* and *Going Existential In The Rave*
  - CIElll – *Tethered*
  - JENO & Wempe – *Contra*
  - LAWTON & Deckers – *Los Retratos*
  - Lumine – *Here For Us* and *who tell em*
  - S3PPA – *Inner Peace*
  - KELLAR & Paige Tomlinson – *Take Me Back*
  - Nedea & Bryan Kearney – *Back Once Again*
  - Mirage – *Remember The Future*
  - CamelPhat & Volkoder – *Unique Moment*
  - NYCO – *Some Love*
  - DEVERAUX – *You Got Me Acting Crazy*
  - CIS – *He The Best*
  - KE-YEN – *YOUR WHOLE LIFE IS JUST A DREAM*
  - Swedish House Mafia & Lykke Li – *Happiness Is So Sad*
  - Cosmic Gate – *Exploration Of Space*
  - Everything But The Girl – *Missing* (two different bootlegs)
  - Trance Mums – *Forever*
  - TWOFACED – *Yearning*

### 3.6 Most frequent artists
Appearances across the 691 entries, with collaborations split:
- **Tiësto: 25** as artist, plus 5 as remixer or editor
- 7 each: KELLAR, Lumine, Bryan Kearney
- 6 each: S3PPA, Pegassi, DOREY, TITVS, dj try
- 5 each: Ben Gold, IDEMI, DJ HEARTSTRING, SWIM, KI/KI, Caroline Roxy, Modeā, LAWTON
- 4 each: BLR, Amber Revival, Entasia, Cosmic Gate, CIElll, KETTAMA, Fahlberg, Daxson, K.ONE, TWOFACED, Giuseppe Ottaviani, Amy Wiles, Headhigh, Wempe, CIS, NYCO

### 3.7 Labels (where shown)
- **Musical Freedom (Tiësto's label): 13**
- Armada (and sub-labels): 6 or more
- Black Hole / Wake Your Mind: 6
- Diynamic: 4
- Enhanced Progressive: 2
- Steel City Dance Discs: 2
- slash/label: 2
- Insomniac: 2
- Singles: FSOE, Anjunabeats, Drumcode, KNTXT-type techno, This Never Happened

The show is clearly also a Musical Freedom promotion vehicle. Musical Freedom tracks include Lumine, CIElll, JENO & Wempe, Benleh, DEVERAUX, CIS, Karl Mac, Saxxon, KE-YEN, Lisa Korver and Bryan Kearney & Modeā.

### 3.8 Tiësto's own material
- **New singles:**
  - Bring Me To Life (w/ FORS)
  - Beautiful Places (w/ Brieanna Grace)
  - Don't Lose Your Head (ft. Olivia Sebastianelli)
  - Echo Sax Finale (w/ Caleb Arredondo)
  - Lost In The Ocean
  - RVN (021)
- **His remixes and edits:**
  - KUKO – Voicemail
  - Flourish & Wolf Cutt – The Sign
  - Paul Oakenfold – Southern Sun (2025)
  - York – Reachers Of Civilization (edit)
- **Classic catalogue in new remixes:**
  - Theme From Norefjell (Ørjan Nilsen)
  - Ten Seconds Before Sunrise (Ciaran McAuley)
  - Forever Today (Sean Tyas and Robbie Seed)
  - Love Comes Again (Emre Gulmez & Klyde Jaxx, Zarka, Innēr Sense)
  - Urban Train (Talla 2XLC)
  - Nyana (Cameron Mo & Seegmo)
  - Flight 643 (Krevix)
  - I Will Be Here (Wolfgang Gartner)
- Overall, **about 1–2 Tiësto-related tracks per episode**, usually the opener.

### 3.9 Recurring programming traits (observed in the lists)
- **Nostalgia and classic-rework slots.** Almost every episode includes a remix, edit or bootleg of a 90s or 2000s classic. Examples: Greece 2000, Insomnia, Café del Mar, Born Slippy, Missing, Stereo Love, Rapture, H2, Summer (Da Hool), Opus, Southern Sun, Solarcoaster, Loops & Tings, Destination, Amsterdam, The Prophet, Saltwater, As The Rush Comes, Schiller's Dream Of You, Walking On A Dream, Shooting Stars, I'm Not Alone, Young & Beautiful.
- **Vocals are frequent.** Most tracks are female-topline trance singles.
- **Running order (inferred from timestamps):**
  - The opener is usually a Tiësto or Musical Freedom vocal track.
  - A mid-show dip into 124–129 BPM MH&T or house is common, for example in 009, 020 and 024.
  - Episodes tend to close on the faster 145–150 neo-rave or hard-trance material, for example KI/KI, DJ Dings & Tim Rausch, Pegassi and NYCO.

### 3.10 Shift over the run
Based on episode tags and track content, which is weak evidence:
- **Episodes 001–010:** more classic and progressive trance and MH&T. Examples: Cosmic Gate, Genix/Anjunabeats, FSOE, Anyma/Ottagon-type melodic techno, Lane 8 remix, Adam Beyer.
- **Episodes 012–039:** the **neo-rave / hard-house-trance lane (145–150 BPM)** and Musical Freedom signings grow. Examples: KI/KI, Benleh, DJ Heartstring, CIS, DEVERAUX, Pegassi, Trance Mums, Britney Speed, Lumine, and Tiësto's own 150-BPM "Voicemail" remix in August.
- **Episodes 036–039:** add a few more techno/MH&T-tagged cuts again (Eli Brown, Kevin de Vries, HACKETT, Einmusik).
- The "Trance" backbone never changes.

## 4. Style specification for a generator
Each point is tagged **[E]** for evidence (a cited source or the measured data above) or **[I]** for my inference or general genre convention.

### 4.1 Target profiles (weights from §3)
| Profile | Weight | BPM | Key | Notes |
|---|---|---|---|---|
| **A. Modern melodic/uplifting "Prismatic" trance** | ~40% | 138–144 (default **140**) | minor (G/F/D minor) | vocal or big supersaw lead [E: BPM/key data] |
| **B. Neo-rave / hard-house trance** | ~25% | 145–150 (default **147**) | minor, sometimes major | offbeat saw bass, acid, gated pads, rave stabs, pitched-up 90s/2000s vocal hooks [E: data; Funk Tribu/KI/KI/Benleh descriptions] |
| **C. Melodic house & techno / progressive** | ~18% | 124–129 (default **126**) | minor | arps, darker pads [E: data] |
| D. Other (tech/peak techno 128–132, big-room/mainstage 128–140, indie dance, bass house) | ~15% | – | – | variety slot [E] |
| Tiësto-own "Bring Me To Life" type | – | 131–134 | – | progressive trance, vocal, big break [E: beatportal/djmag description, BPM data] |

### 4.2 Arrangement
- **[E] Myloops "Uplifting Trance Arrangement" and "Anatomy of a Trance Arrangement":**
  - Sections are multiples of 8 bars, usually 32.
  - Section lengths:

    | Section | Bars |
    |---|---|
    | Intro | 32–64 (kick and top loop only for bars 1–16; bass enters at 17–32, no chords yet) |
    | Groove build | 32 |
    | First break | 16–32 |
    | Main breakdown | 32–96 (classically 32 = 4×8: kick out, sustained pad, then a pluck or piano teases the theme) |
    | Build | 16–32 |
    | Main drop | 32–64 |
    | 2nd break | 16–32 |
    | 2nd drop | 32 |
    | Outro | 32–64 |

  - Extended mixes at 136–140 BPM run about 7–8 minutes.
- **[E] Observed lengths:**
  - Recent Beatport extended mixes of show tracks run **4:46–6:58**:

    | Track | Length |
    |---|---|
    | CIElll – Tethered | 5:00 |
    | Eli Brown – Damaged | 4:48 |
    | Gareth Emery – Under The Sky | 5:23 |
    | Solarcoaster remix | 5:48 |
    | Rubicon | 6:47 |
    | On The Beach | 6:58 |
    | Stomp Your Feet | 5:34 |
    | Bryan Kearney – Forevermore | 5:36 |

  - Newer trance is shorter than the classic 8-minute template.
  - In the radio show each track plays for only about 3 minutes.
- **[I] Recommended template at 140 BPM** (1 bar = 1.714 s), about 5:30 in total:

  | Section | Bars | Bar range |
  |---|---|---|
  | Intro | 16 | 1–16 |
  | Groove with bass | 16 | 17–32 |
  | Drop 1 (half-energy, riff teased) | 32 | 33–64 |
  | Breakdown | 32 | 65–96 |
  | Build | 16 | 97–112 |
  | Main drop | 32 | 113–144 |
  | Short break | 16 | 145–160 |
  | Drop 2 or outro | 32 | 161–192 |

- **[I] Neo-rave (profile B) tracks** are shorter and more loop-driven:
  - The intro is 16 bars.
  - Breakdowns are 16–32 bars, often built on a vocal hook.
  - The drop is 32 bars and repeats with variation.
  - Total length is about 4–5 minutes at 147 BPM.

### 4.3 Drums
- **[E] Hard/modern trance tutorial** (theproducerschool, allanmorrowstudios, myloops):
  - A relentless 4/4 kick on every beat.
  - Clap or snare on beats 2 and 4.
  - **Open hi-hat on every 8th-note offbeat**.
  - A punchy kick, high-passed or cut where it would clash with the bass.
- **[I] Additional detail:**
  - 16th closed hats, lightly swung or not at all.
  - Ride cymbal on the quarter or 8th in drops.
  - Shaker or percussion loop.
  - A crash on bar 1 of every 8- or 16-bar phrase.
  - Fills in the last bar of each 8-bar phrase.
- **[E] Myloops "The Last 16 Bars":**
  - The snare roll accelerates through the build (1/4, then 1/8, 1/16, 1/32).
  - It is high-passed at 300–400 Hz, with a rising pitch envelope and a rising reverb send.
  - Filter, pitch and rhythmic-density curves all peak on the same downbeat.
  - The bar before the drop is mostly empty.
  - Two bars before the drop the kick is cut.
- **[I] Profile C (MH&T):**
  - Kick on every beat.
  - Clap on 2 and 4, or a rimshot pattern instead.
  - Sparser hats.
  - More syncopated percussion.

### 4.4 Bass
- **[E] Five patterns** (theproducerschool "5 essential bass patterns"):

  | Pattern | Description | Use |
  |---|---|---|
  | **Offbeat "donk"** | one note on each 8th offbeat | under vocals and leads |
  | Double offbeat | – | – |
  | **1/16 rolling** | note on every 16th, sidechain mandatory | the classic trance bass |
  | **Octave jump** | 1/8 notes alternating between octaves | euphoric sections |
  | Syncopated groove | – | – |

- **[E] Funk Tribu-style hard trance at 150 BPM:** a **clean saw-wave 1/8 offbeat bass**, two octaves down, with no oscillator randomness.
- **[E] Rolling bass at 145 BPM:** decay of 60–80 ms; each 16th is about 103 ms long.
- **[E] Sidechain:** 4–6 dB gain reduction, 1–5 ms attack, 150–200 ms release at 138 BPM.
- **[I] Mapping for the generator:**
  - Profile A: rolling 16ths ("kick, then bass on the 2nd, 3rd and 4th 16ths", following the chord root) or octave-jump in drops.
  - Profile B: offbeat or double-offbeat.
  - Profile C: a sustained or syncopated sub with a pluck.
  - The bass follows the chord root and changes with the chords, usually every 1 or 2 bars.

### 4.5 Harmony
- **[E] Myloops and unison.audio:**
  - Minor keys dominate.
  - Core progressions are **i–VI–III–VII** (Am–F–C–G, "classic descending lift", used for the main breakdown), **i–VII–VI–VII** and **VI–VII–i**.
  - Minor chords often carry an added 9th.
  - Voicings sit between A3 and C5 with a nearly static top voice, while the sub owns the root below 200 Hz.
- **[I] Harmonic rhythm:**
  - One chord per bar in drops.
  - One chord per 2 bars in breakdowns.
  - 4- or 8-bar loops.
- **[I] Suggested defaults:**
  - Keys: G minor, F minor or D minor (the most common tonics in the data).
  - Progressions: i–VI–III–VII and VI–VII–i–i.

### 4.6 Lead, hook and sound design
- **[E] Theproducerschool "Hard House & Trance: from 90s rave to modern revival":** the core preset set is:
  - saw bass
  - perfect-fifth chord stab
  - classic trance supersaw pluck
  - stuttering sweepy lead (detuned supersaw with LFO amplitude gating)
  - trance-gate pad
  - The "hoover" and new-beat stab come from the Roland JX-3P (Human Resource – Dominator appears in episode 021).
- **[E] Funk Tribu breakdown:**
  - A gritty **phase-distorted lead**.
  - An **acid 303** line (filter resonance and accent).
  - A **trance-gate vocal pad**.
  - The layers alternate rather than stack.
- **[E] "Bring Me To Life" description** (beatportal, djmag): "soaring synths, emotional vocal, high-impact break… atmosphere gives way to full vocal… characteristic trance pad… progressive-trance drop".
- **[E] Musical Freedom's Benleh "Keep Going":** "melodic spirit of 90s trance and early-2000s eurodance with the driving rhythms of contemporary fast techno", 146 BPM, A♭ minor.
- **[I] Lead riff shape:**
  - A 1- or 2-bar 8th/16th-note riff built on chord tones plus the 9th.
  - Call-and-response across 4 bars.
  - The anthem lead enters an octave up in the main drop, with supersaw plus pluck layering.
  - Arpeggiated 16th plucks under the breakdown.
  - An acid 16th line as a counter-melody in profile B.

### 4.7 FX and energy
- **[E]:** white-noise risers, pitch risers, filter-opening automation, reverb swells, snare roll, a one-bar hole before the drop, and a downlifter or impact on bar 1 of the drop (myloops).
- **[I]:** cut the kick for the whole breakdown and bring it back on the drop. The low-pass filter on the lead opens over the 16-bar build.

## 5. Things I could NOT verify
- **No page was opened directly.** Every tracklist, BPM, key and genre came through search-engine summaries, which can contain transcription errors. The most likely errors are in track order, remix names and key or mode. The summariser also sometimes mixed versions:
  - "Beautiful Places" was reported at 90 BPM, which is doubtful.
  - NYCO "Some Love" was reported at 75 BPM; I doubled it.
  - KUKO "Voicemail" original was reported at 112 BPM.
- **9 episodes are incomplete.** Episode 021 is barely covered, with 5 of 29 tracks.
- **Features were found for 66 tracks (plus 5 original-version references), short of the target of 80 or more.** About 25 lookups failed, mostly on very new or obscure 2026 releases, for example LAWTON & Deckers, BLR, Trance Mums and Nedea & Bryan Kearney.
- **About 40% of the genres are inferred**, not taken from Beatport. The CSV `genre_source` column marks them.
- **The show airtime and podcast feed details** come from news articles, and I could not load the pages to confirm them. The Apple Podcasts ID is inferred.
- **The style shift over the run** is a qualitative reading of partial data, not a significance test.
- **Production-technique sources** are generic genre tutorials (myloops.net, theproducerschool.com, unison.audio, allanmorrowstudios), not analyses of the specific Prismatic tracks.
