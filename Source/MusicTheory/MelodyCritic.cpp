#include "MelodyCritic.h"
#include "MelodicTechnoTheory.h"
#include <cmath>
#include <climits>

namespace
{
    // Matches MelodyGridComponent::kMelodyOff — kept local so this module
    // stays decoupled from the UI-layer component (this is pure theory/data).
    constexpr int8_t kNoNote = -128;

    const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F",
                                    "F#", "G", "G#", "A", "A#", "B" };

    juce::String noteName(int semitone)
    {
        return kNoteNames[((semitone % 12) + 12) % 12];
    }

    bool inScale(int offsetSemitones, bool isMinor)
    {
        const int pc = ((offsetSemitones % 12) + 12) % 12;
        const int* primary   = isMinor ? MelodicTechnoTheory::kAeolianScale : MelodicTechnoTheory::kIonianScale;
        for (int i = 0; i < 7; ++i)
            if (primary[i] == pc) return true;

        if (isMinor) // Dorian is equally legitimate in melodic techno - only flag if outside both
            for (int i = 0; i < 7; ++i)
                if (MelodicTechnoTheory::kDorianScale[i] == pc) return true;

        return false;
    }
}

std::vector<MelodyEditSuggestion> MelodyCritic::critique(int trackIndex, const juce::String& trackLabel,
                                                           const std::array<int8_t, 128>& offsets,
                                                           int keyRootSemitone, bool isMinor,
                                                           MelodyCategory category) const
{
    std::vector<MelodyEditSuggestion> out;
    checkScaleConformance(trackIndex, trackLabel, offsets, keyRootSemitone, isMinor, out);
    checkPhraseResolution(trackIndex, trackLabel, offsets, keyRootSemitone, out);
    checkEarCandyGaps(trackIndex, trackLabel, offsets, out);
    checkRegisterViolations(trackIndex, trackLabel, offsets, category, out);
    checkPitchClassSprawl(trackIndex, trackLabel, offsets, out);
    checkStaticRepetition(trackIndex, trackLabel, offsets, out);
    return out;
}

int8_t MelodyCritic::nearestInScaleTone(int8_t offset, bool isMinor)
{
    if (inScale(offset, isMinor))
        return offset;

    for (int d = 1; d <= 6; ++d)
    {
        if (inScale(offset + d, isMinor)) return (int8_t) (offset + d);
        if (inScale(offset - d, isMinor)) return (int8_t) (offset - d);
    }
    return offset; // shouldn't happen — every semitone is within 6 of some scale tone
}

std::optional<LayeringSuggestion> MelodyCritic::suggestLayering(int trackIndex, const juce::String& trackLabel,
                                                                   MelodyCategory sourceCategory,
                                                                   const std::array<int8_t, 128>& offsets,
                                                                   int numExistingTracks, int maxTracks) const
{
    if (numExistingTracks >= maxTracks)
        return std::nullopt;

    int activeSteps = 0;
    for (auto o : offsets)
        if (o != kNoNote)
            ++activeSteps;

    if (activeSteps < 8) // not enough content in this track to be worth doubling
        return std::nullopt;

    LayeringSuggestion s;
    s.sourceTrackIndex = trackIndex;
    s.sourceLabel      = trackLabel;

    switch (sourceCategory)
    {
        case MelodyCategory::Bass:
            s.transposeSemitones = 12; // octave up - the genre's actual go-to move
            s.suggestedCategory  = MelodyCategory::Pad;
            s.reason = "Doubling " + trackLabel + " an octave up on a pad is the classic melodic-techno "
                       "\"chord stack\" - sub/pad/pluck all playing the same progression, stacked in "
                       "octaves. (A plucked layer a fifth up instead works too if you want movement "
                       "rather than stacked harmony.)";
            break;
        case MelodyCategory::Lead:
            s.transposeSemitones = 12; // octave up - same chord-stack idea, one layer up from lead
            s.suggestedCategory  = MelodyCategory::Pad;
            s.reason = "A pad doubling " + trackLabel + " an octave up extends the same three-octave "
                       "chord-stack technique the bass/pad/pluck combo uses, giving the lead a wider "
                       "harmonic bed underneath it.";
            break;
        case MelodyCategory::Pad:
            s.transposeSemitones = 3;  // minor third up
            s.suggestedCategory  = MelodyCategory::Lead;
            s.reason = "A lead a third above " + trackLabel + " picks the motif out of the pad texture.";
            break;
        case MelodyCategory::Pluck:
            s.transposeSemitones = 12; // octave up
            s.suggestedCategory  = MelodyCategory::Synth;
            s.reason = "An octave-up synth layer on " + trackLabel + " continues the chord-stack idiom "
                       "(pad/pluck/sub all in the same progression, one octave apart) with brightness on top.";
            break;
        case MelodyCategory::Synth:
            s.transposeSemitones = 12; // octave up - keeps it in the same chord-stack family
            s.suggestedCategory  = MelodyCategory::Pad;
            s.reason = "A pad an octave above " + trackLabel + " (chord-stack style) gives the motif "
                       "more harmonic weight; a fifth-up pad is the alternative if you want tension instead.";
            break;
        default:
            return std::nullopt; // Fx/Other aren't melodic enough to layer meaningfully
    }

    return s;
}

