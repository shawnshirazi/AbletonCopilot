#pragma once

#include "SongGenerator.h"
#include <string>
#include <vector>

// Offline audio preview of an Engine::Song - so a generated track can be
// heard before it is opened in Ableton. Zero JUCE dependency, pure DSP:
//
//   drums  synthesised kick (pitch-swept sine + click), layered clap, noise
//          hats, pitched percussion
//   bass   saw + sub with an envelope-driven low-pass
//   pad    detuned supersaw chords, slow attack/release
//   arp    filtered pluck through a dotted-8th ping-pong delay
//   lead   detuned saw lead with vibrato
//   fx     noise riser / downlifter, sub impact
//
// Every melodic part is sidechain-ducked by the kick, pad/lead/arp/clap feed
// a shared reverb, and a per-bar "brightness" curve derived from the song's
// sections opens the filters through builds and drops and closes them in
// the breaks - roughly what a producer would automate. It is a sketch-quality
// preview of the arrangement and note data, not a mix: the real sounds come
// from whatever instruments are loaded on the tracks in Live.
namespace Engine
{
    struct StereoBuffer
    {
        double             sampleRate = 44100.0;
        std::vector<float> left, right;
    };

    StereoBuffer renderSongPreview(const Song& song, double sampleRate = 44100.0);

    // 16-bit PCM WAV. False on I/O failure.
    bool writeWav16(const StereoBuffer& audio, const std::string& path);
}
