// Progressive techno song writer - SongStyle::ProgressiveTechno.
//
// The centrepiece is the arp, modelled on a reference loop the user picked
// ("Synth Arp Loop Yearn", D minor, 125 BPM, transcribed in
// MLPipeline/musical_target/progressive_arp_reference.md):
//
//   - a 5-step note CELL repeating against the 16-step bar (polymeter: the
//     low note lands on steps 0,5,10,15 / 4,9,14 / ... so the figure drifts
//     across the bar lines - measured pitch autocorrelation 0.54 at lag 5,
//     ~0 at lag 4)
//   - wide open voicings: low chord root, 5th, octave, 9th, then a top note
//     (D3 A3 D4 E4 F4 = Dm add9)
//   - extended chords, 2 bars each: i add9 -> i sus (top climbs F-G-A) ->
//     VI maj7#11 -> iv/VII 13 (Bb2 F3 D4 E4 A4, G2 D3 C4 E4 A4)
//   - the octave note alternates up an octave on every other cell (D4/D5)
//   - the cell keeps running through chord changes; each new cell picks up
//     the chord sounding at its first note.
#include "SongInternal.h"
#include "Theory.h"
#include <algorithm>
#include <array>
#include <cstdio>

namespace Engine
{
    namespace songdetail
    {
        namespace
        {
            constexpr double kStep = 0.25;

            struct ProgSection
            {
                const char*  name;
                MusicSection kind;
                int          bars;
            };

            // 176 bars = 5:38 at 125 BPM: a long, gradual intro, the arp
            // opening up over a 32-bar theme, a 32-bar breakdown where it is
            // exposed over the pad, a 16-bar build and a 32-bar main drop.
            const std::vector<ProgSection>& progressiveStructure()
            {
                static const std::vector<ProgSection> s = {
                    { "Intro",     MusicSection::Intro,          16 },
                    { "Groove",    MusicSection::Establish,      16 },
                    { "Theme",     MusicSection::Drop,           32 },
                    { "Breakdown", MusicSection::Breakdown,      32 },
                    { "Build",     MusicSection::BreakdownBuild, 16 },
                    { "Main Drop", MusicSection::FinalDrop,      32 },
                    { "Outro",     MusicSection::Outro,          32 },
                };
                return s;
            }

            // A chord = root scale degree + the 5 cell tones as scale steps
            // above that root (sorted, lowest = the root itself). `climb`
            // replaces the top tone in the chord's second bar (the reference's
            // F -> G -> A "yearning" top line); -1 = no change.
            struct ProgChord
            {
                int degree;
                int cell[6]; // scale steps above the root, ascending
                int count;   // 5, or 6 - the reference's last chord grows to a 6-note cell
                int climb;
            };

            struct ProgProgression
            {
                const char* name;
                const char* character;
                ProgChord   chords[4];
            };

            // Chord colours, as scale steps above the root:
            //   add9      {0,4,7,8,9}    root 5th oct 9th 10th      (D A D E F)
            //   sus11     {0,4,7,8,10}   ... 11th, climbing to 12th (D A D E G -> A)
            //   maj7#11   {0,4,9,10,13}  root 5th 3rd #11 maj7      (Bb F D E A)
            //   sus13     {0,4,10,12,15} root 5th 11th 13th 9th     (G D C E A)
            //   add9 (major chords use the same shape)
            const std::vector<ProgProgression>& progressions()
            {
                static const std::vector<ProgProgression> p = {
                    { "i-i(sus)-VI-iv", "yearning (the reference loop)",
                      { { 0, { 0, 4, 7, 8, 9 }, 5, -1 }, { 0, { 0, 4, 7, 8, 10 }, 5, 11 },
                        { 5, { 0, 4, 9, 10, 13 }, 5, -1 }, { 3, { 0, 4, 10, 12, 13, 15 }, 6, -1 } } },
                    { "i-VI-III-VII", "hopeful lift",
                      { { 0, { 0, 4, 7, 8, 9 }, 5, 10 }, { 5, { 0, 4, 9, 10, 13 }, 5, -1 },
                        { 2, { 0, 4, 7, 8, 9 }, 5, 11 }, { 6, { 0, 4, 7, 8, 9, 12 }, 6, -1 } } },
                    { "i-iv-VI-VII", "driving, melancholic",
                      { { 0, { 0, 4, 7, 8, 9 }, 5, -1 }, { 3, { 0, 4, 7, 8, 9 }, 5, 10 },
                        { 5, { 0, 4, 9, 10, 13 }, 5, -1 }, { 6, { 0, 4, 7, 8, 9, 12 }, 6, 11 } } },
                    { "VI-VII-i-i(sus)", "rising, anthemic",
                      { { 5, { 0, 4, 9, 10, 13 }, 5, -1 }, { 6, { 0, 4, 7, 8, 12 }, 5, -1 },
                        { 0, { 0, 4, 7, 8, 9 }, 5, -1 }, { 0, { 0, 4, 7, 8, 9, 10 }, 6, 11 } } },
                };
                return p;
            }

