/*
** ruby-bindings.h
**
** This file is part of mkxp.
**
** Copyright (C) 2024 - 2026 The mkxp-z authors
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

/* This file contains bindings that expose low-level functionality of the Ruby VM to the outside of the sandbox it's running in. They are used by sandbox-bindgen. */

#ifndef SANDBOX_RUBY_BINDINGS_H
#define SANDBOX_RUBY_BINDINGS_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/times.h>
#include <unistd.h>
#include "thread_none.h"
#include "wasm/asyncify.h"
#include "wasm/fiber.h"
#include "wasm/machine.h"
#include "wasm/setjmp.h"

#define MKXP_SANDBOX_API __attribute__((__visibility__("default")))

MKXP_SANDBOX_API struct __rb_wasm_asyncify_jmp_buf mkxp_sandbox_async_buf;
MKXP_SANDBOX_API rb_thread_t *mkxp_sandbox_thread = NULL;
MKXP_SANDBOX_API void (*mkxp_sandbox_fiber_entry_point)(void *, void *) = NULL;
MKXP_SANDBOX_API void *mkxp_sandbox_fiber_arg0 = NULL;
MKXP_SANDBOX_API void *mkxp_sandbox_fiber_arg1 = NULL;
MKXP_SANDBOX_API char mkxp_sandbox_cwd[PATH_MAX] = {0};

/* This function should be called immediately after initializing the sandbox to perform initialization, before calling any other functions.
 * The arguments to this function are the Ruby GC parameters.
 * Each one can be set to 0 to leave the corresponding GC parameters at the default value, or to anything else to set the parameter to the given value. */
MKXP_SANDBOX_API void mkxp_sandbox_init(size_t heap_free_slots, double growth_factor, size_t growth_max_slots, double heap_free_slots_min_ratio, double heap_free_slots_goal_ratio, double heap_free_slots_max_ratio, double oldobject_limit_factor, size_t malloc_limit_min, size_t malloc_limit_max, double malloc_limit_growth_factor, size_t oldmalloc_limit_min, size_t oldmalloc_limit_max, double oldmalloc_limit_growth_factor) {
    void __wasm_call_ctors(void); /* Defined by the LLVM linker */
    __wasm_call_ctors();

    void async_buf_init(struct __rb_wasm_asyncify_jmp_buf *); /* Defined in wasm/setjmp.c in Ruby source code */
    async_buf_init(&mkxp_sandbox_async_buf);

    if (heap_free_slots != 0) gc_params.heap_free_slots = heap_free_slots;
    if (growth_factor != 0) gc_params.growth_factor = growth_factor;
    if (growth_max_slots != 0) gc_params.growth_max_slots = growth_max_slots;
    if (heap_free_slots_min_ratio != 0) gc_params.heap_free_slots_min_ratio = heap_free_slots_min_ratio;
    if (heap_free_slots_goal_ratio != 0) gc_params.heap_free_slots_goal_ratio = heap_free_slots_goal_ratio;
    if (heap_free_slots_max_ratio != 0) gc_params.heap_free_slots_max_ratio = heap_free_slots_max_ratio;
    if (oldobject_limit_factor != 0) gc_params.oldobject_limit_factor = oldobject_limit_factor;
    if (malloc_limit_min != 0) gc_params.malloc_limit_min = malloc_limit_min;
    if (malloc_limit_max != 0) gc_params.malloc_limit_max = malloc_limit_max;
    if (malloc_limit_growth_factor != 0) gc_params.malloc_limit_growth_factor = malloc_limit_growth_factor;
    if (oldmalloc_limit_min != 0) gc_params.oldmalloc_limit_min = oldmalloc_limit_min;
    if (oldmalloc_limit_max != 0) gc_params.oldmalloc_limit_max = oldmalloc_limit_max;
    if (oldmalloc_limit_growth_factor != 0) gc_params.oldmalloc_limit_growth_factor = oldmalloc_limit_growth_factor;
}

/* Exposes the `malloc()` function. */
MKXP_SANDBOX_API void *mkxp_sandbox_malloc(size_t size) {
    return malloc(size);
}

/* Exposes the `free()` function. */
MKXP_SANDBOX_API void mkxp_sandbox_free(void *ptr) {
    free(ptr);
}

/* The offset of the `data` field within a `struct RTypedData`. */
MKXP_SANDBOX_API size_t mkxp_sandbox_rtypeddata_data_offset = offsetof(struct RTypedData, data);

/* Calls the `dmark()` function from a `struct RTypedData *` on a given memory location. */
MKXP_SANDBOX_API void mkxp_sandbox_rtypeddata_dmark(struct RTypedData *data, void *ptr) {
    if (data->type->function.dmark != NULL) {
        data->type->function.dmark(ptr);
    }
}

/* Calls the `dfree()` function from a `struct RTypedData *` on a given memory location. */
MKXP_SANDBOX_API void mkxp_sandbox_rtypeddata_dfree(struct RTypedData *data, void *ptr) {
    if (data->type->function.dfree != NULL) {
        data->type->function.dfree(ptr);
    }
}

