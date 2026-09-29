// Progressive techno song writer - SongStyle::ProgressiveTechno.
//
// Everything melodic is composed fresh per seed, in the spirit of the
// reference loop the user shared (MLPipeline/musical_target/
// progressive_arp_reference.md) but never copied from it:
//
//   HARMONY  a diatonic minor progression (8 to choose from), 2 bars per
//            chord, each chord given an extended colour (add9, sus->5th climb,
//            maj7(#11), 6/9, ...) chosen so the arp's TOP voice moves smoothly
//            and leans upwards - the reference's "yearning" top line.
//   ARP      a polymetric cell (5 or 6 notes against the 16-step bar) over a
//            wide open voicing - low root, 5th, octave, colour tones - in one
//            of several note orders; cells anticipate the next chord; the
//            octave note shimmers low/high/high; the cell opens up from 3
//            notes in the intro to the full cell.
//   PAD      the chord with semitone clusters spread apart (a sustained E+F
//            muddies everything; arpeggiated it is fine).
//   LEAD     an 8-bar call-and-response phrase built from a 1-bar motif, chosen
//            as the best of several hundred candidates by a scorer: no semitone
//            clashes with the arp/pad, chord tones on strong beats, mostly
//            stepwise, one climax, no see-sawing, resolves home. The main drop
//            gets a raised "climax" variant of the phrase in its third quarter.
#include "SongInternal.h"
#include "Theory.h"
#include <algorithm>
#include <array>
#include <cmath>
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
            // opening up over a 32-bar theme, a 32-bar breakdown where the arp
            // and the lead are exposed over the pad, a 16-bar build and a
            // 32-bar main drop.
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

            // ------------------------------------------------------ harmony

            // A chord: root scale degree + its arp cell as scale steps above
            // the root (ascending; cell[0] = 0 = the root). `climb` replaces the
            // top tone in the chord's second bar (-1 = none).
            struct ProgChord
            {
                int degree = 0;
                std::array<int, 6> cell {};
                int count = 5;
                int climb = -1;
            };

            struct Harmony
            {
                const char* name;
                const char* character;
                std::array<ProgChord, 4> chords;
            };

            struct ProgressionShape
            {
                const char* name;
                const char* character;
                int degrees[4];
            };

            const std::vector<ProgressionShape>& progressionShapes()
            {
                static const std::vector<ProgressionShape> p = {
                    { "i-VI-III-VII",  "hopeful lift",        { 0, 5, 2, 6 } },
                    { "i-VII-VI-VII",  "hypnotic",            { 0, 6, 5, 6 } },
                    { "i-iv-VI-VII",   "melancholic",         { 0, 3, 5, 6 } },
                    { "VI-VII-i-i",    "rising",              { 5, 6, 0, 0 } },
                    { "i-VI-iv-VII",   "driving",             { 0, 5, 3, 6 } },
                    { "i-III-VII-VI",  "open, wistful",       { 0, 2, 6, 5 } },
                    { "iv-VI-i-VII",   "yearning",            { 3, 5, 0, 6 } },
                    { "i-i-VI-iv",     "brooding",            { 0, 0, 5, 3 } },
                };
                return p;
            }

            // Colour options per chord quality (scale steps above the root,
            // all diatonic in natural minor).
            std::vector<ProgChord> coloursFor(int degree)
            {
                const int d = ((degree % 7) + 7) % 7;
                std::vector<ProgChord> v;
                auto add = [&](std::initializer_list<int> ks, int climb)
                {
                    ProgChord c;
                    c.degree = degree;
                    c.count = 0;
                    for (int k : ks) c.cell[(size_t) c.count++] = k;
                    c.climb = climb;
                    v.push_back(c);
                };
                switch (d)
                {
                    case 0: // i
                        add({ 0, 4, 7, 8, 9 }, -1);   // add9, top b3
                        add({ 0, 4, 7, 8, 11 }, -1);  // add9, top 5th
                        add({ 0, 4, 7, 8, 10 }, 11);  // sus4 climbing to the 5th
                        add({ 0, 4, 7, 9, 11 }, 13);  // top 5th climbing to b7
                        break;
                    case 3: // iv
                    case 4: // v
                        add({ 0, 4, 7, 8, 9 }, -1);
                        add({ 0, 4, 7, 9, 11 }, -1);
                        add({ 0, 4, 7, 8, 13 }, -1);  // m7(9)
                        break;
                    case 5: // VI
                        add({ 0, 4, 9, 10, 13 }, -1); // maj7#11
                        add({ 0, 4, 7, 9, 13 }, -1);  // maj7
                        add({ 0, 4, 7, 8, 11 }, -1);  // add9, top 5th
                        break;
                    case 2: // III
                        add({ 0, 4, 7, 8, 9 }, -1);   // add9, top 3rd
                        add({ 0, 4, 9, 11, 13 }, -1); // maj7 on top
                        add({ 0, 4, 7, 9, 11 }, -1);
                        break;
                    case 6: // VII
                        add({ 0, 4, 7, 8, 12 }, -1);     // 6/9
                        add({ 0, 4, 7, 8, 9, 12 }, -1);  // 6-note 6/9
                        add({ 0, 4, 7, 9, 11 }, -1);
                        break;
                    default:
                        add({ 0, 4, 7, 9, 11 }, -1);
                        break;
                }
                return v;
            }

            int st(const Song& s, int degree) { return degreeToSemitone(degree, s.scale); }

            // Low (root) note of a chord in [G2, F#3).
            int chordLow(const Song& s, int degree) { return wrapInto(s.rootNote + st(s, degree), 43); }

            int toneOf(const Song& s, const ProgChord& c, int k)
            {
                return chordLow(s, c.degree) + st(s, c.degree + k) - st(s, c.degree);
            }

            int topOf(const Song& s, const ProgChord& c, bool secondBar)
            {
                const int k = (secondBar && c.climb >= 0) ? c.climb : c.cell[(size_t) c.count - 1];
                return toneOf(s, c, k);
            }

            // Pick one colour per chord so the top line moves smoothly, has
            // some movement, and leans upwards; ties broken by the RNG.
            Harmony buildHarmony(const Song& s, const ProgressionShape& shape, std::mt19937& rng)
            {
                std::array<std::vector<ProgChord>, 4> opts;
                for (int i = 0; i < 4; ++i)
                    opts[(size_t) i] = coloursFor(shape.degrees[i]);

                Harmony best { shape.name, shape.character, {} };
                double bestScore = -1e9;
                for (size_t a = 0; a < opts[0].size(); ++a)
                    for (size_t b = 0; b < opts[1].size(); ++b)
                        for (size_t c = 0; c < opts[2].size(); ++c)
                            for (size_t d = 0; d < opts[3].size(); ++d)
                            {
                                const std::array<ProgChord, 4> ch { opts[0][a], opts[1][b], opts[2][c], opts[3][d] };
                                std::vector<int> line;
                                for (const ProgChord& x : ch)
                                {
                                    line.push_back(topOf(s, x, false));
                                    line.push_back(topOf(s, x, true));
                                }
                                double score = 0.0;
                                int moves = 0, ups = 0;
                                for (size_t i = 0; i < line.size(); ++i)
                                {
                                    const int dlt = line[(i + 1) % line.size()] - line[i];
                                    score -= std::abs(dlt) * 0.6;           // smooth
                                    if (std::abs(dlt) > 5) score -= 4.0;     // no big jumps in the top voice
                                    if (dlt != 0) ++moves;
                                    if (dlt > 0 && dlt <= 3) ++ups;
                                    if (std::abs(dlt) == 1) score -= 0.5;    // semitone steps are less "open"
                                }
                                if (moves < 2) score -= 6.0;                 // must move
                                score += ups * 1.2;                          // leaning upwards = yearning
                                for (int t : line)
                                    if (t < 62 || t > 77) score -= 3.0;      // keep the top voice in D4..F5
                                score += (double) (rng() % 1000) / 1000.0;   // variety between seeds
                                if (score > bestScore)
                                {
                                    bestScore = score;
                                    best.chords = ch;
                                }
                            }
                return best;
            }

            struct Ctx
            {
                Song&            song;
                const Harmony&   prog;
                std::vector<int> chordIndexByBar; // 0..3
            };

            const ProgChord& chordAtBeat(const Ctx& c, double beat, int* barInChord = nullptr)
            {
                const int bar = std::max(0, std::min(c.song.totalBars - 1, (int) (beat / 4.0)));
                if (barInChord != nullptr)
                    *barInChord = bar % 2;
                return c.prog.chords[(size_t) c.chordIndexByBar[(size_t) bar]];
            }

            // Pitch classes the arp/pad sound over this chord (both bars).
            std::vector<int> chordPcs(const Song& s, const ProgChord& ch, bool secondBar)
            {
                std::vector<int> pcs;
                for (int i = 0; i < ch.count; ++i)
                {
                    const int k = (i == ch.count - 1 && secondBar && ch.climb >= 0) ? ch.climb : ch.cell[(size_t) i];
                    pcs.push_back(((toneOf(s, ch, k)) % 12 + 12) % 12);
                }
                return pcs;
            }

            void addNote(std::vector<AbsNote>& out, double beat, double len, int pitch, int vel)
            {
                out.push_back({ beat, len, pitch, clampVel(vel) });
            }

            // ------------------------------------------------------ the arp

            void writeArp(Ctx& c, std::mt19937& rng, std::vector<AbsNote>& arp)
            {
                static constexpr int kOrders5[][5] = { { 0, 1, 2, 3, 4 }, { 0, 1, 3, 2, 4 }, { 0, 2, 1, 3, 4 },
                                                       { 0, 1, 2, 4, 3 }, { 0, 3, 1, 2, 4 } };
                static constexpr int kOrders6[][6] = { { 0, 1, 2, 3, 4, 5 }, { 0, 1, 2, 4, 3, 5 }, { 0, 2, 1, 3, 5, 4 } };
                const int length = pick(rng, 5) < 3 ? 5 : 6;           // 5-step (60%) or 6-step cell
                const int order  = length == 5 ? pick(rng, 5) : pick(rng, 3);
                const bool shimmer = pick(rng, 10) < 7;

                for (const SongSection& s : c.song.sections)
                {
                    int from = 0, to = s.bars;
                    if (s.kind == MusicSection::Intro)  from = s.bars / 2;
                    if (s.kind == MusicSection::Outro)  to = s.bars / 2;
                    const bool opening = s.kind == MusicSection::Intro; // 3-note cell while the track opens up
                    const double start = (s.startBar + from) * 4.0;
                    const double end   = (s.startBar + to) * 4.0;
                    int cellIndex = 0, pos = 0, cellLen = 0;
                    std::array<int, 6> cell {};
                    for (int step = 0; start + step * kStep < end - 1e-9; ++step)
                    {
                        const double t = start + step * kStep;
                        if (pos == cellLen)
                        {
                            const int len = opening ? 3 : length;
                            // A new cell anticipates the harmony: it takes the chord
                            // sounding at its LAST note.
                            int barInChord = 0;
                            const ProgChord& ch = chordAtBeat(c, t + (len - 1) * kStep, &barInChord);
                            std::array<int, 7> tones {};
                            for (int i = 0; i < ch.count; ++i)
                            {
                                const int k = (i == ch.count - 1 && barInChord == 1 && ch.climb >= 0) ? ch.climb : ch.cell[(size_t) i];
                                tones[(size_t) i] = toneOf(c.song, ch, k);
                            }
                            // Octave shimmer: the octave tone runs low, high, high.
                            if (shimmer && ch.cell[2] == 7 && cellIndex % 3 != 0)
                                tones[2] += 12;
                            if (ch.count == 5)
                                tones[5] = tones[1] + 12; // 6th tone: the 5th an octave up
                            const int top = ch.count - 1;
                            // A 5-step cell over a 6-tone chord keeps the top (colour) tone
                            // and drops the 4th tone instead.
                            static constexpr int kFive5[5] = { 0, 1, 2, 3, 4 };
                            static constexpr int kFive6[5] = { 0, 1, 2, 4, 5 };
                            const int* five = ch.count == 6 ? kFive6 : kFive5;
                            if (opening)
                                cell = { tones[0], tones[2] - (tones[2] > tones[(size_t) top] ? 12 : 0), tones[(size_t) top], 0, 0, 0 };
                            else if (len == 5)
                                for (int i = 0; i < 5; ++i) cell[(size_t) i] = tones[(size_t) five[kOrders5[order][i]]];
                            else
                                for (int i = 0; i < 6; ++i) cell[(size_t) i] = tones[(size_t) kOrders6[order][i]];
                            cellLen = len;
                            pos = 0;
                            ++cellIndex;
                        }
                        const int vel = pos == 0 ? 104 : (step % 4 == 0 ? 96 : 86);
                        addNote(arp, t, kStep * 0.95, cell[(size_t) pos], vel);
                        ++pos;
                    }
                }
            }

            // ------------------------------------------------------ pad

            // Chord tones in the A3..F5 pad register with semitone clusters
            // spread apart (the upper note of any minor 2nd moves up an octave).
            std::vector<int> padVoicing(const Song& s, const ProgChord& ch)
            {
                std::vector<int> v;
                for (int i = 1; i < ch.count; ++i)
                {
                    int p = toneOf(s, ch, ch.cell[(size_t) i]);
                    while (p < 55) p += 12;
                    while (p > 74) p -= 12;
                    if (std::find(v.begin(), v.end(), p) == v.end()) v.push_back(p);
                }
                for (int guard = 0; guard < 8; ++guard)
                {
                    std::sort(v.begin(), v.end());
                    bool moved = false;
                    for (size_t i = 0; i + 1 < v.size() && !moved; ++i)
                        for (size_t j = i + 1; j < v.size() && !moved; ++j)
                            if (v[j] - v[i] == 1)
                            {
                                v[j] += 12;
                                moved = true;
                            }
                    if (!moved) break;
                }
                return v;
            }

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
                        const ProgChord& ch = c.prog.chords[(size_t) c.chordIndexByBar[(size_t) bar]];
                        const int bars = std::min(2, to - b);
                        const int vel = s.kind == MusicSection::Breakdown || s.kind == MusicSection::BreakdownBuild ? 96 : 70;
                        for (int p : padVoicing(c.song, ch))
                            addNote(pad, bar * 4.0, bars * 4.0, p, vel);
                    }
                }
            }

            // ------------------------------------------------------ lead

            struct LNote
            {
                int step;  // 16ths from the phrase start (0..127)
                int len;   // in 16ths
                int pitch;
            };
            using Phrase = std::vector<LNote>;

            // Absolute scale-degree index of an in-scale pitch (and back).
            int degreeOfPitch(const Song& s, int pitch)
            {
                int d = (int) std::floor((pitch - s.rootNote) / 12.0) * 7;
                while (s.rootNote + st(s, d) < pitch) ++d;
                while (s.rootNote + st(s, d) > pitch) --d;
                return d;
            }
            int pitchOfDegree(const Song& s, int d) { return s.rootNote + st(s, d); }

            struct LeadPlan
            {
                int bar1;   // rhythm template index for the motif bar
                int tail;   // tail rhythm for the call's 2nd bar
                int rtail;  // tail rhythm for the response's 2nd bar
                std::array<int, 8> contourA, contourB; // scale-step moves
                int anchorShift;
            };

            constexpr int kBar1[][6][2] = {
                { { 0, 6 }, { 6, 4 }, { 10, 6 }, { -1, 0 } },
                { { 0, 3 }, { 3, 3 }, { 6, 2 }, { 8, 8 }, { -1, 0 } },
                { { 2, 2 }, { 4, 4 }, { 10, 2 }, { 12, 4 }, { -1, 0 } },
                { { 0, 4 }, { 6, 2 }, { 8, 2 }, { 10, 6 }, { -1, 0 } },
                { { 0, 10 }, { 10, 3 }, { 13, 3 }, { -1, 0 } },
                { { 3, 3 }, { 6, 3 }, { 9, 7 }, { -1, 0 } },
                { { 0, 2 }, { 2, 2 }, { 4, 4 }, { 8, 4 }, { 12, 4 }, { -1, 0 } },
                { { 0, 6 }, { 6, 2 }, { 8, 8 }, { -1, 0 } },
            };
            constexpr int kNumBar1 = (int) (sizeof(kBar1) / sizeof(kBar1[0]));
            constexpr int kTails[][4][2] = {
                { { 0, 12 }, { -1, 0 } },
                { { 0, 8 }, { 8, 6 }, { -1, 0 } },
                { { 0, 4 }, { 4, 4 }, { 8, 8 }, { -1, 0 } },
                { { 2, 6 }, { 8, 8 }, { -1, 0 } },
                { { 0, 14 }, { -1, 0 } },
            };
            constexpr int kNumTails = (int) (sizeof(kTails) / sizeof(kTails[0]));

            // Chord tones a lead may land on for strong beats: root, 3rd, 5th, and the 9th.
            bool isStableFor(const Song& s, const ProgChord& ch, int pitch)
            {
                for (int k : { 0, 2, 4, 1 })
                    if (((pitch - toneOf(s, ch, k)) % 12 + 12) % 12 == 0)
                        return true;
                return false;
            }

            // A lead note clashes if it is a semitone from a tone the arp/pad is
            // sounding and is not itself one of those tones.
            bool clashes(const std::vector<int>& pcs, int pitch)
            {
                const int pc = ((pitch % 12) + 12) % 12;
                if (std::find(pcs.begin(), pcs.end(), pc) != pcs.end())
                    return false;
                for (int q : pcs)
                {
                    const int iv = ((pc - q) % 12 + 12) % 12;
                    if (iv == 1 || iv == 11)
                        return true;
                }
                return false;
            }

            // Every pitch class the arp/pad sound while a lead note in `unit`
            // (step/len within the 2-bar unit) is held: both bars of the chord
            // it overlaps (the top note may climb in bar 2, and the arp
            // anticipates that ~5 steps early), plus the next chord when the
            // note reaches into the arp's anticipation window before it.
            std::vector<int> spanPcs(const Ctx& c, int unit, int step, int len)
            {
                const ProgChord& ch = c.prog.chords[(size_t) (unit % 4)];
                const int end = step + len;
                std::vector<int> pcs;
                auto add = [&](const std::vector<int>& v) { for (int x : v) if (std::find(pcs.begin(), pcs.end(), x) == pcs.end()) pcs.push_back(x); };
                if (step < 16) add(chordPcs(c.song, ch, false));
                if (end > 16 - 6) add(chordPcs(c.song, ch, true));
                if (end > 32 - 6) add(chordPcs(c.song, c.prog.chords[(size_t) ((unit + 1) % 4)], false));
                return pcs;
            }

            // The full clash rule for a held lead note: against the arp (which
            // moves) a semitone is only a problem if the lead doesn't double one
            // of its tones; against the PAD (which sustains) any semitone is a
            // flat-9 rub - e.g. the root over a maj7 pad, the b3 over an add9.
            bool noteClashes(const Ctx& c, int unit, int step, int len, int pitch)
            {
                if (clashes(spanPcs(c, unit, step, len), pitch))
                    return true;
                const int pc = ((pitch % 12) + 12) % 12;
                for (int q : padVoicing(c.song, c.prog.chords[(size_t) (unit % 4)]))
                {
                    const int iv = ((pc - q) % 12 + 12) % 12;
                    if (iv == 1 || iv == 11)
                        return true;
                }
                return false;
            }

            // Turn a plan into the 8-bar phrase over the 4 chords: first a
            // SKELETON of chord-tone targets on the strong notes (voice-led
            // from unit to unit, moving by the plan's contour through the
            // chord's stable tones), then the weak notes filled with passing
            // or neighbour tones stepping towards the next target.
            Phrase realise(const Ctx& c, const LeadPlan& plan, int startAnchor)
            {
                const Song& s = c.song;
                Phrase out;
                int prev = startAnchor;
                for (int unit = 0; unit < 4; ++unit)
                {
                    const ProgChord& ch = c.prog.chords[(size_t) unit];
                    const bool response = unit % 2 == 1;

                    // The unit's note slots: motif bar + tail bar.
                    struct Slot { int step, len; bool strong; };
                    std::vector<Slot> slots;
                    for (int i = 0; i < 6 && kBar1[plan.bar1][i][0] >= 0; ++i)
                        slots.push_back({ kBar1[plan.bar1][i][0], kBar1[plan.bar1][i][1],
                                          i == 0 || kBar1[plan.bar1][i][0] % 8 == 0 || kBar1[plan.bar1][i][1] >= 6 });
                    const int tIdx = response ? plan.rtail : plan.tail;
                    for (int i = 0; i < 4 && kTails[tIdx][i][0] >= 0; ++i)
                        slots.push_back({ 16 + kTails[tIdx][i][0], kTails[tIdx][i][1],
                                          i == 0 || kTails[tIdx][i][0] % 8 == 0 || kTails[tIdx][i][1] >= 6 });

                    // Stable, non-clashing chord tones in the lead register (E4..F5 targets).
                    auto stableTones = [&](const Slot& sl)
                    {
                        std::vector<int> v;
                        for (int p = 64; p <= 77; ++p)
                            if (isInScale(p - s.rootNote, s.scale) && isStableFor(s, ch, p)
                                && !noteClashes(c, unit, sl.step, sl.len, p))
                                v.push_back(p);
                        return v;
                    };

                    // Skeleton targets.
                    const std::array<int, 8>& contour = response ? plan.contourB : plan.contourA;
                    std::vector<int> target(slots.size(), -1);
                    int ci = 0, idx = -1;
                    for (size_t i = 0; i < slots.size(); ++i)
                    {
                        if (!slots[i].strong) continue;
                        const std::vector<int> tones = stableTones(slots[i]);
                        if (tones.empty()) continue;
                        if (idx < 0)
                        {
                            // Nearest to where the line is, lifted by the climax shift in the second half.
                            idx = 0;
                            for (size_t k = 0; k < tones.size(); ++k)
                                if (std::abs(tones[k] - prev) < std::abs(tones[(size_t) idx] - prev)) idx = (int) k;
                            if (unit >= 2) idx += plan.anchorShift;
                        }
                        else
                            idx += std::max(-2, std::min(2, contour[(size_t) (ci++ % 8)]));
                        idx = std::max(0, std::min((int) tones.size() - 1, idx));
                        target[i] = tones[(size_t) idx];
                    }
                    // Final note of the phrase resolves home (tonic, else the key's 5th).
                    if (unit == 3)
                    {
                        int bestP = -1;
                        for (int p = 60; p <= 79; ++p)
                        {
                            const int rel = ((p - s.rootNote) % 12 + 12) % 12;
                            if ((rel == 0 || rel == st(s, 4)) && !noteClashes(c, unit, slots.back().step, slots.back().len, p)
                                && (bestP < 0 || std::abs(p - prev) + (rel ? 3 : 0) < std::abs(bestP - prev) + (((bestP - s.rootNote) % 12 + 12) % 12 ? 3 : 0)))
                                bestP = p;
                        }
                        if (bestP >= 0) { slots.back().strong = true; target.back() = bestP; }
                    }

                    // Fill.
                    for (size_t i = 0; i < slots.size(); ++i)
                    {
                        int p;
                        auto bad = [&](int q) { return noteClashes(c, unit, slots[i].step, slots[i].len, q); };
                        if (target[i] >= 0)
                            p = target[i];
                        else
                        {
                            // Next target ahead (or stay around the last pitch).
                            int nextT = -1;
                            for (size_t k = i + 1; k < slots.size() && nextT < 0; ++k) nextT = target[k];
                            const int d = degreeOfPitch(s, prev);
                            if (nextT >= 0 && nextT != prev)
                                p = pitchOfDegree(s, d + (nextT > prev ? 1 : -1));       // passing tone
                            else
                                p = pitchOfDegree(s, d + (((int) i + unit) % 2 == 0 ? 1 : -1)); // neighbour tone
                            if (bad(p))
                                p = pitchOfDegree(s, degreeOfPitch(s, p) + (p > prev ? 1 : -1));
                            for (int r = 1; r <= 3 && bad(p); ++r)
                                for (int cand : { pitchOfDegree(s, degreeOfPitch(s, prev) + r), pitchOfDegree(s, degreeOfPitch(s, prev) - r) })
                                    if (!bad(cand)) { p = cand; break; }
                        }
                        out.push_back({ unit * 32 + slots[i].step, slots[i].len, p });
                        prev = p;
                    }
                }
                return out;
            }

            double scorePhrase(const Ctx& c, const Phrase& ph)
            {
                const Song& s = c.song;
                double score = 0.0;
                int steps = 0, lo = 999, hi = -999, hiCount = 0, hiUnit = -1;
                for (size_t i = 0; i < ph.size(); ++i)
                {
                    const LNote& n = ph[i];
                    const int unit = n.step / 32;
                    const ProgChord& ch = c.prog.chords[(size_t) unit];
                    if (noteClashes(c, unit, n.step % 32, n.len, n.pitch)) score -= 6.0 + 3.0 * n.len / 4.0;
                    if ((n.step % 8 == 0 || n.len >= 6) && isStableFor(s, ch, n.pitch)) score += 1.5;
                    lo = std::min(lo, n.pitch);
                    if (n.pitch > hi) { hi = n.pitch; hiCount = 1; hiUnit = unit; }
                    else if (n.pitch == hi) ++hiCount;
                    if (i > 0)
                    {
                        const int iv = std::abs(n.pitch - ph[i - 1].pitch);
                        if (iv <= 2) ++steps;
                        if (iv > 7) score -= 3.0;
                        if (iv > 12) score -= 10.0;
                    }
                    if (i >= 3 && n.pitch == ph[i - 2].pitch && ph[i - 1].pitch == ph[i - 3].pitch && n.pitch != ph[i - 1].pitch)
                        score -= 3.0; // a-b-a-b see-saw
                    if (i >= 2 && n.pitch == ph[i - 1].pitch && n.pitch == ph[i - 2].pitch)
                        score -= 2.5; // same-note stutter (3 in a row)
                    // Gap fill: after a leap of a 4th or more, step back the other way.
                    if (i >= 2)
                    {
                        const int leap = ph[i - 1].pitch - ph[i - 2].pitch;
                        const int next = n.pitch - ph[i - 1].pitch;
                        if (std::abs(leap) >= 5)
                            score += (next != 0 && (next > 0) != (leap > 0) && std::abs(next) <= 3) ? 1.5 : -4.0;
                    }
                }
                const double r = ph.size() > 1 ? (double) steps / (double) (ph.size() - 1) : 0.0;
                score += 6.0 * (1.0 - 2.0 * std::abs(r - 0.65));
                if (hi - lo > 14) score -= (hi - lo - 14);
                if (hi - lo < 5) score -= 4.0;                    // too static
                if (hiCount == 1 && hiUnit >= 2) score += 3.0;     // one climax, late in the phrase
                if (lo < 60 || hi > 84) score -= 5.0;
                // Rhythmic interest: at least one syncopated onset per unit.
                for (int u = 0; u < 4; ++u)
                {
                    bool sync = false;
                    int notes = 0;
                    for (const LNote& n : ph)
                        if (n.step / 32 == u) { ++notes; sync |= (n.step % 4) != 0; }
                    if (sync) score += 0.5;
                    if (notes < 3 || notes > 8) score -= 3.0;
                }
                // The phrase repeats: the loop-around from its last note back to
                // its first should be as smooth as any other interval.
                {
                    const int wrap = std::abs(ph.front().pitch - ph.back().pitch);
                    if (wrap >= 5) score -= 3.0;
                    if (wrap > 7) score -= 5.0;
                }
                // Ending: home.
                const int rel = ((ph.back().pitch - s.rootNote) % 12 + 12) % 12;
                if (rel == 0) score += 4.0;
                else if (rel == st(s, 4)) score += 2.0;
                return score;
            }

            struct LeadMaterial
            {
                Phrase phrase, climax;
            };

            LeadMaterial composeLead(const Ctx& c, std::mt19937& rng)
            {
                static constexpr int kMoves[] = { -1, -1, 0, 1, 1, 1, -1, 2, -2, 0, 1 };
                LeadPlan bestPlan {};
                Phrase best;
                double bestScore = -1e18;
                // Start near the tonic in the lead register - where the phrase ends.
                int startAnchor = 64;
                while (((startAnchor - c.song.rootNote) % 12 + 12) % 12 != 0) ++startAnchor;
                for (int cand = 0; cand < 2000; ++cand)
                {
                    LeadPlan plan;
                    plan.bar1 = pick(rng, kNumBar1);
                    plan.tail = pick(rng, kNumTails);
                    plan.rtail = pick(rng, kNumTails);
                    for (int i = 0; i < 8; ++i)
                    {
                        plan.contourA[(size_t) i] = kMoves[pick(rng, 11)];
                        plan.contourB[(size_t) i] = kMoves[pick(rng, 11)];
                    }
                    plan.anchorShift = 0;
                    const Phrase ph = realise(c, plan, startAnchor);
                    const double sc = scorePhrase(c, ph);
                    if (sc > bestScore)
                    {
                        bestScore = sc;
                        best = ph;
                        bestPlan = plan;
                    }
                }
                LeadMaterial m;
                m.phrase = best;
                // Climax variant: the second half lifted by one chord tone -
                // kept only if it still scores well (no clashes, sensible range).
                LeadPlan up = bestPlan;
                up.anchorShift = 1;
                const Phrase climax = realise(c, up, startAnchor);
                m.climax = scorePhrase(c, climax) >= bestScore - 4.0 ? climax : best;
                return m;
            }

            void placePhrase(std::vector<AbsNote>& lead, const Phrase& ph, int startBar, int maxBars, int velBase)
            {
                for (const LNote& n : ph)
                {
                    if (n.step >= maxBars * 16) continue;
                    const double len = std::min(n.len, maxBars * 16 - n.step) * kStep * 0.94;
                    addNote(lead, startBar * 4.0 + n.step * kStep, len, n.pitch, velBase + (n.step % 8 == 0 ? 8 : 0));
                }
            }

            void writeLead(Ctx& c, std::mt19937& rng, std::vector<AbsNote>& lead)
            {
                const LeadMaterial m = composeLead(c, rng);
                for (const SongSection& s : c.song.sections)
                {
                    switch (s.kind)
                    {
                        case MusicSection::Breakdown:
                            // Revealed in the second half of the breakdown, softly.
                            placePhrase(lead, m.phrase, s.startBar + 16, 8, 78);
                            placePhrase(lead, m.phrase, s.startBar + 24, 8, 84);
                            break;
                        case MusicSection::BreakdownBuild:
                            placePhrase(lead, m.phrase, s.startBar, 8, 86);
                            placePhrase(lead, m.phrase, s.startBar + 8, 7, 90); // the last bar stays empty
                            break;
                        case MusicSection::FinalDrop:
                            placePhrase(lead, m.phrase, s.startBar, 8, 96);
                            placePhrase(lead, m.phrase, s.startBar + 8, 8, 96);
                            placePhrase(lead, m.climax, s.startBar + 16, 8, 104);
                            placePhrase(lead, m.phrase, s.startBar + 24, 8, 96);
                            break;
                        default:
                            break;
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

            const auto& shapes = progressionShapes();
            const int shapeIndex = (params.progressionIndex >= 0 && params.progressionIndex < (int) shapes.size())
                                       ? params.progressionIndex
                                       : pick(rng, (int) shapes.size());
            const Harmony harmony = buildHarmony(song, shapes[(size_t) shapeIndex], rng);
            song.progression = { harmony.name, harmony.character,
                                 { harmony.chords[0].degree, harmony.chords[1].degree,
                                   harmony.chords[2].degree, harmony.chords[3].degree } };

            // Two bars per chord, an 8-bar cycle restarting at every section.
            Ctx ctx { song, harmony, {} };
            for (const SongSection& s : song.sections)
                for (int b = 0; b < s.bars; ++b)
                {
                    ctx.chordIndexByBar.push_back((b / 2) % 4);
                    song.chordDegreeByBar.push_back(harmony.chords[(size_t) ((b / 2) % 4)].degree);
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
            writeLead(ctx, rng, lead);
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
