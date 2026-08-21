#pragma once
#include <JuceHeader.h>
#include <vector>

// Persistent local "Sound DNA" library - real Serum 2 sounds, learned once,
// re-usable forever. Each entry stores a REAL captured Serum 2 state (the
// existing getStateInformation()/setStateInformation() mechanism, see
// PluginProcessor::captureMelodyTrackState/loadCapturedPreset - this file
// does not add a second hosting/capture path) plus real, RENDERED-AUDIO-
// DERIVED measurements at a fixed set of registers/note-lengths/velocities
// (Source/SerumSoundTest.h). Nothing here infers a sound's character from
// its filename - see LearnedSound::status/statusReason for the explicit
// SUCCESS/FAILED/UNKNOWN honesty contract this whole file exists to
// enforce (never silently promoted to SUCCESS).
namespace SoundDna
{
    enum class LearnStatus
    {
        Unknown, // discovered, not yet attempted (or identity/state could not be safely confirmed)
        Failed,  // a real attempt was made and did NOT produce a verified, rendered result
        Success  // a real Serum 2 state was loaded, VERIFIED (see statusReason for how), and rendered
    };

    inline juce::String toString(LearnStatus s)
    {
        switch (s)
        {
            case LearnStatus::Success: return "SUCCESS";
            case LearnStatus::Failed:  return "FAILED";
            default:                   return "UNKNOWN";
        }
    }

    inline LearnStatus statusFromString(const juce::String& s)
    {
        if (s == "SUCCESS") return LearnStatus::Success;
        if (s == "FAILED")  return LearnStatus::Failed;
        return LearnStatus::Unknown;
    }

    // A single real, rendered-audio measurement - every field here is
    // either a direct pass-through of the existing FeatureExtractor's own
    // AudioFeatures (see the comment on each field) or new, simple,
    // self-contained DSP computed directly from the rendered buffer
    // (rms/stereoCorrelation/transientStrength) where FeatureExtractor
    // doesn't already expose it. `valid` false means this specific cell
    // was never actually rendered/measured - callers must not read the
    // other fields as real data in that case.
    struct BandMeasurement
    {
        bool  valid              = false;
        float spectralCentroidHz = 0.0f;  // FeatureExtractor::AudioFeatures.spectralCentroidHz, unchanged
        float subPct             = 0.0f;  // FeatureExtractor, unchanged (20-80 Hz)
        float lowPct             = 0.0f;  // FeatureExtractor, unchanged (80-250 Hz)
        float lowMidPct          = 0.0f;  // FeatureExtractor, unchanged (250-2k Hz)
        float highMidPct         = 0.0f;  // FeatureExtractor, unchanged (2k-8k Hz)
        float airPct             = 0.0f;  // FeatureExtractor, unchanged (8k+ Hz)
        float stereoWidth        = 0.0f;  // FeatureExtractor, unchanged
        float peakDb             = -99.0f;// FeatureExtractor, unchanged
        // New, self-computed this pass (NOT from FeatureExtractor) -
        // see SoundDnaLibrary.cpp's own analyzeBuffer() for the exact
        // formulas.
        float rms                = 0.0f;
        float stereoCorrelation  = 0.0f;  // -1..1, simple Pearson correlation between L/R
        float transientStrength  = 0.0f;  // ratio of the first ~20ms's RMS to the whole render's RMS - higher = more front-loaded/percussive
    };

    struct OctaveResult
    {
        juce::String   label;       // "C1".."C4"
        int            midiPitch = -1;
        BandMeasurement measurement; // rendered at the standard medium length/medium velocity
    };

    struct NoteLengthResult
    {
        juce::String   label;       // "short"/"medium"/"sustained"
        int            lengthSteps = 0;
        BandMeasurement measurement;
        float          releaseTimeMs = 0.0f; // new, self-computed: time from note-off until the render's RMS drops below -40dB of its sustained-portion RMS - only meaningful for the "sustained" cell (see SoundDnaLibrary.cpp)
    };

    struct VelocityResult
    {
        juce::String   label;       // "soft"/"medium"/"hard"
        int            velocity = 0;
        BandMeasurement measurement;
    };