/* Calls the `dsize()` function from a `struct RTypedData *` on a given memory location. */
MKXP_SANDBOX_API size_t mkxp_sandbox_rtypeddata_dsize(struct RTypedData *data, const void *ptr) {
    if (data->type->function.dsize != NULL) {
        return data->type->function.dsize(ptr);
    } else {
        return 0;
    }
}

/* Calls the `dcompact()` function from a `struct RTypedData *` on a given memory location. */
MKXP_SANDBOX_API void mkxp_sandbox_rtypeddata_dcompact(struct RTypedData *data, void *ptr) {
    if (data->type->function.dcompact != NULL) {
        data->type->function.dcompact(ptr);
    }
}

/* Calls `chdir()` and returns whether or not the call succeeded. */
MKXP_SANDBOX_API bool mkxp_sandbox_chdir(const char *path) {
    return chdir(path) == 0;
}

/* Calls `getcwd()` on `mkxp_sandbox_cwd` and returns whether or not the call succeeded. */
MKXP_SANDBOX_API bool mkxp_sandbox_getcwd(void) {
    return getcwd(mkxp_sandbox_cwd, PATH_MAX) != NULL;
}

static void mkxp_sandbox_update_fiber(void) {
#ifdef MKXPZ_RUBY_HAVE_USER_THREADS
    rb_thread_t *th = GET_THREAD();
    th->nt->fiber_entry_point = mkxp_sandbox_fiber_entry_point;
    th->nt->fiber_arg0 = mkxp_sandbox_fiber_arg0;
    th->nt->fiber_arg1 = mkxp_sandbox_fiber_arg1;
#endif /* MKXPZ_RUBY_HAVE_USER_THREADS */
}

#ifdef MKXPZ_RUBY_HAVE_USER_THREADS
static rb_thread_t **mkxp_thread_priorityqueue = NULL;
static size_t mkxp_thread_priorityqueue_size = 0;
static size_t mkxp_thread_priorityqueue_capacity = 4;
static uint64_t mkxp_thread_counter = 0;
static bool mkxp_thread_switching;

struct mkxp_zombie_node {
    rb_thread_t *th;
    struct mkxp_zombie_node *prev;
    struct mkxp_zombie_node *next;
};

static struct mkxp_zombie_node *mkxp_thread_zombies = NULL;

static void mkxp_thread_stop_waiting(rb_thread_t *);

/* Returns the current monotonic clock time in nanoseconds. */
uint64_t mkxp_thread_timestamp(void) {
    struct tms buffer;
    return times(&buffer);
}

/* Returns true if a should be scheduled before b, otherwise false. */
static inline bool mkxp_thread_compare(const rb_thread_t *a, const rb_thread_t *b) {
    if (a->nt->timestamp < b->nt->timestamp) {
        return true;
    }
    if (a->nt->timestamp > b->nt->timestamp) {
        return false;
    }
    return a->nt->counter < b->nt->counter;
}

/* Returns true if the current timestamp is greater than or equal to the timestamp a thread is scheduled to run at, otherwise false. */
bool mkxp_thread_is_ready(const rb_thread_t *thread) {
    return thread != NULL && mkxp_thread_timestamp() + 1 >= thread->nt->timestamp;
}

/* Returns the next thread in the queue ready to be scheduled, or null if there isn't one. */
static rb_thread_t *mkxp_thread_priorityqueue_peek(void) {
    if (mkxp_thread_priorityqueue_size == 0) {
        return NULL;
    }
    rb_thread_t *th = mkxp_thread_priorityqueue[0];
    return mkxp_thread_is_ready(th) ? th : NULL;
}

/* Returns true if the thread queue contains the given thread even if it isn't ready to be scheduled yet, otherwise false. */
static bool mkxp_thread_priorityqueue_contains(const rb_thread_t *thread) {
    return thread != NULL && thread->nt->priorityqueue_index < mkxp_thread_priorityqueue_size && mkxp_thread_priorityqueue[thread->nt->priorityqueue_index] == thread;
}

/* Swaps the threads at the given indices of the queue. Returns true if successful or false otherwise. */
static bool mkxp_thread_priorityqueue_swap_indices(size_t index1, size_t index2) {
    if (index1 >= mkxp_thread_priorityqueue_size || index2 >= mkxp_thread_priorityqueue_size) {
        return false;
    }

    {
        size_t index = mkxp_thread_priorityqueue[index1]->nt->priorityqueue_index;
        mkxp_thread_priorityqueue[index1]->nt->priorityqueue_index = mkxp_thread_priorityqueue[index2]->nt->priorityqueue_index;
        mkxp_thread_priorityqueue[index2]->nt->priorityqueue_index = index;
    }

    {
        rb_thread_t *thread = mkxp_thread_priorityqueue[index1];
        mkxp_thread_priorityqueue[index1] = mkxp_thread_priorityqueue[index2];
        mkxp_thread_priorityqueue[index2] = thread;
    }

    return true;
}

