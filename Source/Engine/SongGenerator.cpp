#include "SongGenerator.h"
#include "SongInternal.h"
#include "BassEngine.h"
#include "DrumEngine.h"
#include "Grid.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <random>

namespace Engine
{
    // ------------------------------------------------------------------
    // Structure
    // ------------------------------------------------------------------

    std::vector<SongSectionSpec> fullMelodicTechnoStructure()
    {
        // SectionEnergyTarget field order:
        //   energyStart, energyEnd, tension,
        //   drumEnergyStart, drumEnergyEnd, bassEnergyStart, bassEnergyEnd,
        //   melodicEnergyStart, melodicEnergyEnd
        //
        // Starting points are defaultMelodicTechnoCycle()'s own values for
        // each section kind (Arrangement.cpp documents their rationale);
        // the differences below are the ones research 3.2-3.4 calls for:
        // bass enters as its own section ("Bass in", Track C), Drop 1 is
        // deliberately the "subtle impact" drop (PDF guide wording), a
        // post-drop groove section carries the element reintroduction, and
        // a short 8-bar "Melodic Break" breather sits between Drop 2 and
        // Drop 3 (Track A).
        return {
            { "Intro",         { MusicSection::Intro,          16, { 0.20f, 0.30f, 0.15f, 0.35f, 0.45f, 0.00f, 0.00f, 0.45f, 0.55f } } },
            { "Bass In",       { MusicSection::Establish,      16, { 0.40f, 0.45f, 0.20f, 0.55f, 0.65f, 0.45f, 0.60f, 0.45f, 0.50f } } },
            { "Full Theme",    { MusicSection::Build,          16, { 0.50f, 0.70f, 0.55f, 0.65f, 0.80f, 0.60f, 0.75f, 0.55f, 0.70f } } },
            { "Drop 1",        { MusicSection::Drop,           16, { 0.85f, 0.85f, 0.30f, 0.90f, 0.90f, 0.90f, 0.90f, 0.70f, 0.70f } } },
            { "Groove",        { MusicSection::Establish,      16, { 0.70f, 0.70f, 0.35f, 0.75f, 0.75f, 0.70f, 0.70f, 0.60f, 0.65f } } },
            { "Pre-Break",     { MusicSection::PreDrop,         8, { 0.55f, 0.35f, 0.70f, 0.60f, 0.30f, 0.50f, 0.20f, 0.60f, 0.75f } } },
            { "Break",         { MusicSection::Breakdown,      24, { 0.15f, 0.20f, 0.65f, 0.05f, 0.05f, 0.10f, 0.10f, 0.90f, 0.90f } } },
            { "Buildup",       { MusicSection::BreakdownBuild,  8, { 0.30f, 0.60f, 0.85f, 0.20f, 0.60f, 0.10f, 0.40f, 0.75f, 0.80f } } },
            { "Drop 2",        { MusicSection::FinalDrop,      32, { 1.00f, 1.00f, 0.25f, 1.00f, 1.00f, 1.00f, 1.00f, 0.90f, 0.90f } } },
            { "Melodic Break", { MusicSection::Breakdown,       8, { 0.30f, 0.35f, 0.60f, 0.20f, 0.25f, 0.10f, 0.10f, 0.90f, 0.90f } } },
            { "Drop 3",        { MusicSection::FinalDrop,      24, { 1.00f, 1.00f, 0.25f, 1.00f, 1.00f, 1.00f, 1.00f, 0.90f, 0.90f } } },
            { "Outro",         { MusicSection::Outro,          16, { 0.40f, 0.10f, 0.10f, 0.50f, 0.20f, 0.40f, 0.00f, 0.40f, 0.15f } } },
        };
    }

    const std::vector<ChordProgression>& progressionCatalogue()
    {
        // 0-based degrees: 0=i 1=ii 2=III 3=iv 4=v 5=VI 6=VII
        static const std::vector<ChordProgression> catalogue = {
            { "i-VII-VI-VII", "dark, cyclical, hypnotic",   { 0, 6, 5, 6 } }, // research section 6 (Beatportal guide)
            { "i-v-iv-VII",   "introspective",              { 0, 4, 3, 6 } }, // research section 6, diatonic form
            { "i-VI-III-VII", "emotional, uplifting",       { 0, 5, 2, 6 } },
            { "i-iv-VI-v",    "melancholic, driving",       { 0, 3, 5, 4 } },
        };
        return catalogue;
    }

