#pragma once
#include <JuceHeader.h>
#include "DrumSampleIndex.h" // IndexedSample
#include "Engine/DrumSampleScoring.h" // Engine::scoreForRole
#include <vector>

// JUCE-side ranking/selection wrapper around Engine::scoreForRole and
// Engine::selectSampleIndex. Deliberately dumb about musical timing (no
// idea what a step, a bar, or a pattern is) and about sound analysis (no
// idea how a sample got measured) - it only combines "how good is this
// candidate for this role" (Engine::scoreForRole) with "which of the
// good ones, deterministically" (Engine::selectSampleIndex).
struct DrumSampleChoice
{
    juce::File file;            // invalid (File()) if no candidates existed for this role
    int        poolSize     = 0; // candidates considered for this role (post role-filter, pre-ranking)
    int        shortlistSize = 0; // size of the top-ranked shortlist actually sampled from
    float      score        = 0.0f; // the chosen sample's own score, for diagnostics/reporting
};

// Six functional roles - one choice per Engine::DrumRole value (see
// DrumVoiceSynth.h) - up from the earlier four-role (kick/clap/hat/perc)
// design. hatClosed/hatOpen are two DIFFERENT sample pools (a "good" closed
// hat is not necessarily right for the open-hat/ride accent role); percA
// and percB are two independently-selected candidates from the SAME PERC
// pool, deliberately forced apart (see selectDrumSamples) so two real
// library samples can play complementary motifs rather than one role
// being silently duplicated onto two voices.
struct DrumSampleSelection
{
    DrumSampleChoice kick, clap, hatClosed, hatOpen, percA, percB;
};

// indexed = whatever DrumSampleIndex last analyzed (PluginEditor's
// latestSampleIndex). Clap draws from the CLAP and SNARE racks combined -
// melodic techno productions use them close to interchangeably for this
// role. hatOpen draws from OPEN_HAT and RIDE combined (both are real,
// measured "open, ringing, longer-decay" material - see
// MLPipeline/drum_grammar/analyze_drum_grammar.py's ONESHOT_CORPUS).
// Kick/hatClosed/percA/percB each draw from their own single rack only,
// never borrowing from an unrelated category. bpm is passed straight to
// Engine::scoreForRole (Kick only actually uses it - see
// Engine/DrumSampleScoring.h).
DrumSampleSelection selectDrumSamples(const std::vector<IndexedSample>& indexed, uint32_t seed, double bpm);
