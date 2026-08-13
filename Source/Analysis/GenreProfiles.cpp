#include "GenreProfiles.h"

GenreProfiles& GenreProfiles::getInstance()
{
    static GenreProfiles instance;
    return instance;
}

GenreProfiles::GenreProfiles()
{
    // Melodic Techno
    {
        GenreProfile p;
        p.id          = "melodic_techno";
        p.displayName = "Melodic Techno";
        p.lufsTarget  = -9.5f;
        p.lufsMin     = -11.5f; p.lufsMax = -7.5f;
        p.bpmMin      = 124.0f; p.bpmMax  = 132.0f;
        p.subPctTarget     = 0.20f;
        p.lowPctTarget     = 0.20f;
        p.lowMidPctTarget  = 0.34f;
        p.highMidPctTarget = 0.18f;
        p.airPctTarget     = 0.08f;
        p.stereoWidthMin   = 0.62f;
        p.dynamicRangeMin  = 4.0f; p.dynamicRangeMax = 9.0f;
        p.keyPreference    = "minor";
        p.arrangementNote  = "Long intro (2-4 min), cinematic build, filter sweep into drop. "
                             "Heavy reverb/delay on synths. Pads and leads are essential - "
                             "if the midrange feels empty, add a pad layer.";
        profiles.push_back(p);
    }

    // Tech House
    {
        GenreProfile p;
        p.id          = "tech_house";
        p.displayName = "Tech House";
        p.lufsTarget  = -8.0f;
        p.lufsMin     = -9.5f; p.lufsMax = -6.5f;
        p.bpmMin      = 126.0f; p.bpmMax = 132.0f;
        p.subPctTarget     = 0.22f;
        p.lowPctTarget     = 0.22f;
        p.lowMidPctTarget  = 0.33f;
        p.highMidPctTarget = 0.16f;
        p.airPctTarget     = 0.07f;
        p.stereoWidthMin   = 0.48f;
        p.dynamicRangeMin  = 3.0f; p.dynamicRangeMax = 7.0f;
        p.keyPreference    = "any";
        p.arrangementNote  = "Groove-forward and punchy. Short intro (~1 min), heavy sub bass, "
                             "vocal stabs and FX loops are common. Kick and bass should be "
                             "tightly locked - sidechain compression is standard.";
        profiles.push_back(p);
    }

    // Deep House
    {
        GenreProfile p;
        p.id          = "deep_house";
        p.displayName = "Deep House";
        p.lufsTarget  = -11.0f;
        p.lufsMin     = -13.0f; p.lufsMax = -9.0f;
        p.bpmMin      = 118.0f; p.bpmMax  = 126.0f;
        p.subPctTarget     = 0.18f;
        p.lowPctTarget     = 0.24f;
        p.lowMidPctTarget  = 0.36f;
        p.highMidPctTarget = 0.15f;
        p.airPctTarget     = 0.07f;
        p.stereoWidthMin   = 0.42f;
        p.dynamicRangeMin  = 5.0f; p.dynamicRangeMax = 13.0f;
        p.keyPreference    = "any";
        p.arrangementNote  = "Warm, soulful, breathing. Sustained chords, organic textures, "
                             "subtle percussion. Dynamics matter - don't over-compress. "
                             "Rhodes, jazzy chords and filtered bass lines define the genre.";
        profiles.push_back(p);
    }

    // Minimal Techno
    {
        GenreProfile p;
        p.id          = "minimal_techno";
        p.displayName = "Minimal Techno";
        p.lufsTarget  = -9.0f;
        p.lufsMin     = -11.0f; p.lufsMax = -7.0f;
        p.bpmMin      = 128.0f; p.bpmMax  = 138.0f;
        p.subPctTarget     = 0.18f;
        p.lowPctTarget     = 0.22f;
        p.lowMidPctTarget  = 0.37f;
        p.highMidPctTarget = 0.17f;
        p.airPctTarget     = 0.06f;
        p.stereoWidthMin   = 0.38f;
        p.dynamicRangeMin  = 4.0f; p.dynamicRangeMax = 8.0f;
        p.keyPreference    = "minor";
        p.arrangementNote  = "Repetitive, hypnotic, sparse. Elements evolve slowly over time. "
                             "Focus on rhythm and texture rather than melody. "
                             "Stereo field is often intentionally narrow.";
        profiles.push_back(p);
    }

    // Afro House
    {
        GenreProfile p;
        p.id          = "afro_house";
        p.displayName = "Afro House";
        p.lufsTarget  = -9.5f;
        p.lufsMin     = -11.0f; p.lufsMax = -7.5f;
        p.bpmMin      = 120.0f; p.bpmMax  = 128.0f;
        p.subPctTarget     = 0.20f;
        p.lowPctTarget     = 0.22f;
        p.lowMidPctTarget  = 0.35f;
        p.highMidPctTarget = 0.16f;
        p.airPctTarget     = 0.07f;
        p.stereoWidthMin   = 0.55f;
        p.dynamicRangeMin  = 4.0f; p.dynamicRangeMax = 9.0f;
        p.keyPreference    = "any";
        p.arrangementNote  = "Percussive richness is the defining quality. Layered congas, "
                             "shakers, and talking drums. Vocal samples (chants, phrases) common. "
                             "Warm organic bass - avoid overly digital sounds.";
        profiles.push_back(p);
    }

    // Progressive House
    {
        GenreProfile p;
        p.id          = "progressive_house";
        p.displayName = "Progressive House";
        p.lufsTarget  = -9.0f;
        p.lufsMin     = -11.0f; p.lufsMax = -7.0f;
        p.bpmMin      = 126.0f; p.bpmMax  = 132.0f;
        p.subPctTarget     = 0.20f;
        p.lowPctTarget     = 0.20f;
        p.lowMidPctTarget  = 0.33f;
        p.highMidPctTarget = 0.19f;
        p.airPctTarget     = 0.08f;
        p.stereoWidthMin   = 0.60f;
        p.dynamicRangeMin  = 4.0f; p.dynamicRangeMax = 9.0f;
        p.keyPreference    = "minor";
        p.arrangementNote  = "Journey-focused. Long builds with filter sweeps, big contrast "
                             "between breakdown and drop. Melodic leads are essential. "
                             "High-end presence (air) should be generous for the trance-adjacent sound.";
        profiles.push_back(p);
    }
}

