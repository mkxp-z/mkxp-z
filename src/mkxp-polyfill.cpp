/*
** mkxp-polyfill.cpp
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

#ifdef MKXPZ_NO_ISASCII
extern "C" int isascii(int c) {
    return c < 128;
}
#endif

#include "mkxp-polyfill.h"
#include <cassert>
#include <cstring>

#include "forced-assert.h"

#if defined(MKXPZ_NO_SPRINTF) || defined(MKXPZ_NO_SNPRINTF) || defined(MKXPZ_NO_VSPRINTF) || defined(MKXPZ_NO_VSNPRINTF)
#  include <stb_sprintf.h>
#endif

#ifdef MKXPZ_HAVE_ALIGNED_MALLOC
#  include <malloc.h>
#endif

#if defined(MKXPZ_HAVE_ALIGNED_OPERATOR_NEW) || defined(MKXPZ_DARWIN_NO_ALIGNED_OPERATOR_NEW)
#  include <new>
#endif

#ifndef MKXPZ_NO_STD_THIS_THREAD_SLEEP_FOR
#  include <thread>
#elif !defined(MKXPZ_NO_USLEEP)
#  include <unistd.h>
#elif !defined(MKXPZ_NO_NANOSLEEP)
#  include <time.h>
#endif

#ifndef MKXPZ_NO_STD_MUTEX
#  include <mutex>
#endif

#ifdef MKXPZ_NO_STD_BAD_OPTIONAL_ACCESS
#  include <optional>
const char *std::bad_optional_access::what() const noexcept {
    return "std::bad_optional_access";
}
#endif

#ifdef MKXPZ_NO_STD_BAD_VARIANT_ACCESS
#  include <variant>
const char *std::bad_variant_access::what() const noexcept {
    return "std::bad_variant_access";
}
#endif

#  ifdef MKXPZ_NO_STD_MUTEX
std::mutex::mutex() noexcept {
    MKXPZ_FORCED_ASSERT(!mkxp_mutex_init(&inner, false));
}

std::mutex::~mutex() noexcept {
    MKXPZ_FORCED_ASSERT(!mkxp_mutex_destroy(&inner));
}

void std::mutex::lock() noexcept {
    MKXPZ_FORCED_ASSERT(!mkxp_mutex_lock(&inner));
}

void std::mutex::unlock() noexcept {
    MKXPZ_FORCED_ASSERT(!mkxp_mutex_unlock(&inner));
}
#  endif

#  ifdef MKXPZ_NO_STD_RECURSIVE_MUTEX
std::recursive_mutex::recursive_mutex() noexcept {
    MKXPZ_FORCED_ASSERT(!mkxp_mutex_init(&inner, true));
}

std::recursive_mutex::~recursive_mutex() noexcept {
    MKXPZ_FORCED_ASSERT(!mkxp_mutex_destroy(&inner));
}

void std::recursive_mutex::lock() noexcept {
    MKXPZ_FORCED_ASSERT(!mkxp_mutex_lock(&inner));
}

void std::recursive_mutex::unlock() noexcept {
    MKXPZ_FORCED_ASSERT(!mkxp_mutex_unlock(&inner));
}
#  endif

#  ifdef MKXPZ_NO_STD_CONDITION_VARIABLE_ANY
std::condition_variable_any::condition_variable_any() noexcept {
    MKXPZ_FORCED_ASSERT(!mkxp_cond_init(&inner));
}

std::condition_variable_any::~condition_variable_any() noexcept {
    MKXPZ_FORCED_ASSERT(!mkxp_cond_destroy(&inner));
}

void std::condition_variable_any::notify_one() noexcept {
    MKXPZ_FORCED_ASSERT(!mkxp_cond_signal(&inner));
}

void std::condition_variable_any::notify_all() noexcept {
    MKXPZ_FORCED_ASSERT(!mkxp_cond_broadcast(&inner));
}

void std::condition_variable_any::wait(std::mutex &mutex) noexcept {
    MKXPZ_FORCED_ASSERT(!mkxp_cond_wait(&inner, &mutex.inner));
}
#  endif

#ifdef MKXPZ_NO_SPRINTF
extern "C" int sprintf(char *buffer, const char *format, ...) {
    va_list vlist;
    va_start(vlist, format);
    int result = stbsp_vsprintf(buffer, buf_size, format, vlist);
    va_end(vlist);
    return result;
}
#endif

#ifdef MKXPZ_NO_SNPRINTF
extern "C" int snprintf(char *buffer, size_t buf_size, const char *format, ...) {
    va_list vlist;
    va_start(vlist, format);
    int result = stbsp_vsnprintf(buffer, buf_size, format, vlist);
    va_end(vlist);
    return result;
}
#endif

#ifdef MKXPZ_NO_VSPRINTF
extern "C" int vsprintf(char *buffer, const char *format, va_list vlist) {
    return stbsp_vsprintf(buffer, buf_size, format, vlist);
}
#endif

#ifdef MKXPZ_NO_VSNPRINTF
extern "C" int vsnprintf(char *buffer, size_t buf_size, const char *format, va_list vlist) {
    return stbsp_vsnprintf(buffer, buf_size, format, vlist);
}
#endif

#ifdef MKXPZ_NO_MEMCCPY
extern "C" void *memccpy(void *dest, const void *src, int c, size_t n) {
    const void *ptr = std::memchr(src, c, n);
    if (ptr != nullptr) {
        std::memcpy(dest, src, (const uint8_t *)ptr - (const uint8_t *)src + 1);
        return (uint8_t *)ptr + 1;
    } else {
        std::memcpy(dest, src, n);
        return nullptr;
    }
}
#endif

#ifdef MKXPZ_NO_FSYNC
#  ifdef _WIN32
#    include <io.h>
int fsync(int fd) {
    return _commit(fd);
}
#  else
#    error "no function for flushing a file descriptor"
#  endif
#endif

extern "C" void *mkxp_aligned_malloc(size_t alignment, size_t size) {
#if defined(MKXPZ_HAVE_POSIX_MEMALIGN) || defined(MKXPZ_BUILD_XCODE)
    void *mem;
    return posix_memalign(&mem, alignment, size) ? NULL : mem;
#elif defined(MKXPZ_HAVE_ALIGNED_MALLOC)
    return _aligned_malloc(size, alignment);
#elif defined(MKXPZ_HAVE_ALIGNED_ALLOC)
    return aligned_alloc(alignment, size);
#elif defined(MKXPZ_HAVE_STD_ALIGNED_ALLOC)
    return std::aligned_alloc(alignment, size);
#elif defined(MKXPZ_HAVE_ALIGNED_OPERATOR_NEW)
    return operator new[](size, std::align_val_t(alignment), std::nothrow_t());
#else
#  error "no aligned memory allocation function found"
#endif
}

extern "C" void mkxp_aligned_free(void *ptr, size_t alignment) {
#if defined(MKXPZ_HAVE_ALIGNED_MALLOC)
    _aligned_free(ptr);
#elif defined(MKXPZ_HAVE_ALIGNED_OPERATOR_NEW)
    operator delete[](ptr, std::align_val_t(alignment));
#else
    std::free(ptr);
#endif
}

#if defined(MKXPZ_DARWIN_NO_ALIGNED_OPERATOR_NEW)
void *operator new(size_t size, std::align_val_t alignment) {
    void *ptr = mkxp_aligned_malloc((size_t)alignment, size);
    if (ptr == nullptr) {
        MKXPZ_THROW(std::bad_alloc());
    }
    return ptr;
}

void *operator new(size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept {
    return mkxp_aligned_malloc((size_t)alignment, size);
}

void operator delete(void *ptr, std::align_val_t alignment) noexcept {
    mkxp_aligned_free(ptr, (size_t)alignment);
}

void operator delete(void *ptr, std::align_val_t alignment, const std::nothrow_t &) noexcept {
    mkxp_aligned_free(ptr, (size_t)alignment);
}
#endif

#if defined(MKXPZ_DEVKITARM_NO_PTHREAD_H_MUTEX) || defined(MKXPZ_DEVKITARM_NO_PTHREAD_H_THREAD)
extern "C" {
    void LightLock_Init(_LOCK_T *);
    void LightLock_Lock(_LOCK_T *);
    void LightLock_Unlock(_LOCK_T *);
    int LightLock_TryLock(_LOCK_T *);
    void RecursiveLock_Init(_LOCK_RECURSIVE_T *);
    void RecursiveLock_Lock(_LOCK_RECURSIVE_T *);
    void RecursiveLock_Unlock(_LOCK_RECURSIVE_T *);
    int RecursiveLock_TryLock(_LOCK_RECURSIVE_T *);
    void CondVar_Init(mkxp_cond_t *);
    void CondVar_Wait(mkxp_cond_t *, _LOCK_T *);
    void CondVar_WakeUp(mkxp_cond_t *, int32_t);
    uint32_t APT_CheckNew3DS(bool *out);
    uint32_t svcGetThreadPriority(int32_t *out, uint32_t handle);
    void *threadCreate(void (*entrypoint)(void *), void *arg, size_t stack_size, int prio, int core_id, bool detached);
    void *threadJoin(void *thread, uint64_t timeout_ns);
    void threadFree(void *thread);
    void *threadGetCurrent(void);
};
static bool mutex_inited = false;
static _LOCK_T safe_double_thread_launch;
static void *(*start_routine_jump)(void *);
static void ctr_thread_launcher(void *data) {
    // Copied from https://github.com/libretro/libretro-common/blob/master/rthreads/ctr_pthread.h (licensed MIT)
    void *(*start_routine_jump_safe)(void*) = start_routine_jump;
    LightLock_Unlock(&safe_double_thread_launch);
    start_routine_jump_safe(data);
}
#endif

#if defined(MKXPZ_NO_SEMAPHORE_H) && defined(MKXPZ_NO_DISPATCH_DISPATCH_H) && !defined(MKXPZ_NO_MUTEX)
struct mkxp_sem_private {
    unsigned int value;
    mkxp_mutex_t mutex;
    mkxp_cond_t cond;
};
#endif

extern "C" int mkxp_mutex_init(mkxp_mutex_t *mutex, bool recursive) {
#ifndef MKXPZ_NO_STD_MUTEX
    mutex->recursive = recursive;
    if (recursive) {
        mutex->inner = new std::recursive_mutex;
    } else {
        mutex->inner = new std::mutex;
    }
    return 0;
#elif defined(MKXPZ_DEVKITARM_NO_PTHREAD_H_MUTEX)
    mutex->recursive = recursive;
    if (recursive) {
        RecursiveLock_Init(&mutex->inner.recursive);
    } else {
        LightLock_Init(&mutex->inner.light);
    }
    return 0;
#elif !defined(MKXPZ_NO_PTHREAD_H_MUTEX)
    if (recursive) {
        pthread_mutexattr_t attr;
        if (pthread_mutexattr_init(&attr)) {
            return -1;
        }
        if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE)) {
            return -1;
        }
        return pthread_mutex_init(mutex, &attr);
    } else {
        return pthread_mutex_init(mutex, NULL);
    }
#else
    *mutex = 0;
    return 0;
#endif
}

extern "C" int mkxp_mutex_destroy(mkxp_mutex_t *mutex) {
#ifndef MKXPZ_NO_STD_MUTEX
    if (mutex->recursive) {
        delete (std::recursive_mutex *)mutex->inner;
    } else {
        delete (std::mutex *)mutex->inner;
    }
    return 0;
#elif defined(MKXPZ_DEVKITARM_NO_PTHREAD_H_MUTEX)
    return 0;
#elif !defined(MKXPZ_NO_PTHREAD_H_MUTEX)
    return pthread_mutex_destroy(mutex);
#else
    assert(!*mutex);
    return 0;
#endif
}

extern "C" int mkxp_mutex_lock(mkxp_mutex_t *mutex) {
#ifndef MKXPZ_NO_STD_MUTEX
    if (mutex->recursive) {
        ((std::recursive_mutex *)mutex->inner)->lock();
    } else {
        ((std::mutex *)mutex->inner)->lock();
    }
    return 0;
#elif defined(MKXPZ_DEVKITARM_NO_PTHREAD_H_MUTEX)
    if (mutex->recursive) {
        RecursiveLock_Lock(&mutex->inner.recursive);
    } else {
        LightLock_Lock(&mutex->inner.light);
    }
    return 0;
#elif !defined(MKXPZ_NO_PTHREAD_H_MUTEX)
    return pthread_mutex_lock(mutex);
#else
    ++*mutex;
    return 0;
#endif
}

extern "C" int mkxp_mutex_unlock(mkxp_mutex_t *mutex) {
#ifndef MKXPZ_NO_STD_MUTEX
    if (mutex->recursive) {
        ((std::recursive_mutex *)mutex->inner)->unlock();
    } else {
        ((std::mutex *)mutex->inner)->unlock();
    }
    return 0;
#elif defined(MKXPZ_DEVKITARM_NO_PTHREAD_H_MUTEX)
    if (mutex->recursive) {
        RecursiveLock_Unlock(&mutex->inner.recursive);
    } else {
        LightLock_Unlock(&mutex->inner.light);
    }
    return 0;
#elif !defined(MKXPZ_NO_PTHREAD_H_MUTEX)
    return pthread_mutex_unlock(mutex);
#else
    assert(*mutex);
    --*mutex;
    return 0;
#endif
}

extern "C" int mkxp_cond_init(mkxp_cond_t *cond) {
#ifndef MKXPZ_NO_STD_MUTEX
    *cond = new std::condition_variable_any;
    return 0;
#elif defined(MKXPZ_DEVKITARM_NO_PTHREAD_H_MUTEX)
    CondVar_Init(cond);
    return 0;
#elif !defined(MKXPZ_NO_PTHREAD_H_MUTEX)
    return pthread_cond_init(cond, NULL);
#else
    return 0;
#endif
}

extern "C" int mkxp_cond_destroy(mkxp_cond_t *cond) {
#ifndef MKXPZ_NO_STD_MUTEX
    delete (std::condition_variable_any *)*cond;
    return 0;
#elif defined(MKXPZ_DEVKITARM_NO_PTHREAD_H_MUTEX)
    return 0;
#elif !defined(MKXPZ_NO_PTHREAD_H_MUTEX)
    return pthread_cond_destroy(cond);
#else
    return 0;
#endif
}

extern "C" int mkxp_cond_signal(mkxp_cond_t *cond) {
#ifndef MKXPZ_NO_STD_MUTEX
    ((std::condition_variable_any *)*cond)->notify_one();
    return 0;
#elif defined(MKXPZ_DEVKITARM_NO_PTHREAD_H_MUTEX)
    CondVar_WakeUp(cond, 1);
    return 0;
#elif !defined(MKXPZ_NO_PTHREAD_H_MUTEX)
    return pthread_cond_signal(cond);
#else
    return 0;
#endif
}

extern "C" int mkxp_cond_broadcast(mkxp_cond_t *cond) {
#ifndef MKXPZ_NO_STD_MUTEX
    ((std::condition_variable_any *)*cond)->notify_all();
    return 0;
#elif defined(MKXPZ_DEVKITARM_NO_PTHREAD_H_MUTEX)
    CondVar_WakeUp(cond, -1);
    return 0;
#elif !defined(MKXPZ_NO_PTHREAD_H_MUTEX)
    return pthread_cond_broadcast(cond);
#else
    return 0;
#endif
}

extern "C" int mkxp_cond_wait(mkxp_cond_t *cond, mkxp_mutex_t *mutex) {
#ifndef MKXPZ_NO_STD_MUTEX
    if (mutex->recursive) {
        ((std::condition_variable_any *)*cond)->wait(*(std::recursive_mutex *)mutex->inner);
    } else {
        ((std::condition_variable_any *)*cond)->wait(*(std::mutex *)mutex->inner);
    }
    return 0;
#elif defined(MKXPZ_DEVKITARM_NO_PTHREAD_H_MUTEX)
    MKXPZ_FORCED_ASSERT(!mutex->recursive);
    CondVar_Wait(cond, &mutex->inner.light);
    return 0;
#elif !defined(MKXPZ_NO_PTHREAD_H_MUTEX)
    return pthread_cond_wait(cond, mutex);
#else
    return 0;
#endif
}

extern "C" int mkxp_sem_init(mkxp_sem_t *sem, unsigned int value) {
#ifndef MKXPZ_NO_SEMAPHORE_H
    return sem_init(sem, 0, value);
#elif !defined(MKXPZ_NO_DISPATCH_DISPATCH_H)
    return (*sem = dispatch_semaphore_create(value)) == nullptr ? -1 : 0;
#elif !defined(MKXPZ_NO_MUTEX)
    *sem = (void *)new mkxp_sem_private;
    int mutex_init_result = mkxp_mutex_init(&((struct mkxp_sem_private *)*sem)->mutex, false);
    if (mutex_init_result) {
        return -1;
    }
    int cond_init_result = mkxp_cond_init(&((struct mkxp_sem_private *)*sem)->cond);
    if (cond_init_result) {
        mkxp_mutex_destroy(&((struct mkxp_sem_private *)*sem)->mutex);
        return -1;
    }
    ((struct mkxp_sem_private *)*sem)->value = value;
    return 0;
#else
    *sem = value;
    return 0;
#endif
}

extern "C" int mkxp_sem_destroy(mkxp_sem_t *sem) {
#ifndef MKXPZ_NO_SEMAPHORE_H
    return sem_destroy(sem);
#elif !defined(MKXPZ_NO_DISPATCH_DISPATCH_H)
    dispatch_release(*sem);
    return 0;
#elif !defined(MKXPZ_NO_MUTEX)
    mkxp_cond_destroy(&((struct mkxp_sem_private *)*sem)->cond);
    mkxp_mutex_destroy(&((struct mkxp_sem_private *)*sem)->mutex);
    delete (struct mkxp_sem_private *)*sem;
    return 0;
#else
    return 0;
#endif
}

extern "C" int mkxp_sem_post(mkxp_sem_t *sem) {
#ifndef MKXPZ_NO_SEMAPHORE_H
    return sem_post(sem);
#elif !defined(MKXPZ_NO_DISPATCH_DISPATCH_H)
    dispatch_semaphore_signal(*sem);
    return 0;
#elif !defined(MKXPZ_NO_MUTEX)
    while (mkxp_mutex_lock(&((struct mkxp_sem_private *)*sem)->mutex)) {}
    ++((struct mkxp_sem_private *)*sem)->value;
    mkxp_cond_signal(&((struct mkxp_sem_private *)*sem)->cond);
    mkxp_mutex_unlock(&((struct mkxp_sem_private *)*sem)->mutex);
    return 0;
#else
    ++*sem;
    return 0;
#endif
}

extern "C" int mkxp_sem_wait(mkxp_sem_t *sem) {
#ifndef MKXPZ_NO_SEMAPHORE_H
    return sem_wait(sem);
#elif !defined(MKXPZ_NO_DISPATCH_DISPATCH_H)
    return dispatch_semaphore_wait(*sem, DISPATCH_TIME_FOREVER);
#elif !defined(MKXPZ_NO_MUTEX)
    while (mkxp_mutex_lock(&((struct mkxp_sem_private *)*sem)->mutex)) {}
    for (;;) {
        if (((struct mkxp_sem_private *)*sem)->value) {
            --((struct mkxp_sem_private *)*sem)->value;
            mkxp_mutex_unlock(&((struct mkxp_sem_private *)*sem)->mutex);
            return 0;
        }
        while (mkxp_cond_wait(&((struct mkxp_sem_private *)*sem)->cond, &((struct mkxp_sem_private *)*sem)->mutex)) {}
    }
#else
    assert(*sem);
    --*sem;
    return 0;
#endif
}

extern "C" void mkxp_sleep_ms(uint32_t milliseconds) {
#ifndef MKXPZ_NO_STD_THIS_THREAD_SLEEP_FOR
    std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(milliseconds));
#elif !defined(MKXPZ_NO_USLEEP)
    usleep((useconds_t)1000 * (useconds_t)milliseconds);
#elif !defined(MKXPZ_NO_NANOSLEEP)
    struct timespec t;
    t.tv_sec = milliseconds / 1000;
    t.tv_nsec = milliseconds % 1000;
    t.tv_nsec *= 1000000;
    nanosleep(&t, nullptr);
#endif
}

#ifndef MKXPZ_NO_THREAD
extern "C" int mkxp_thread_create(mkxp_thread_t *thread, void *(*func)(void *), void *arg) {
#ifndef MKXPZ_NO_STD_THREAD
    *thread = new std::thread(func, arg);
    return 0;
#elif defined(MKXPZ_DEVKITARM_NO_PTHREAD_H_THREAD)
    // Copied from https://github.com/libretro/libretro-common/blob/master/rthreads/ctr_pthread.h (licensed MIT)
    int32_t prio = 0;
    void *new_ctr_thread;
    int procnum = -2; /* use default cpu */
    bool isNew3DS;

    APT_CheckNew3DS(&isNew3DS);

    if (isNew3DS)
        procnum = 2;

    if (!mutex_inited)
    {
        LightLock_Init(&safe_double_thread_launch);
        mutex_inited = true;
    }

    /* Must wait if attempting to launch 2 threads at once to prevent corruption of function pointer*/
    while (LightLock_TryLock(&safe_double_thread_launch) != 0);

    svcGetThreadPriority(&prio, 0xFFFF8001);

    start_routine_jump = func;
    new_ctr_thread     = threadCreate(ctr_thread_launcher, arg, 32 * 1024, prio - 1, procnum, false);

    if (!new_ctr_thread)
    {
        LightLock_Unlock(&safe_double_thread_launch);
        return EAGAIN;
    }

    *thread = new_ctr_thread;
    return 0;
