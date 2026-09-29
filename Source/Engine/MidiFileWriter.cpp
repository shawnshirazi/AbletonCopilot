#include "MidiFileWriter.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Engine
{
    namespace
    {
        struct Event
        {
            uint32_t             tick;
            int                  order; // note-offs before note-ons at the same tick
            std::vector<uint8_t> bytes;
        };

        void putVarLen(std::vector<uint8_t>& out, uint32_t v)
        {
            uint8_t buf[5];
            int n = 0;
            buf[n++] = (uint8_t) (v & 0x7f);
            while ((v >>= 7) != 0)
                buf[n++] = (uint8_t) ((v & 0x7f) | 0x80);
            while (n > 0)
                out.push_back(buf[--n]);
        }

        void put32(std::vector<uint8_t>& out, uint32_t v)
        {
            for (int s = 24; s >= 0; s -= 8)
                out.push_back((uint8_t) (v >> s));
        }

        std::vector<uint8_t> metaText(uint8_t type, const std::string& text)
        {
            std::vector<uint8_t> b { 0xff, type };
            putVarLen(b, (uint32_t) text.size());
            b.insert(b.end(), text.begin(), text.end());
            return b;
        }

        uint32_t toTicks(double beats) { return (uint32_t) std::llround(std::max(0.0, beats) * kMidiTicksPerBeat); }

        void appendTrackChunk(std::vector<uint8_t>& file, std::vector<Event> events, uint32_t endTick)
        {
            std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b)
                             { return a.tick < b.tick || (a.tick == b.tick && a.order < b.order); });
            std::vector<uint8_t> data;
            uint32_t last = 0;
            for (const Event& e : events)
            {
                putVarLen(data, e.tick - last);
                last = e.tick;
                data.insert(data.end(), e.bytes.begin(), e.bytes.end());
            }
            putVarLen(data, endTick > last ? endTick - last : 0);
            data.insert(data.end(), { 0xff, 0x2f, 0x00 }); // end of track

            file.insert(file.end(), { 'M', 'T', 'r', 'k' });
            put32(file, (uint32_t) data.size());
            file.insert(file.end(), data.begin(), data.end());
        }
    }

    std::vector<uint8_t> writeMidiFile(const Song& song)
    {
        std::vector<uint8_t> file { 'M', 'T', 'h', 'd' };
        put32(file, 6);
        const uint16_t numTracks = (uint16_t) (song.tracks.size() + 1);
        file.insert(file.end(), { 0x00, 0x01, (uint8_t) (numTracks >> 8), (uint8_t) numTracks,
                                  (uint8_t) (kMidiTicksPerBeat >> 8), (uint8_t) kMidiTicksPerBeat });

        const uint32_t endTick = toTicks(song.totalBars * 4.0);

        // Conductor track.
        {
            std::vector<Event> ev;
            ev.push_back({ 0, 0, metaText(0x03, song.title) });
            const uint32_t usPerBeat = (uint32_t) std::llround(60000000.0 / std::max(1.0, song.bpm));
            ev.push_back({ 0, 1, { 0xff, 0x51, 0x03, (uint8_t) (usPerBeat >> 16), (uint8_t) (usPerBeat >> 8), (uint8_t) usPerBeat } });
            ev.push_back({ 0, 2, { 0xff, 0x58, 0x04, 4, 2, 24, 8 } }); // 4/4
            for (const SongSection& s : song.sections)
                ev.push_back({ toTicks(s.startBar * 4.0), 3, metaText(0x06, s.name) });
            appendTrackChunk(file, std::move(ev), endTick);
        }

        for (const SongTrack& t : song.tracks)
        {
            std::vector<Event> ev;
            ev.push_back({ 0, 0, metaText(0x03, t.name) });
            for (const SongClip& c : t.clips)
                for (const SongNote& n : c.notes)
                {
                    const double start = c.startBar * 4.0 + n.startBeat;
                    const uint32_t on  = toTicks(start);
                    const uint32_t off = std::max(on + 1, toTicks(start + n.lengthBeats));
                    const uint8_t pitch = (uint8_t) std::max(0, std::min(127, n.pitch));
                    const uint8_t vel   = (uint8_t) std::max(1, std::min(127, n.velocity));
                    ev.push_back({ off, 1, { 0x80, pitch, 0 } });
                    ev.push_back({ on, 2, { 0x90, pitch, vel } });
                }
            appendTrackChunk(file, std::move(ev), endTick);
        }
        return file;
    }

    bool writeMidiFileToPath(const Song& song, const std::string& path)
    {
        const std::vector<uint8_t> bytes = writeMidiFile(song);
        FILE* f = std::fopen(path.c_str(), "wb");
        if (f == nullptr)
            return false;
        const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
        return std::fclose(f) == 0 && ok;
    }
}
