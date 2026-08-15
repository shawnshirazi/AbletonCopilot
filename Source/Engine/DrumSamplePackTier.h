#pragma once

#include <string>

// Genre-aware source/pack confidence classifier - Part C of the bass-
// runtime-bug + sample-selection investigation. RackClassification.h
// already answers "what INSTRUMENT is this sample" (kick/clap/hat/perc);
// this file answers a completely different question: "how confident are
// we that the PACK this sample came from is actually Melodic Techno
// material, versus a different genre the same library happens to
// contain (Tech House, Trance, Future Rave, generic FL Studio factory
// content, ...)". DrumSampleScoring.h's Gaussian fit already judges a
// sample's measured DSP character against the genre's fingerprint, but
// two samples can measure identically on duration/attack/ZCR/etc. while
// one is a real melodic-techno-branded hat and the other is a Tech House
// hat that just happens to share those numbers - source identity is
// real information the DSP features alone can't see.
//
// Zero JUCE dependency (same reasoning as Theory.h/DrumSampleScoring.h):
// works on a plain path string, testable in the zero-JUCE Engine/tests/
// harness. The JUCE layer (Source/DrumSampleSelector.cpp) is the only
// caller that has a real juce::File - it passes file.getFullPathName().
namespace Engine
{
    // Ordered exactly as the user's own stated priority: 1) explicit
    // Melodic Techno packs, 2) adjacent Techno/Progressive Techno packs,
    // 3) everything else electronic/unclassified (the neutral default -
    // most of the library's non-genre-branded or not-yet-inventoried
    // packs land here, not penalized, not favoured), 4) explicit
    // off-genre fallback material (Tech House and similar - only ever
    // preferred if nothing better exists for a role).
    enum class PackTier
    {
        MelodicTechno,   // Tier 1 - e.g. PML Mirage/Mystique, Odd Frequency Exo, Ekko Mirage
        AdjacentTechno,  // Tier 2 - e.g. Odd Frequency Grid, Afterhours, Progressive Techno Cr2
        OtherElectronic, // Tier 3 - default/unknown packs, plus explicitly-inventoried driving/dark techno
        OffGenreFallback // Tier 4 - Tech House and other confirmed off-genre packs
    };

    struct PackClassification
    {
        PackTier    tier = PackTier::OtherElectronic;
        std::string packName; // best-guess human label for diagnostics; empty if unmatched/default
    };

    // Multiplicative confidence weight applied on top of a sample's own
    // measured-character score (Engine::scoreForRole) - see
    // DrumSampleSelector.cpp's pick(). Tier1/2 are a real but modest
    // boost (this is a tie-breaker among already-plausible candidates,
    // not a replacement for DSP fit - a badly-measured Tier1 sample can
    // still lose to a well-measured Tier3 one). Tier4 is a much heavier
    // penalty, matching "generic Tech House material only as a
    // fallback": it should only win a role if literally nothing else
    // scored above zero.
    constexpr float kMelodicTechnoWeight   = 1.20f;
    constexpr float kAdjacentTechnoWeight  = 1.08f;
    constexpr float kOtherElectronicWeight = 1.00f;
    constexpr float kOffGenreFallbackWeight = 0.55f;

    inline float tierWeight(PackTier tier)
    {
        switch (tier)
        {
            case PackTier::MelodicTechno:    return kMelodicTechnoWeight;
            case PackTier::AdjacentTechno:   return kAdjacentTechnoWeight;
            case PackTier::OtherElectronic:  return kOtherElectronicWeight;
            case PackTier::OffGenreFallback: return kOffGenreFallbackWeight;
        }
        return kOtherElectronicWeight;
    }

    // absolutePath: the sample's full filesystem path (any OS separator -
    // tokenization splits on every non-alphanumeric character, not just
    // '/', so it works on the path as one string with no OS-specific
    // parsing). Classification is a whole-path token match, not a fixed
    // directory depth - real packs nest their instrument subfolders at
    // varying depths under the pack's own name (see e.g. "Odd Frequency -
    // Modern Melodic Techno Mega Bundle/Odd Frequency - Exo - Full
    // Bundle/..."), so this deliberately looks at every path segment
    // rather than "the parent" or "the grandparent" the way
    // RackClassification's instrument classifier does.
    //
    // The tier-1/2/4 signatures below are drawn directly from the actual
    // pack folder names inventoried in
    // MLPipeline/musical_target/melodic_techno_research.md section 9
    // (confirmed against the real library on disk) - this is not a
    // single arbitrary keyword per pack; most rules require two or more
    // co-occurring tokens specifically so an unrelated file can't
    // accidentally trip a pack signature by sharing one common word.
    // Any pack NOT in that inventory (including every not-yet-reviewed
    // folder in the library) falls through to Tier 3 (OtherElectronic) -
    // a neutral default, not a penalty - per the user's explicit
    // instruction not to hardcode only a handful of filenames and not to
    // treat "unclassified" as "off-genre".
    PackClassification classifyPackTier(const std::string& absolutePath);
}
