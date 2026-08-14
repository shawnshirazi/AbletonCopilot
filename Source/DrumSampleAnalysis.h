#pragma once
#include <JuceHeader.h>
#include "Engine/DrumSampleFeatures.h"

// Lightweight, dependency-free (beyond JUCE's own decoder) DSP feature
// extraction for a single one-shot drum sample - no FFT, no ML, no
// heavyweight analysis library. Pure measurement: this file has no
// opinion about what makes a "good" kick/hat/perc - see
// Engine/DrumSampleScoring.h for how these numbers become a
// Melodic-Techno-appropriate ranking. Never called from the audio thread
// - see DrumSampleIndex.h, which always runs analysis on its own
// background thread.

// Reads and analyzes `file` directly. Returns
// Engine::DrumSampleFeatures{valid=false} if the file can't be decoded
// (missing, corrupt, unsupported format).
Engine::DrumSampleFeatures analyzeDrumSample(const juce::File& file);