void MelodyCritic::checkScaleConformance(int trackIndex, const juce::String& trackLabel,
                                          const std::array<int8_t, 128>& offsets,
                                          int keyRootSemitone, bool isMinor,
                                          std::vector<MelodyEditSuggestion>& out) const
{
    for (int step = 0; step < (int) offsets.size(); ++step)
    {
        const int8_t off = offsets[step];
        if (off == kNoNote)
            continue;

        if (inScale(off, isMinor))
            continue;

        const int8_t best = nearestInScaleTone(off, isMinor);
        if (best == off)
            continue; // shouldn't happen — every semitone is within 6 of some scale tone

        out.push_back({ trackIndex, trackLabel, step, off, best,
            "Step " + juce::String(step) + " (" + noteName(keyRootSemitone + off) +
            ") is outside the current " + juce::String(isMinor ? "minor" : "major") +
            " scale — nearest in-scale note is " + noteName(keyRootSemitone + best) + "." });
    }
}

void MelodyCritic::checkPhraseResolution(int trackIndex, const juce::String& trackLabel,
                                          const std::array<int8_t, 128>& offsets,
                                          int keyRootSemitone,
                                          std::vector<MelodyEditSuggestion>& out) const
{
    constexpr int kPhraseSteps = 32; // 2 bars, matches the 4-pass phrase structure used at generation
    constexpr int kNumPhrases  = 4;

    for (int phrase = 0; phrase < kNumPhrases; ++phrase)
    {
        const int start = phrase * kPhraseSteps;
        int    lastStep   = -1;
        int8_t lastOffset = 0;

        for (int step = start; step < start + kPhraseSteps; ++step)
        {
            if (offsets[step] != kNoNote)
            {
                lastStep   = step;
                lastOffset = offsets[step];
            }
        }
        if (lastStep < 0)
            continue; // empty phrase, nothing to resolve

        const int pc = ((lastOffset % 12) + 12) % 12;
        if (pc == 0 || pc == 7)
            continue; // already lands on root or fifth

        const int octave = lastOffset >= 0 ? lastOffset / 12 : (lastOffset - 11) / 12;
        const int root    = octave * 12;
        const int fifth    = octave * 12 + 7;
        const int8_t target = (int8_t) (std::abs(lastOffset - root) <= std::abs(lastOffset - fifth) ? root : fifth);

        out.push_back({ trackIndex, trackLabel, lastStep, lastOffset, target,
            "Phrase " + juce::String(phrase + 1) + " ends on " + noteName(keyRootSemitone + lastOffset) +
            ", which doesn't resolve to the root or fifth — landing on " +
            noteName(keyRootSemitone + target) + " instead reads as more resolved." });
    }
}

