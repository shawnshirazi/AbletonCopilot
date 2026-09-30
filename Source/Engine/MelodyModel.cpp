#include "MelodyModel.h"
#include "MelodyModelData.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <set>

namespace Engine
{
    namespace MelodyModel
    {
        namespace
        {
            using Data = MelodyModelData::ArchetypeModel;

            const Data& model(Archetype a) { return a == Archetype::Arp ? MelodyModelData::kArp : MelodyModelData::kLine; }

            int st(const Spec& s, int d) { return degreeToSemitone(d, s.scale); }
            int pitchOf(const Spec& s, int d) { return s.rootNote + st(s, d); }
            int mod7(int x) { return ((x % 7) + 7) % 7; }

            int degreeOf(const Spec& s, int pitch) // in-scale pitch -> absolute degree
            {
                int d = (int) std::floor((pitch - s.rootNote) / 12.0) * 7;
                while (pitchOf(s, d) < pitch) ++d;
                while (pitchOf(s, d) > pitch) --d;
                return d;
            }

            int chordAt(const Spec& s, int step)
            {
                if (s.chordDegreeByBar.empty()) return 0;
                const int bar = std::max(0, std::min((int) s.chordDegreeByBar.size() - 1, step / 16));
                return s.chordDegreeByBar[(size_t) bar];
            }

            bool strongPos(int step, int len) { return step % 8 == 0 || len >= 6; }

            double noteLogProb(const Spec& s, const Data& m, int prevIv, int d, int dPrev, int step, int len, bool last, bool first)
            {
                double lp = 0.0;
                const int iv = std::max(-7, std::min(7, d - dPrev));
                if (!first)
                    lp += m.intervalLogProb[std::max(-7, std::min(7, prevIv)) + 7][iv + 7];
                const int rel = mod7(d - chordAt(s, step));
                lp += strongPos(step, len) ? m.strongChordDegLogProb[rel] : m.weakChordDegLogProb[rel];
                if (last && s.endHome)
                    lp += 1.5 * m.finalDegreeLogProb[mod7(d)];
                return lp;
            }

            // Weighted pick of a bar rhythm; `maxNotes` limits busy bars (endings).
            const MelodyModelData::BarRhythm& sampleRhythm(const Data& m, std::mt19937& rng, int minNotes, int maxNotes,
                                                           int maxFirstOnset = 15)
            {
                auto ok = [&](int i) { return m.rhythms[i].notes >= minNotes && m.rhythms[i].notes <= maxNotes
                                              && m.rhythms[i].onset[0] <= maxFirstOnset; };
                int total = 0;
                for (int i = 0; i < m.numRhythms; ++i)
                    if (ok(i)) total += m.rhythms[i].count;
                if (total == 0) return m.rhythms[0];
                int r = (int) (rng() % (uint32_t) total);
                for (int i = 0; i < m.numRhythms; ++i)
                    if (ok(i))
                    {
                        r -= m.rhythms[i].count;
                        if (r < 0) return m.rhythms[i];
                    }
                return m.rhythms[0];
            }

            double uniform(std::mt19937& rng) { return (double) (rng() >> 8) / (double) (1u << 24); }
        }

        bool isMusical(const Spec& spec, const std::vector<Note>& ph)
        {
            if (ph.size() < (size_t) std::max(4, spec.bars * 2)) return false;
            std::set<int> distinct;
            int repeats = 0, lo = 999, hi = -999;
            for (size_t i = 0; i < ph.size(); ++i)
            {
                distinct.insert(ph[i].pitch);
                lo = std::min(lo, ph[i].pitch);
                hi = std::max(hi, ph[i].pitch);
                if (i > 0 && ph[i].pitch == ph[i - 1].pitch) ++repeats;
            }
            const bool arp = spec.archetype == Archetype::Arp;
            return (int) distinct.size() >= (arp ? 3 : 5)
                   && hi - lo >= 5
                   && repeats * 100 <= (int) ph.size() * (arp ? 60 : 50);
        }