    int SongTrack::noteCount() const
    {
        int n = 0;
        for (auto& c : clips)
            n += (int) c.notes.size();
        return n;
    }

    std::string Song::keyName() const
    {
        // Conventional spelling: flats for the keys musicians write with flats.
        static const char* kMinor[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };
        static const char* kMajor[12] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        const int pc = ((rootNote % 12) + 12) % 12;
        std::string name = scale == ScaleType::Ionian ? kMajor[pc] : kMinor[pc];
        switch (scale)
        {
            case ScaleType::Aeolian: return name + " minor";
            case ScaleType::Dorian:  return name + " dorian";
            case ScaleType::Ionian:  return name + " major";
        }
        return name;
    }

    namespace songdetail
    {
        // Plain modulo draws only - std::uniform_int_distribution's output
        // is implementation-defined, and the same seed must produce the
        // same song on the user's Mac (libc++) as in these tests.
        int pick(std::mt19937& rng, int n) { return n <= 1 ? 0 : (int) (rng() % (uint32_t) n); }

        // splitmix32-style finaliser so neighbouring seeds (1, 2, 3...)
        // start genuinely unrelated RNG streams.
        uint32_t mixSeed(uint32_t x)
        {
            x += 0x9e3779b9u;
            x = (x ^ (x >> 16)) * 0x85ebca6bu;
            x = (x ^ (x >> 13)) * 0xc2b2ae35u;
            return x ^ (x >> 16);
        }

        bool isGrooveKind(MusicSection k)
        {
            return k == MusicSection::Drop || k == MusicSection::FinalDrop;
        }

        // Octave-wrap `pitch` into [lo, lo+12).
        int wrapInto(int pitch, int lo)
        {
            while (pitch < lo)       pitch += 12;
            while (pitch >= lo + 12) pitch -= 12;
            return pitch;
        }

        int clampVel(int v) { return std::max(1, std::min(127, v)); }

        std::vector<SongClip> splitIntoClips(const Song& song, std::vector<AbsNote> notes)
        {
            std::stable_sort(notes.begin(), notes.end(), [](const AbsNote& a, const AbsNote& b)
                             { return a.start < b.start || (a.start == b.start && a.pitch < b.pitch); });

            // Same-pitch notes must never overlap (MIDI note-off pairing and
            // Live's per-key note lists both assume it): trim each note to
            // the next onset of its pitch, and drop exact duplicates.
            {
                std::vector<AbsNote> cleaned;
                std::vector<int> lastIndexOfPitch(128, -1);
                for (const AbsNote& a : notes)
                {
                    int& last = lastIndexOfPitch[(size_t) std::max(0, std::min(127, a.pitch))];
                    if (last >= 0)
                    {
                        AbsNote& prev = cleaned[(size_t) last];
                        if (prev.start == a.start)
                            continue;
                        prev.length = std::min(prev.length, a.start - prev.start);
                    }
                    last = (int) cleaned.size();
                    cleaned.push_back(a);
                }
                notes.swap(cleaned);
            }

            std::vector<SongClip> clips;
            size_t n = 0;
            for (const SongSection& s : song.sections)
            {
                const double begin = s.startBar * 4.0;
                const double end   = (s.startBar + s.bars) * 4.0;
                SongClip clip;
                clip.name     = s.name;
                clip.startBar = s.startBar;
                clip.bars     = s.bars;
                while (n < notes.size() && notes[n].start < end)
                {
                    const AbsNote& a = notes[n++];
                    if (a.start < begin)
                        continue;
                    const double len = std::min(a.length, end - a.start);
                    clip.notes.push_back({ a.start - begin, len, a.pitch, a.velocity });
                }
                if (!clip.notes.empty())
                    clips.push_back(std::move(clip));
            }
            return clips;
        }
    }

    namespace
    {
        using namespace songdetail;
        constexpr int kStepsPerBar = 16;
        constexpr double kBeatsPerStep = 0.25;

