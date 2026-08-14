#pragma once
#include <JuceHeader.h>

// Lightweight startup-timing diagnostics - answers "what's actually
// blocking initialization" with real measurements instead of guesswork.
// Reset() truncates the log fresh at the start of
// AbletonCopilotAudioProcessor's constructor (the earliest point in a load
// cycle); Mark() appends a millisecond-resolution timestamp from both the
// processor and (if the host opens it) the editor's constructor/first
// paint, so one file shows the full timeline for the most recent load.
namespace StartupTiming
{
    inline juce::File logFile()
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("AbletonCopilot/startup_timing_log.txt");
    }

    inline void reset()
    {
        auto f = logFile();
        f.getParentDirectory().createDirectory();
        f.deleteFile();
    }

    inline void mark(const char* what)
    {
        logFile().appendText(juce::String(juce::Time::getMillisecondCounterHiRes(), 2) + "ms  " + what + "\n");
    }
}
