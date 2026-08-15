#include "DrumSampleIndex.h"

namespace
{
    // Only these racks are ever drum-role candidates for the generated
    // pattern (see DrumSampleSelector.h) - loops, FX, vocals, bass, toms,
    // cymbals, and misc are skipped entirely, so a 20k-file library only
    // ever yields a few hundred/thousand analysis candidates, not tens of
    // thousands. OPEN_HAT is included alongside HIHAT because
    // DrumSampleSelector's HAT role draws from both (see its
    // candidatesForRackIds call) - TOM is deliberately excluded, it isn't
    // a role DrumEngine generates for.
    bool isDrumRoleRack(const juce::String& rackId)
    {
        return rackId == "KICK" || rackId == "SNARE" || rackId == "CLAP"
            || rackId == "HIHAT" || rackId == "OPEN_HAT" || rackId == "PERC";
    }
}

DrumSampleIndex::DrumSampleIndex() : juce::Thread("DrumSampleIndex") {}

DrumSampleIndex::~DrumSampleIndex()
{
    stopThread(4000);
}

void DrumSampleIndex::analyzeRacks(const std::vector<Rack>& racks)
{
    if (isThreadRunning())
        stopThread(2000);

    pendingRacks = racks;
    startThread();
}

juce::File DrumSampleIndex::cacheFile() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
               .getChildFile("AbletonCopilot/drum_sample_index_cache.json");
}

void DrumSampleIndex::saveCache(const std::vector<IndexedSample>& samples) const
{
    juce::Array<juce::var> arr;

    for (auto& s : samples)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("path",   s.file.getFullPathName());
        obj->setProperty("rackId", s.rackId);
        obj->setProperty("fileSize", s.file.getSize());
        obj->setProperty("mtimeMs",  s.file.getLastModificationTime().toMilliseconds());

        obj->setProperty("valid",            s.features.valid);
        obj->setProperty("durationSec",      s.features.durationSec);
        obj->setProperty("peakAmplitude",    (double) s.features.peakAmplitude);
        obj->setProperty("rms",              (double) s.features.rms);
        obj->setProperty("attackTimeMs",     (double) s.features.attackTimeMs);
        obj->setProperty("zeroCrossingRate", (double) s.features.zeroCrossingRate);
        obj->setProperty("estimatedPitchHz", (double) s.features.estimatedPitchHz);

        arr.add(juce::var(obj));
    }

    auto f = cacheFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText(juce::JSON::toString(juce::var(arr)));
}

void DrumSampleIndex::run()
{
    // Reload the cache fresh each run (this thread never runs
    // concurrently with itself - analyzeRacks() stops any prior run
    // first - so no locking needed for this local-only state).
    struct CachedRecord { juce::int64 fileSize; juce::int64 mtimeMs; IndexedSample sample; };
    std::map<juce::String, CachedRecord> cached;

    auto f = cacheFile();
    if (f.existsAsFile())
    {
        auto parsed = juce::JSON::parse(f);
        if (parsed.isArray())
        {
            for (auto& entryVar : *parsed.getArray())
            {
                if (threadShouldExit())
                    return;

                auto* obj = entryVar.getDynamicObject();
                if (obj == nullptr)
                    continue;

                const juce::String path = obj->getProperty("path").toString();
                if (path.isEmpty())
                    continue;

                CachedRecord rec;
                rec.fileSize = (juce::int64) (double) obj->getProperty("fileSize");
                rec.mtimeMs  = (juce::int64) (double) obj->getProperty("mtimeMs");
                rec.sample.file   = juce::File(path);
                rec.sample.rackId = obj->getProperty("rackId").toString();

                auto& feat = rec.sample.features;
                feat.valid            = (bool) obj->getProperty("valid");
                feat.durationSec      = (double) obj->getProperty("durationSec");
                feat.peakAmplitude    = (float) (double) obj->getProperty("peakAmplitude");
                feat.rms              = (float) (double) obj->getProperty("rms");
                feat.attackTimeMs     = (float) (double) obj->getProperty("attackTimeMs");
                feat.zeroCrossingRate = (float) (double) obj->getProperty("zeroCrossingRate");
                feat.estimatedPitchHz = (float) (double) obj->getProperty("estimatedPitchHz");

                cached[path] = rec;
            }
        }
    }

    std::vector<IndexedSample> results;

    for (auto& rack : pendingRacks)
    {
        if (threadShouldExit())
            return;
        if (!isDrumRoleRack(rack.id))
            continue;

        for (auto& entry : rack.samples)
        {
            if (threadShouldExit())
                return;

            const juce::String path = entry.file.getFullPathName();
            const juce::int64  size  = entry.file.getSize();
            const juce::int64  mtime = entry.file.getLastModificationTime().toMilliseconds();

            auto it = cached.find(path);
            if (it != cached.end() && it->second.fileSize == size && it->second.mtimeMs == mtime)
            {
                // Unchanged since the last scan - reuse the cached
                // measurement instead of re-decoding/re-analyzing.
                IndexedSample reused = it->second.sample;
                reused.rackId = rack.id;
                results.push_back(std::move(reused));
                continue;
            }

            IndexedSample sample;
            sample.file     = entry.file;
            sample.rackId   = rack.id;
            sample.features = analyzeDrumSample(entry.file);
            results.push_back(std::move(sample));
        }
    }

    if (threadShouldExit())
        return;

    saveCache(results);

    auto callback = onIndexReady;
    juce::MessageManager::callAsync([callback, results]() mutable
    {
        if (callback)
            callback(results);
    });
}
