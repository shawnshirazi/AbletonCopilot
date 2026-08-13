#include "DrumVoiceSynth.h"
#include <algorithm>
#include <cmath>

namespace Engine
{
    namespace
    {
        constexpr double kPi = 3.14159265358979323846;

        // xorshift32 - fast, deterministic, no external dependency.
        uint32_t nextNoise(uint32_t& state)
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return state;
        }

        // -1..1 uniform white noise sample.
        float whiteNoise(uint32_t& state)
        {
            const uint32_t r = nextNoise(state) & 0x00FFFFFFu;
            return (float) r / (float) 0x00FFFFFFu * 2.0f - 1.0f;
        }

        // One-pole highpass: RC = 1/(2*pi*fc), alpha = RC/(RC+dt).
        float highpass(float x, float& prevX, float& prevY, double fc, double sampleRate)
        {
            const double rc    = 1.0 / (2.0 * kPi * fc);
            const double dt    = 1.0 / sampleRate;
            const double alpha = rc / (rc + dt);
            const float  y     = (float) alpha * (prevY + x - prevX);
            prevX = x;
            prevY = y;
            return y;
        }

        // One-pole lowpass: alpha = dt/(RC+dt).
        float lowpass(float x, float& prevY, double fc, double sampleRate)
        {
            const double rc    = 1.0 / (2.0 * kPi * fc);
            const double dt    = 1.0 / sampleRate;
            const double alpha = dt / (rc + dt);
            const float  y     = prevY + (float) alpha * (x - prevY);
            prevY = y;
            return y;
        }

        // --- Per-role fixed timbre constants (Phase 1: simple, parametric,
        // not sample-based - see DrumVoiceSynth.h). ---
        namespace Kick
        {
            constexpr double freqStartHz = 150.0;
            constexpr double freqEndHz   = 50.0;
            constexpr double pitchTauSec = 0.025;
            constexpr double ampTauSec   = 0.15;
            constexpr double durationSec = 0.35;
        }
        namespace Clap
        {
            constexpr double burstOffsets[3] = { 0.0, 0.012, 0.024 };
            constexpr double burstTauSec     = 0.02;
            constexpr double durationSec     = 0.15;
            constexpr double hpHz            = 1000.0;
            constexpr double lpHz            = 6000.0;
        }
        namespace Hat
        {
            constexpr double ampTauSec   = 0.012;
            constexpr double durationSec = 0.06;
            constexpr double hpHz        = 6000.0;
        }
        namespace Perc
        {
            constexpr double ampTauSec   = 0.02;
            constexpr double durationSec = 0.08;
            constexpr double hpHz        = 1500.0;
            constexpr double lpHz        = 3500.0;
        }

        // Fixed per-role noise seeds - deterministic (never time-based), so
        // identical (role, velocity) triggers always render identical audio.
        uint32_t noiseSeedFor(DrumRole role)
        {
            switch (role)
            {
                case DrumRole::Clap:  return 0xC1A9F00Du;
                case DrumRole::Hat:   return 0x4A570001u;
                case DrumRole::Perc:  return 0xBE9CAEE0u;
                case DrumRole::Kick:
                case DrumRole::Count: return 1u;
            }
            return 1u;
        }

        double durationFor(DrumRole role)
        {
            switch (role)
            {
                case DrumRole::Kick:  return Kick::durationSec;
                case DrumRole::Clap:  return Clap::durationSec;
                case DrumRole::Hat:   return Hat::durationSec;
                case DrumRole::Perc:  return Perc::durationSec;
                case DrumRole::Count: return 0.0;
            }
            return 0.0;
        }

        float renderKickSample(DrumVoiceState& v, double t)
        {
            const double freq = Kick::freqEndHz + (Kick::freqStartHz - Kick::freqEndHz)
                                                       * std::exp(-t / Kick::pitchTauSec);
            v.phase += 2.0 * kPi * freq / v.sampleRate;
            if (v.phase > 2.0 * kPi)
                v.phase -= 2.0 * kPi;

            const double amp = std::exp(-t / Kick::ampTauSec);
            return (float) (v.velocity * amp * std::sin(v.phase));
        }

