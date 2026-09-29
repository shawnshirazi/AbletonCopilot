// Regression tests for the Trance / NeoRave styles (Source/Engine/
// TranceGenerator.cpp), checked against the PRISMATIC style spec in
// MLPipeline/musical_target/prismatic_style.md.
#include "../SongGenerator.h"
#include "../MidiFileWriter.h"
#include "../AlsWriter.h"
#include "TestSupport.h"
#include <cmath>
#include <map>
#include <string>

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

    // Absolute-beat -> notes, for one track.
    std::multimap<double, SongNote> notesOf(const SongTrack* t)
    {
        std::multimap<double, SongNote> m;
        if (t != nullptr)
            for (auto& c : t->clips)
                for (auto& n : c.notes)
                    m.insert({ c.startBar * 4.0 + n.startBeat, n });
        return m;
    }

    int countInBar(const std::multimap<double, SongNote>& m, int bar, int pitch = -1)
    {
        int n = 0;
        for (auto it = m.lower_bound(bar * 4.0 - 1e-9); it != m.end() && it->first < bar * 4.0 + 4.0 - 1e-9; ++it)
            n += (pitch < 0 || it->second.pitch == pitch);
        return n;
    }

    bool hasAt(const std::multimap<double, SongNote>& m, double beat, int pitch = -1)
    {
        for (auto it = m.lower_bound(beat - 1e-6); it != m.end() && it->first < beat + 1e-6; ++it)
            if (pitch < 0 || it->second.pitch == pitch) return true;
        return false;
    }

    void commonChecks(const Song& song)
    {
        for (auto& s : song.sections)
            CHECK(s.bars % 8 == 0); // every section a multiple of 8 bars [E]
        for (auto& t : song.tracks)
        {
            CHECK(t.noteCount() > 0);
            for (auto& c : t.clips)
            {
                std::map<int, double> lastEnd;
                for (auto& n : c.notes)
                {
                    CHECK(n.startBeat >= 0.0 && n.lengthBeats > 0.0 && n.startBeat + n.lengthBeats <= c.bars * 4.0 + 1e-9);
                    CHECK(n.velocity >= 1 && n.velocity <= 127);
                    auto it = lastEnd.find(n.pitch);
                    CHECK(it == lastEnd.end() || n.startBeat >= it->second - 1e-9);
                    lastEnd[n.pitch] = n.startBeat + n.lengthBeats;
                }
            }
        }
        for (const char* name : { "Bass", "Pad", "Pluck", "Lead", "Rave Stab", "Acid" })
            if (const SongTrack* t = track(song, name))
                for (auto& c : t->clips)
                    for (auto& n : c.notes)
                    {
                        const bool inScale = isInScale(n.pitch - song.rootNote, song.scale)
                                             || (std::string(name) == "Rave Stab"); // perfect fifths above chord tones may leave the scale on VII/III
                        CHECK(inScale);
                    }

        // Drops: four-on-the-floor, clap on 2 and 4, open hat on every offbeat [E].
        const auto kick = notesOf(track(song, "Kick"));
        const auto clap = notesOf(track(song, "Clap"));
        const auto hats = notesOf(track(song, "Hats"));
        for (auto& s : song.sections)
        {
            const bool drop = s.kind == MusicSection::Drop || s.kind == MusicSection::FinalDrop;
            const bool breakdown = s.kind == MusicSection::Breakdown || s.kind == MusicSection::BreakdownBuild;
            for (int b = s.startBar; b < s.startBar + s.bars; ++b)
            {
                if (drop)
                {
                    CHECK(countInBar(kick, b) == 4);
                    CHECK(hasAt(clap, b * 4.0 + 1.0) && hasAt(clap, b * 4.0 + 3.0));
                    for (int beat = 0; beat < 4; ++beat)
                        CHECK(hasAt(hats, b * 4.0 + beat + 0.5, SongDrumNotes::kHatOpen));
                    // one chord per bar in the drops [I]
                    if (b > s.startBar)
                        CHECK(song.chordDegreeByBar[(size_t) b] == song.progression.degrees[(b - s.startBar) % 4]);
                }
                if (breakdown)
                    CHECK(countInBar(kick, b) == 0); // kick out through breakdown and build [I]
            }
            if (s.kind == MusicSection::Breakdown) // one chord per 2 bars in breakdowns [I]
                for (int b = 0; b + 1 < s.bars; b += 2)
                    CHECK(song.chordDegreeByBar[(size_t) (s.startBar + b)] == song.chordDegreeByBar[(size_t) (s.startBar + b + 1)]);
        }

        // Snare roll accelerates through each Build and the bar before the drop is empty [E].
        const auto roll = notesOf(track(song, "Snare Roll"));
        for (auto& s : song.sections)
        {
            if (s.kind != MusicSection::BreakdownBuild) continue;
            CHECK(countInBar(roll, s.startBar) < countInBar(roll, s.startBar + s.bars - 2));
            CHECK(countInBar(roll, s.startBar + s.bars - 1) == 0);
        }

        // Every drop: impact + crash on the downbeat, a riser ending exactly on it.
        const auto fx = notesOf(track(song, "FX"));
        for (auto& s : song.sections)
        {
            if (s.kind != MusicSection::Drop && s.kind != MusicSection::FinalDrop) continue;
            CHECK(hasAt(fx, s.startBar * 4.0, SongFxNotes::kImpact));
            CHECK(hasAt(fx, s.startBar * 4.0, SongFxNotes::kCrash));
            bool riserEnds = false;
            for (auto& [t, n] : fx)
                if (n.pitch == SongFxNotes::kRiser && std::fabs(t + n.lengthBeats - s.startBar * 4.0) < 1e-6)
                    riserEnds = true;
            CHECK(riserEnds);
        }

        // Exports stay consistent.
        CHECK(writeMidiFile(song).size() > 100);
        CHECK(buildAlsXml(song).find('@') == std::string::npos);
    }
}