        // Distance of the phrase's overall shape from the corpus's
        // interquartile range (0 inside it): longest repeated-note run, mean
        // run, range, distinct pitches, direction-change rate, leap rate.
        double profilePenalty(const Spec& spec, const std::vector<Note>& ph)
        {
            const Data& m = model(spec.archetype);
            if (ph.size() < 2) return 10.0;
            int runMax = 1, run = 1, runs = 1, lo = 999, hi = -999, leaps = 0, dirCh = 0, lastDir = 0, nz = 0;
            std::set<int> distinct;
            for (size_t i = 0; i < ph.size(); ++i)
            {
                distinct.insert(ph[i].pitch);
                lo = std::min(lo, ph[i].pitch);
                hi = std::max(hi, ph[i].pitch);
                if (i == 0) continue;
                const int mv = ph[i].pitch - ph[i - 1].pitch;
                if (mv == 0) { run += 1; runMax = std::max(runMax, run); }
                else
                {
                    run = 1;
                    ++runs;
                    const int dir = mv > 0 ? 1 : -1;
                    if (lastDir != 0 && dir != lastDir) ++dirCh;
                    lastDir = dir;
                    ++nz;
                }
                if (std::abs(mv) >= 5) ++leaps;
            }
            // Scale to an 8-bar window (the profile's unit).
            const double f[6] = { (double) runMax, (double) ph.size() / (double) runs, (double) (hi - lo), (double) distinct.size(),
                                  nz > 1 ? (double) dirCh / (double) (nz - 1) : 0.0, (double) leaps / (double) (ph.size() - 1) };
            // Target band per feature: the corpus's interquartile range,
            // narrowed to its MORE MELODIC half where the user's feedback has
            // been explicit (stuttering / static lines rejected twice): repeat
            // runs in [p25, p50], distinct pitches and direction changes in
            // [p50, p75]; range and leap rate across the full IQR.
            const double bandLo[6] = { m.profileP25[0], m.profileP25[1], m.profileP25[2], m.profileP50[3], m.profileP50[4], m.profileP25[5] };
            const double bandHi[6] = { m.profileP50[0], m.profileP50[1], m.profileP75[2], m.profileP75[3], m.profileP75[4], m.profileP75[5] };
            static constexpr double kWeight[6] = { 3.0, 3.0, 1.0, 2.0, 1.5, 1.0 };
            double pen = 0.0;
            for (int k = 0; k < 6; ++k)
            {
                const double width = std::max(0.5, m.profileP75[k] - m.profileP25[k]);
                const double out = f[k] < bandLo[k] ? bandLo[k] - f[k] : (f[k] > bandHi[k] ? f[k] - bandHi[k] : 0.0);
                pen += kWeight[k] * (out / width) * (out / width) * 4.0;
            }
            return pen;
        }

        double score(const Spec& spec, const std::vector<Note>& ph)
        {
            if (ph.empty()) return -1e9;
            const Data& m = model(spec.archetype);
            double lp = 0.0, penalty = 0.0;
            int prevIv = 0;
            for (size_t i = 0; i < ph.size(); ++i)
            {
                const Note& n = ph[i];
                const int d = degreeOf(spec, n.pitch);
                const int dPrev = i > 0 ? degreeOf(spec, ph[i - 1].pitch) : d;
                lp += noteLogProb(spec, m, prevIv, d, dPrev, n.step, n.len, i + 1 == ph.size(), i == 0);
                prevIv = d - dPrev;
                if (n.pitch < spec.low || n.pitch > spec.high) penalty += 3.0;
                if (spec.forbidden && spec.forbidden(n.step, n.len, n.pitch)) penalty += 6.0;
            }
            return lp / (double) ph.size() - penalty - profilePenalty(spec, ph);
        }

