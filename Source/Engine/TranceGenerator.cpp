// Trance / neo-rave song writer - SongStyle::Trance and SongStyle::NeoRave.
//
// Modelled on the measured style of Tiesto's PRISMATIC radio show (see
// MLPipeline/musical_target/prismatic_style.md): ~65% trance, median 140 BPM
// (neo-rave lane 145-150), 68% minor keys (G/F/D minor most common). Every
// rule below is tagged in that document as evidence [E] (a cited tutorial or
// the measured tracklist data) or inference [I].
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
            constexpr double kStep = 0.25; // one 16th in beats

            struct TranceSection
            {
                const char*  name;
                MusicSection kind;
                int          bars;
            };

            // [I] template from prismatic_style.md 4.2: 192 bars = 5:29 at
            // 140 BPM (recent extended mixes of show tracks run 4:46-6:58 [E]).
            const std::vector<TranceSection>& tranceStructure()
            {
                static const std::vector<TranceSection> s = {
                    { "Intro",     MusicSection::Intro,          16 },
                    { "Groove",    MusicSection::Establish,      16 },
                    { "Drop 1",    MusicSection::Drop,           32 },
                    { "Breakdown", MusicSection::Breakdown,      32 },
                    { "Build",     MusicSection::BreakdownBuild, 16 },
                    { "Main Drop", MusicSection::FinalDrop,      32 },
                    { "Break",     MusicSection::Breakdown,      16 },
                    { "Drop 2",    MusicSection::FinalDrop,      16 },
                    { "Outro",     MusicSection::Outro,          16 },
                };
                return s;
            }

            // [I] neo-rave tracks are shorter and loop-driven: 160 bars = 4:21 at 147 BPM.
            const std::vector<TranceSection>& neoRaveStructure()
            {
                static const std::vector<TranceSection> s = {
                    { "Intro",     MusicSection::Intro,          16 },
                    { "Groove",    MusicSection::Establish,      16 },
                    { "Break",     MusicSection::Breakdown,      16 },
                    { "Build",     MusicSection::BreakdownBuild,  8 },
                    { "Drop",      MusicSection::FinalDrop,      32 },
                    { "Breakdown", MusicSection::Breakdown,      16 },
                    { "Build 2",   MusicSection::BreakdownBuild,  8 },
                    { "Drop 2",    MusicSection::FinalDrop,      32 },
                    { "Outro",     MusicSection::Outro,          16 },
                };
                return s;
            }

            // [E] i-VI-III-VII, i-VII-VI-VII, VI-VII-i (myloops, unison.audio);
            // [I] i-iv-VI-VII.
            const std::vector<ChordProgression>& tranceProgressions()
            {
                static const std::vector<ChordProgression> p = {
                    { "i-VI-III-VII", "classic descending lift", { 0, 5, 2, 6 } },
                    { "i-VII-VI-VII", "hypnotic, driving",       { 0, 6, 5, 6 } },
                    { "VI-VII-i-i",   "anthemic resolve",        { 5, 6, 0, 0 } },
                    { "i-iv-VI-VII",  "euphoric climb",          { 0, 3, 5, 6 } },
                };
                return p;
            }

            bool isDrop(MusicSection k) { return k == MusicSection::Drop || k == MusicSection::FinalDrop; }

            // --------------------------------------------------------- hook

            // 2-bar riff rhythms in 16ths: (start step, length in steps).
            // Trance riffs are 1-2 bar 8th/16th-note figures [I, 4.6].
            struct RiffNote { int step, len, ladder; };

            constexpr int kRiffRhythms[][12][2] = {
                // 3-3-2 "tresillo" drive in both bars
                { { 0, 2 }, { 3, 2 }, { 6, 2 }, { 8, 2 }, { 11, 2 }, { 14, 2 }, { 16, 2 }, { 19, 2 }, { 22, 2 }, { 24, 2 }, { 27, 2 }, { 30, 2 } },
                // long-short anthem
                { { 0, 3 }, { 3, 3 }, { 6, 2 }, { 8, 4 }, { 12, 2 }, { 14, 2 }, { 16, 3 }, { 19, 3 }, { 22, 2 }, { 24, 6 }, { 30, 2 }, { -1, 0 } },
                // 8ths with a pickup
                { { 0, 2 }, { 2, 2 }, { 4, 2 }, { 6, 1 }, { 7, 1 }, { 8, 4 }, { 14, 2 }, { 16, 2 }, { 18, 2 }, { 20, 2 }, { 22, 2 }, { 24, 8 } },
                // syncopated gallop
                { { 0, 2 }, { 3, 1 }, { 4, 2 }, { 6, 2 }, { 10, 2 }, { 12, 3 }, { 16, 2 }, { 19, 1 }, { 20, 2 }, { 22, 2 }, { 26, 2 }, { 28, 4 } },
            };
            constexpr int kNumRiffRhythms = (int) (sizeof(kRiffRhythms) / sizeof(kRiffRhythms[0]));

            // Ladder of scale degrees above the chord root the hook moves on:
            // root, 3rd, 5th, octave, 9th, 10th, 12th - chord tones plus the
            // added 9th trance leans on [E, 4.5].
            constexpr int kLadder[] = { 0, 2, 4, 7, 8, 9, 11 };
            constexpr int kLadderSize = 7;

            struct Hook
            {
                std::vector<RiffNote> call, response; // each 2 bars (32 steps)
            };

            Hook buildHook(std::mt19937& rng)
            {
                Hook h;
                const int r = pick(rng, kNumRiffRhythms);
                const bool pedal = pick(rng, 2) == 0; // alternate a fixed top note with a moving voice
                const int top = 4 + pick(rng, 2);     // 9th or 10th
                int idx = 3;                          // start on the octave
                int n = 0;
                for (int i = 0; i < 12 && kRiffRhythms[r][i][0] >= 0; ++i, ++n)
                {
                    int ladder = idx;
                    if (pedal && (n % 2 == 1))
                        ladder = top;
                    else
                    {
                        const int move = pick(rng, 5) == 0 ? 2 : 1;
                        idx += pick(rng, 2) == 0 ? move : -move;
                        idx = std::max(1, std::min(kLadderSize - 1, idx));
                        ladder = idx;
                    }
                    h.call.push_back({ kRiffRhythms[r][i][0], kRiffRhythms[r][i][1], ladder });
                }
                // Call ends open (3rd/5th), response resolves down to the octave root.
                h.call.back().ladder = pick(rng, 2) == 0 ? 1 : 2;
                h.response = h.call;
                const size_t m = h.response.size();
                if (m >= 3)
                {
                    h.response[m - 3].ladder = 4;
                    h.response[m - 2].ladder = 2;
                }
                h.response[m - 1].ladder = 3;
                return h;
            }

            struct Ctx
            {
                Song& song;
                bool  neoRave;
            };

            int st(const Song& s, int degree) { return degreeToSemitone(degree, s.scale); }

            // Absolute pitch of ladder step `ladder` over `chordDegree`, with
            // the chord root placed in [rootLo, rootLo + 12).
            int ladderPitch(const Song& s, int chordDegree, int ladder, int rootLo)
            {
                const int root = wrapInto(s.rootNote + st(s, chordDegree), rootLo);
                return root + st(s, chordDegree + kLadder[ladder]) - st(s, chordDegree);
            }

            int chordAt(const Song& s, double beat)
            {
                const int bar = std::max(0, std::min(s.totalBars - 1, (int) (beat / 4.0)));
                return s.chordDegreeByBar[(size_t) bar];
            }

            // --------------------------------------------------------- parts

            void addHit(std::vector<AbsNote>& out, double beat, int pitch, int vel, double len = kStep)
            {
                out.push_back({ beat, len, pitch, clampVel(vel) });
            }

            void snareRoll(std::vector<AbsNote>& out, int startBar, int bars, bool leaveLastBarEmpty)
            {
                // [E] accelerating 1/4 -> 1/8 -> 1/16 -> 1/32, velocity rising,
                // "the bar before the drop mostly empty" (myloops "The Last 16 Bars").
                const int rollBars = leaveLastBarEmpty && bars >= 8 ? bars - 1 : bars;
                const double total = rollBars * 4.0;
                double t = 0.0;
                while (t < total - 1e-9)
                {
                    const double x = t / total;
                    const double stepLen = x < 0.25 ? 1.0 : x < 0.5 ? 0.5 : x < 0.75 ? 0.25 : 0.125;
                    addHit(out, startBar * 4.0 + t, SongDrumNotes::kSnare, (int) (50 + 77 * x), std::min(stepLen, 0.2));
                    t += stepLen;
                }
            }

            void writeDrums(Ctx& c, std::mt19937& rng, std::vector<AbsNote>& kick, std::vector<AbsNote>& clap,
                            std::vector<AbsNote>& hats, std::vector<AbsNote>& perc, std::vector<AbsNote>& roll,
                            std::vector<AbsNote>& fx)
            {
                static constexpr int kPercTemplates[][4] = { { 3, 11, -1, -1 }, { 6, 14, -1, -1 }, { 3, 10, 13, -1 }, { 7, 10, 15, -1 } };
                const int percTpl = pick(rng, 4);

                for (const SongSection& s : c.song.sections)
                {
                    for (int b = 0; b < s.bars; ++b)
                    {
                        const int bar = s.startBar + b;
                        const double t0 = bar * 4.0;
                        const bool drop = isDrop(s.kind);
                        const bool outroTail = s.kind == MusicSection::Outro && b >= s.bars - 4;

                        bool kickOn = false, clapOn = false, closed16 = false, closed8 = false, openHat = false,
                             ride = false, percOn = false;
                        switch (s.kind)
                        {
                            case MusicSection::Intro:
                                kickOn = true; closed8 = true; clapOn = b >= 8; percOn = b >= 8; break;
                            case MusicSection::Establish:
                                kickOn = clapOn = closed16 = openHat = percOn = true; break;
                            case MusicSection::Drop:
                            case MusicSection::FinalDrop:
                                kickOn = clapOn = closed16 = openHat = percOn = true;
                                ride = s.kind == MusicSection::FinalDrop; break;
                            case MusicSection::Outro:
                                kickOn = true; closed16 = !outroTail; closed8 = outroTail; openHat = b < 8;
                                clapOn = b < s.bars - 2; break;
                            case MusicSection::BreakdownBuild:
                            case MusicSection::Breakdown:
                            case MusicSection::PreDrop:
                            case MusicSection::Build:
                                break; // [I] kick out for the whole breakdown/build, back on the drop
                        }

                        for (int step = 0; step < 16; ++step)
                        {
                            const double t = t0 + step * kStep;
                            if (kickOn && step % 4 == 0)                        addHit(kick, t, SongDrumNotes::kKick, 120);
                            if (clapOn && (step == 4 || step == 12))            addHit(clap, t, SongDrumNotes::kClap, 110);
                            if (openHat && step % 4 == 2)                       addHit(hats, t, SongDrumNotes::kHatOpen, 100);
                            if (closed16 && step % 4 != 2)                      addHit(hats, t, SongDrumNotes::kHatClosed, step % 4 == 0 ? 82 : step % 4 == 1 ? 58 : 70);
                            if (closed8 && step % 2 == 0)                       addHit(hats, t, SongDrumNotes::kHatClosed, step % 4 == 2 ? 88 : 64);
                            if (ride && step % 2 == 0)                          addHit(perc, t, SongDrumNotes::kRide, step % 4 == 2 ? 80 : 60);
                        }
                        if (percOn)
                            for (int k = 0; k < 4 && kPercTemplates[percTpl][k] >= 0; ++k)
                                addHit(perc, t0 + kPercTemplates[percTpl][k] * kStep,
                                       k % 2 == 0 ? SongDrumNotes::kRim : SongDrumNotes::kPercB, 84);

                        // [I] a fill in the last bar of each 16-bar phrase of a groove section.
                        if ((drop || s.kind == MusicSection::Establish) && b % 16 == 15 && b != s.bars - 1)
                            for (int step = 12; step < 16; ++step)
                                addHit(roll, t0 + step * kStep, SongDrumNotes::kSnare, 80 + 10 * (step - 12));

                        // [I] crash on bar 1 of every 8-bar phrase in the drops.
                        if (drop && b % 8 == 0)
                            addHit(fx, t0, SongFxNotes::kCrash, b == 0 ? 120 : 96, 4.0);
                    }

                    // Snare rolls into every drop [E]: the whole Build, the last
                    // 8 bars of a Break, the last 4 bars of the Groove.
                    if (s.kind == MusicSection::BreakdownBuild)
                        snareRoll(roll, s.startBar, s.bars, true);
                    else if (!c.neoRave && s.kind == MusicSection::Breakdown && s.bars <= 16)
                        snareRoll(roll, s.startBar + s.bars - 8, 8, true); // short Break straight into Drop 2
                    else if (s.kind == MusicSection::Establish)
                        snareRoll(roll, s.startBar + s.bars - 4, 4, false);
                }
            }

            void writeBass(Ctx& c, std::vector<AbsNote>& bass)
            {
                for (const SongSection& s : c.song.sections)
                {
                    bool on = false;
                    switch (s.kind)
                    {
                        case MusicSection::Establish:
                        case MusicSection::Drop:
                        case MusicSection::FinalDrop: on = true; break;
                        default: break;
                    }
                    for (int b = 0; b < s.bars; ++b)
                    {
                        const int bar = s.startBar + b;
                        bool barOn = on
                            || (s.kind == MusicSection::Outro && b < 8)
                            // [I] the bass sneaks back in under the second half of a long build
                            || (s.kind == MusicSection::BreakdownBuild && s.bars >= 16 && b >= s.bars / 2 && b < s.bars - 1);
                        if (!barOn)
                            continue;
                        const int chord = c.song.chordDegreeByBar[(size_t) bar];
                        const int root = wrapInto(c.song.rootNote + st(c.song, chord), 31); // G1..F#2
                        for (int beat = 0; beat < 4; ++beat)
                        {
                            const double t = bar * 4.0 + beat;
                            if (c.neoRave)
                            {
                                // [E] clean saw one-note-per-offbeat ("donk"); double offbeat in the second half of drops.
                                addHit(bass, t + 0.5, root, 112, 0.22);
                                if (isDrop(s.kind) && b >= s.bars / 2)
                                    addHit(bass, t + 0.75, root + 12, 96, 0.18);
                            }
                            else
                            {
                                // [E] rolling 16ths: kick on the beat, bass on the 2nd-4th 16th.
                                addHit(bass, t + 0.25, root, 100, 0.19);
                                addHit(bass, t + 0.50, root, 92, 0.19);
                                addHit(bass, t + 0.75, root, 108, 0.19);
                            }
                        }
                    }
                }
            }

            // Triad + added 9th, voiced between A3 and C5 with smooth voice leading [E, 4.5].
            std::array<int, 4> voiceChord(const Song& s, int degree, const std::array<int, 4>* prev)
            {
                const int tones[4] = { st(s, degree), st(s, degree + 2), st(s, degree + 4), st(s, degree + 1) };
                std::array<int, 4> best {};
                int bestCost = 1 << 30;
                for (int inv = 0; inv < 3; ++inv)
                {
                    std::array<int, 4> v {};
                    int last = 0;
                    for (int i = 0; i < 3; ++i)
                    {
                        v[(size_t) i] = last = wrapInto(s.rootNote + tones[(inv + i) % 3], i == 0 ? 57 : last + 1);
                    }
                    v[3] = wrapInto(s.rootNote + tones[3], v[2] + 1); // the 9th on top
                    int cost = prev ? 0 : std::abs(v[0] - 60);
                    if (prev)
                        for (int i = 0; i < 4; ++i)
                            cost += std::abs(v[(size_t) i] - (*prev)[(size_t) i]);
                    if (v[3] > 79) cost += 50;
                    if (cost < bestCost) { bestCost = cost; best = v; }
                }
                return best;
            }

            void writePad(Ctx& c, std::vector<AbsNote>& pad)
            {
                std::array<int, 4> prev {};
                bool havePrev = false;
                static constexpr int kGate[16] = { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1 };
                for (const SongSection& s : c.song.sections)
                {
                    bool on = true;
                    if (s.kind == MusicSection::Intro) on = false;
                    int bar = s.startBar;
                    const int end = s.startBar + s.bars - (s.kind == MusicSection::Outro ? 8 : 0);
                    while (on && bar < end)
                    {
                        const int chord = c.song.chordDegreeByBar[(size_t) bar];
                        int stop = bar + 1;
                        while (stop < end && c.song.chordDegreeByBar[(size_t) stop] == chord) ++stop;
                        const std::array<int, 4> v = voiceChord(c.song, chord, havePrev ? &prev : nullptr);
                        prev = v;
                        havePrev = true;
                        const int vel = s.kind == MusicSection::Breakdown || s.kind == MusicSection::BreakdownBuild ? 100 : 78;
                        const bool gated = c.neoRave && s.kind != MusicSection::Establish;
                        for (int p : v)
                        {
                            if (gated)
                            {
                                // [E] trance-gate pad (theproducerschool hard-house preset set).
                                for (int step = 0; step < (stop - bar) * 16; ++step)
                                    if (kGate[step % 16])
                                        pad.push_back({ bar * 4.0 + step * kStep, 0.2, p, clampVel(vel - (step % 4 ? 12 : 0)) });
                            }
                            else
                                pad.push_back({ bar * 4.0, (stop - bar) * 4.0, p, vel });
                        }
                        bar = stop;
                    }
                }
            }

            // The hook, played over the section `s` from bar `fromBar` for `bars` bars.
            void playHook(Ctx& c, const Hook& hook, const SongSection& s, int fromBar, int bars, int rootLo,
                          int velocity, bool fifths, std::vector<AbsNote>& out)
            {
                for (int b = fromBar; b < fromBar + bars && b < s.bars; b += 2)
                {
                    const bool response = ((b - fromBar) / 2) % 2 == 1;
                    const std::vector<RiffNote>& half = response ? hook.response : hook.call;
                    for (size_t i = 0; i < half.size(); ++i)
                    {
                        const RiffNote& n = half[i];
                        const int bar = s.startBar + b + n.step / 16;
                        if (bar >= s.startBar + s.bars || bar >= s.startBar + fromBar + bars)
                            continue;
                        const double t = (s.startBar + b) * 4.0 + n.step * kStep;
                        const int chord = chordAt(c.song, t);
                        const int p = ladderPitch(c.song, chord, n.ladder, rootLo);
                        const int vel = velocity - (i % 2 == 1 ? 10 : 0) + (n.step % 4 == 0 ? 6 : 0);
                        out.push_back({ t, n.len * kStep * 0.92, p, clampVel(vel) });
                        if (fifths) // [E] perfect-fifth rave stab
                            out.push_back({ t, n.len * kStep * 0.92, p + 7, clampVel(vel - 8) });
                    }
                }
            }

            // 16th arpeggio over the ladder (breakdown teaser / groove texture).
            void playArp(Ctx& c, const SongSection& s, int fromBar, int bars, int rootLo, int velocity, bool eighths,
                         std::vector<AbsNote>& out)
            {
                static constexpr int kShape[16] = { 0, 1, 2, 3, 2, 1, 2, 4, 0, 1, 2, 3, 5, 3, 2, 1 };
                for (int b = fromBar; b < fromBar + bars && b < s.bars; ++b)
                    for (int step = 0; step < 16; step += eighths ? 2 : 1)
                    {
                        const double t = (s.startBar + b) * 4.0 + step * kStep;
                        const int p = ladderPitch(c.song, chordAt(c.song, t), kShape[step], rootLo);
                        out.push_back({ t, kStep * 0.8, p, clampVel(velocity - (step % 4 ? 14 : 0)) });
                    }
            }

            // [E] acid 303 counter-line (Funk Tribu breakdown description): 16ths
            // on root/octave/b3/5th/b7 with accents, following the chord root.
            void writeAcid(Ctx& c, std::mt19937& rng, std::vector<AbsNote>& acid)
            {
                static constexpr int kDegrees[] = { 0, 0, 7, 2, 4, 6, -3 };
                std::array<int, 16> pattern {}, accent {};
                for (int i = 0; i < 16; ++i)
                {
                    pattern[(size_t) i] = pick(rng, 4) == 0 ? -99 : kDegrees[pick(rng, 7)];
                    accent[(size_t) i]  = pick(rng, 3) == 0;
                }
                pattern[0] = 0;
                for (const SongSection& s : c.song.sections)
                {
                    int from = 0, to = 0;
                    switch (s.kind)
                    {
                        case MusicSection::Intro:     from = 8; to = s.bars; break;
                        case MusicSection::Establish:
                        case MusicSection::Drop:
                        case MusicSection::FinalDrop: to = s.bars; break;
                        case MusicSection::Breakdown: from = s.bars / 2; to = s.bars; break;
                        case MusicSection::BreakdownBuild: to = s.bars - 1; break;
                        default: break;
                    }
                    for (int b = from; b < to; ++b)
                    {
                        const int bar = s.startBar + b;
                        const int chord = c.song.chordDegreeByBar[(size_t) bar];
                        for (int step = 0; step < 16; ++step)
                        {
                            const int d = pattern[(size_t) step];
                            if (d == -99) continue;
                            const int root = wrapInto(c.song.rootNote + st(c.song, chord), 43);
                            const int p = root + st(c.song, chord + d) - st(c.song, chord);
                            const bool slide = step < 15 && pattern[(size_t) step + 1] != -99 && pick(rng, 4) == 0;
                            acid.push_back({ bar * 4.0 + step * kStep, slide ? kStep * 1.05 : kStep * 0.55, p,
                                             accent[(size_t) step] ? 124 : 88 });
                        }
                    }
                }
            }

            void writeMelodic(Ctx& c, std::mt19937& rng, std::vector<AbsNote>& pluck, std::vector<AbsNote>& lead)
            {
                const Hook hook = buildHook(rng);
                for (const SongSection& s : c.song.sections)
                {
                    switch (s.kind)
                    {
                        case MusicSection::Establish:
                            if (!c.neoRave) playArp(c, s, s.bars / 2, s.bars / 2, 55, 70, true, pluck); // filtered teaser
                            break;
                        case MusicSection::Drop:
                            // [I] Drop 1: half-energy, the riff teased on the pluck only.
                            playHook(c, hook, s, 0, s.bars, 55, 96, false, pluck);
                            break;
                        case MusicSection::Breakdown:
                            if (s.bars >= 32)
                            {
                                // [E] "sustained pad, then a pluck or piano teases the theme" -> full hook on the lead.
                                playArp(c, s, 0, s.bars / 2, 55, 78, true, pluck);
                                playHook(c, hook, s, s.bars / 2, s.bars / 2, 55, 92, false, lead);
                            }
                            else
                            {
                                playHook(c, hook, s, 0, s.bars / 2, 55, 88, c.neoRave, lead);
                                if (!c.neoRave) playArp(c, s, s.bars / 2, s.bars / 2, 55, 72, false, pluck);
                            }
                            break;
                        case MusicSection::BreakdownBuild:
                            playHook(c, hook, s, 0, s.bars - 1, 55, 100, c.neoRave, lead);
                            if (!c.neoRave) playArp(c, s, 0, s.bars - 1, 55, 80, false, pluck);
                            break;
                        case MusicSection::FinalDrop:
                            // [I] the anthem lead with the pluck layered an octave below.
                            playHook(c, hook, s, 0, s.bars, 55, 112, c.neoRave, lead);
                            if (!c.neoRave) playHook(c, hook, s, 0, s.bars, 43, 100, false, pluck);
                            break;
                        default:
                            break;
                    }
                }
            }

            void writeFx(const Song& song, std::vector<AbsNote>& fx)
            {
                for (size_t i = 0; i < song.sections.size(); ++i)
                {
                    const SongSection& s = song.sections[i];
                    const double start = s.startBar * 4.0;
                    if (isDrop(s.kind))
                        fx.push_back({ start, 4.0, SongFxNotes::kImpact, 127 });
                    if (s.kind == MusicSection::Breakdown)
                        fx.push_back({ start, 8.0, SongFxNotes::kDownlifter, 110 });
                    if (i + 1 < song.sections.size() && isDrop(song.sections[i + 1].kind))
                    {
                        const int riserBars = std::min(s.kind == MusicSection::BreakdownBuild ? 16 : 8, s.bars);
                        fx.push_back({ (s.startBar + s.bars - riserBars) * 4.0, riserBars * 4.0, SongFxNotes::kRiser, 120 });
                    }
                }
            }
        }

        Song generateTranceSong(const SongParams& params)
        {
            Song song;
            song.style    = params.style;
            song.bpm      = params.bpm;
            song.rootNote = ((params.rootNote % 12) + 12) % 12;
            song.scale    = params.scale;
            song.seed     = params.seed;
            const bool neoRave = params.style == SongStyle::NeoRave;

            std::mt19937 rng(mixSeed(params.seed ^ (neoRave ? 0x4E52u : 0x7452u)));

            const auto& structure = neoRave ? neoRaveStructure() : tranceStructure();
            int bar = 0;
            for (const TranceSection& s : structure)
            {
                song.sections.push_back({ s.name, s.kind, bar, s.bars });
                bar += s.bars;
            }
            song.totalBars = bar;

            const auto& progs = tranceProgressions();
            const int progIndex = (params.progressionIndex >= 0 && params.progressionIndex < (int) progs.size())
                                      ? params.progressionIndex
                                      : pick(rng, (int) progs.size());
            song.progression = progs[(size_t) progIndex];

            // [I] one chord per bar in drops/grooves, one per 2 bars in breakdowns.
            for (const SongSection& s : song.sections)
            {
                const bool slow = s.kind == MusicSection::Breakdown || s.kind == MusicSection::Intro;
                for (int b = 0; b < s.bars; ++b)
                    song.chordDegreeByBar.push_back(song.progression.degrees[((slow ? b / 2 : b)) % 4]);
            }

            char title[96];
            std::snprintf(title, sizeof(title), "%s %04u - %s %d BPM", neoRave ? "Neo-Rave" : "Trance",
                          (unsigned) (params.seed % 10000), song.keyName().c_str(), (int) (params.bpm + 0.5));
            song.title = title;

            Ctx ctx { song, neoRave };
            std::vector<AbsNote> kick, clap, hats, perc, roll, bass, pad, pluck, lead, acid, fx;
            writeDrums(ctx, rng, kick, clap, hats, perc, roll, fx);
            writeBass(ctx, bass);
            writePad(ctx, pad);
            writeMelodic(ctx, rng, pluck, lead);
            if (neoRave)
                writeAcid(ctx, rng, acid);
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
            if (neoRave)
                addTrack("Acid", SongTrackRole::Acid, 9, acid);
            else
                addTrack("Pluck", SongTrackRole::Pluck, 19, pluck);
            addTrack(neoRave ? "Rave Stab" : "Lead", SongTrackRole::Lead, 21, lead);
            addTrack("FX", SongTrackRole::Fx, 13, fx);
            return song;
        }
    }
}
