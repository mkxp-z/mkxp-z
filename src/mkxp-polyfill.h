/*
** mkxp-polyfill.h
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

#ifndef MKXPZ_POLYFILL_H
#define MKXPZ_POLYFILL_H

#include <math.h>
#include <tgmath.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(__APPLE__) && !defined(MKXPZ_NO_SEMAPHORE_H)
#  define MKXPZ_NO_SEMAPHORE_H
#endif

#if !defined(MKXPZ_NO_PTHREAD_H_MUTEX) || !defined(MKXPZ_NO_PTHREAD_H_THREAD)
#  include <pthread.h>
#endif

#ifndef MKXPZ_NO_SEMAPHORE_H
#  include <semaphore.h>
#endif

#ifndef MKXPZ_NO_DISPATCH_DISPATCH_H
#  include <dispatch/dispatch.h>
#endif

#if defined(_WIN32) && !defined(EDQUOT)
#  define EDQUOT 122
#endif

#if defined(_WIN32) && !defined(S_ISLNK)
#  define S_ISLNK(x) 0
#endif

#if !defined(__cplusplus) || defined(MKXPZ_NO_EXCEPTIONS)
#  define MKXPZ_THROW(...) do { fprintf(stderr, "Exception thrown: %s\n", (__VA_ARGS__).what()); fflush(stderr); abort(); } while (0)
#  define MKXPZ_RETHROW do { } while (0)
#  define MKXPZ_TRY if (1)
#  define MKXPZ_CATCH(...) if (0)
#else
#  define MKXPZ_THROW(...) throw __VA_ARGS__
#  define MKXPZ_RETHROW throw
#  define MKXPZ_TRY try
#  define MKXPZ_CATCH(...) catch (__VA_ARGS__)
#endif

#ifndef MKXPZ_NO_STD_MUTEX
typedef struct {
    void *inner;
    bool recursive;
} mkxp_mutex_t;
typedef void *mkxp_cond_t;
#elif defined(MKXPZ_DEVKITARM_NO_PTHREAD_H_MUTEX)
#  include <sys/lock.h>
typedef struct {
    union {
        _LOCK_T light;
        _LOCK_RECURSIVE_T recursive;
    } inner;
    bool recursive;
} mkxp_mutex_t;
typedef int32_t mkxp_cond_t;
#elif !defined(MKXPZ_NO_PTHREAD_H_MUTEX)
typedef pthread_mutex_t mkxp_mutex_t;
typedef pthread_cond_t mkxp_cond_t;
#else
typedef unsigned int mkxp_mutex_t;
typedef bool mkxp_cond_t;
#endif

#ifndef MKXPZ_NO_SEMAPHORE_H
typedef sem_t mkxp_sem_t;
#elif !defined(MKXPZ_NO_DISPATCH_DISPATCH_H)
typedef dispatch_semaphore_t mkxp_sem_t;
#elif !defined(MKXPZ_NO_MUTEX)
typedef void *mkxp_sem_t;
#else
typedef unsigned int mkxp_sem_t;
#endif

#ifndef MKXPZ_NO_STD_THREAD
typedef void *mkxp_thread_t;
typedef size_t mkxp_thread_id_t;
#elif defined(MKXPZ_DEVKITARM_NO_PTHREAD_H_THREAD)
typedef void *mkxp_thread_t;
typedef void *mkxp_thread_id_t;
#elif !defined(MKXPZ_NO_PTHREAD_H_THREAD)
typedef pthread_t mkxp_thread_t;
typedef pthread_t mkxp_thread_id_t;
#else
typedef uint8_t mkxp_thread_id_t;
#endif

#ifdef __cplusplus
#include <array>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <condition_variable>
#include <string>

extern "C" long long strtoll(const char *str, char **str_end, int base);
extern "C" unsigned long long strtoull(const char *str, char **str_end, int base);

extern "C" {
#endif

#ifdef MKXPZ_NO_SPRINTF
int sprintf(char *buffer, const char *format, ...);
#endif

#ifdef MKXPZ_NO_SNPRINTF
int snprintf(char *buffer, size_t buf_size, const char *format, ...);
#endif

#ifdef MKXPZ_NO_VSPRINTF
int vsprintf(char *buffer, const char *format, va_list vlist);
#endif

#ifdef MKXPZ_NO_VSNPRINTF
int vsnprintf(char *buffer, size_t buf_size, const char *format, va_list vlist);
#endif

#ifdef MKXPZ_NO_MEMCCPY
void *memccpy(void *dest, const void *src, int c, size_t n);
#endif

#ifdef MKXPZ_NO_FSYNC
int fsync(int);
#endif

void *mkxp_aligned_malloc(size_t alignment, size_t size);

void mkxp_aligned_free(void *ptr, size_t alignment);

int mkxp_mutex_init(mkxp_mutex_t *mutex, bool recursive);

int mkxp_mutex_destroy(mkxp_mutex_t *mutex);

int mkxp_mutex_lock(mkxp_mutex_t *mutex);

int mkxp_mutex_unlock(mkxp_mutex_t *mutex);

int mkxp_cond_init(mkxp_cond_t *cond);

int mkxp_cond_destroy(mkxp_cond_t *cond);

int mkxp_cond_signal(mkxp_cond_t *cond);

int mkxp_cond_broadcast(mkxp_cond_t *cond);

int mkxp_cond_wait(mkxp_cond_t *cond, mkxp_mutex_t *mutex);

int mkxp_sem_init(mkxp_sem_t *sem, unsigned int value);

int mkxp_sem_destroy(mkxp_sem_t *sem);

int mkxp_sem_post(mkxp_sem_t *sem);

int mkxp_sem_wait(mkxp_sem_t *sem);

void mkxp_sleep_ms(uint32_t milliseconds);

mkxp_thread_id_t mkxp_thread_self(void);

#ifndef MKXPZ_NO_THREAD
int mkxp_thread_create(mkxp_thread_t *thread, void *(*func)(void *), void *arg);

int mkxp_thread_join(mkxp_thread_t thread);
#endif

#ifdef __cplusplus
}

#  ifdef MKXPZ_NO_STD_MUTEX
namespace std {
    class mutex {
    public:
        typedef mkxp_mutex_t *native_handle_type;

        mkxp_mutex_t inner;

        mutex() noexcept;
        ~mutex() noexcept;
        void lock() noexcept;
        void unlock() noexcept;

        inline native_handle_type native_handle() noexcept {
            return &inner;
        }
    };
}
#  endif

#  ifdef MKXPZ_NO_STD_RECURSIVE_MUTEX
namespace std {
    class recursive_mutex {
    public:
        typedef mkxp_mutex_t *native_handle_type;

        mkxp_mutex_t inner;

        recursive_mutex() noexcept;
        ~recursive_mutex() noexcept;
        void lock() noexcept;
        void unlock() noexcept;

        inline native_handle_type native_handle() noexcept {
            return &inner;
        }
    };
}
#  endif

#  ifdef MKXPZ_NO_STD_CONDITION_VARIABLE_ANY
namespace std {
    class condition_variable_any {
    public:
        mkxp_cond_t inner;

        condition_variable_any() noexcept;
        ~condition_variable_any() noexcept;
        void notify_one() noexcept;
        void notify_all() noexcept;
        void wait(std::mutex &mutex) noexcept;
    };
}
#  endif

#  ifdef MKXPZ_NO_STD_SPRINTF
namespace std {
    inline int sprintf(char *buffer, const char *format, ...) {
        va_list vlist;
        va_start(vlist, format);
        int result = ::vsprintf(buffer, buf_size, format, vlist);
        va_end(vlist);
        return result;
    }
}
#  endif

#  ifdef MKXPZ_NO_STD_SNPRINTF
namespace std {
    inline int snprintf(char *buffer, size_t buf_size, const char *format, ...) {
        va_list vlist;
        va_start(vlist, format);
        int result = ::vsnprintf(buffer, buf_size, format, vlist);
        va_end(vlist);
        return result;
    }
}
#  endif

#  ifdef MKXPZ_NO_STD_VSPRINTF
namespace std {
    inline int vsprintf(char *buffer, const char *format, va_list vlist) {
        return ::vsprintf(buffer, buf_size, format, vlist);
    }
}
#  endif

#  ifdef MKXPZ_NO_STD_VSNPRINTF
namespace std {
    inline int vsnprintf(char *buffer, size_t buf_size, const char *format, va_list vlist) {
        return ::vsnprintf(buffer, buf_size, format, vlist);
    }
}
#  endif

#  ifdef MKXPZ_NO_STD_ROUND
namespace std {
    inline constexpr float round(float x) {
        return ::roundf(x);
    }

    inline constexpr double round(double x) {
        return ::round(x);
    }

    inline constexpr long double round(long double x) {
        return ::roundl(x);
    }
}
#  endif

#  ifdef MKXPZ_NO_STD_LROUND
namespace std {
    inline constexpr long lround(float x) {
        return ::lroundf(x);
    }

    inline constexpr long lround(double x) {
        return ::lround(x);
    }

    inline constexpr long lround(long double x) {
        return ::lroundl(x);
    }
}
#  endif

#  ifdef MKXPZ_NO_STD_COPYSIGN
namespace std {
    inline constexpr float copysign(float x, float y) {
        return ::copysignf(x, y);
    }

    inline constexpr double copysign(double x, double y) {
        return ::copysign(x, y);
    }

    inline constexpr long double copysign(long double x, long double y) {
        return ::copysignl(x, y);
    }
}
#  endif

#  ifdef MKXPZ_NO_STD_CBRT
namespace std {
    inline constexpr float cbrt(float x) {
        return ::cbrtf(x);
    }

    inline constexpr double cbrt(double x) {
        return ::cbrt(x);
    }

    inline constexpr long double cbrt(long double x) {
        return ::cbrtl(x);
    }
}
#  endif

#  ifdef MKXPZ_NO_STD_LOG2
namespace std {
    inline constexpr float log2(float x) {
        return ::log2f(x);
    }

    inline constexpr double log2(double x) {
        return ::log2(x);
    }
}
#  endif

#  ifdef MKXPZ_NO_STD_TO_STRING
namespace std {
    inline string to_string(int x) {
        array<char, 22> array;
        sprintf(array.data(), "%d", x);
        return array.data();
    }

    inline string to_string(long x) {
        array<char, 22> array;
        sprintf(array.data(), "%ld", x);
        return array.data();
    }

    inline string to_string(long long x) {
        array<char, 22> array;
        sprintf(array.data(), "%lld", x);
        return array.data();
    }

    inline string to_string(unsigned int x) {
        array<char, 22> array;
        sprintf(array.data(), "%u", x);
        return array.data();
    }

    inline string to_string(unsigned long x) {
        array<char, 22> array;
        sprintf(array.data(), "%lu", x);
        return array.data();
    }

    inline string to_string(unsigned long long x) {
        array<char, 22> array;
        sprintf(array.data(), "%llu", x);
        return array.data();
    }

    inline string to_string(float x) {
        array<char, 256> array;
        sprintf(array.data(), "%f", x);
        return array.data();
    }

    inline string to_string(double x) {
        array<char, 256> array;
        sprintf(array.data(), "%f", x);
        return array.data();
    }
}
#  endif

#  ifdef MKXPZ_NO_STD_STOI
namespace std {
    inline int stoi(const string &str, size_t *pos = nullptr, int base = 10) {
        char *ptr;
        int result = (int)::strtol(str.c_str(), &ptr, base);
        if (pos != nullptr) {
            *pos = ptr - str.c_str();
        }
        return result;
    }
}
#  endif

#  ifdef MKXPZ_NO_STD_STOL
namespace std {
    inline long stol(const string &str, size_t *pos = nullptr, int base = 10) {
        char *ptr;
        long result = ::strtol(str.c_str(), &ptr, base);
        if (pos != nullptr) {
            *pos = ptr - str.c_str();
        }
        return result;
    }
}
#  endif

#  ifdef MKXPZ_NO_STD_STOLL
namespace std {
    inline long long stoll(const string &str, size_t *pos = nullptr, int base = 10) {
        char *ptr;
        long long result = ::strtoll(str.c_str(), &ptr, base);
        if (pos != nullptr) {
            *pos = ptr - str.c_str();
        }
        return result;
    }
}
#  endif

#  ifdef MKXPZ_NO_STD_STOUL
namespace std {
    inline unsigned long stoul(const string &str, size_t *pos = nullptr, int base = 10) {
        char *ptr;
        unsigned long result = ::strtoul(str.c_str(), &ptr, base);
        if (pos != nullptr) {
            *pos = ptr - str.c_str();
        }
        return result;
    }
}
#  endif

#  ifdef MKXPZ_NO_STD_STOULL
namespace std {
    inline unsigned long long stoull(const string &str, size_t *pos = nullptr, int base = 10) {
        char *ptr;
        unsigned long long result = ::strtoull(str.c_str(), &ptr, base);
        if (pos != nullptr) {
            *pos = ptr - str.c_str();
        }
        return result;
    }
}
#  endif

#  ifdef MKXPZ_NO_STD_STOF
namespace std {
    inline float stof(const string &str, size_t *pos = nullptr) {
        char *ptr;
        float result = ::strtof(str.c_str(), &ptr);
        if (pos != nullptr) {
            *pos = ptr - str.c_str();
        }
        return result;
    }
}
#  endif

#  ifdef MKXPZ_NO_STD_STOD
namespace std {
    inline double stod(const string &str, size_t *pos = nullptr) {
        char *ptr;
        double result = ::strtod(str.c_str(), &ptr);
        if (pos != nullptr) {
            *pos = ptr - str.c_str();
        }
        return result;
    }
}
#  endif

#  ifdef MKXPZ_NO_STD_STOLD
namespace std {
    inline long double stold(const string &str, size_t *pos = nullptr) {
        char *ptr;
        long double result = ::strtold(str.c_str(), &ptr);
        if (pos != nullptr) {
            *pos = ptr - str.c_str();
        }
        return result;
    }
}
#  endif
#endif

#endif // MKXPZ_POLYFILL_H
