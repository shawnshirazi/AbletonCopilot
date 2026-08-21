#include "SoundDnaLibrary.h"
#include "Analysis/FeatureExtractor.h"
#include <cmath>

namespace SoundDna
{
    juce::File libraryDir()
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("AbletonCopilot/SoundLibrary");
    }

    namespace
    {
        // This project's JUCE module set has no juce_cryptography (no
        // SHA256/MD5 available) - juce::String::hashCode64() is not
        // cryptographic, but that's not the requirement here: this only
        // needs to be a STABLE, deterministic identity check ("has the
        // source changed"), not tamper-resistance. Rendered as hex so it
        // reads the same as a real hash would.
        juce::String hex64(int64_t value)
        {
            return juce::String::toHexString((juce::int64) value).paddedLeft('0', 16);
        }
    }

    juce::String fingerprintForFile(const juce::File& f)
    {
        juce::String basis = f.getFullPathName() + "|" + juce::String(f.getSize())
                            + "|" + juce::String(f.getLastModificationTime().toMilliseconds());
        return hex64(basis.hashCode64());
    }

    juce::String fingerprintForCapturedState(const juce::MemoryBlock& state)
    {
        // MemoryBlock has no built-in hash - hash its base64 text
        // representation instead (still fully deterministic for identical
        // bytes).
        return hex64(state.toBase64Encoding().hashCode64());
    }

    juce::String makeId(const juce::String& fingerprint)
    {
        return fingerprint.substring(0, 16);
    }

    //==============================================================================
    namespace
    {
        juce::var bandToVar(const BandMeasurement& m)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty("valid", m.valid);
            o->setProperty("spectralCentroidHz", m.spectralCentroidHz);
            o->setProperty("subPct", m.subPct);
            o->setProperty("lowPct", m.lowPct);
            o->setProperty("lowMidPct", m.lowMidPct);
            o->setProperty("highMidPct", m.highMidPct);
            o->setProperty("airPct", m.airPct);
            o->setProperty("stereoWidth", m.stereoWidth);
            o->setProperty("peakDb", m.peakDb);
            o->setProperty("rms", m.rms);
            o->setProperty("stereoCorrelation", m.stereoCorrelation);
            o->setProperty("transientStrength", m.transientStrength);
            return juce::var(o);
        }

        BandMeasurement bandFromVar(const juce::var& v)
        {
            BandMeasurement m;
            if (!v.isObject()) return m;
            m.valid              = (bool) v.getProperty("valid", false);
            m.spectralCentroidHz = (float) (double) v.getProperty("spectralCentroidHz", 0.0);
            m.subPct             = (float) (double) v.getProperty("subPct", 0.0);
            m.lowPct             = (float) (double) v.getProperty("lowPct", 0.0);
            m.lowMidPct          = (float) (double) v.getProperty("lowMidPct", 0.0);
            m.highMidPct         = (float) (double) v.getProperty("highMidPct", 0.0);
            m.airPct             = (float) (double) v.getProperty("airPct", 0.0);
            m.stereoWidth        = (float) (double) v.getProperty("stereoWidth", 0.0);
            m.peakDb             = (float) (double) v.getProperty("peakDb", -99.0);
            m.rms                = (float) (double) v.getProperty("rms", 0.0);
            m.stereoCorrelation  = (float) (double) v.getProperty("stereoCorrelation", 0.0);
            m.transientStrength  = (float) (double) v.getProperty("transientStrength", 0.0);
            return m;
        }
    }

    juce::var toVar(const LearnedSound& s)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty("id", s.id);
        o->setProperty("displayName", s.displayName);
        o->setProperty("role", s.role);
        o->setProperty("sourceHint", s.sourceHint);
        o->setProperty("sourceFingerprint", s.sourceFingerprint);
        o->setProperty("analysisVersion", s.analysisVersion);
        o->setProperty("dateLearnedIso", s.dateLearnedIso);
        o->setProperty("capturedStateFile", s.capturedStateFile);
        o->setProperty("status", toString(s.status));
        o->setProperty("statusReason", s.statusReason);

        juce::Array<juce::var> octaves;
        for (auto& oc : s.octaves)
        {
            auto* eo = new juce::DynamicObject();
            eo->setProperty("label", oc.label);
            eo->setProperty("midiPitch", oc.midiPitch);
            eo->setProperty("measurement", bandToVar(oc.measurement));
            octaves.add(juce::var(eo));
        }
        o->setProperty("octaves", octaves);

        juce::Array<juce::var> lengths;
        for (auto& nl : s.noteLengths)
        {
            auto* eo = new juce::DynamicObject();
            eo->setProperty("label", nl.label);
            eo->setProperty("lengthSteps", nl.lengthSteps);
            eo->setProperty("measurement", bandToVar(nl.measurement));
            eo->setProperty("releaseTimeMs", nl.releaseTimeMs);
            lengths.add(juce::var(eo));
        }
        o->setProperty("noteLengths", lengths);

        juce::Array<juce::var> velocities;
        for (auto& vel : s.velocities)
        {
            auto* eo = new juce::DynamicObject();
            eo->setProperty("label", vel.label);
            eo->setProperty("velocity", vel.velocity);
            eo->setProperty("measurement", bandToVar(vel.measurement));
            velocities.add(juce::var(eo));
        }
        o->setProperty("velocities", velocities);

        o->setProperty("dnaLabels", s.dnaLabels);

        return juce::var(o);
    }

    LearnedSound fromVar(const juce::var& v)
    {
        LearnedSound s;
        if (!v.isObject()) return s;

        s.id                = v.getProperty("id", "").toString();
        s.displayName       = v.getProperty("displayName", "unknown").toString();
        s.role              = v.getProperty("role", "").toString();
        s.sourceHint        = v.getProperty("sourceHint", "").toString();
        s.sourceFingerprint = v.getProperty("sourceFingerprint", "").toString();
        s.analysisVersion   = (int) v.getProperty("analysisVersion", 1);
        s.dateLearnedIso    = v.getProperty("dateLearnedIso", "").toString();
        s.capturedStateFile = v.getProperty("capturedStateFile", "").toString();
        s.status            = statusFromString(v.getProperty("status", "UNKNOWN").toString());
        s.statusReason      = v.getProperty("statusReason", "").toString();

        if (auto* octaves = v.getProperty("octaves", juce::var()).getArray())
        {
            for (auto& ev : *octaves)
            {
                OctaveResult oc;
                oc.label       = ev.getProperty("label", "").toString();
                oc.midiPitch   = (int) ev.getProperty("midiPitch", -1);
                oc.measurement = bandFromVar(ev.getProperty("measurement", juce::var()));
                s.octaves.push_back(oc);
            }
        }
        if (auto* lengths = v.getProperty("noteLengths", juce::var()).getArray())
        {
            for (auto& ev : *lengths)
            {
                NoteLengthResult nl;
                nl.label         = ev.getProperty("label", "").toString();
                nl.lengthSteps   = (int) ev.getProperty("lengthSteps", 0);
                nl.measurement   = bandFromVar(ev.getProperty("measurement", juce::var()));
                nl.releaseTimeMs = (float) (double) ev.getProperty("releaseTimeMs", 0.0);
                s.noteLengths.push_back(nl);
            }
        }
        if (auto* velocities = v.getProperty("velocities", juce::var()).getArray())
        {
            for (auto& ev : *velocities)
            {
                VelocityResult vel;
                vel.label       = ev.getProperty("label", "").toString();
                vel.velocity    = (int) ev.getProperty("velocity", 0);
                vel.measurement = bandFromVar(ev.getProperty("measurement", juce::var()));
                s.velocities.push_back(vel);
            }
        }
        if (auto* labels = v.getProperty("dnaLabels", juce::var()).getArray())
            for (auto& l : *labels)
                s.dnaLabels.add(l.toString());

        return s;
    }

    //==============================================================================
    bool loadLibraryIndex(std::vector<LearnedSound>& out)
    {
        auto file = libraryDir().getChildFile("index.json");
        if (!file.existsAsFile())
        {
            out.clear();
            return true; // no library yet - not an error
        }

        auto text = file.loadFileAsString();
        juce::var parsed;
        if (juce::JSON::parse(text, parsed).failed())
            return false;

        std::vector<LearnedSound> loaded;
        if (auto* arr = parsed.getArray())
            for (auto& v : *arr)
                loaded.push_back(fromVar(v));

        out = std::move(loaded);
        return true;
    }

    bool saveLibraryIndex(const std::vector<LearnedSound>& sounds)
    {
        auto dir = libraryDir();
        if (!dir.createDirectory().wasOk() && !dir.isDirectory())
            return false;

        juce::Array<juce::var> arr;
        for (auto& s : sounds)
            arr.add(toVar(s));

        auto text = juce::JSON::toString(juce::var(arr));

        // Write to a temp file then rename - never leaves a partial/
        // corrupt index.json if the process is interrupted mid-write
        // (directly serves the "closing halfway through" resumability
        // requirement - a half-written index would otherwise make the
        // NEXT run fail to parse anything at all).
        auto finalFile = dir.getChildFile("index.json");
        auto tempFile  = dir.getChildFile("index.json.tmp");
        if (!tempFile.replaceWithText(text))
            return false;
        return tempFile.moveFileTo(finalFile);
    }

    //==============================================================================
    // Analysis - built on the existing FeatureExtractor, plus new, simple,
    // self-contained DSP for the fields it doesn't already expose.
    namespace
    {
        float computeRms(const juce::AudioBuffer<float>& buf, int startSample, int numSamples)
        {
            if (numSamples <= 0 || buf.getNumChannels() <= 0) return 0.0f;
            double sumSq = 0.0;
            int64_t count = 0;
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            {
                const auto* data = buf.getReadPointer(ch);
                for (int i = startSample; i < startSample + numSamples && i < buf.getNumSamples(); ++i)
                {
                    sumSq += (double) data[i] * (double) data[i];
                    ++count;
                }
            }
            return count > 0 ? (float) std::sqrt(sumSq / (double) count) : 0.0f;
        }

        float computeStereoCorrelation(const juce::AudioBuffer<float>& buf)
        {
            if (buf.getNumChannels() < 2 || buf.getNumSamples() <= 0)
                return 0.0f; // mono or empty - no meaningful correlation

            const auto* l = buf.getReadPointer(0);
            const auto* r = buf.getReadPointer(1);
            const int n = buf.getNumSamples();

            double sumL = 0.0, sumR = 0.0;
            for (int i = 0; i < n; ++i) { sumL += l[i]; sumR += r[i]; }
            const double meanL = sumL / n, meanR = sumR / n;

            double cov = 0.0, varL = 0.0, varR = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double dl = l[i] - meanL, dr = r[i] - meanR;
                cov  += dl * dr;
                varL += dl * dl;
                varR += dr * dr;
            }
            const double denom = std::sqrt(varL * varR);
            return denom > 1.0e-12 ? (float) juce::jlimit(-1.0, 1.0, cov / denom) : 0.0f;
        }
    }

    BandMeasurement analyzeBuffer(const juce::AudioBuffer<float>& rendered, double sampleRate)
    {
        BandMeasurement m;
        if (rendered.getNumSamples() <= 0)
            return m;

        FeatureExtractor extractor;
        const AudioFeatures f = extractor.extract(rendered, sampleRate);
        if (!f.valid)
            return m;

        m.valid              = true;
        m.spectralCentroidHz = f.spectralCentroidHz;
        m.subPct             = f.subPct;
        m.lowPct             = f.lowPct;
        m.lowMidPct          = f.lowMidPct;
        m.highMidPct         = f.highMidPct;
        m.airPct             = f.airPct;
        m.stereoWidth        = f.stereoWidth;
        m.peakDb             = f.peakDb;

        // New this pass, not from FeatureExtractor:
        m.rms               = computeRms(rendered, 0, rendered.getNumSamples());
        m.stereoCorrelation = computeStereoCorrelation(rendered);

        // Transient strength: RMS of roughly the first 20ms vs the whole
        // render's own RMS - a simple, disclosed [HEURISTIC] proxy for
        // "how front-loaded/percussive this render's energy is," not a
        // validated attack-detection algorithm.
        const int earlySamples = juce::jmin(rendered.getNumSamples(), (int) std::lround(0.020 * sampleRate));
        const float earlyRms = computeRms(rendered, 0, earlySamples);
        m.transientStrength = m.rms > 1.0e-6f ? (earlyRms / m.rms) : 0.0f;

        return m;
    }

    float measureReleaseTimeMs(const juce::AudioBuffer<float>& sustainedRender, double sampleRate, int noteOffSampleIndex)
    {
        if (sustainedRender.getNumSamples() <= 0 || sampleRate <= 0.0)
            return 0.0f;
        if (noteOffSampleIndex < 0 || noteOffSampleIndex >= sustainedRender.getNumSamples())
            return 0.0f;

        // Reference level: RMS of the render's last quarter BEFORE
        // note-off (the sustained portion, avoiding the initial attack).
        const int sustainedWindowStart = juce::jmax(0, (int) (noteOffSampleIndex * 0.75));
        const int sustainedWindowLen   = noteOffSampleIndex - sustainedWindowStart;
        const float sustainedRms = computeRms(sustainedRender, sustainedWindowStart, sustainedWindowLen);
        if (sustainedRms <= 1.0e-8f)
            return 0.0f; // essentially silent already while "sustaining" - nothing meaningful to measure

        const float targetRms = sustainedRms * 0.01f; // -40dB
        const int windowSamples = juce::jmax(1, (int) std::lround(0.010 * sampleRate)); // 10ms analysis windows

        for (int pos = noteOffSampleIndex; pos < sustainedRender.getNumSamples(); pos += windowSamples)
        {
            const int len = juce::jmin(windowSamples, sustainedRender.getNumSamples() - pos);
            if (computeRms(sustainedRender, pos, len) <= targetRms)
                return (float) ((pos - noteOffSampleIndex) / sampleRate * 1000.0);
        }
        return -1.0f; // never decayed that far within the rendered tail - a genuinely long release, not clamped to a fake number
    }

    juce::StringArray deriveDnaLabels(const LearnedSound& s)
    {
        juce::StringArray labels;

        // Use the representative (medium/medium) measurement as the
        // primary basis - prefer the note-length "medium" entry (same
        // pitch as the octave sweep's own representative point) if
        // present, else fall back to the first valid octave measurement.
        const BandMeasurement* rep = nullptr;
        for (auto& nl : s.noteLengths)
            if (nl.label == "medium" && nl.measurement.valid) { rep = &nl.measurement; break; }
        if (rep == nullptr)
            for (auto& oc : s.octaves)
                if (oc.measurement.valid) { rep = &oc.measurement; break; }
        if (rep == nullptr)
            return labels; // nothing valid measured - no labels, not guessed ones

        // Brightness [HEURISTIC thresholds, not scientifically validated]
        if (rep->spectralCentroidHz < 800.0f)       labels.add("dark");
        else if (rep->spectralCentroidHz > 2500.0f) labels.add("bright");

        // Dominant frequency band -> sub/low-mid/mid-heavy, airy
        if (rep->subPct > 0.35f)                    labels.add("sub-heavy");
        if (rep->lowMidPct > 0.35f)                  labels.add("low-mid-heavy");
        if (rep->highMidPct > 0.30f)                 labels.add("mid-heavy");
        if (rep->airPct > 0.20f)                     labels.add("airy");

        // warm: real low-mid presence without being dominated by air/high-mid
        if (rep->lowMidPct > 0.25f && rep->airPct < 0.15f && rep->highMidPct < 0.25f)
            labels.add("warm");

        // aggressive/soft: brightness + high-mid/air content together
        if (rep->spectralCentroidHz > 2000.0f && (rep->highMidPct + rep->airPct) > 0.35f)
            labels.add("aggressive");
        else if (rep->spectralCentroidHz < 1200.0f && (rep->highMidPct + rep->airPct) < 0.20f)
            labels.add("soft");

        // wide/narrow: stereoWidth
        if (rep->stereoWidth > 0.5f)      labels.add("wide");
        else if (rep->stereoWidth < 0.1f) labels.add("narrow");

        // plucky/sustained/short: compare "short" vs "sustained" note-length
        // RMS - a patch whose sustained render's RMS is much lower than its
        // short render's RMS decays fast (plucky/short); one that stays
        // close is sustained.
        const NoteLengthResult* shortNl = nullptr;
        const NoteLengthResult* sustainedNl = nullptr;
        for (auto& nl : s.noteLengths)
        {
            if (nl.label == "short" && nl.measurement.valid)     shortNl = &nl;
            if (nl.label == "sustained" && nl.measurement.valid) sustainedNl = &nl;
        }
        if (shortNl != nullptr && sustainedNl != nullptr && shortNl->measurement.rms > 1.0e-6f)
        {
            const float ratio = sustainedNl->measurement.rms / shortNl->measurement.rms;
            if (ratio < 0.4f)      labels.add("plucky");
            else if (ratio > 0.85f) labels.add("sustained");
        }
        if (sustainedNl != nullptr && sustainedNl->releaseTimeMs >= 0.0f && sustainedNl->releaseTimeMs < 150.0f)
            labels.add("short");

        return labels;
    }
}
