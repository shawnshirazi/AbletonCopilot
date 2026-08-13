#include "MelodyScorer.h"
#include <cmath>
#include <vector>

namespace
{
    // Matches MelodyGridComponent::kMelodyOff — kept local, same convention
    // already used in MelodyCritic.cpp/TheoryAdvisor.cpp/ArrangementAdvisor.cpp.
    constexpr int8_t kNoNote = -128;

    constexpr int kPhraseSteps = 32; // 2 bars, matches the 4-pass phrase structure used elsewhere
    constexpr int kNumPhrases  = 4;

    // Ordered list of (step, offset) for every active note.
    std::vector<std::pair<int, int8_t>> activeNotes(const std::array<int8_t, 128>& offsets)
    {
        std::vector<std::pair<int, int8_t>> notes;
        for (int step = 0; step < (int) offsets.size(); ++step)
            if (offsets[(size_t) step] != kNoNote)
                notes.push_back({ step, offsets[(size_t) step] });
        return notes;
    }

    float scoreMotifEconomy(const std::array<int8_t, 128>& offsets)
    {
        // Compare phrases 2-4 against phrase 1 (the "opening idea"): a mix
        // of rhythmic-placement agreement (same steps active, regardless of
        // pitch) and melodic-contour agreement (same up/down/same direction
        // between corresponding active notes) - transposed repeats of the
        // seed motif still count, exact pitch doesn't need to match.
        auto phraseActiveMask = [&](int phrase)
        {
            std::array<bool, kPhraseSteps> mask {};
            const int start = phrase * kPhraseSteps;
            for (int i = 0; i < kPhraseSteps; ++i)
                mask[(size_t) i] = offsets[(size_t) (start + i)] != kNoNote;
            return mask;
        };

        auto phraseContour = [&](int phrase)
        {
            std::vector<int> dirs; // -1/0/1 between consecutive active notes within the phrase
            int8_t last = 0;
            bool haveLast = false;
            const int start = phrase * kPhraseSteps;
            for (int i = 0; i < kPhraseSteps; ++i)
            {
                int8_t v = offsets[(size_t) (start + i)];
                if (v == kNoNote) continue;
                if (haveLast)
                    dirs.push_back(v > last ? 1 : (v < last ? -1 : 0));
                last = v;
                haveLast = true;
            }
            return dirs;
        };

        const auto seedMask    = phraseActiveMask(0);
        const auto seedContour = phraseContour(0);

        float total = 0.0f;
        int   phrasesWithContent = 0;

        for (int phrase = 1; phrase < kNumPhrases; ++phrase)
        {
            const auto mask = phraseActiveMask(phrase);
            int matches = 0;
            for (int i = 0; i < kPhraseSteps; ++i)
                if (mask[(size_t) i] == seedMask[(size_t) i])
                    ++matches;
            const float rhythmMatch = (float) matches / (float) kPhraseSteps;

            const auto contour = phraseContour(phrase);
            float contourMatch = 1.0f; // no notes to compare - neutral, don't penalize
            if (!contour.empty() || !seedContour.empty())
            {
                const size_t n = juce::jmin(contour.size(), seedContour.size());
                if (n == 0)
                {
                    contourMatch = 0.0f;
                }
                else
                {
                    size_t agree = 0;
                    for (size_t i = 0; i < n; ++i)
                        if (contour[i] == seedContour[i])
                            ++agree;
                    contourMatch = (float) agree / (float) n;
                }
            }

            bool hasContent = false;
            for (bool b : mask) if (b) { hasContent = true; break; }
            if (hasContent)
                ++phrasesWithContent;

            total += 0.5f * rhythmMatch + 0.5f * contourMatch;
        }

        return total / (float) (kNumPhrases - 1);
    }

    float scoreContourDiscipline(const std::array<int8_t, 128>& offsets)
    {
        auto notes = activeNotes(offsets);
        if (notes.size() < 2)
            return 1.0f; // nothing to judge - neutral

        int steps = 0, leaps = 0, recoveredLeaps = 0;
        for (size_t i = 1; i < notes.size(); ++i)
        {
            const int interval = std::abs((int) notes[i].second - (int) notes[i - 1].second);
            const bool isLeap = interval >= 3;
            if (isLeap)
            {
                ++leaps;
                if (i + 1 < notes.size())
                {
                    const int prevDir = notes[i].second > notes[i - 1].second ? 1 : -1;
                    const int nextDir = notes[i + 1].second > notes[i].second ? 1
                                       : notes[i + 1].second < notes[i].second ? -1 : 0;
                    if (nextDir == -prevDir)
                        ++recoveredLeaps;
                }
            }
            else
            {
                ++steps;
            }
        }

        const int totalIntervals = steps + leaps;
        const float stepRatio = totalIntervals > 0 ? (float) steps / (float) totalIntervals : 1.0f;
        const float stepScore = 1.0f - juce::jmin(1.0f, std::abs(stepRatio - 0.7f) / 0.7f);
        const float recoveryScore = leaps > 0 ? (float) recoveredLeaps / (float) leaps : 1.0f;

        return 0.5f * stepScore + 0.5f * recoveryScore;
    }

    float scorePitchClassRestraint(const std::array<int8_t, 128>& offsets)
    {
        float total = 0.0f;
        for (int phrase = 0; phrase < kNumPhrases; ++phrase)
        {
            bool seen[12] = {};
            int distinct = 0;
            const int start = phrase * kPhraseSteps;
            for (int i = 0; i < kPhraseSteps; ++i)
            {
                int8_t v = offsets[(size_t) (start + i)];
                if (v == kNoNote) continue;
                int pc = ((v % 12) + 12) % 12;
                if (!seen[pc]) { seen[pc] = true; ++distinct; }
            }
            total += distinct == 0 ? 1.0f : juce::jmax(0.0f, 1.0f - (float) juce::jmax(0, distinct - 4) * 0.15f);
        }
        return total / (float) kNumPhrases;
    }

    float scorePhraseResolution(const std::array<int8_t, 128>& offsets)
    {
        int resolved = 0, withContent = 0;
        for (int phrase = 0; phrase < kNumPhrases; ++phrase)
        {
            const int start = phrase * kPhraseSteps;
            int8_t lastOffset = 0;
            bool hasContent = false;
            for (int i = 0; i < kPhraseSteps; ++i)
            {
                int8_t v = offsets[(size_t) (start + i)];
                if (v != kNoNote) { lastOffset = v; hasContent = true; }
            }
            if (!hasContent)
                continue;
            ++withContent;
            const int pc = ((lastOffset % 12) + 12) % 12;
            if (pc == 0 || pc == 7)
                ++resolved;
        }
        return withContent == 0 ? 1.0f : (float) resolved / (float) withContent;
    }
}

MelodyScore MelodyScorer::scoreTrack(const std::array<int8_t, 128>& offsets, MelodyCategory category,
                                      int keyRootSemitone, bool isMinor)
{
    juce::ignoreUnused(category, keyRootSemitone, isMinor); // reserved for future scoring dimensions

    MelodyScore s;
    s.motifEconomy        = scoreMotifEconomy(offsets);
    s.contourDiscipline   = scoreContourDiscipline(offsets);
    s.pitchClassRestraint = scorePitchClassRestraint(offsets);
    s.phraseResolution    = scorePhraseResolution(offsets);

    s.total = 0.30f * s.motifEconomy + 0.25f * s.contourDiscipline
            + 0.20f * s.pitchClassRestraint + 0.25f * s.phraseResolution;
    return s;
}
