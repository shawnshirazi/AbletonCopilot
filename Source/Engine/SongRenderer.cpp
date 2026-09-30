#include "SongRenderer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Engine
{
    namespace
    {
        constexpr double kPi = 3.14159265358979323846;

        double mtof(double pitch) { return 440.0 * std::pow(2.0, (pitch - 69.0) / 12.0); }

        struct Noise
        {
            uint32_t s = 0x12345678u;
            float next()
            {
                s ^= s << 13;
                s ^= s >> 17;
                s ^= s << 5;
                return (float) ((double) s / 2147483648.0 - 1.0);
            }
        };

        // PolyBLEP band-limited saw, phase in [0, 1).
        float polyBlep(double t, double dt)
        {
            if (t < dt)
            {
                t /= dt;
                return (float) (t + t - t * t - 1.0);
            }
            if (t > 1.0 - dt)
            {
                t = (t - 1.0) / dt;
                return (float) (t * t + t + t + 1.0);
            }
            return 0.0f;
        }

        struct SawOsc
        {
            double phase = 0.0;
            float next(double freq, double sr)
            {
                const double dt = std::min(0.5, freq / sr);
                const float v = (float) (2.0 * phase - 1.0) - polyBlep(phase, dt);
                phase += dt;
                if (phase >= 1.0) phase -= 1.0;
                return v;
            }
            float nextSquare(double freq, double sr)
            {
                const double dt = std::min(0.5, freq / sr);
                double p2 = phase + 0.5;
                if (p2 >= 1.0) p2 -= 1.0;
                float v = (phase < 0.5 ? 1.0f : -1.0f) + polyBlep(phase, dt) - polyBlep(p2, dt);
                phase += dt;
                if (phase >= 1.0) phase -= 1.0;
                return v;
            }
        };

        // Topology-preserving-transform state-variable filter (Simper).
        struct Svf
        {
            float ic1 = 0.0f, ic2 = 0.0f;
            float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.0f;
            float low = 0.0f, band = 0.0f, high = 0.0f;

            void set(double cutoff, double q, double sr)
            {
                cutoff = std::max(20.0, std::min(cutoff, sr * 0.45));
                const double g = std::tan(kPi * cutoff / sr);
                k  = (float) (1.0 / q);
                a1 = (float) (1.0 / (1.0 + g * (g + k)));
                a2 = (float) (g * a1);
                a3 = (float) (g * a2);
            }
            void process(float v0)
            {
                const float v3 = v0 - ic2;
                const float v1 = a1 * ic1 + a2 * v3;
                const float v2 = ic2 + a2 * ic1 + a3 * v3;
                ic1 = 2.0f * v1 - ic1;
                ic2 = 2.0f * v2 - ic2;
                low = v2;
                band = v1;
                high = v0 - k * v1 - v2;
            }
        };

        // ------------------------------------------------------------ mix buses

        struct Mix
        {
            double sr;
            size_t n;
            std::vector<float> l, r;        // dry master
            std::vector<float> reverbSend;  // mono
            std::vector<float> delaySend;   // mono
            std::vector<float> duck;        // 0..1 kick sidechain shape (1 = fully ducked)
            std::vector<float> hallSend;    // mono - the long "atmosphere" hall (~8 s tail)

            void add(size_t i, float v, float pan, float rev = 0.0f, float del = 0.0f, float hall = 0.0f)
            {
                if (i >= n) return;
                // Constant-power-ish pan, pan in [-1, 1].
                const float pl = std::sqrt(0.5f * (1.0f - pan));
                const float pr = std::sqrt(0.5f * (1.0f + pan));
                l[i] += v * pl;
                r[i] += v * pr;
                if (rev != 0.0f) reverbSend[i] += v * rev;
                if (del != 0.0f) delaySend[i] += v * del;
                if (hall != 0.0f) hallSend[i] += v * hall;
            }
            float sc(size_t i, float depth) const { return i < n ? 1.0f - depth * duck[i] : 1.0f; }
        };

        // Per-bar filter "brightness" - what a producer would automate:
        // closed in intros/breaks, opening through builds, fully open in drops.
        std::vector<float> brightnessPerBar(const Song& song)
        {
            std::vector<float> b((size_t) song.totalBars + 1, 0.5f);
            for (const SongSection& s : song.sections)
                for (int i = 0; i < s.bars; ++i)
                {
                    const float t = s.bars <= 1 ? 1.0f : (float) i / (float) (s.bars - 1);
                    float v = 0.5f;
                    switch (s.kind)
                    {
                        case MusicSection::Intro:          v = 0.25f + 0.20f * t; break;
                        case MusicSection::Establish:      v = 0.50f + 0.10f * t; break;
                        case MusicSection::Build:          v = 0.50f + 0.35f * t; break;
                        case MusicSection::Drop:           v = 0.85f; break;
                        case MusicSection::FinalDrop:      v = 1.00f; break;
                        case MusicSection::PreDrop:        v = 0.75f - 0.35f * t; break;
                        case MusicSection::Breakdown:      v = song.style == SongStyle::DrivingTechno ? 0.60f + 0.25f * t // the reference's breakdown is bright and airy
                                                                                                 : 0.30f + 0.35f * t; break;
                        case MusicSection::BreakdownBuild: v = 0.55f + 0.45f * t; break;
                        case MusicSection::Outro:          v = 0.60f - 0.40f * t; break;
                    }
                    b[(size_t) (s.startBar + i)] = v;
                }
            b.back() = b[b.size() > 1 ? b.size() - 2 : 0];
            return b;
        }

        struct Ctx
        {
            const Song&        song;
            Mix&               mix;
            std::vector<float> bright;
            double             secPerBeat;

            float brightnessAtSample(size_t i) const
            {
                const double bar = (double) i / mix.sr / secPerBeat / 4.0;
                const size_t b0 = std::min((size_t) bar, bright.size() - 1);
                const size_t b1 = std::min(b0 + 1, bright.size() - 1);
                const float f = (float) (bar - (double) b0);
                return bright[b0] + (bright[b1] - bright[b0]) * f;
            }
        };

        // ------------------------------------------------------------ voices

        void renderKick(Ctx& c, size_t s0, float vel)
        {
            const double sr = c.mix.sr;
            const size_t len = (size_t) (0.5 * sr);
            double phase = 0.0;
            Noise nz;
            for (size_t j = 0; j < len; ++j)
            {
                const double t = j / sr;
                const double f = 48.0 + 115.0 * std::exp(-t / 0.032);
                phase += 2.0 * kPi * f / sr;
                const double amp = t < 0.015 ? 1.0 : std::exp(-(t - 0.015) / 0.17);
                float v = (float) (std::sin(phase) * amp) + nz.next() * 0.25f * (float) std::exp(-t / 0.002);
                v = std::tanh(1.8f * v) * 0.50f * vel;
                c.mix.add(s0 + j, v, 0.0f);
            }
        }

        void renderClap(Ctx& c, size_t s0, float vel)
        {
            const double sr = c.mix.sr;
            Svf bp;
            bp.set(1300.0, 1.4, sr);
            Noise nz;
            nz.s = 0xC1A9u;
            const size_t len = (size_t) (0.45 * sr);
            for (size_t j = 0; j < len; ++j)
            {
                const double t = j / sr;
                double env = 0.0;
                for (int b = 0; b < 3; ++b)
                {
                    const double tb = t - b * 0.0105;
                    if (tb >= 0.0) env = std::max(env, std::exp(-tb / 0.0045));
                }
                if (t > 0.021) env = std::max(env, 0.8 * std::exp(-(t - 0.021) / 0.11));
                bp.process(nz.next());
                c.mix.add(s0 + j, bp.band * (float) env * 2.2f * vel, 0.0f, 0.35f);
            }
        }

        void renderHat(Ctx& c, size_t s0, float vel, bool open)
        {
            const double sr = c.mix.sr;
            static constexpr double kMetal[6] = { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };
            std::array<SawOsc, 6> osc {};
            Svf hp;
            hp.set(open ? 7500.0 : 8500.0, 0.9, sr);
            Noise nz;
            nz.s = open ? 0x0BE1u : 0xC105u;
            const double decay = open ? 0.20 : 0.032 + 0.02 * vel;
            const size_t len = (size_t) ((open ? 0.9 : 0.25) * sr);
            for (size_t j = 0; j < len; ++j)
            {
                const double t = j / sr;
                float metal = 0.0f;
                for (int k = 0; k < 6; ++k)
                    metal += osc[(size_t) k].nextSquare(kMetal[k] * 1.9, sr);
                hp.process(nz.next() * 0.65f + metal * 0.08f);
                const float env = (float) std::exp(-t / decay);
                c.mix.add(s0 + j, hp.high * env * (c.song.style == SongStyle::MelodicTechno ? 0.95f : 0.55f) * vel, open ? -0.2f : 0.18f, open ? 0.08f : 0.0f);
            }
        }

        void renderPerc(Ctx& c, size_t s0, float vel, bool rim)
        {
            const double sr = c.mix.sr;
            const size_t len = (size_t) ((rim ? 0.15 : 0.5) * sr);
            double phase = 0.0;
            Svf bp;
            bp.set(rim ? 2600.0 : 900.0, 2.0, sr);
            Noise nz;
            nz.s = rim ? 0x51Au : 0x7033u;
            for (size_t j = 0; j < len; ++j)
            {
                const double t = j / sr;
                const double f = rim ? 820.0 : 150.0 + 80.0 * std::exp(-t / 0.03);
                phase += 2.0 * kPi * f / sr;
                bp.process(nz.next());
                const float body = (float) (std::sin(phase) * std::exp(-t / (rim ? 0.03 : 0.16)));
                const float click = bp.band * (float) std::exp(-t / (rim ? 0.012 : 0.02));
                c.mix.add(s0 + j, (body * (rim ? 0.3f : 0.4f) + click * 1.2f) * vel, rim ? -0.4f : 0.35f, 0.12f);
            }
        }

        void renderBass(Ctx& c, size_t s0, size_t noteLen, int pitch, float vel)
        {
            const double sr = c.mix.sr;
            const double f = mtof(pitch);
            const size_t release = (size_t) (0.03 * sr);
            SawOsc saw;
            double subPhase = 0.0;
            Svf lp;
            const float bright = c.brightnessAtSample(s0);
            for (size_t j = 0; j < noteLen + release; ++j)
            {
                const double t = j / sr;
                if (j % 8 == 0)
                {
                    if (c.song.style == SongStyle::MelodicTechno)
                        lp.set(160.0 + 320.0 * bright + (300.0 + 900.0 * bright) * std::exp(-t / 0.09), 1.3, sr);
                    else // punchy trance roller / clean neo-rave saw: brighter, faster envelope
                        lp.set(240.0 + 700.0 * bright + 2200.0 * bright * std::exp(-t / 0.045), 1.2, sr);
                }
                subPhase += 2.0 * kPi * f / sr;
                lp.process(saw.next(f, sr) * 0.7f);
                float amp = (float) std::min(1.0, t / 0.003);
                if (j >= noteLen) amp *= 1.0f - (float) (j - noteLen) / (float) release;
                float v = (lp.low + (float) std::sin(subPhase) * 0.75f) * amp;
                v = std::tanh(1.3f * v) * 0.42f * vel;
                c.mix.add(s0 + j, v * c.mix.sc(s0 + j, 0.8f), 0.0f);
            }
        }

        void renderPad(Ctx& c, size_t s0, size_t noteLen, int pitch, float vel, uint32_t seed)
        {
            const double sr = c.mix.sr;
            // Slow swells for melodic techno; faster trance pads; near-instant
            // for the 16th-note chops of a trance-gate pad.
            const bool   mt         = c.song.style == SongStyle::MelodicTechno
                                      || c.song.style == SongStyle::ProgressiveTechno
                                      || c.song.style == SongStyle::DrivingTechno; // slow swells
            const double noteSec    = (double) noteLen / sr;
            const double attack     = std::min(mt ? 1.4 : 0.3, std::max(0.003, noteSec * 0.25));
            const double releaseSec = noteSec < 0.5 ? 0.06 : (mt ? 2.2 : 1.2);
            const size_t release = (size_t) (releaseSec * sr);
            static constexpr double kDetune[3] = { -0.11, 0.0, 0.12 };
            static constexpr float  kPan[3]    = { -0.6f, 0.0f, 0.6f };
            std::array<SawOsc, 3> osc {};
            Noise nz;
            nz.s = seed | 1u;
            for (auto& o : osc)
                o.phase = (nz.next() + 1.0f) * 0.5f;
            std::array<Svf, 3> lp {};
            for (size_t j = 0; j < noteLen + release; ++j)
            {
                const double t = j / sr;
                if (j % 16 == 0)
                {
                    const float b = c.brightnessAtSample(s0 + j);
                    const double cutoff = 380.0 + 2600.0 * b * b + 250.0 * std::sin(t * 0.6);
                    for (auto& f : lp) f.set(cutoff, 0.85, sr);
                }
                float amp = (float) std::min(1.0, t / attack);
                if (j >= noteLen) amp *= (float) std::exp(-(double) (j - noteLen) / sr / (releaseSec / 4.0));
                const float g = amp * 0.11f * vel * c.mix.sc(s0 + j, 0.6f);
                for (int k = 0; k < 3; ++k)
                {
                    lp[(size_t) k].process(osc[(size_t) k].next(mtof(pitch + kDetune[k]), sr));
                    c.mix.add(s0 + j, lp[(size_t) k].low * g, kPan[k], 0.45f, 0.0f, mt ? 0.35f : 0.0f);
                }
            }
        }

        void renderArp(Ctx& c, size_t s0, size_t noteLen, int pitch, float vel)
        {
            const double sr = c.mix.sr;
            const double f = mtof(pitch);
            const size_t tail = (size_t) (0.06 * sr);
            SawOsc saw, sq;
            Svf lp;
            const float b = c.brightnessAtSample(s0);
            for (size_t j = 0; j < noteLen + tail; ++j)
            {
                const double t = j / sr;
                if (j % 8 == 0)
                    lp.set(420.0 + 1800.0 * b + 4200.0 * b * std::exp(-t / 0.075), 2.2, sr);
                lp.process(saw.next(f, sr) * 0.55f + sq.nextSquare(f * 1.003, sr) * 0.25f);
                float amp = (float) (std::min(1.0, t / 0.002) * std::exp(-t / 0.2));
                if (j >= noteLen) amp *= 1.0f - (float) (j - noteLen) / (float) tail;
                const float v = lp.low * amp * 0.40f * vel * c.mix.sc(s0 + j, 0.45f);
                c.mix.add(s0 + j, v, (pitch % 2 == 0) ? 0.25f : -0.25f, 0.18f, 0.35f);
            }
        }

        void renderLead(Ctx& c, size_t s0, size_t noteLen, int pitch, float vel)
        {
            const double sr = c.mix.sr;
            const size_t release = (size_t) (0.18 * sr);
            SawOsc a, b;
            Svf lp;
            const float br = c.brightnessAtSample(s0);
            lp.set(1100.0 + 3200.0 * br, 1.1, sr);
            for (size_t j = 0; j < noteLen + release; ++j)
            {
                const double t = j / sr;
                const double vib = t > 0.25 ? 0.12 * std::sin(2.0 * kPi * 5.2 * t) * std::min(1.0, (t - 0.25) / 0.3) : 0.0;
                lp.process(a.next(mtof(pitch - 0.07 + vib), sr) * 0.5f + b.next(mtof(pitch + 0.07 + vib), sr) * 0.5f);
                float amp = (float) std::min(1.0, t / 0.012);
                if (j >= noteLen) amp *= (float) std::exp(-(double) (j - noteLen) / sr / 0.05);
                const float v = lp.low * amp * 0.30f * vel * c.mix.sc(s0 + j, 0.35f);
                c.mix.add(s0 + j, v, 0.0f, 0.4f, 0.28f);
            }
        }

        // ATMOS: an evolving drone/texture - 6 detuned saws spread across the
        // stereo field through a low-pass that sweeps slowly (a ~20 s LFO,
        // phase per note), plus band-passed noise "air" riding on top, a
        // 2-4 s swell, breathing with the kick, sent deep into the hall.
        void renderAtmos(Ctx& c, size_t s0, size_t noteLen, int pitch, float vel, uint32_t seed)
        {
            const double sr = c.mix.sr;
            const double attack = std::min(3.0, std::max(0.05, noteLen / sr * 0.35));
            const size_t release = (size_t) (3.5 * sr);
            static constexpr double kDet[6] = { -0.16, -0.09, -0.03, 0.03, 0.1, 0.17 };
            static constexpr float  kPan[6] = { -0.95f, -0.55f, -0.2f, 0.2f, 0.55f, 0.95f };
            std::array<SawOsc, 6> osc {};
            Noise nz;
            nz.s = seed | 1u;
            for (auto& o : osc) o.phase = (nz.next() + 1.0f) * 0.5f;
            std::array<Svf, 6> lp {};
            Svf airL, airR;
            const double lfoPhase = (nz.next() + 1.0) * kPi;
            for (size_t j = 0; j < noteLen + release; ++j)
            {
                const double t = j / sr;
                const double abs = (double) (s0 + j) / sr;
                if (j % 32 == 0)
                {
                    const float b = c.brightnessAtSample(s0 + j);
                    const double sweep = 0.5 + 0.5 * std::sin(2.0 * kPi * abs / 21.0 + lfoPhase);
                    const double cutoff = 300.0 + (900.0 + 2600.0 * b) * sweep;
                    for (auto& f : lp) f.set(cutoff, 0.9, sr);
                    airL.set(3500.0 + 3000.0 * sweep, 1.6, sr);
                    airR.set(4200.0 + 2600.0 * (1.0 - sweep), 1.6, sr);
                }
                double env = std::min(1.0, t / attack);
                if (j >= noteLen) env *= std::exp(-(double) (j - noteLen) / sr / 1.0);
                const float g = (float) env * 0.05f * vel * c.mix.sc(s0 + j, 0.5f);
                for (int k = 0; k < 6; ++k)
                {
                    lp[(size_t) k].process(osc[(size_t) k].next(mtof(pitch + kDet[k]), sr));
                    c.mix.add(s0 + j, lp[(size_t) k].low * g, kPan[k], 0.1f, 0.0f, 0.55f);
                }
                airL.process(nz.next());
                airR.process(nz.next());
                c.mix.add(s0 + j, airL.band * g * 1.8f, -0.85f, 0.0f, 0.0f, 0.5f);
                c.mix.add(s0 + j, airR.band * g * 1.8f, 0.85f, 0.0f, 0.0f, 0.5f);
            }
        }

        // Driving-techno RUMBLE: a driven sub (sine through tanh = warm
        // harmonics) with a long, low-passed smear so consecutive 16ths blur
        // into a continuous growl - the reference's low end has no single
        // clean pitch, just energy spread across ~45-60 Hz.
        void renderRumble(Ctx& c, size_t s0, size_t noteLen, int pitch, float vel)
        {
            const double sr = c.mix.sr;
            const double f = mtof(pitch);
            const bool longNote = noteLen > (size_t) (0.5 * sr);
            const size_t len = noteLen + (size_t) ((longNote ? 0.6 : 0.22) * sr);
            double ph = 0.0, ph2 = 0.0;
            Svf lp;
            lp.set(130.0, 0.8, sr);
            for (size_t j = 0; j < len; ++j)
            {
                const double t = j / sr;
                ph  += 2.0 * kPi * f / sr;
                ph2 += 2.0 * kPi * f * 1.012 / sr; // slight detune = movement
                const double env = longNote ? std::min(1.0, t / 0.2) * (j < noteLen ? 1.0 : std::exp(-(double) (j - noteLen) / sr / 0.2))
                                            : std::exp(-t / 0.075) * 0.8 + 0.2 * std::exp(-t / 0.3);
                lp.process((float) std::tanh(2.4 * (std::sin(ph) + 0.5 * std::sin(ph2))));
                const float v = lp.low * (float) env * (longNote ? 0.12f : 0.55f) * vel * c.mix.sc(s0 + j, 0.9f);
                c.mix.add(s0 + j, v, 0.0f, 0.04f);
            }
        }

        // Pumping tonic/5th STAB: short filtered saw, heavily ducked by the kick.
        void renderStab(Ctx& c, size_t s0, size_t noteLen, int pitch, float vel)
        {
            const double sr = c.mix.sr;
            const double f = mtof(pitch);
            const size_t len = noteLen + (size_t) (0.05 * sr);
            SawOsc a, b;
            Svf lp;
            const float br = c.brightnessAtSample(s0);
            for (size_t j = 0; j < len; ++j)
            {
                const double t = j / sr;
                if (j % 8 == 0)
                    lp.set(300.0 + 1100.0 * br + 1200.0 * br * std::exp(-t / 0.03), 1.6, sr);
                lp.process(a.next(f * 0.997, sr) * 0.5f + b.next(f * 1.003, sr) * 0.5f);
                const float env = (float) (std::min(1.0, t / 0.002) * std::exp(-t / 0.06));
                c.mix.add(s0 + j, lp.low * env * 0.30f * vel * c.mix.sc(s0 + j, 0.7f), (pitch % 2 == 0) ? 0.45f : -0.45f, 0.08f, 0.1f);
            }
        }

        // Driving-techno HOOK synth: mid-range, dark and round (saw + pulse
        // through a gentle low-pass), singing sustain with a soft tail, sent to
        // the dotted-8th delay and the reverb.
        void renderHook(Ctx& c, size_t s0, size_t noteLen, int pitch, float vel)
        {
            const double sr = c.mix.sr;
            const double f = mtof(pitch);
            const size_t tail = (size_t) (0.25 * sr);
            SawOsc a, b, sq;
            Svf lp, lpR;
            const float br = c.brightnessAtSample(s0);
            for (size_t j = 0; j < noteLen + tail; ++j)
            {
                const double t = j / sr;
                if (j % 8 == 0)
                {
                    lp.set(700.0 + 2400.0 * br + 900.0 * std::exp(-t / 0.08), 1.2, sr);
                    lpR.set(700.0 + 2400.0 * br + 900.0 * std::exp(-t / 0.08), 1.2, sr);
                }
                const float sub = sq.nextSquare(f * 0.5, sr) * 0.15f;
                lp.process(a.next(f * 0.996, sr) * 0.45f + sub);
                const float left = lp.low;
                lpR.process(b.next(f * 1.004, sr) * 0.45f + sub);
                double env = std::min(1.0, t / 0.004) * (0.55 + 0.45 * std::exp(-t / 0.18));
                if (j >= noteLen) env *= std::exp(-(double) (j - noteLen) / sr / 0.08);
                const float g = (float) env * 0.6f * vel * c.mix.sc(s0 + j, 0.4f);
                c.mix.add(s0 + j, left * g, -0.6f, 0.3f, 0.35f, 0.25f);
                c.mix.add(s0 + j, lpR.low * g, 0.6f, 0.3f, 0.35f, 0.25f);
            }
        }

        // Warm, wide progressive arp - matched to the reference loop's measured
        // character: ~97% of its energy below 2 kHz, a ~50 ms hold then a gentle
        // ~9 dB fall across the 16th (notes blur into each other), wide stereo
        // (side/mid 0.48), and a strong dotted-8th echo. Two detuned saws + a
        // pulse, panned apart, through a soft low-pass that the arrangement's
        // brightness curve opens.
        void renderWarmArp(Ctx& c, size_t s0, size_t noteLen, int pitch, float vel)
        {
            const double sr = c.mix.sr;
            const double f = mtof(pitch);
            const size_t tail = (size_t) (0.16 * sr);
            SawOsc a, b, sq;
            Svf fl, fr;
            const float br = c.brightnessAtSample(s0);
            for (size_t j = 0; j < noteLen + tail; ++j)
            {
                const double t = j / sr;
                if (j % 8 == 0)
                {
                    const bool driving = c.song.style == SongStyle::DrivingTechno;
                    const double cutoff = driving ? 1100.0 + 4200.0 * br + 2500.0 * std::exp(-t / 0.06)
                                                  : 520.0 + 2700.0 * br * br + 1800.0 * br * std::exp(-t / 0.07);
                    fl.set(cutoff, 1.25, sr);
                    fr.set(cutoff, 1.25, sr);
                }
                const float pulse = sq.nextSquare(f * 0.5, sr) * 0.18f; // sub-octave body
                fl.process(a.next(f * 0.9965, sr) * 0.5f + pulse);
                fr.process(b.next(f * 1.0035, sr) * 0.5f + pulse);
                double env = t < 0.05 ? 1.0 : std::exp(-(t - 0.05) / 0.11);
                if (j >= noteLen) env *= std::exp(-(double) (j - noteLen) / sr / 0.05);
                const float g = (float) std::min(1.0, t / 0.003) * (float) env * 0.5f * vel * c.mix.sc(s0 + j, 0.35f);
                const float hall = c.song.style == SongStyle::DrivingTechno ? 0.3f : 0.0f;
                c.mix.add(s0 + j, fl.low * g, -0.9f, 0.25f, 0.42f, hall);
                c.mix.add(s0 + j, fr.low * g, 0.9f, 0.25f, 0.42f, hall);
            }
        }

        // 7-voice detuned supersaw, the core trance sound. Writes a stereo pair.
        struct SuperSaw
        {
            static constexpr int kVoices = 7;
            std::array<SawOsc, kVoices> osc {};
            explicit SuperSaw(uint32_t seed)
            {
                Noise nz;
                nz.s = seed | 1u;
                for (auto& o : osc) o.phase = (nz.next() + 1.0f) * 0.5f;
            }
            void next(double pitch, double detuneSemis, double sr, float& l, float& r)
            {
                static constexpr double kSpread[kVoices] = { -1.0, -0.62, -0.3, 0.0, 0.28, 0.6, 0.97 };
                l = r = 0.0f;
                for (int k = 0; k < kVoices; ++k)
                {
                    const float v = osc[(size_t) k].next(mtof(pitch + kSpread[k] * detuneSemis), sr);
                    const float pan = (float) kSpread[k] * 0.8f;
                    l += v * (1.0f - pan) * 0.5f;
                    r += v * (1.0f + pan) * 0.5f;
                }
                l /= 3.0f;
                r /= 3.0f;
            }
        };

        // Trance anthem lead: supersaw through an open low-pass, short
        // attack, a little vibrato on held notes, heavy delay + reverb.
        void renderSuperLead(Ctx& c, size_t s0, size_t noteLen, int pitch, float vel, bool stab)
        {
            const double sr = c.mix.sr;
            const size_t release = (size_t) ((stab ? 0.08 : 0.22) * sr);
            SuperSaw ss((uint32_t) (s0 * 2654435761u) ^ (uint32_t) pitch);
            Svf fl, fr;
            const float br = c.brightnessAtSample(s0);
            for (size_t j = 0; j < noteLen + release; ++j)
            {
                const double t = j / sr;
                if (j % 16 == 0)
                {
                    const double cutoff = (stab ? 900.0 : 1300.0) + 6500.0 * br * br
                                          + (stab ? 3000.0 * std::exp(-t / 0.06) : 0.0);
                    fl.set(cutoff, 0.9, sr);
                    fr.set(cutoff, 0.9, sr);
                }
                const double vib = (!stab && t > 0.3) ? 0.1 * std::sin(2.0 * kPi * 5.5 * t) : 0.0;
                float l, r;
                ss.next(pitch + vib, stab ? 0.18 : 0.24, sr, l, r);
                fl.process(l);
                fr.process(r);
                float amp = (float) std::min(1.0, t / 0.004);
                if (j >= noteLen) amp *= (float) std::exp(-(double) (j - noteLen) / sr / (stab ? 0.025 : 0.07));
                const float g = amp * 0.34f * vel * c.mix.sc(s0 + j, 0.4f);
                c.mix.add(s0 + j, fl.low * g, -0.5f, 0.35f, 0.22f);
                c.mix.add(s0 + j, fr.low * g, 0.5f, 0.35f, 0.22f);
            }
        }

        // Trance pluck: supersaw with a fast filter + amp decay.
        void renderPluck(Ctx& c, size_t s0, size_t noteLen, int pitch, float vel)
        {
            const double sr = c.mix.sr;
            const size_t len = std::max(noteLen, (size_t) (0.28 * sr));
            SuperSaw ss((uint32_t) (s0 * 40503u) ^ (uint32_t) pitch);
            Svf fl, fr;
            const float br = c.brightnessAtSample(s0);
            for (size_t j = 0; j < len; ++j)
            {
                const double t = j / sr;
                if (j % 8 == 0)
                {
                    const double cutoff = 350.0 + 900.0 * br + 6000.0 * br * std::exp(-t / 0.07);
                    fl.set(cutoff, 1.4, sr);
                    fr.set(cutoff, 1.4, sr);
                }
                float l, r;
                ss.next(pitch, 0.12, sr, l, r);
                fl.process(l);
                fr.process(r);
                const float amp = (float) (std::min(1.0, t / 0.002) * std::exp(-t / 0.16));
                const float g = amp * 0.42f * vel * c.mix.sc(s0 + j, 0.45f);
                c.mix.add(s0 + j, fl.low * g, -0.45f, 0.25f, 0.3f);
                c.mix.add(s0 + j, fr.low * g, 0.45f, 0.25f, 0.3f);
            }
        }

        // Acid 303: saw into a resonant low-pass with an accent-driven envelope.
        void renderAcid(Ctx& c, size_t s0, size_t noteLen, int pitch, float vel)
        {
            const double sr = c.mix.sr;
            const double f = mtof(pitch);
            const size_t tail = (size_t) (0.02 * sr);
            SawOsc saw;
            Svf lp;
            const float br = c.brightnessAtSample(s0);
            const bool accent = vel > 0.9f;
            for (size_t j = 0; j < noteLen + tail; ++j)
            {
                const double t = j / sr;
                if (j % 8 == 0)
                    lp.set(250.0 + 900.0 * br + (accent ? 3500.0 : 1600.0) * br * std::exp(-t / (accent ? 0.09 : 0.05)),
                           accent ? 9.0 : 6.0, sr);
                lp.process(saw.next(f, sr));
                float amp = (float) std::min(1.0, t / 0.002);
                if (j >= noteLen) amp *= 1.0f - (float) (j - noteLen) / (float) tail;
                const float v = std::tanh(lp.low * 1.8f) * amp * (accent ? 0.30f : 0.22f) * c.mix.sc(s0 + j, 0.5f);
                c.mix.add(s0 + j, v, 0.1f, 0.12f, 0.2f);
            }
        }

        // Snare for rolls and fills: tone + high-passed noise, into the reverb.
        void renderSnare(Ctx& c, size_t s0, float vel)
        {
            const double sr = c.mix.sr;
            const size_t len = (size_t) (0.22 * sr);
            double phase = 0.0;
            Svf hp;
            hp.set(1800.0, 0.8, sr);
            Noise nz;
            nz.s = 0x5A4Eu;
            for (size_t j = 0; j < len; ++j)
            {
                const double t = j / sr;
                phase += 2.0 * kPi * (185.0 + 60.0 * std::exp(-t / 0.01)) / sr;
                hp.process(nz.next());
                const float v = (float) (std::sin(phase) * std::exp(-t / 0.05)) * 0.35f + hp.high * (float) std::exp(-t / 0.07) * 0.9f;
                c.mix.add(s0 + j, v * vel * 0.7f, 0.0f, 0.3f);
            }
        }

        // Crash (FX lane) and ride (perc lane): inharmonic metal + noise.
        void renderCymbal(Ctx& c, size_t s0, float vel, bool crash)
        {
            const double sr = c.mix.sr;
            static constexpr double kMetal[6] = { 263.0, 400.0, 421.0, 474.0, 587.0, 845.0 };
            std::array<SawOsc, 6> osc {};
            Svf hp;
            hp.set(crash ? 4200.0 : 6500.0, 0.7, sr);
            Noise nz;
            nz.s = crash ? 0xC4A5u : 0x41DEu;
            const double decay = crash ? 1.1 : 0.3;
            const size_t len = (size_t) (decay * 4.0 * sr);
            for (size_t j = 0; j < len; ++j)
            {
                const double t = j / sr;
                float metal = 0.0f;
                for (int k = 0; k < 6; ++k)
                    metal += osc[(size_t) k].nextSquare(kMetal[k] * (crash ? 3.1 : 4.3), sr);
                hp.process(nz.next() * (crash ? 0.8f : 0.3f) + metal * 0.12f);
                const float env = (float) std::exp(-t / decay);
                c.mix.add(s0 + j, hp.high * env * (crash ? 0.45f : 0.22f) * vel, crash ? 0.1f : 0.45f, crash ? 0.25f : 0.05f);
            }
        }

        void renderFx(Ctx& c, size_t s0, size_t noteLen, int pitch, float vel)
        {
            const double sr = c.mix.sr;
            Noise nz;
            nz.s = 0xF00Du + (uint32_t) pitch;
            Svf f;
            if (pitch == SongFxNotes::kCrash)
            {
                renderCymbal(c, s0, vel, true);
                return;
            }
            if (pitch == SongFxNotes::kImpact)
            {
                double phase = 0.0;
                f.set(700.0, 0.7, sr);
                const size_t len = (size_t) (2.5 * sr);
                for (size_t j = 0; j < len; ++j)
                {
                    const double t = j / sr;
                    phase += 2.0 * kPi * (38.0 + 30.0 * std::exp(-t / 0.08)) / sr;
                    f.process(nz.next());
                    const float v = (float) (std::sin(phase) * std::exp(-t / 0.9)) * 0.55f
                                    + f.low * (float) std::exp(-t / 0.35) * 0.6f;
                    c.mix.add(s0 + j, v * vel, 0.0f, 0.5f);
                }
                return;
            }
            const bool riser = pitch == SongFxNotes::kRiser;
            const size_t len = riser ? noteLen : (size_t) (2.0 * 4.0 * c.secPerBeat * sr);
            for (size_t j = 0; j < len; ++j)
            {
                const double x = (double) j / (double) len; // 0..1
                const double cutoff = riser ? 250.0 * std::pow(40.0, x) : 9000.0 * std::pow(1.0 / 45.0, x);
                if (j % 16 == 0) f.set(cutoff, 2.5, sr);
                f.process(nz.next());
                const float amp = (float) (riser ? x * x : (1.0 - x) * (1.0 - x));
                const float pan = (float) std::sin(2.0 * kPi * x * (riser ? 3.0 : 1.5)) * 0.5f;
                c.mix.add(s0 + j, f.band * amp * 0.45f * vel, pan, 0.45f);
            }
        }

        // ------------------------------------------------------------ effects

        // Freeverb-style reverb: 8 parallel combs + 4 series allpasses per side.
        // Freeverb-style reverb. The default settings are the room used for
        // everything; the hall (feedback 0.935, longer pre-delay, wider L/R
        // spread) is the long atmosphere space pads, drones and throws sit in.
        void applyReverb(Mix& m, const std::vector<float>& send, std::vector<float>& outL, std::vector<float>& outR,
                         float feedback = 0.86f, float damp = 0.3f, float gain = 3.0f, int stereoSpread = 23,
                         double preDelaySec = 0.0)
        {
            static constexpr int kComb[8]    = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
            static constexpr int kAllpass[4] = { 556, 441, 341, 225 };
            const double scale = m.sr / 44100.0;
            const size_t pre = (size_t) (preDelaySec * m.sr);
            for (int side = 0; side < 2; ++side)
            {
                const int spread = side == 0 ? 0 : stereoSpread;
                std::vector<std::vector<float>> combs, alls;
                std::vector<size_t> ci(8, 0), ai(4, 0);
                std::vector<float> store(8, 0.0f);
                for (int k = 0; k < 8; ++k) combs.emplace_back((size_t) ((kComb[k] + spread) * scale), 0.0f);
                for (int k = 0; k < 4; ++k) alls.emplace_back((size_t) ((kAllpass[k] + spread) * scale), 0.0f);
                std::vector<float>& out = side == 0 ? outL : outR;
                for (size_t i = 0; i < m.n; ++i)
                {
                    const float in = (i >= pre ? send[i - pre] : 0.0f) * 0.015f;
                    float acc = 0.0f;
                    for (int k = 0; k < 8; ++k)
                    {
                        auto& buf = combs[(size_t) k];
                        const float y = buf[ci[(size_t) k]];
                        store[(size_t) k] = y * (1.0f - damp) + store[(size_t) k] * damp;
                        buf[ci[(size_t) k]] = in + store[(size_t) k] * feedback;
                        if (++ci[(size_t) k] >= buf.size()) ci[(size_t) k] = 0;
                        acc += y;
                    }
                    for (int k = 0; k < 4; ++k)
                    {
                        auto& buf = alls[(size_t) k];
                        const float b = buf[ai[(size_t) k]];
                        buf[ai[(size_t) k]] = acc + b * 0.5f;
                        acc = b - acc;
                        if (++ai[(size_t) k] >= buf.size()) ai[(size_t) k] = 0;
                    }
                    out[i] += acc * gain;
                }
            }
        }

        // Dotted-8th ping-pong delay with damped feedback.
        void applyDelay(Mix& m, double secPerBeat, std::vector<float>& outL, std::vector<float>& outR)
        {
            const size_t d = std::max<size_t>(1, (size_t) (0.75 * secPerBeat * m.sr));
            std::vector<float> bl(d, 0.0f), br(d, 0.0f);
            size_t idx = 0;
            float dampL = 0.0f, dampR = 0.0f;
            for (size_t i = 0; i < m.n; ++i)
            {
                const float yl = bl[idx], yr = br[idx];
                dampL += 0.35f * (yl - dampL);
                dampR += 0.35f * (yr - dampR);
                bl[idx] = m.delaySend[i] + dampR * 0.42f;
                br[idx] = dampL * 0.42f;
                if (++idx >= d) idx = 0;
                outL[i] += yl * 0.55f;
                outR[i] += yr * 0.55f;
            }
        }
    }

    StereoBuffer renderSongPreview(const Song& song, double sampleRate)
    {
        const double secPerBeat = 60.0 / std::max(1.0, song.bpm);
        const double songSeconds = song.totalBars * 4.0 * secPerBeat;
        const size_t n = (size_t) ((songSeconds + 8.0) * sampleRate); // + hall/release tail

        Mix mix { sampleRate, n, std::vector<float>(n, 0.0f), std::vector<float>(n, 0.0f),
                  std::vector<float>(n, 0.0f), std::vector<float>(n, 0.0f), std::vector<float>(n, 0.0f),
                  std::vector<float>(n, 0.0f) };
        Ctx ctx { song, mix, brightnessPerBar(song), secPerBeat };

        auto toSample = [&](const SongClip& c, double beat) { return (size_t) ((c.startBar * 4.0 + beat) * secPerBeat * sampleRate); };

        // Sidechain shape from every kick: instant duck, ~200 ms quadratic recovery.
        for (const SongTrack& t : song.tracks)
            if (t.role == SongTrackRole::Kick)
                for (const SongClip& c : t.clips)
                    for (const SongNote& note : c.notes)
                    {
                        const size_t s0 = toSample(c, note.startBeat);
                        const size_t rel = (size_t) (0.2 * sampleRate);
                        for (size_t j = 0; j < rel && s0 + j < n; ++j)
                        {
                            const float x = 1.0f - (float) j / (float) rel;
                            mix.duck[s0 + j] = std::max(mix.duck[s0 + j], x * x);
                        }
                    }

        uint32_t padSeed = song.seed;
        for (const SongTrack& t : song.tracks)
            for (const SongClip& c : t.clips)
                for (const SongNote& note : c.notes)
                {
                    const size_t s0  = toSample(c, note.startBeat);
                    const size_t len = std::max<size_t>(1, (size_t) (note.lengthBeats * secPerBeat * sampleRate));
                    const float vel  = (float) note.velocity / 127.0f;
                    switch (t.role)
                    {
                        case SongTrackRole::Kick: renderKick(ctx, s0, vel); break;
                        case SongTrackRole::Clap: renderClap(ctx, s0, vel); break;
                        case SongTrackRole::Hats: renderHat(ctx, s0, vel, note.pitch == SongDrumNotes::kHatOpen); break;
                        case SongTrackRole::Perc:
                            if (note.pitch == SongDrumNotes::kRide) renderCymbal(ctx, s0, vel, false);
                            else renderPerc(ctx, s0, vel, note.pitch == SongDrumNotes::kRim);
                            break;
                        case SongTrackRole::SnareRoll: renderSnare(ctx, s0, vel); break;
                        case SongTrackRole::Pluck:     renderPluck(ctx, s0, len, note.pitch, vel); break;
                        case SongTrackRole::Acid:      renderAcid(ctx, s0, len, note.pitch, vel); break;
                        case SongTrackRole::Bass:
                            if (song.style == SongStyle::DrivingTechno) renderRumble(ctx, s0, len, note.pitch, vel);
                            else renderBass(ctx, s0, len, note.pitch, vel);
                            break;
                        case SongTrackRole::Stab: renderStab(ctx, s0, len, note.pitch, vel); break;
                        case SongTrackRole::Atmos: renderAtmos(ctx, s0, len, note.pitch, vel, padSeed = padSeed * 1664525u + 1013904223u); break;
                        case SongTrackRole::Pad:  renderPad(ctx, s0, len, note.pitch, vel, padSeed = padSeed * 1664525u + 1013904223u); break;
                        case SongTrackRole::Arp:
                            if (song.style == SongStyle::ProgressiveTechno || song.style == SongStyle::DrivingTechno)
                                renderWarmArp(ctx, s0, len, note.pitch, vel);
                            else renderArp(ctx, s0, len, note.pitch, vel);
                            break;
                        case SongTrackRole::Lead:
                            if (song.style == SongStyle::DrivingTechno)
                                renderHook(ctx, s0, len, note.pitch, vel);
                            else if (song.style == SongStyle::MelodicTechno || song.style == SongStyle::ProgressiveTechno)
                                renderLead(ctx, s0, len, note.pitch, vel);
                            else renderSuperLead(ctx, s0, len, note.pitch, vel, song.style == SongStyle::NeoRave);
                            break;
                        case SongTrackRole::Fx:   renderFx(ctx, s0, len, note.pitch, vel); break;
                    }
                }

        StereoBuffer out;
        out.sampleRate = sampleRate;
        out.left  = mix.l;
        out.right = mix.r;
        applyDelay(mix, secPerBeat, out.left, out.right);
        applyReverb(mix, mix.reverbSend, out.left, out.right);
        applyReverb(mix, mix.hallSend, out.left, out.right, 0.935f, 0.5f, 1.4f, 61, 0.045);

        // Master: DC-blocking high-pass (~15 Hz), normalise, gentle tanh
        // saturation as a limiter, final gain.
        for (std::vector<float>* ch : { &out.left, &out.right })
        {
            const float rc = (float) std::exp(-2.0 * kPi * 15.0 / sampleRate);
            float px = 0.0f, py = 0.0f;
            for (float& v : *ch)
            {
                const float y = v - px + rc * py;
                px = v;
                py = y;
                v = y;
            }
        }
        float peak = 1e-9f;
        for (size_t i = 0; i < n; ++i)
            peak = std::max(peak, std::max(std::fabs(out.left[i]), std::fabs(out.right[i])));
        const float driveAmt = song.style == SongStyle::DrivingTechno ? 4.5f : 2.6f; // peak-time techno is mastered hot
        const float drive = driveAmt / peak;
        const float norm = 0.93f / std::tanh(driveAmt);
        for (size_t i = 0; i < n; ++i)
        {
            out.left[i]  = std::tanh(out.left[i] * drive) * norm;
            out.right[i] = std::tanh(out.right[i] * drive) * norm;
        }
        return out;
    }

    bool writeWav16(const StereoBuffer& audio, const std::string& path)
    {
        FILE* f = std::fopen(path.c_str(), "wb");
        if (f == nullptr)
            return false;
        const uint32_t frames = (uint32_t) std::min(audio.left.size(), audio.right.size());
        const uint32_t sr = (uint32_t) audio.sampleRate;
        const uint32_t dataBytes = frames * 4;
        auto u32 = [&](uint32_t v) { uint8_t b[4] = { (uint8_t) v, (uint8_t) (v >> 8), (uint8_t) (v >> 16), (uint8_t) (v >> 24) }; std::fwrite(b, 1, 4, f); };
        auto u16 = [&](uint16_t v) { uint8_t b[2] = { (uint8_t) v, (uint8_t) (v >> 8) }; std::fwrite(b, 1, 2, f); };
        std::fwrite("RIFF", 1, 4, f); u32(36 + dataBytes); std::fwrite("WAVE", 1, 4, f);
        std::fwrite("fmt ", 1, 4, f); u32(16); u16(1); u16(2); u32(sr); u32(sr * 4); u16(4); u16(16);
        std::fwrite("data", 1, 4, f); u32(dataBytes);
        std::vector<int16_t> buf;
        buf.reserve(8192);
        for (uint32_t i = 0; i < frames; ++i)
        {
            for (float s : { audio.left[i], audio.right[i] })
                buf.push_back((int16_t) std::lrint(std::max(-1.0f, std::min(1.0f, s)) * 32767.0f));
            if (buf.size() >= 8192)
            {
                std::fwrite(buf.data(), 2, buf.size(), f); // little-endian hosts (macOS/x86/arm)
                buf.clear();
            }
        }
        std::fwrite(buf.data(), 2, buf.size(), f);
        return std::fclose(f) == 0;
    }
}