            int st(const Song& s, int degree) { return degreeToSemitone(degree, s.scale); }

            // Low (root) note of a chord in [G2, F#3) - the reference's D3 / Bb2 / G2.
            int chordLow(const Song& s, int degree) { return wrapInto(s.rootNote + st(s, degree), 43); }

            int toneOf(const Song& s, const ProgChord& c, int k)
            {
                return chordLow(s, c.degree) + st(s, c.degree + k) - st(s, c.degree);
            }

            struct Ctx
            {
                Song&                  song;
                const ProgProgression& prog;
                std::vector<int>       chordIndexByBar; // 0..3
            };

            const ProgChord& chordAtBeat(const Ctx& c, double beat, int* barInChord = nullptr)
            {
                const int bar = std::max(0, std::min(c.song.totalBars - 1, (int) (beat / 4.0)));
                if (barInChord != nullptr)
                    *barInChord = bar % 2;
                return c.prog.chords[c.chordIndexByBar[(size_t) bar]];
            }

            void addNote(std::vector<AbsNote>& out, double beat, double len, int pitch, int vel)
            {
                out.push_back({ beat, len, pitch, clampVel(vel) });
            }

            // ------------------------------------------------------ the arp

            void writeArp(Ctx& c, std::mt19937& rng, std::vector<AbsNote>& arp)
            {
                // Most seeds follow the reference exactly (5-note cells, 6 on the
                // chord that has six tones); some use a tighter 3-note cell.
                const bool threeNote = pick(rng, 5) == 0;
                const bool descendingTail = pick(rng, 4) == 0; // low, 5th, top, 9th, oct instead of ascending

                for (const SongSection& s : c.song.sections)
                {
                    int from = 0, to = s.bars;
                    if (s.kind == MusicSection::Intro)  from = s.bars / 2;
                    if (s.kind == MusicSection::Outro)  to = s.bars / 2;
                    const double start = (s.startBar + from) * 4.0;
                    const double end   = (s.startBar + to) * 4.0;
                    int cellIndex = 0, pos = 0, cellLen = 0;
                    std::array<int, 6> cell {};
                    for (int step = 0; start + step * kStep < end - 1e-9; ++step)
                    {
                        const double t = start + step * kStep;
                        if (pos == cellLen)
                        {
                            // A new cell ANTICIPATES the harmony: it takes the chord
                            // sounding at its last note (reference: the Bb cell starts
                            // 3 steps before bar 5, the climbing top note 3 steps early).
                            int barInChord = 0;
                            const ProgChord& ch = chordAtBeat(c, t + 4 * kStep, &barInChord);
                            const int n = threeNote ? 3 : ch.count;
                            std::array<int, 6> k {};
                            for (int i = 0; i < ch.count; ++i)
                                k[(size_t) i] = ch.cell[i];
                            if (barInChord == 1 && ch.climb >= 0)
                                k[(size_t) ch.count - 1] = ch.climb;
                            std::array<int, 6> tones {};
                            for (int i = 0; i < ch.count; ++i)
                                tones[(size_t) i] = toneOf(c.song, ch, k[(size_t) i]);
                            // Reference detail: the octave note runs low, high, high (D4 D5 D5 ...).
                            if (cellIndex % 3 != 0 && k[2] == 7)
                                tones[2] += 12;
                            if (descendingTail)
                                std::swap(tones[2], tones[(size_t) ch.count - 1]);
                            if (threeNote)
                                cell = { tones[0], tones[2], tones[(size_t) ch.count - 1], 0, 0, 0 };
                            else
                                cell = tones;
                            cellLen = n;
                            pos = 0;
                            ++cellIndex;
                        }
                        const int vel = pos == 0 ? 104 : (step % 4 == 0 ? 96 : 86);
                        addNote(arp, t, kStep * 0.95, cell[(size_t) pos], vel);
                        ++pos;
                    }
                }
            }

