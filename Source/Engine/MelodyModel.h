#pragma once

#include "Theory.h"
#include <cstdint>
#include <functional>
#include <random>
#include <vector>

// Data-driven melody composer. The statistics in MelodyModelData.h were
// learned from real, well-loved instrumental lead melodies (melodic /
// progressive house, techno and trance - see Tools/melody_model/ and
// MLPipeline/musical_target/melody_corpus_stats.md): which interval follows
// which, which note sits over the chord on strong vs weak positions, how
// phrases end, real one-bar rhythms, and how often bar 3 restates bar 1's
// rhythm (and its pitches). compose() samples many candidate phrases from
// that model under the caller's harmony and hard constraints, and keeps the
// one the model finds most typical of the corpus - with taste checks so the
// winner is never a degenerate one-note line. Zero JUCE dependency.
namespace Engine
{
    namespace MelodyModel
    {
        enum class Archetype { Line, Arp };

        struct Note
        {
            int step;  // 16ths from the phrase start
            int len;   // 16ths
            int pitch; // MIDI
        };

        struct Spec
        {
            Archetype        archetype = Archetype::Line;
            int              bars      = 8;
            int              rootNote  = 9;
            ScaleType        scale     = ScaleType::Aeolian;
            std::vector<int> chordDegreeByBar; // key scale degree of the chord root, one per bar
            int              low = 60, high = 79; // register (MIDI, inclusive)
            bool             endHome = true;      // last note on the tonic or the key's 5th
            // Hard constraint from the arrangement (e.g. a sustained semitone
            // against a pad/pedal note). Null = no constraint.
            std::function<bool(int step, int len, int pitch)> forbidden;
        };

        // Mean per-note log-likelihood of `phrase` under the learned model
        // (interval transitions + chord-relative position + ending), minus
        // penalties for broken constraints. Higher = more like the corpus.
        double score(const Spec& spec, const std::vector<Note>& phrase);

        // True if the phrase passes the taste checks: enough distinct
        // pitches, a real range, not dominated by repeated notes.
        bool isMusical(const Spec& spec, const std::vector<Note>& phrase);

        // Best of `candidates` sampled phrases (deterministic for a given rng state).
        std::vector<Note> compose(const Spec& spec, std::mt19937& rng, int candidates = 400, double* bestScore = nullptr);
    }
}
