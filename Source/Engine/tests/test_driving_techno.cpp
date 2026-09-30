// Regression tests for SongStyle::DrivingTechno (DrivingTechnoGenerator.cpp),
// checked against the techniques measured in the reference track
// (MLPipeline/musical_target/driving_techno_reference.md).
#include "../SongGenerator.h"
#include "../MidiFileWriter.h"
#include "../AlsWriter.h"
#include "TestSupport.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace Engine;

namespace
{
    const SongTrack* track(const Song& s, const std::string& name)
    {
        for (auto& t : s.tracks)
            if (t.name == name) return &t;
        return nullptr;
    }

    const SongSection* section(const Song& s, const std::string& name)
    {
        for (auto& x : s.sections)
            if (x.name == name) return &x;
        return nullptr;
    }

    // absolute 16th step -> pitches starting there
    std::map<int, std::vector<int>> steps(const SongTrack* t)
    {
        std::map<int, std::vector<int>> m;
        for (auto& c : t->clips)
            for (auto& n : c.notes)
                m[(int) std::lround((c.startBar * 4.0 + n.startBeat) * 4.0)].push_back(n.pitch);
        return m;
    }

    void checkSong(const Song& song)
    {
        CHECK(song.totalBars == 144);
        for (const char* name : { "Kick", "Clap", "Hats", "Perc", "Snare Roll", "Rumble", "Stab", "Pad", "Pulse", "Lead", "FX" })
        {
            const SongTrack* t = track(song, name);
            CHECK(t != nullptr && t->noteCount() > 0);
        }
        // Notes inside clips, no same-pitch overlaps.
        for (auto& t : song.tracks)
            for (auto& c : t.clips)
            {
                std::map<int, double> lastEnd;
                for (auto& n : c.notes)
                {
                    CHECK(n.startBeat >= 0.0 && n.startBeat + n.lengthBeats <= c.bars * 4.0 + 1e-9);
                    auto it = lastEnd.find(n.pitch);
                    CHECK(it == lastEnd.end() || n.startBeat >= it->second - 1e-9);
                    lastEnd[n.pitch] = n.startBeat + n.lengthBeats;
                }
            }

        const auto kick = steps(track(song, "Kick"));
        const auto rumble = steps(track(song, "Rumble"));
        const auto stab = steps(track(song, "Stab"));
        const auto hats = steps(track(song, "Hats"));
        const auto clap = steps(track(song, "Clap"));

        // The groove in the drop: kick on the beat; rumble on the 3rd+4th 16th only ("k . b b");
        // stab on every 16th except the kick; open hat on the 8th offbeat; clap on 2 and 4.
        const SongSection* drop = section(song, "Drop");
        for (int b = drop->startBar; b < drop->startBar + 4; ++b)
            for (int beat = 0; beat < 4; ++beat)
            {
                const int s0 = b * 16 + beat * 4;
                CHECK(kick.count(s0) && !kick.count(s0 + 2));
                CHECK(!rumble.count(s0) && !rumble.count(s0 + 1) && rumble.count(s0 + 2) && rumble.count(s0 + 3));
                CHECK(!stab.count(s0) && stab.count(s0 + 1) && stab.count(s0 + 2) && stab.count(s0 + 3));
                bool openHat = false;
                for (int p : hats.count(s0 + 2) ? hats.at(s0 + 2) : std::vector<int>{}) openHat |= p == SongDrumNotes::kHatOpen;
                CHECK(openHat);
                CHECK(clap.count(s0) == (beat == 1 || beat == 3 ? 1u : 0u));
            }

        // No kick in the breakdown; in the build only bars 4-11 (then a 4-bar hole into the drop).
        const SongSection* bd = section(song, "Breakdown");
        const SongSection* build = section(song, "Build");
        for (auto& [st, ps] : kick)
        {
            const int bar = st / 16;
            CHECK(!(bar >= bd->startBar && bar < bd->startBar + bd->bars));
            if (bar >= build->startBar && bar < build->startBar + build->bars)
                CHECK(bar - build->startBar >= 4 && bar - build->startBar < 12);
        }

        // Theme (composed by the learned melody model): diatonic, a real
        // phrase in each 8 bars of the drop (>= 16 notes, >= 5 distinct
        // pitches, range >= 5 semitones), ending on a common corpus ending, and
        // no sustained (>= a quarter) note a semitone from the tonic/5th pedal.
        for (auto& c : track(song, "Lead")->clips)
            for (auto& n : c.notes)
                CHECK(isInScale(n.pitch - song.rootNote, song.scale));
        {
            std::vector<std::pair<double, SongNote>> dropLead;
            for (auto& c : track(song, "Lead")->clips)
                for (auto& n : c.notes)
                {
                    const double t = c.startBar * 4.0 + n.startBeat;
                    if (t >= drop->startBar * 4.0 && t < (drop->startBar + 8) * 4.0) dropLead.push_back({ t, n });
                }
            std::sort(dropLead.begin(), dropLead.end(), [](auto& x, auto& y) { return x.first < y.first; });
            std::set<int> distinct;
            int lo = 999, hi = 0;
            for (auto& [t, n] : dropLead)
            {
                distinct.insert(n.pitch);
                lo = std::min(lo, n.pitch);
                hi = std::max(hi, n.pitch);
                if (n.lengthBeats >= 0.95)
                    for (int ped : { song.rootNote, song.rootNote + 7 })
                    {
                        const int iv = ((n.pitch - ped) % 12 + 12) % 12;
                        CHECK(iv != 1 && iv != 11);
                    }
            }
            CHECK(dropLead.size() >= 16);
            CHECK(distinct.size() >= 5);
            CHECK(hi - lo >= 5);
            if (!dropLead.empty())
            {
                const int rel = ((dropLead.back().second.pitch - song.rootNote) % 12 + 12) % 12;
                // The corpus's four common phrase endings (1, 5, 2, b7 - 70% of
                // endings), or a tone of the theme's second chord, which its last bars sit over.
                const int progB = song.progression.degrees[1];
                bool tonOfB = false;
                for (int k : { 0, 2, 4 })
                    tonOfB |= ((dropLead.back().second.pitch - song.rootNote - degreeToSemitone(progB + k, song.scale)) % 12 + 12) % 12 == 0;
                CHECK(rel == 0 || rel == 7 || rel == 2 || rel == 10 || tonOfB);
            }
        }

        // Breakdown pulse: 16 repeated 16ths per bar, at most two pitches per bar, diatonic.
        const auto pulse = steps(track(song, "Pulse"));
        for (int b = bd->startBar; b < bd->startBar + bd->bars; ++b)
        {
            std::set<int> ps;
            int count = 0;
            for (int s = 0; s < 16; ++s)
                if (pulse.count(b * 16 + s)) { ++count; ps.insert(pulse.at(b * 16 + s)[0]); }
            CHECK(count == 16);
            CHECK(ps.size() >= 1 && ps.size() <= 2);
            for (int p : ps) CHECK(isInScale(p - song.rootNote, song.scale));
        }

        // Build: a bII major triad in the pad at bar 2, then the pulse climbs
        // chromatically (non-decreasing, <= 2 semitones per bar) to its octave.
        {
            bool flat2 = false;
            for (auto& c : track(song, "Pad")->clips)
                for (auto& n : c.notes)
                    if (std::fabs(c.startBar * 4.0 + n.startBeat - (build->startBar + 2) * 4.0) < 1e-9
                        && ((n.pitch - song.rootNote - 1) % 12 + 12) % 12 == 0)
                        flat2 = true;
            CHECK(flat2);
            int prev = -1, first = -1, last = -1;
            for (int b = 3; b < 15; ++b)
            {
                const int p = pulse.at((build->startBar + b) * 16)[0];
                if (first < 0) first = p;
                if (prev >= 0) CHECK(p >= prev && p - prev <= 2);
                prev = last = p;
            }
            CHECK(last - first >= 9);
        }

        CHECK(buildAlsXml(song).find('@') == std::string::npos);
    }
}

int main()
{
    SongParams p;
    p.style = SongStyle::DrivingTechno;
    p.bpm = 0.0;
    p.rootNote = 9;
    p.seed = 1;
    p.progressionIndex = 0;
    const Song song = generateSong(p);
    CHECK(song.bpm == 128.0);
    CHECK(song.keyName() == "A minor");
    checkSong(song);

    for (uint32_t seed = 1; seed <= 12; ++seed)
    {
        SongParams q = p;
        q.seed = seed;
        q.rootNote = (int) ((seed * 7) % 12);
        q.progressionIndex = (int) (seed % 4);
        checkSong(generateSong(q));
    }

    CHECK(writeMidiFile(generateSong(p)) == writeMidiFile(song));
    TEST_SUMMARY_AND_EXIT();
}
