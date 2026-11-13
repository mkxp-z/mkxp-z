#ifndef SWITCH_TRACE_H
#define SWITCH_TRACE_H

// Verbose startup tracing for the Switch port, written to sdmc:/mkxpz_log.txt.
// Enabled at runtime when sdmc:/switch/mkxp-z/debug.txt exists (see
// switch/switch_main.cpp); compiled out on other platforms.

#ifdef __SWITCH__
#include <stdio.h>

#ifdef __cplusplus
extern "C" int switch_trace_enabled;
#else
extern int switch_trace_enabled;
#endif

#define SWITCH_TRACE(...)                                                      \
    do {                                                                       \
        if (switch_trace_enabled) {                                            \
            printf(__VA_ARGS__);                                               \
            fflush(stdout);                                                    \
        }                                                                      \
    } while (0)
#else
#define SWITCH_TRACE(...) do { } while (0)
#endif

#endif // SWITCH_TRACE_H
