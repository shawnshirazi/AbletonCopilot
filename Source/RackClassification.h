#pragma once

// Individual JUCE module includes (not <JuceHeader.h>) so this header has
// no dependency on the Projucer-generated umbrella header or on
// juce_gui_basics/juce_events (juce::Component/juce::Thread) - only
// juce::String/StringArray/File (juce_core) and juce::AudioFormatManager/
// AudioFormatReader (juce_audio_formats) are actually needed to classify a
// sample. Inside the real plugin build these are already pulled in via
// JuceHeader.h before this header is reached, so the includes below are a
// harmless no-op there; standalone (Source/tests/test_rack_classification.cpp,
// and the offline analysis tools under MLPipeline/) they're what makes this
// header usable without linking the entire GUI stack.
#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>

// Sample-to-rack classification - deliberately kept separate from
// RackBrowserComponent (which owns the actual GUI/background-scan
// machinery) so it's unit-testable in isolation. See
// Source/tests/test_rack_classification.cpp, which regression-tests the
// exact bug this precedence order was built to fix: a Tom sitting in a
// pack whose FILENAME happens to contain the word "Kick" (because that's
// the pack's own name, e.g. "Kick & Bass Vol.1 - Tom 30 - D#.wav") must
// classify as TOM, never KICK.
namespace RackClassification
{
    // Loops run several seconds or more; real one-shots are brief. Duration
    // wins over filename every time — a "Kick_Loop.wav" that's actually 4
    // bars long cannot be a kick one-shot no matter what it's called, and
    // triggering it as one on every kick step is exactly what sounded "off".
    constexpr double kLoopDurationThresholdSecs = 2.0;

    inline juce::StringArray tokenize(const juce::String& stem)
    {
        auto normalised = stem.toLowerCase()
                               .replaceCharacter('_', ' ')
                               .replaceCharacter('-', ' ')
                               .replaceCharacter('.', ' ');
        juce::StringArray tokens;
        tokens.addTokens(normalised, " ", "");
        tokens.removeEmptyStrings();
        return tokens;
    }

    // Classify by *whole-word* token, not substring-anywhere — the old
    // "contains" check matched "hh" inside "Ahh_Vocal.wav" and "hat"
    // inside "Whatever.wav", which is how a vocal chop ended up in
    // Hi-Hats. Anything that doesn't hit an exact token falls through to
    // MISC rather than risk a false positive. Shared by both the
    // directory-name classifier and the filename-fallback classifier
    // below - same keyword logic, applied to two different strings.
    //
    // Order matters: TOM is checked before KICK (a folder/filename
    // mentioning both, e.g. a pack-name prefix, must not let the pack
    // name win over the actual instrument), and the OPEN_HAT check
    // (explicit "open" qualifier, or an unambiguous "ohh"/"openhat"
    // token) is checked before the generic HIHAT check for the same
    // reason.
    inline juce::String classifyByTokens(const juce::StringArray& tokens)
    {
        auto has = [&](std::initializer_list<const char*> kws)
        {
            for (auto* kw : kws)
                if (tokens.contains(kw))
                    return true;
            return false;
        };

        if (has({ "loop", "loops", "break", "breaks" }))                          return juce::String("LOOP");
        if (has({ "tom", "toms" }))                                               return juce::String("TOM");
        if (has({ "kick", "kicks", "kck" }))                                      return juce::String("KICK");
        if (has({ "snare", "snares", "snr" }))                                    return juce::String("SNARE");
        if (has({ "clap", "claps", "clp" }))                                      return juce::String("CLAP");
        if (has({ "ohh", "openhat", "openhats", "openhihat" })
            || (tokens.contains("open") && has({ "hat", "hats", "hihat", "hihats", "hh" })))
            return juce::String("OPEN_HAT");
        if (has({ "hihat", "hihats", "hat", "hats", "hh", "chh" }))               return juce::String("HIHAT");
        if (has({ "cymbal", "cymbals", "crash", "ride" }))                        return juce::String("CYMBAL");
        if (has({ "perc", "percs", "percussion", "shaker", "conga", "bongo" }))   return juce::String("PERC");
        if (has({ "bass", "sub", "808" }))                                        return juce::String("BASS");
        if (has({ "fx", "riser", "impact", "sweep", "noise" }))                   return juce::String("FX");
        if (has({ "vocal", "vocals", "vox", "adlib", "acapella" }))               return juce::String("VOCAL");
        return juce::String("MISC");
    }

    // Directory structure is a stronger, less ambiguous signal than the
    // filename: real sample packs reliably name the actual instrument
    // subfolder ("Kicks", "Toms", "Closed Hats", ...) per-instrument,
    // while the FILENAME often carries the whole pack/edition name as a
    // prefix (e.g. "Kick & Bass Vol.1 - Tom 30 - D#.wav" - the "Kick"
    // there is the PACK's name, not this sample's instrument; the file
    // sits in a literal "Toms" folder). Checks the immediate parent, then
    // the grandparent, and stops there deliberately - packs put the
    // instrument folder at most one or two levels above the file, and
    // anything further up the tree is typically pack/bundle/edition
    // marketing text, exactly the kind of token that caused this bug in
    // the first place and must not be consulted here. Returns an empty
    // string (not MISC) when neither level gives a confident answer, so
    // the caller knows to fall back to filename tokens instead of
    // wrongly committing to MISC.
    inline juce::String classifyByDirectory(const juce::File& file)
    {
        juce::File dir = file.getParentDirectory();
        for (int level = 0; level < 2 && dir != juce::File(); ++level)
        {
            const auto result = classifyByTokens(tokenize(dir.getFileName()));
            if (result != "MISC")
                return result;
            dir = dir.getParentDirectory();
        }
        return {};
    }

    // Duration (when readable) overrides everything else - see
    // kLoopDurationThresholdSecs. Then directory structure (see
    // classifyByDirectory above) takes precedence over the filename;
    // filename tokens are only a fallback for files sitting in an
    // unhelpfully-named folder (a flat "Samples" or "One Shots" dump with
    // no per-instrument subfolder).
    inline juce::String classifySample(const juce::File& file, juce::AudioFormatManager& formatManager)
    {
        if (auto reader = std::unique_ptr<juce::AudioFormatReader>(formatManager.createReaderFor(file)))
        {
            if (reader->sampleRate > 0.0
                && (double) reader->lengthInSamples / reader->sampleRate >= kLoopDurationThresholdSecs)
                return "LOOP";
        }

        const auto byDirectory = classifyByDirectory(file);
        if (byDirectory.isNotEmpty())
            return byDirectory;

        return classifyByTokens(tokenize(file.getFileNameWithoutExtension()));
    }
}
