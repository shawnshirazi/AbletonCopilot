#include "PresetLibraryScanner.h"
#include <algorithm>

PresetLibraryScanner::PresetLibraryScanner() : juce::Thread("PresetLibraryScan") {}

PresetLibraryScanner::~PresetLibraryScanner()
{
    aliveFlag->store(false, std::memory_order_release);
    stopThread(4000);
}

void PresetLibraryScanner::setLibraryDir(const juce::File& dir)
{
    if (isThreadRunning())
        stopThread(2000);

    pendingDir = dir;
    startThread();
}

void PresetLibraryScanner::run()
{
    auto dir = pendingDir;
    std::vector<PresetEntry> built;

    if (dir.isDirectory())
    {
        auto addAll = [&](const char* wildcard, const juce::String& pluginName)
        {
            auto files = dir.findChildFiles(juce::File::findFiles, true, wildcard);
            for (auto& f : files)
            {
                if (threadShouldExit())
                    return;

                auto stem = f.getFileNameWithoutExtension();
                built.push_back({ pluginName, stem, f, parseCategoryFromPrompt(stem) });
            }
        };

        addAll("*.SerumPreset", "Serum 2");
        if (threadShouldExit()) return;
        addAll("*.h2p", "Diva");
    }

    if (threadShouldExit())
        return;

    std::sort(built.begin(), built.end(),
              [](const PresetEntry& a, const PresetEntry& b) { return a.name < b.name; });

    auto flag = aliveFlag;
    juce::MessageManager::callAsync([this, flag, built]() mutable
    {
        if (!flag->load(std::memory_order_acquire))
            return; // scanner was destroyed before this landed

        if (onPresetsChanged)
            onPresetsChanged(built);
    });
}
