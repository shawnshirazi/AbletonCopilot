// Real, functional test of RackBrowserComponent's path+size+mtime-keyed
// classification cache (see RackBrowserComponent::cacheFile/saveCache/run)
// - the fix for "reopening the plugin rescans the whole ~21k-file library
// every time". Uses real temp WAV files and the real background-thread
// scan, not a mock - JUCE-linked the same way test_rack_classification.cpp
// is (see that file's own build notes): compile against the pre-built
// mod_core.o/mod_audio_basics.o/mod_audio_formats.o/mod_compiletime.o
// module objects rather than the whole plugin.
#include <JuceHeader.h>
#include "../RackBrowserComponent.h"
#include "../Engine/tests/TestSupport.h"
#include <cstdio>
#include <functional>

namespace
{
    // A minimal, real, decodable WAV file - just enough for
    // AudioFormatManager::createReaderFor() to succeed and report a real
    // duration, which is all classifySample() (and therefore the cache)
    // actually needs.
    void writeShortWav(const juce::File& file, double lengthSeconds)
    {
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::FileOutputStream> stream(file.createOutputStream());
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wav.createWriterFor(stream.get(), 44100.0, 1, 16, {}, 0));
        if (writer != nullptr)
        {
            stream.release(); // writer now owns it
            const int numSamples = (int) (lengthSeconds * 44100.0);
            juce::AudioBuffer<float> buffer(1, numSamples);
            buffer.clear();
            writer->writeFromAudioSampleBuffer(buffer, 0, numSamples);
        }
    }

    // Blocks (with a real timeout, not an unbounded wait) until either
    // onRacksChanged fires or timeoutMs elapses - the scan runs on
    // RackBrowserComponent's own background juce::Thread and reports back
    // via juce::MessageManager::callAsync, so this ALSO has to actively
    // pump the message queue (runDispatchLoopUntil) while waiting, not
    // just sleep - callAsync's posted callback never runs on its own
    // without something dispatching it, which a plain console main() with
    // no running event loop doesn't do by default.
    // `alsoCall`, if given, is invoked with the real scan result BEFORE
    // this function's own done-flag is set - lets a caller capture the
    // actual rack contents without its own onRacksChanged assignment
    // being silently overwritten by this helper (an earlier version of
    // this test did exactly that and produced two false failures - a bug
    // in the test's own plumbing, not in RackBrowserComponent).
    bool waitForScan(RackBrowserComponent& rb, int timeoutMs = 5000,
                      std::function<void(const std::vector<Rack>&)> alsoCall = nullptr)
    {
        std::atomic<bool> done { false };
        rb.onRacksChanged = [&done, alsoCall](const std::vector<Rack>& racks)
        {
            if (alsoCall)
                alsoCall(racks);
            done.store(true);
        };
        auto* mm = juce::MessageManager::getInstance();
        const auto deadline = juce::Time::getMillisecondCounterHiRes() + timeoutMs;
        while (!done.load() && juce::Time::getMillisecondCounterHiRes() < deadline)
            mm->runDispatchLoopUntil(10);
        return done.load();
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("AbletonCopilot_cache_test_" + juce::String(juce::Random::getSystemRandom().nextInt(1000000)));
    tempDir.createDirectory();

    // A cache-invalidation test needs the REAL cache file the component
    // writes to (~/Library/.../rack_classification_cache.json) to start
    // from a known state for this run - back up any existing one and
    // restore it afterward so this test doesn't clobber the user's own
    // real cache.
    auto realCacheFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                              .getChildFile("AbletonCopilot/rack_classification_cache.json");
    juce::String backedUpCache;
    const bool hadRealCache = realCacheFile.existsAsFile();
    if (hadRealCache)
        backedUpCache = realCacheFile.loadFileAsString();
    realCacheFile.deleteFile();

    auto restoreCache = [&]
    {
        if (hadRealCache)
            realCacheFile.replaceWithText(backedUpCache);
        else
            realCacheFile.deleteFile();
    };

    // --- Build a small, real, classifiable library ------------------
    auto kickDir = tempDir.getChildFile("Kicks");
    kickDir.createDirectory();
    auto kick1 = kickDir.getChildFile("Kick_001.wav");
    auto kick2 = kickDir.getChildFile("Kick_002.wav");
    writeShortWav(kick1, 0.3);
    writeShortWav(kick2, 0.3);

    // =====================================================================
    // First scan of a never-before-seen directory: no cache exists yet,
    // so classification is 100% fresh work.
    // =====================================================================
    {
        RackBrowserComponent rb;
        rb.setLibraryDir(tempDir);
        CHECK(waitForScan(rb, 8000));
        CHECK(!rb.lastScanWasFullyCached());
        CHECK(rb.lastScanCachedFileCount() == 0);
        CHECK(rb.lastScanChangedFileCount() == 2);
        CHECK(realCacheFile.existsAsFile()); // the cache file was actually written to disk
    }

    // =====================================================================
    // Unchanged file uses the cached classification: a second scan of the
    // SAME unmodified directory should be fully served from cache.
    // =====================================================================
    {
        RackBrowserComponent rb;
        rb.setLibraryDir(tempDir);
        CHECK(waitForScan(rb, 8000));
        CHECK(rb.lastScanWasFullyCached());
        CHECK(rb.lastScanCachedFileCount() == 2);
        CHECK(rb.lastScanChangedFileCount() == 0);
    }

    // =====================================================================
    // Cache survives plugin restart: this is a BRAND NEW RackBrowserComponent
    // instance (simulating a fresh plugin load), reading the SAME on-disk
    // cache file the previous instance wrote - not an in-memory cache tied
    // to one component's lifetime.
    // =====================================================================
    {
        RackBrowserComponent freshInstance;
        freshInstance.setLibraryDir(tempDir);
        CHECK(waitForScan(freshInstance, 8000));
        CHECK(freshInstance.lastScanWasFullyCached());
        CHECK(freshInstance.lastScanCachedFileCount() == 2);
    }

    // =====================================================================
    // Modified file invalidates the cache: touching kick1's content (and
    // therefore its size/mtime) means the NEXT scan must re-classify just
    // that one file, while kick2 still comes from cache.
    // =====================================================================
    {
        juce::Thread::sleep(1050); // ensure a real, distinguishable mtime change (some filesystems have 1s mtime resolution)
        writeShortWav(kick1, 0.5); // different length -> different file size

        RackBrowserComponent rb;
        rb.setLibraryDir(tempDir);
        CHECK(waitForScan(rb, 8000));
        CHECK(!rb.lastScanWasFullyCached());
        CHECK(rb.lastScanChangedFileCount() == 1);
        CHECK(rb.lastScanCachedFileCount() == 1);
    }

    // =====================================================================
    // New file gets analyzed: adding a third file to the library means
    // the next scan sees exactly one changed (new) file, the other two
    // stay cached.
    // =====================================================================
    {
        auto kick3 = kickDir.getChildFile("Kick_003.wav");
        writeShortWav(kick3, 0.3);

        RackBrowserComponent rb;
        std::vector<Rack> finalRacks;
        rb.setLibraryDir(tempDir);
        CHECK(waitForScan(rb, 8000, [&finalRacks](const std::vector<Rack>& r) { finalRacks = r; }));
        CHECK(rb.lastScanChangedFileCount() == 1); // just the new file
        CHECK(rb.lastScanCachedFileCount() == 2);  // the other two, unchanged

        int totalKickFiles = 0;
        for (auto& r : finalRacks)
            if (r.id == "KICK")
                totalKickFiles = (int) r.samples.size();
        CHECK(totalKickFiles == 3);
    }

    // =====================================================================
    // Deleted file disappears: removing kick2 means it no longer shows up
    // in the classified racks, even though it's still sitting in the
    // on-disk cache (a stale entry is harmless - it's just never matched
    // again since the file it was keyed by no longer exists on disk).
    // =====================================================================
    {
        kick2.deleteFile();

        RackBrowserComponent rb;
        std::vector<Rack> finalRacks;
        rb.setLibraryDir(tempDir);
        CHECK(waitForScan(rb, 8000, [&finalRacks](const std::vector<Rack>& r) { finalRacks = r; }));

        int totalKickFiles = 0;
        bool kick2StillPresent = false;
        for (auto& r : finalRacks)
            if (r.id == "KICK")
            {
                totalKickFiles = (int) r.samples.size();
                for (auto& s : r.samples)
                    if (s.file == kick2)
                        kick2StillPresent = true;
            }
        CHECK(totalKickFiles == 2); // kick1 + kick3, kick2 gone
        CHECK(!kick2StillPresent);
    }

    restoreCache();
    tempDir.deleteRecursively();

    TEST_SUMMARY_AND_EXIT();
}
