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
// No bundled samples - Kick is a pitched/decaying sine, everything else is
// a filtered noise burst (shaped per role below). Simple and parametric on
// purpose (real sample playback and richer sound design are later
// upgrades, not this milestone).

namespace Engine
{
    // The six roles DrumEngine generates patterns for (see DrumEngine.h) -
    // expanded from the earlier four-role (Kick/Clap/Hat/Perc) design into
    // a layered hat hierarchy (closed pulse vs. open/ride accent) and two
    // independent percussion voices (so two different library samples can
    // play complementary, non-duplicate motifs - see DrumSampleSelector.h).
    // Values double as array indices - Count is the number of roles, not a
    // real role.
    enum class DrumRole
    {
        Kick      = 0,
        Clap      = 1,
        HatClosed = 2,
        HatOpen   = 3,
        PercA     = 4,
        PercB     = 5,
        Count     = 6
    };

    // Maps a General MIDI drum-map note number to a DrumRole, using real GM
    // drum-map assignments throughout (Kick=36 Bass Drum 1, Clap=39 Hand
    // Clap, HatClosed=42 Closed Hi-Hat, HatOpen=46 Open Hi-Hat, PercA=37
    // Side Stick, PercB=63 Open Hi Conga) so this lines up with most drum
    // racks/instruments by default. Returns false (outRole untouched) if
    // the note isn't one of these six - lets PluginProcessor drive the
    // exact same voices from either the generated pattern or real
    // incoming MIDI note-ons.
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

        uint32_t noiseState     = 1;       // everything except Kick - xorshift32 state

        // One-pole filter state (everything except Kick). Clap/PercA/PercB
        // cascade highpass -> lowpass for a band-passed character;
        // HatClosed/HatOpen use only the highpass stage.
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
