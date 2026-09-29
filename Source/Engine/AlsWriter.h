#pragma once

#include "SongGenerator.h"
#include <cstdint>
#include <string>
#include <vector>

// Ableton Live Set (.als) export of an Engine::Song: one MIDI track per song
// track (named + coloured, no devices - load your own instruments), every
// clip placed in the Arrangement view, the song tempo, and one Arrangement
// locator per section. Zero JUCE dependency.
//
// Written in the Live 11.3 schema, which both Live 11 and Live 12 open (Live
// refuses sets saved by a NEWER version, so a Live 12 schema would lock out
// Live 11 users). Built the way every working open-source .als generator
// does it: from a real, Live-saved set (AlsTemplate.h, generated from
// Ableton's own MIT-licensed test set) with only known fields edited:
//
//   - The template's single MIDI track is cloned per song track. Every
//     element in Live's global "pointee" pool (*Target / Pointee /
//     ControllerTargets.N ids) gets a fresh, globally unique id, and
//     NextPointeeId is set to max + 1 - duplicate or too-low ids are the #1
//     cause of "corrupt set" errors.
//   - Arrangement clips go in MainSequencer/ClipTimeable/ArrangerAutomation/
//     Events, in the exact Live 11 MidiClip layout (Time == CurrentStart,
//     absolute beats; LoopOn=false with LoopEnd == clip length; one
//     KeyTrack per pitch sorted by MidiKey; 8-attribute Live 11
//     MidiNoteEvents with unique NoteIds; NoteIdGenerator = max + 1).
//   - Tempo is written to BOTH Tempo/Manual and the master's -63072000
//     default automation event (the envelope wins over Manual otherwise).
namespace Engine
{
    // The uncompressed Live Set XML.
    std::string buildAlsXml(const Song& song);

    // gzip(buildAlsXml(song)) - the bytes of a .als file.
    std::vector<uint8_t> writeAlsFile(const Song& song);

    // Writes the .als to `path`; false on any I/O failure.
    bool writeAlsFileToPath(const Song& song, const std::string& path);
}