/* Repeatedly swaps the thread at the given index with its parent thread in the priority queue until the parent thread should not be scheduled after the given thread. */
static void mkxp_thread_priorityqueue_bubble_up(size_t index) {
    if (index >= mkxp_thread_priorityqueue_size) {
        return;
    }

    while (index > 0) {
        size_t parent_index = (index - 1) / 2;
        if (!mkxp_thread_compare(mkxp_thread_priorityqueue[index], mkxp_thread_priorityqueue[parent_index])) {
            break;
        }
        mkxp_thread_priorityqueue_swap_indices(parent_index, index);
        index = parent_index;
    }
}

/* Repeatedly swaps the thread at the given index with one of its child threads in the priority queue until the given thread should not be scheduled after any of its child threads. */
static void mkxp_thread_priorityqueue_bubble_down(size_t index) {
    if (index >= mkxp_thread_priorityqueue_size) {
        return;
    }

    for (;;) {
        size_t left_child_index = index * 2 + 1;
        if (left_child_index >= mkxp_thread_priorityqueue_size || left_child_index <= index) {
            break;
        }
        size_t right_child_index = left_child_index + 1;
        if (right_child_index >= mkxp_thread_priorityqueue_size || right_child_index <= index) {
            if (!mkxp_thread_compare(mkxp_thread_priorityqueue[left_child_index], mkxp_thread_priorityqueue[index])) {
                break;
            }
            mkxp_thread_priorityqueue_swap_indices(index, left_child_index);
            index = left_child_index;
        } else {
            if (!mkxp_thread_compare(mkxp_thread_priorityqueue[left_child_index], mkxp_thread_priorityqueue[index]) && !mkxp_thread_compare(mkxp_thread_priorityqueue[right_child_index], mkxp_thread_priorityqueue[index])) {
                break;
            }
            if (!mkxp_thread_compare(mkxp_thread_priorityqueue[right_child_index], mkxp_thread_priorityqueue[left_child_index])) {
                mkxp_thread_priorityqueue_swap_indices(index, left_child_index);
                index = left_child_index;
            } else {
                mkxp_thread_priorityqueue_swap_indices(index, right_child_index);
                index = right_child_index;
            }
        }
    }
}

/* Adds a thread to the queue if it isn't already in the queue. */
static void mkxp_thread_priorityqueue_push(rb_thread_t *thread) {
    if (mkxp_thread_priorityqueue == NULL) {
        mkxp_thread_priorityqueue = (rb_thread_t **)ruby_xmalloc(mkxp_thread_priorityqueue_capacity * sizeof(rb_thread_t *));
    }

    if (mkxp_thread_priorityqueue_contains(thread)) {
        return;
    }

    if (mkxp_thread_priorityqueue_size == mkxp_thread_priorityqueue_capacity) {
        assert(mkxp_thread_priorityqueue_capacity * 2 * sizeof(rb_thread_t *) > mkxp_thread_priorityqueue_capacity * sizeof(rb_thread_t *));
        mkxp_thread_priorityqueue_capacity *= 2;
        mkxp_thread_priorityqueue = ruby_xrealloc(mkxp_thread_priorityqueue, mkxp_thread_priorityqueue_capacity * sizeof(rb_thread_t *));
    }

    thread->nt->priorityqueue_index = mkxp_thread_priorityqueue_size;
    mkxp_thread_priorityqueue[mkxp_thread_priorityqueue_size++] = thread;
    mkxp_thread_priorityqueue_bubble_up(mkxp_thread_priorityqueue_size - 1);
}

/* This must be called after the priority of a thread in the queue is modified. */
static void mkxp_thread_priorityqueue_update(rb_thread_t *thread) {
    if (!mkxp_thread_priorityqueue_contains(thread)) {
        return;
    }

    mkxp_thread_priorityqueue_bubble_up(thread->nt->priorityqueue_index);
    mkxp_thread_priorityqueue_bubble_down(thread->nt->priorityqueue_index);
}

/* Removes a thread from the queue if it exists in the queue. Returns the thread if it existed in the queue and was removed or null otherwise. */
static rb_thread_t *mkxp_thread_priorityqueue_remove(rb_thread_t *thread) {
    if (!mkxp_thread_priorityqueue_contains(thread)) {
        return NULL;
    }

    mkxp_thread_stop_waiting(thread);

    size_t index = thread->nt->priorityqueue_index;
    mkxp_thread_priorityqueue_swap_indices(thread->nt->priorityqueue_index, mkxp_thread_priorityqueue_size - 1);
    --mkxp_thread_priorityqueue_size;
    if (index < mkxp_thread_priorityqueue_size) {
        if (mkxp_thread_compare(thread, mkxp_thread_priorityqueue[index])) {
            mkxp_thread_priorityqueue_bubble_down(index);
        } else {
            mkxp_thread_priorityqueue_bubble_up(index);
        }
    }

    if (mkxp_thread_priorityqueue_size > 4 && mkxp_thread_priorityqueue_size == mkxp_thread_priorityqueue_capacity / 4) {
        mkxp_thread_priorityqueue_capacity = mkxp_thread_priorityqueue_size;
        mkxp_thread_priorityqueue = ruby_xrealloc(mkxp_thread_priorityqueue, mkxp_thread_priorityqueue_capacity * sizeof(rb_thread_t *));
    }

    return thread;
}

