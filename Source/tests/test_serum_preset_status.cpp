// Regression tests for Source/SerumPresetStatus.h - the pure honesty-gate
// logic extracted out of PluginEditor's serumStatusFor/updateTrackTitle so
// it's testable without constructing the full editor (see
// test_loop_generator_processor.cpp's own header comment for why that's
// deliberately avoided elsewhere in this suite). Proves the core contract:
// a preset name is NEVER displayed as loaded/captured unless BOTH a
// confirmed name AND a processor-confirmed "actually loaded" flag are true
// - either one alone must fall through to the honest fallback message.
//
// JUCE-dependent (juce::String only), build/run with the same technique as
// test_rack_classification.cpp:
//   clang++ -std=c++17 -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DDEBUG=1 \
//     -I "$JUCE_MODULES" -I Source -c test_serum_preset_status.cpp
//   (link against compiled juce_core translation unit)

#include "../SerumPresetStatus.h"
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

int main()
{
    // ---- statusText: confirmed name AND processor confirmation both
    // present -> shows the real name, "(captured)". ----
    {
        auto s = SerumPresetStatus::statusText("Bass", "PML BS Rolling Close", true, "some bass", "Serum 2 loaded");
        CHECK(s.contains("PML BS Rolling Close"));
        CHECK(s.contains("(captured)"));
    }

    // ---- Name confirmed locally but processor says NOT actually loaded
    // (the exact desync the real bugs produced) -> must fall through to
    // the honest factory-Init message, never the name. ----
    {
        auto s = SerumPresetStatus::statusText("Bass", "PML BS Rolling Close", /*capturedPresetActive*/ false,
                                                 "some bass", "Serum 2 loaded");
        CHECK(!s.contains("PML BS Rolling Close"));
        CHECK(!s.contains("(captured)"));
        CHECK(s.contains("factory Init patch"));
    }

    // ---- Processor says loaded, but no confirmed name locally (e.g.
    // nothing captured yet) -> also falls through. ----
    {
        auto s = SerumPresetStatus::statusText("Bass", "", /*capturedPresetActive*/ true,
                                                 "some bass", "Serum 2 loaded");
        CHECK(!s.contains("(captured)"));
        CHECK(s.contains("factory Init patch"));
    }

    // ---- Neither present -> honest fallback, includes live processor
    // status text and the real suggestion string (not a generic message). ----
    {
        auto s = SerumPresetStatus::statusText("Melody", "", false, "Melodic Techno lead", "Not loaded");
        CHECK(s.contains("factory Init patch"));
        CHECK(s.contains("Melodic Techno lead"));
        CHECK(s.contains("Not loaded"));
    }

    // ---- titleText: same two-condition gate. ----
    {
        CHECK(SerumPresetStatus::titleText("Bass", "Reese Fall", true).contains("Reese Fall"));
        CHECK(!SerumPresetStatus::titleText("Bass", "Reese Fall", false).contains("Reese Fall"));
        CHECK(SerumPresetStatus::titleText("Bass", "Reese Fall", false).contains("no captured sounds yet"));
        CHECK(SerumPresetStatus::titleText("Bass", "", true).contains("no captured sounds yet"));
    }

    std::printf("%d/%d checks passed\n", g_checksRun - g_checksFailed, g_checksRun);
    return g_checksFailed == 0 ? 0 : 1;
}
