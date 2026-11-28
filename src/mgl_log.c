/*
 * mgl_log.c
 * MGL
 */

#include <mach/mach_time.h>
#include <stdlib.h>

#include "mgl_log.h"

int mgl_log_level = -1;

int mglLogLevel(void)
{
    if (mgl_log_level < 0)
    {
        const char *env = getenv("MGL_LOG_LEVEL");

        mgl_log_level = env ? atoi(env) : MGL_LOG_ERROR;

        if (mgl_log_level < 0)
            mgl_log_level = 0;
    }

    return mgl_log_level;
}

static int build_times = -1;
static unsigned long long build_ticks[MGL_BUILD_STEPS], build_count[MGL_BUILD_STEPS];

static void printBuildTimes(void)
{
    static const char *names[MGL_BUILD_STEPS] = {
        "glslang compile", "glslang link", "SPIRV-Cross", "Metal compile", "pipelines",
        "cached links"
    };
    mach_timebase_info_data_t tb;

    mach_timebase_info(&tb);
    fprintf(stderr, "MGL build times:\n");
    for (int i = 0; i < MGL_BUILD_STEPS; i++)
        fprintf(stderr, "  %-16s %9.1f ms  %6llu\n", names[i],
                (double)build_ticks[i] * tb.numer / tb.denom / 1e6, build_count[i]);
}

unsigned long long mglBuildTimeStart(void)
{
    if (build_times < 0)
    {
        build_times = getenv("MGL_BUILD_TIMES") != NULL;
        if (build_times)
            atexit(printBuildTimes);
    }

    return build_times ? mach_absolute_time() : 0;
}

void mglBuildTimeAdd(int step, unsigned long long start)
{
    if (start == 0)
        return;

    __atomic_fetch_add(&build_ticks[step], mach_absolute_time() - start, __ATOMIC_RELAXED);
    __atomic_fetch_add(&build_count[step], 1, __ATOMIC_RELAXED);
}