static __attribute__((noinline)) void mkxp_thread_switch0(const rb_thread_t *current_thread) {
    if (!mkxp_thread_switching) {
        mkxp_thread_switching = true;
        asyncify_start_unwind(&current_thread->nt->async_buf);
    } else {
        asyncify_stop_rewind();
    }
}

/* Switch to the next thread ready to be scheduled as returned by mkxp_thread_priorityqueue_peek(). */
void mkxp_thread_switch(void) {
    rb_thread_t *current_thread = GET_THREAD();

    {
        rb_thread_t *next_thread = mkxp_thread_priorityqueue_peek();
        if (next_thread == current_thread) {
            mkxp_thread_stop_waiting(current_thread);
            return;
        }
    }

    mkxp_thread_switching = false;
    mkxp_thread_switch0(current_thread);

    mkxp_thread_stop_waiting(current_thread);
}

/* Schedules a thread to never run. */
static void mkxp_thread_schedule_never(rb_thread_t *th) {
    th->nt->timestamp = -1;
    mkxp_thread_priorityqueue_update(th);
}

/* Schedules a thread to run again after all other threads currently scheduled to run on or before the given timestamp have been run. */
static void mkxp_thread_schedule_at(rb_thread_t *th, uint64_t timestamp) {
    if (timestamp == (uint64_t)-1) {
        mkxp_thread_schedule_never(th);
        return;
    }
    th->nt->counter = mkxp_thread_counter++;
    th->nt->timestamp = timestamp + 1;
    mkxp_thread_priorityqueue_update(th);
}

/* Schedules a thread to run again after all other threads scheduled to run on or before the current timestamp have been run. */
void mkxp_thread_schedule(rb_thread_t *th) {
    mkxp_thread_schedule_at(th, mkxp_thread_timestamp());
}

/* Schedules a thread to be run immediately after the current thread. */
void mkxp_thread_schedule_now(rb_thread_t *th) {
    th->nt->counter = ~mkxp_thread_counter++;
    th->nt->timestamp = 0;
    mkxp_thread_priorityqueue_update(th);
}

/* Removes a thread from the scheduler. This will never be called on the main thread. */
void mkxp_thread_unschedule(rb_thread_t *th) {
    mkxp_thread_priorityqueue_remove(th);
    if (th == GET_THREAD()) {
        mkxp_thread_switch();
    }
}

/* This is called when the main thread is created. The `nt` field will be allocated and all the fields will be zero-initialized beforehand. The VM stack will also have been initialized using `rb_ec_initialize_vm_stack()` beforehand. */
void mkxp_thread_main_initialize(rb_thread_t *th) {
    /* Initialize the main thread */
    void async_buf_init(struct __rb_wasm_asyncify_jmp_buf *); /* Defined in wasm/setjmp.c in Ruby source code */
    async_buf_init(&th->nt->async_buf);
    th->nt->thread_id = th->nt;

    /* Set the main thread as the current thread */
    ruby_thread_set_native(th);

    /* Push the main thread to the thread queue */
    mkxp_thread_schedule(th);
    mkxp_thread_priorityqueue_push(th);
}

/* This is called when a non-main thread is created. The `nt` field will not be allocated beforehand, so make sure to do it here. You also need to initialize the VM stack using `rb_ec_initialize_vm_stack()` here. */
void mkxp_thread_initialize(rb_thread_t *th) {
    /* Allocate the `nt` field */
    th->nt = ZALLOC(struct rb_native_thread);

    /* Initialize the thread */
    void async_buf_init(struct __rb_wasm_asyncify_jmp_buf *); /* Defined in wasm/setjmp.c in Ruby source code */
    async_buf_init(&th->nt->async_buf);
    th->nt->thread_id = th->nt;
    th->nt->needs_start = true;

    /* Initialize the VM stack */
    {
        size_t vm_stack_word_size = th->vm->default_params.thread_vm_stack_size / sizeof(VALUE);
        void *vm_stack = ruby_xmalloc(vm_stack_word_size * sizeof(VALUE));
        th->nt->vm_stack_start = vm_stack;
        rb_ec_initialize_vm_stack(th->ec, vm_stack, vm_stack_word_size);
    }

    /* Push the new thread to the thread queue */
    mkxp_thread_schedule_now(th);
    mkxp_thread_priorityqueue_push(th);

    /* Switch to the new thread */
    mkxp_thread_switch();
}