        float renderClapSample(DrumVoiceState& v, double t)
        {
            double amp = 0.0;
            for (double offset : Clap::burstOffsets)
                if (t >= offset)
                    amp = std::max(amp, std::exp(-(t - offset) / Clap::burstTauSec));

            const float noise = whiteNoise(v.noiseState);
            const float hp    = highpass(noise, v.hpPrevX, v.hpPrevY, Clap::hpHz, v.sampleRate);
            const float bp    = lowpass(hp, v.lpPrevY, Clap::lpHz, v.sampleRate);
            return (float) (v.velocity * amp) * bp;
        }

        float renderHatSample(DrumVoiceState& v, double t)
        {
            const double amp   = std::exp(-t / Hat::ampTauSec);
            const float  noise = whiteNoise(v.noiseState);
            const float  hp    = highpass(noise, v.hpPrevX, v.hpPrevY, Hat::hpHz, v.sampleRate);
            return (float) (v.velocity * amp) * hp;
        }

        float renderPercSample(DrumVoiceState& v, double t)
        {
            const double amp   = std::exp(-t / Perc::ampTauSec);
            const float  noise = whiteNoise(v.noiseState);
            const float  hp    = highpass(noise, v.hpPrevX, v.hpPrevY, Perc::hpHz, v.sampleRate);
            const float  bp    = lowpass(hp, v.lpPrevY, Perc::lpHz, v.sampleRate);
            return (float) (v.velocity * amp) * bp;
        }
    }

    bool drumRoleForGmNote(int midiNote, DrumRole& outRole)
    {
        switch (midiNote)
        {
            case 36: outRole = DrumRole::Kick; return true;
            case 39: outRole = DrumRole::Clap; return true;
            case 42: outRole = DrumRole::Hat;  return true;
            case 37: outRole = DrumRole::Perc; return true;
            default: return false;
        }
    }

    void triggerDrumVoice(DrumVoiceState& voice, DrumRole role, float velocity, double sampleRate)
    {
        voice.active         = true;
        voice.role            = role;
        voice.velocity         = velocity < 0.0f ? 0.0f : (velocity > 1.0f ? 1.0f : velocity);
        voice.sampleRate       = sampleRate > 0.0 ? sampleRate : 44100.0;
        voice.elapsedSamples   = 0;
        voice.phase            = 0.0;
        voice.noiseState       = noiseSeedFor(role);
        voice.hpPrevX = voice.hpPrevY = voice.lpPrevY = 0.0f;
    }

    void renderDrumVoice(DrumVoiceState& voice, float* out, int numSamples)
    {
        if (!voice.active)
        {
            for (int i = 0; i < numSamples; ++i)
                out[i] = 0.0f;
            return;
        }

        const double duration = durationFor(voice.role);

        for (int i = 0; i < numSamples; ++i)
        {
            const double t = (double) voice.elapsedSamples / voice.sampleRate;

            if (t >= duration)
            {
                voice.active = false;
                // Once inactive, the rest of this block (and all future
                // blocks until re-triggered) stays silent - fill the
                // remainder up front instead of branching per sample.
                for (int j = i; j < numSamples; ++j)
                    out[j] = 0.0f;
                return;
            }

            switch (voice.role)
            {
                case DrumRole::Kick:  out[i] = renderKickSample(voice, t); break;
                case DrumRole::Clap:  out[i] = renderClapSample(voice, t); break;
                case DrumRole::Hat:   out[i] = renderHatSample (voice, t); break;
                case DrumRole::Perc:  out[i] = renderPercSample(voice, t); break;
                case DrumRole::Count: out[i] = 0.0f; break;
            }

            ++voice.elapsedSamples;
        }
    }
}