        int scaleSemitone(const Song& song, int degree) { return degreeToSemitone(degree, song.scale); }

        // ---------------- drums ----------------

        void addDrumLane(std::vector<AbsNote>& out, const StepArray& steps, int pitch)
        {
            const std::vector<int> vel = toVelocityArray(steps);
            for (size_t s = 0; s < vel.size(); ++s)
                if (vel[s] > 0)
                    out.push_back({ s * kBeatsPerStep, kBeatsPerStep, pitch, clampVel(vel[s]) });
        }

        // ---------------- bass ----------------

        // Bass pitch for a theme offset over the current chord: follows the
        // chord root, stays diatonic, and is octave-wrapped into the
        // measured F1-C3 register (research 11.5 - same band as
        // PluginProcessor.h's clampBassRegisterPitch).
        int bassPitch(const Song& song, int chordDegree, int offsetFromRoot)
        {
            const int chordRoot = scaleSemitone(song, chordDegree);
            const int rel       = nearestInScale(chordRoot + offsetFromRoot, song.scale);
            int p = 36 + song.rootNote + rel;
            while (p > 48) p -= 12;
            while (p < 29) p += 12;
            return p;
        }

        void generateBass(Song& song, const MusicArrangement& arr, uint32_t seed, std::vector<AbsNote>& out)
        {
            // ONE 16-bar theme for the whole song (see header comment).
            StepGridConfig grid;
            grid.stepsPerBar = kStepsPerBar;
            grid.numBars     = 16;
            DrumPatternParams dp;
            dp.seed = seed ^ 0x5bd1e995u;
            const DropPattern themeDrums = generateDrop(grid, dp);
            BassPatternParams bp;
            bp.seed = seed ^ 0x27d4eb2fu;
            const BassLoop16 theme = generateBassLoop16(themeDrums, bp);
            const int themeSteps = (int) theme.pitchOffsets.size();

            for (int bar = 0; bar < arr.totalBars(); ++bar)
            {
                const MusicState& st = arr.barStates[(size_t) bar];
                if (isPreDropFinalBar(arr, bar))
                    continue; // leave space, same rule the drums follow
                const int chord = song.chordDegreeByBar[(size_t) bar];
                const double barBeat = bar * 4.0;
                const float e = st.bassEnergy;

                if (e >= 0.45f)
                {
                    // Full theme (>=0.7) or its 8th-grid skeleton (0.45-0.7).
                    const bool full = e >= 0.7f;
                    const int themeBar = st.barInSection % 16;
                    for (int s = 0; s < kStepsPerBar; ++s)
                    {
                        const int idx = (themeBar * kStepsPerBar + s) % themeSteps;
                        const int off = theme.pitchOffsets[(size_t) idx];
                        if (off == kBassOffValue)
                            continue;
                        if (!full && (s % 2) != 0)
                            continue;

                        int gate = theme.gateLengthSteps[(size_t) idx];
                        if (gate <= 0)
                        {
                            // No explicit gate: hold until the next onset, max 2 steps.
                            gate = 1;
                            while (gate < 2 && idx + gate < themeSteps
                                   && theme.pitchOffsets[(size_t) (idx + gate)] == kBassOffValue)
                                ++gate;
                        }
                        const int vel = (s % 4 == 2) ? 112 : (s % 4 == 0 ? 100 : 92);
                        out.push_back({ barBeat + s * kBeatsPerStep, gate * kBeatsPerStep * 0.9,
                                        bassPitch(song, chord, off), clampVel((int) (vel * (0.75f + 0.25f * e))) });
                    }
                }
                else if (e >= 0.2f)
                {
                    // Classic rolling off-beat root - a build/thinning state.
                    for (int s = 2; s < kStepsPerBar; s += 4)
                        out.push_back({ barBeat + s * kBeatsPerStep, 2 * kBeatsPerStep * 0.85,
                                        bassPitch(song, chord, 0), 100 });
                }
                else if (e >= 0.08f && st.barInSection % 4 == 0)
                {
                    // "Largely disappear... possibly retain an occasional
                    // root" (BassEngine.h's own breakdown brief): one long
                    // sub root every 4 bars.
                    out.push_back({ barBeat, 4.0 * 3.5, bassPitch(song, chord, 0), 84 });
                }
            }
        }