        std::vector<Note> compose(const Spec& spec, std::mt19937& rng, int candidates, double* bestScore)
        {
            const Data& m = model(spec.archetype);
            std::vector<Note> best;
            double bestS = -1e18;
            const int centre = (spec.low + spec.high) / 2;

            for (int cand = 0; cand < candidates; ++cand)
            {
                // ---- rhythm plan (4-bar groups, restating bar 1 at the learned rates)
                std::vector<const MelodyModelData::BarRhythm*> rhythm((size_t) spec.bars);
                std::vector<int> copyPitchesFrom((size_t) spec.bars, -1);
                const bool repeatFirstHalf = spec.bars >= 8 && uniform(rng) < 0.19; // exact repeat of bars 1-4
                for (int b = 0; b < spec.bars; ++b)
                {
                    const int k = b % 4;
                    const bool phraseEnd = k == 3;
                    if (b >= 4 && repeatFirstHalf && b < spec.bars - 1)
                    {
                        rhythm[(size_t) b] = rhythm[(size_t) (b - 4)];
                        copyPitchesFrom[(size_t) b] = b - 4;
                        continue;
                    }
                    if (k > 0 && uniform(rng) < m.rhythmRepeat[k - 1] && !phraseEnd)
                    {
                        rhythm[(size_t) b] = rhythm[(size_t) (b - k)];
                        if (uniform(rng) < m.pitchRepeatGivenRhythm[k - 1] * 0.6) // leave room for "same rhythm, new notes"
                            copyPitchesFrom[(size_t) b] = b - k;
                        continue;
                    }
                    if (b >= 4 && k == 0 && uniform(rng) < 0.6)
                    {
                        rhythm[(size_t) b] = rhythm[0]; // the second half opens like the first
                        continue;
                    }
                    // Phrase endings land early in the bar and ring (the final note is extended below).
                    rhythm[(size_t) b] = phraseEnd ? &sampleRhythm(m, rng, 1, 4, 4) : &sampleRhythm(m, rng, 3, 8);
                }

                // ---- pitches
                std::vector<Note> ph;
                std::vector<int> barStartIndex((size_t) spec.bars, 0);
                int dPrev = degreeOf(spec, [&] {
                    // start on a strong chord tone near the register centre
                    int p = centre;
                    while (!isInScale(p - spec.rootNote, spec.scale)) ++p;
                    return p;
                }());
                int prevIv = 0;
                bool first = true;
                for (int b = 0; b < spec.bars; ++b)
                {
                    barStartIndex[(size_t) b] = (int) ph.size();
                    const MelodyModelData::BarRhythm& r = *rhythm[(size_t) b];
                    const int src = copyPitchesFrom[(size_t) b];
                    for (int i = 0; i < r.notes; ++i)
                    {
                        const int step = b * 16 + r.onset[i];
                        int len = r.len[i];
                        const bool lastOfPhrase = b == spec.bars - 1 && i == r.notes - 1;
                        if (lastOfPhrase) len = std::max(len, 16 - r.onset[i]); // let the final note ring to the bar end
                        len = std::min(len, spec.bars * 16 - step);
                        if (len <= 0) continue;

                        int d = -999;
                        if (src >= 0 && barStartIndex[(size_t) src] + i < (int) ph.size())
                        {
                            const int p = ph[(size_t) (barStartIndex[(size_t) src] + i)].pitch;
                            if (!(spec.forbidden && spec.forbidden(step, len, p))) d = degreeOf(spec, p);
                        }
                        if (d == -999)
                        {
                            // Sample the next degree from the model (temperature 0.85).
                            std::array<double, 15> w {};
                            double tot = 0.0;
                            for (int iv = -7; iv <= 7; ++iv)
                            {
                                const int cd = dPrev + iv;
                                const int p = pitchOf(spec, cd);
                                if (p < spec.low || p > spec.high) continue;
                                if (spec.forbidden && spec.forbidden(step, len, p)) continue;
                                const double lp = noteLogProb(spec, m, prevIv, cd, dPrev, step, len, lastOfPhrase, first);
                                w[(size_t) (iv + 7)] = std::exp(lp / 1.0);
                                tot += w[(size_t) (iv + 7)];
                            }
                            if (tot <= 0.0) { d = dPrev; }
                            else
                            {
                                double r2 = uniform(rng) * tot;
                                for (int iv = -7; iv <= 7; ++iv)
                                {
                                    r2 -= w[(size_t) (iv + 7)];
                                    if (r2 <= 0.0 && w[(size_t) (iv + 7)] > 0.0) { d = dPrev + iv; break; }
                                }
                                if (d == -999) d = dPrev;
                            }
                        }
                        ph.push_back({ step, len, pitchOf(spec, d) });
                        prevIv = first ? 0 : d - dPrev;
                        dPrev = d;
                        first = false;
                    }
                }
                // Trim overlaps (a tied note must end where the next begins).
                for (size_t i = 0; i + 1 < ph.size(); ++i)
                    ph[i].len = std::max(1, std::min(ph[i].len, ph[i + 1].step - ph[i].step));

                if (!isMusical(spec, ph)) continue;
                const double s = score(spec, ph);
                if (s > bestS)
                {
                    bestS = s;
                    best = ph;
                }
            }
            if (bestScore != nullptr) *bestScore = bestS;
            return best;
        }
    }
}
