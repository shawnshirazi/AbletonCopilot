// Regression tests for SongStyle::ProgressiveTechno (ProgressiveGenerator.cpp):
// the arp's polymeter, clean pad voicings, and the lead composer's quality
// rules (no semitone clashes with the arp/pad, resolves home, mostly
// stepwise, sensible range, no see-sawing) across many seeds and keys.
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

    struct Abs { double start, len; int pitch; };

    std::vector<Abs> absNotes(const SongTrack* t)
    {
        std::vector<Abs> v;
        for (auto& c : t->clips)
            for (auto& n : c.notes)
                v.push_back({ c.startBar * 4.0 + n.startBeat, n.lengthBeats, n.pitch });
        std::sort(v.begin(), v.end(), [](const Abs& a, const Abs& b) { return a.start < b.start || (a.start == b.start && a.pitch < b.pitch); });
        return v;
    }

    void checkSong(const Song& song)
    {
        // Every part present; pitched parts diatonic; notes inside clips; no same-pitch overlaps.
        for (const char* name : { "Kick", "Clap", "Hats", "Perc", "Snare Roll", "Bass", "Pad", "Arp", "Lead", "FX" })
        {
            const SongTrack* t = track(song, name);
            CHECK(t != nullptr && t->noteCount() > 0);
        }
        for (const char* name : { "Bass", "Pad", "Arp", "Lead" })
            for (auto& c : track(song, name)->clips)
                for (auto& n : c.notes)
                    CHECK(isInScale(n.pitch - song.rootNote, song.scale) && n.pitch >= 24 && n.pitch <= 96);
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

        // Arp: continuous 16ths in the Theme, repeating with a 5- or 6-step
        // period against the 16-step bar (polymeter), not a 4-step one.
        {
            const auto arp = absNotes(track(song, "Arp"));
            const SongSection* th = section(song, "Theme");
            std::vector<int> seq;
            for (auto& n : arp)
                if (n.start >= th->startBar * 4.0 && n.start < th->startBar * 4.0 + 8.0) seq.push_back(n.pitch % 12);
            CHECK(seq.size() == 32);
            int best = 0, bestLag = 0;
            for (int lag = 3; lag <= 8; ++lag)
            {
                int same = 0;
                for (size_t i = 0; i + (size_t) lag < seq.size(); ++i) same += seq[i] == seq[i + (size_t) lag];
                if (same > best) { best = same; bestLag = lag; }
            }
            CHECK(bestLag == 5 || bestLag == 6);
        }

        // Pad: no sustained semitone clusters.
        {
            std::map<double, std::vector<int>> chords;
            for (auto& n : absNotes(track(song, "Pad"))) chords[n.start].push_back(n.pitch);
            for (auto& [t, ps] : chords)
                for (size_t i = 0; i < ps.size(); ++i)
                    for (size_t j = 0; j < ps.size(); ++j)
                        CHECK(std::abs(ps[i] - ps[j]) != 1);
        }

        // Lead quality.
        const auto lead = absNotes(track(song, "Lead"));
        const auto arp  = absNotes(track(song, "Arp"));
        const auto pad  = absNotes(track(song, "Pad"));
        CHECK(lead.size() > 60);
        int clashes = 0, steps = 0, seesaw = 0, lo = 999, hi = 0;
        for (size_t i = 0; i < lead.size(); ++i)
        {
            const Abs& n = lead[i];
            std::set<int> pcs;
            for (const auto* v : { &arp, &pad })
                for (auto& o : *v)
                    if (o.start < n.start + n.len * 0.75 && o.start + o.len > n.start + 0.01) pcs.insert(o.pitch % 12);
            if (!pcs.count(n.pitch % 12))
                for (int q : pcs)
                    if ((n.pitch - q + 12) % 12 == 1 || (q - n.pitch + 12) % 12 == 1) { ++clashes; break; }
            lo = std::min(lo, n.pitch);
            hi = std::max(hi, n.pitch);
            if (i > 0 && std::abs(n.pitch - lead[i - 1].pitch) <= 2) ++steps;
            if (i >= 3 && n.pitch == lead[i - 2].pitch && lead[i - 1].pitch == lead[i - 3].pitch && n.pitch != lead[i - 1].pitch) ++seesaw;
        }
        CHECK(clashes * 50 <= (int) lead.size());                 // <= 2% of notes
        CHECK((double) steps / (double) (lead.size() - 1) >= 0.5); // mostly stepwise
        CHECK(hi - lo <= 17);
        CHECK(lo >= 55 && hi <= 84);
        CHECK(seesaw * 10 <= (int) lead.size());

        // The Main Drop's first phrase ends on the tonic (or the key's 5th).
        {
            const SongSection* md = section(song, "Main Drop");
            const Abs* last = nullptr;
            for (auto& n : lead)
                if (n.start >= md->startBar * 4.0 && n.start < (md->startBar + 8) * 4.0) last = &n;
            CHECK(last != nullptr);
            if (last != nullptr)
            {
                const int rel = ((last->pitch - song.rootNote) % 12 + 12) % 12;
                CHECK(rel == 0 || rel == 7);
            }
        }

        // Arrangement: no kick in the breakdown, four-on-the-floor in the main drop.
        {
            const SongSection* bd = section(song, "Breakdown");
            const SongSection* md = section(song, "Main Drop");
            int kb = 0, kd = 0;
            for (auto& n : absNotes(track(song, "Kick")))
            {
                if (n.start >= bd->startBar * 4.0 && n.start < (bd->startBar + bd->bars) * 4.0) ++kb;
                if (n.start >= md->startBar * 4.0 && n.start < (md->startBar + md->bars) * 4.0) ++kd;
            }
            CHECK(kb == 0);
            CHECK(kd == md->bars * 4);
        }
        CHECK(buildAlsXml(song).find('@') == std::string::npos);
    }
}

int main()
{
    SongParams p;
    p.style = SongStyle::ProgressiveTechno;
    p.bpm = 0.0;
    p.rootNote = 9;
    p.seed = 2;
    const Song song = generateSong(p);
    CHECK(song.bpm == 125.0);
    CHECK(song.totalBars == 176);
    CHECK(song.keyName() == "A minor");

    // Many seeds x keys x all 8 progressions.
    int n = 0;
    for (uint32_t seed = 1; seed <= 16; ++seed)
    {
        SongParams q = p;
        q.seed = seed;
        q.rootNote = (int) ((seed * 5) % 12);
        q.progressionIndex = (int) (seed % 8);
        checkSong(generateSong(q));
        ++n;
    }
    CHECK(n == 16);

    // Flat key spelling.
    {
        SongParams q = p;
        q.rootNote = 10;
        CHECK(generateSong(q).keyName() == "Bb minor");
    }

    // Deterministic; different seeds give different songs.
    CHECK(writeMidiFile(generateSong(p)) == writeMidiFile(song));
    {
        SongParams q = p;
        q.seed = 3;
        CHECK(writeMidiFile(generateSong(q)) != writeMidiFile(song));
    }

    TEST_SUMMARY_AND_EXIT();
}
