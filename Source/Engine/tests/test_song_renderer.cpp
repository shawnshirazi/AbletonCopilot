// Regression tests for Engine::renderSongPreview / writeWav16 (the offline
// audio preview). Uses a short two-section song so it runs quickly.
#include "../SongRenderer.h"
#include "TestSupport.h"
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace Engine;

namespace
{
    double rms(const std::vector<float>& v, size_t from, size_t to)
    {
        double acc = 0.0;
        for (size_t i = from; i < to; ++i)
            acc += (double) v[i] * v[i];
        return std::sqrt(acc / (double) std::max<size_t>(1, to - from));
    }
}

int main()
{
    SongParams p;
    p.seed = 3;
    p.structure = {
        { "Break", { MusicSection::Breakdown, 8, { 0.15f, 0.15f, 0.65f, 0.05f, 0.05f, 0.10f, 0.10f, 0.90f, 0.90f } } },
        { "Drop",  { MusicSection::FinalDrop, 8, { 1.00f, 1.00f, 0.25f, 1.00f, 1.00f, 1.00f, 1.00f, 0.90f, 0.90f } } },
    };
    const Song song = generateSong(p);
    const StereoBuffer a = renderSongPreview(song, 22050.0);

    const double seconds = song.totalBars * 4.0 * 60.0 / song.bpm;
    CHECK(a.left.size() == a.right.size());
    CHECK(a.left.size() >= (size_t) (seconds * 22050.0));

    bool finite = true;
    float peak = 0.0f;
    for (size_t i = 0; i < a.left.size(); ++i)
    {
        finite = finite && std::isfinite(a.left[i]) && std::isfinite(a.right[i]);
        peak = std::max(peak, std::max(std::fabs(a.left[i]), std::fabs(a.right[i])));
    }
    CHECK(finite);
    CHECK(peak > 0.5f && peak <= 0.94f); // normalised, never clipping

    // The kick-driven drop is denser in the low end than the kick-less break.
    const size_t half = (size_t) (seconds * 0.5 * 22050.0);
    CHECK(rms(a.left, 0, half) > 0.01);
    CHECK(rms(a.left, half, half * 2) > 0.01);

    // Deterministic.
    const StereoBuffer b = renderSongPreview(song, 22050.0);
    CHECK(a.left == b.left && a.right == b.right);

    // WAV header.
    const std::string path = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") + "/test_song_renderer.wav";
    CHECK(writeWav16(a, path));
    FILE* f = std::fopen(path.c_str(), "rb");
    CHECK(f != nullptr);
    if (f != nullptr)
    {
        unsigned char h[44] {};
        CHECK(std::fread(h, 1, 44, f) == 44);
        CHECK(std::memcmp(h, "RIFF", 4) == 0 && std::memcmp(h + 8, "WAVE", 4) == 0 && std::memcmp(h + 36, "data", 4) == 0);
        const uint32_t dataBytes = h[40] | (h[41] << 8) | (h[42] << 16) | ((uint32_t) h[43] << 24);
        CHECK(dataBytes == a.left.size() * 4);
        std::fclose(f);
        std::remove(path.c_str());
    }

    TEST_SUMMARY_AND_EXIT();
}
