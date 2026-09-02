#include "SoundLibraryLearner.h"
#include "SerumAutomation.h"
#include "SerumSoundTest.h"
#include "SoundRecommendation.h"
#include <algorithm>
#include <unistd.h>

namespace SoundLibraryLearner
{
    namespace
    {
        // Best-effort visual evidence of what Serum 2 is actually showing
        // before/after a preset-advance click. Serum 2's own preset-name
        // label is NOT exposed as an AX text element (SerumAutomation.h's
        // own top comment - a real AX-tree dump showed zero accessible
        // sub-elements on its content canvas), so a screen pixel capture of
        // the target window is the only way to independently confirm the
        // visible preset changed. Diagnostic-only: a failure here (e.g. no
        // Screen Recording permission) must never fail the actual learn.
        void captureDiagnosticScreenshot(const SerumAutomation::WindowBounds& b, const juce::File& outFile)
        {
            if (!b.found || b.w <= 0.0f || b.h <= 0.0f)
                return;
            juce::StringArray args {
                "/usr/sbin/screencapture", "-x",
                "-R" + juce::String((int) b.x) + "," + juce::String((int) b.y) + ","
                     + juce::String((int) b.w) + "," + juce::String((int) b.h),
                outFile.getFullPathName()
            };
            juce::ChildProcess proc;
            if (proc.start(args))
                proc.waitForProcessToFinish(3000);
        }
    }

    std::vector<Candidate> discoverCandidates(const juce::String& role, int maxPerCategory, const juce::File& factoryRoot)
    {
        std::vector<Candidate> result;
        juce::Array<juce::File> folders;

        // Real, verified-to-exist-on-this-machine Factory category
        // folders for each role - never assumed. "Category only
        // organizes the learning set" per this pass's own instruction;
        // the actual analysis (not the folder name) determines Sound DNA.
        if (role == "Bass")
            folders.add(factoryRoot.getChildFile("Bass"));
        else if (role == "Melody")
        {
            folders.add(factoryRoot.getChildFile("Lead"));
            folders.add(factoryRoot.getChildFile("Pluck"));
            folders.add(factoryRoot.getChildFile("Arp"));
            folders.add(factoryRoot.getChildFile("Seq"));
        }
        else if (role == "Pad")
        {
            folders.add(factoryRoot.getChildFile("Pad"));
            folders.add(factoryRoot.getChildFile("Soundscape"));
            folders.add(factoryRoot.getChildFile("String"));
        }
        else
        {
            return result; // unrecognized role - empty, not a guess
        }

        juce::Array<juce::File> allFiles;
        for (auto& folder : folders)
        {
            if (!folder.isDirectory())
                continue;
            juce::Array<juce::File> found;
            folder.findChildFiles(found, juce::File::findFiles, true, "*.SerumPreset"); // recursive - real subfolders like Bass/Reese, Bass/Sub
            allFiles.addArray(found);
        }
        allFiles.sort();

        for (auto& f : allFiles)
        {
            if ((int) result.size() >= maxPerCategory)
                break;
            Candidate c;
            c.file       = f;
            c.role       = role;
            c.sourceHint = f.getFullPathName().fromFirstOccurrenceOf("Factory", true, false);
            result.push_back(c);
        }
        return result;
    }

