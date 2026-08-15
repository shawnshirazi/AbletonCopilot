#pragma once
#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include "../MelodyCategory.h"

// The real, research-informed melodic-techno motif generator - extracted
// from MelodyGridComponent::generateMelodyFromMotifShapes so it has a
// single owner instead of being reachable only through the manual-editing
// grid component. Five named motif shapes (offbeat pulse, rolling 16ths,
// walking octaves, kick interplay, sparse hypnotic - see
// MelodyMotifGenerator.cpp for the real house/techno idiom each is built
// from), a per-pass chord-progression arc, register/scale-constrained
// output, and candidate ranking via MelodyScorer::scoreTrack
// (motifEconomy/contourDiscipline/pitchClassRestraint/phraseResolution) -
// unchanged from the original, just no longer tied to a live grid track.
//
// generateMelodyFromMotifShapes is now a thin wrapper around this that
// writes the result into a track's offsets array - both the manual-editing
// UI and the loop-generator workflow (Source/Engine/MusicIdentity.h's
// JUCE-side melody counterpart) call this ONE function rather than two
// independent random generators (per the "don't build a second generation
// system" rule the loop-generator work was built under).
namespace MelodyMotifGenerator
{
    // keyRootSemitone is accepted for interface symmetry with the rest of
    // this codebase's generation calls but, like the original function,
    // unused - the output is root-relative; playback adds the key root
    // itself (see MelodyGridComponent/PluginProcessor's melody-voice
    // triggering). seed makes this fully deterministic (the original
    // used a default-constructed juce::Random, i.e. system-entropy-seeded
    // and NOT reproducible - this is the one real behavioural change from
    // the extraction, needed for "same seed -> same loop every time").
    std::array<int8_t, 128> generateMelodyMotif(int keyRootSemitone, bool isMinor,
                                                 MelodyCategory category, MelodyStyle style,
                                                 uint32_t seed);
}