        // ---------------- harmony / pad ----------------

        std::array<int, 4> chordTones(const Song& song, int degree)
        {
            return { scaleSemitone(song, degree), scaleSemitone(song, degree + 2),
                     scaleSemitone(song, degree + 4), scaleSemitone(song, degree + 6) };
        }

        // Close-position 7th-chord voicing with the smallest total movement
        // from `prev` (smooth voice leading), lowest note kept in [48, 60).
        std::array<int, 4> voicePad(const Song& song, int degree, const std::array<int, 4>* prev)
        {
            const std::array<int, 4> tones = chordTones(song, degree);
            std::array<int, 4> best {};
            int bestCost = 1 << 30;
            for (int inv = 0; inv < 4; ++inv)
            {
                std::array<int, 4> v {};
                int last = -1;
                for (int i = 0; i < 4; ++i)
                {
                    const int p = wrapInto(48 + song.rootNote + tones[(size_t) ((inv + i) % 4)],
                                           i == 0 ? 48 : last + 1); // next chord tone above the previous voice
                    v[(size_t) i] = last = p;
                }
                int cost = 0;
                if (prev != nullptr)
                    for (int i = 0; i < 4; ++i)
                        cost += std::abs(v[(size_t) i] - (*prev)[(size_t) i]);
                else
                    cost = std::abs(v[0] - 55); // centre the first chord around G3
                if (cost < bestCost)
                {
                    bestCost = cost;
                    best     = v;
                }
            }
            return best;
        }

        void generatePad(Song& song, const MusicArrangement& arr, std::vector<AbsNote>& out)
        {
            std::array<int, 4> prev {};
            bool havePrev = false;
            int bar = 0;
            while (bar < arr.totalBars())
            {
                // A pad note lasts until the chord changes or the section ends.
                const MusicState& st = arr.barStates[(size_t) bar];
                const int chord = song.chordDegreeByBar[(size_t) bar];
                int end = bar + 1;
                while (end < arr.totalBars() && song.chordDegreeByBar[(size_t) end] == chord
                       && arr.barStates[(size_t) end].barInSection != 0)
                    ++end;

                const bool fadingOutroTail = st.section == MusicSection::Outro
                                             && st.barInSection >= st.barsInSection - 8;
                if (st.melodicEnergy >= 0.4f && !fadingOutroTail)
                {
                    const std::array<int, 4> v = voicePad(song, chord, havePrev ? &prev : nullptr);
                    prev = v;
                    havePrev = true;
                    const int vel = clampVel((int) (60 + 45 * st.melodicEnergy));
                    for (int p : v)
                        out.push_back({ bar * 4.0, (end - bar) * 4.0, p, vel });
                }
                bar = end;
            }
        }

        // ---------------- arp ----------------

        // 16-step shapes, values index the arp's tone ladder (root, 3rd,
        // 5th across two octaves), -1 = rest. Research section 7: "arpeggios
        // at 16th-note grid with 50-75% gate and 3 velocity tiers".
        constexpr int kArpShapes[][16] = {
            { 0, 2, 3, 2,  4, 2, 3, 2,  0, 2, 3, 2,  5, 4, 3, 2 }, // rolling
            { 0,-1, 3, 2,  0,-1, 4, 3,  0,-1, 3, 2,  5,-1, 4, 3 }, // gated
            { 0, 1, 2, 3,  4, 3, 2, 1,  0, 1, 2, 3,  5, 4, 3, 2 }, // up-down
            { 3, 0, 2, 0,  4, 0, 2, 0,  5, 0, 2, 0,  4, 0, 3, 0 }, // pedal root
            { 0, 2, 4,-1,  3, 2, 4,-1,  0, 2, 5,-1,  4, 3, 2,-1 }, // triplet-ish
        };
        constexpr int kNumArpShapes = (int) (sizeof(kArpShapes) / sizeof(kArpShapes[0]));

