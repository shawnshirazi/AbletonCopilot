#pragma once

#include "SongGenerator.h"
#include <cstdint>
#include <random>
#include <vector>

// Internal helpers shared by the song generators (SongGenerator.cpp for the
// melodic-techno style, TranceGenerator.cpp for trance / neo-rave). Not part
// of the public Engine API.
namespace Engine
{
    namespace songdetail
    {
        // Absolute-time note (beats from song start), split into per-section clips at the end.
        struct AbsNote
        {
            double start, length;
            int    pitch, velocity;
        };

        int pick(std::mt19937& rng, int n);
        uint32_t mixSeed(uint32_t x);
        bool isGrooveKind(MusicSection k);
        int wrapInto(int pitch, int lo);
        int clampVel(int v);

        // Sorts, removes same-pitch overlaps, and cuts `notes` into one clip
        // per section (sections without notes get no clip).
        std::vector<SongClip> splitIntoClips(const Song& song, std::vector<AbsNote> notes);

        Song generateTranceSong(const SongParams& params);
        Song generateProgressiveSong(const SongParams& params);
        Song generateDrivingTechnoSong(const SongParams& params);
    }
}
