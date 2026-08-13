#pragma once

// *** DEAD CODE - DO NOT USE ***
// Every AudioFeatures value in this file (LUFS, BPM, key, spectral %) is
// HAND-INVENTED / FABRICATED - not a real measurement of the named songs.
// This was an early, abandoned approach; it is NOT registered in
// AbletonCopilot.jucer and is never compiled into the plugin. Kept in the
// repo only for history. Never wire this in or treat its numbers as real
// data about the named tracks/artists - see the real reference-track
// pipeline instead (Source/Analysis/ReferenceTrackAnalyzer.h,
// Source/Analysis/ReferenceFragmentJob.h), which analyzes actual
// user-supplied audio.

#include <JuceHeader.h>
#include <vector>
#include "FeatureExtractor.h"

struct ReferenceTrack
{
    juce::String  id;
    juce::String  artist;
    juce::String  title;
    juce::String  year;
    juce::String  genreId;
    AudioFeatures features;
};

class ReferenceTrackDB
{
public:
    static const ReferenceTrackDB& getInstance();

    const ReferenceTrack*              getTrack(const juce::String& id) const;
    std::vector<const ReferenceTrack*> getTracksForGenre(const juce::String& genreId) const;

private:
    ReferenceTrackDB();
    std::vector<ReferenceTrack> tracks;
};