const GenreProfile* GenreProfiles::getProfile(const juce::String& id) const
{
    if (id == kReferenceProfileId)
        return referenceProfile.has_value() ? &(*referenceProfile) : nullptr;

    for (auto& p : profiles)
        if (p.id == id) return &p;
    return nullptr;
}

juce::StringArray GenreProfiles::getIds() const
{
    juce::StringArray ids;
    for (auto& p : profiles) ids.add(p.id);
    return ids;
}

juce::StringArray GenreProfiles::getDisplayNames() const
{
    juce::StringArray names;
    for (auto& p : profiles) names.add(p.displayName);
    return names;
}

GenreProfile buildProfileFromReference(const juce::String& displayName, const AudioFeatures& f)
{
    GenreProfile p;
    p.id          = kReferenceProfileId;
    p.displayName = "Reference: " + displayName;

    p.lufsTarget = f.lufs;
    p.lufsMin    = f.lufs - 1.0f;
    p.lufsMax    = f.lufs + 1.0f;

    // computeBpm() can fail to find a confident peak on some material -
    // fall back to a generic melodic-techno-range midpoint rather than a
    // bpmMin/bpmMax window of zero.
    const float bpm = f.bpm > 0.0f ? f.bpm : 126.0f;
    p.bpmMin = bpm - 3.0f;
    p.bpmMax = bpm + 3.0f;

    p.subPctTarget      = f.subPct;
    p.lowPctTarget       = f.lowPct;
    p.lowMidPctTarget    = f.lowMidPct;
    p.highMidPctTarget   = f.highMidPct;
    p.airPctTarget        = f.airPct;
    p.spectralTolerance  = 0.03f; // tighter than the archetype profiles - this is a real measurement, not a guess

    p.stereoWidthMin = f.stereoWidth * 0.85f;

    p.dynamicRangeMin = juce::jmax(1.0f, f.dynamicRangeDb - 2.0f);
    p.dynamicRangeMax = f.dynamicRangeDb + 3.0f;

    p.keyPreference = f.key.endsWithIgnoreCase("minor") ? "minor"
                     : f.key.endsWithIgnoreCase("major") ? "major" : "any";

    p.arrangementNote = "Targets are measured directly from \"" + displayName +
                         "\" - not a genre archetype guess.";
    return p;
}
