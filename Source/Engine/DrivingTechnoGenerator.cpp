// Driving techno song writer - SongStyle::DrivingTechno.
//
// Modelled on a reference track the user chose (Inner Sense - "People Can
// Fly", Spectrum 2025; analysed in MLPipeline/musical_target/
// driving_techno_reference.md). What the analysis found, and this file
// writes fresh for every seed (never copying the track's notes):
//
//   GROOVE   128 BPM; kick on every beat, clap 2/4, open hat on every 8th
//            offbeat, 16th shaker; a RUMBLE bass on the 3rd+4th 16th of each
//            beat ("k . b b" - nothing right after the kick); a pumping
//            tonic+5th STAB on every 16th except the kick hits.
//   HOOK     sparse and descending over the tonic pedal (reference: F-E-D-A,
//            b6-5-4-1 in A minor) with a two-note pickup into each repeat;
//            later an octave lower.
//   HARMONY  two chords, 4 bars each - i <-> VI in the reference, so the
//            VI chord lifts with a lydian #11.
//   PULSE    the breakdown melody is a repeated-note 16th pulse whose pitch
//            changes once per bar, turning slowly around a tone the two
//            chords share (reference: C A A C | B-G A A B over Am | F).
//   BUILD    a bII chord (Phrygian darkness), then the pulse climbs
//            CHROMATICALLY one semitone per bar up to the octave, landing on
//            the drop; the pulse melody returns inside the last drop.
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

            struct DtSection
            {
                const char*  name;
                MusicSection kind;
                int          bars;
            };

            // 144 bars = 4:30 at 128 BPM, following the reference edit's shape
            // (28-bar intro/groove, 4-bar break, 32-bar drop, 16-bar breakdown,
            // 16-bar build, 32-bar drop) plus an outro.
            const std::vector<DtSection>& structure()
            {
                static const std::vector<DtSection> s = {
                    { "Intro",     MusicSection::Intro,          16 },
                    { "Groove",    MusicSection::Establish,      12 },
                    { "Break",     MusicSection::PreDrop,         4 },
                    { "Drop",      MusicSection::Drop,           32 },
                    { "Breakdown", MusicSection::Breakdown,      16 },
                    { "Build",     MusicSection::BreakdownBuild, 16 },
                    { "Drop 2",    MusicSection::FinalDrop,      32 },
                    { "Outro",     MusicSection::Outro,          16 },
                };
                return s;
            }

            struct TwoChords
            {
                const char* name;
                const char* character;
                int a, b; // scale degrees, 4 bars each
            };

            const std::vector<TwoChords>& chordPairs()
            {
                static const std::vector<TwoChords> p = {
                    { "i-VI",  "lifting (lydian VI)", 0, 5 },
                    { "i-VII", "driving",             0, 6 },
                    { "i-iv",  "brooding",            0, 3 },
                    { "i-III", "hopeful",             0, 2 },
                };
                return p;
            }

            int st(const Song& s, int degree) { return degreeToSemitone(degree, s.scale); }
            int pc(int p) { return ((p % 12) + 12) % 12; }

            struct Ctx
            {
                Song&            song;
                TwoChords        pair;
                std::vector<int> chordByBar; // scale degree of the chord root, per bar
                int              pivot = 69; // the pulse melody's common tone
                int              upperA = 72, upperB = 71, lowerB = 67;
            };

            // Pitch classes of a chord: triad, and optionally its colour tones (9, 7, #11/11, 6).
            std::vector<int> triadPcs(const Song& s, int deg)
            {
                return { pc(s.rootNote + st(s, deg)), pc(s.rootNote + st(s, deg + 2)), pc(s.rootNote + st(s, deg + 4)) };
            }
            std::vector<int> colourPcs(const Song& s, int deg)
            {
                return { pc(s.rootNote + st(s, deg + 1)), pc(s.rootNote + st(s, deg + 6)),
                         pc(s.rootNote + st(s, deg + 3)), pc(s.rootNote + st(s, deg + 5)) };
            }
            bool has(const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }

            // Pulse-melody tones: a pivot shared by both chords, an upper
            // neighbour for each chord (a triad tone for the home chord, a
            // colour tone for the second - the lift) and a lower turn tone.
            void choosePulseTones(Ctx& c)
            {
                const Song& s = c.song;
                const std::vector<int> ta = triadPcs(s, c.pair.a), tb = triadPcs(s, c.pair.b);
                const std::vector<int> cb = colourPcs(s, c.pair.b);
                std::vector<int> both = ta;
                for (int x : colourPcs(s, c.pair.a)) both.push_back(x);
                // Prefer the tonic, then the b3, then the 5th as the shared tone.
                int pivotPc = -1;
                for (int d : { 0, 2, 4, 1 })
                {
                    const int x = pc(s.rootNote + st(s, d));
                    if (has(both, x) && (has(tb, x) || has(cb, x))) { pivotPc = x; break; }
                }
                if (pivotPc < 0) pivotPc = pc(s.rootNote);
                c.pivot = 65;
                while (pc(c.pivot) != pivotPc) ++c.pivot; // pivot in F4..E5

                auto nextInScale = [&](int p, int dir, auto accept)
                {
                    for (int q = p + dir; std::abs(q - p) <= 12; q += dir)
                        if (isInScale(q - s.rootNote, s.scale) && accept(pc(q))) return q;
                    return p + dir * 2;
                };
                c.upperA = nextInScale(c.pivot, 1, [&](int x) { return has(ta, x) && x != pivotPc; });
                c.upperB = nextInScale(c.pivot, 1, [&](int x) { return (has(cb, x) || has(tb, x)) && x != pivotPc; });
                c.lowerB = nextInScale(c.pivot, -1, [&](int x) { return (has(cb, x) || has(tb, x)) && x != pivotPc; });
            }

            void addNote(std::vector<AbsNote>& out, double beat, double len, int pitch, int vel)
            {
                out.push_back({ beat, len, pitch, clampVel(vel) });
            }

            // 16th repeated-note pulse over one bar (optionally split into two pitches).
            void pulseBar(std::vector<AbsNote>& out, int bar, int p1, int p2, int vel)
            {
                for (int step = 0; step < 16; ++step)
                    addNote(out, bar * 4.0 + step * kStep, kStep * 0.9, step < 8 ? p1 : p2, vel - (step % 4 == 0 ? 0 : 10));
            }

            // One 8-bar statement of the pulse melody: 4 bars over chord A, 4 over chord B.
            struct PulsePattern { int a[4]; int b[4]; }; // 0 = pivot, 1 = upper, 2 = turn (upper -> lower)
            void pulseMelody(Ctx& c, const PulsePattern& pat, std::vector<AbsNote>& out, int startBar, int bars, int vel, int octave)
            {
                for (int i = 0; i < bars; ++i)
                {
                    const int k = i % 8;
                    const bool onA = k < 4;
                    const int code = onA ? pat.a[k] : pat.b[k - 4];
                    const int up = onA ? c.upperA : c.upperB;
                    int p1 = c.pivot, p2 = c.pivot;
                    if (code == 1) p1 = p2 = up;
                    if (code == 2) { p1 = c.upperB; p2 = c.lowerB; }
                    pulseBar(out, startBar + i, p1 + octave, p2 + octave, vel);
                }
            }

            struct Hook
            {
                std::array<int, 4> degrees; // scale steps from the tonic
                int rhythm;
                bool pickup;
            };

            constexpr int kHookRhythms[][4][2] = {
                { { 1, 1 }, { 2, 3 }, { 5, 2 }, { 8, 2 } },   // the reference's shape
                { { 0, 2 }, { 3, 3 }, { 6, 2 }, { 8, 4 } },
                { { 2, 2 }, { 4, 4 }, { 10, 2 }, { 12, 2 } },
                { { 1, 1 }, { 2, 2 }, { 6, 2 }, { 10, 4 } },
            };

            // 2-bar hook statements from `startBar` for `bars` bars.
            void playHook(Ctx& c, const Hook& h, std::vector<AbsNote>& out, int startBar, int bars, int vel,
                          int octave, int notesPerStatement = 4)
            {
                const int tonic = wrapInto(c.song.rootNote, 57); // A3-ish register
                auto pitchOf = [&](int d) { return tonic + st(c.song, d) + octave; };
                for (int b = 0; b + 1 < bars + 1 && b < bars; b += 2)
                {
                    const int bar = startBar + b;
                    for (int i = 0; i < notesPerStatement; ++i)
                        addNote(out, bar * 4.0 + kHookRhythms[h.rhythm][i][0] * kStep, kHookRhythms[h.rhythm][i][1] * kStep * 0.95,
                                pitchOf(h.degrees[(size_t) i]), vel + (i == 0 ? 6 : 0));
                    if (h.pickup && b + 1 < bars && notesPerStatement == 4)
                    {
                        addNote(out, (bar + 1) * 4.0 + 13 * kStep, 2 * kStep * 0.95, pitchOf(h.degrees[1]), vel - 8);
                        addNote(out, (bar + 1) * 4.0 + 15 * kStep, kStep * 0.95, pitchOf(h.degrees[0]), vel - 4);
                    }
                }
            }
        }

        Song generateDrivingTechnoSong(const SongParams& params)
        {
            Song song;
            song.style    = params.style;
            song.bpm      = params.bpm;
            song.rootNote = ((params.rootNote % 12) + 12) % 12;
            song.scale    = params.scale;
            song.seed     = params.seed;

            std::mt19937 rng(mixSeed(params.seed ^ 0x44524956u));

            int bar = 0;
            for (const DtSection& s : structure())
            {
                song.sections.push_back({ s.name, s.kind, bar, s.bars });
                bar += s.bars;
            }
            song.totalBars = bar;

            const auto& pairs = chordPairs();
            const int pairIndex = (params.progressionIndex >= 0 && params.progressionIndex < (int) pairs.size())
                                      ? params.progressionIndex
                                      : (pick(rng, 3) == 0 ? pick(rng, (int) pairs.size()) : 0); // the reference's i-VI most often
            Ctx c { song, pairs[(size_t) pairIndex], {}, 69, 72, 71, 67 };
            song.progression = { c.pair.name, c.pair.character, { c.pair.a, c.pair.b, c.pair.a, c.pair.b } };

            // Harmony per bar: tonic pedal in the intro/groove/break and the
            // first half of each drop; A/B every 4 bars elsewhere.
            for (const SongSection& s : song.sections)
                for (int b = 0; b < s.bars; ++b)
                {
                    int deg = c.pair.a;
                    const bool alternate = s.kind == MusicSection::Breakdown
                                           || ((s.kind == MusicSection::Drop || s.kind == MusicSection::FinalDrop) && b >= s.bars / 2);
                    if (alternate && (b / 4) % 2 == 1) deg = c.pair.b;
                    c.chordByBar.push_back(deg);
                    song.chordDegreeByBar.push_back(deg);
                }
            choosePulseTones(c);

            static constexpr int kHooks[][4] = { { 5, 4, 3, 0 }, { 4, 3, 2, 0 }, { 5, 4, 2, 0 }, { 6, 5, 4, 0 }, { 3, 2, 1, 0 } }; // all descend to the tonic
            Hook hook;
            {
                const int h = pick(rng, 5);
                for (int i = 0; i < 4; ++i) hook.degrees[(size_t) i] = kHooks[h][i];
                hook.rhythm = pick(rng, 4);
                hook.pickup = pick(rng, 4) != 0;
            }
            static constexpr PulsePattern kPatterns[] = {
                { { 1, 0, 0, 1 }, { 2, 0, 0, 1 } }, // the reference's C A A C | B-G A A B shape
                { { 0, 1, 0, 1 }, { 1, 0, 2, 0 } },
                { { 1, 0, 1, 0 }, { 2, 0, 1, 0 } },
            };
            const PulsePattern& pulsePat = kPatterns[pick(rng, 3)];

            char title[96];
            std::snprintf(title, sizeof(title), "Driving Techno %04u - %s %d BPM", (unsigned) (params.seed % 10000),
                          song.keyName().c_str(), (int) (params.bpm + 0.5));
            song.title = title;

            std::vector<AbsNote> kick, clap, hats, perc, roll, rumble, stab, pad, pulse, lead, fx;
            const int tonicStab = wrapInto(song.rootNote, 55);

            for (const SongSection& s : song.sections)
            {
                for (int b = 0; b < s.bars; ++b)
                {
                    const int bar = s.startBar + b;
                    const double t0 = bar * 4.0;
                    bool kickOn = false, clapOn = false, openHat = false, shaker = false, ride = false, rim = false,
                         rumbleOn = false, stabOn = false;
                    switch (s.kind)
                    {
                        case MusicSection::Intro:
                            kickOn = rumbleOn = true; openHat = b >= 4; clapOn = shaker = b >= 8; stabOn = b >= 8; break;
                        case MusicSection::Establish:
                            kickOn = rumbleOn = openHat = clapOn = shaker = stabOn = rim = true; break;
                        case MusicSection::PreDrop:
                            stabOn = b < s.bars - 1; openHat = b < s.bars - 1; break;
                        case MusicSection::Drop:
                            kickOn = rumbleOn = openHat = clapOn = shaker = stabOn = rim = true; break;
                        case MusicSection::FinalDrop:
                            kickOn = rumbleOn = openHat = clapOn = shaker = stabOn = rim = true; ride = b >= s.bars / 2; break;
                        case MusicSection::BreakdownBuild:
                            // Drums return for the middle of the build, then a 4-bar hole into the drop.
                            kickOn = clapOn = openHat = b >= 4 && b < 12; stabOn = b >= 4 && b < 12; break;
                        case MusicSection::Outro:
                            kickOn = b < s.bars - 2; rumbleOn = stabOn = b < 8; openHat = shaker = b < 12; clapOn = b < 10; break;
                        default:
                            break;
                    }
                    const int chord = c.chordByBar[(size_t) bar];
                    const int root = [&] { int r = song.rootNote + st(song, chord); while (r < 28) r += 12; while (r >= 40) r -= 12; return r; }();
                    for (int step = 0; step < 16; ++step)
                    {
                        const double t = t0 + step * kStep;
                        if (kickOn && step % 4 == 0)                     addNote(kick, t, kStep, SongDrumNotes::kKick, 120);
                        if (clapOn && (step == 4 || step == 12))         addNote(clap, t, kStep, SongDrumNotes::kClap, 104);
                        if (openHat && step % 4 == 2)                    addNote(hats, t, kStep, SongDrumNotes::kHatOpen, 92);
                        if (shaker && step % 4 != 2)                     addNote(hats, t, kStep, SongDrumNotes::kHatClosed, step % 4 == 0 ? 64 : 50);
                        if (ride && step % 2 == 0)                       addNote(perc, t, kStep, SongDrumNotes::kRide, step % 4 == 2 ? 70 : 52);
                        if (rim && (step == 7 || step == 15))            addNote(perc, t, kStep, SongDrumNotes::kRim, 72);
                        if (rumbleOn && (step % 4 == 2 || step % 4 == 3)) addNote(rumble, t, kStep * 0.9, root, step % 4 == 2 ? 112 : 100);
                        if (stabOn && step % 4 != 0)
                        {
                            const int v = step % 4 == 2 ? 100 : 80;
                            addNote(stab, t, kStep * 0.8, tonicStab, v);
                            addNote(stab, t, kStep * 0.8, tonicStab + 7, v - 12);
                        }
                    }
                    // Fill in the last bar of each 16-bar phrase of a groove section.
                    if ((s.kind == MusicSection::Drop || s.kind == MusicSection::FinalDrop) && b % 16 == 15 && b != s.bars - 1)
                        for (int step = 12; step < 16; ++step)
                            addNote(roll, t0 + step * kStep, kStep, SongDrumNotes::kSnare, 70 + 10 * (step - 12));
                    if ((s.kind == MusicSection::Drop || s.kind == MusicSection::FinalDrop) && b % 16 == 0)
                        addNote(fx, t0, 4.0, SongFxNotes::kCrash, b == 0 ? 116 : 92);
                }

                // Melodic layers per section.
                switch (s.kind)
                {
                    case MusicSection::Intro:
                        playHook(c, hook, lead, s.startBar + 12, 4, 70, 0, 2); // first fragments
                        break;
                    case MusicSection::Establish:
                        playHook(c, hook, lead, s.startBar + 4, s.bars - 4, 80, 0);
                        break;
                    case MusicSection::PreDrop:
                        playHook(c, hook, lead, s.startBar, s.bars - 1, 88, 0);
                        break;
                    case MusicSection::Drop:
                        playHook(c, hook, lead, s.startBar, s.bars, 100, 0);
                        break;
                    case MusicSection::Breakdown:
                    {
                        // Pad: chord A / chord B, 4 bars each, closely voiced around C4.
                        for (int b = 0; b < s.bars; b += 4)
                        {
                            const int deg = c.chordByBar[(size_t) (s.startBar + b)];
                            for (int k : { 0, 2, 4 })
                            {
                                int p = song.rootNote + st(song, deg + k);
                                while (p < 55) p += 12;
                                while (p >= 67) p -= 12;
                                addNote(pad, (s.startBar + b) * 4.0, 16.0, p, 58);
                            }
                            int sub = song.rootNote + st(song, deg);
                            while (sub < 28) sub += 12;
                            while (sub >= 40) sub -= 12;
                            addNote(rumble, (s.startBar + b) * 4.0, 15.0, sub, 80); // long sub root
                        }
                        pulseMelody(c, pulsePat, pulse, s.startBar, s.bars, 62, 0); // the breakdown sits ~6 dB under the drops
                        break;
                    }
                    case MusicSection::BreakdownBuild:
                    {
                        // Bars 0-1: home chord; bars 2-3: the bII chord; then the
                        // pulse climbs chromatically to the pivot's octave by bar 14.
                        const int homeDeg = c.pair.a;
                        for (int k : { 0, 2, 4 })
                        {
                            int p = song.rootNote + st(song, homeDeg + k);
                            while (p < 55) p += 12;
                            while (p >= 67) p -= 12;
                            addNote(pad, s.startBar * 4.0, 8.0, p, 58);
                        }
                        const int flat2 = song.rootNote + 1; // Phrygian bII: a major triad a semitone above the tonic
                        for (int iv : { 0, 4, 7 })
                        {
                            int p = flat2 + iv;
                            while (p < 55) p += 12;
                            while (p >= 67) p -= 12;
                            addNote(pad, (s.startBar + 2) * 4.0, 8.0, p, 62);
                        }
                        pulseBar(pulse, s.startBar, c.upperA, c.upperA, 64);
                        pulseBar(pulse, s.startBar + 1, c.pivot, c.pivot, 64);
                        int climb = c.pivot + 1; // bII root near the pivot's register
                        while (pc(climb) != pc(flat2)) ++climb;
                        pulseBar(pulse, s.startBar + 2, climb, climb, 66);
                        pulseBar(pulse, s.startBar + 3, climb, climb, 66);
                        const int target = c.pivot + 12;
                        for (int b = 4; b < s.bars; ++b)
                        {
                            const int p = std::min(target, climb + (b - 3) * std::max(1, (target - climb + 10) / 11));
                            pulseBar(pulse, s.startBar + b, p, std::min(target, p + (b >= 12 ? 1 : 0)), 66 + 3 * (b - 3)); // swells into the drop
                        }
                        // Soft snare roll through the 4-bar hole, the final bar empty.
                        for (int b = 12; b < 15; ++b)
                        {
                            const double sub = b == 14 ? 0.125 : 0.25;
                            for (double t = 0.0; t < 4.0 - 1e-9; t += sub)
                                addNote(roll, (s.startBar + b) * 4.0 + t, std::min(sub, 0.2), SongDrumNotes::kSnare,
                                        (int) (50 + 50.0 * ((b - 12) * 4.0 + t) / 12.0));
                        }
                        break;
                    }
                    case MusicSection::FinalDrop:
                        // First half: the hook an octave lower; second half: the hook
                        // plus the pulse melody inside the drop (the climax).
                        playHook(c, hook, lead, s.startBar, s.bars / 2, 96, -12);
                        playHook(c, hook, lead, s.startBar + s.bars / 2, s.bars / 2, 104, 0);
                        pulseMelody(c, pulsePat, pulse, s.startBar + s.bars / 2, s.bars / 2, 90, 0);
                        break;
                    case MusicSection::Outro:
                        playHook(c, hook, lead, s.startBar, 4, 80, 0);
                        break;
                    default:
                        break;
                }
            }

            // FX: impacts on the drops, risers into them, a downlifter into the breakdown.
            for (size_t i = 0; i < song.sections.size(); ++i)
            {
                const SongSection& s = song.sections[i];
                if (s.kind == MusicSection::Drop || s.kind == MusicSection::FinalDrop)
                    fx.push_back({ s.startBar * 4.0, 4.0, SongFxNotes::kImpact, 116 });
                if (s.kind == MusicSection::Breakdown)
                    fx.push_back({ s.startBar * 4.0, 8.0, SongFxNotes::kDownlifter, 100 });
                if (i + 1 < song.sections.size()
                    && (song.sections[i + 1].kind == MusicSection::Drop || song.sections[i + 1].kind == MusicSection::FinalDrop))
                {
                    const int bars = std::min(s.kind == MusicSection::BreakdownBuild ? 16 : 8, s.bars);
                    fx.push_back({ (s.startBar + s.bars - bars) * 4.0, bars * 4.0, SongFxNotes::kRiser, 112 });
                }
            }

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
            addTrack("Rumble", SongTrackRole::Bass, 26, rumble);
            addTrack("Stab", SongTrackRole::Stab, 9, stab);
            addTrack("Pad", SongTrackRole::Pad, 23, pad);
            addTrack("Pulse", SongTrackRole::Arp, 19, pulse);
            addTrack("Lead", SongTrackRole::Lead, 21, lead);
            addTrack("FX", SongTrackRole::Fx, 13, fx);
            return song;
        }
    }
}
