// generate_track - command-line front end for Engine::generateSong.
//
// Writes a complete Melodic Techno arrangement as an Ableton Live Set
// (.als) plus a Standard MIDI File (.mid) fallback.
//
//   Tools/build_generate_track.sh
//   ./build/generate_track --key Am --bpm 124 --seed 7 --out ~/Music/AbletonCopilot
//
// Options:
//   --seed N          reproducible variation (default: random)
//   --key K           Am, F#m, Ebm, C, Dm-dorian ... (default Am)
//   --bpm B           60-200 (default 124)
//   --progression I   0..3, see Engine::progressionCatalogue() (default: from seed)
//   --bars-per-chord N  harmonic rhythm (default 8)
//   --out DIR         output directory (default .)
//   --no-als / --no-mid
//   --wav             also render an audio preview (<title>.wav, synthesised sketch sounds)
#include "../Source/Engine/AlsWriter.h"
#include "../Source/Engine/MidiFileWriter.h"
#include "../Source/Engine/SongGenerator.h"
#include "../Source/Engine/SongRenderer.h"
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
    bool parseKey(std::string k, int& root, Engine::ScaleType& scale)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        if (k.empty())
            return false;
        std::string lower;
        for (char c : k)
            lower += (char) std::tolower((unsigned char) c);

        scale = Engine::ScaleType::Ionian;
        std::string note = k.substr(0, 1);
        note[0] = (char) std::toupper((unsigned char) note[0]);
        size_t i = 1;
        int accidental = 0;
        if (i < k.size() && (k[i] == '#' || k[i] == 'b'))
        {
            accidental = k[i] == '#' ? 1 : -1;
            ++i;
        }
        const std::string rest = lower.substr(i);
        if (rest == "m" || rest == "min" || rest == "minor" || rest == "-minor")
            scale = Engine::ScaleType::Aeolian;
        else if (rest == "m-dorian" || rest == "dorian" || rest == "-dorian")
            scale = Engine::ScaleType::Dorian;
        else if (!(rest.empty() || rest == "maj" || rest == "major"))
            return false;

        for (int n = 0; n < 12; ++n)
            if (note == names[n])
            {
                root = ((n + accidental) % 12 + 12) % 12;
                return true;
            }
        return false;
    }

    std::string safeFileName(const std::string& s)
    {
        std::string out;
        for (char c : s)
            out += (std::isalnum((unsigned char) c) || c == ' ' || c == '-' || c == '#') ? c : '_';
        return out;
    }

    int usage()
    {
        std::fprintf(stderr, "usage: generate_track [--seed N] [--key Am] [--bpm 124] [--progression 0-3] "
                             "[--bars-per-chord 8] [--out DIR] [--no-als] [--no-mid] [--wav]\n");
        return 2;
    }
}

int main(int argc, char** argv)
{
    Engine::SongParams params;
    params.seed = (uint32_t) std::chrono::system_clock::now().time_since_epoch().count() % 10000u;
    std::string outDir = ".";
    bool writeAls = true, writeMid = true, writeWav = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
        const char* v = nullptr;
        if (a == "--seed" && (v = next()))                 params.seed = (uint32_t) std::strtoul(v, nullptr, 10);
        else if (a == "--bpm" && (v = next()))             params.bpm = std::atof(v);
        else if (a == "--progression" && (v = next()))     params.progressionIndex = std::atoi(v);
        else if (a == "--bars-per-chord" && (v = next()))  params.barsPerChord = std::atoi(v);
        else if (a == "--out" && (v = next()))             outDir = v;
        else if (a == "--key" && (v = next()))
        {
            if (!parseKey(v, params.rootNote, params.scale))
            {
                std::fprintf(stderr, "unrecognised key '%s' (try Am, F#m, C, Dm-dorian)\n", v);
                return 2;
            }
        }
        else if (a == "--no-als") writeAls = false;
        else if (a == "--no-mid") writeMid = false;
        else if (a == "--wav")    writeWav = true;
        else return usage();
    }
    if (params.bpm < 60.0 || params.bpm > 200.0)
    {
        std::fprintf(stderr, "--bpm must be between 60 and 200\n");
        return 2;
    }

    const Engine::Song song = Engine::generateSong(params);
    const std::string base = outDir + "/" + safeFileName(song.title);

    std::printf("%s\n", song.title.c_str());
    std::printf("  progression %s (%s), %d bars, %d:%02d\n", song.progression.name, song.progression.character,
                song.totalBars, (int) song.lengthSeconds() / 60, (int) song.lengthSeconds() % 60);
    for (const auto& s : song.sections)
        std::printf("  bar %3d  %-14s %2d bars\n", s.startBar + 1, s.name.c_str(), s.bars);
    for (const auto& t : song.tracks)
        std::printf("  track %-5s %5d notes in %zu clips\n", t.name.c_str(), t.noteCount(), t.clips.size());

    int rc = 0;
    if (writeAls)
    {
        const std::string path = base + ".als";
        if (Engine::writeAlsFileToPath(song, path))
            std::printf("wrote %s\n", path.c_str());
        else
        {
            std::fprintf(stderr, "failed to write %s\n", path.c_str());
            rc = 1;
        }
    }
    if (writeMid)
    {
        const std::string path = base + ".mid";
        if (Engine::writeMidiFileToPath(song, path))
            std::printf("wrote %s\n", path.c_str());
        else
        {
            std::fprintf(stderr, "failed to write %s\n", path.c_str());
            rc = 1;
        }
    }
    if (writeWav)
    {
        const std::string path = base + ".wav";
        if (Engine::writeWav16(Engine::renderSongPreview(song), path))
            std::printf("wrote %s\n", path.c_str());
        else
        {
            std::fprintf(stderr, "failed to write %s\n", path.c_str());
            rc = 1;
        }
    }
    return rc;
}
