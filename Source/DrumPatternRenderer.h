#pragma once
#include <JuceHeader.h>
#include "StackRenderer.h"
#include <array>
#include <functional>
#include <vector>

// One drum element's fixed 8-bar (16th-note) hit pattern plus the exact
// sample file assigned to it in the drum machine grid. Size must match
// DrumMachineComponent::kNumSteps (8 bars x 16 steps/bar).
struct DrumHitRow
{
    juce::String          elementName;
    juce::File            sampleFile;
    std::array<bool, 128> steps {};
};

using DrumRenderCallback =
    std::function<void(bool success, const juce::String& log, std::vector<StackStem>)>;

// Renders an 8-bar drum loop directly from the samples assigned in the
// DrumMachineComponent grid — one WAV per element, all sharing the same
// length/tempo grid so dragging them onto separate Ableton tracks keeps them
// in sync. Pure C++ (juce::AudioFormatManager), no Companion/Python involved,
// so what you see in the grid is exactly what gets rendered.
class DrumPatternRenderer : private juce::Thread
{
public:
    DrumPatternRenderer();
    ~DrumPatternRenderer() override;

    void render(std::vector<DrumHitRow> rowsToRender, float bpm, DrumRenderCallback callback);
    bool isRendering() const noexcept { return isThreadRunning(); }

private:
    void run() override;

    std::vector<DrumHitRow> pendingRows;
    float                    pendingBpm = 124.0f;
    DrumRenderCallback       pendingCallback;
};
