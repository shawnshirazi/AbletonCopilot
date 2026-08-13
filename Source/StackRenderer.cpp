#include "StackRenderer.h"

namespace
{
    juce::File companionDir()
    {
        return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                   .getChildFile("Developer/AbletonCopilot/Companion");
    }

    juce::File stackOutputDir()
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("AbletonCopilot/Session/DrumStack");
    }

    std::vector<StackSection> scanOutputDir(const juce::File& dir)
    {
        std::vector<StackSection> sections;
        if (!dir.isDirectory())
            return sections;

        auto sectionDirs = dir.findChildFiles(juce::File::findDirectories, false);
        sectionDirs.sort();

        for (auto& secDir : sectionDirs)
        {
            StackSection section;
            // Folder names look like "01_Intro" — strip the numeric prefix for display.
            auto raw = secDir.getFileName();
            auto us  = raw.indexOfChar('_');
            section.display = (us >= 0 ? raw.substring(us + 1) : raw).replaceCharacter('_', ' ');

            auto wavs = secDir.findChildFiles(juce::File::findFiles, false, "*.wav");
            wavs.sort();
            for (auto& w : wavs)
                section.stems.push_back({ w.getFileNameWithoutExtension(), w });

            if (!section.stems.empty())
                sections.push_back(std::move(section));
        }
        return sections;
    }
}

StackRenderer::StackRenderer() : juce::Thread("StackRenderer") {}

StackRenderer::~StackRenderer()
{
    stopThread(4000);
}

void StackRenderer::render(const juce::String& genreId, float bpm, const juce::String& key,
                            const juce::File& libraryDir, StackRenderCallback callback)
{
    if (isThreadRunning())
        return;

    pendingGenreId    = genreId;
    pendingBpm        = bpm;
    pendingKey        = key;
    pendingLibraryDir = libraryDir;
    pendingCallback   = std::move(callback);

    startThread();
}

void StackRenderer::run()
{
    auto companion = companionDir();
    auto venvPython = companion.getChildFile("venv/bin/python3");
    juce::String pythonExe = venvPython.existsAsFile() ? venvPython.getFullPathName() : "python3";

    juce::StringArray args;
    args.add(pythonExe);
    args.add(companion.getChildFile("main.py").getFullPathName());
    args.add("--library"); args.add(pendingLibraryDir.getFullPathName());
    args.add("--genre");   args.add(pendingGenreId);
    args.add("--bpm");     args.add(juce::String(pendingBpm, 2));
    args.add("--key");     args.add(pendingKey.isNotEmpty() ? pendingKey : "C");

    juce::ChildProcess proc;
    bool started = proc.start(args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr);

    juce::String output;
    bool ok = false;
    if (started)
    {
        output = proc.readAllProcessOutput();
        proc.waitForProcessToFinish(120000);
        ok = proc.getExitCode() == 0;
    }
    else
    {
        output = "Failed to launch " + pythonExe;
    }

    std::vector<StackSection> sections;
    if (ok)
        sections = scanOutputDir(stackOutputDir());

    auto callback = pendingCallback;
    juce::MessageManager::callAsync([callback, ok, output, sections]() mutable
    {
        if (callback)
            callback(ok, output, std::move(sections));
    });
}
