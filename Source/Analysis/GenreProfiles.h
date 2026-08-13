#pragma once
#include <JuceHeader.h>
#include <vector>
#include <optional>
#include "FeatureExtractor.h" // AudioFeatures - buildProfileFromReference()

struct GenreProfile
{
    juce::String id;
    juce::String displayName;

    // Loudness (LUFS)
    float lufsTarget    = -9.0f;
    float lufsMin       = -11.0f;
    float lufsMax       = -7.0f;

    // Tempo
    float bpmMin        = 120.0f;
    float bpmMax        = 135.0f;

    // Spectral balance targets (proportions 0–1)
    float subPctTarget      = 0.20f;
    float lowPctTarget      = 0.22f;
    float lowMidPctTarget   = 0.34f;
    float highMidPctTarget  = 0.17f;
    float airPctTarget      = 0.07f;

    // Tolerance on spectral bands (absolute difference before flagging)
    float spectralTolerance = 0.07f;

    // Stereo
    float stereoWidthMin    = 0.50f;

    // Dynamics
    float dynamicRangeMin   = 3.0f;
    float dynamicRangeMax   = 12.0f;

    juce::String keyPreference;    // "minor", "major", "any"
    juce::String arrangementNote;  // shown as context in feedback panel
};

// Reserved id for the ad-hoc profile built from a user-loaded reference
// track (see buildProfileFromReference below) - not one of the hand-picked
// genre archetypes, so it's kept out of getIds()/getDisplayNames() and only
// resolves through getProfile() once a reference has actually been loaded.
static const juce::String kReferenceProfileId = "reference_track";

class GenreProfiles
{
public:
    // Non-const so setReferenceProfile() can be called on the singleton -
    // every existing caller only ever calls the const methods below, so
    // this is a non-breaking signature widening, not a behavior change.
    static GenreProfiles& getInstance();

    const GenreProfile* getProfile(const juce::String& id) const;
    juce::StringArray   getIds() const;
    juce::StringArray   getDisplayNames() const;

    // Reference-track support: set/clear the ad-hoc profile resolved under
    // kReferenceProfileId. Not part of the hardcoded `profiles` list.
    void setReferenceProfile(std::optional<GenreProfile> profile) { referenceProfile = std::move(profile); }
    bool hasReferenceProfile() const noexcept { return referenceProfile.has_value(); }

private:
    GenreProfiles();
    std::vector<GenreProfile>   profiles;
    std::optional<GenreProfile> referenceProfile;
};

// Builds a synthetic GenreProfile whose targets ARE the reference track's
// actually-measured features (not an archetype guess) - tight tolerance
// bands around each measured value, so the existing FeedbackEngine/
// computeCorrections comparison code can target a real song, unmodified.
GenreProfile buildProfileFromReference(const juce::String& displayName, const AudioFeatures& f);
