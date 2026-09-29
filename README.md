# AbletonCopilot

A JUCE audio plugin (VST3 / AU / Standalone, macOS) for writing **Melodic Techno** in Ableton Live.
It generates drums, bass and melody, plays them through its own synth, real samples and a hosted
Serum 2, and can compose a complete track from scratch as an Ableton Live Set.

## Generate a full track from scratch

The full-track generator (`Source/Engine/SongGenerator.h`) composes a complete ~6.5 minute
arrangement (200 bars at 124 BPM). The structure follows the three real reference arrangements
analysed in `MLPipeline/musical_target/melodic_techno_research.md`:

```
Intro 16 > Bass In 16 > Full Theme 16 > Drop 1 16 > Groove 16 > Pre-Break 8 >
Break 24 > Buildup 8 > Drop 2 32 > Melodic Break 8 > Drop 3 24 > Outro 16
```

It writes nine tracks, each split into one clip per section:

| Track | Content |
|---|---|
| Kick, Clap, Hats, Perc | measured-grammar drums (`DrumEngine`), section-aware: no kick in the breaks, a silent bar before the break |
| Bass | one 16-bar bass theme (`BassEngine` archetypes) shared by every drop, following the chord roots, F1-C3 register |
| Pad | sustained 7th chords with smooth voice leading |
| Arp | 16th-note chord arpeggio, 3 velocity tiers, varied every 4 bars |
| Lead | 2-bar call-and-response hook, first heard in the Break, then carried by Drops 2 and 3 |
| FX | impact (C1) on every drop, riser (D1) into every drop, downlifter (E1) into breaks |

Harmony is a diatonic minor progression (i-VII-VI-VII, i-v-iv-VII, i-VI-III-VII or i-iv-VI-v)
with 8 bars per chord. Drum notes use Drum Rack / General MIDI numbers (kick C1, clap D#1,
closed hat F#1, open hat A#1, perc C#1/A1).

Output:
- **`<title>.als`**: an Ableton Live Set with the tempo, named and coloured MIDI tracks, every clip
  placed in the Arrangement view, and a locator for every section. The tracks have no instruments
  on them, so drop your own (Serum 2, Drum Racks, samples) onto each track.
- **`<title>.mid`**: the same song as a type-1 MIDI file (one track per part, section markers). You
  can drag it into any DAW.

The Live Set uses the **Live 11.3 format**, which opens in Live 11 and Live 12 (Live refuses sets
saved by a newer version). It is built the way working open-source .als generators build theirs:
from a real set that Live saved (Ableton's own MIT-licensed test set, in `Tools/als_template/`),
editing only known fields. Checks run on every change:
- `Tools/als_template/validate_als.py song.als song.mid` checks the rules real Live sets follow:
  unique pointee ids and NextPointeeId, list ids, clip layout, clip slots per scene, tempo written in
  both places, and the note data compared with the .mid file.
- The file also passes the validator from the open-source
  [`ableton-als`](https://github.com/kevinkirsten/ableton-als) library, and
  [`als2mid`](https://github.com/twobob/als2mid) reads back every note identically.

To regenerate the embedded template after changing it: `python3 Tools/als_template/make_als_template.py`.

### From the plugin
Studio tab, **Generate Full Track (.als)**. Pick a folder. The song uses the plugin's selected key
and the host tempo, and gets a new seed on every click.

### From the command line (no JUCE or Xcode needed)
```bash
Tools/build_generate_track.sh
./build/generate_track --key Am --bpm 124 --seed 7 --out ~/Music
# options: --seed N  --key Am|F#m|C|Dm-dorian  --bpm 60-200  --progression 0-3
#          --bars-per-chord N  --out DIR  --no-als  --no-mid
```
The same seed and options always produce the same song.

## Layout

| Path | What |
|---|---|
| `Source/Engine/` | music engine with no JUCE dependency: theory, grid, arrangement timeline, drum/bass engines, **SongGenerator**, **AlsWriter**, **MidiFileWriter**, Gzip |
| `Source/Engine/tests/` | plain C++17 unit tests, run them with `Source/Engine/tests/run_all.sh` |
| `Source/` | JUCE plugin: processor, editor UI, Serum 2 hosting/automation, sample selection, stem export |
| `Source/Analysis/`, `Source/MusicTheory/` | mix analysis/advisors and the JUCE-side melody generator |
| `SourceMIDI/` | AbletonCopilotMIDI, a small companion MIDI-effect plugin |
| `Companion/` | Python helper (sample matching, drum stacks, AbletonOSC) |
| `MLPipeline/` | corpus analysis scripts and the research notes the generators are based on |

## Building the plugin
Open `AbletonCopilot.jucer` in Projucer, save to generate the Xcode project (`Builds/`, which is
git-ignored), then build in Xcode. The Engine code and its tests build without JUCE:

```bash
Source/Engine/tests/run_all.sh          # all tests
Source/Engine/tests/run_all.sh test_song_generator
```