/* This is called to initialize the main thread's machine stack. */
void mkxp_thread_main_stack_initialize(rb_thread_t *th) {
    th->ec->machine.stack_maxsize = (size_t)(th->ec->machine.stack_start = rb_wasm_stack_get_base());
}

/* This is called to initialize a non-main thread's machine stack. */
void mkxp_thread_stack_initialize(rb_thread_t *th) {
    size_t stack_maxsize = (size_t)rb_wasm_stack_get_base();
    void *stack_end = ruby_xmalloc(stack_maxsize);
    void *stack_start = (void *)(((uintptr_t)stack_end + (uintptr_t)stack_maxsize) & ~(uintptr_t)15); /* We round the stack start address down to the nearest multiple of 16 because LLVM requires a stack alignment of 16 on WebAssembly targets */
    assert(stack_start > stack_end);
    stack_maxsize = (uintptr_t)stack_start - (uintptr_t)stack_end;

    th->nt->machine_stack_end = stack_end;
    th->ec->machine.stack_start = stack_start;
    th->ec->machine.stack_maxsize = stack_maxsize;
}

/* Turns a thread into a zombie thread and removes it from the scheduler. The thread will not be the main thread. */
void mkxp_thread_to_zombie(rb_thread_t *th) {
    /* Add the thread to the list of zombie threads */
    struct mkxp_zombie_node *node = (struct mkxp_zombie_node *)ruby_xmalloc(sizeof(struct mkxp_zombie_node));
    node->th = th;
    node->prev = NULL;
    node->next = mkxp_thread_zombies;
    if (mkxp_thread_zombies != NULL) {
        mkxp_thread_zombies->prev = node;
    }
    th->nt->zombie_node = node;

    mkxp_thread_unschedule(th);
}

/* Calls `rb_gc_mark(th->self)` on each zombie thread `th` in existence that has not yet been destroyed. */
void mkxp_thread_mark_zombies(void) {
    for (struct mkxp_zombie_node *node = mkxp_thread_zombies; node != NULL; node = node->next) {
        rb_gc_mark(node->th->self);
    }
}

/* Destroys a thread. You need to destroy the `nt` field of the thread, the VM stack and the machine stack as well. The thread will not be the main thread. */
void mkxp_thread_destroy(rb_thread_t *th) {
    assert(th != GET_THREAD());

    mkxp_thread_priorityqueue_remove(th);

    if (th->nt->zombie_node != NULL) {
        struct mkxp_zombie_node *node = (struct mkxp_zombie_node *)th->nt->zombie_node;

        if (node->prev != NULL) {
            node->prev->next = node->next;
        }

        if (node->next != NULL) {
            node->next->prev = node->prev;
        }

        if (mkxp_thread_zombies == node) {
            mkxp_thread_zombies = node->next;
        }

        ruby_xfree(node);
        th->nt->zombie_node = NULL;
    }

    if (th->nt->machine_stack_end != NULL) {
        ruby_xfree(th->nt->machine_stack_end);
    }

    if (th->nt->vm_stack_start != NULL) {
        ruby_xfree(th->nt->vm_stack_start);
    }

    ruby_xfree(th->nt);
}

/* Prematurely wakes up a thread from sleep or waiting on a condition variable. */
static void mkxp_thread_wakeup(void *th) {
    ((rb_thread_t *)th)->status = THREAD_RUNNABLE;

    mkxp_thread_stop_waiting((rb_thread_t *)th);

    if ((rb_thread_t *)th == GET_THREAD()) {
        return;
    }

    mkxp_thread_schedule_now((rb_thread_t *)th);
    mkxp_thread_switch();
}

/* Blocks the given thread for the given number of nanoseconds. Before the duration passes, it should be possible to wake up the thread and cancel any remaining sleep time by calling `th->unblock.func(th->unblock.arg)` on the thread `th`. */
void mkxp_thread_sleep(rb_thread_t *th, uint64_t duration) {
    if (__builtin_add_overflow(mkxp_thread_timestamp(), duration, &duration)) {
        duration = -1;
    }
    mkxp_thread_schedule_at(th, duration);

    /* Allow the thread to be unblocked from a different thread */
    th->unblock.func = mkxp_thread_wakeup;
    th->unblock.arg = th;

    /* If the thread is the currently active one, switch to another thread */
    if (th == GET_THREAD()) {
        mkxp_thread_switch();
    }
}

struct mkxp_mutex_node {
    rb_thread_t *th;
    struct mkxp_mutex_node *prev;
    struct mkxp_mutex_node *next;
};

struct rb_nativethread_lock_t {
    struct mkxp_mutex_node *wait_queue;
    bool locked;
};

/* Creates a new unlocked mutex. */
rb_nativethread_lock_t mkxp_thread_mutex_initialize(void) {
    return ZALLOC(struct rb_nativethread_lock_t);
}