    struct LearnedSound
    {
        juce::String id;                // stable, derived from sourceFingerprint - see makeId()
        juce::String displayName;       // a REAL confirmed name, or "unknown" - NEVER fabricated (see this file's own top comment)
        juce::String role;              // "Bass"/"Melody"/"Pad" - which role category this candidate was discovered under (organizational only, per the user's own "category only organizes the learning set" instruction - NOT a musical judgment)
        juce::String sourceHint;        // human-readable provenance, e.g. "Factory/Bass/Reese/BA - Gnarly Reese.SerumPreset" or "Captured: My Bass Sound"
        juce::String sourceFingerprint; // stable hash of the source (path+size+mtime, or captured-file content hash) - used to skip already-learned sounds and to invalidate an entry if the source changed
        int          analysisVersion = 1;
        juce::String dateLearnedIso;    // ISO8601, real wall-clock time of the actual successful learn
        juce::String capturedStateFile; // filename (not full path) under the library directory holding this sound's real captured Serum 2 state - empty if never captured
        LearnStatus  status = LearnStatus::Unknown;
        juce::String statusReason;      // why - always populated for Failed/Unknown, e.g. "state bytes unchanged after preset-advance (automation likely not trusted)"

        std::vector<OctaveResult>     octaves;      // C1-C4
        std::vector<NoteLengthResult> noteLengths;   // short/medium/sustained
        std::vector<VelocityResult>   velocities;    // soft/medium/hard

        // Derived, measurement-grounded character labels (see
        // SoundDnaLibrary.cpp's deriveDnaLabels()) - every label here must
        // be traceable back to a specific field above; never invented from
        // displayName/sourceHint text.
        juce::StringArray dnaLabels;
    };

    // The real, on-disk root for this library - mirrors the existing
    // CapturedPresets/ convention (same parent directory,
    // ~/Library/AbletonCopilot/).
    juce::File libraryDir();

    // Stable fingerprint for a real on-disk factory preset file (path +
    // size + last-modified, hashed) - changes if the source file changes,
    // which is exactly the "invalidate if the source changed" contract
    // requested.
    juce::String fingerprintForFile(const juce::File& f);

    // Stable fingerprint for an already-captured Serum 2 state (hash of
    // the actual captured bytes) - two different captures of the exact
    // same live state hash identically, which is the correct behaviour.
    juce::String fingerprintForCapturedState(const juce::MemoryBlock& state);

    // Deterministic id derived from a fingerprint - short, filesystem-safe.
    juce::String makeId(const juce::String& fingerprint);

    // Load/save the whole library index as JSON
    // (SoundLibrary/index.json). Returns false (leaving `out` unchanged on
    // load, or not writing on save) only on a real I/O failure - never a
    // partial/corrupt write (writes to a temp file then renames).
    bool loadLibraryIndex(std::vector<LearnedSound>& out);
    bool saveLibraryIndex(const std::vector<LearnedSound>& sounds);

    // Pure (de)serialization, exposed separately from file I/O so it's
    // directly unit-testable.
    juce::var          toVar(const LearnedSound& s);
    LearnedSound        fromVar(const juce::var& v);

    // Runs the existing FeatureExtractor (Source/Analysis/FeatureExtractor.h,
    // already used elsewhere in this codebase for genre analysis) over a
    // real rendered buffer, plus new, simple, self-contained DSP for the
    // three fields FeatureExtractor doesn't already expose
    // (rms/stereoCorrelation/transientStrength - see the .cpp for the
    // exact formulas). `valid=false` (0-sample buffer) returns a
    // default-constructed, `valid=false` BandMeasurement - never a
    // fabricated measurement.
    BandMeasurement analyzeBuffer(const juce::AudioBuffer<float>& rendered, double sampleRate);

    // Measures the release/decay time of a "sustained"-length render:
    // time from the note-off sample until the tail's RMS first drops
    // below -40dB relative to the sustained portion's own RMS (measured
    // in the render's last quarter before note-off, as a strings-free
    // "how loud was it while actually holding" reference level). Returns
    // 0 if the buffer is too short/empty, or -1 if the tail never decays
    // that far within the render (a genuinely very long release - honest,
    // not clamped to a fake number).
    float measureReleaseTimeMs(const juce::AudioBuffer<float>& sustainedRender, double sampleRate,
                                int noteOffSampleIndex);

    // Derives measurement-grounded character labels from everything
    // already stored on `s` (octaves/noteLengths/velocities) - every rule
    // here is documented in the .cpp next to the label it produces, and
    // every label is traceable back to a specific numeric field. Never
    // reads displayName/sourceHint - labels come only from real
    // measurements, per this file's own top comment.
    juce::StringArray deriveDnaLabels(const LearnedSound& s);
}