#elif !defined(MKXPZ_NO_PTHREAD_H_THREAD)
    return pthread_create(thread, nullptr, func, arg);
#endif
}

extern "C" int mkxp_thread_join(mkxp_thread_t thread) {
#ifndef MKXPZ_NO_STD_THREAD
    ((std::thread *)thread)->join();
    delete (std::thread *)thread;
    return 0;
#elif defined(MKXPZ_DEVKITARM_NO_PTHREAD_H_THREAD)
    // Copied from https://github.com/libretro/libretro-common/blob/master/rthreads/ctr_pthread.h (licensed MIT)
    /*retval is ignored*/
    if (threadJoin(thread, (uint64_t)-1))
       return -1;

    threadFree(thread);

    return 0;
#elif !defined(MKXPZ_NO_PTHREAD_H_THREAD)
    return pthread_join(thread, nullptr);
#endif
}
#endif

extern "C" mkxp_thread_id_t mkxp_thread_self(void) {
#ifndef MKXPZ_NO_STD_THREAD
    static_assert(sizeof(std::thread::id) <= sizeof(mkxp_thread_id_t), "`std::thread::id` is too big to fit in `mkxp_thread_id_t`");
    mkxp_thread_id_t output = 0;
    std::thread::id input = std::this_thread::get_id();
    std::memcpy(&output, &input, sizeof(std::thread::id));
    return output;
#elif defined(MKXPZ_DEVKITARM_NO_PTHREAD_H_THREAD)
    return threadGetCurrent();
#elif !defined(MKXPZ_NO_PTHREAD_H_THREAD)
    return pthread_self();
#else
    return 0;
#endif
}