void MelodyCritic::checkEarCandyGaps(int trackIndex, const juce::String& trackLabel,
                                      const std::array<int8_t, 128>& offsets,
                                      std::vector<MelodyEditSuggestion>& out) const
{
    constexpr int kPhraseSteps  = 32;
    constexpr int kNumPhrases   = 4;
    constexpr int kMinGapSteps  = 8; // only flag a genuinely long stretch of silence

    for (int phrase = 0; phrase < kNumPhrases; ++phrase)
    {
        const int start = phrase * kPhraseSteps;
        const int end   = start + kPhraseSteps;

        int gapStart = -1;
        for (int step = start; step <= end; ++step)
        {
            const bool empty = step < end && offsets[(size_t) step] == kNoNote;
            if (empty && gapStart < 0)
            {
                gapStart = step;
            }
            else if (!empty && gapStart >= 0)
            {
                const int gapLen = step - gapStart;
                // Needs an active note on both sides to reference — a gap at
                // the very start/end of the phrase is a deliberate rest, not
                // something to fill.
                if (gapLen >= kMinGapSteps && gapStart > start && step < end)
                {
                    const int fillStep   = gapStart + gapLen / 2;
                    const int8_t anchor  = offsets[(size_t) (gapStart - 1)];
                    const int8_t fillNote = (int8_t) juce::jlimit(-24, 24, (int) anchor + 3); // a third above

                    out.push_back({ trackIndex, trackLabel, fillStep, kNoNote, fillNote,
                        "Ear candy: steps " + juce::String(gapStart) + "-" + juce::String(step - 1) +
                        " are empty in phrase " + juce::String(phrase + 1) +
                        " - a passing note here breaks up the static stretch." });
                }
                gapStart = -1;
            }
        }
    }
}

void MelodyCritic::checkRegisterViolations(int trackIndex, const juce::String& trackLabel,
                                            const std::array<int8_t, 128>& offsets, MelodyCategory category,
                                            std::vector<MelodyEditSuggestion>& out) const
{
    const auto& range = MelodicTechnoTheory::kCategoryRegister[(int) category];

    for (int step = 0; step < (int) offsets.size(); ++step)
    {
        const int8_t off = offsets[(size_t) step];
        if (off == kNoNote)
            continue;
        if (off >= range.minSemitone && off <= range.maxSemitone)
            continue;

        // Octave-shift toward the valid range rather than clamping to the
        // edge, so the pitch class (and scale membership) is preserved.
        int shifted = off;
        while (shifted > range.maxSemitone) shifted -= 12;
        while (shifted < range.minSemitone) shifted += 12;
        shifted = juce::jlimit(range.minSemitone, range.maxSemitone, shifted);

        out.push_back({ trackIndex, trackLabel, step, off, (int8_t) shifted,
            "Step " + juce::String(step) + " is outside " + trackLabel + "'s usual register (" +
            juce::String(range.minSemitone) + " to " + juce::String(range.maxSemitone) +
            " semitones from root) - octave-shifting keeps the same pitch class in a more idiomatic range." });
    }
}

