// Regression tests for Engine::generateSong (the full-track generator) and
// Engine::writeMidiFile. Build/run: Source/Engine/tests/run_all.sh
#include "../SongGenerator.h"
#include "../MidiFileWriter.h"
#include "TestSupport.h"
#include <cmath>
#include <set>
#include <string>

using namespace Engine;

namespace
{
    const SongTrack* findTrack(const Song& s, const std::string& name)
    {
        for (auto& t : s.tracks)
            if (t.name == name)
                return &t;
        return nullptr;
    }

    const SongSection* findSection(const Song& s, const std::string& name)
    {
        for (auto& sec : s.sections)
            if (sec.name == name)
                return &sec;
        return nullptr;
    }

    const SongClip* clipAt(const SongTrack& t, int startBar)
    {
        for (auto& c : t.clips)
            if (c.startBar == startBar)
                return &c;
        return nullptr;
    }

    bool sameSong(const Song& a, const Song& b)
    {
        if (a.tracks.size() != b.tracks.size() || a.totalBars != b.totalBars)
            return false;
        for (size_t t = 0; t < a.tracks.size(); ++t)
        {
            const auto& ca = a.tracks[t].clips;
            const auto& cb = b.tracks[t].clips;
            if (ca.size() != cb.size())
                return false;
            for (size_t c = 0; c < ca.size(); ++c)
            {
                if (ca[c].notes.size() != cb[c].notes.size() || ca[c].startBar != cb[c].startBar)
                    return false;
                for (size_t n = 0; n < ca[c].notes.size(); ++n)
                {
                    const SongNote& x = ca[c].notes[n];
                    const SongNote& y = cb[c].notes[n];
                    if (x.pitch != y.pitch || x.velocity != y.velocity || x.startBeat != y.startBeat
                        || x.lengthBeats != y.lengthBeats)
                        return false;
                }
            }
        }
        return true;
    }

    int kicksInBar(const Song& s, int bar)
    {
        int n = 0;
        for (auto& c : findTrack(s, "Kick")->clips)
            for (auto& note : c.notes)
                if ((int) std::floor((c.startBar * 4.0 + note.startBeat) / 4.0) == bar)
                    ++n;
        return n;
    }
}