        bool arpActive(const MusicState& st, const MusicArrangement& arr, int bar)
        {
            if (isPreDropFinalBar(arr, bar))
                return false;
            switch (st.section)
            {
                case MusicSection::Build:
                case MusicSection::Drop:
                case MusicSection::FinalDrop:
                case MusicSection::BreakdownBuild:
                case MusicSection::PreDrop:
                    return true;
                case MusicSection::Establish:
                    return st.melodicEnergy >= 0.55f;
                case MusicSection::Breakdown:
                    // Enters half-way through the break to build tension.
                    return st.barInSection >= st.barsInSection / 2;
                case MusicSection::Intro:
                case MusicSection::Outro:
                    return false;
            }
            return false;
        }

        void generateArp(Song& song, const MusicArrangement& arr, std::mt19937& rng, std::vector<AbsNote>& out)
        {
            const int shape      = pick(rng, kNumArpShapes);
            const int varShape   = (shape + 1 + pick(rng, kNumArpShapes - 1)) % kNumArpShapes;
            const double gate    = 0.5 + 0.25 * (pick(rng, 3) / 2.0); // 50 / 62.5 / 75 %

            for (int bar = 0; bar < arr.totalBars(); ++bar)
            {
                const MusicState& st = arr.barStates[(size_t) bar];
                if (!arpActive(st, arr, bar))
                    continue;
                const int chord = song.chordDegreeByBar[(size_t) bar];
                const std::array<int, 4> t = chordTones(song, chord);
                const int base = wrapInto(60 + song.rootNote + t[0], 55);
                const int ladder[7] = { base, base + (t[1] - t[0]), base + (t[2] - t[0]),
                                        base + 12, base + 12 + (t[1] - t[0]), base + 12 + (t[2] - t[0]), base + 24 };

                // Sparse (8th-only) while the section is still building.
                const bool sparse = st.melodicEnergy < 0.65f || st.section == MusicSection::Breakdown
                                    || (st.section == MusicSection::Build && st.barInSection < st.barsInSection / 2);
                // "Introduce a small variation every 4-8 bars" (research 7):
                // the last beat of every 4th bar borrows from a second shape.
                const bool varied = st.barInSection % 4 == 3;

                for (int s = 0; s < kStepsPerBar; ++s)
                {
                    const int idx = (varied && s >= 12) ? kArpShapes[varShape][s] : kArpShapes[shape][s];
                    if (idx < 0 || (sparse && s % 2 != 0))
                        continue;
                    const int vel = s % 4 == 0 ? 112 + pick(rng, 9) : (s % 2 == 0 ? 85 + pick(rng, 11) : 65 + pick(rng, 11));
                    const double velScale = st.section == MusicSection::Breakdown ? 0.8 : 1.0;
                    out.push_back({ bar * 4.0 + s * kBeatsPerStep, kBeatsPerStep * gate, ladder[idx],
                                    clampVel((int) (vel * velScale)) });
                }
            }
        }

        // ---------------- lead ----------------

        struct MotifNote
        {
            int start, length; // steps within a 2-bar (32-step) phrase half
            int degree;        // scale degrees above the current chord root
        };

        // 2-bar rhythm templates - (start, length) in 16ths.
        constexpr int kLeadRhythms[][7][2] = {
            { { 0, 4 }, { 6, 2 }, { 8, 6 }, { 16, 4 }, { 22, 2 }, { 24, 8 }, { -1, 0 } },
            { { 0, 6 }, { 6, 6 }, { 12, 4 }, { 16, 12 }, { 28, 4 }, { -1, 0 }, { -1, 0 } },
            { { 2, 2 }, { 4, 4 }, { 10, 2 }, { 12, 4 }, { 18, 2 }, { 20, 4 }, { 26, 6 } },
            { { 0, 3 }, { 3, 3 }, { 6, 4 }, { 10, 6 }, { 16, 3 }, { 19, 3 }, { 22, 10 } },
        };
        constexpr int kNumLeadRhythms = (int) (sizeof(kLeadRhythms) / sizeof(kLeadRhythms[0]));

