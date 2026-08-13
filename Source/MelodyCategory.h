#pragma once
#include <JuceHeader.h>
#include "UIStyle.h"

// Shared vocabulary for prompt-driven melody generation — used by both
// PluginEditor (preset filtering + prompt parsing) and DrumMachineComponent
// (pattern generation), so "what counts as a bass/lead/pad preset" and
// "what does 'atmospheric' mean" each live in exactly one place.
enum class MelodyCategory { Bass, Lead, Pad, Pluck, Synth, Fx, Other };
enum class MelodyStyle    { Default, Rolling, Atmospheric };

// Scans the prompt text for a category keyword. Defaults to Bass — the only
// category with a real transcribed-fragment library (see
// MLPipeline/extract_fragments.py), so an unrecognised/empty prompt still
// produces something musically solid rather than a shrug.
inline MelodyCategory parseCategoryFromPrompt(const juce::String& prompt)
{
    auto p = prompt.toLowerCase();
    if (p.contains("lead"))                                                  return MelodyCategory::Lead;
    if (p.contains("pad") || p.contains("atmos") || p.contains("ambient"))    return MelodyCategory::Pad;
    if (p.contains("pluck"))                                                 return MelodyCategory::Pluck;
    if (p.contains("synth"))                                                 return MelodyCategory::Synth;
    if (p.contains("fx") || p.contains("effect"))                            return MelodyCategory::Fx;
    if (p.contains("bass"))                                                  return MelodyCategory::Bass;
    return MelodyCategory::Bass;
}

// Scans the prompt text for a rhythmic/textural style hint.
inline MelodyStyle parseStyleFromPrompt(const juce::String& prompt)
{
    auto p = prompt.toLowerCase();
    if (p.contains("atmospheric") || p.contains("ambient") || p.contains("sparse")
        || p.contains("slow") || p.contains("sustained") || p.contains("pad"))
        return MelodyStyle::Atmospheric;
    if (p.contains("rolling") || p.contains("driving") || p.contains("groove")
        || p.contains("moving") || p.contains("energetic"))
        return MelodyStyle::Rolling;
    return MelodyStyle::Default;
}

inline juce::String categoryDisplayName(MelodyCategory category)
{
    switch (category)
    {
        case MelodyCategory::Bass:  return "BASS";
        case MelodyCategory::Lead:  return "LEAD";
        case MelodyCategory::Pad:   return "PAD";
        case MelodyCategory::Pluck: return "PLUCK";
        case MelodyCategory::Synth: return "SYNTH";
        case MelodyCategory::Fx:    return "FX";
        default:                    return "MELODY";
    }
}

inline juce::Colour categoryColour(MelodyCategory category)
{
    switch (category)
    {
        case MelodyCategory::Bass:  return UIStyle::kBass;
        case MelodyCategory::Lead:  return UIStyle::kLead;
        case MelodyCategory::Pad:   return UIStyle::kPad;
        case MelodyCategory::Pluck: return UIStyle::kPluck;
        case MelodyCategory::Synth: return UIStyle::kSynthCat;
        case MelodyCategory::Fx:    return UIStyle::kFx;
        default:                    return UIStyle::kMisc;
    }
}