void MelodyCritic::checkPitchClassSprawl(int trackIndex, const juce::String& trackLabel,
                                          const std::array<int8_t, 128>& offsets,
                                          std::vector<MelodyEditSuggestion>& out) const
{
    constexpr int kPhraseSteps    = 32;
    constexpr int kNumPhrases     = 4;
    constexpr int kMaxIdiomaticPC = 4; // root/third/fifth/one passing tone reads as intentional

    for (int phrase = 0; phrase < kNumPhrases; ++phrase)
    {
        const int start = phrase * kPhraseSteps;

        int pcCount[12] = {};
        int lastStepForPC[12];
        for (int i = 0; i < 12; ++i) lastStepForPC[i] = -1;

        for (int i = 0; i < kPhraseSteps; ++i)
        {
            const int8_t v = offsets[(size_t) (start + i)];
            if (v == kNoNote) continue;
            const int pc = ((v % 12) + 12) % 12;
            ++pcCount[pc];
            lastStepForPC[pc] = start + i;
        }

        int distinct = 0;
        for (int i = 0; i < 12; ++i) if (pcCount[i] > 0) ++distinct;
        if (distinct <= kMaxIdiomaticPC)
            continue;

        // Flag the least-common pitch class (ties broken by lowest pc) as
        // the outlier to consolidate - it's the one contributing least to
        // an established idea.
        int outlierPC = -1, outlierCount = INT_MAX;
        for (int i = 0; i < 12; ++i)
        {
            if (pcCount[i] > 0 && pcCount[i] < outlierCount)
            {
                outlierCount = pcCount[i];
                outlierPC = i;
            }
        }
        if (outlierPC < 0)
            continue;

        // Consolidate toward the most-used pitch class in the phrase.
        int dominantPC = -1, dominantCount = -1;
        for (int i = 0; i < 12; ++i)
        {
            if (i != outlierPC && pcCount[i] > dominantCount)
            {
                dominantCount = pcCount[i];
                dominantPC = i;
            }
        }
        if (dominantPC < 0)
            continue;

        const int step = lastStepForPC[outlierPC];
        const int8_t oldOffset = offsets[(size_t) step];
        const int delta = dominantPC - outlierPC;
        const int8_t newOffset = (int8_t) (oldOffset + (delta > 6 ? delta - 12 : delta < -6 ? delta + 12 : delta));

        out.push_back({ trackIndex, trackLabel, step, oldOffset, newOffset,
            "Phrase " + juce::String(phrase + 1) + " uses " + juce::String(distinct) +
            " different pitches - consolidating the least-used one toward a note already established "
            "in this phrase reads as more intentional." });
    }
}

void MelodyCritic::checkStaticRepetition(int trackIndex, const juce::String& trackLabel,
                                          const std::array<int8_t, 128>& offsets,
                                          std::vector<MelodyEditSuggestion>& out) const
{
    constexpr int kPhraseSteps = 32;
    constexpr int kNumPhrases  = 4;

    for (int phrase = 1; phrase < kNumPhrases; ++phrase)
    {
        const int start     = phrase * kPhraseSteps;
        const int seedStart = 0;

        bool identical = true;
        for (int i = 0; i < kPhraseSteps && identical; ++i)
            if (offsets[(size_t) (start + i)] != offsets[(size_t) (seedStart + i)])
                identical = false;

        if (!identical)
            continue;

        // Only judge phrases that actually have content - an identically
        // empty phrase isn't "static repetition", it's a deliberate rest.
        bool hasContent = false;
        for (int i = 0; i < kPhraseSteps && !hasContent; ++i)
            if (offsets[(size_t) (start + i)] != kNoNote)
                hasContent = true;
        if (!hasContent)
            continue;

        // Prefer swapping the first root-note hit to the fifth (the named
        // sourced technique); fall back to nudging the first active note's
        // step by one if there's no root hit to swap.
        int rootStep = -1;
        for (int i = 0; i < kPhraseSteps; ++i)
        {
            const int8_t v = offsets[(size_t) (start + i)];
            if (v != kNoNote && ((v % 12) + 12) % 12 == 0) { rootStep = start + i; break; }
        }

        if (rootStep >= 0)
        {
            const int8_t oldOffset = offsets[(size_t) rootStep];
            const int8_t newOffset = (int8_t) (oldOffset + 7); // root -> fifth
            out.push_back({ trackIndex, trackLabel, rootStep, oldOffset, newOffset,
                "Phrase " + juce::String(phrase + 1) + " repeats phrase 1 note-for-note - swapping one "
                "root hit for the fifth is the standard way melodic techno basslines stay hypnotic "
                "without going fully static." });
            continue;
        }

        int firstActiveStep = -1;
        for (int i = 0; i < kPhraseSteps; ++i)
            if (offsets[(size_t) (start + i)] != kNoNote) { firstActiveStep = start + i; break; }

        if (firstActiveStep >= 0 && firstActiveStep + 1 < start + kPhraseSteps
            && offsets[(size_t) (firstActiveStep + 1)] == kNoNote)
        {
            out.push_back({ trackIndex, trackLabel, firstActiveStep, offsets[(size_t) firstActiveStep], kNoNote,
                "Phrase " + juce::String(phrase + 1) + " repeats phrase 1 note-for-note - nudging this "
                "note's timing by a 16th breaks the static repetition (approve, then place a note one "
                "step later to complete the shift)." });
        }
    }
}
