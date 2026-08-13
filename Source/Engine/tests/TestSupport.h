#pragma once
#include "../DrumEngine.h"
#include "../Grid.h"
#include <cstdio>
#include <cstdlib>
#include <string>

// Minimal, dependency-free test harness - plain clang++, no framework.
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

#define TEST_SUMMARY_AND_EXIT() \
    do { \
        std::printf("%d/%d checks passed\n", g_checksRun - g_checksFailed, g_checksRun); \
        return g_checksFailed == 0 ? 0 : 1; \
    } while (0)

// Human-readable one-bar-per-line rendering of a generated pattern, for
// visual inspection without needing Ableton. One character per 16th step:
//   .  = no hit
//   x  = soft hit  (velocity < 0.5)
//   o  = medium hit (0.5 <= velocity < 0.8)
//   #  = strong hit (velocity >= 0.8)
inline std::string renderPattern(const Engine::StepArray& steps, const Engine::StepGridConfig& grid)
{
    std::string out;
    for (int bar = 0; bar < grid.numBars; ++bar)
    {
        char lineNum[8];
        std::snprintf(lineNum, sizeof(lineNum), "%2d: ", bar);
        out += lineNum;

        for (int s = 0; s < grid.stepsPerBar; ++s)
        {
            const int step = bar * grid.stepsPerBar + s;
            const auto& hit = steps[(size_t) step];

            char c = '.';
            if (hit.active)
            {
                c = hit.velocity >= 0.8f ? '#'
                  : hit.velocity >= 0.5f ? 'o'
                  :                        'x';
            }
            out += c;

            // Visual beat separator every 4 steps (quarter notes), matching
            // the stepsPerBar=16 convention this engine defaults to.
            if ((s + 1) % 4 == 0 && s + 1 < grid.stepsPerBar)
                out += ' ';
        }
        out += '\n';
    }
    return out;
}

inline void printPattern(const char* roleName, const Engine::StepArray& steps, const Engine::StepGridConfig& grid)
{
    int active = 0;
    for (auto& h : steps)
        if (h.active) ++active;

    std::printf("--- %s (%d/%d active) ---\n", roleName, active, (int) steps.size());
    std::printf("%s", renderPattern(steps, grid).c_str());
}
