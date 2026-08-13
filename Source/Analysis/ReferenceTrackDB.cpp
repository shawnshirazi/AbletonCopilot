// *** DEAD CODE - all AudioFeatures values below are FABRICATED, not real
// measurements. See the warning in ReferenceTrackDB.h before reading further. ***
#include "ReferenceTrackDB.h"

const ReferenceTrackDB& ReferenceTrackDB::getInstance()
{
    static ReferenceTrackDB instance;
    return instance;
}

static ReferenceTrack makeTrack(const char* id, const char* artist, const char* title,
                                 const char* year, const char* genreId,
                                 float lufs, float peakDb, float bpm, const char* key,
                                 float subPct, float lowPct, float lowMidPct,
                                 float highMidPct, float airPct, float stereoWidth)
{
    ReferenceTrack t;
    t.id      = id;
    t.artist  = artist;
    t.title   = title;
    t.year    = year;
    t.genreId = genreId;
    t.features.lufs           = lufs;
    t.features.peakDb         = peakDb;
    t.features.dynamicRangeDb = peakDb - lufs;
    t.features.bpm            = bpm;
    t.features.key            = key;
    t.features.subPct         = subPct;
    t.features.lowPct         = lowPct;
    t.features.lowMidPct      = lowMidPct;
    t.features.highMidPct     = highMidPct;
    t.features.airPct         = airPct;
    t.features.stereoWidth    = stereoWidth;
    t.features.valid          = true;
    return t;
}

ReferenceTrackDB::ReferenceTrackDB()
{
    // Melodic Techno
    tracks.push_back(makeTrack(
        "anyma_running", "Anyma", "Running", "2022", "melodic-techno",
        -9.0f, -0.3f, 128.0f, "A minor",
        0.22f, 0.24f, 0.32f, 0.15f, 0.07f, 0.62f));
    tracks.push_back(makeTrack(
        "benbohmer_breathing", "Ben Bohmer", "Breathing", "2019", "melodic-techno",
        -10.5f, -0.5f, 124.0f, "F minor",
        0.18f, 0.22f, 0.35f, 0.18f, 0.07f, 0.65f));
    tracks.push_back(makeTrack(
        "taleofus_saturn", "Tale Of Us", "Saturn", "2014", "melodic-techno",
        -9.5f, -0.3f, 128.0f, "D minor",
        0.20f, 0.23f, 0.33f, 0.17f, 0.07f, 0.60f));

    // Tech House
    tracks.push_back(makeTrack(
        "fisher_losingit", "Fisher", "Losing It", "2018", "tech-house",
        -8.0f, -0.2f, 129.0f, "G minor",
        0.19f, 0.25f, 0.35f, 0.15f, 0.06f, 0.55f));
    tracks.push_back(makeTrack(
        "chrislake_operator", "Chris Lake", "Operator", "2021", "tech-house",
        -8.5f, -0.3f, 128.0f, "A minor",
        0.18f, 0.24f, 0.36f, 0.16f, 0.06f, 0.52f));
    tracks.push_back(makeTrack(
        "johnsummit_ladanza", "John Summit", "La Danza", "2022", "tech-house",
        -8.0f, -0.2f, 128.0f, "C minor",
        0.20f, 0.26f, 0.33f, 0.15f, 0.06f, 0.53f));

    // Deep House
    tracks.push_back(makeTrack(
        "bicep_glue", "Bicep", "Glue", "2017", "deep-house",
        -10.0f, -0.5f, 122.0f, "A minor",
        0.16f, 0.26f, 0.38f, 0.15f, 0.05f, 0.58f));
    tracks.push_back(makeTrack(
        "mjcoles_whattheysay", "Maya Jane Coles", "What They Say", "2012", "deep-house",
        -11.0f, -0.8f, 120.0f, "D minor",
        0.14f, 0.28f, 0.40f, 0.13f, 0.05f, 0.55f));
    tracks.push_back(makeTrack(
        "disclosure_latch", "Disclosure", "Latch", "2012", "deep-house",
        -9.5f, -0.4f, 120.0f, "G major",
        0.17f, 0.25f, 0.37f, 0.15f, 0.06f, 0.60f));

    // Minimal Techno
    tracks.push_back(makeTrack(
        "plastikman_spastik", "Plastikman", "Spastik", "1993", "minimal-techno",
        -9.5f, -0.3f, 130.0f, "A minor",
        0.24f, 0.22f, 0.30f, 0.17f, 0.07f, 0.48f));
    tracks.push_back(makeTrack(
        "villalobos_fizheuer", "Ricardo Villalobos", "Fizheuer Zieheuer", "2006", "minimal-techno",
        -10.0f, -0.5f, 130.0f, "C minor",
        0.22f, 0.21f, 0.32f, 0.18f, 0.07f, 0.45f));
    tracks.push_back(makeTrack(
        "rhood_internal", "Robert Hood", "Internal Empire", "1994", "minimal-techno",
        -9.0f, -0.2f, 135.0f, "A minor",
        0.25f, 0.22f, 0.29f, 0.17f, 0.07f, 0.46f));

    // Afro House
    tracks.push_back(makeTrack(
        "blackcoffee_youneedme", "Black Coffee", "You Need Me", "2010", "afro-house",
        -9.5f, -0.4f, 122.0f, "G minor",
        0.18f, 0.24f, 0.36f, 0.16f, 0.06f, 0.55f));
    tracks.push_back(makeTrack(
        "enoonapa_virus", "Enoo Napa", "Virus", "2018", "afro-house",
        -10.0f, -0.5f, 124.0f, "D minor",
        0.17f, 0.23f, 0.37f, 0.17f, 0.06f, 0.57f));
    tracks.push_back(makeTrack(
        "themba_khaya", "Themba", "Khaya", "2019", "afro-house",
        -9.5f, -0.4f, 123.0f, "A minor",
        0.18f, 0.24f, 0.36f, 0.16f, 0.06f, 0.56f));

    // Progressive House
    tracks.push_back(makeTrack(
        "prydz_pjanoo", "Eric Prydz", "Pjanoo", "2008", "progressive-house",
        -9.0f, -0.3f, 128.0f, "D minor",
        0.19f, 0.23f, 0.35f, 0.17f, 0.06f, 0.60f));
    tracks.push_back(makeTrack(
        "deadmau5_strobe", "deadmau5", "Strobe", "2009", "progressive-house",
        -9.5f, -0.5f, 128.0f, "E major",
        0.17f, 0.22f, 0.36f, 0.18f, 0.07f, 0.63f));
    tracks.push_back(makeTrack(
        "ab_sunandmoon", "Above & Beyond", "Sun & Moon", "2011", "progressive-house",
        -9.0f, -0.3f, 128.0f, "B major",
        0.18f, 0.22f, 0.35f, 0.18f, 0.07f, 0.62f));
}

const ReferenceTrack* ReferenceTrackDB::getTrack(const juce::String& id) const
{
    for (const auto& t : tracks)
        if (t.id == id) return &t;
    return nullptr;
}

std::vector<const ReferenceTrack*> ReferenceTrackDB::getTracksForGenre(
    const juce::String& genreId) const
{
    std::vector<const ReferenceTrack*> result;
    for (const auto& t : tracks)
        if (t.genreId == genreId) result.push_back(&t);
    return result;
}