        // Research section 7: "2-bar phrase motifs with call-and-response
        // structure; simple motifs on scale degrees 1-3-5-7". The call ends
        // unresolved, the response copies it and resolves to the root.
        void buildLeadMotif(std::mt19937& rng, std::vector<MotifNote>& call, std::vector<MotifNote>& response)
        {
            const int r = pick(rng, kNumLeadRhythms);
            static constexpr int kChordDegrees[] = { 0, 2, 4, 6, 7, 9 };
            int deg = pick(rng, 2) == 0 ? 4 : 7;
            for (int i = 0; i < 7 && kLeadRhythms[r][i][0] >= 0; ++i)
            {
                call.push_back({ kLeadRhythms[r][i][0], kLeadRhythms[r][i][1], deg });
                // Mostly stepwise between chord tones, occasional leap.
                int j = 0;
                while (kChordDegrees[j] != deg && j < 5) ++j;
                const int move = pick(rng, 4) == 0 ? 2 : 1;
                j += pick(rng, 2) == 0 ? move : -move;
                j = std::max(0, std::min(5, j));
                deg = kChordDegrees[j];
            }
            static constexpr int kUnresolved[] = { 1, 3, 4 };
            call.back().degree = kUnresolved[pick(rng, 3)];

            response = call;
            response.back().degree = pick(rng, 2) == 0 ? 0 : 7;
            if (response.size() >= 2)
                response[response.size() - 2].degree = 2;
        }

        bool leadActive(const MusicState& st)
        {
            // The hook is revealed in the break and then carried by the
            // later drops - Drop 1 stays the "subtle impact" drop.
            return st.section == MusicSection::Breakdown || st.section == MusicSection::BreakdownBuild
                   || st.section == MusicSection::FinalDrop;
        }

        void generateLead(Song& song, const MusicArrangement& arr, std::mt19937& rng, std::vector<AbsNote>& out)
        {
            std::vector<MotifNote> call, response;
            buildLeadMotif(rng, call, response);

            for (int bar = 0; bar < arr.totalBars(); bar += 2)
            {
                const MusicState& st = arr.barStates[(size_t) bar];
                if (!leadActive(st))
                    continue;
                const bool isResponse = (st.barInSection / 2) % 2 == 1;
                const std::vector<MotifNote>& half = isResponse ? response : call;
                for (size_t i = 0; i < half.size(); ++i)
                {
                    const MotifNote& n = half[i];
                    const int absBar = bar + n.start / kStepsPerBar;
                    if (absBar >= arr.totalBars() || arr.barStates[(size_t) absBar].section != st.section)
                        continue;
                    const int chord = song.chordDegreeByBar[(size_t) absBar];
                    const int rootPc = wrapInto(60 + song.rootNote + scaleSemitone(song, chord), 60);
                    const int pitch = rootPc + scaleSemitone(song, chord + n.degree) - scaleSemitone(song, chord);
                    const int vel = i == 0 ? 104 : (i % 2 == 0 ? 96 : 90);
                    const double velScale = st.section == MusicSection::FinalDrop ? 1.0 : 0.9;
                    out.push_back({ bar * 4.0 + n.start * kBeatsPerStep, n.length * kBeatsPerStep * 0.95, pitch,
                                    clampVel((int) (vel * velScale)) });
                }
            }
        }

        // ---------------- FX ----------------

        void generateFx(const Song& song, std::vector<AbsNote>& out)
        {
            for (size_t i = 0; i < song.sections.size(); ++i)
            {
                const SongSection& s = song.sections[i];
                const double start = s.startBar * 4.0;
                if (isGrooveKind(s.kind))
                    out.push_back({ start, 4.0, SongFxNotes::kImpact, 127 });
                if (s.kind == MusicSection::Breakdown || s.kind == MusicSection::Outro)
                {
                    out.push_back({ start, 4.0, SongFxNotes::kImpact, 110 });
                    out.push_back({ start, 8.0, SongFxNotes::kDownlifter, 110 });
                }
                // Riser through (at most) the last 8 bars before every drop.
                if (i + 1 < song.sections.size() && isGrooveKind(song.sections[i + 1].kind))
                {
                    const int riserBars = std::min(8, s.bars);
                    const double riserStart = (s.startBar + s.bars - riserBars) * 4.0;
                    out.push_back({ riserStart, riserBars * 4.0, SongFxNotes::kRiser, 120 });
                }
            }
        }

    }

