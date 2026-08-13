#include "ReferenceFragmentJob.h"

namespace
{
    juce::File mlPipelineDir()
    {
        return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                   .getChildFile("Developer/AbletonCopilot/MLPipeline");
    }

    juce::File outputDir()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("AbletonCopilot");
        if (!dir.exists())
            dir.createDirectory();
        return dir;
    }

    juce::File outputJsonFile()  { return outputDir().getChildFile("reference_fragments.json"); }
    juce::File drumsJsonFile()   { return outputDir().getChildFile("reference_drums.json"); }
}

ReferenceFragmentJob::ReferenceFragmentJob() : juce::Thread("ReferenceFragmentJob") {}

ReferenceFragmentJob::~ReferenceFragmentJob()
{
    stopThread(4000);
}

void ReferenceFragmentJob::extract(const juce::File& sourceAudioFile, float bpm, int keyRootSemitone, bool isMinor,
                                    std::function<void(const ReferenceFragmentResult&)> callback)
{
    if (isThreadRunning())
        return;

    pendingSourceFile = sourceAudioFile;
    pendingBpm        = bpm;
    pendingKeyRoot    = keyRootSemitone;
    pendingIsMinor    = isMinor;
    pendingCallback   = std::move(callback);
    startThread();
}

void ReferenceFragmentJob::run()
{
    ReferenceFragmentResult result;

    auto python = mlPipelineDir().getChildFile("venv/bin/python");
    auto script = mlPipelineDir().getChildFile("extract_reference_fragments.py");
    auto output = outputJsonFile();
    auto drums  = drumsJsonFile();

    if (!python.existsAsFile() || !script.existsAsFile())
    {
        result.errorMessage = "MLPipeline venv/script not found - run pip install in MLPipeline/venv first.";
        auto cb = pendingCallback;
        juce::MessageManager::callAsync([cb, result] { cb(result); });
        return;
    }

    juce::StringArray args;
    args.add(python.getFullPathName());
    args.add(script.getFullPathName());
    args.add("--input");  args.add(pendingSourceFile.getFullPathName());
    args.add("--bpm");    args.add(juce::String(pendingBpm, 2));
    args.add("--key-root"); args.add(juce::String(pendingKeyRoot));
    args.add("--minor");  args.add(pendingIsMinor ? "true" : "false");
    args.add("--output"); args.add(output.getFullPathName());
    args.add("--drums-output"); args.add(drums.getFullPathName());

    juce::ChildProcess proc;
    if (!proc.start(args))
    {
        result.errorMessage = "Could not launch the extraction script.";
        auto cb = pendingCallback;
        juce::MessageManager::callAsync([cb, result] { cb(result); });
        return;
    }

    const juce::String log = proc.readAllProcessOutput();
    // Full 4-stem separation + bass transcription + drum onset analysis on
    // a whole track can genuinely take several minutes on CPU.
    const bool finished = proc.waitForProcessToFinish(600000);
    const int exitCode = finished ? proc.getExitCode() : -1;

    if (threadShouldExit())
        return;

    if (exitCode != 0 || !output.existsAsFile())
    {
        result.errorMessage = log.isNotEmpty() ? log.trim() : "Extraction failed (no output produced).";
        auto cb = pendingCallback;
        juce::MessageManager::callAsync([cb, result] { cb(result); });
        return;
    }

    auto parsed = juce::JSON::parse(output);
    result.success       = parsed.isArray();
    result.fragmentCount = result.success ? parsed.getArray()->size() : 0;
    result.outputJson    = output;

    if (drums.existsAsFile())
    {
        auto drumsParsed = juce::JSON::parse(drums);
        if (auto* obj = drumsParsed.getDynamicObject())
        {
            for (auto& prop : obj->getProperties())
            {
                auto* roleObj = prop.value.getDynamicObject();
                if (roleObj == nullptr)
                    continue;

                auto* stepsArr = roleObj->getProperty("steps").getArray();
                if (stepsArr == nullptr || stepsArr->size() != 128)
                    continue;

                DrumRolePattern pattern;
                pattern.rackId = prop.name.toString();
                for (int i = 0; i < 128; ++i)
                    pattern.steps[(size_t) i] = (bool) (*stepsArr)[i];
                pattern.sampleFile = juce::File(roleObj->getProperty("sample").toString());

                result.drumPatterns.push_back(pattern);
            }
        }
    }

    auto cb = pendingCallback;
    juce::MessageManager::callAsync([cb, result] { cb(result); });
}
