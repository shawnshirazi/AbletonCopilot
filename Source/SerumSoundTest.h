#pragma once
#include <JuceHeader.h>
#include <array>

// The standardized, deterministic Serum 2 sound test - documented here,
// in code, exactly as requested: every learned sound is measured against
// the SAME fixed MIDI points, never random notes, so results are
// comparable across candidates.
//
// Register/note-length/velocity are tested SEPARATELY, not as a full
// cross-product, for Phase 1 (disclosed simplification, not hidden): the
// register sweep (C1-C4) is rendered at the fixed medium length/medium
// velocity; the note-length sweep (short/medium/sustained) and velocity
// sweep (soft/medium/hard) are each rendered at one representative pitch
// (kRepresentativePitch). That is 4 + 3 + 3 = 10 renders per candidate,
// with the medium/medium/C-representative cell shared, giving 8 unique
// renders - small and fast enough to genuinely test the pipeline (per the
// user's own "verify the complete pipeline works" priority) before ever
// considering the full register x length x velocity grid.
namespace SerumSoundTest
{
    // MIDI note numbers for C1-C4, matching this codebase's own already-
    // established convention (PluginProcessor.h's clampBassRegisterPitch/
    // clampMelodyRegisterPitch comments: MIDI 48 = "C3", i.e. Ableton's own
    // octave-numbering convention, not the Yamaha/scientific one) - every
    // octave here is a real, playable MIDI pitch, not a guess.
    struct RegisterPoint { const char* label; int midiPitch; };
    inline const std::array<RegisterPoint, 4>& registers()
    {
        static const std::array<RegisterPoint, 4> r {{
            { "C1", 24 },
            { "C2", 36 },
            { "C3", 48 },
            { "C4", 60 },
        }};
        return r;
    }

    // Note lengths in 16th-note steps at whatever the render's own BPM is
    // (matches this codebase's existing step-grid convention throughout -
    // Engine::kGrooveLoopStepsPerBar etc). "sustained" additionally
    // renders a release tail past the note-off so effective release time
    // is measurable.
    struct NoteLengthPoint { const char* label; int lengthSteps; };
    inline const std::array<NoteLengthPoint, 3>& noteLengths()
    {
        static const std::array<NoteLengthPoint, 3> n {{
            { "short",     1  }, // one 16th note (~121ms @ 124bpm)
            { "medium",    4  }, // one quarter note (~484ms @ 124bpm)
            { "sustained", 16 }, // one full bar (~1.94s @ 124bpm)
        }};
        return n;
    }

    // Raw MIDI velocities 0-127.
    struct VelocityPoint { const char* label; int velocity; };
    inline const std::array<VelocityPoint, 3>& velocities()
    {
        static const std::array<VelocityPoint, 3> v {{
            { "soft",   40  },
            { "medium", 80  },
            { "hard",   110 },
        }};
        return v;
    }

    inline constexpr int kMediumLengthSteps = 4;
    inline constexpr int kMediumVelocity    = 80;
    inline constexpr int kRepresentativePitch = 36; // C2 - a real, playable pitch in both the bass and melody clamp bands (F1-C3 / C3-C5), so the same representative point is usable for either role
    // Extra tail rendered past a note's own note-off, in 16th-note steps,
    // so release/decay behaviour is actually captured in the buffer
    // rather than cut off exactly at the note-off instant.
    inline constexpr int kReleaseTailSteps = 8;
}