            // ------------------------------------------------------ pad + lead

            void writePad(Ctx& c, std::vector<AbsNote>& pad)
            {
                for (const SongSection& s : c.song.sections)
                {
                    if (s.kind == MusicSection::Intro)
                        continue;
                    const int to = s.kind == MusicSection::Outro ? s.bars / 2 : s.bars;
                    for (int b = 0; b < to; b += 2)
                    {
                        const int bar = s.startBar + b;
                        const ProgChord& ch = c.prog.chords[c.chordIndexByBar[(size_t) bar]];
                        const int bars = std::min(2, to - b);
                        const int vel = s.kind == MusicSection::Breakdown || s.kind == MusicSection::BreakdownBuild ? 96 : 70;
                        // The chord's upper tones, lifted into the A3-E5 pad register.
                        for (int i = 1; i < ch.count; ++i)
                        {
                            int p = toneOf(c.song, ch, ch.cell[i]);
                            while (p < 55) p += 12;
                            while (p > 76) p -= 12;
                            addNote(pad, bar * 4.0, bars * 4.0, p, vel);
                        }
                    }
                }
            }

            // Long-note counter-melody on the arp's top voice, an octave up -
            // the "yearning" line made explicit. Enters in the second half of
            // the breakdown and carries through the build and main drop.
            void writeLead(Ctx& c, std::vector<AbsNote>& lead)
            {
                for (const SongSection& s : c.song.sections)
                {
                    int from = 0, to = 0;
                    if (s.kind == MusicSection::Breakdown)      { from = s.bars / 2; to = s.bars; }
                    if (s.kind == MusicSection::BreakdownBuild) { from = 0; to = s.bars - 1; }
                    if (s.kind == MusicSection::FinalDrop)      { from = 0; to = s.bars; }
                    for (int b = from; b + 1 < to + 1 && b < to; b += 2)
                    {
                        const int bar = s.startBar + b;
                        const ProgChord& ch = c.prog.chords[c.chordIndexByBar[(size_t) bar]];
                        const ProgChord& next = c.prog.chords[c.chordIndexByBar[(size_t) std::min(bar + 2, c.song.totalBars - 1)]];
                        const int top  = toneOf(c.song, ch, ch.cell[ch.count - 1]) + 12;
                        const int top2 = toneOf(c.song, ch, ch.climb >= 0 ? ch.climb : ch.cell[ch.count - 1]) + 12;
                        const int nextTop = toneOf(c.song, next, next.cell[next.count - 1]) + 12;
                        // Pickup: the scale neighbour of this chord's top, stepping towards the next chord's.
                        const int dir = nextTop > top2 ? 1 : -1;
                        int pickup = top2 + dir;
                        while (!isInScale(pickup - c.song.rootNote, c.song.scale)) pickup += dir;
                        addNote(lead, bar * 4.0, 3.0, top, 100);
                        addNote(lead, bar * 4.0 + 3.0, 1.0, top2 == top ? pickup : top2, 88);
                        if (b + 1 < to)
                        {
                            addNote(lead, bar * 4.0 + 4.0, 2.5, top2, 96);
                            addNote(lead, bar * 4.0 + 6.5, 1.5, pickup, 84);
                        }
                    }
                }
            }

            // ------------------------------------------------------ bass

            void writeBass(Ctx& c, std::vector<AbsNote>& bass)
            {
                for (const SongSection& s : c.song.sections)
                    for (int b = 0; b < s.bars; ++b)
                    {
                        const int bar = s.startBar + b;
                        const ProgChord& ch = c.prog.chords[c.chordIndexByBar[(size_t) bar]];
                        const int root = chordLow(c.song, ch.degree) - 12; // G1..F#2
                        switch (s.kind)
                        {
                            case MusicSection::Intro:
                                break;
                            case MusicSection::Breakdown:
                                if (b % 2 == 0) addNote(bass, bar * 4.0, 7.5, root, 80); // sustained sub root
                                break;
                            case MusicSection::BreakdownBuild:
                                if (b >= s.bars / 2 && b < s.bars - 1)
                                    for (int beat = 0; beat < 4; ++beat)
                                        addNote(bass, bar * 4.0 + beat + 0.5, 0.22, root, 100);
                                break;
                            case MusicSection::Outro:
                                if (b >= s.bars / 2) break;
                                [[fallthrough]];
                            default:
                                // Rolling progressive bass: the two 16ths after the 8th offbeat ("k . b b").
                                for (int beat = 0; beat < 4; ++beat)
                                {
                                    addNote(bass, bar * 4.0 + beat + 0.50, 0.2, root, 108);
                                    addNote(bass, bar * 4.0 + beat + 0.75, 0.2, root, 94);
                                }
                                break;
                        }
                    }
            }