    BatchResult learnBatch(AbletonCopilotAudioProcessor& processor, int trackIndex,
                            const juce::String& role, int maxPerCategory,
                            const juce::String& windowTitleHint,
                            ProgressCallback onProgress)
    {
        BatchResult result;

        // Persistent, append-only diagnostic transcript - onProgress only
        // reaches a transient UI label (each call overwrites the last), so
        // without this file every message before the most recent one is
        // unrecoverable once the run finishes. Also holds the before/after
        // screenshots below. Never gates/fails the actual learn.
        const auto diagDir = SoundDna::libraryDir().getChildFile("diag");
        diagDir.createDirectory();
        const auto diagLog = diagDir.getChildFile("test_log.txt");

        auto report = [&](const juce::String& msg, int idx, int total)
        {
            if (onProgress)
                onProgress({ msg, idx, total });
            juce::Logger::writeToLog(msg);
            diagLog.appendText(msg + "\n", false, false);
        };

        const auto factoryRoot = SoundRecommendation::defaultSerumFactoryPresetsRoot();
        const auto candidates  = discoverCandidates(role, maxPerCategory, factoryRoot);
        const int totalOnDisk  = (int) candidates.size();

        report("Found " + juce::String(totalOnDisk) + " " + role + " candidate(s) on disk (capped at "
                   + juce::String(maxPerCategory) + " per category)", 0, totalOnDisk);

        std::vector<SoundDna::LearnedSound> library;
        SoundDna::loadLibraryIndex(library);

        const bool automationOk = SerumAutomation::requestAutomationPermission();
        if (!automationOk)
            report("Accessibility permission not granted to this process - real factory-library "
                   "candidates cannot be automatically loaded/verified this run (System Settings -> "
                   "Privacy & Security -> Accessibility). Every candidate below will be recorded as "
                   "FAILED with this reason - never silently skipped or claimed successful.", 0, totalOnDisk);

        const double sr = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 44100.0;

        int index = 0;
        for (auto& cand : candidates)
        {
            ++index;
            const juce::String fingerprint = SoundDna::fingerprintForFile(cand.file);
            const juce::String id          = SoundDna::makeId(fingerprint);

            auto existing = std::find_if(library.begin(), library.end(),
                                          [&](const SoundDna::LearnedSound& s) { return s.id == id; });
            if (existing != library.end() && existing->status == SoundDna::LearnStatus::Success)
            {
                result.skipped++;
                result.sounds.push_back(*existing);
                report("Skipped (already learned): " + cand.sourceHint, index, totalOnDisk);
                continue;
            }

            report(role + " " + juce::String(index) + "/" + juce::String(totalOnDisk) + " - " + cand.sourceHint,
                   index, totalOnDisk);

            SoundDna::LearnedSound entry;
            entry.id                = id;
            entry.role               = role;
            entry.sourceHint         = cand.sourceHint;
            entry.sourceFingerprint  = fingerprint;
            entry.displayName        = "unknown"; // never fabricated - see SoundDnaLibrary.h's own top comment
            entry.analysisVersion    = 1;

            auto persistAndContinue = [&]
            {
                result.sounds.push_back(entry);
                library.erase(std::remove_if(library.begin(), library.end(),
                                              [&](const SoundDna::LearnedSound& s) { return s.id == entry.id; }),
                              library.end());
                library.push_back(entry);
                SoundDna::saveLibraryIndex(library);
            };

            if (!automationOk)
            {
                entry.status       = SoundDna::LearnStatus::Failed;
                entry.statusReason = "Accessibility permission not granted to this process";
                result.failed++;
                persistAndContinue();
                continue;
            }

            // Verification baseline: real Serum 2 state BEFORE the click.
            juce::MemoryBlock before;
            processor.captureMelodyTrackState(trackIndex, before);

            const auto boundsBeforeClick = SerumAutomation::findOwnWindow(windowTitleHint);
            const juce::String diagBase  = role + "_" + juce::String(index);
            report("  [diag] pid=" + juce::String((int) getpid())
                       + " window=\"" + windowTitleHint + "\""
                       + " bounds=(" + juce::String((int) boundsBeforeClick.x) + "," + juce::String((int) boundsBeforeClick.y)
                       + " " + juce::String((int) boundsBeforeClick.w) + "x" + juce::String((int) boundsBeforeClick.h) + ")"
                       + " windowFound=" + (boundsBeforeClick.found ? juce::String("yes") : juce::String("no"))
                       + " expectedCandidate=\"" + cand.sourceHint + "\""
                       + " fpBefore=" + SoundDna::fingerprintForCapturedState(before),
                   index, totalOnDisk);
            captureDiagnosticScreenshot(boundsBeforeClick, diagDir.getChildFile(diagBase + "_before.png"));

            report("  advancing Serum 2's browser...", index, totalOnDisk);
            const bool clicked = SerumAutomation::clickNextPreset(windowTitleHint);
            // 150ms was proven too short on this machine: a direct, isolated
            // CGEventPost test at this exact click coordinate visibly
            // advanced Serum 2's preset browser (confirmed by screenshot),
            // yet captureMelodyTrackState() below still saw byte-identical
            // state at 150ms - Serum 2's UI-driven preset selection needs
            // more time to propagate into its serialized VST state than
            // the visible name-bar update takes. 400ms is the fix.
            juce::Thread::sleep(400);

            juce::MemoryBlock after;
            const bool captured = processor.captureMelodyTrackState(trackIndex, after);

            const auto boundsAfterClick = SerumAutomation::findOwnWindow(windowTitleHint);
            captureDiagnosticScreenshot(boundsAfterClick, diagDir.getChildFile(diagBase + "_after.png"));
            report("  [diag] fpAfter=" + (captured ? SoundDna::fingerprintForCapturedState(after) : juce::String("n/a"))
                       + " stateChanged=" + ((captured && after.getSize() > 0 && after != before) ? "yes" : "no")
                       + " captureSucceeded=" + (captured ? "yes" : "no")
                       + " clickDispatched=" + (clicked ? "yes" : "no"),
                   index, totalOnDisk);

            if (!clicked || !captured || after.getSize() == 0 || after == before)
            {
                entry.status = SoundDna::LearnStatus::Failed;
                entry.statusReason = !clicked
                    ? "automation click could not be dispatched (Serum 2 window not found)"
                    : (!captured || after.getSize() == 0)
                        ? "could not capture Serum 2 state after click"
                        : "state bytes unchanged after preset-advance click (click likely missed the target)";
                result.failed++;
                report("  FAILED: " + entry.statusReason, index, totalOnDisk);
                persistAndContinue();
                continue;
            }

            // VERIFIED: the real Serum 2 state genuinely changed. Render
            // the standardized test (Source/SerumSoundTest.h).
            report("  rendering standardized test (registers/lengths/velocities)...", index, totalOnDisk);
            bool anyRendered = false;

            for (auto& reg : SerumSoundTest::registers())
            {
                auto buf = processor.renderStandardizedNote(trackIndex, reg.midiPitch,
                    SerumSoundTest::kMediumLengthSteps, SerumSoundTest::kMediumVelocity, 4);
                SoundDna::OctaveResult r;
                r.label       = reg.label;
                r.midiPitch   = reg.midiPitch;
                r.measurement = SoundDna::analyzeBuffer(buf, sr);
                if (r.measurement.valid) anyRendered = true;
                entry.octaves.push_back(r);
            }

            for (auto& nl : SerumSoundTest::noteLengths())
            {
                const bool isSustained = juce::String(nl.label) == "sustained";
                const int tail = isSustained ? SerumSoundTest::kReleaseTailSteps : 4;
                auto buf = processor.renderStandardizedNote(trackIndex, SerumSoundTest::kRepresentativePitch,
                    nl.lengthSteps, SerumSoundTest::kMediumVelocity, tail);
                SoundDna::NoteLengthResult r;
                r.label       = nl.label;
                r.lengthSteps = nl.lengthSteps;
                r.measurement = SoundDna::analyzeBuffer(buf, sr);
                if (r.measurement.valid) anyRendered = true;
                if (isSustained && r.measurement.valid)
                {
                    const double bpm = 124.0; // matches renderStandardizedNote's own fallback used when currentBpm was never set by a host
                    const double secPerStep = 60.0 / bpm / 4.0;
                    const int samplesPerStep = juce::jmax(1, (int) std::lround(secPerStep * sr));
                    const int noteOffSample = nl.lengthSteps * samplesPerStep;
                    r.releaseTimeMs = SoundDna::measureReleaseTimeMs(buf, sr, noteOffSample);
                }
                entry.noteLengths.push_back(r);
            }

            for (auto& vel : SerumSoundTest::velocities())
            {
                auto buf = processor.renderStandardizedNote(trackIndex, SerumSoundTest::kRepresentativePitch,
                    SerumSoundTest::kMediumLengthSteps, vel.velocity, 4);
                SoundDna::VelocityResult r;
                r.label       = vel.label;
                r.velocity    = vel.velocity;
                r.measurement = SoundDna::analyzeBuffer(buf, sr);
                if (r.measurement.valid) anyRendered = true;
                entry.velocities.push_back(r);
            }

            if (!anyRendered)
            {
                entry.status       = SoundDna::LearnStatus::Failed;
                entry.statusReason = "Serum 2 state was verified changed, but no standardized-test render produced usable audio";
                result.failed++;
                report("  FAILED: " + entry.statusReason, index, totalOnDisk);
            }
            else
            {
                auto dir = SoundDna::libraryDir();
                dir.createDirectory();
                entry.capturedStateFile = entry.id + ".serumstate";
                dir.getChildFile(entry.capturedStateFile).replaceWithData(after.getData(), after.getSize());

                entry.dateLearnedIso = juce::Time::getCurrentTime().toISO8601(true);
                entry.dnaLabels      = SoundDna::deriveDnaLabels(entry);
                entry.status         = SoundDna::LearnStatus::Success;
                entry.statusReason   = "verified: Serum 2 state bytes changed after the preset-advance click, "
                                        "and the standardized test produced real rendered audio";
                result.learned++;
                report("  Learned (" + entry.dnaLabels.joinIntoString(", ") + ")", index, totalOnDisk);
            }

            persistAndContinue();
        }

        report("Learned: " + juce::String(result.learned) + "  Failed: " + juce::String(result.failed)
                   + "  Skipped: " + juce::String(result.skipped), totalOnDisk, totalOnDisk);
        return result;
    }
}