/* Destroys an unlocked mutex. */
void mkxp_thread_mutex_destroy(rb_nativethread_lock_t mutex) {
    assert(!mutex->locked);
    assert(mutex->wait_queue == NULL);
    ruby_xfree(mutex);
}

/* Locks a mutex and returns zero if it is unlocked. Otherwise returns a nonzero value and does nothing else. */
int mkxp_thread_mutex_trylock(rb_nativethread_lock_t mutex) {
    if (mutex->locked) {
        return -EBUSY;
    } else {
        assert(mutex->wait_queue == NULL);
        mutex->locked = true;
        return 0;
    }
}

/* Locks a mutex. If it is already locked, waits until it is unlocked first. */
void mkxp_thread_mutex_lock(rb_nativethread_lock_t mutex) {
    if (!mkxp_thread_mutex_trylock(mutex)) {
        return;
    }

    /* Block the current thread */
    rb_thread_t *th = GET_THREAD();
    mkxp_thread_schedule_never(th);

    /* Push the current thread to the mutex's queue */
    struct mkxp_mutex_node *node = (struct mkxp_mutex_node *)ruby_xmalloc(sizeof(struct mkxp_mutex_node));
    node->th = th;
    node->prev = NULL;
    node->next = mutex->wait_queue;
    if (mutex->wait_queue != NULL) {
        mutex->wait_queue->prev = node;
    }
    mutex->wait_queue = node;
    th->nt->mutex = mutex;
    th->nt->mutex_node = node;

    /* Switch to another thread */
    mkxp_thread_switch();
}

static bool mkxp_thread_mutex_unlock0(rb_nativethread_lock_t mutex) {
    if (!mutex->locked) {
        return true;
    }

    /* If there aren't any threads waiting on the mutex, just unlock it */
    if (mutex->wait_queue == NULL) {
        mutex->locked = false;
        return true;
    }

    /* Remove and unblock one thread from the mutex's queue */
    struct mkxp_mutex_node *node = mutex->wait_queue;
    rb_thread_t *th = node->th;
    assert(th->nt->mutex == mutex);
    assert(th->nt->mutex_node == node);
    mkxp_thread_stop_waiting(th);
    ruby_xfree(node);
    mkxp_thread_schedule_now(th);

    return false;
}

/* Unlocks a mutex. The mutex must have been locked by the current thread beforehand. */
void mkxp_thread_mutex_unlock(rb_nativethread_lock_t mutex) {
    if (mkxp_thread_mutex_unlock0(mutex)) {
        return;
    }

    /* Yield to the unblocked thread */
    mkxp_thread_switch();
}

struct mkxp_cond_node {
    rb_thread_t *th;
    struct mkxp_cond_node *prev;
    struct mkxp_cond_node *next;
};

struct rb_nativethread_cond_t {
    struct mkxp_cond_node *wait_queue;
};

/* Creates a new condition variable. */
rb_nativethread_cond_t mkxp_thread_cond_initialize(void) {
    return ZALLOC(struct rb_nativethread_cond_t);
}

/* Destroys a condition variable that no threads are waiting on. */
void mkxp_thread_cond_destroy(rb_nativethread_cond_t cond) {
    assert(cond->wait_queue == NULL);
    ruby_xfree(cond);
}

/* Unblocks one thread that is currently waiting on a condition variable. If there are no threads currently waiting on the condition variable, does nothing. */
void mkxp_thread_cond_signal(rb_nativethread_cond_t cond) {
    if (cond->wait_queue == NULL) {
        return;
    }

    /* Remove and unblock one thread from the condition variable's queue */
    struct mkxp_cond_node *node = cond->wait_queue;
    rb_thread_t *th = node->th;
    assert(th->nt->cond == cond);
    assert(th->nt->cond_node == node);
    mkxp_thread_stop_waiting(th);
    ruby_xfree(node);
    mkxp_thread_schedule_now(th);

    /* Yield to the unblocked thread */
    mkxp_thread_switch();
}

/* Unblocks all threads that are currently waiting on a condition variable. If there are no threads currently waiting on the condition variable, does nothing. */
void mkxp_thread_cond_broadcast(rb_nativethread_cond_t cond) {
    if (cond->wait_queue == NULL) {
        return;
    }

    while (cond->wait_queue != NULL) {
        /* Remove and unblock one thread from the condition variable's queue */
        struct mkxp_cond_node *node = cond->wait_queue;
        rb_thread_t *th = node->th;
        assert(th->nt->cond == cond);
        assert(th->nt->cond_node == node);
        mkxp_thread_stop_waiting(th);
        ruby_xfree(node);
        mkxp_thread_schedule_now(th);
    }

    /* Yield to one of the unblocked threads */
    mkxp_thread_switch();
}