            // ------------------------------------------------------ drums

            void writeDrums(Ctx& c, std::mt19937& rng, std::vector<AbsNote>& kick, std::vector<AbsNote>& clap,
                            std::vector<AbsNote>& hats, std::vector<AbsNote>& perc, std::vector<AbsNote>& roll,
                            std::vector<AbsNote>& fx)
            {
                static constexpr int kPerc[][4] = { { 3, 6, 11, -1 }, { 6, 10, 14, -1 }, { 3, 10, 13, -1 }, { 7, 11, 14, -1 } };
                const int percTpl = pick(rng, 4);
                for (const SongSection& s : c.song.sections)
                {
                    for (int b = 0; b < s.bars; ++b)
                    {
                        const int bar = s.startBar + b;
                        const double t0 = bar * 4.0;
                        bool kickOn = false, clapOn = false, hat8 = false, shaker = false, openHat = false, ride = false, percOn = false;
                        switch (s.kind)
                        {
                            case MusicSection::Intro:
                                kickOn = true; hat8 = b >= 4; percOn = b >= 8; break;
                            case MusicSection::Establish:
                                kickOn = hat8 = percOn = true; clapOn = b >= 8; shaker = b >= 8; break;
                            case MusicSection::Drop:
                                kickOn = clapOn = hat8 = shaker = percOn = true; break;
                            case MusicSection::FinalDrop:
                                kickOn = clapOn = hat8 = shaker = percOn = openHat = true; ride = b >= s.bars / 2; break;
                            case MusicSection::Outro:
                                kickOn = b < s.bars - 2; hat8 = true; clapOn = b < 8; percOn = b < 16; break;
                            default:
                                break; // breakdown / build: no drums
                        }
                        for (int step = 0; step < 16; ++step)
                        {
                            const double t = t0 + step * kStep;
                            if (kickOn && step % 4 == 0)                   addNote(kick, t, kStep, SongDrumNotes::kKick, 118);
                            if (clapOn && (step == 4 || step == 12))       addNote(clap, t, kStep, SongDrumNotes::kClap, 100);
                            if (hat8 && step % 4 == 2)                     addNote(hats, t, kStep, SongDrumNotes::kHatClosed, 92);
                            if (shaker && (step % 4 == 1 || step % 4 == 3)) addNote(hats, t, kStep, SongDrumNotes::kHatClosed, step % 4 == 3 ? 58 : 46);
                            if (openHat && step % 4 == 2 && step % 8 == 6) addNote(hats, t, kStep, SongDrumNotes::kHatOpen, 84);
                            if (ride && step % 2 == 0)                     addNote(perc, t, kStep, SongDrumNotes::kRide, step % 4 == 2 ? 74 : 54);
                        }
                        if (percOn)
                            for (int k = 0; k < 4 && kPerc[percTpl][k] >= 0; ++k)
                                addNote(perc, t0 + kPerc[percTpl][k] * kStep, kStep,
                                        k == 1 ? SongDrumNotes::kPercB : SongDrumNotes::kRim, 80);
                        // Subtle fill at the end of every 16-bar phrase of a groove section.
                        if ((s.kind == MusicSection::Drop || s.kind == MusicSection::FinalDrop || s.kind == MusicSection::Establish)
                            && b % 16 == 15)
                            for (int step = 13; step < 16; ++step)
                                addNote(roll, t0 + step * kStep, kStep, SongDrumNotes::kSnare, 60 + 12 * (step - 13));
                        if ((s.kind == MusicSection::Drop || s.kind == MusicSection::FinalDrop) && b % 16 == 0)
                            addNote(fx, t0, 4.0, SongFxNotes::kCrash, b == 0 ? 112 : 88);
                    }
                    // Tension without the trance snare-roll cliche: a soft 8-bar roll
                    // only at the end of the build, and the bar before the drop empty.
                    if (s.kind == MusicSection::BreakdownBuild)
                    {
                        const int rollStart = s.startBar + s.bars - 8;
                        for (int b = 0; b < 7; ++b)
                        {
                            const double sub = b < 4 ? 0.5 : (b < 6 ? 0.25 : 0.125);
                            for (double t = 0.0; t < 4.0 - 1e-9; t += sub)
                                addNote(roll, (rollStart + b) * 4.0 + t, std::min(sub, 0.2), SongDrumNotes::kSnare,
                                        (int) (40 + 60.0 * (b * 4.0 + t) / 28.0));
                        }
                    }
                }
            }

