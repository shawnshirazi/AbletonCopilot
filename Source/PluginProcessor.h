#pragma once
#include <JuceHeader.h>
#include "Analysis/AudioAnalyzer.h"
#include "Analysis/FeatureExtractor.h"
#include "MelodyCategory.h"
#include "Engine/DrumVoiceSynth.h"
#include "StartupTiming.h"
#include "DrumStemExporter.h"

// Register clamp - octave-WRAPS (not truncates) a raw computed pitch into
// a fixed register regardless of key, fixing the diagnosed register-drift
// bug (keyRoot(0-11) + offset combining into the same base with no clamp
// lets the SAME archetype/seed land anywhere across several octaves purely
// from key choice). Wrapping by octave preserves which note plays (the
// pitch class) while forcing a consistent register. Parameterized (not
// bass-specific) so both bass and melody can each use their own band -
// see clampBassRegisterPitch/clampMelodyRegisterPitch below, and
// PluginProcessor.cpp's melody-voice trigger loop for where each is
// actually applied per track.
inline int clampRegisterPitch(int rawPitch, int minPitch, int maxPitch)
{
    while (rawPitch > maxPitch) rawPitch -= 12;
    while (rawPitch < minPitch) rawPitch += 12;
    return rawPitch;
}

// Bass register clamp - see MLPipeline/musical_target/
// melodic_techno_research.md section 11.5 for the real absolute-register
// evidence this band is anchored to (F1-C3). Shared between
// PluginProcessor's real trigger loop and PluginEditor's diagnostics so
// the displayed range can never drift out of sync with what's actually
// played.
inline int clampBassRegisterPitch(int rawPitch)
{
    constexpr int kBassRegisterMin = 29; // F1
    constexpr int kBassRegisterMax = 48; // C3
    return clampRegisterPitch(rawPitch, kBassRegisterMin, kBassRegisterMax);
}