int main()
{
    // ---------------- Trance ----------------
    SongParams p;
    p.style    = SongStyle::Trance;
    p.bpm      = 0.0; // style default
    p.rootNote = 7;   // G minor
    p.seed     = 21;
    const Song tr = generateSong(p);
    CHECK(tr.style == SongStyle::Trance);
    CHECK(tr.bpm == 140.0);
    CHECK(tr.totalBars == 192);
    CHECK(tr.keyName() == "G minor");
    CHECK(tr.lengthSeconds() > 300.0 && tr.lengthSeconds() < 360.0);
    CHECK(track(tr, "Pluck") != nullptr && track(tr, "Lead") != nullptr && track(tr, "Acid") == nullptr);
    commonChecks(tr);

    // Rolling bass: the 2nd-4th 16th of every beat in the drops, never on the kick [E].
    {
        const auto bass = notesOf(track(tr, "Bass"));
        const SongSection* md = section(tr, "Main Drop");
        CHECK(md != nullptr);
        if (md != nullptr)
            for (int b = md->startBar; b < md->startBar + 4; ++b)
                for (int beat = 0; beat < 4; ++beat)
                {
                    const double t = b * 4.0 + beat;
                    CHECK(!hasAt(bass, t));
                    CHECK(hasAt(bass, t + 0.25) && hasAt(bass, t + 0.5) && hasAt(bass, t + 0.75));
                }
        for (auto& [t, n] : bass)
            CHECK(n.pitch >= 31 && n.pitch < 43);
    }

    // The hook: teased on the pluck in Drop 1 (no lead), full lead in the Main Drop.
    {
        const SongTrack* lead = track(tr, "Lead");
        const SongSection* d1 = section(tr, "Drop 1");
        const SongSection* md = section(tr, "Main Drop");
        bool leadInDrop1 = false, leadInMain = false;
        for (auto& c : lead->clips)
        {
            leadInDrop1 |= c.startBar == d1->startBar;
            leadInMain  |= c.startBar == md->startBar;
        }
        CHECK(!leadInDrop1);
        CHECK(leadInMain);
    }

    // Deterministic, and seeds differ.
    {
        const Song again = generateSong(p);
        CHECK(writeMidiFile(again) == writeMidiFile(tr));
        SongParams q = p;
        q.seed = 22;
        CHECK(writeMidiFile(generateSong(q)) != writeMidiFile(tr));
    }

    // ---------------- Neo-rave ----------------
    SongParams r = p;
    r.style = SongStyle::NeoRave;
    const Song nr = generateSong(r);
    CHECK(nr.bpm == 147.0);
    CHECK(nr.totalBars == 160);
    CHECK(track(nr, "Acid") != nullptr && track(nr, "Rave Stab") != nullptr && track(nr, "Pluck") == nullptr);
    commonChecks(nr);
    {
        // Offbeat saw bass: on the 8th offbeat, never on the kick [E].
        const auto bass = notesOf(track(nr, "Bass"));
        const SongSection* d = section(nr, "Drop");
        for (int beat = 0; beat < 16; ++beat)
        {
            const double t = d->startBar * 4.0 + beat;
            CHECK(hasAt(bass, t + 0.5));
            CHECK(!hasAt(bass, t));
        }
        // Trance-gate pad: 16th chops in the drop.
        const auto pad = notesOf(track(nr, "Pad"));
        int chops = 0;
        for (auto it = pad.lower_bound(d->startBar * 4.0); it != pad.end() && it->first < d->startBar * 4.0 + 4.0; ++it)
            chops += it->second.lengthBeats <= 0.25;
        CHECK(chops >= 16);
    }

    // Explicit BPM wins over the style default.
    {
        SongParams q = p;
        q.bpm = 138.0;
        CHECK(generateSong(q).bpm == 138.0);
    }

    TEST_SUMMARY_AND_EXIT();
}
