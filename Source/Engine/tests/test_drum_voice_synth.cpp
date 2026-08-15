#include "../DrumVoiceSynth.h"
#include "TestSupport.h"
#include <cmath>
#include <vector>

using namespace Engine;

namespace
{
    std::vector<float> renderN(DrumVoiceState& voice, int n)
    {
        std::vector<float> out((size_t) n, 0.0f);
        renderDrumVoice(voice, out.data(), n);
        return out;
    }

    bool anyNonZero(const std::vector<float>& v)
    {
        for (float f : v)
            if (f != 0.0f)
                return true;
        return false;
    }

    bool allZero(const std::vector<float>& v)
    {
        for (float f : v)
            if (f != 0.0f)
                return false;
        return true;
    }

    bool allFinite(const std::vector<float>& v)
    {
        for (float f : v)
            if (!std::isfinite(f))
                return false;
        return true;
    }

    float peakAbs(const std::vector<float>& v)
    {
        float p = 0.0f;
        for (float f : v)
            p = std::max(p, std::fabs(f));
        return p;
    }

    bool sameArray(const std::vector<float>& a, const std::vector<float>& b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (a[i] != b[i])
                return false;
        return true;
    }

    const DrumRole kAllRoles[6] = { DrumRole::Kick, DrumRole::Clap, DrumRole::HatClosed,
                                     DrumRole::HatOpen, DrumRole::PercA, DrumRole::PercB };
    const double   kSampleRate  = 44100.0;
}

int main()
{
    // =====================================================================
    // GM note -> role mapping
    // =====================================================================
    {
        DrumRole role;
        CHECK(drumRoleForGmNote(36, role) && role == DrumRole::Kick);
        CHECK(drumRoleForGmNote(39, role) && role == DrumRole::Clap);
        CHECK(drumRoleForGmNote(42, role) && role == DrumRole::HatClosed);
        CHECK(drumRoleForGmNote(46, role) && role == DrumRole::HatOpen);
        CHECK(drumRoleForGmNote(37, role) && role == DrumRole::PercA);
        CHECK(drumRoleForGmNote(63, role) && role == DrumRole::PercB);
        CHECK(!drumRoleForGmNote(60, role)); // arbitrary unrelated note
        CHECK(!drumRoleForGmNote(-1, role));
    }

    // =====================================================================
    // Silence when inactive
    // =====================================================================
    {
        for (auto role : kAllRoles)
        {
            DrumVoiceState v; // default-constructed: active = false
            v.role = role;
            auto out = renderN(v, 512);
            CHECK(allZero(out));
            CHECK(!v.active); // rendering an inactive voice doesn't activate it
        }
    }

    // =====================================================================
    // Non-zero output when triggered
    // =====================================================================
    {
        for (auto role : kAllRoles)
        {
            DrumVoiceState v;
            triggerDrumVoice(v, role, 1.0f, kSampleRate);
            CHECK(v.active);
            auto out = renderN(v, 256);
            CHECK(anyNonZero(out));
        }
    }

    // =====================================================================
    // Velocity affects amplitude - roughly proportional (implementation
    // multiplies velocity straight into the final sample, so halving
    // velocity should roughly halve peak level) for every role.
    // =====================================================================
    {
        for (auto role : kAllRoles)
        {
            DrumVoiceState vLow, vHigh;
            triggerDrumVoice(vLow,  role, 0.25f, kSampleRate);
            triggerDrumVoice(vHigh, role, 1.0f,  kSampleRate);

            auto outLow  = renderN(vLow,  512);
            auto outHigh = renderN(vHigh, 512);

            const float peakLow  = peakAbs(outLow);
            const float peakHigh = peakAbs(outHigh);
            CHECK(peakHigh > peakLow);
            // Expect roughly 4x (1.0 / 0.25) - generous tolerance since
            // noise-based roles sample different points in their envelope.
            CHECK(peakHigh > peakLow * 2.5f);
        }

        // velocity = 0 -> silent regardless of role.
        for (auto role : kAllRoles)
        {
            DrumVoiceState v;
            triggerDrumVoice(v, role, 0.0f, kSampleRate);
            auto out = renderN(v, 512);
            CHECK(allZero(out));
        }
    }

    // =====================================================================
    // Kick pitch/envelope behaviour is deterministic
    // =====================================================================
    {
        DrumVoiceState a, b;
        triggerDrumVoice(a, DrumRole::Kick, 0.8f, kSampleRate);
        triggerDrumVoice(b, DrumRole::Kick, 0.8f, kSampleRate);
        auto outA = renderN(a, 4096);
        auto outB = renderN(b, 4096);
        CHECK(sameArray(outA, outB));

        // Envelope actually decays: later window's peak is clearly lower
        // than the first window's peak (pitched/decaying sine, not a flat
        // tone).
        DrumVoiceState c;
        triggerDrumVoice(c, DrumRole::Kick, 1.0f, kSampleRate);
        std::vector<float> full((size_t) (0.34 * kSampleRate), 0.0f);
        renderDrumVoice(c, full.data(), (int) full.size());

        std::vector<float> early(full.begin(), full.begin() + 200);
        std::vector<float> late(full.end() - 200, full.end());
        CHECK(peakAbs(early) > peakAbs(late) * 2.0f);
    }

    // =====================================================================
    // Voices eventually decay to silence
    // =====================================================================
    {
        for (auto role : kAllRoles)
        {
            DrumVoiceState v;
            triggerDrumVoice(v, role, 1.0f, kSampleRate);

            // Render well past every role's fixed duration (longest is
            // Kick at 0.35s) in one big block.
            auto out = renderN(v, (int) (1.0 * kSampleRate));
            CHECK(!v.active);

            // The tail must be exact silence, not just quiet.
            std::vector<float> tail(out.end() - 1000, out.end());
            CHECK(allZero(tail));

            // Re-rendering an already-finished voice stays silent and
            // doesn't reactivate it.
            auto after = renderN(v, 256);
            CHECK(allZero(after));
            CHECK(!v.active);
        }
    }

    // =====================================================================
    // Same parameters -> identical output, for every role
    // =====================================================================
    {
        for (auto role : kAllRoles)
        {
            DrumVoiceState a, b;
            triggerDrumVoice(a, role, 0.6f, kSampleRate);
            triggerDrumVoice(b, role, 0.6f, kSampleRate);
            CHECK(sameArray(renderN(a, 2048), renderN(b, 2048)));
        }

        // Different velocity -> output actually differs.
        DrumVoiceState d1, d2;
        triggerDrumVoice(d1, DrumRole::Clap, 0.3f, kSampleRate);
        triggerDrumVoice(d2, DrumRole::Clap, 0.9f, kSampleRate);
        CHECK(!sameArray(renderN(d1, 512), renderN(d2, 512)));
    }

    // =====================================================================
    // No NaN/Inf samples, across a full render at max velocity
    // =====================================================================
    {
        for (auto role : kAllRoles)
        {
            DrumVoiceState v;
            triggerDrumVoice(v, role, 1.0f, kSampleRate);
            auto out = renderN(v, (int) (0.5 * kSampleRate));
            CHECK(allFinite(out));
        }
    }

    TEST_SUMMARY_AND_EXIT();
}
