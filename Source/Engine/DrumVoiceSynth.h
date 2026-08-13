#pragma once

#include <cstdint>

// Phase 1 deterministic engine: drum voice synthesis.
//
// Pure DSP - zero JUCE dependency (see Theory.h for why), so it's testable
// with the same standalone clang++ harness as Grid/Theory/DrumEngine. Owns
// per-voice timbre/envelope/phase state; PluginProcessor only handles host
// integration (timing, triggering from the sequencer or incoming MIDI, and
// mixing the rendered audio into the output buffer).
//
// No bundled samples - Kick is a pitched/decaying sine, Clap/Hat/Perc are
// filtered noise bursts. Simple and parametric on purpose (real sample
// playback and richer sound design are later upgrades, not this milestone).

namespace Engine
{
    // The four roles DrumEngine generates patterns for (see DrumEngine.h).
    // Values double as array indices - Count is the number of roles, not a
    // real role.
    enum class DrumRole
    {
        Kick  = 0,
        Clap  = 1,
        Hat   = 2,
        Perc  = 3,
        Count = 4
    };

    // Maps a General MIDI drum-map note number to a DrumRole, using the same
    // convention already used for this plugin's MIDI output (Kick=36,
    // Clap=39, Hat=42, Perc=37). Returns false (outRole untouched) if the
    // note isn't one of these four - lets PluginProcessor drive the exact
    // same voices from either the generated pattern or real incoming MIDI
    // note-ons.
    bool drumRoleForGmNote(int midiNote, DrumRole& outRole);

    // Runtime state for a single drum voice (one per role). All state a
    // trigger/render cycle needs lives here, not in the caller, so
    // PluginProcessor just owns an array of these and never touches their
    // internals directly.
    struct DrumVoiceState
    {
        bool     active         = false;
        DrumRole role           = DrumRole::Kick;
        float    velocity       = 1.0f;    // 0..1, fixed for the life of this trigger
        double   sampleRate     = 44100.0;
        int      elapsedSamples = 0;

        double   phase          = 0.0;     // Kick only - running sine phase

        uint32_t noiseState     = 1;       // Clap/Hat/Perc only - xorshift32 state

        // One-pole filter state (Clap/Hat/Perc only). Clap/Perc cascade
        // highpass -> lowpass for a band-passed character; Hat uses only
        // the highpass stage.
        float hpPrevX = 0.0f, hpPrevY = 0.0f;
        float lpPrevY = 0.0f;
    };

    // Resets `voice` to t=0 for the given role/velocity (velocity clamped to
    // 0..1). The SAME function is used whether the trigger came from the
    // deterministic sequencer (a DrumEngine pattern hit) or a live/incoming
    // MIDI note-on - both call sites produce identical voice behaviour, and
    // identical (role, velocity, sampleRate) always produces identical
    // subsequent output (fixed per-role noise seed, no time-based state).
    void triggerDrumVoice(DrumVoiceState& voice, DrumRole role, float velocity, double sampleRate);

    // Renders `numSamples` of mono audio into `out` (overwritten, not
    // mixed - the caller adds it into the real output buffer). If the voice
    // is inactive, `out` is filled with silence and nothing else happens.
    // Advances the voice's internal position; sets `voice.active = false`
    // once its fixed duration has fully decayed (and fills the remainder of
    // `out` with exact silence), so a finished voice costs nothing on
    // subsequent blocks until re-triggered.
    void renderDrumVoice(DrumVoiceState& voice, float* out, int numSamples);
}
