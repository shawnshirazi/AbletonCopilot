#pragma once
#include <JuceHeader.h>

// One shared dark palette + type scale for the whole editor UI, so
// PluginEditor and DrumMachineComponent (previously each hardcoding their
// own near-duplicate constants) read as one cohesively-designed instrument
// instead of two separately-styled panels.
//
// Palette sourced from XLN Audio's XO (real product screenshots, not just
// the reference image — see /private/tmp/.../scratchpad/xo-ref/): cooler,
// darker near-black background, thin dividers instead of banded rows, and
// a real per-row accent palette rather than one dominant brand color.
namespace UIStyle
{
    static const juce::Colour kBg          { 0xff17181b };
    static const juce::Colour kPanel       { 0xff1e1f22 };
    static const juce::Colour kPanelAlt    { 0xff1b1c1f };
    static const juce::Colour kRowAlt      { 0xff202126 };
    static const juce::Colour kBorder      { 0xff2a2b2f };
    static const juce::Colour kAccent      { 0xff4dd6e6 };
    static const juce::Colour kTextPrimary { 0xffe9eaec };
    static const juce::Colour kTextDim     { 0xff8a8d93 };
    static const juce::Colour kGood        { 0xff4caf50 };
    static const juce::Colour kWarn        { 0xffe0b039 };
    static const juce::Colour kCrit        { 0xffef3b53 };
    static const juce::Colour kPlaying     { 0xff4caf50 };
    static const juce::Colour kStopped     { 0xff55575c };
    static const juce::Colour kMelody      { 0xfff2ede0 };

    // Per-instrument row accent palette, matched to XO's own row hues where
    // there's a direct analog (kick/snare/clap/hihat/cymbal/perc), distinct
    // saturated colours elsewhere so every row reads uniquely on the dark bg.
    static const juce::Colour kKick   { 0xffef3b53 };
    static const juce::Colour kSnare  { 0xff2f8fe0 };
    static const juce::Colour kClap   { 0xfff15fa6 };
    static const juce::Colour kClapAlt{ 0xffc23f81 }; // layered "CLP2" variant row
    static const juce::Colour kHihat  { 0xffe0b039 };
    static const juce::Colour kCymbal { 0xff3ecf6e };
    static const juce::Colour kPerc   { 0xff29c4d9 };
    static const juce::Colour kBass   { 0xff9d6bff };
    static const juce::Colour kFx     { 0xffff6ec7 };
    static const juce::Colour kVocal  { 0xffffa64d };
    static const juce::Colour kMisc   { 0xff9a9da3 };

    // Melody-track category accents (MelodyCategory.h) — Bass/Fx reuse the
    // drum-row colours above; these fill in the categories that don't have
    // a drum-row analog.
    static const juce::Colour kLead     { 0xffff7a59 };
    static const juce::Colour kPad      { 0xff8899ee };
    static const juce::Colour kPluck    { 0xff2edcc4 };
    static const juce::Colour kSynthCat { 0xffffb347 };

    // Mute (red) / Solo (amber) active-state fills — standard DAW convention.
    static const juce::Colour kMuteActive { 0xffef3b53 };
    static const juce::Colour kSoloActive { 0xffe0b039 };

    inline juce::FontOptions title()    { return juce::FontOptions(17.0f).withStyle("Bold"); }
    inline juce::FontOptions tab()      { return juce::FontOptions(12.5f).withStyle("Bold"); }
    inline juce::FontOptions heading()  { return juce::FontOptions(11.5f).withStyle("Bold"); }
    inline juce::FontOptions body()     { return juce::FontOptions(11.0f); }
    inline juce::FontOptions small()    { return juce::FontOptions(9.5f); }
    inline juce::FontOptions badge()    { return juce::FontOptions(9.0f).withStyle("Bold"); }
}
