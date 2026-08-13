#pragma once
#include <JuceHeader.h>
#include "Analysis/AudioAnalyzer.h"
#include "Analysis/FeatureExtractor.h"
#include "MelodyCategory.h"

// Parameters for the master-bus correction chain.
// Computed by the editor from analysis results, applied by the processor in processBlock.
struct CorrectionParams
{
    float outputGainDb = 0.0f;  // overall gain to reach target LUFS
    float subBoostDb   = 0.0f;  // peaking EQ at 60 Hz, Q=1.0
    float midBoostDb   = 0.0f;  // peaking EQ at 400 Hz, Q=1.0
    float airBoostDb   = 0.0f;  // high shelf at 10 kHz, Q=0.707
    float widthScale   = 1.0f;  // M/S side multiplier: >1 = wider, <1 = narrower
};

class AbletonCopilotAudioProcessor : public juce::AudioProcessor
{
public:
    AbletonCopilotAudioProcessor();
    ~AbletonCopilotAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
   #endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    AudioAnalyzer& getAnalyzer() noexcept { return analyzer; }

    // While the Advisor tab is listening, our own generated drum/melody
    // audio shouldn't play (or be captured by the analyzer, since it reads
    // the same buffer after this plugin's own output is mixed in) -
    // gates only the output-mixing step, not the sequencer's own
    // triggering/bookkeeping, so nothing about playback state changes when
    // toggled. See PluginEditor::setShowingAdvisor.
    void setSuppressOwnPlayback(bool suppress) noexcept
    {
        suppressOwnPlayback.store(suppress, std::memory_order_relaxed);
    }

    // --- Correction chain (message thread writes, audio thread applies) ---
    void applyCorrections(const CorrectionParams& p);
    void clearCorrections();
    bool  correctionsEnabled() const noexcept { return corrActive.load(); }
    CorrectionParams getCorrections() const;

    // --- Transport state ---
    std::atomic<bool>  playbackJustStopped { false };
    std::atomic<bool>  currentlyPlaying    { false };
    std::atomic<float> currentBpm          { 0.0f  };
    std::atomic<bool>  suppressOwnPlayback { false };
    std::atomic<bool>  genreEqEnabled      { true };

    // --- Live meters ---
    std::atomic<float> liveRmsDb      { -99.0f };
    std::atomic<float> liveStereoWidth { 0.0f  };
    std::atomic<float> livePeakDb     { -99.0f };

    // --- Drum machine: real-time one-shot playback, driven by the editor's
    // step grid and synced to the host's PPQ position (message thread writes,
    // audio thread reads/triggers). 8 bars = 128 steps, assumed 4/4. Row
    // count is dynamic in the UI (one per non-empty sample category, up to
    // 10: Kick/Snare/Clap/Hi-Hat/Cymbal/Perc/Bass/FX/Vocal/Other) — kDrumRows
    // is a fixed upper bound; unused slots just stay silent.
    static constexpr int kDrumRows  = 10;
    static constexpr int kDrumSteps = 128; // must match DrumMachineComponent::kNumSteps

    void setDrumRowSample(int rowIndex, const juce::File& file);
    void setDrumRowStep(int rowIndex, int step, bool isOn);
    void setDrumRowMuted(int rowIndex, bool muted);
    void setDrumRowSolo(int rowIndex, bool solo);

    // --- Hosted Serum2: N independent melodic voices (e.g. Bass + Lead),
    // each its own in-process Serum2 instance (hardcoded search by name for
    // now — see chat) so each plays through its own sound without any
    // manual MIDI-track routing. All active voices mix straight into this
    // plugin's own output, right alongside the drums. kMaxMelodyTracks is a
    // fixed upper bound, same convention as kDrumRows; unused slots are
    // fully skipped (not loaded, not mixed).
    static constexpr int kMelodySteps     = kDrumSteps; // must match DrumMachineComponent::kNumSteps
    static constexpr int kMaxMelodyTracks = 4;

    // Allocates the next melody-voice slot and kicks off loading its Serum2
    // instance. Returns the new track index, or -1 if already at
    // kMaxMelodyTracks.
    int addMelodyTrack();
    int getActiveMelodyTrackCount() const noexcept { return activeMelodyTrackCount.load(std::memory_order_relaxed); }

