#pragma once
#include <JuceHeader.h>
#include <functional>
#include <vector>

struct StackStem
{
    juce::String name;   // e.g. "Kick"
    juce::File   file;
};

struct StackSection
{
    juce::String display;  // e.g. "Intro"
    std::vector<StackStem> stems;
};

using StackRenderCallback =
    std::function<void(bool success, const juce::String& log, std::vector<StackSection>)>;

// Shells out to Companion/main.py --genre --bpm --key to render a synced
// drum stack, then scans the output folder for the resulting stems.
class StackRenderer : private juce::Thread
{
public:
    StackRenderer();
    ~StackRenderer() override;

    void render(const juce::String& genreId, float bpm, const juce::String& key,
                const juce::File& libraryDir, StackRenderCallback callback);

    bool isRendering() const noexcept { return isThreadRunning(); }

private:
    void run() override;

    juce::String        pendingGenreId;
    float                pendingBpm = 0.0f;
    juce::String        pendingKey;
    juce::File           pendingLibraryDir;
    StackRenderCallback pendingCallback;
};