static void mkxp_thread_cond_wait0(rb_nativethread_cond_t cond, rb_nativethread_lock_t mutex, uint64_t timestamp) {
    rb_thread_t *th = GET_THREAD();

    /* Push the current thread to the condition variable's queue */
    struct mkxp_cond_node *node = (struct mkxp_cond_node *)ruby_xmalloc(sizeof(struct mkxp_cond_node));
    node->th = th;
    node->prev = NULL;
    node->next = cond->wait_queue;
    if (cond->wait_queue != NULL) {
        cond->wait_queue->prev = node;
    }
    cond->wait_queue = node;
    th->nt->cond = cond;
    th->nt->cond_node = node;

    /* Unlock the mutex */
    mkxp_thread_mutex_unlock0(mutex);

    /* Block the thread */
    mkxp_thread_schedule_at(th, timestamp);

    /* Allow the thread to be unblocked from a different thread */
    th->unblock.func = mkxp_thread_wakeup;
    th->unblock.arg = th;

    /* Switch to another thread */
    mkxp_thread_switch();

    /* Relock the mutex */
    mkxp_thread_mutex_lock(mutex);
}

/* Unlocks a mutex, waits until another thread unblocks the current thread by signalling or broadcasting on a condition variable and then locks the mutex again. The mutex must have been locked beforehand by the current thread. While the thread is waiting, it should be possible to cancel the wait by calling `th->unblock.func(th->unblock.arg)` on the waiting thread `th`; the mutex will still be relocked if this occurs. */
void mkxp_thread_cond_wait(rb_nativethread_cond_t cond, rb_nativethread_lock_t mutex) {
    mkxp_thread_cond_wait0(cond, mutex, -1);
}

/* Unlocks a mutex, waits until another thread unblocks the current thread by signalling or broadcasting on this condition variable or until the given number of milliseconds passes and then locks the mutex again. The mutex must have been locked beforehand by the current thread. While the thread is waiting, it should be possible to cancel the remaining wait duration by calling `th->unblock.func(th->unblock.arg)` on the thread `th`; the mutex will still be relocked if this occurs. */
void mkxp_thread_cond_timedwait(rb_nativethread_cond_t cond, rb_nativethread_lock_t mutex, uint64_t duration) {
    if (__builtin_mul_overflow(duration, (uint64_t)1000000, &duration)) {
        duration = -1;
    }
    if (__builtin_add_overflow(mkxp_thread_timestamp(), duration, &duration)) {
        duration = -1;
    }
    mkxp_thread_cond_wait0(cond, mutex, duration);
}

/* Removes any remaining sleep duration from a thread and removes a thread from the wait queue of any mutex or condition variable it is waiting on. */
static void mkxp_thread_stop_waiting(rb_thread_t *th) {
    if (th->unblock.func == mkxp_thread_wakeup) {
        th->unblock.func = NULL;
        th->unblock.arg = NULL;
    }

    if (th->nt->mutex_node != NULL) {
        struct mkxp_mutex_node *node = (struct mkxp_mutex_node *)th->nt->mutex_node;

        if (node->prev != NULL) {
            node->prev->next = node->next;
        }

        if (node->next != NULL) {
            node->next->prev = node->prev;
        }

        if (th->nt->mutex->wait_queue == node) {
            th->nt->mutex->wait_queue = node->next;
        }

        ruby_xfree(node);
        th->nt->mutex_node = NULL;
    }

    if (th->nt->cond_node != NULL) {
        struct mkxp_cond_node *node = (struct mkxp_cond_node *)th->nt->cond_node;

        if (node->prev != NULL) {
            node->prev->next = node->next;
        }

        if (node->next != NULL) {
            node->next->prev = node->prev;
        }

        if (th->nt->cond->wait_queue == node) {
            th->nt->cond->wait_queue = node->next;
        }

        ruby_xfree(node);
        th->nt->cond_node = NULL;
    }
}
#endif /* MKXPZ_RUBY_HAVE_USER_THREADS */

/* This function drives Ruby's asynchronous runtime. It's based on the `rb_wasm_rt_start()` function from wasm/runtime.c in the Ruby source code.
 * After calling `mkxp_sandbox_run_threads()` or any function that starts with `rb_` or `ruby_` other than `ruby_sysinit()`, you need to call `mkxp_sandbox_yield()`.
 * Possible return values for `mkxp_sandbox_yield()` and their meanings:
 *   - If it returns 0, the original function is done running and you may proceed as usual.
 *   - If it returns 1, the original function is not done running. To continue running it, you need to call the original function again with the same arguments and then call `mkxp_sandbox_yield()` again.
 *   - If it returns 2, the original function is not done running. To continue running it, you need to wait until the next video frame and then call `mkxp_sandbox_yield()` again without calling the original function.
 * The return value 2 is only used to handle the edge case where all of the VM threads are sleeping at the same time or if `SANDBOX_YIELD` is used from outside of the root fiber of the main thread, so if you can make sure that doesn't happen, you can safely assume the return value will not be 2.
 * Note: This function must be called from the root fiber of the main thread. */