    void setMelodyPattern(int trackIndex, const std::array<int8_t, kMelodySteps>& offsets, int keyRoot);
    void setMelodyTrackMuted(int trackIndex, bool muted);
    void setMelodyTrackSolo(int trackIndex, bool solo);
    juce::AudioPluginInstance* getHostedSerumInstance(int trackIndex) const noexcept;

    // Rebuilds that voice's static, role-based default EQ (see
    // PluginProcessor.cpp's kVoiceEqSettings) - call whenever a track's
    // category is set (initial default, category switch, or a new track
    // created by generation/layering/hook suggestions).
    void setMelodyTrackCategory(int trackIndex, MelodyCategory category);

    void setGenreEqEnabled(bool enabled) noexcept { genreEqEnabled.store(enabled, std::memory_order_relaxed); }
    bool isGenreEqEnabled() const noexcept { return genreEqEnabled.load(std::memory_order_relaxed); }

    // Preset capture: Serum2's real VST3 state ("VC2!" + XML — confirmed via
    // a diagnostic dump, nothing to do with .fxp/VST2 chunk bytes, which is
    // why the earlier .fxp-based loader never actually worked) captured
    // directly from a sound the user picked in Serum2's own browser, and
    // fed straight back later — no format translation, so it's guaranteed
    // compatible. No-op (returns false) if that track's Serum2 hasn't
    // finished loading.
    bool captureMelodyTrackState(int trackIndex, juce::MemoryBlock& outState) const;
    bool loadCapturedPreset(int trackIndex, const juce::File& captureFile);

    juce::String getMelodyTrackStatus(int trackIndex) const;
    bool         isMelodyTrackLoaded(int trackIndex) const;

    // --- Deterministic drum-MIDI output (Phase 1 engine, Source/Engine/) ---
    // Emits real MIDI notes to the host (General MIDI drum map, channel 10)
    // so a downstream instrument (e.g. a Drum Rack on the same MIDI track)
    // can play them - independent of the kDrumRows/kDrumSteps sample-based
    // drum machine above, which stays exactly as it was. Step count comes
    // from whatever grid the pattern was generated with (Engine::totalSteps),
    // not tied to the fixed kDrumSteps=128 (8-bar) grid above - this is how
    // the product default became 16 bars without hardcoding it here too.
    static constexpr int kMaxGeneratedDrumRoles = 4;

    struct GeneratedDrumRole
    {
        int              midiNote = -1;   // -1 = unused slot
        std::vector<int> velocity;        // sized to the pattern's totalSteps; 0 = no hit, 1-127 = velocity
    };

    // Replaces the current generated drum pattern (message thread only -
    // called from PluginEditor's Generate Drum Pattern button). At most
    // kMaxGeneratedDrumRoles entries are used; extras are ignored.
    void setGeneratedDrumPattern(const std::vector<GeneratedDrumRole>& roles);

private:
    void loadSerum();

    class SerumLoaderThread : public juce::Thread
    {
    public:
        explicit SerumLoaderThread(AbletonCopilotAudioProcessor& o)
            : juce::Thread("SerumLoader"), owner(o) {}
        void run() override;
    private:
        AbletonCopilotAudioProcessor& owner;
    };
    friend class SerumLoaderThread;


    void rebuildCorrFilters(double sampleRate, const CorrectionParams& p);

    AudioAnalyzer analyzer;

    struct DrumVoice
    {
        int readPos       = -1; // -1 = silent
        int lastStepIndex = -1;
    };

    juce::AudioFormatManager                   drumFormatManager;
    juce::CriticalSection                      drumBufferLock;
    std::shared_ptr<juce::AudioBuffer<float>>  drumRowBuffers[kDrumRows];
    std::atomic<bool>                          drumSteps[kDrumRows][kDrumSteps];
    std::atomic<bool>                          drumRowMuted[kDrumRows];
    std::atomic<bool>                          drumRowSolo[kDrumRows];
    std::atomic<int>                           soloedRowCount { 0 };
    DrumVoice                                  drumVoices[kDrumRows];

