#pragma once

#include "SongGenerator.h"
#include <cstdint>
#include <string>
#include <vector>

// Standard MIDI File (type 1) export of an Engine::Song - the portable
// fallback next to the Ableton Live Set (AlsWriter.h). Dragging the .mid
// into Live's Arrangement view creates one MIDI track per song track.
// Track 0 carries tempo, 4/4 time signature and one marker per section.
// Zero JUCE dependency.
namespace Engine
{
    constexpr int kMidiTicksPerBeat = 480;

    std::vector<uint8_t> writeMidiFile(const Song& song);

    // Writes the bytes to `path`; false on any I/O failure.
    bool writeMidiFileToPath(const Song& song, const std::string& path);
}