    const char* styleName(SongStyle style)
    {
        switch (style)
        {
            case SongStyle::Trance:        return "Trance";
            case SongStyle::NeoRave:       return "Neo-Rave Trance";
            case SongStyle::MelodicTechno: return "Melodic Techno";
            case SongStyle::ProgressiveTechno: return "Progressive Techno";
        }
        return "?";
    }

    double defaultBpm(SongStyle style)
    {
        switch (style)
        {
            case SongStyle::Trance:        return 140.0;
            case SongStyle::NeoRave:       return 147.0;
            case SongStyle::MelodicTechno: return 124.0;
            case SongStyle::ProgressiveTechno: return 125.0;
        }
        return 124.0;
    }

    Song generateSong(const SongParams& paramsIn)
    {
        SongParams params = paramsIn;
        if (params.bpm <= 0.0)
            params.bpm = defaultBpm(params.style);
        if (params.style == SongStyle::ProgressiveTechno)
            return generateProgressiveSong(params);
        if (params.style != SongStyle::MelodicTechno)
            return generateTranceSong(params);

        Song song;
        song.style    = params.style;
        song.bpm      = params.bpm;
        song.rootNote = ((params.rootNote % 12) + 12) % 12;
        song.scale    = params.scale;
        song.seed     = params.seed;

        std::mt19937 rng(mixSeed(params.seed));

        // Timeline.
        ArrangementConfig cfg;
        cfg.sections.clear();
        for (const auto& s : params.structure)
            cfg.sections.push_back(s.spec);
        cfg.bpm      = params.bpm;
        cfg.rootNote = song.rootNote;
        cfg.seed     = params.seed;
        const MusicArrangement arr = buildArrangement(cfg);
        song.totalBars = arr.totalBars();

        int bar = 0;
        for (const auto& s : params.structure)
        {
            song.sections.push_back({ s.name, s.spec.section, bar, s.spec.bars });
            bar += s.spec.bars;
        }

        // Harmony.
        const auto& catalogue = progressionCatalogue();
        const int progIndex = (params.progressionIndex >= 0 && params.progressionIndex < (int) catalogue.size())
                                  ? params.progressionIndex
                                  : pick(rng, (int) catalogue.size());
        song.progression = catalogue[(size_t) progIndex];
        const int barsPerChord = std::max(1, params.barsPerChord);
        for (int b = 0; b < song.totalBars; ++b)
            song.chordDegreeByBar.push_back(song.progression.degrees[(b / barsPerChord) % 4]);

        char title[96];
        std::snprintf(title, sizeof(title), "Melodic Techno %04u - %s %d BPM", (unsigned) (params.seed % 10000),
                      song.keyName().c_str(), (int) (params.bpm + 0.5));
        song.title = title;

        // Drums - the existing section-aware whole-arrangement generator.
        DrumPatternParams dp;
        dp.seed = params.seed;
        const DropPattern drums = generateArrangementDrop(arr, kStepsPerBar, dp);

        std::vector<AbsNote> kick, clap, hats, perc, bass, pad, arp, lead, fx;
        addDrumLane(kick, drums.kick, SongDrumNotes::kKick);
        addDrumLane(clap, drums.clap, SongDrumNotes::kClap);
        addDrumLane(hats, drums.hatClosed, SongDrumNotes::kHatClosed);
        addDrumLane(hats, drums.hatOpen, SongDrumNotes::kHatOpen);
        addDrumLane(perc, drums.percA, SongDrumNotes::kRim);
        addDrumLane(perc, drums.percB, SongDrumNotes::kPercB);

        generateBass(song, arr, params.seed, bass);
        generatePad(song, arr, pad);
        generateArp(song, arr, rng, arp);
        generateLead(song, arr, rng, lead);
        generateFx(song, fx);

        // Colour indices into Live's 70-entry clip colour palette.
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
        addTrack("Bass", SongTrackRole::Bass, 26, bass);
        addTrack("Pad", SongTrackRole::Pad, 23, pad);
        addTrack("Arp", SongTrackRole::Arp, 19, arp);
        addTrack("Lead", SongTrackRole::Lead, 21, lead);
        addTrack("FX", SongTrackRole::Fx, 13, fx);
        return song;
    }
}