    // Hosted Serum2 instances — one per melody voice (message-thread owns
    // each unique_ptr; audio thread only ever reads the raw atomic pointer,
    // never deletes it). One shared format manager/loader thread/alive-flag
    // serves all voices — JUCE plugin formats support multiple simultaneous
    // instances of the same plugin file, this is normal VST3 hosting, not a
    // workaround.
    static constexpr int8_t kMelodyOffValue = -128;

    struct MelodyVoice
    {
        std::unique_ptr<juce::AudioPluginInstance> ownerInstance;   // message-thread only
        std::atomic<juce::AudioPluginInstance*>    instance { nullptr };
        juce::AudioBuffer<float>                   scratch;
        juce::MemoryBlock                          pendingState;   // stashed until Serum2 finishes loading

        juce::CriticalSection                      lock;           // guards offsets/keyRoot
        std::array<int8_t, kMelodySteps>           offsets;
        int                                        keyRoot = 0;

        // Audio-thread-only playback state.
        int  lastStepIndex  = -1;
        bool noteOn         = false;
        int  soundingPitch  = -1;

        std::atomic<bool> active        { false }; // false = unused slot, fully skipped
        std::atomic<bool> loadAttempted { false };
        std::atomic<bool> loaded        { false };
        std::atomic<bool> muted         { false };
        std::atomic<bool> solo          { false };

        // Static, role-based default EQ (see setMelodyTrackCategory) — a
        // high-pass + shelf pair matching real mixing convention per
        // instrument category, applied to this voice's own output before
        // it's mixed with anything else. Audio-thread reads only; rebuilt
        // on the message thread whenever the track's category changes.
        FeatureExtractor::Biquad eqHp[2], eqShelf[2];
        std::atomic<bool>        eqActive { true }; // false for Fx/Other - no strong convention

        mutable juce::CriticalSection statusLock;
        juce::String                  statusMessage { "Not loaded" };

        MelodyVoice() { offsets.fill(kMelodyOffValue); }
    };

    juce::AudioPluginFormatManager     pluginFormatManager;
    std::shared_ptr<std::atomic<bool>> aliveFlag = std::make_shared<std::atomic<bool>>(true);
    SerumLoaderThread                  serumLoader { *this };
    MelodyVoice                        melodyVoices[kMaxMelodyTracks];
    std::atomic<int>                   activeMelodyTrackCount { 0 };
    std::atomic<int>                   soloedMelodyTrackCount { 0 };

    // Audio-thread-only state
    bool  wasPlaying    = false;
    float rmsSmoothed   = 0.0f;
    float widthSmoothed = 0.0f;

    // Correction DSP state (audio-thread-only after rebuild)
    using Biquad = FeatureExtractor::Biquad;
    Biquad corrSub[2];    // peaking EQ at 60 Hz
    Biquad corrMid[2];    // peaking EQ at 400 Hz
    Biquad corrAir[2];    // high shelf at 10 kHz
    float  corrGainLin  = 1.0f;
    float  corrWidthMul = 1.0f;

    // Cross-thread correction params
    CorrectionParams      corrPending;
    juce::CriticalSection corrLock;
    std::atomic<bool>     corrActive { false };
    std::atomic<bool>     corrDirty  { false };

    // Deterministic drum-MIDI output state (see setGeneratedDrumPattern
    // above). generatedDrum* fields guarded by generatedDrumLock (message
    // thread writes, audio thread reads a local copy each block); the
    // per-role gate-voice state below is audio-thread-only, same split as
    // the existing DrumVoice/drumBufferLock pattern above.
    juce::CriticalSection            generatedDrumLock;
    std::vector<GeneratedDrumRole>   generatedDrumRoles;      // guarded by generatedDrumLock
    int                              generatedDrumTotalSteps = 0; // guarded by generatedDrumLock

    struct GeneratedDrumVoiceState
    {
        bool noteOn          = false;
        int  pitch           = -1;
        int  samplesUntilOff = 0;
    };
    GeneratedDrumVoiceState generatedDrumVoices[kMaxGeneratedDrumRoles];
    int                     generatedDrumLastStepIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AbletonCopilotAudioProcessor)
};