int main()
{
    SongParams p;
    p.seed = 42;
    const Song song = generateSong(p);

    // --- structure ---
    CHECK(song.totalBars == 200);
    CHECK(song.sections.size() == 12);
    CHECK(song.lengthSeconds() > 360.0 && song.lengthSeconds() < 420.0); // ~6.5 min at 124 BPM
    {
        int expected = 0;
        for (auto& s : song.sections)
        {
            CHECK(s.startBar == expected);
            CHECK(s.bars % 8 == 0); // research 3.3 #1
            expected += s.bars;
        }
        CHECK(expected == song.totalBars);
    }
    CHECK(song.keyName() == "A minor");
    CHECK((int) song.chordDegreeByBar.size() == song.totalBars);

    // --- determinism ---
    CHECK(sameSong(song, generateSong(p)));
    {
        SongParams q = p;
        q.seed = 43;
        CHECK(!sameSong(song, generateSong(q)));
    }

    // --- every part exists and has material ---
    const char* names[] = { "Kick", "Clap", "Hats", "Perc", "Bass", "Pad", "Arp", "Lead", "FX" };
    for (const char* n : names)
    {
        const SongTrack* t = findTrack(song, n);
        CHECK(t != nullptr);
        if (t != nullptr)
            CHECK(t->noteCount() > 0);
    }

    // --- every note sits inside its clip, valid MIDI ranges ---
    for (auto& t : song.tracks)
    {
        int prevEnd = -1;
        for (auto& c : t.clips)
        {
            CHECK(c.startBar >= prevEnd); // clips never overlap
            prevEnd = c.startBar + c.bars;
            for (auto& n : c.notes)
            {
                CHECK(n.startBeat >= 0.0);
                CHECK(n.lengthBeats > 0.0);
                CHECK(n.startBeat + n.lengthBeats <= c.bars * 4.0 + 1e-9);
                CHECK(n.pitch >= 0 && n.pitch <= 127);
                CHECK(n.velocity >= 1 && n.velocity <= 127);
            }
        }
    }

    // --- pitched parts stay diatonic and in register ---
    for (const char* n : { "Bass", "Pad", "Arp", "Lead" })
        for (auto& c : findTrack(song, n)->clips)
            for (auto& note : c.notes)
                CHECK(isInScale(note.pitch - song.rootNote, song.scale));
    for (auto& c : findTrack(song, "Bass")->clips)
        for (auto& note : c.notes)
            CHECK(note.pitch >= 29 && note.pitch <= 48); // F1-C3, research 11.5
    for (auto& c : findTrack(song, "Pad")->clips)
        for (auto& note : c.notes)
            CHECK(note.pitch >= 48 && note.pitch < 72);

    // --- harmony: 3-4 chord changes per 32 bars (research section 6) ---
    for (int start = 0; start + 32 <= song.totalBars; start += 32)
    {
        int changes = 0;
        for (int b = start + 1; b < start + 32; ++b)
            changes += song.chordDegreeByBar[(size_t) b] != song.chordDegreeByBar[(size_t) (b - 1)];
        CHECK(changes <= 4);
    }

    // --- arrangement behaviour ---
    const SongSection* brk   = findSection(song, "Break");
    const SongSection* drop1 = findSection(song, "Drop 1");
    const SongSection* drop2 = findSection(song, "Drop 2");
    const SongSection* pre   = findSection(song, "Pre-Break");
    CHECK(brk && drop1 && drop2 && pre);
    if (brk && drop1 && drop2 && pre)
    {
        for (int b = brk->startBar; b < brk->startBar + brk->bars; ++b)
            CHECK(kicksInBar(song, b) == 0);                 // kick out in the break
        for (int b = drop2->startBar; b < drop2->startBar + drop2->bars; ++b)
            CHECK(kicksInBar(song, b) >= 4);                 // four-on-the-floor in the drop
        CHECK(kicksInBar(song, pre->startBar + pre->bars - 1) == 0); // space before the break

        const SongTrack* lead = findTrack(song, "Lead");
        CHECK(clipAt(*lead, drop1->startBar) == nullptr);    // Drop 1 = subtle, no hook yet
        CHECK(clipAt(*lead, brk->startBar) != nullptr);      // hook revealed in the break
        CHECK(clipAt(*lead, drop2->startBar) != nullptr);    // ...and carried by Drop 2

        const SongTrack* bass = findTrack(song, "Bass");
        const SongClip* d1 = clipAt(*bass, drop1->startBar);
        const SongClip* d2 = clipAt(*bass, drop2->startBar);
        CHECK(d1 != nullptr && d2 != nullptr);
        if (d1 != nullptr && d2 != nullptr)
        {
            // Shared bass theme: Drop 1 and Drop 2 play the same rhythm.
            std::set<double> r1, r2;
            for (auto& n : d1->notes) if (n.startBeat < 16.0) r1.insert(n.startBeat);
            for (auto& n : d2->notes) if (n.startBeat < 16.0) r2.insert(n.startBeat);
            CHECK(!r1.empty() && r1 == r2);
        }

        // FX: impact on every drop downbeat, riser ending exactly at it.
        const SongTrack* fx = findTrack(song, "FX");
        for (auto& s : song.sections)
        {
            if (s.kind != MusicSection::Drop && s.kind != MusicSection::FinalDrop)
                continue;
            bool impact = false, riserEnds = false;
            for (auto& c : fx->clips)
                for (auto& n : c.notes)
                {
                    const double abs = c.startBar * 4.0 + n.startBeat;
                    if (n.pitch == SongFxNotes::kImpact && std::fabs(abs - s.startBar * 4.0) < 1e-9)
                        impact = true;
                    if (n.pitch == SongFxNotes::kRiser && std::fabs(abs + n.lengthBeats - s.startBar * 4.0) < 1e-9)
                        riserEnds = true;
                }
            CHECK(impact);
            CHECK(riserEnds);
        }
    }

    // --- key parameter ---
    {
        SongParams q = p;
        q.rootNote = 4; // E minor
        const Song e = generateSong(q);
        CHECK(e.keyName() == "E minor");
        for (const char* n : { "Bass", "Pad", "Arp", "Lead" })
            for (auto& c : findTrack(e, n)->clips)
                for (auto& note : c.notes)
                    CHECK(isInScale(note.pitch - 4, e.scale));
    }

    // --- MIDI file: header + note-on count round trip ---
    {
        const std::vector<uint8_t> mid = writeMidiFile(song);
        CHECK(mid.size() > 14);
        CHECK(mid[0] == 'M' && mid[1] == 'T' && mid[2] == 'h' && mid[3] == 'd');
        CHECK(mid[9] == 1);                                        // format 1
        CHECK(((mid[10] << 8) | mid[11]) == (int) song.tracks.size() + 1);
        CHECK(((mid[12] << 8) | mid[13]) == kMidiTicksPerBeat);

        // Walk every chunk and count note-ons.
        int noteOns = 0, chunks = 0;
        size_t pos = 14;
        while (pos + 8 <= mid.size())
        {
            const size_t len = ((size_t) mid[pos + 4] << 24) | ((size_t) mid[pos + 5] << 16)
                               | ((size_t) mid[pos + 6] << 8) | mid[pos + 7];
            size_t i = pos + 8;
            const size_t end = i + len;
            ++chunks;
            while (i < end)
            {
                while (mid[i] & 0x80) ++i; // delta
                ++i;
                const uint8_t status = mid[i];
                if (status == 0xff)
                {
                    size_t l = 0;
                    i += 2;
                    while (mid[i] & 0x80) l = (l << 7) | (mid[i++] & 0x7f);
                    l = (l << 7) | mid[i++];
                    i += l;
                }
                else
                {
                    if ((status & 0xf0) == 0x90) ++noteOns;
                    i += 3;
                }
            }
            CHECK(i == end);
            pos = end;
        }
        CHECK(pos == mid.size());
        CHECK(chunks == (int) song.tracks.size() + 1);
        int total = 0;
        for (auto& t : song.tracks)
            total += t.noteCount();
        CHECK(noteOns == total);
    }

    TEST_SUMMARY_AND_EXIT();
}
