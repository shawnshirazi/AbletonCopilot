#pragma once
#include <JuceHeader.h>
#include <array>
#include "../MelodyCategory.h"

// How idiomatic a candidate pattern is, as plain arithmetic over the note
// data - no audio, no ML, no subjective judgment call. This is the
// "constrain, then rank the legal space" half of the approach: register and
// scale are enforced as hard constraints elsewhere (MelodyCritic,
// generation); this scores what's left using real, well-established
// melody-writing heuristics, so a generator can pick the best of several
// already-legal candidates instead of the first one found.
struct MelodyScore
{
    float total               = 0.0f; // weighted combination, higher = more idiomatic
    float motifEconomy        = 0.0f; // how much of the pattern echoes the opening 2-bar idea
    float contourDiscipline   = 0.0f; // mostly stepwise motion, leaps recovered by opposite-direction steps
    float pitchClassRestraint = 0.0f; // fewer distinct pitch classes per phrase reads as more intentional
    float phraseResolution    = 0.0f; // fraction of phrases ending on root/fifth
};

namespace MelodyScorer
{
    MelodyScore scoreTrack(const std::array<int8_t, 128>& offsets, MelodyCategory category,
                            int keyRootSemitone, bool isMinor);
}