MKXP_SANDBOX_API __attribute__((noinline)) uint8_t mkxp_sandbox_yield(void) {
    static bool unwound;
    static bool new_fiber_started;
    static void *asyncify_buf;

#ifdef MKXPZ_RUBY_HAVE_USER_THREADS
    static rb_thread_t *next_thread;
    static bool all_threads_sleeping = false;
#endif /* MKXPZ_RUBY_HAVE_USER_THREADS */

    extern void *rb_asyncify_unwind_buf; /* Defined in wasm/setjmp.c in Ruby source code */

    unwound = false;

    while (1) {
#ifdef MKXPZ_RUBY_HAVE_USER_THREADS
        if (!all_threads_sleeping)
#endif /* MKXPZ_RUBY_HAVE_USER_THREADS */
        {
            if (mkxp_sandbox_fiber_entry_point != NULL) {
                mkxp_sandbox_fiber_entry_point(mkxp_sandbox_fiber_arg0, mkxp_sandbox_fiber_arg1);
            }
#ifdef MKXPZ_RUBY_HAVE_USER_THREADS
            else if (mkxp_sandbox_thread != NULL) {
                void mkxp_thread_entry_point(rb_thread_t *);
                mkxp_thread_entry_point(mkxp_sandbox_thread);
            }
#endif /* MKXPZ_RUBY_HAVE_USER_THREADS */
            else if (unwound) {
                return 1;
            }
        }

        unwound = true;

#ifdef MKXPZ_RUBY_HAVE_USER_THREADS
        if (all_threads_sleeping) {
            all_threads_sleeping = false;
        }
        else
#endif /* MKXPZ_RUBY_HAVE_USER_THREADS */
        {
            if (rb_asyncify_unwind_buf == NULL) {
                return (
                    (
                        mkxp_sandbox_fiber_entry_point != NULL
#ifdef MKXPZ_RUBY_HAVE_USER_THREADS
                            || mkxp_sandbox_thread != NULL
#endif /* MKXPZ_RUBY_HAVE_USER_THREADS */
                    )
                        ? 2
                        : 0
                );
            }

            asyncify_stop_unwind();

            if ((asyncify_buf = rb_wasm_handle_jmp_unwind()) != NULL) {
                asyncify_start_rewind(asyncify_buf);
                continue;
            }
            if ((asyncify_buf = rb_wasm_handle_scan_unwind()) != NULL) {
                asyncify_start_rewind(asyncify_buf);
                continue;
            }

            if ((asyncify_buf = rb_wasm_handle_fiber_unwind(&mkxp_sandbox_fiber_entry_point, &mkxp_sandbox_fiber_arg0, &mkxp_sandbox_fiber_arg1, &new_fiber_started)) != NULL) {
                mkxp_sandbox_update_fiber();
                asyncify_start_rewind(asyncify_buf);
                continue;
            } else if (new_fiber_started) {
                mkxp_sandbox_update_fiber();
                continue;
            }
        }

#ifdef MKXPZ_RUBY_HAVE_USER_THREADS
        if ((next_thread = mkxp_thread_priorityqueue_peek()) == NULL) {
            all_threads_sleeping = true;
            return 2;
        }
        GET_THREAD()->nt->machine_stack_pointer = rb_wasm_get_stack_pointer();
        mkxp_sandbox_thread = next_thread == GET_VM()->ractor.main_thread ? NULL : next_thread;
        mkxp_sandbox_fiber_entry_point = next_thread->nt->fiber_entry_point;
        mkxp_sandbox_fiber_arg0 = next_thread->nt->fiber_arg0;
        mkxp_sandbox_fiber_arg1 = next_thread->nt->fiber_arg1;
        ruby_thread_set_native(next_thread);
        if (next_thread->nt->needs_start) {
            next_thread->nt->needs_start = false;
        } else {
            rb_wasm_set_stack_pointer(next_thread->nt->machine_stack_pointer);
            asyncify_start_rewind(&next_thread->nt->async_buf);
        }
        continue;
#else
        return 0;
#endif /* MKXPZ_RUBY_HAVE_USER_THREADS */
    }
}

/* Call this function every once in a while to schedule the threads.
 * Note that this function requires `mkxp_sandbox_yield()` afterwards; see the comments for `mkxp_sandbox_yield()`. */
MKXP_SANDBOX_API void mkxp_sandbox_pump_threads(void) {
#ifdef MKXPZ_RUBY_HAVE_USER_THREADS
    mkxp_thread_schedule(GET_THREAD());
    mkxp_thread_switch();
#endif /* MKXPZ_RUBY_HAVE_USER_THREADS */
}

/* WASK SDK versions later than 21 ship a broken version of mprotect() that causes Ruby to crash on startup.
 * This function replaces the normal mprotect() so that Ruby continues to function when using newer WASI SDK versions. */
int mkxp_sandbox_mprotect(void *addr, size_t len, int prot) {
    return 0;
}

#endif /* SANDBOX_RUBY_BINDINGS_H */
