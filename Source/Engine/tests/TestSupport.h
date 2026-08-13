#pragma once
#include <cstdio>
#include <cstdlib>

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
