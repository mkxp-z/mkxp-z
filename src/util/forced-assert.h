/*
** forced-assert.h
**
** This file is part of mkxp.
**
** Copyright (C) 2025 - 2026 The mkxp-z authors
**
** mkxp is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** mkxp is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MKXPZ_FORCED_ASSERT

#include <cstdio>
#include <cstdlib>

#define _MKXPZ_FORCED_ASSERT_DETAIL2(x) #x
#define _MKXPZ_FORCED_ASSERT_DETAIL(x) _MKXPZ_FORCED_ASSERT_DETAIL2(x)

#ifdef MKXPZ_RETRO
#  include <libretro.h>
extern retro_log_printf_t mkxp_retro_log_printf;
#  define MKXPZ_FORCED_ASSERT_WITH_MESSAGE(condition, message) do { \
    if (!(condition)) { \
        const char *fmt = "[mkxp-z @ " __FILE__ ":" _MKXPZ_FORCED_ASSERT_DETAIL(__LINE__) "] Fatal error: %s\n"; \
        std::fprintf(stderr, fmt, message); \
        ::mkxp_retro_log_printf(RETRO_LOG_ERROR, fmt, message); \
        std::fflush(stderr); \
        std::abort(); \
    } \
} while (0)
#else
#  define MKXPZ_FORCED_ASSERT_WITH_MESSAGE(condition, message) do { \
    if (!(condition)) { \
        const char *fmt = "[mkxp-z @ " __FILE__ ":" _MKXPZ_FORCED_ASSERT_DETAIL(__LINE__) "] Fatal error: %s\n"; \
        std::fprintf(stderr, fmt, message); \
        std::fflush(stderr); \
        std::abort(); \
    } \
} while (0)
#endif // MKXPZ_RETRO

/* Like `assert()`, except will not be disabled if `NDEBUG` is defined,
 * which is useful if the assertion must always occur for security reasons. */
#define MKXPZ_FORCED_ASSERT(condition) MKXPZ_FORCED_ASSERT_WITH_MESSAGE(condition, #condition)

#endif // MKXPZ_FORCED_ASSERT
