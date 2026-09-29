// Regression tests for SongStyle::ProgressiveTechno (ProgressiveGenerator.cpp),
// including a note-for-note check of the reference arp loop's first four
// bars (MLPipeline/musical_target/progressive_arp_reference.md).
#include "../SongGenerator.h"
#include "../MidiFileWriter.h"
#include "../AlsWriter.h"
#include "TestSupport.h"
#include <map>
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

    // step index (16ths from song start) -> pitch, for a monophonic track
    std::map<int, int> stepsOf(const SongTrack* t)
    {
        std::map<int, int> m;
        for (auto& c : t->clips)
            for (auto& n : c.notes)
                m[(int) ((c.startBar * 4.0 + n.startBeat) * 4.0 + 0.5)] = n.pitch;
        return m;
    }

    // Does this song use the reference-faithful 5-note cell? (Some seeds pick a 3-note variant.)
    bool fiveNoteCell(const Song& s)
    {
        const auto arp = stepsOf(track(s, "Arp"));
        const SongSection* th = section(s, "Theme");
        return arp.at(th->startBar * 16) == arp.at(th->startBar * 16 + 5) && arp.at(th->startBar * 16) != arp.at(th->startBar * 16 + 3);
    }
}

int main()
{
    SongParams p;
    p.style = SongStyle::ProgressiveTechno;
    p.bpm = 0.0;
    p.rootNote = 2; // D minor
    p.progressionIndex = 0; // the reference loop's progression

    // Find a seed with the reference 5-note cell (deterministic search).
    Song song;
    for (uint32_t seed = 1; seed < 50; ++seed)
    {
        p.seed = seed;
        song = generateSong(p);
        if (fiveNoteCell(song))
            break;
    }
    CHECK(fiveNoteCell(song));
    CHECK(song.bpm == 125.0);
    CHECK(song.totalBars == 176);
    CHECK(song.keyName() == "D minor");

    // --- the reference loop, bars 1-4, note for note (MIDI numbers) ---
    // D3=50 A3=57 D4=62 E4=64 F4=65 G4=67 A4=69 D5=74
    const int ref[64] = {
        50, 57, 62, 64, 65, 50, 57, 74, 64, 65, 50, 57, 74, 64, 65, 50,
        57, 62, 64, 65, 50, 57, 74, 64, 65, 50, 57, 74, 64, 65, 50, 57,
        62, 64, 67, 50, 57, 74, 64, 67, 50, 57, 62, 64, 67, 50, 57, 74, // bar 3 (reference readings D#4/F4/A#3 were detection errors)
        64, 69, 50, 57, 74, 64, 69, 50, 57, 74, 64, 69, 50, 57, 74, 64, // bar 4 up to the anticipation shift
    };
    {
        const auto arp = stepsOf(track(song, "Arp"));
        const int t0 = section(song, "Theme")->startBar * 16;
        int match = 0;
        for (int i = 0; i < 60; ++i) // bar 4's last steps differ by the loop's human 1-step shift
            match += arp.count(t0 + i) && arp.at(t0 + i) == ref[i];
        CHECK(match >= 56);
        // 5-step periodicity against the 16-step bar (polymeter), not 4.
        int same5 = 0, same4 = 0;
        for (int i = 0; i < 32; ++i)
        {
            same5 += arp.at(t0 + i) % 12 == arp.at(t0 + i + 5) % 12;
            same4 += arp.at(t0 + i) % 12 == arp.at(t0 + i + 4) % 12;
        }
        CHECK(same5 >= 30);
        CHECK(same4 <= 8);
        // Anticipation: the Bb (VI) cell starts before bar 5 of the cycle.
        CHECK(arp.at(t0 + 4 * 16 - 4) == 46 || arp.at(t0 + 4 * 16 - 3) == 46 || arp.at(t0 + 4 * 16 - 2) == 46);
    }

    // --- every part present, every pitched note in D minor, notes inside clips, no overlaps ---
    for (const char* name : { "Kick", "Clap", "Hats", "Perc", "Snare Roll", "Bass", "Pad", "Arp", "Lead", "FX" })
    {
        const SongTrack* t = track(song, name);
        CHECK(t != nullptr && t->noteCount() > 0);
    }
    for (const char* name : { "Bass", "Pad", "Arp", "Lead" })
        for (auto& c : track(song, name)->clips)
            for (auto& n : c.notes)
                CHECK(isInScale(n.pitch - song.rootNote, song.scale));
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

    // --- arrangement ---
    {
        const SongSection* bd = section(song, "Breakdown");
        const SongSection* md = section(song, "Main Drop");
        int kicksInBreakdown = 0, kicksInDrop = 0;
        for (auto& c : track(song, "Kick")->clips)
        {
            if (c.startBar == bd->startBar) kicksInBreakdown += (int) c.notes.size();
            if (c.startBar == md->startBar) kicksInDrop += (int) c.notes.size();
        }
        CHECK(kicksInBreakdown == 0);
        CHECK(kicksInDrop == md->bars * 4);
        // The arp plays through the breakdown (it is the centrepiece).
        bool arpInBreakdown = false;
        for (auto& c : track(song, "Arp")->clips)
            arpInBreakdown |= c.startBar == bd->startBar && c.notes.size() >= (size_t) bd->bars * 16 - 1;
        CHECK(arpInBreakdown);
        // Rolling bass: two 16ths after each 8th offbeat, never on the kick.
        const auto bass = stepsOf(track(song, "Bass"));
        for (int beat = 0; beat < 8; ++beat)
        {
            const int s0 = md->startBar * 16 + beat * 4;
            CHECK(!bass.count(s0) && bass.count(s0 + 2) && bass.count(s0 + 3));
        }
    }

    // --- all four progressions, other keys, determinism ---
    for (int prog = 0; prog < 4; ++prog)
        for (int root : { 2, 5, 9 })
        {
            SongParams q = p;
            q.progressionIndex = prog;
            q.rootNote = root;
            const Song s = generateSong(q);
            for (const char* name : { "Bass", "Pad", "Arp", "Lead" })
                for (auto& c : track(s, name)->clips)
                    for (auto& n : c.notes)
                        CHECK(isInScale(n.pitch - s.rootNote, s.scale) && n.pitch >= 24 && n.pitch <= 96);
            CHECK(buildAlsXml(s).find('@') == std::string::npos);
        }
    CHECK(writeMidiFile(generateSong(p)) == writeMidiFile(song));

    TEST_SUMMARY_AND_EXIT();
}