// Melody register clamp - same register-drift bug as bass (see above),
// previously unfixed for melody (processBlock's trigger loop only ever
// applied clampBassRegisterPitch for track 0). No measured melody-register
// corpus exists (melodic_techno_research.md section 7 - external guidance
// only, LOW confidence) so this band is an [INTERPRETATION], not a
// corpus-derived number: C3-C5, starting exactly where the bass band tops
// out (C3), giving melody two full octaves of clearly-above-the-bass
// space rather than a number picked with no reasoning at all.
inline int clampMelodyRegisterPitch(int rawPitch)
{
    constexpr int kMelodyRegisterMin = 48; // C3
    constexpr int kMelodyRegisterMax = 72; // C5
    return clampRegisterPitch(rawPitch, kMelodyRegisterMin, kMelodyRegisterMax);
}

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

    // Generated-loop pattern (Source/Engine/MusicIdentity.h for drums/bass,
    // Source/MusicTheory/MelodyMotifGenerator.h for melody) - a SEPARATE,
    // dynamically-sized path from setMelodyPattern above, mirroring how
    // setGeneratedDrumPattern already coexists with the fixed-size manual
    // drum grid below rather than reusing it. kMelodySteps is fixed at 128
    // (8 bars) because MelodyGridComponent's manual-editing UI and its
    // reference-track bass-fragment-matching feature both depend on that
    // exact size - the 16-bar (256-step) loop this drives is longer, so it
    // needs its own container rather than forcing those unrelated features
    // to grow (or truncating the loop to fit them). Empty `offsets`
    // deactivates this path and falls back to the normal setMelodyPattern/
    // manual-editing behaviour for that track - nothing about the existing
    // melody-voice trigger path changes for a track that never calls this.
    // Used for BOTH track 0 (Bass) and track 1 (Melody) - one mechanism,
    // not two independent ones, per the "don't build separate generation
    // systems" rule. gateLengthSteps is optional (default empty - the
    // melody call site never passes one, so its playback is completely
    // unaffected) - see MelodyVoice::generatedGateLengthSteps/
    // gateSamplesRemaining for what actually makes a non-empty one
    // audible (Source/Engine/BassArchetype.h's archetype note lengths).
    void setGeneratedMelodyPattern(int trackIndex, const std::vector<int8_t>& offsets, int keyRoot,
                                    const std::vector<int8_t>& gateLengthSteps = {});
    void setMelodyTrackMuted(int trackIndex, bool muted);
    void setMelodyTrackSolo(int trackIndex, bool solo);
    juce::AudioPluginInstance* getHostedSerumInstance(int trackIndex) const noexcept;

    // Starts/stops looping playback of trackIndex's ALREADY-STORED
    // generated pattern (whatever setGeneratedMelodyPattern last set -
    // never a separate test pattern, never regenerated, never modified by
    // this call) using an internal sample clock independent of the host
    // transport - so browsing/auditioning Serum 2 sounds against the real
    // generated material works whether or not the DAW's transport happens
    // to be playing (processBlock is still called continuously by the
    // host/standalone wrapper either way - only note-TRIGGERING was ever
    // gated on isPlayingNow). Bypasses that voice's own mute while active
    // (auditioning must always be audible - see processBlock's own
    // comment); never touches solo state, never touches any other track,
    // never modifies the stored pattern. Starting audition resets that
    // voice's internal clock back to step 0 (see processBlock's
    // auditionStartPending handling) so repeated auditions - e.g. after
    // tweaking the Serum 2 preset - always start from the same point for
    // a fair comparison.
    void setVoiceAuditionActive(int trackIndex, bool active);
    bool isVoiceAuditionActive(int trackIndex) const noexcept;

    // Offline (message-thread, non-realtime) render of trackIndex's
    // ALREADY-STORED generated pattern through its Serum2 instance IN
    // ISOLATION - never touches drums, never touches any other track,
    // never touches live mute/solo state, never regenerates or modifies
    // anything. This is what a candidate-evaluation pass renders and
    // hands to FeatureExtractor for real spectral/register/stereo-width
    // analysis (see Source/CandidateEvaluator.h) - "does this sound good
    // in this exact musical context" answered from real rendered audio,
    // not metadata. Returns an empty buffer (0 samples) if this track has
    // no loaded Serum2 instance yet or no generated pattern stored -
    // never a guess, never silence pretending to be a real render.
    // `registerOctaveShift` transposes every triggered note by that many
    // OCTAVES (12 semitones each) before the existing per-role clamp is
    // applied - lets a caller evaluate "this preset one octave down"
    // without touching the real generated pattern or the live register-
    // clamp logic used during actual playback.
    juce::AudioBuffer<float> renderVoiceAuditionAudio(int trackIndex, int numBars, int registerOctaveShift = 0);

    // Offline (message-thread, non-realtime) render of a SINGLE
    // deterministic note - NOT this track's generated pattern - through
    // that track's Serum2 instance in isolation. This is the Sound DNA
    // library's standardized test primitive (Source/SerumSoundTest.h):
    // whatever REAL Serum 2 state is currently loaded on this track gets
    // played the exact same fixed pitch/length/velocity every time, so
    // results are comparable across candidates. Renders `lengthSteps` of
    // held note plus `releaseTailSteps` more of ringing-out afterward (so
    // release/decay behaviour is actually captured). Never touches the
    // stored generated pattern, never touches any other track. Returns an
    // empty buffer if this track has no loaded Serum2 instance.
    juce::AudioBuffer<float> renderStandardizedNote(int trackIndex, int midiPitch, int lengthSteps,
                                                      int velocity, int releaseTailSteps);

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
    bool captureMelodyTrackState(int trackIndex, juce::MemoryBlock& outState);
    bool loadCapturedPreset(int trackIndex, const juce::File& captureFile);

    juce::String getMelodyTrackStatus(int trackIndex) const;

    // Real runtime proof for the bass-audibility investigation - every
    // field is read from state actually set by processBlock/
    // setGeneratedMelodyPattern, never inferred or assumed. noteOnEventsSent
    // and lastBlockPeakOut only advance while the host transport is
    // running (processBlock only runs then) - see PluginEditor's own
    // status text for that caveat.
    struct MelodyVoiceDiagnostics
    {
        bool    serumInstanceLoaded    = false; // voice.instance != nullptr right now
        bool    generatedPatternActive = false; // setGeneratedMelodyPattern has a non-empty pattern stored for this track
        int     generatedTotalSteps    = 0;
        // noteOnEventsSent/noteOffEventsSent count real MIDI events built
        // into this voice's own serumMidi buffer each block - this happens
        // unconditionally, independent of whether a Serum2 instance has
        // finished loading yet (see processBlock's melody-voice loop: the
        // `serum == nullptr` check only gates actually rendering audio
        // through the instance, not building/counting the MIDI itself) -
        // so these prove the MIDI pattern is real even before/without a
        // loaded instrument.
        int64_t noteOnEventsSent       = 0;
        int64_t noteOffEventsSent      = 0;
        float   lastBlockPeakOut       = 0.0f;  // peak |sample| in this voice's own scratch buffer AFTER serum->processBlock(), BEFORE any downstream suppression/mixing gate - proves whether Serum's own output is silent independent of whether it reaches the main mix
        bool    suppressOwnPlayback    = false; // the shared gate that silences ALL melody-voice (and drum) output when true - see setSuppressOwnPlayback
        int     generatedGateLengthCount = 0;   // number of non-zero entries in generatedGateLengthSteps - proves setGeneratedMelodyPattern's optional gate array actually reached this voice (0 for every track that never passed one, e.g. Melody)

        // True ONLY immediately after loadCapturedPreset() successfully
        // called setStateInformation() on a real, loaded Serum2 instance -
        // never set from UI-side bookkeeping (a preset FILE having been
        // selected/cycled to is NOT the same as it actually being loaded).
        // False on construction, and false again after a host-restore
        // (setStateInformation via the project-load path) applies a
        // pendingState blob, since that blob has no known on-disk preset
        // name to honestly report. The UI must never display "(captured)"
        // or a preset name without checking this first - see
        // PluginEditor's serumStatusFor/updateTrackTitle.
        bool    capturedPresetActive   = false;

        // True iff a host-restored state blob (setStateInformation's
        // project-load path) has ever been queued for this voice - see
        // MelodyVoice::pendingState. Distinct from capturedPresetActive:
        // a host restore genuinely changed this voice's live Serum2 sound
        // (it's real, applied state), it just has no on-disk preset NAME
        // to honestly report - so capturedPresetActive correctly stays
        // false for it, but callers that would otherwise auto-apply a
        // guessed on-disk capture (see PluginEditor's timerCallback) must
        // check this first and skip doing so, or they'd silently
        // overwrite a legitimate project-restored sound with an unrelated
        // guess. Never cleared once set - once true for a voice, stays
        // true for that voice's lifetime.
        bool    hostStateRestored      = false;

        // Mirrors voice.auditionActive - read from the processor, not
        // UI-side button-toggle bookkeeping, so a test/caller can confirm
        // audition state actually took effect.
        bool    auditionActive         = false;
    };
    MelodyVoiceDiagnostics getMelodyVoiceDiagnostics(int trackIndex) const;
    bool         isMelodyTrackLoaded(int trackIndex) const;

    // Test/diagnostic-only: a single recorded note-on/off event actually
    // built into a melody voice's own serumMidi buffer during processBlock
    // (see that loop's own comment on why this is captured unconditionally,
    // independent of whether a Serum2 instance is loaded), with a sample-
    // accurate ABSOLUTE timestamp - this voice's own running total of
    // samples actually processed, not a per-block-relative offset. Exists
    // so tests can prove real event ORDERING/timing (e.g. "a retrigger's
    // note-off strictly precedes its note-on"), not just cumulative
    // counts (noteOnEventsSent/noteOffEventsSent above already cover the
    // count side).
    struct VoiceMidiEventRecord
    {
        bool    isNoteOn = false;
        int     pitch = -1;
        int64_t absoluteSample = 0;
    };
    // Up to the most recent kMaxRecentVoiceMidiEvents events recorded for
    // this voice, oldest first - real events only, built from the exact
    // juce::MidiBuffer each block actually assembled for this voice, never
    // reconstructed or inferred after the fact.
    static constexpr int kMaxRecentVoiceMidiEvents = 8192;
    std::vector<VoiceMidiEventRecord> getRecentVoiceMidiEvents(int trackIndex) const;

    // --- Deterministic drum-MIDI output (Phase 1 engine, Source/Engine/) ---
    // Emits real MIDI notes to the host (General MIDI drum map, channel 10)
    // so a downstream instrument (e.g. a Drum Rack on the same MIDI track)
    // can play them - independent of the kDrumRows/kDrumSteps sample-based
    // drum machine above, which stays exactly as it was. Step count comes
    // from whatever grid the pattern was generated with (Engine::totalSteps),
    // not tied to the fixed kDrumSteps=128 (8-bar) grid above - this is how
    // the product default became 16 bars without hardcoding it here too.
    static constexpr int kMaxGeneratedDrumRoles = 6;

    struct GeneratedDrumRole
    {
        int              midiNote = -1;   // -1 = unused slot
        std::vector<int> velocity;        // sized to the pattern's totalSteps; 0 = no hit, 1-127 = velocity
        // Sample chosen for this role by the sample-selection layer (see
        // Source/DrumSampleSelector.h) - an invalid/empty File means no
        // suitable sample was found in the user's library for this role,
        // in which case this role falls back to DrumVoiceSynth (see
        // processBlock). DrumEngine itself never sets this - it stays
        // default (empty) unless PluginEditor's sample-selection step
        // fills it in, keeping WHEN/WHERE/velocity (DrumEngine) separate
        // from WHAT SOUND (this).
        juce::File       sampleFile;
    };

    // Replaces the current generated drum pattern (message thread only -
    // called from PluginEditor's Generate button). At most
    // kMaxGeneratedDrumRoles entries are used; extras are ignored.
    void setGeneratedDrumPattern(const std::vector<GeneratedDrumRole>& roles);

    // Per-role MIX control (not a generation control) for the generated
    // drum pattern - see Engine::RoleMix/renderMode (Source/Engine/
    // MusicIdentity.h) for how DROP/BREAKDOWN suggest defaults for these.
    // Gates ONLY the triggering of new notes (same idiom already used for
    // drumRowMuted/MelodyVoice::muted below - see processBlock's trigger
    // loop) - never touches generatedDrumRoles/generatedRoleSampleBuffers,
    // so muting/unmuting or changing gain can never regenerate the
    // pattern, change sample selection, or destroy the grid. `role` is an
    // Engine::DrumRole ordinal (0..kMaxGeneratedDrumRoles-1).
    void setGeneratedDrumRoleMuted(int role, bool muted);
    void setGeneratedDrumRoleGain(int role, float gain);

    // The file ACTUALLY loaded into the playback voice for `role` right
    // now - an invalid File() means that role is currently sounding via
    // DrumVoiceSynth (no usable sample loaded), not a guess or the sample
    // selector's original suggestion. Message-thread callers (the editor's
    // status display) can call this immediately after
    // setGeneratedDrumPattern() returns - that call is fully synchronous,
    // so this is always up to date by the time it returns.
    juce::File getGeneratedRoleLoadedFile(Engine::DrumRole role) const;

    // Bug-hunting diagnostic (temporary, per the "prove the runtime audio
    // path" milestone): every step setGeneratedDrumPattern() actually
    // checked for `role`'s candidate, captured at the exact point each
    // check happened - not reconstructed after the fact, so it can't lie
    // about what really occurred during loading.
    struct GeneratedRoleLoadDiagnostics
    {
        juce::File   candidateFile;              // the sample-selection layer's suggestion, unmodified
        bool         candidateExists      = false; // candidateFile.existsAsFile() at load time
        juce::String extension;                  // candidateFile.getFileExtension()
        bool         formatRecognized     = false; // a registered AudioFormat claims this extension
        juce::String recognizedFormatName;        // e.g. "WAV file", empty if formatRecognized is false
        bool         readerCreated        = false; // drumFormatManager.createReaderFor() returned non-null
        juce::int64  decodedLengthSamples = 0;     // reader->lengthInSamples, only meaningful if readerCreated
        juce::File   finalLoadedFile;              // == candidateFile iff every step above succeeded; invalid File() otherwise
    };
    GeneratedRoleLoadDiagnostics getGeneratedRoleLoadDiagnostics(Engine::DrumRole role) const;

    // Read-only snapshot of exactly what's currently stored for the
    // generated drum pattern (pattern/sample buffers/loaded files/mute/
    // gain) shaped for DrumStemExporter::renderDrumStems. Takes
    // generatedDrumLock and generatedSampleLock briefly to copy out
    // shared_ptr/File/vector data, then releases them - never mutates any
    // of that state, never regenerates anything, so calling this (i.e.
    // exporting stems) can never change what's already playing or what a
    // later Generate click would use. `bpm` is passed in by the caller
    // (the editor's own currentIdentity->bpm, the actual generation bpm)
    // rather than read from any live-transport cache here, so a snapshot
    // taken before the host transport has ever run is still correct.
    DrumStemExporter::Input getGeneratedDrumStemSnapshot(double bpm) const;

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

    // Triggers whichever voice actually plays `role` right now: a real
    // sample if the sample-selection layer found and loaded one (see
    // setGeneratedDrumPattern/Source/DrumSampleSelector.h), otherwise
    // DrumVoiceSynth - the fallback stays fully available, not bypassed
    // when samples exist elsewhere in the pattern. Shared by both trigger
    // sites in processBlock (the generated-pattern sequencer and incoming
    // MIDI note-ons) so they always agree on which voice a given role
    // uses. Audio-thread only.
    void triggerGeneratedRole(Engine::DrumRole role, float velocity01);

    // Actually fires a generated-pattern role's note: real sample/synth
    // audio via triggerGeneratedRole, plus the optional secondary MIDI
    // noteOn/noteOff bookkeeping (generatedDrumVoices[r]) - the exact body
    // that used to be inline at the step-detection point, factored out so
    // both an immediate (non-swung) trigger and a swing-delayed one
    // (generatedSwingTriggers' own countdown, see processBlock) call the
    // same code instead of two copies. Audio-thread only.
    void fireGeneratedRoleNote(int r, int midiNote, float velocity01, juce::MidiBuffer& midiMessages);

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

    // Per-generated-drum-role MIX state (see setGeneratedDrumRoleMuted/
    // setGeneratedDrumRoleGain) - indexed by Engine::DrumRole ordinal,
    // same sizing/indexing as generatedDrumRoles below. Gates triggering
    // only; never touched by setGeneratedDrumPattern.
    std::atomic<bool>                          generatedDrumRoleMuted[kMaxGeneratedDrumRoles];
    std::atomic<float>                         generatedDrumRoleGain[kMaxGeneratedDrumRoles];

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

        juce::CriticalSection                      lock;           // guards offsets/keyRoot/generatedOffsets/generatedTotalSteps
        std::array<int8_t, kMelodySteps>           offsets;
        int                                        keyRoot = 0;

        // Arrangement-driven generated pattern (see setGeneratedMelodyPattern) -
        // empty = inactive, this track plays `offsets`/kMelodySteps as
        // normal. Non-empty overrides offsets entirely for this track
        // (see processBlock's melody-voice trigger loop) until cleared.
        std::vector<int8_t>                        generatedOffsets;
        int                                         generatedTotalSteps = 0;

        // Audition state (see setVoiceAuditionActive's own comment).
        // auditionActive/auditionStartPending are the only fields the
        // message thread writes here; auditionSampleCounter is audio-
        // thread-only (reset by the audio thread itself when it observes
        // auditionStartPending, never written directly from the message
        // thread, so there's no data race on a non-atomic field).
        std::atomic<bool> auditionActive        { false };
        std::atomic<bool> auditionStartPending  { false };
        int64_t            auditionSampleCounter = 0;

        // Real gate-length data (Source/Engine/BassArchetype.h's archetype
        // note lengths) - empty = no explicit gate anywhere, exactly the
        // previous behaviour (a note rings until the next onset
        // unconditionally retriggers it). Non-empty and non-zero at a
        // given onset step means "cut this note short after N steps if
        // the next onset hasn't already done so" - see processBlock's
        // gateSamplesRemaining countdown, the same decrement-to-zero idiom
        // already used for generated-drum note-off/swing scheduling.
        std::vector<int8_t>                        generatedGateLengthSteps;
        int                                         gateSamplesRemaining = -1; // audio-thread only; -1 = no gate armed for the currently-sounding note

        // Test/diagnostic-only (see getRecentVoiceMidiEvents's own
        // comment). Audio thread appends under midiEventLock each block;
        // message thread reads a copy. processedSampleCount is this
        // voice's own running total of samples actually processed - audio
        // thread only, advanced once per processBlock call regardless of
        // whether any event occurred that block.
        mutable juce::CriticalSection              midiEventLock;
        std::vector<VoiceMidiEventRecord>           recentMidiEvents;
        int64_t                                     processedSampleCount = 0;

        // Runtime proof for getMelodyVoiceDiagnostics() - written from the
        // audio thread in processBlock, read from the message thread.
        // noteOnEventsSent counts real noteOn MIDI events actually queued
        // into this voice's Serum2 instance (not just "would have been
        // sent if audible"); lastBlockPeakOut is the peak |sample| in this
        // voice's own scratch buffer straight out of
        // serum->processBlock(), before the EQ/suppression/mix stage - so
        // it proves whether SERUM'S OWN OUTPUT is silent, independent of
        // whether anything downstream would have blocked it from reaching
        // the main output.
        std::atomic<int64_t> noteOnEventsSent { 0 };
        std::atomic<int64_t> noteOffEventsSent { 0 };
        std::atomic<float>   lastBlockPeakOut { 0.0f };

        // Audio-thread-only playback state.
        int  lastStepIndex  = -1;
        bool noteOn         = false;
        int  soundingPitch  = -1;

        std::atomic<bool> active        { false }; // false = unused slot, fully skipped
        std::atomic<bool> loadAttempted { false };
        std::atomic<bool> loaded        { false };
        std::atomic<bool> muted         { false };
        std::atomic<bool> solo          { false };

        // True ONLY right after loadCapturedPreset() successfully applies
        // real captured state to a loaded Serum2 instance - see
        // MelodyVoiceDiagnostics::capturedPresetActive for the full
        // honesty contract this exists to support.
        std::atomic<bool> capturedPresetActive { false };

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

    // Real swing timing (Part 4 of the groove-improvement pass): hatClosed/
    // percA/percB's off-beat 16th positions are delayed a small, sourced,
    // deterministic amount (see kSwingAmount and Engine::stepTimeSeconds,
    // Source/Engine/Grid.h - that swing-aware timing math already existed,
    // this is what actually wires it to real playback). Kick/clap/bass
    // always trigger exactly on the block-boundary detection below,
    // unchanged. A swung hit is queued here instead of firing immediately,
    // then counted down block-by-block the same way
    // GeneratedDrumVoiceState::samplesUntilOff already counts down note-off
    // - the established idiom in this file for "an event that must land at
    // a specific future sample, not immediately," not a new mechanism.
    struct PendingSwingTrigger
    {
        bool  pending          = false;
        int   midiNote         = -1;
        float velocity01       = 0.0f;
        int   samplesRemaining = 0;
    };
    PendingSwingTrigger generatedSwingTriggers[kMaxGeneratedDrumRoles];

    // Internal audio synthesis (Source/Engine/DrumVoiceSynth.h) - the
    // primary way to hear the generated pattern; the MIDI-out gate state
    // above stays available as an optional secondary output, but nothing
    // downstream is required to hear these. One voice per role, indexed by
    // Engine::DrumRole (not by generatedDrumRoles' array position), so both
    // the sequencer below and incoming MIDI note-ons (see processBlock)
    // trigger the exact same voice for a given GM drum note. Audio-thread
    // only; sized in prepareToPlay.
    static_assert(kMaxGeneratedDrumRoles == (int) Engine::DrumRole::Count,
                  "one synth voice per DrumRole");
    Engine::DrumVoiceState   generatedDrumSynthVoices[kMaxGeneratedDrumRoles];
    juce::AudioBuffer<float> generatedDrumScratch; // mono, reused for per-voice rendering before mixing

    // Sample-based playback for the generated pattern (see
    // Source/DrumSampleSelector.h) - preferred over DrumVoiceSynth
    // per-role whenever a real sample was found/loaded for that role;
    // DrumVoiceSynth stays the fallback for any role without one, not
    // deleted or bypassed. Same guarded-by-lock / audio-thread-only split
    // as drumRowBuffers/DrumVoice above, and loaded through the same
    // drumFormatManager - no second sample-loading system.
    juce::CriticalSection                     generatedSampleLock;
    std::shared_ptr<juce::AudioBuffer<float>> generatedRoleSampleBuffers[kMaxGeneratedDrumRoles]; // guarded by generatedSampleLock; nullptr = use DrumVoiceSynth for this role
    // The file that ACTUALLY produced generatedRoleSampleBuffers[r] above -
    // set at the exact same time as the buffer, in setGeneratedDrumPattern().
    // This is deliberately separate from GeneratedDrumRole::sampleFile (the
    // sample-selection layer's suggestion): if that file fails to decode,
    // the buffer stays null and this stays an invalid File(), so
    // getGeneratedRoleLoadedFile() always reflects reality, never intent.
    juce::File generatedRoleLoadedFile[kMaxGeneratedDrumRoles]; // guarded by generatedSampleLock
    GeneratedRoleLoadDiagnostics generatedRoleLoadDiagnostics[kMaxGeneratedDrumRoles]; // guarded by generatedSampleLock

    struct GeneratedSampleVoiceState
    {
        int   readPos = -1; // -1 = silent
        float gain    = 1.0f; // velocity/127, applied on trigger
    };
    GeneratedSampleVoiceState generatedSampleVoices[kMaxGeneratedDrumRoles];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AbletonCopilotAudioProcessor)
};
