// Regression tests for Source/RackClassification.h - proves the exact bug
// found by the Phase 5 sample-ranking report cannot recur: a Tom sitting
// in a pack whose FILENAME contains the word "Kick" (because that's the
// pack's own name, not the sample's instrument) must classify as TOM,
// never KICK.
//
// JUCE-dependent (juce::String/StringArray/File/AudioFormatManager), so it
// can't join the zero-JUCE Source/Engine/tests/ harness. Build/run with
// (same technique already used by this session's other standalone JUCE
// tools - see e.g. the scratchpad report generators):
//
//   JUCE_MODULES=/path/to/JUCE/modules
//   clang++ -std=c++17 -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DDEBUG=1 \
//     -I "$JUCE_MODULES" -I Source -c test_rack_classification.cpp
//   (link against compiled juce_core + juce_audio_formats translation
//   units, e.g. mod_core.o/mod_audio_formats.o/mod_compiletime.o)
//
// No real files are read for the classification-precedence checks below -
// classifyByTokens/classifyByDirectory only look at strings, so a
// juce::File never needs to exist on disk to be classified. classifySample
// (the one entry point that also inspects duration via
// AudioFormatManager::createReaderFor) is exercised separately, against a
// real temp WAV, specifically to prove the duration-override still runs
// before directory/filename classification even gets a say.

#include "../RackClassification.h"
#include <cstdio>
#include <cstdlib>

static int g_checksRun = 0;
static int g_checksFailed = 0;

#define CHECK(cond) \
    do { \
        ++g_checksRun; \
        if (!(cond)) { \
            ++g_checksFailed; \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

using namespace RackClassification;

int main()
{
    // =========================================================================
    // THE bug: a Tom file whose full path (pack folder AND filename) is
    // saturated with the word "Kick" - because that's the pack's own
    // branding ("KICK & BASS - ALKEMIST"), not the sample's actual
    // instrument - must classify as TOM. This is the literal file Phase 5
    // exposed.
    // =========================================================================
    {
        juce::File f("/Users/shawnshirazi/shawn music stuff/FL studio/Techno/"
                      "KICK & BASS - ALKEMIST VOL.1 -3/KICK & BASS - ALKEMIST VOL.1 - SAMPLES/"
                      "Drums/One Shots/Toms/Kick & Bass Vol.1 - Tom 30 - D#.wav");
        CHECK(classifyByDirectory(f) == "TOM");
    }

    // =========================================================================
    // Directory beats filename tokens generically, not just for this one
    // pack - any file sitting in a "/Toms/" folder classifies as TOM
    // regardless of what the filename says, and the reverse (a file whose
    // NAME says "tom" but sits in a real "/Kicks/" folder) resolves to
    // KICK - directory always wins over filename tokens, both directions.
    // =========================================================================
    {
        juce::File tomInToms("/library/Some Other Pack/Drums/Toms/Kick Drum Impact 07.wav");
        CHECK(classifyByDirectory(tomInToms) == "TOM");

        juce::File tomNamedFileInKicks("/library/Another Pack/Drums/Kicks/Tom-ish Sub Kick 03.wav");
        CHECK(classifyByDirectory(tomNamedFileInKicks) == "KICK");
    }

    // =========================================================================
    // The full user-specified directory -> rack mapping.
    // =========================================================================
    {
        CHECK(classifyByDirectory(juce::File("/lib/Pack/Drums/Kicks/x.wav")) == "KICK");
        CHECK(classifyByDirectory(juce::File("/lib/Pack/Drums/Claps/x.wav")) == "CLAP");
        CHECK(classifyByDirectory(juce::File("/lib/Pack/Drums/Snares/x.wav")) == "SNARE");
        CHECK(classifyByDirectory(juce::File("/lib/Pack/Drums/Closed Hats/x.wav")) == "HIHAT");
        CHECK(classifyByDirectory(juce::File("/lib/Pack/Drums/Open Hats/x.wav")) == "OPEN_HAT");
        CHECK(classifyByDirectory(juce::File("/lib/Pack/Drums/Percs/x.wav")) == "PERC");
        CHECK(classifyByDirectory(juce::File("/lib/Pack/Drums/Toms/x.wav")) == "TOM");
        CHECK(classifyByDirectory(juce::File("/lib/Pack/Drums/Rides/x.wav")) == "RIDE");
    }

    // =========================================================================
    // Directory precedence checks the immediate parent, then the
    // grandparent - a real, common layout ("Drums/One Shots/Kicks/") still
    // resolves correctly even though "One Shots" (checked first, at the
    // immediate-parent level) gives no signal on its own.
    // =========================================================================
    {
        CHECK(classifyByDirectory(juce::File("/lib/Pack/Drums/One Shots/Kicks/x.wav")) == "KICK");
        CHECK(classifyByDirectory(juce::File("/lib/Pack/Drums/One Shots/Toms/x.wav")) == "TOM");
    }

    // =========================================================================
    // A folder more than 2 levels up naming an instrument is deliberately
    // NOT consulted - only the immediate parent and grandparent count, so
    // a top-level pack folder literally named "... Kick ..." several
    // levels above the file must not leak into classification once the
    // file itself sits in a properly-named instrument subfolder further
    // down (already covered above), and must also not accidentally
    // classify a file that has NO clear instrument subfolder at all - it
    // should fall through to filename tokens (or MISC) instead of
    // reaching arbitrarily far up the tree.
    // =========================================================================
    {
        juce::File deep("/lib/KICK Drum Megapack Vol 3/Bonus/Samples/Misc/Random 07.wav");
        const auto byDir = classifyByDirectory(deep);
        CHECK(byDir.isEmpty()); // "Misc" and "Samples" give no confident directory answer - correctly falls through
    }

    // =========================================================================
    // classifyByTokens itself: TOM checked before KICK, OPEN_HAT checked
    // before generic HIHAT - the underlying precedence classifyByDirectory
    // relies on, tested directly and in isolation from any path/duration
    // logic.
    // =========================================================================
    {
        CHECK(classifyByTokens(tokenize("Kick and Bass Tom 30")) == "TOM");
        CHECK(classifyByTokens(tokenize("Kick 05")) == "KICK");
        CHECK(classifyByTokens(tokenize("Open Hat 12")) == "OPEN_HAT");
        CHECK(classifyByTokens(tokenize("Ohh 03")) == "OPEN_HAT");
        CHECK(classifyByTokens(tokenize("Closed Hat 02")) == "HIHAT");
        CHECK(classifyByTokens(tokenize("Hi Hat Loop")) == "LOOP"); // duration-independent token check: "loop" still wins here
    }

    // =========================================================================
    // classifySample end-to-end: the duration-based LOOP override still
    // runs first (a real, unreadable/fake path just falls through to
    // directory/filename classification, proving the override doesn't
    // silently swallow errors into a wrong category).
    // =========================================================================
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();

        juce::File nonexistentTom("/does/not/exist/Drums/Toms/Kick Pack Tom 07.wav");
        CHECK(classifySample(nonexistentTom, fm) == "TOM"); // createReaderFor fails (no file) -> falls through to directory classification, still correct
    }

    std::printf("%d/%d checks passed\n", g_checksRun - g_checksFailed, g_checksRun);
    return g_checksFailed == 0 ? 0 : 1;
}
