#pragma once
#include <JuceHeader.h>
#include <vector>
#include "FeatureExtractor.h"
#include "GenreProfiles.h"

enum class Severity { Good, Warning, Critical };

struct FeedbackItem
{
    Severity     severity  = Severity::Good;
    juce::String category;
    juce::String headline;
    juce::String detail;
    juce::String fix;
};

class FeedbackEngine
{
public:
    std::vector<FeedbackItem> generate(const AudioFeatures& features,
                                        const GenreProfile& profile) const;

private:
    void checkLoudness   (const AudioFeatures&, const GenreProfile&, std::vector<FeedbackItem>&) const;
    void checkDynamics   (const AudioFeatures&, const GenreProfile&, std::vector<FeedbackItem>&) const;
    void checkSpectral   (const AudioFeatures&, const GenreProfile&, std::vector<FeedbackItem>&) const;
    void checkStereo     (const AudioFeatures&, const GenreProfile&, std::vector<FeedbackItem>&) const;
    void checkRhythm     (const AudioFeatures&, const GenreProfile&, std::vector<FeedbackItem>&) const;
    void checkKey        (const AudioFeatures&, const GenreProfile&, std::vector<FeedbackItem>&) const;
    void suggestChain    (const AudioFeatures&, const GenreProfile&, std::vector<FeedbackItem>&) const;
    static FeedbackItem good    (const juce::String& cat, const juce::String& headline);
    static FeedbackItem warn    (const juce::String& cat, const juce::String& headline,
                                 const juce::String& detail, const juce::String& fix);
    static FeedbackItem critical(const juce::String& cat, const juce::String& headline,
                                 const juce::String& detail, const juce::String& fix);
    static juce::String db(float v);
    static juce::String pct(float v);
};
