#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
#include <optional>
#include "../MelodyCategory.h"
#include "MelodicTechnoTheory.h" // RegisterRange/kCategoryRegister

// Pure symbolic critique — reads a melody track's actual note data (the
// same std::array<int8_t,128> offsets MelodyGridComponent owns) and the
// current key, and flags mechanically-checkable issues against
// MelodicTechnoTheory, plus a couple of additive "make it better" moves
// (fill a long gap with a passing tone, propose layering a harmony track).
// No audio, no UI. Deliberately limited to concrete, describable rules
// rather than claiming any broader sense of "good melody" — that's not
// something a rule-based pass over note data can honestly judge.
struct MelodyEditSuggestion
{
    int          trackIndex = -1;
    juce::String trackLabel;   // badge, e.g. "BASS" — for display only
    int          step        = -1;
    int8_t       oldOffset   = 0;
    int8_t       newOffset   = 0;
    juce::String reason;
};

// A different action shape from MelodyEditSuggestion - creates a whole new
// track rather than editing one step, so it's kept separate rather than
// forced into the same struct.
struct LayeringSuggestion
{
    int            sourceTrackIndex = -1;
    juce::String   sourceLabel;
    int            transposeSemitones = 7;
    MelodyCategory suggestedCategory  = MelodyCategory::Pluck;
    juce::String   reason;
};

class MelodyCritic
{
public:
    std::vector<MelodyEditSuggestion> critique(int trackIndex, const juce::String& trackLabel,
                                                const std::array<int8_t, 128>& offsets,
                                                int keyRootSemitone, bool isMinor,
                                                MelodyCategory category) const;

    // Only returns a suggestion when the source track has enough active
    // content to be worth doubling and there's room for another track
    // (numExistingTracks < maxTracks) - silently declines otherwise rather
    // than proposing something that can't actually be applied.
    std::optional<LayeringSuggestion> suggestLayering(int trackIndex, const juce::String& trackLabel,
                                                        MelodyCategory sourceCategory,
                                                        const std::array<int8_t, 128>& offsets,
                                                        int numExistingTracks, int maxTracks) const;

    // Nearest in-scale semitone to offset (Aeolian/Dorian for minor keys -
    // Dorian is equally legitimate in melodic techno; Ionian for major),
    // searching outward so the result stays as close to offset as possible.
    // Exposed so generation (MelodyGridComponent) can guarantee real
    // transcribed fragments respect the selected key, not just critique
    // them after the fact.
    static int8_t nearestInScaleTone(int8_t offset, bool isMinor);

private:
    void checkScaleConformance(int trackIndex, const juce::String& trackLabel,
                                const std::array<int8_t, 128>& offsets,
                                int keyRootSemitone, bool isMinor,
                                std::vector<MelodyEditSuggestion>& out) const;

    void checkPhraseResolution(int trackIndex, const juce::String& trackLabel,
                                const std::array<int8_t, 128>& offsets,
                                int keyRootSemitone,
                                std::vector<MelodyEditSuggestion>& out) const;

    // "Ear candy": within each 2-bar phrase, a long empty stretch between
    // two active notes gets a proposed passing-tone fill rather than being
    // left flat - reuses the same edit-suggestion shape (oldOffset = the
    // empty-step sentinel), no new plumbing needed to apply it.
    void checkEarCandyGaps(int trackIndex, const juce::String& trackLabel,
                            const std::array<int8_t, 128>& offsets,
                            std::vector<MelodyEditSuggestion>& out) const;

    // Flags any active note outside MelodicTechnoTheory::kCategoryRegister
    // for this track's category, proposes octave-shifting it back into range.
    void checkRegisterViolations(int trackIndex, const juce::String& trackLabel,
                                  const std::array<int8_t, 128>& offsets, MelodyCategory category,
                                  std::vector<MelodyEditSuggestion>& out) const;

    // Per phrase, if more distinct pitch classes are used than reads as
    // intentional, flags the least-common outlier as a candidate to
    // consolidate toward a dominant pitch class already established there.
    void checkPitchClassSprawl(int trackIndex, const juce::String& trackLabel,
                                const std::array<int8_t, 128>& offsets,
                                std::vector<MelodyEditSuggestion>& out) const;

    // Sourced technique: a bassline that repeats identically every 2 bars
    // for the whole 8-bar loop reads as static rather than hypnotic - real
    // practice is to nudge it subtly every 8-16 bars (swap a root hit for
    // the fifth, or shift one note's step by one). Flags phrase 3/4 if
    // they're an exact copy of phrase 1 and proposes one concrete nudge.
    void checkStaticRepetition(int trackIndex, const juce::String& trackLabel,
                                const std::array<int8_t, 128>& offsets,
                                std::vector<MelodyEditSuggestion>& out) const;
};
