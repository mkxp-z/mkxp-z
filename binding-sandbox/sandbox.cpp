/*
** sandbox.cpp
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

#include <string>
#include <wasm-rt.h>
#include "wasi.h"
#include <mkxp-sandbox-ruby.h>
#include <mkxp-sandbox-ruby-instantiate-args.h>
#include "mkxp-polyfill.h"
#include "forced-assert.h"
#include "sandbox.h"
#include "core.h"

#define RB (ruby.get())
#define AWAIT(statement) do statement; while (w2c_ruby_mkxp_sandbox_yield(RB))

using namespace mkxp_sandbox;

wasm_ptr_t sandbox::sandbox_malloc(wasm_size_t size) {
    wasm_ptr_t buf = w2c_ruby_mkxp_sandbox_malloc(RB, size);

    // Verify that the returned pointer is non-null and the entire allocated buffer is in valid memory
    wasm_ptr_t buf_end;
    if (buf == 0 || (buf_end = buf + size) < buf || buf_end >= ruby->w2c_memory.size) {
        MKXPZ_THROW(std::bad_alloc());
    }

    return buf;
}

void sandbox::sandbox_free(wasm_ptr_t ptr) {
    w2c_ruby_mkxp_sandbox_free(RB, ptr);
}

sandbox::sandbox(const Config &conf) : ruby(new struct w2c_ruby), wasi(new wasi_instance(ruby)), bindings(ruby), movie(nullptr), yielding(false), sandbox_yield_state(1), trans_map(nullptr), transitioning(false) {
    // Initialize the sandbox
    wasm2c_ruby_instantiate(RB, SANDBOX_INSTANTIATE_ARGS(wasi.get()));

    {
        static const char *const makers[] = {"XP", "VX", "VX Ace"};
        LOG_PRINTF(RETRO_LOG_INFO, "RGSS version %d (RPG Maker %s)\n", conf.rgssVersion, conf.rgssVersion >= 1 && conf.rgssVersion <= 3 ? makers[conf.rgssVersion - 1] : nullptr);
    }

    w2c_ruby_mkxp_sandbox_init(
        RB,
        0,              // heap_free_slots
        1.1,            // growth_factor
        0,              // growth_max_slots
        0,              // heap_free_slots_min_ratio
        0,              // heap_free_slots_goal_ratio
        0,              // heap_free_slots_max_ratio
        0,              // oldobject_limit_factor
        1 * 0x100000,   // malloc_limit_min
        4 * 0x100000,   // malloc_limit_max
        1.1,            // malloc_limit_growth_factor
        4 * 0x100000,   // oldmalloc_limit_min
        8 * 0x100000,   // oldmalloc_limit_max
        1.1             // oldmalloc_limit_growth_factor
    );

    // Change the current working directory to the game directory
    wasm_ptr_t chdir_buf = sandbox_malloc(sizeof("/Game"));
    wasi->strcpy(chdir_buf, "/Game");
    w2c_ruby_mkxp_sandbox_chdir(RB, chdir_buf);
    sandbox_free(chdir_buf);

    // Determine Ruby command-line arguments
    std::vector<std::string> args{"mkxp-z"};
    args.push_back("-EUTF-8"); // Sets the default external encoding to UTF-8
    args.push_back("/Dist/bin/mkxp-z");

    // Copy all the command-line arguments into the sandbox (sandboxed code can't access memory that's outside the sandbox!)
    wasm_ptr_t argv_buf = sandbox_malloc(args.size() * sizeof(wasm_ptr_t));
    for (wasm_size_t i = 0; i < args.size(); ++i) {
        wasm_ptr_t arg_buf = sandbox_malloc(args[i].length() + 1);
        wasi->strcpy(arg_buf, args[i].c_str());
        wasi->ref<wasm_ptr_t>(argv_buf, i) = arg_buf;
    }

    // Pass the command-line arguments to Ruby
    AWAIT(w2c_ruby_ruby_init_stack(RB, w2c_ruby_rb_wasm_get_stack_pointer(RB)));
    AWAIT(w2c_ruby_ruby_init(RB));
    wasm_ptr_t node;
    AWAIT(node = w2c_ruby_ruby_options(RB, args.size(), argv_buf));

    // Print the Ruby version to the log
    AWAIT(w2c_ruby_ruby_show_version(RB));

    // Start up Ruby executable node
    bool valid;
    uint32_t state;
    wasm_ptr_t state_buf = sandbox_malloc(sizeof(wasm_ptr_t));
    AWAIT(valid = w2c_ruby_ruby_executable_node(RB, node, state_buf));
    if (valid) {
        AWAIT(state = w2c_ruby_ruby_exec_node(RB, node));
    }
    MKXPZ_FORCED_ASSERT(valid && state == 0);
    sandbox_free(state_buf);
}

sandbox::~sandbox() {
    set_movie(nullptr);
    if (w2c_ruby_asyncify_get_state(ruby.get()) == 1) {
        w2c_ruby_asyncify_stop_unwind(ruby.get());
    }
    bindings.reset(); // Destroy the bindings before destroying the runtime since the bindings destructor requires the runtime to be alive
    wasm2c_ruby_free(RB);
}

bool sandbox::sandbox_serialize_wasi(void *&data, wasm_size_t &max_size) const {
    return wasi->sandbox_serialize(data, max_size);
}

bool sandbox::sandbox_deserialize_wasi(const void *&data, wasm_size_t &max_size) {
    return wasi->sandbox_deserialize(data, max_size);
}

Movie *sandbox::get_movie_from_main_thread() {
    return movie.load(std::memory_order_relaxed); // No need for synchronization because we always set the movie from the main thread
}

Movie *sandbox::get_movie_from_audio_thread() {
    return movie.load(std::memory_order_seq_cst);
}

void sandbox::set_movie(Movie *new_movie) {
    Movie *old_movie = get_movie_from_main_thread();
    if (old_movie == new_movie) {
        return;
    }
    if (old_movie != nullptr) {
        movie.store(nullptr, std::memory_order_seq_cst);
        AudioMutexGuard guard(movie_mutex);
        Graphics::stopMovie(old_movie);
    }
    if (new_movie != nullptr) {
        movie.store(new_movie, std::memory_order_seq_cst);
    }
}

struct sandbox_str_guard sandbox::getcwd() {
    if (w2c_ruby_mkxp_sandbox_getcwd(ruby.get())) {
        return bindings->str(ruby->w2c_mkxp_sandbox_cwd);
    } else {
        return "/Game";
    }
}