            void writeFx(const Song& song, std::vector<AbsNote>& fx)
            {
                for (size_t i = 0; i < song.sections.size(); ++i)
                {
                    const SongSection& s = song.sections[i];
                    if (isGrooveKind(s.kind))
                        fx.push_back({ s.startBar * 4.0, 4.0, SongFxNotes::kImpact, 110 });
                    if (s.kind == MusicSection::Breakdown)
                        fx.push_back({ s.startBar * 4.0, 8.0, SongFxNotes::kDownlifter, 100 });
                    if (i + 1 < song.sections.size() && isGrooveKind(song.sections[i + 1].kind))
                    {
                        const int bars = std::min(s.kind == MusicSection::BreakdownBuild ? 16 : 8, s.bars);
                        fx.push_back({ (s.startBar + s.bars - bars) * 4.0, bars * 4.0, SongFxNotes::kRiser, 110 });
                    }
                }
            }
        }

        Song generateProgressiveSong(const SongParams& params)
        {
            Song song;
            song.style    = params.style;
            song.bpm      = params.bpm;
            song.rootNote = ((params.rootNote % 12) + 12) % 12;
            song.scale    = params.scale;
            song.seed     = params.seed;

            std::mt19937 rng(mixSeed(params.seed ^ 0x50524fu));

            int bar = 0;
            for (const ProgSection& s : progressiveStructure())
            {
                song.sections.push_back({ s.name, s.kind, bar, s.bars });
                bar += s.bars;
            }
            song.totalBars = bar;

            const auto& progs = progressions();
            const int progIndex = (params.progressionIndex >= 0 && params.progressionIndex < (int) progs.size())
                                      ? params.progressionIndex
                                      : pick(rng, (int) progs.size());
            const ProgProgression& prog = progs[(size_t) progIndex];
            song.progression = { prog.name, prog.character,
                                 { prog.chords[0].degree, prog.chords[1].degree, prog.chords[2].degree, prog.chords[3].degree } };

            // Two bars per chord, an 8-bar cycle restarting at every section.
            Ctx ctx { song, prog, {} };
            for (const SongSection& s : song.sections)
                for (int b = 0; b < s.bars; ++b)
                {
                    ctx.chordIndexByBar.push_back((b / 2) % 4);
                    song.chordDegreeByBar.push_back(prog.chords[(b / 2) % 4].degree);
                }

            char title[96];
            std::snprintf(title, sizeof(title), "Progressive %04u - %s %d BPM", (unsigned) (params.seed % 10000),
                          song.keyName().c_str(), (int) (params.bpm + 0.5));
            song.title = title;

            std::vector<AbsNote> kick, clap, hats, perc, roll, bass, pad, arp, lead, fx;
            writeDrums(ctx, rng, kick, clap, hats, perc, roll, fx);
            writeBass(ctx, bass);
            writePad(ctx, pad);
            writeArp(ctx, rng, arp);
            writeLead(ctx, lead);
            writeFx(song, fx);

            auto addTrack = [&](const char* name, SongTrackRole role, int color, std::vector<AbsNote>& notes)
            {
                SongTrack t;
                t.name       = name;
                t.role       = role;
                t.colorIndex = color;
                t.clips      = splitIntoClips(song, std::move(notes));
                song.tracks.push_back(std::move(t));
            };
            addTrack("Kick", SongTrackRole::Kick, 14, kick);
            addTrack("Clap", SongTrackRole::Clap, 1, clap);
            addTrack("Hats", SongTrackRole::Hats, 4, hats);
            addTrack("Perc", SongTrackRole::Perc, 5, perc);
            addTrack("Snare Roll", SongTrackRole::SnareRoll, 2, roll);
            addTrack("Bass", SongTrackRole::Bass, 26, bass);
            addTrack("Pad", SongTrackRole::Pad, 23, pad);
            addTrack("Arp", SongTrackRole::Arp, 19, arp);
            addTrack("Lead", SongTrackRole::Lead, 21, lead);
            addTrack("FX", SongTrackRole::Fx, 13, fx);
            return song;
        }
    }
}
