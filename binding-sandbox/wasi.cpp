/*
** wasi.cpp
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

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstring>
#include <functional>
#include <random>
#include <utility>
#include <mkxp-sandbox-ruby.h>
#include "filesystem.h"
#include "core.h"
#include "wasi.h"
#include "binding-base.h"
#include "sandbox-serial-util.h"

#if !defined(MKXPZ_NO_CLOCK_GETTIME) || !defined(MKXPZ_NO_CLOCK_GETRES)
#  include <time.h>
#elif !defined(MKXPZ_NO_STD_CHRONO_SYSTEM_CLOCK_NOW)
#  include <chrono>
#endif

#if !defined(MKXPZ_NO_GMTIME_R) || !defined(MKXPZ_NO_GMTIME_S) || !defined(MKXPZ_NO_GMTIME_S_WOE32)
#  include <time.h>
#endif

using namespace mkxp_sandbox;

static const std::pair<std::string, std::string> wasi_env[] = {
    {"HOME", "/Save"},
};
static constexpr wasm_size_t wasi_env_size = sizeof wasi_env / sizeof *wasi_env;

// Returns the absolute path obtained by joining the path of the directory corresponding to file descriptor `fd` with the relative path given by `path` and `path_len`.
// Assumes that `fd` corresponds to a `wasi_fd_type::FS` or `wasi_fd_type::FSDIR`.
// Assumes that `path` points to a buffer that is at least as long as `path_len`.
// If the resulting path is not a descendant of the path of `fd`, returns an empty string.
static std::string dir_path_join(const struct wasi_instance *wasi, uint32_t fd, wasm_ptr_t path, wasm_size_t path_len) {
    // Verify that the path is relative
    if (path_len > 0) {
        auto first_character = wasi->ref<char>(path);
        if (first_character == '/' || first_character == '\\') {
            return "";
        }
    }

    std::string joined_path(wasi->fdtable[fd].dir_handle()->path);
    joined_path.push_back('/');
    joined_path.append(wasi->str(path, path_len), path_len);
    joined_path = mkxp_retro::fs->normalize(joined_path.c_str(), false, true);

    // Verify that the joined path is a descendant of the directory corresponding to `fd`
    if (std::strncmp(joined_path.c_str(), wasi->fdtable[fd].dir_handle()->path.c_str(), wasi->fdtable[fd].dir_handle()->path.length()) != 0) {
        return "";
    }

    return joined_path;
}

struct fs_dir *wasi_file_entry::dir_handle() const noexcept {
    return (struct fs_dir *)handle;
}

struct fs_dir_stream *wasi_file_entry::dir_stream() const noexcept {
    return (struct fs_dir_stream *)handle;
}

struct fs_file *wasi_file_entry::file_handle() const noexcept {
    return (struct fs_file *)handle;
}

struct fs_file_stream *wasi_file_entry::file_stream() const noexcept {
    return (struct fs_file_stream *)handle;
}

struct ai_stream *wasi_file_entry::ai_stream() const noexcept {
    return (struct ai_stream *)handle;
}

wasi_instance::wasi_instance(std::shared_ptr<struct w2c_ruby> ruby) : ruby(ruby), prng_buffer_size(0) {
    // Initialize PRNG
    static_assert(sizeof(unsigned int) == sizeof(uint32_t), "unsigned int should be 32 bits");
    static std::random_device dev;
    prng_state = dev();
    prng_state <<= 32U;
    prng_state |= dev();
    std::memset(prng_buffer, 0, 4);

    // Initialize WASI file descriptor table
#if MKXPZ_WASI_VERSION_MAJOR > 0 || MKXPZ_WASI_VERSION_MINOR >= 2
    fdtable.push_back({nullptr, wasi_fd_type::STDIN}); // Push a dummy file descriptor as FD 0 when targeting WASI preview 2 and later because resource 0 is considered to be null
#endif
    fdtable.push_back({nullptr, wasi_fd_type::STDIN});
    fdtable.push_back({nullptr, wasi_fd_type::STDOUT});
    fdtable.push_back({nullptr, wasi_fd_type::STDERR});
    fdtable.push_back({new fs_dir {"/Game", 0, true}, wasi_fd_type::FS});
    fdtable.push_back({new fs_dir {"/Save", 0, true}, wasi_fd_type::FS});
    fdtable.push_back({new fs_dir {"/System", 0, false}, wasi_fd_type::FS});
    fdtable.push_back({new fs_dir {"/Dist", 0, false}, wasi_fd_type::FS});
}

wasi_instance::~wasi_instance() {
    // Flush standard output and standard error
    for (size_t i = 0; i < 2; ++i) {
        if (!stdio_line_buffers[i].empty()) {
            mkxp_retro_log_printf(
                i == 0 ? RETRO_LOG_INFO : RETRO_LOG_WARN,
                i == 0 ? "[mkxp-z stdout] %s\n" : "[mkxp-z stderr] %s\n",
                stdio_line_buffers[i].c_str()
            );
            stdio_line_buffers[i].clear();
        }
    }

    // Close all of the open WASI file descriptors
    for (uint32_t i = fdtable.size(); i > 0;) {
        deallocate_file_descriptor(--i);
    }
}

uint32_t wasi_instance::allocate_file_descriptor(enum wasi_fd_type type, void *handle) {
    if (vacant_fds.empty()) {
        if (fdtable.size() >= UINT32_MAX) {
            MKXPZ_THROW(std::bad_alloc());
        }
        uint32_t fd = fdtable.size();
        fdtable.push_back({handle, type});
        return fd;
    } else {
        uint32_t fd = vacant_fds.minimum();
        vacant_fds.pop_minimum();
        fdtable[fd].handle = handle;
        fdtable[fd].type = type;
        return fd;
    }
}

// Closes a file stream without deallocating its file descriptor.
static void close_file_stream(struct wasi_instance *wasi, uint32_t fd) {
    uint32_t file_fd = wasi->fdtable[fd].file_stream()->root;
    if (file_fd < wasi->fdtable.size() && wasi->fdtable[file_fd].type == wasi_fd_type::FSFILE) {
        wasi->fdtable[file_fd].file_handle()->streams.erase(fd);
    }
    wasi->fdtable[fd].file_stream()->root = 0;
}

void wasi_instance::deallocate_file_descriptor(uint32_t fd) {
    if (fd >= fdtable.size() || fdtable[fd].type == wasi_fd_type::VACANT) {
        return;
    }

    if (fdtable[fd].handle != nullptr) {
        switch (fdtable[fd].type) {
            case wasi_fd_type::FS:
            case wasi_fd_type::FSDIR:
                delete fdtable[fd].dir_handle();
                break;
            case wasi_fd_type::FSFILE:
                for (uint32_t filestream_fd : fdtable[fd].file_handle()->streams) {
                    // Close all file streams that are backed by this file
                    if (filestream_fd < fdtable.size() && fdtable[filestream_fd].type == wasi_fd_type::FSFILESTREAM) {
                        fdtable[filestream_fd].file_stream()->root = 0;
                    }
                }
                delete fdtable[fd].file_handle();
                break;
            case wasi_fd_type::FSDIRSTREAM:
                delete fdtable[fd].dir_stream();
                break;
            case wasi_fd_type::FSFILESTREAM:
                // Remove this file stream from the backing file's set of file streams
                close_file_stream(this, fd);
                delete fdtable[fd].file_stream();
                break;
            case wasi_fd_type::AISTREAM:
                delete fdtable[fd].ai_stream();
                break;
            default:
                break;
        }
    }

    if (fd == fdtable.size() - 1) {
        fdtable.pop_back();
        while (!fdtable.empty() && fdtable.back().type == wasi_fd_type::VACANT) {
            assert(!vacant_fds.empty() && vacant_fds.maximum() == fdtable.size() - 1);
            vacant_fds.pop_maximum();
            fdtable.pop_back();
        }
    } else {
        fdtable[fd] = {nullptr, wasi_fd_type::VACANT};
        vacant_fds.push(fd);
    }
}

void wasi_instance::check_bounds(mkxp_sandbox::wasm_ptr_t address, mkxp_sandbox::wasm_size_t size) const noexcept {
    sandbox_check_bounds(*ruby, address, size);
}

wasm_size_t wasi_instance::strlen(wasm_ptr_t address) const noexcept {
    return sandbox_strlen(*ruby, address);
}

void wasi_instance::strcpy(wasm_ptr_t dst_address, const char *src) const noexcept {
    sandbox_strcpy(*ruby, dst_address, src);
}

void wasi_instance::strncpy_s(wasm_ptr_t dst_address, const char *src, wasm_size_t max_size) const noexcept {
    sandbox_strncpy_s(*ruby, dst_address, src, max_size);
}

struct mkxp_sandbox::sandbox_str_guard wasi_instance::str(wasm_ptr_t address, wasm_size_t max_size) const noexcept {
    return sandbox_str(*ruby, address, max_size);
}

wasm_ptr_t wasi_instance::cabi_alloc_impl(wasm_size_t alignment, wasm_size_t size) const noexcept {
#if MKXPZ_WASI_VERSION_MAJOR > 0 || MKXPZ_WASI_VERSION_MINOR >= 2
    wasm_ptr_t ptr = w2c_ruby_cabi_realloc(ruby.get(), 0, 0, alignment, size);
#else
    wasm_ptr_t ptr = 0;
#endif
    MKXPZ_FORCED_ASSERT(ptr != 0); // The result shouldn't be null even if the size is 0
    check_bounds(ptr, size);
    return ptr;
}

////////////////////////////////////////////////////////////////////////////////
// wasi:cli
////////////////////////////////////////////////////////////////////////////////

extern "C" void w2c_wasi0x3Acli0x2Fenvironment0x4000x2E20x2E0_get0x2Denvironment(struct w2c_wasi0x3Acli0x2Fenvironment0x4000x2E20x2E0 *wasi, wasm_ptr_t result) {
    LOG_PRINT(RETRO_LOG_DEBUG, "wasi:cli/environment@0.2.0::get-environment()\n");

    wasi->check_bounds(result, 2 * sizeof(wasm_ptr_t));

    wasm_ptr_t buf = wasi->cabi_alloc<wasm_ptr_t>(wasi_env_size * 4 * sizeof(wasm_ptr_t));
    wasi->ref<wasm_ptr_t>(result) = buf;
    wasi->ref<wasm_size_t>(result + sizeof(wasm_ptr_t)) = wasi_env_size;

    for (wasm_size_t i = 0; i < wasi_env_size; ++i) {
        wasm_size_t key_len = wasi_env[i].first.length();
        wasm_ptr_t key_buf = wasi->cabi_alloc<char>(key_len);
        wasi->arycpy(key_buf, wasi_env[i].first.c_str(), key_len);

        wasm_size_t val_len = wasi_env[i].second.length();
        wasm_ptr_t val_buf = wasi->cabi_alloc<char>(val_len);
        wasi->arycpy(val_buf, wasi_env[i].second.c_str(), val_len);

        wasi->ref<wasm_ptr_t>(buf) = key_buf;
        buf += sizeof(wasm_ptr_t);
        wasi->ref<wasm_size_t>(buf) = key_len;
        buf += sizeof(wasm_ptr_t);
        wasi->ref<wasm_ptr_t>(buf) = val_buf;
        buf += sizeof(wasm_ptr_t);
        wasi->ref<wasm_size_t>(buf) = val_len;
        buf += sizeof(wasm_ptr_t);
    }
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_environ_get(struct w2c_wasi__snapshot__preview1 *wasi, wasm_ptr_t env, wasm_ptr_t env_buf) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::environ_get(0x%08llx, 0x%08llx)\n", (unsigned long long)env, (unsigned long long)env_buf);

    wasi->check_bounds(env, wasi_env_size * sizeof(wasm_ptr_t));

    wasm_size_t buf_size = 0;
    for (wasm_size_t i = 0; i < wasi_env_size; ++i) {
        buf_size += wasi_env[i].first.length() + wasi_env[i].second.length() + 2;
    }

    wasi->check_bounds(env_buf, buf_size);

    for (wasm_size_t i = 0; i < wasi_env_size; ++i) {
        wasi->ref<wasm_ptr_t>(env) = env_buf;
        env += sizeof(wasm_ptr_t);

        wasi->strcpy(env_buf, wasi_env[i].first.c_str());
        env_buf += wasi_env[i].first.length();
        wasi->ref<char>(env_buf) = '=';
        env_buf += 1;
        wasi->strcpy(env_buf, wasi_env[i].second.c_str());
        env_buf += wasi_env[i].second.length() + 1;
    }

    return WASIP1_ESUCCESS;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_environ_sizes_get(struct w2c_wasi__snapshot__preview1 *wasi, wasm_ptr_t env_size, wasm_ptr_t env_buf_size) {
    LOG_PRINT(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::environ_sizes_get()\n");

    wasi->check_bounds(env_size, 4);
    wasi->check_bounds(env_buf_size, 4);

    wasm_size_t buf_size = 0;
    for (wasm_size_t i = 0; i < wasi_env_size; ++i) {
        buf_size += wasi_env[i].first.length() + wasi_env[i].second.length() + 2;
    }

    wasi->ref<uint32_t>(env_size) = wasi_env_size;
    wasi->ref<uint32_t>(env_buf_size) = buf_size;
    return WASIP1_ESUCCESS;
}

extern "C" void w2c_wasi0x3Acli0x2Fenvironment0x4000x2E20x2E0_get0x2Darguments(struct w2c_wasi0x3Acli0x2Fenvironment0x4000x2E20x2E0 *wasi, wasm_ptr_t result) {
    LOG_PRINT(RETRO_LOG_DEBUG, "wasi:cli/environment@0.2.0::get-arguments()\n");

    wasi->check_bounds(result, 2 * sizeof(wasm_ptr_t));

    wasi->ref<wasm_ptr_t>(result) = wasi->cabi_alloc<wasm_ptr_t>(0);
    wasi->ref<wasm_size_t>(result + sizeof(wasm_ptr_t)) = 0;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_args_get(struct w2c_wasi__snapshot__preview1 *wasi, wasm_ptr_t argv, wasm_ptr_t argv_buf) {
    LOG_PRINT(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::args_get()\n");
    return WASIP1_ESUCCESS;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_args_sizes_get(struct w2c_wasi__snapshot__preview1 *wasi, wasm_ptr_t argc, wasm_ptr_t argv_buf_size) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::args_sizes_get(0x%08llx, 0x%08llx)\n", (unsigned long long)argc, (unsigned long long)argv_buf_size);

    wasi->check_bounds(argc, 4);
    wasi->check_bounds(argv_buf_size, 4);

    wasi->ref<uint32_t>(argc) = 0;
    wasi->ref<uint32_t>(argv_buf_size) = 0;
    return WASIP1_ESUCCESS;
}

extern "C" void w2c_wasi0x3Acli0x2Fexit0x4000x2E20x2E0_exit(struct w2c_wasi0x3Acli0x2Fexit0x4000x2E20x2E0 *wasi, uint32_t rval) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:cli/exit@0.2.0::exit(%d)\n", (int)rval);
    MKXPZ_FORCED_ASSERT_WITH_MESSAGE(false, "Ruby VM terminated unexpectedly");
}

extern "C" void w2c_wasi__snapshot__preview1_proc_exit(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t rval) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::proc_exit(%d)\n", (int)rval);
    MKXPZ_FORCED_ASSERT_WITH_MESSAGE(false, "Ruby VM terminated unexpectedly");
}

extern "C" wasm_resource_t w2c_wasi0x3Acli0x2Fstdin0x4000x2E20x2E0_get0x2Dstdin(struct w2c_wasi0x3Acli0x2Fstdin0x4000x2E20x2E0 *wasi) {
    LOG_PRINT(RETRO_LOG_DEBUG, "wasi:cli/stdin@0.2.0::get-stdin()\n");
    LOG_PRINT(RETRO_LOG_DEBUG, "WASI resource created: output-stream(1) -> standard input stream\n");
    return 1;
}

extern "C" wasm_resource_t w2c_wasi0x3Acli0x2Fstdout0x4000x2E20x2E0_get0x2Dstdout(struct w2c_wasi0x3Acli0x2Fstdout0x4000x2E20x2E0 *wasi) {
    LOG_PRINT(RETRO_LOG_DEBUG, "wasi:cli/stdout@0.2.0::get-stdout()\n");
    LOG_PRINT(RETRO_LOG_DEBUG, "WASI resource created: output-stream(2) -> standard output stream\n");
    return 2;
}

extern "C" wasm_resource_t w2c_wasi0x3Acli0x2Fstderr0x4000x2E20x2E0_get0x2Dstderr(struct w2c_wasi0x3Acli0x2Fstderr0x4000x2E20x2E0 *wasi) {
    LOG_PRINT(RETRO_LOG_DEBUG, "wasi:cli/stderr@0.2.0::get-stderr()\n");
    LOG_PRINT(RETRO_LOG_DEBUG, "WASI resource created: output-stream(3) -> standard error stream\n");
    return 3;
}

extern "C" void w2c_wasi0x3Acli0x2Fterminal0x2Dstdin0x4000x2E20x2E0_get0x2Dterminal0x2Dstdin(struct w2c_wasi0x3Acli0x2Fterminal0x2Dstdin0x4000x2E20x2E0 *wasi, wasm_ptr_t result) {
    LOG_PRINT(RETRO_LOG_DEBUG, "wasi:cli/terminal-stdin@0.2.0::get-terminal-stdin()\n");

    wasi->check_bounds(result, 8);

    wasi->ref<uint8_t>(result) = false;
}

extern "C" void w2c_wasi0x3Acli0x2Fterminal0x2Dstdout0x4000x2E20x2E0_get0x2Dterminal0x2Dstdout(struct w2c_wasi0x3Acli0x2Fterminal0x2Dstdout0x4000x2E20x2E0 *wasi, wasm_ptr_t result) {
    LOG_PRINT(RETRO_LOG_DEBUG, "wasi:cli/terminal-stdout@0.2.0::get-terminal-stdout()\n");

    wasi->check_bounds(result, 8);

    LOG_PRINT(RETRO_LOG_DEBUG, "WASI resource created: terminal-output(2) -> standard output terminal\n");
    wasi->ref<uint8_t>(result) = true;
    wasi->ref<wasm_resource_t>(result + 4) = 2;
}

extern "C" void w2c_wasi0x3Acli0x2Fterminal0x2Dstderr0x4000x2E20x2E0_get0x2Dterminal0x2Dstderr(struct w2c_wasi0x3Acli0x2Fterminal0x2Dstderr0x4000x2E20x2E0 *wasi, wasm_ptr_t result) {
    LOG_PRINT(RETRO_LOG_DEBUG, "wasi:cli/terminal-stderr@0.2.0::get-terminal-stderr()\n");

    wasi->check_bounds(result, 8);

    LOG_PRINT(RETRO_LOG_DEBUG, "WASI resource created: terminal-output(3) -> standard error terminal\n");
    wasi->ref<uint8_t>(result) = true;
    wasi->ref<wasm_resource_t>(result + 4) = 3;
}

extern "C" void w2c_wasi0x3Acli0x2Fterminal0x2Dinput0x4000x2E20x2E0_0x5Bresource0x2Ddrop0x5Dterminal0x2Dinput(struct w2c_wasi0x3Acli0x2Fterminal0x2Dinput0x4000x2E20x2E0 *wasi, wasm_resource_t self) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:cli/terminal-input@0.2.0::[resource-drop]terminal-input(%u)\n", (unsigned int)self);
}

extern "C" void w2c_wasi0x3Acli0x2Fterminal0x2Doutput0x4000x2E20x2E0_0x5Bresource0x2Ddrop0x5Dterminal0x2Doutput(struct w2c_wasi0x3Acli0x2Fterminal0x2Doutput0x4000x2E20x2E0 *wasi, wasm_resource_t self) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:cli/terminal-output@0.2.0::[resource-drop]terminal-output(%u)\n", (unsigned int)self);
}

////////////////////////////////////////////////////////////////////////////////
// wasi:clocks
////////////////////////////////////////////////////////////////////////////////

extern "C" uint64_t w2c_wasi0x3Aclocks0x2Fmonotonic0x2Dclock0x4000x2E20x2E0_now(struct w2c_wasi0x3Aclocks0x2Fmonotonic0x2Dclock0x4000x2E20x2E0 *wasi) {
    //LOG_PRINT(RETRO_LOG_DEBUG, "wasi:clocks/monotonic-clock@0.2.0::now()\n");
    return mkxp_retro::get_ticks_us() * (uint64_t)1000;
}

static std::pair<uint64_t, uint32_t> wall_clock_now_impl() {
#ifndef MKXPZ_NO_CLOCK_GETTIME
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        ts.tv_sec = 0;
        ts.tv_nsec = 0;
    }
    std::pair<uint64_t, uint32_t> now((uint64_t)ts.tv_sec, (uint32_t)ts.tv_nsec);
#elif !defined(MKXPZ_NO_STD_CHRONO_SYSTEM_CLOCK_NOW)
    auto duration = std::chrono::system_clock::now().time_since_epoch();
    auto duration_seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    duration -= duration_seconds;
    std::pair<uint64_t, uint32_t> now((uint64_t)duration_seconds.count(), (uint32_t)std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
#else
    std::pair<uint64_t, uint32_t> now(0, 0);
#endif

#if !defined(MKXPZ_NO_GMTIME_R) || !defined(MKXPZ_NO_GMTIME_S) || !defined(MKXPZ_NO_GMTIME_S_WOE32)
    // Convert the time from UTC to the local timezone
    if (now.first != 0) {
        time_t t1 = (time_t)now.first;
        struct tm buf;
        if (
#ifndef MKXPZ_NO_GMTIME_R
            gmtime_r(&t1, &buf) != nullptr
#elif !defined(MKXPZ_NO_GMTIME_S)
            gmtime_s(&t1, &buf) != nullptr
#elif !defined(MKXPZ_NO_GMTIME_S_WOE32)
            gmtime_s(&buf, &t1) == 0
#endif
        ) {
            time_t t2 = mktime(&buf);
            if (t2 != (time_t)-1) {
                now.first += (uint64_t)t1 - (uint64_t)t2;
            }
        }
    }
#endif

    return now;
}

extern "C" void w2c_wasi0x3Aclocks0x2Fwall0x2Dclock0x4000x2E20x2E0_now(struct w2c_wasi0x3Aclocks0x2Fwall0x2Dclock0x4000x2E20x2E0 *wasi, wasm_ptr_t result) {
    LOG_PRINT(RETRO_LOG_DEBUG, "wasi:clocks/wall-clock@0.2.0::now()\n");

    wasi->check_bounds(result, 16);

    std::pair<uint64_t, uint32_t> now(wall_clock_now_impl());
    wasi->ref<uint64_t>(result) = now.first;
    wasi->ref<uint32_t>(result + 8) = now.second;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_clock_time_get(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t id, uint64_t precision, wasm_ptr_t result) {
    if (id == 0) {
        LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::clock_time_get(%u, %llu)\n", (unsigned int)id, (unsigned long long)precision);
    }

    wasi->check_bounds(result, 8);

    if (id == 0) {
        std::pair<uint64_t, uint32_t> now(wall_clock_now_impl());
        wasi->ref<uint64_t>(result) = now.first * (uint64_t)1000000000U + (uint64_t)now.second;
    } else {
        wasi->ref<uint64_t>(result) = mkxp_retro::get_ticks_us() * (uint64_t)1000;
    }
    return WASIP1_ESUCCESS;
}

extern "C" uint64_t w2c_wasi0x3Aclocks0x2Fmonotonic0x2Dclock0x4000x2E20x2E0_resolution(struct w2c_wasi0x3Aclocks0x2Fmonotonic0x2Dclock0x4000x2E20x2E0 *wasi) {
    LOG_PRINT(RETRO_LOG_DEBUG, "wasi:clocks/monotonic-clock@0.2.0::resolution()\n");
    return 1000;
}

static std::pair<uint64_t, uint32_t> wall_clock_resolution_impl() {
#ifndef MKXPZ_NO_CLOCK_GETRES
    struct timespec ts;
    if (clock_getres(CLOCK_REALTIME, &ts) != 0) {
        ts.tv_sec = 0;
        ts.tv_nsec = 1;
    }
    return {(uint64_t)ts.tv_sec, (uint32_t)ts.tv_nsec};
#else
    return {0, 1};
#endif
}

extern "C" void w2c_wasi0x3Aclocks0x2Fwall0x2Dclock0x4000x2E20x2E0_resolution(struct w2c_wasi0x3Aclocks0x2Fwall0x2Dclock0x4000x2E20x2E0 *wasi, wasm_ptr_t result) {
    LOG_PRINT(RETRO_LOG_DEBUG, "wasi:clocks/wall-clock@0.2.0::resolution()\n");

    wasi->check_bounds(result, 16);

    std::pair<uint64_t, uint32_t> resolution(wall_clock_resolution_impl());
    wasi->ref<uint64_t>(result) = resolution.first;
    wasi->ref<uint32_t>(result + 8) = resolution.second;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_clock_res_get(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t id, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::clock_res_get(%u)\n", (unsigned int)id);

    wasi->check_bounds(result, 8);

    std::pair<uint64_t, uint32_t> resolution(wall_clock_resolution_impl());
    wasi->ref<uint64_t>(result) = resolution.first * (uint64_t)1000000000U + (uint64_t)resolution.second;
    return WASIP1_ESUCCESS;
}

extern "C" wasm_resource_t w2c_wasi0x3Aclocks0x2Fmonotonic0x2Dclock0x4000x2E20x2E0_subscribe0x2Dduration(struct w2c_wasi0x3Aclocks0x2Fmonotonic0x2Dclock0x4000x2E20x2E0 *wasi, uint64_t when) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:clocks/monotonic-clock@0.2.0::subscribe-duration(%llu)\n", (unsigned long long)when);
    LOG_PRINTF(RETRO_LOG_DEBUG, "WASI resource created: pollable(%u) -> monotonic clock\n", (unsigned int)-1);
    return -1;
}

extern "C" wasm_resource_t w2c_wasi0x3Aclocks0x2Fmonotonic0x2Dclock0x4000x2E20x2E0_subscribe0x2Dinstant(struct w2c_wasi0x3Aclocks0x2Fmonotonic0x2Dclock0x4000x2E20x2E0 *wasi, uint64_t when) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:clocks/monotonic-clock@0.2.0::subscribe-instant(%llu)\n", (unsigned long long)when);
    LOG_PRINTF(RETRO_LOG_DEBUG, "WASI resource created: pollable(%u) -> monotonic clock\n", (unsigned int)-1);
    return -1;
}

////////////////////////////////////////////////////////////////////////////////
// wasi:filesystem
////////////////////////////////////////////////////////////////////////////////

template <bool is_write_stream, bool seek_to_end> static void open_stream_impl(struct wasi_instance *wasi, uint32_t fd, uint64_t offset, wasm_ptr_t result) {
    wasi->check_bounds(result, 8);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_INVALID;
            return;

        case wasi_fd_type::FSFILE:
            if (wasi->fdtable.size() >= UINT32_MAX && wasi->vacant_fds.empty()) {
                wasi->ref<uint8_t>(result) = true;
                wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_IO;
                return;
            }
            if (seek_to_end && (wasi->fdtable[fd].file_handle()->file.is_read_open() || wasi->fdtable[fd].file_handle()->file.is_write_open())) {
                offset = (uint64_t)PHYSFS_fileLength(wasi->fdtable[fd].file_handle()->file.is_read_open() ? wasi->fdtable[fd].file_handle()->file.get_read() : wasi->fdtable[fd].file_handle()->file.get_write());
                if (offset == (uint64_t)-1) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_IO;
                    return;
                }
            }
            uint32_t stream_fd = wasi->allocate_file_descriptor(wasi_fd_type::FSFILESTREAM, new fs_file_stream {offset, fd});
            wasi->fdtable[fd].file_handle()->streams.insert(stream_fd);
            if (is_write_stream) {
                LOG_PRINTF(RETRO_LOG_DEBUG, "WASI resource created: output-stream(%u) -> file descriptor %u \"%s\"\n", (unsigned int)stream_fd, (unsigned int)fd, wasi->fdtable[fd].file_handle()->file.path());
            } else {
                LOG_PRINTF(RETRO_LOG_DEBUG, "WASI resource created: input-stream(%u) -> file descriptor %u \"%s\"\n", (unsigned int)stream_fd, (unsigned int)fd, wasi->fdtable[fd].file_handle()->file.path());
            }
            wasi->ref<uint8_t>(result) = false;
            wasi->ref<wasm_resource_t>(result + 4) = stream_fd;
            return;
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Eread0x2Dvia0x2Dstream(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint64_t offset, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.read-via-stream(%u, %llu)\n", (unsigned int)fd, (unsigned long long)offset);
    open_stream_impl<false, false>(wasi, fd, offset, result);
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Ewrite0x2Dvia0x2Dstream(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint64_t offset, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.write-via-stream(%u, %llu)\n", (unsigned int)fd, (unsigned long long)offset);
    open_stream_impl<true, false>(wasi, fd, offset, result);
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Eappend0x2Dvia0x2Dstream(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.append-via-stream(%u)\n", (unsigned int)fd);
    open_stream_impl<true, true>(wasi, fd, 0, result);
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Eadvise(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint64_t offset, uint64_t length, uint32_t advice, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.advise(%u, %llu, %llu, %u)\n", (unsigned int)fd, (unsigned long long)offset, (unsigned long long)length, (unsigned int)advice);

    wasi->ref<uint8_t>(result) = false;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_advise(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, uint64_t offset, uint64_t len, uint32_t advice) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::fd_advise(%u, %llu, %llu, %u)\n", (unsigned int)fd, (unsigned long long)offset, (unsigned long long)len, (unsigned int)advice);
    return WASIP1_ESUCCESS;
}

static void flush_impl(const struct wasi_instance *wasi, uint32_t fd, wasm_ptr_t result) noexcept {
    wasi->check_bounds(result, 2);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_INVALID;
            return;

        case wasi_fd_type::FSFILE:
            if (!wasi->fdtable[fd].file_handle()->file.is_write_open()) {
                wasi->ref<uint8_t>(result) = false;
            } else if (PHYSFS_flush(wasi->fdtable[fd].file_handle()->file.get_write()) == 0) {
                wasi->ref<uint8_t>(result) = true;
                wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_IO;
            } else {
                wasi->ref<uint8_t>(result) = false;
            }
            return;
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Esync0x2Ddata(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.sync-data(%u)\n", (unsigned int)fd);
    flush_impl(wasi, fd, result);
}

static uint32_t flush_impl1(const struct wasi_instance *wasi, uint32_t fd) noexcept {
    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_EINVAL;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
            return WASIP1_ESUCCESS;

        case wasi_fd_type::FSFILE:
            if (!wasi->fdtable[fd].file_handle()->file.is_write_open()) {
                return WASIP1_ESUCCESS;
            } else if (PHYSFS_flush(wasi->fdtable[fd].file_handle()->file.get_write()) == 0) {
                return WASIP1_EIO;
            } else {
                return WASIP1_ESUCCESS;
            }
    }

    return WASIP1_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_datasync(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::fd_datasync(%u)\n", (unsigned int)fd);
    return flush_impl1(wasi, fd);
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Eget0x2Dflags(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.get-flags(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 2);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_INVALID;
            return;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            wasi->ref<uint8_t>(result) = false;
            wasi->ref<uint8_t>(result + 1) = wasi->fdtable[fd].dir_handle()->writable ? WASI_FILESYSTEM_DESCRIPTOR_READ | WASI_FILESYSTEM_DESCRIPTOR_WRITE | WASI_FILESYSTEM_DESCRIPTOR_MUTATE_DIRECTORY : WASI_FILESYSTEM_DESCRIPTOR_READ;
            return;

        case wasi_fd_type::FSFILE:
            wasi->ref<uint8_t>(result) = false;
            wasi->ref<uint8_t>(result + 1) = (wasi->fdtable[fd].file_handle()->file.is_read_open() ? WASI_FILESYSTEM_DESCRIPTOR_READ : 0) | (wasi->fdtable[fd].file_handle()->file.is_write_open() ? WASI_FILESYSTEM_DESCRIPTOR_WRITE : 0);
            return;
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_fdstat_get(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::fd_fdstat_get(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 24);

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_EINVAL;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSFILE:
            wasi->ref<uint8_t>(result) = WASIP1_IFCHR; // fs_filetype
            wasi->ref<uint16_t>(result + 2) = 0; // fs_flags
            wasi->ref<uint64_t>(result + 8) = WASIP1_FD_READ | WASIP1_FD_WRITE | WASIP1_FD_FILESTAT_GET; // fs_rights_base
            wasi->ref<uint64_t>(result + 16) = 0; // fs_rights_inheriting
            return WASIP1_ESUCCESS;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            wasi->ref<uint8_t>(result) = WASIP1_IFDIR; // fs_filetype
            wasi->ref<uint16_t>(result + 2) = 0; // fs_flags
            wasi->ref<uint64_t>(result + 8) = WASIP1_PATH_OPEN | WASIP1_FD_READDIR | WASIP1_PATH_FILESTAT_GET | WASIP1_FD_FILESTAT_GET; // fs_rights_base
            wasi->ref<uint64_t>(result + 16) = 0; // fs_rights_inheriting
            return WASIP1_ESUCCESS;
    }

    return WASIP1_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_fdstat_set_flags(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, uint32_t flags) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::fd_fdstat_set_flags(%u, %u)\n", (unsigned int)fd, (unsigned int)flags);

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_ESUCCESS;
    }

    return WASIP1_EBADF;
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Eset0x2Dsize(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint64_t size, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.set-size(%u, %llu)\n", (unsigned int)fd, (unsigned long long)size);

    wasi->check_bounds(result, 2);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_INVALID;
            return;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_UNSUPPORTED;
            return;
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_filestat_set_size(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, uint64_t size) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::fd_filestat_set_size(%u, %llu)\n", (unsigned int)fd, (unsigned long long)size);

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
            return WASIP1_EINVAL;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_ENOTSUP;
    }

    return WASIP1_EBADF;
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Eread(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint64_t length, uint64_t offset, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.read(%u, %llu, %llu)\n", (unsigned int)fd, (unsigned long long)length, (unsigned long long)offset);

    wasi->check_bounds(result, 4 * sizeof(wasm_ptr_t));

    MKXPZ_FORCED_ASSERT(length <= (wasm_size_t)-1);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_FILESYSTEM_ERROR_INVALID;
            return;

        case wasi_fd_type::FSFILE:
            {
                if (!wasi->fdtable[fd].file_handle()->file.is_read_open()) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_FILESYSTEM_ERROR_ACCESS;
                    return;
                }
                if (!PHYSFS_seek(wasi->fdtable[fd].file_handle()->file.get_read(), offset)) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_FILESYSTEM_ERROR_IO;
                    return;
                }
                std::vector<uint8_t> buffer(length);
                uint64_t n = PHYSFS_readBytes(wasi->fdtable[fd].file_handle()->file.get_read(), buffer.data(), length);
                if (n == (uint64_t)-1) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_FILESYSTEM_ERROR_IO;
                    return;
                }
                wasm_ptr_t buf = wasi->cabi_alloc<uint8_t>(n);
                wasi->arycpy(buf, buffer.data(), n);
                wasi->ref<uint8_t>(result) = false;
                wasi->ref<wasm_ptr_t>(result + sizeof(wasm_ptr_t)) = buf;
                wasi->ref<wasm_size_t>(result + 2 * sizeof(wasm_ptr_t)) = n;
                wasi->ref<uint8_t>(result + 3 * sizeof(wasm_ptr_t)) = PHYSFS_eof(wasi->fdtable[fd].file_handle()->file.get_read()) != 0;
                return;
            }
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_pread(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, wasm_ptr_t iovs, uint32_t iovs_len, uint64_t offset, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::fd_pread(%u, 0x%08llx (%u), %llu)\n", (unsigned int)fd, (unsigned long long)iovs, (unsigned int)iovs_len, (unsigned long long)offset);

    MKXPZ_FORCED_ASSERT(8 * (wasm_size_t)iovs_len >= (wasm_size_t)iovs_len);
    wasi->check_bounds(iovs, 8 * (wasm_size_t)iovs_len);
    wasi->check_bounds(result, 4);

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
            wasi->ref<uint32_t>(result) = 0;
            return WASIP1_ESUCCESS;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_EINVAL;

        case wasi_fd_type::FSFILE:
            {
                if (!wasi->fdtable[fd].file_handle()->file.is_read_open()) {
                    return WASIP1_EACCES;
                }
                if (!PHYSFS_seek(wasi->fdtable[fd].file_handle()->file.get_read(), wasi->fdtable[fd].file_handle()->offset)) {
                    return WASIP1_EIO;
                }
                uint32_t size = 0;
                while (iovs_len > 0) {
                    uint32_t ptr = wasi->ref<uint32_t>(iovs);
                    uint32_t length = wasi->ref<uint32_t>(iovs + 4);
                    wasi->check_bounds(ptr, length);
#ifdef MKXPZ_BIG_ENDIAN
                    uint8_t *buffer = &wasi->ref<uint8_t>(ptr, std::max(length, (uint32_t)1) - 1);
#else
                    uint8_t *buffer = &wasi->ref<uint8_t>(ptr);
#endif // MKXPZ_BIG_ENDIAN
                    uint64_t n = PHYSFS_readBytes(wasi->fdtable[fd].file_handle()->file.get_read(), buffer, length);
                    if (n == (uint64_t)-1) {
                        return WASIP1_EIO;
                    }
#ifdef MKXPZ_BIG_ENDIAN
                    std::reverse(buffer, buffer + length);
#endif // MKXPZ_BIG_ENDIAN
                    size += n;
                    iovs += 8;
                    --iovs_len;
                }
                wasi->ref<uint32_t>(result) = size;
                return WASIP1_ESUCCESS;
            }
    }

    return WASIP1_EBADF;
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Ewrite(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t buffer, wasm_size_t buffer_len, uint64_t offset, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.write(%u, 0x%08llx (%llu), %llu)\n", (unsigned int)fd, (unsigned long long)buffer, (unsigned long long)buffer_len, (unsigned long long)offset);

    wasi->check_bounds(buffer, buffer_len);
    wasi->check_bounds(result, 16);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_INVALID;
            return;

        case wasi_fd_type::FSFILE:
            {
                if (!wasi->fdtable[fd].file_handle()->file.is_write_open()) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_ACCESS;
                    return;
                }
                if (!PHYSFS_seek(wasi->fdtable[fd].file_handle()->file.get_write(), offset)) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_IO;
                    return;
                }
#ifdef MKXPZ_BIG_ENDIAN
                uint8_t *buf = &wasi->ref<uint8_t>(buffer, std::max(buffer_len, (wasm_size_t)1) - 1);
                std::reverse(buf, buf + buffer_len);
#else
                uint8_t *buf = &wasi->ref<uint8_t>(buffer);
#endif // MKXPZ_BIG_ENDIAN
                uint64_t n = PHYSFS_writeBytes(wasi->fdtable[fd].file_handle()->file.get_write(), buf, buffer_len);
#ifdef MKXPZ_BIG_ENDIAN
                std::reverse(buf, buf + buffer_len);
#endif // MKXPZ_BIG_ENDIAN
                if (n == (uint64_t)-1) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_IO;
                } else {
                    wasi->ref<uint8_t>(result) = false;
                    wasi->ref<uint64_t>(result + 8) = n;
                }
                return;
            }
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_pwrite(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, wasm_ptr_t iovs, uint32_t iovs_len, uint64_t offset, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::fd_pwrite(%u, 0x%08llx (%u), %llu)\n", (unsigned int)fd, (unsigned long long)iovs, (unsigned int)iovs_len, (unsigned long long)offset);

    MKXPZ_FORCED_ASSERT(8 * (wasm_size_t)iovs_len >= (wasm_size_t)iovs_len);
    wasi->check_bounds(iovs, 8 * (wasm_size_t)iovs_len);
    wasi->check_bounds(result, 4);

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::STDIN:
            wasi->ref<uint32_t>(result) = 0;
            return WASIP1_ESUCCESS;

        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
            {
                uint32_t size = 0;
                std::string buf;
                while (iovs_len > 0) {
                    wasm_ptr_t str = wasi->ref<uint32_t>(iovs);
                    uint32_t n = wasi->ref<uint32_t>(iovs + 4);
                    wasi->check_bounds(str, n);
#ifdef MKXPZ_BIG_ENDIAN
                    if (n > 0) {
                        std::reverse(&wasi->ref<char>(str) - (n - 1), &wasi->ref<char>(str) - 1);
                        buf.append(&wasi->ref<char>(str) - (n - 1), n);
                        std::reverse(&wasi->ref<char>(str) - (n - 1), &wasi->ref<char>(str) - 1);
                    }
#else
                    buf.append(&wasi->ref<char>(str), n);
#endif // MKXPZ_BIG_ENDIAN
                    size += n;
                    iovs += 8;
                    --iovs_len;
                }
                std::string &line_buffer = wasi->stdio_line_buffers[wasi->fdtable[fd].type == wasi_fd_type::STDOUT ? 0 : 1];
                size_t line_start_index = 0, i = 0;
                for (char c : buf) {
                    if (c == '\n') {
                        mkxp_retro_log_printf(
                            wasi->fdtable[fd].type == wasi_fd_type::STDOUT ? RETRO_LOG_INFO : RETRO_LOG_WARN,
                            wasi->fdtable[fd].type == wasi_fd_type::STDOUT ? "[mkxp-z stdout] %.*s\n" : "[mkxp-z stderr] %.*s\n",
                            std::min(line_buffer.length() + (i - line_start_index), (size_t)INT_MAX),
                            line_buffer.empty() ? buf.data() + line_start_index : line_buffer.append(buf.data() + line_start_index, i - line_start_index).c_str()
                        );
                        line_buffer.clear();
                        line_start_index = i + 1;
                    }
                    ++i;
                }
                if ((wasm_ptr_t)line_buffer.size() + (wasm_ptr_t)(buf.size() - line_start_index) >= (wasm_ptr_t)line_buffer.size()) {
                    line_buffer.append(buf.begin() + line_start_index, buf.end());
                }
                wasi->ref<uint32_t>(result) = size;
                return WASIP1_ESUCCESS;
            }

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_EINVAL;

        case wasi_fd_type::FSFILE:
            {
                if (!wasi->fdtable[fd].file_handle()->file.is_write_open()) {
                    return WASIP1_EACCES;
                }
                if (!PHYSFS_seek(wasi->fdtable[fd].file_handle()->file.get_write(), wasi->fdtable[fd].file_handle()->offset)) {
                    return WASIP1_EIO;
                }
                uint32_t size = 0;
                while (iovs_len > 0) {
                    uint32_t ptr = wasi->ref<uint32_t>(iovs);
                    uint32_t length = wasi->ref<uint32_t>(iovs + 4);
                    wasi->check_bounds(ptr, length);
#ifdef MKXPZ_BIG_ENDIAN
                    uint8_t *buffer = &wasi->ref<uint8_t>(ptr, std::max(length, (uint32_t)1) - 1);
                    std::reverse(buffer, buffer + length);
#else
                    uint8_t *buffer = &wasi->ref<uint8_t>(ptr);
#endif // MKXPZ_BIG_ENDIAN
                    uint64_t n = PHYSFS_writeBytes(wasi->fdtable[fd].file_handle()->file.get_write(), buffer, length);
#ifdef MKXPZ_BIG_ENDIAN
                    std::reverse(buffer, buffer + length);
#endif // MKXPZ_BIG_ENDIAN
                    if (n == (uint64_t)-1) {
                        return WASIP1_EIO;
                    }
                    iovs += 8;
                    --iovs_len;
                }
                wasi->ref<uint32_t>(result) = size;
                return WASIP1_ESUCCESS;
            }
    }

    return WASIP1_EBADF;
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Eread0x2Ddirectory(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.read-directory(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 8);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_INVALID;
            return;

        case wasi_fd_type::FSDIR:
            {
                std::deque<std::pair<std::string, enum PHYSFS_FileType>> deque;
                bool success = PHYSFS_enumerate(
                    wasi->fdtable[fd].dir_handle()->path.c_str(),
                    [](void *data, const char *path, const char *filename) {
                        if (std::strlen(filename) > (wasm_size_t)-1) {
                            return PHYSFS_ENUM_OK;
                        }
                        PHYSFS_Stat stat;
                        if (!PHYSFS_stat((std::string(path) + "/" + filename).c_str(), &stat)) {
                            return PHYSFS_ENUM_OK;
                        }
                        if (stat.filetype != PHYSFS_FILETYPE_DIRECTORY && stat.filetype != PHYSFS_FILETYPE_REGULAR) {
                            return PHYSFS_ENUM_OK;
                        }
                        std::deque<std::pair<std::string, enum PHYSFS_FileType>> *deque = (std::deque<std::pair<std::string, enum PHYSFS_FileType>> *)data;
                        deque->emplace_back(filename, stat.filetype);
                        return PHYSFS_ENUM_OK;
                    },
                    (void *)&deque
                );
                if (success) {
                    wasm_resource_t dirstream = wasi->allocate_file_descriptor(wasi_fd_type::FSDIRSTREAM, new fs_dir_stream {std::move(deque)});
                    LOG_PRINTF(RETRO_LOG_DEBUG, "WASI resource created: directory-entry-stream(%u) -> directory descriptor %u \"%s\"\n", (unsigned int)dirstream, (unsigned int)fd, wasi->fdtable[fd].dir_handle()->path.c_str());
                    wasi->ref<uint8_t>(result) = false;
                    wasi->ref<wasm_resource_t>(result + 4) = dirstream;
                } else {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_NO_ENTRY;
                }
                return;
            }
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

struct fs_enumerate_data_wasip1 {
    struct wasi_instance *wasi;
    wasm_ptr_t original_buf;
    wasm_ptr_t buf;
    uint32_t buf_len;
    uint64_t initial_cookie;
    uint64_t cookie;
    wasm_ptr_t result;
};

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_readdir(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, wasm_ptr_t buf, uint32_t buf_len, uint64_t cookie, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::fd_readdir(%u, 0x%08llx (%u), %llu)\n", (unsigned int)fd, (unsigned long long)buf, (unsigned int)buf_len, (unsigned long long)cookie);

    wasi->check_bounds(buf, buf_len);
    wasi->check_bounds(result, 4);

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_EINVAL;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                struct fs_enumerate_data_wasip1 edata = {
                    wasi,
                    buf,
                    buf,
                    buf_len,
                    cookie,
                    0,
                    result,
                };
                bool success = PHYSFS_enumerate(
                    wasi->fdtable[fd].dir_handle()->path.c_str(),
                    [](void *data, const char *path, const char *filename) {
                        struct fs_enumerate_data_wasip1 *edata = (struct fs_enumerate_data_wasip1 *)data;
                        struct wasi_instance *wasi = edata->wasi;

                        PHYSFS_Stat stat;
                        if (!PHYSFS_stat((std::string(path) + "/" + filename).c_str(), &stat)) {
                            return PHYSFS_ENUM_OK;
                        }
                        if (stat.filetype != PHYSFS_FILETYPE_DIRECTORY && stat.filetype != PHYSFS_FILETYPE_REGULAR) {
                            return PHYSFS_ENUM_OK;
                        }

                        if (edata->cookie++ < edata->initial_cookie) {
                            return PHYSFS_ENUM_OK;
                        }

                        if (edata->buf - edata->original_buf + 8 > edata->buf_len) {
                            return PHYSFS_ENUM_STOP;
                        }
                        std::memcpy(wasi->ptr_unaligned<uint64_t>(edata->buf), &edata->cookie, 8);
                        edata->buf += 8;

                        if (edata->buf - edata->original_buf + 8 > edata->buf_len) {
                            return PHYSFS_ENUM_STOP;
                        }
                        std::memset(wasi->ptr_unaligned<uint64_t>(edata->buf), 0, 8);
                        edata->buf += 8;

                        if (edata->buf - edata->original_buf + 4 > edata->buf_len) {
                            return PHYSFS_ENUM_STOP;
                        }
                        {
                            const uint32_t value = std::strlen(filename);
                            std::memcpy(wasi->ptr_unaligned<uint32_t>(edata->buf), &value, 4);
                        }
                        edata->buf += 4;

                        if (edata->buf - edata->original_buf + 4 > edata->buf_len) {
                            return PHYSFS_ENUM_STOP;
                        }
                        wasi->ref<uint8_t>(edata->buf) = stat.filetype;
                        edata->buf += 4;

                        uint32_t len = std::min(std::strlen(filename), (size_t)(edata->original_buf + edata->buf_len - edata->buf));
                        wasi->arycpy(edata->buf, filename, std::strlen(filename));
                        edata->buf += len;
                        return PHYSFS_ENUM_OK;
                    },
                    (void *)&edata
                );
                if (success) {
                    wasi->ref<uint32_t>(result) = edata.buf - edata.original_buf;
                    return WASIP1_ESUCCESS;
                } else {
                    return WASIP1_ENOENT;
                }
            }
    }

    return WASIP1_EBADF;
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Esync(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.sync(%u)\n", (unsigned int)fd);
    flush_impl(wasi, fd, result);
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_sync(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::fd_sync(%u)\n", (unsigned int)fd);
    return flush_impl1(wasi, fd);
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Ecreate0x2Ddirectory0x2Dat(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t path, wasm_size_t path_len, wasm_ptr_t result) {
    wasi->check_bounds(path, path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.create-directory-at(%u, \"%.*s\")\n", (unsigned int)fd, (int)std::min(path_len, (wasm_size_t)INT_MAX), (const char *)wasi->str(path, path_len));

    wasi->check_bounds(result, 2);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_NOT_DIRECTORY;
            return;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                if (!wasi->fdtable[fd].dir_handle()->writable) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_READ_ONLY;
                    return;
                }

                std::string joined_path = dir_path_join(wasi, fd, path, path_len);
                if (joined_path.empty()) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_NOT_PERMITTED;
                    return;
                }

                if (!PHYSFS_mkdir(joined_path.c_str() + (wasi->fdtable[fd].dir_handle()->path.length() + 1))) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_IO;
                    return;
                }
            }
            wasi->ref<uint8_t>(result) = false;
            return;
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_path_create_directory(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, wasm_ptr_t path, uint32_t path_len) {
    wasi->check_bounds(path, path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::path_create_directory(%u, \"%.*s\")\n", (unsigned int)fd, (int)std::min(path_len, (uint32_t)INT_MAX), (const char *)wasi->str(path, path_len));

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_ENOTDIR;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                if (!wasi->fdtable[fd].dir_handle()->writable) {
                    return WASIP1_EROFS;
                }

                std::string joined_path = dir_path_join(wasi, fd, path, path_len);
                if (joined_path.empty()) {
                    return WASIP1_EPERM;
                }

                if (!PHYSFS_mkdir(joined_path.c_str() + (wasi->fdtable[fd].dir_handle()->path.length() + 1))) {
                    return WASIP1_EIO;
                }
            }
            return WASIP1_ESUCCESS;
    }

    return WASIP1_EBADF;
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Estat(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.stat(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 104);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_INVALID;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
            wasi->ref<uint8_t>(result) = false;
            wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_DESCRIPTOR_TYPE_CHARACTER_DEVICE; // type
            wasi->ref<uint64_t>(result + 16) = 1; // link-count
            wasi->ref<uint64_t>(result + 24) = 0; // size
            wasi->ref<uint8_t>(result + 32) = false; // data-access-timestamp discriminant (false = none, true = some)
            //wasi->ref<uint64_t>(result + 40) = 0; // data-access-timestamp seconds
            //wasi->ref<uint32_t>(result + 48) = 0; // data-access-timestamp nanoseconds
            wasi->ref<uint8_t>(result + 56) = false; // data-modification-timestamp discriminant (false = none, true = some)
            //wasi->ref<uint64_t>(result + 64) = 0; // data-modification-timestamp seconds
            //wasi->ref<uint32_t>(result + 72) = 0; // data-modification-timestamp nanoseconds
            wasi->ref<uint8_t>(result + 80) = false; // status-change-timestamp discriminant (false = none, true = some)
            //wasi->ref<uint64_t>(result + 88) = 0; // status-change-timestamp seconds
            //wasi->ref<uint32_t>(result + 96) = 0; // status-change-timestamp nanoseconds
            return;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                PHYSFS_Stat stat;
                if (!PHYSFS_stat(wasi->fdtable[fd].dir_handle()->path.c_str(), &stat)) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_NO_ENTRY;
                    return;
                }
                if (stat.filetype != PHYSFS_FILETYPE_DIRECTORY) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_IO;
                    return;
                }
                wasi->ref<uint8_t>(result) = false;
                wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_DESCRIPTOR_TYPE_DIRECTORY; // type
                wasi->ref<uint64_t>(result + 16) = 1; // link-count
                wasi->ref<uint64_t>(result + 24) = 0; // size
                wasi->ref<uint8_t>(result + 32) = true; // data-access-timestamp discriminant (false = none, true = some)
                wasi->ref<uint64_t>(result + 40) = stat.accesstime; // data-access-timestamp seconds
                wasi->ref<uint32_t>(result + 48) = 0; // data-access-timestamp nanoseconds
                wasi->ref<uint8_t>(result + 56) = true; // data-modification-timestamp discriminant (false = none, true = some)
                wasi->ref<uint64_t>(result + 64) = stat.modtime; // data-modification-timestamp seconds
                wasi->ref<uint32_t>(result + 72) = 0; // data-modification-timestamp nanoseconds
                wasi->ref<uint8_t>(result + 80) = true; // status-change-timestamp discriminant (false = none, true = some)
                wasi->ref<uint64_t>(result + 88) = stat.createtime; // status-change-timestamp seconds
                wasi->ref<uint32_t>(result + 96) = 0; // status-change-timestamp nanoseconds
                return;
            }

        case wasi_fd_type::FSFILE:
            {
                PHYSFS_Stat stat;
                if (!PHYSFS_stat(wasi->fdtable[fd].file_handle()->file.path(), &stat)) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_NO_ENTRY;
                    return;
                }
                if (stat.filetype != PHYSFS_FILETYPE_REGULAR) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_IO;
                    return;
                }
                wasi->ref<uint8_t>(result) = false;
                wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_DESCRIPTOR_TYPE_REGULAR_FILE; // type
                wasi->ref<uint64_t>(result + 16) = 1; // link-count
                wasi->ref<uint64_t>(result + 24) = stat.filesize == -1 ? 0 : stat.filesize; // size
                wasi->ref<uint8_t>(result + 32) = true; // data-access-timestamp discriminant (false = none, true = some)
                wasi->ref<uint64_t>(result + 40) = stat.accesstime; // data-access-timestamp seconds
                wasi->ref<uint32_t>(result + 48) = 0; // data-access-timestamp nanoseconds
                wasi->ref<uint8_t>(result + 56) = true; // data-modification-timestamp discriminant (false = none, true = some)
                wasi->ref<uint64_t>(result + 64) = stat.modtime; // data-modification-timestamp seconds
                wasi->ref<uint32_t>(result + 72) = 0; // data-modification-timestamp nanoseconds
                wasi->ref<uint8_t>(result + 80) = true; // status-change-timestamp discriminant (false = none, true = some)
                wasi->ref<uint64_t>(result + 88) = stat.createtime; // status-change-timestamp seconds
                wasi->ref<uint32_t>(result + 96) = 0; // status-change-timestamp nanoseconds
                return;
            }
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_filestat_get(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::fd_filestat_get(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 64);

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_EINVAL;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
            wasi->ref<uint64_t>(result) = fd; // dev
            wasi->ref<uint64_t>(result + 8) = 0; // ino
            wasi->ref<uint8_t>(result + 16) = WASIP1_IFCHR; // filetype
            wasi->ref<uint32_t>(result + 24) = 1; // nlink
            wasi->ref<uint64_t>(result + 32) = 0; // size
            wasi->ref<uint64_t>(result + 40) = 0; // atim
            wasi->ref<uint64_t>(result + 48) = 0; // mtim
            wasi->ref<uint64_t>(result + 56) = 0; // ctim
            return WASIP1_ESUCCESS;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                PHYSFS_Stat stat;
                if (!PHYSFS_stat(wasi->fdtable[fd].dir_handle()->path.c_str(), &stat)) {
                    return WASIP1_ENOENT;
                }
                if (stat.filetype != PHYSFS_FILETYPE_DIRECTORY) {
                    return WASIP1_EIO;
                }
                wasi->ref<uint64_t>(result) = fd; // dev
                wasi->ref<uint64_t>(result + 8) = 0; // ino
                wasi->ref<uint8_t>(result + 16) = WASIP1_IFDIR; // filetype
                wasi->ref<uint32_t>(result + 24) = 1; // nlink
                wasi->ref<uint64_t>(result + 32) = 0; // size
                wasi->ref<uint64_t>(result + 40) = (uint64_t)stat.accesstime * (uint64_t)1000000000U; // atim
                wasi->ref<uint64_t>(result + 48) = (uint64_t)stat.modtime * (uint64_t)1000000000U; // mtim
                wasi->ref<uint64_t>(result + 56) = (uint64_t)stat.createtime * (uint64_t)1000000000U; // ctim
                return WASIP1_ESUCCESS;
            }

        case wasi_fd_type::FSFILE:
            {
                PHYSFS_Stat stat;
                if (!PHYSFS_stat(wasi->fdtable[fd].file_handle()->file.path(), &stat)) {
                    return WASIP1_ENOENT;
                }
                if (stat.filetype != PHYSFS_FILETYPE_REGULAR) {
                    return WASIP1_EIO;
                }
                wasi->ref<uint64_t>(result) = fd; // dev
                wasi->ref<uint64_t>(result + 8) = 0; // ino
                wasi->ref<uint8_t>(result + 16) = WASIP1_IFREG; // filetype
                wasi->ref<uint32_t>(result + 24) = 1; // nlink
                wasi->ref<uint64_t>(result + 32) = stat.filesize == -1 ? 0 : stat.filesize; // size
                wasi->ref<uint64_t>(result + 40) = (uint64_t)stat.accesstime * (uint64_t)1000000000U; // atim
                wasi->ref<uint64_t>(result + 48) = (uint64_t)stat.modtime * (uint64_t)1000000000U; // mtim
                wasi->ref<uint64_t>(result + 56) = (uint64_t)stat.createtime * (uint64_t)1000000000U; // ctim
                return WASIP1_ESUCCESS;
            }
    }

    return WASIP1_EBADF;
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Estat0x2Dat(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint32_t path_flags, wasm_ptr_t path, wasm_size_t path_len, wasm_ptr_t result) {
    wasi->check_bounds(path, path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.stat-at(%u, %u, \"%.*s\")\n", (unsigned int)fd, (unsigned int)path_flags, (int)std::min(path_len, (wasm_size_t)INT_MAX), (const char *)wasi->str(path, path_len));

    wasi->check_bounds(result, 104);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_NOT_DIRECTORY;
            return;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                std::string joined_path = dir_path_join(wasi, fd, path, path_len);
                if (joined_path.empty()) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_NOT_PERMITTED;
                    return;
                }
                PHYSFS_Stat stat;
                if (!PHYSFS_stat(joined_path.c_str(), &stat)) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_NO_ENTRY;
                    return;
                }
                if (stat.filetype != PHYSFS_FILETYPE_DIRECTORY && stat.filetype != PHYSFS_FILETYPE_REGULAR) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_IO;
                    return;
                }
                wasi->ref<uint8_t>(result) = false;
                wasi->ref<uint8_t>(result + 8) = stat.filetype == PHYSFS_FILETYPE_DIRECTORY ? WASI_FILESYSTEM_DESCRIPTOR_TYPE_DIRECTORY : WASI_FILESYSTEM_DESCRIPTOR_TYPE_REGULAR_FILE; // type
                wasi->ref<uint64_t>(result + 16) = 1; // link-count
                wasi->ref<uint64_t>(result + 24) = stat.filetype == PHYSFS_FILETYPE_DIRECTORY ? 0 : stat.filesize == -1 ? 0 : stat.filesize; // size
                wasi->ref<uint8_t>(result + 32) = true; // data-access-timestamp discriminant (false = none, true = some)
                wasi->ref<uint64_t>(result + 40) = stat.accesstime; // data-access-timestamp seconds
                wasi->ref<uint32_t>(result + 48) = 0; // data-access-timestamp nanoseconds
                wasi->ref<uint8_t>(result + 56) = true; // data-modification-timestamp discriminant (false = none, true = some)
                wasi->ref<uint64_t>(result + 64) = stat.modtime; // data-modification-timestamp seconds
                wasi->ref<uint32_t>(result + 72) = 0; // data-modification-timestamp nanoseconds
                wasi->ref<uint8_t>(result + 80) = true; // status-change-timestamp discriminant (false = none, true = some)
                wasi->ref<uint64_t>(result + 88) = stat.createtime; // status-change-timestamp seconds
                wasi->ref<uint32_t>(result + 96) = 0; // status-change-timestamp nanoseconds
                return;
            }
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_path_filestat_get(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, uint32_t flags, wasm_ptr_t path, uint32_t path_len, wasm_ptr_t result) {
    wasi->check_bounds(path, path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::path_filestat_get(%u, %u, \"%.*s\")\n", (unsigned int)fd, (unsigned int)flags, (int)std::min(path_len, (uint32_t)INT_MAX), (const char *)wasi->str(path, path_len));

    wasi->check_bounds(result, 64);

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_ENOTDIR;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                std::string joined_path = dir_path_join(wasi, fd, path, path_len);
                if (joined_path.empty()) {
                    return WASIP1_EPERM;
                }

                PHYSFS_Stat stat;
                if (!PHYSFS_stat(joined_path.c_str(), &stat)) {
                    return WASIP1_ENOENT;
                }
                if (stat.filetype != PHYSFS_FILETYPE_DIRECTORY && stat.filetype != PHYSFS_FILETYPE_REGULAR) {
                    return WASIP1_EIO;
                }

                wasi->ref<uint64_t>(result) = fd; // dev
                wasi->ref<uint64_t>(result + 8) = 0; // ino
                wasi->ref<uint8_t>(result + 16) = stat.filetype == PHYSFS_FILETYPE_DIRECTORY ? WASIP1_IFDIR : WASIP1_IFREG; // filetype
                wasi->ref<uint32_t>(result + 24) = 1; // nlink
                wasi->ref<uint64_t>(result + 32) = stat.filetype == PHYSFS_FILETYPE_DIRECTORY ? 0 : stat.filesize == -1 ? 0 : stat.filetype; // size
                wasi->ref<uint64_t>(result + 40) = (uint64_t)stat.accesstime * (uint64_t)1000000000U; // atim
                wasi->ref<uint64_t>(result + 48) = (uint64_t)stat.modtime * (uint64_t)1000000000U; // mtim
                wasi->ref<uint64_t>(result + 56) = (uint64_t)stat.createtime * (uint64_t)1000000000U; // ctim
                return WASIP1_ESUCCESS;
            }
    }

    return WASIP1_EBADF;
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Eset0x2Dtimes0x2Dat(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint32_t path_flags, wasm_ptr_t path, wasm_size_t path_len, uint32_t data_access_timestamp, uint64_t data_access_timestamp_seconds, uint32_t data_access_timestamp_nanoseconds, uint32_t data_modification_timestamp, uint64_t data_modification_timestamp_seconds, uint32_t data_modification_timestamp_nanoseconds, uint32_t result) {
    wasi->check_bounds(path, path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.set-times-at(%u, %u, \"%.*s\", (%u, %llu, %u), (%u, %llu, %u))\n", (unsigned int)fd, (unsigned int)path_flags, (int)std::min(path_len, (wasm_size_t)INT_MAX), (const char *)wasi->str(path, path_len), (unsigned int)data_access_timestamp, (unsigned long long)data_access_timestamp_seconds, (unsigned int)data_access_timestamp_nanoseconds, (unsigned int)data_modification_timestamp, (unsigned long long)data_modification_timestamp_seconds, (unsigned int)data_modification_timestamp_nanoseconds);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_NOT_DIRECTORY;
            return;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_UNSUPPORTED;
            return;
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_path_filestat_set_times(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, uint32_t flags, wasm_ptr_t path, uint32_t path_len, uint64_t atim, uint64_t ntim, uint32_t fst_flags) {
    wasi->check_bounds(path, path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::path_filestat_set_times(%u, %u, \"%.*s\", %llu, %llu, %u)\n", (unsigned int)fd, (unsigned int)flags, (int)std::min(path_len, (uint32_t)INT_MAX), (const char *)wasi->str(path, path_len), (unsigned long long)atim, (unsigned long long)ntim, (unsigned int)fst_flags);

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_ENOTDIR;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                std::string joined_path = dir_path_join(wasi, fd, path, path_len);
                if (joined_path.empty()) {
                    return WASIP1_ENOTSUP;
                }
                return WASIP1_EINVAL;
            }
    }

    return WASIP1_EBADF;
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Elink0x2Dat(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint32_t old_path_flags, wasm_ptr_t old_path, wasm_size_t old_path_len, wasm_resource_t new_descriptor, wasm_ptr_t new_path, wasm_size_t new_path_len, wasm_ptr_t result) {
    wasi->check_bounds(old_path, old_path_len);
    wasi->check_bounds(new_path, new_path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.link-at(%u, %u, \"%.*s\", %u, \"%.*s\")\n", (unsigned int)fd, (unsigned int)old_path_flags, (int)std::min(old_path_len, (wasm_size_t)INT_MAX), (const char *)wasi->str(old_path, old_path_len), (unsigned int)new_descriptor, (int)std::min(new_path_len, (wasm_size_t)INT_MAX), (const char *)wasi->str(new_path, new_path_len));

    wasi->check_bounds(result, 2);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_NOT_DIRECTORY;
            return;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_UNSUPPORTED;
            return;
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_path_link(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t old_fd, uint32_t old_flags, wasm_ptr_t old_path, uint32_t old_path_len, uint32_t new_fd, wasm_ptr_t new_path, uint32_t new_path_len) {
    wasi->check_bounds(old_path, old_path_len);
    wasi->check_bounds(new_path, new_path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::path_link(%u, %u, \"%.*s\", %u, \"%.*s\")\n", (unsigned int)old_fd, (unsigned int)old_flags, (int)std::min(old_path_len, (uint32_t)INT_MAX), (const char *)wasi->str(old_path, old_path_len), (unsigned int)new_fd, (int)std::min(new_path_len, (uint32_t)INT_MAX), (const char *)wasi->str(new_path, new_path_len));
    return WASIP1_ENOTSUP;
}

static std::pair<uint32_t, enum PHYSFS_ErrorCode> open_impl(struct wasi_instance *wasi, uint32_t fd, std::string path, bool exists, bool is_directory, bool truncate, bool needs_read, bool needs_write, bool ignore_open_write_errors) {
    bool writable = wasi->fdtable[fd].dir_handle()->writable;

    uint32_t root = wasi->fdtable[fd].type == wasi_fd_type::FS ? fd : wasi->fdtable[fd].dir_handle()->root;

    if (exists && is_directory) {
        if (needs_write && !writable) {
            return {0, PHYSFS_ERR_READ_ONLY};
        }
        struct fs_dir *handle = new fs_dir {path, root, needs_write};
        return {wasi->allocate_file_descriptor(wasi_fd_type::FSDIR, handle), PHYSFS_ERR_OK};
    }

    bool close_write_after_open = false;
    if (!needs_write && truncate) {
        needs_write = true;
        close_write_after_open = true;
    }

    bool close_read_after_open = false;
    if (!needs_read && !needs_write) {
        needs_read = true;
        close_read_after_open = true;
    }

    if (needs_write && !writable) {
        return {0, PHYSFS_ERR_READ_ONLY};
    }

    const char *write_path_prefix;
    if (needs_write) {
        uint32_t root = wasi->fdtable[fd].type == wasi_fd_type::FS ? fd : wasi->fdtable[fd].dir_handle()->root;
        write_path_prefix = wasi->fdtable[root].dir_handle()->path.c_str();
    } else {
        write_path_prefix = nullptr;
    }

    struct fs_file *handle = new fs_file {{*mkxp_retro::fs, path.c_str(), write_path_prefix, truncate, needs_read, exists}, {}, 0, root};

    // Check for errors opening the read handle and/or write handle
    if (needs_read && !handle->file.is_read_open()) {
        PHYSFS_ErrorCode error = handle->file.get_read_error();
        delete handle;
        return {0, error};
    }
    if (needs_write && !ignore_open_write_errors && !handle->file.is_write_open()) {
        PHYSFS_ErrorCode error = handle->file.get_write_error();
        delete handle;
        return {0, error};
    }

    if (close_read_after_open) {
        handle->file.close_read();
    }
    if (close_write_after_open) {
        handle->file.close_write();
    }

    return {wasi->allocate_file_descriptor(wasi_fd_type::FSFILE, handle), PHYSFS_ERR_OK};
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Eopen0x2Dat(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint32_t path_flags, wasm_ptr_t path, wasm_size_t path_len, uint32_t open_flags, uint32_t flags, wasm_ptr_t result) {
    wasi->check_bounds(path, path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.open-at(%u, %u, \"%.*s\", %u, %u)\n", (unsigned int)fd, (unsigned int)path_flags, (int)std::min(path_len, (wasm_size_t)INT_MAX), (const char *)wasi->str(path, path_len), (unsigned int)open_flags, (unsigned int)flags);

    wasi->check_bounds(result, 8);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_NOT_DIRECTORY;
            return;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                std::string joined_path = dir_path_join(wasi, fd, path, path_len);
                if (joined_path.empty()) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_NOT_PERMITTED;
                    return;
                }

                PHYSFS_Stat stat;
                bool exists = PHYSFS_stat(joined_path.c_str(), &stat);

                // Fail if create flag of open_flags isn't set and the path doesn't exist
                if (!exists && !(open_flags & WASI_FILESYSTEM_OPEN_CREATE)) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_NO_ENTRY;
                    return;
                }

                // Fail if directory flag of open_flags is set and the path exists and isn't a directory
                if (exists && open_flags & WASI_FILESYSTEM_OPEN_DIRECTORY && stat.filetype != PHYSFS_FILETYPE_DIRECTORY) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_NOT_DIRECTORY;
                    return;
                }

                // Fail if exclusive flag of open_flags is set and the path exists
                if (exists && open_flags & WASI_FILESYSTEM_OPEN_EXCLUSIVE) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_EXIST;
                    return;
                }

                // Fail if the path exists and isn't a regular file or directory (e.g. a device file, named pipe, socket or symbolic link)
                if (exists && (stat.filetype != PHYSFS_FILETYPE_DIRECTORY && stat.filetype != PHYSFS_FILETYPE_REGULAR)) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_IO;
                    return;
                }

                bool truncate = open_flags & WASI_FILESYSTEM_OPEN_TRUNCATE;
                bool needs_read = flags & WASI_FILESYSTEM_DESCRIPTOR_READ;
                bool needs_write = flags & WASI_FILESYSTEM_DESCRIPTOR_WRITE || (stat.filetype == PHYSFS_FILETYPE_DIRECTORY && flags & WASI_FILESYSTEM_DESCRIPTOR_MUTATE_DIRECTORY);

                if (wasi->fdtable.size() >= UINT32_MAX && wasi->vacant_fds.empty()) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_IO;
                    return;
                }

                std::pair<uint32_t, enum PHYSFS_ErrorCode> open_result = open_impl(wasi, fd, joined_path, exists, stat.filetype == PHYSFS_FILETYPE_DIRECTORY, truncate, needs_read, needs_write, false);
                if (open_result.second == PHYSFS_ERR_OK) {
                    LOG_PRINTF(RETRO_LOG_DEBUG, "WASI resource created: descriptor(%u) -> %s \"%s\"\n", (unsigned int)open_result.first, stat.filetype == PHYSFS_FILETYPE_DIRECTORY ? "directory" : "file", joined_path.c_str());
                    wasi->ref<uint8_t>(result) = false;
                    wasi->ref<wasm_resource_t>(result + 4) = open_result.first;
                } else {
                    wasi->ref<uint8_t>(result) = true;
                    switch (open_result.second) {
                        case PHYSFS_ERR_READ_ONLY:
                        case PHYSFS_ERR_NO_WRITE_DIR:
                            wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_READ_ONLY;
                            break;
                        case PHYSFS_ERR_PERMISSION:
                        case PHYSFS_ERR_NOT_FOUND:
                            wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_ACCESS;
                            break;
                        default:
                            wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_IO;
                            break;
                    }
                }
                return;
            }
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 4) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_path_open(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, uint32_t dirflags, wasm_ptr_t path, uint32_t path_len, uint32_t oflags, uint64_t fs_base_rights, uint64_t fs_rights_inheriting, uint32_t fdflags, wasm_ptr_t result) {
    wasi->check_bounds(path, path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::path_open(%u, %u, \"%.*s\", %u, %llu, %llu, %u)\n", (unsigned int)fd, (unsigned int)dirflags, (int)std::min(path_len, (uint32_t)INT_MAX), (const char *)wasi->str(path, path_len), (unsigned int)oflags, (unsigned long long)fs_base_rights, (unsigned long long)fs_rights_inheriting, (unsigned int)fdflags);

    wasi->check_bounds(result, 4);

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_ENOTDIR;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                std::string joined_path = dir_path_join(wasi, fd, path, path_len);
                if (joined_path.empty()) {
                    return WASIP1_EPERM;
                }

                PHYSFS_Stat stat;
                bool exists = PHYSFS_stat(joined_path.c_str(), &stat);

                // Fail if bit 0 of oflags isn't set and the path doesn't exist
                if (!exists && !(oflags & (1 << 0))) {
                    return WASIP1_ENOENT;
                }

                // Fail if bit 1 of oflags is set and the path exists and isn't a directory
                if (exists && oflags & (1 << 1) && stat.filetype != PHYSFS_FILETYPE_DIRECTORY) {
                    return WASIP1_ENOTDIR;
                }

                // Fail if bit 2 of oflags is set and the path exists
                if (exists && oflags & (1 << 2)) {
                    return WASIP1_EEXIST;
                }

                // Fail if the path exists and isn't a regular file or directory (e.g. a device file, named pipe, socket or symbolic link)
                if (exists && (stat.filetype != PHYSFS_FILETYPE_DIRECTORY && stat.filetype != PHYSFS_FILETYPE_REGULAR)) {
                    return WASIP1_EIO;
                }

                bool truncate = oflags & (1 << 3);
                bool needs_read = true;
                bool needs_write = wasi->fdtable[fd].dir_handle()->writable;

                if (wasi->fdtable.size() >= UINT32_MAX && wasi->vacant_fds.empty()) {
                    return WASIP1_EMFILE;
                }

                std::pair<uint32_t, enum PHYSFS_ErrorCode> open_result = open_impl(wasi, fd, joined_path, exists, stat.filetype == PHYSFS_FILETYPE_DIRECTORY, truncate, needs_read, needs_write, true);
                if (open_result.second == PHYSFS_ERR_OK) {
                    LOG_PRINTF(RETRO_LOG_DEBUG, "WASI resource created: fd(%u) -> %s \"%s\"\n", (unsigned int)open_result.first, stat.filetype == PHYSFS_FILETYPE_DIRECTORY ? "directory" : "file", joined_path.c_str());
                    wasi->ref<uint32_t>(result) = open_result.first;
                    return WASIP1_ESUCCESS;
                } else {
                    switch (open_result.second) {
                        case PHYSFS_ERR_READ_ONLY:
                        case PHYSFS_ERR_NO_WRITE_DIR:
                            return WASIP1_EROFS;
                        case PHYSFS_ERR_PERMISSION:
                        case PHYSFS_ERR_NOT_FOUND:
                            return WASIP1_EACCES;
                        default:
                            return WASIP1_EIO;
                    }
                }
            }
    }

    return WASIP1_EBADF;
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Ereadlink0x2Dat(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t path, wasm_size_t path_len, wasm_ptr_t result) {
    wasi->check_bounds(path, path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.readlink-at(%u, \"%.*s\")\n", (unsigned int)fd, (int)std::min(path_len, (wasm_size_t)INT_MAX), (const char *)wasi->str(path, path_len));

    wasi->check_bounds(result, 3 * sizeof(wasm_ptr_t));

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_FILESYSTEM_ERROR_NOT_DIRECTORY;
            return;

        case wasi_fd_type::FSDIR:
            {
                std::string joined_path = dir_path_join(wasi, fd, path, path_len);
                if (joined_path.empty()) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_FILESYSTEM_ERROR_NOT_PERMITTED;
                    return;
                }
                wasi->ref<uint8_t>(result) = true;
                wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_FILESYSTEM_ERROR_INVALID;
                return;
            }
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_path_readlink(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, wasm_ptr_t path, uint32_t path_len, wasm_ptr_t buf, uint32_t buf_len, wasm_ptr_t result) {
    wasi->check_bounds(path, path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::path_readlink(%u, \"%.*s\", 0x%08llx (%u))\n", (unsigned int)fd, (int)std::min(path_len, (uint32_t)INT_MAX), (const char *)wasi->str(path, path_len), (unsigned long long)buf, (unsigned int)buf_len);

    wasi->check_bounds(buf, buf_len);
    wasi->check_bounds(result, 4);

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_ENOTDIR;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                std::string joined_path = dir_path_join(wasi, fd, path, path_len);
                if (joined_path.empty()) {
                    return WASIP1_ENOTSUP;
                }
                return WASIP1_EINVAL;
            }
    }

    return WASIP1_EBADF;
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Eremove0x2Ddirectory0x2Dat(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t path, wasm_size_t path_len, wasm_ptr_t result) {
    wasi->check_bounds(path, path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.remove-directory-at(%u, \"%.*s\")\n", (unsigned int)fd, (int)std::min(path_len, (wasm_size_t)INT_MAX), (const char *)wasi->str(path, path_len));

    wasi->check_bounds(result, 2);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_NOT_DIRECTORY;
            return;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                if (!wasi->fdtable[fd].dir_handle()->writable) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_READ_ONLY;
                    return;
                }

                std::string joined_path = dir_path_join(wasi, fd, path, path_len);
                if (joined_path.empty()) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_NOT_PERMITTED;
                    return;
                }

                uint32_t root = wasi->fdtable[fd].type == wasi_fd_type::FS ? fd : wasi->fdtable[fd].dir_handle()->root;
                if (joined_path.c_str() == wasi->fdtable[root].dir_handle()->path) { // Don't allow deleting preopened directories
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_NOT_PERMITTED;
                    return;
                }

                PHYSFS_Stat stat;
                if (!PHYSFS_stat(joined_path.c_str(), &stat)) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_NO_ENTRY;
                    return;
                }

                if (stat.filetype != PHYSFS_FILETYPE_DIRECTORY) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_NOT_DIRECTORY;
                    return;
                }

                if (!PHYSFS_delete(joined_path.c_str() + wasi->fdtable[root].dir_handle()->path.length())) {
                    switch (PHYSFS_getLastErrorCode()) {
                        case PHYSFS_ERR_DIR_NOT_EMPTY:
                            wasi->ref<uint8_t>(result) = true;
                            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_NOT_EMPTY;
                            return;
                        case PHYSFS_ERR_READ_ONLY:
                        case PHYSFS_ERR_NO_WRITE_DIR:
                            wasi->ref<uint8_t>(result) = true;
                            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_READ_ONLY;
                            return;
                        case PHYSFS_ERR_PERMISSION:
                            wasi->ref<uint8_t>(result) = true;
                            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_ACCESS;
                            return;
                        default:
                            wasi->ref<uint8_t>(result) = true;
                            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_IO;
                            return;
                    }
                }

                wasi->ref<uint8_t>(result) = false;
                return;
            }
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_path_remove_directory(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, wasm_ptr_t path, uint32_t path_len) {
    wasi->check_bounds(path, path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::path_remove_directory(%u, \"%.*s\")\n", (unsigned int)fd, (int)std::min(path_len, (uint32_t)INT_MAX), (const char *)wasi->str(path, path_len));

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_ENOTDIR;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                if (!wasi->fdtable[fd].dir_handle()->writable) {
                    return WASIP1_EROFS;
                }

                std::string joined_path = dir_path_join(wasi, fd, path, path_len);
                if (joined_path.empty()) {
                    return WASIP1_EPERM;
                }

                uint32_t root = wasi->fdtable[fd].type == wasi_fd_type::FS ? fd : wasi->fdtable[fd].dir_handle()->root;
                if (joined_path.c_str() == wasi->fdtable[root].dir_handle()->path) { // Don't allow deleting preopened directories
                    return WASIP1_EPERM;
                }

                PHYSFS_Stat stat;
                if (!PHYSFS_stat(joined_path.c_str(), &stat)) {
                    return WASIP1_ENOENT;
                }

                if (stat.filetype != PHYSFS_FILETYPE_DIRECTORY) {
                    return WASIP1_ENOTDIR;
                }

                if (!PHYSFS_delete(joined_path.c_str() + wasi->fdtable[root].dir_handle()->path.length())) {
                    switch (PHYSFS_getLastErrorCode()) {
                        case PHYSFS_ERR_DIR_NOT_EMPTY:
                            return WASIP1_ENOTEMPTY;
                        case PHYSFS_ERR_READ_ONLY:
                        case PHYSFS_ERR_NO_WRITE_DIR:
                            return WASIP1_EROFS;
                        case PHYSFS_ERR_PERMISSION:
                            return WASIP1_EACCES;
                        default:
                            return WASIP1_EIO;
                    }
                }

                return WASIP1_ESUCCESS;
            }
    }

    return WASIP1_EBADF;
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Erename0x2Dat(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t old_path, wasm_size_t old_path_len, wasm_resource_t new_descriptor, wasm_ptr_t new_path, wasm_size_t new_path_len, wasm_ptr_t result) {
    wasi->check_bounds(old_path, old_path_len);
    wasi->check_bounds(new_path, new_path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.rename-at(%u, \"%.*s\", %u, \"%.*s\")\n", (unsigned int)fd, (int)std::min(old_path_len, (wasm_size_t)INT_MAX), (const char *)wasi->str(old_path, old_path_len), (unsigned int)new_descriptor, (int)std::min(new_path_len, (wasm_size_t)INT_MAX), (const char *)wasi->str(new_path, new_path_len));

    wasi->check_bounds(result, 2);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_NOT_DIRECTORY;
            return;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                if (!wasi->fdtable[fd].dir_handle()->writable) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_READ_ONLY;
                    return;
                }

                std::string joined_old_path = dir_path_join(wasi, fd, old_path, old_path_len);
                if (joined_old_path.empty()) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_NOT_PERMITTED;
                    return;
                }

                std::string joined_new_path = dir_path_join(wasi, fd, new_path, new_path_len);
                if (joined_new_path.empty()) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_NOT_PERMITTED;
                    return;
                }

                PHYSFS_Stat old_stat;
                if (!PHYSFS_stat(joined_old_path.c_str(), &old_stat)) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_NO_ENTRY;
                    return;
                }

                if (old_stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
                    // We don't support renaming directories yet
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_UNSUPPORTED;
                    return;
                } else if (old_stat.filetype != PHYSFS_FILETYPE_REGULAR) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_IO;
                    return;
                }

                if (joined_old_path == joined_new_path) {
                    wasi->ref<uint8_t>(result) = false;
                    return;
                }

                PHYSFS_File *read_handle = PHYSFS_openRead(joined_old_path.c_str());
                if (read_handle == nullptr) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_IO;
                    return;
                }

                uint32_t root = wasi->fdtable[fd].type == wasi_fd_type::FS ? fd : wasi->fdtable[fd].dir_handle()->root;
                PHYSFS_File *write_handle = PHYSFS_openWrite(joined_new_path.c_str() + wasi->fdtable[root].dir_handle()->path.length());
                if (write_handle == nullptr) {
                    PHYSFS_close(read_handle);
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_IO;
                    return;
                }

                {
                    std::array<uint8_t, 4096> buffer;
                    PHYSFS_sint64 n;
                    for (;;) {
                        n = PHYSFS_readBytes(read_handle, buffer.data(), buffer.size());
                        if (n > 0) {
                            PHYSFS_writeBytes(write_handle, buffer.data(), n);
                        }
                        if (n < 0 || (size_t)n < buffer.size()) {
                            break;
                        }
                    }
                }

                PHYSFS_close(write_handle);
                PHYSFS_close(read_handle);
                PHYSFS_delete(joined_old_path.c_str() + wasi->fdtable[root].dir_handle()->path.length());

                wasi->ref<uint8_t>(result) = false;
                return;
            }
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_path_rename(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, wasm_ptr_t old_path, uint32_t old_path_len, uint32_t new_fd, wasm_ptr_t new_path, uint32_t new_path_len) {
    wasi->check_bounds(old_path, old_path_len);
    wasi->check_bounds(new_path, new_path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::path_rename(%u, \"%.*s\", %u, \"%.*s\")\n", (unsigned int)fd, (int)std::min(old_path_len, (uint32_t)INT_MAX), (const char *)wasi->str(old_path, old_path_len), (unsigned int)new_fd, (int)std::min(new_path_len, (uint32_t)INT_MAX), (const char *)wasi->str(new_path, new_path_len));

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_ENOTDIR;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                if (!wasi->fdtable[fd].dir_handle()->writable) {
                    return WASIP1_EROFS;
                }

                std::string joined_old_path = dir_path_join(wasi, fd, old_path, old_path_len);
                if (joined_old_path.empty()) {
                    return WASIP1_EPERM;
                }

                std::string joined_new_path = dir_path_join(wasi, fd, new_path, new_path_len);
                if (joined_new_path.empty()) {
                    return WASIP1_EPERM;
                }

                PHYSFS_Stat old_stat;
                if (!PHYSFS_stat(joined_old_path.c_str(), &old_stat)) {
                    return WASIP1_ENOENT;
                }

                if (old_stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
                    // We don't support renaming directories yet
                    return WASIP1_ENOTSUP;
                } else if (old_stat.filetype != PHYSFS_FILETYPE_REGULAR) {
                    return WASIP1_EIO;
                }

                if (joined_old_path == joined_new_path) {
                    return WASIP1_ESUCCESS;
                }

                PHYSFS_File *read_handle = PHYSFS_openRead(joined_old_path.c_str());
                if (read_handle == nullptr) {
                    return WASIP1_EIO;
                }

                uint32_t root = wasi->fdtable[fd].type == wasi_fd_type::FS ? fd : wasi->fdtable[fd].dir_handle()->root;
                PHYSFS_File *write_handle = PHYSFS_openWrite(joined_new_path.c_str() + wasi->fdtable[root].dir_handle()->path.length());
                if (write_handle == nullptr) {
                    PHYSFS_close(read_handle);
                    return WASIP1_EIO;
                }

                {
                    std::array<uint8_t, 4096> buffer;
                    PHYSFS_sint64 n;
                    for (;;) {
                        n = PHYSFS_readBytes(read_handle, buffer.data(), buffer.size());
                        if (n > 0) {
                            PHYSFS_writeBytes(write_handle, buffer.data(), n);
                        }
                        if (n < 0 || (size_t)n < buffer.size()) {
                            break;
                        }
                    }
                }

                PHYSFS_close(write_handle);
                PHYSFS_close(read_handle);
                PHYSFS_delete(joined_old_path.c_str() + wasi->fdtable[root].dir_handle()->path.length());

                return WASIP1_ESUCCESS;
            }
    }

    return WASIP1_EBADF;
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Esymlink0x2Dat(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t old_path, wasm_size_t old_path_len, wasm_ptr_t new_path, wasm_size_t new_path_len, wasm_ptr_t result) {
    wasi->check_bounds(old_path, old_path_len);
    wasi->check_bounds(new_path, new_path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.symlink-at(%u, \"%.*s\", \"%.*s\")\n", (unsigned int)fd, (int)std::min(old_path_len, (wasm_size_t)INT_MAX), (const char *)wasi->str(old_path, old_path_len), (int)std::min(new_path_len, (wasm_size_t)INT_MAX), (const char *)wasi->str(new_path, new_path_len));

    wasi->check_bounds(result, 2);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_NOT_DIRECTORY;
            return;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_UNSUPPORTED;
            return;
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_path_symlink(struct w2c_wasi__snapshot__preview1 *wasi, wasm_ptr_t old_path, uint32_t old_path_len, uint32_t fd, wasm_ptr_t new_path, uint32_t new_path_len) {
    wasi->check_bounds(old_path, old_path_len);
    wasi->check_bounds(new_path, new_path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::path_symlink(\"%.*s\", %u, \"%.*s\")\n", (int)std::min(old_path_len, (uint32_t)INT_MAX), (const char *)wasi->str(old_path, old_path_len), (unsigned int)fd, (int)std::min(new_path_len, (uint32_t)INT_MAX), (const char *)wasi->str(new_path, new_path_len));

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_ENOTDIR;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            return WASIP1_ENOTSUP;
    }

    return WASIP1_EBADF;
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Eunlink0x2Dfile0x2Dat(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t path, wasm_size_t path_len, wasm_ptr_t result) {
    wasi->check_bounds(path, path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.unlink-file-at(%u, \"%.*s\")\n", (unsigned int)fd, (int)std::min(path_len, (wasm_size_t)INT_MAX), (const char *)wasi->str(path, path_len));

    wasi->check_bounds(result, 2);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_NOT_DIRECTORY;
            return;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                if (!wasi->fdtable[fd].dir_handle()->writable) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_READ_ONLY;
                    return;
                }

                std::string joined_path = dir_path_join(wasi, fd, path, path_len);
                if (joined_path.empty()) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_NOT_PERMITTED;
                    return;
                }

                PHYSFS_Stat stat;
                if (!PHYSFS_stat(joined_path.c_str(), &stat)) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_NO_ENTRY;
                    return;
                }

                if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_IS_DIRECTORY;
                    return;
                } else if (stat.filetype != PHYSFS_FILETYPE_REGULAR) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_IO;
                    return;
                }

                uint32_t root = wasi->fdtable[fd].type == wasi_fd_type::FS ? fd : wasi->fdtable[fd].dir_handle()->root;
                if (!PHYSFS_delete(joined_path.c_str() + wasi->fdtable[root].dir_handle()->path.length())) {
                    switch (PHYSFS_getLastErrorCode()) {
                        case PHYSFS_ERR_READ_ONLY:
                        case PHYSFS_ERR_NO_WRITE_DIR:
                            wasi->ref<uint8_t>(result) = true;
                            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_READ_ONLY;
                            return;
                        case PHYSFS_ERR_PERMISSION:
                            wasi->ref<uint8_t>(result) = true;
                            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_ACCESS;
                            return;
                        default:
                            wasi->ref<uint8_t>(result) = true;
                            wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_IO;
                            return;
                    }
                }

                wasi->ref<uint8_t>(result) = false;
                return;
            }
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 1) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_path_unlink_file(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, wasm_ptr_t path, uint32_t path_len) {
    wasi->check_bounds(path, path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::path_unlink_file(%u, \"%.*s\")\n", (unsigned int)fd, (int)std::min(path_len, (uint32_t)INT_MAX), (const char *)wasi->str(path, path_len));

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_ENOTDIR;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                if (!wasi->fdtable[fd].dir_handle()->writable) {
                    return WASIP1_EROFS;
                }

                std::string joined_path = dir_path_join(wasi, fd, path, path_len);
                if (joined_path.empty()) {
                    return WASIP1_EPERM;
                }

                PHYSFS_Stat stat;
                if (!PHYSFS_stat(joined_path.c_str(), &stat)) {
                    return WASIP1_ENOENT;
                }

                if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
                    return WASIP1_EISDIR;
                } else if (stat.filetype != PHYSFS_FILETYPE_REGULAR) {
                    return WASIP1_EIO;
                }

                uint32_t root = wasi->fdtable[fd].type == wasi_fd_type::FS ? fd : wasi->fdtable[fd].dir_handle()->root;
                if (!PHYSFS_delete(joined_path.c_str() + wasi->fdtable[root].dir_handle()->path.length())) {
                    switch (PHYSFS_getLastErrorCode()) {
                        case PHYSFS_ERR_READ_ONLY:
                        case PHYSFS_ERR_NO_WRITE_DIR:
                            return WASIP1_EROFS;
                        case PHYSFS_ERR_PERMISSION:
                            return WASIP1_EACCES;
                        default:
                            return WASIP1_EIO;
                    }
                }

                return WASIP1_ESUCCESS;
            }
    }

    return WASIP1_EBADF;
}

static void metadata_hash_impl(const struct wasi_instance *wasi, const char *path, wasm_ptr_t result) noexcept {
    PHYSFS_Stat stat;
    if (PHYSFS_stat(path, &stat)) {
        wasi->ref<uint8_t>(result) = false;
        wasi->ref<uint64_t>(result + 8) = (uint64_t)stat.createtime ^ (uint64_t)stat.filesize;
        wasi->ref<uint64_t>(result + 16) = (uint64_t)stat.modtime ^ (uint64_t)stat.filetype;
    } else {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_IO;
    }
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Emetadata0x2Dhash(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.metadata-hash(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 24);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_INVALID;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
            wasi->ref<uint8_t>(result) = false;
            wasi->ref<uint64_t>(result + 8) = fd;
            wasi->ref<uint64_t>(result + 16) = 0;
            return;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            metadata_hash_impl(wasi, wasi->fdtable[fd].dir_handle()->path.c_str(), result);
            return;

        case wasi_fd_type::FSFILE:
            metadata_hash_impl(wasi, wasi->fdtable[fd].file_handle()->file.path(), result);
            return;
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddescriptor0x2Emetadata0x2Dhash0x2Dat(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint32_t path_flags, wasm_ptr_t path, wasm_size_t path_len, wasm_ptr_t result) {
    wasi->check_bounds(path, path_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]descriptor.metadata-hash-at(%u, %u, \"%.*s\")\n", (unsigned int)fd, (unsigned int)path_flags, (int)std::min(path_len, (wasm_size_t)INT_MAX), (const char *)wasi->str(path, path_len));

    wasi->check_bounds(result, 24);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_NOT_DIRECTORY;
            return;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                std::string joined_path = dir_path_join(wasi, fd, path, path_len);
                if (joined_path.empty()) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_NOT_PERMITTED;
                    return;
                }
                metadata_hash_impl(wasi, joined_path.c_str(), result);
                return;
            }
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 8) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bresource0x2Ddrop0x5Ddescriptor(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[resource-drop]descriptor(%u)\n", (unsigned int)fd);

    if (fd >= wasi->fdtable.size()) {
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return;

        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSFILE:
            wasi->deallocate_file_descriptor(fd);
            return;
    }
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_close(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::fd_close(%u)\n", (unsigned int)fd);

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_EINVAL;

        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSFILE:
            wasi->deallocate_file_descriptor(fd);
            return WASIP1_ESUCCESS;
    }

    return WASIP1_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_renumber(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, uint32_t to) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::fd_renumber(%u, %u)\n", (unsigned int)fd, (unsigned int)to);

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_EINVAL;

        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSFILE:
            break;
    }

    if (fd == to) {
        return WASIP1_ESUCCESS;
    }

    if (to >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[to].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_EINVAL;

        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSFILE:
            wasi->deallocate_file_descriptor(to);
            if (to == wasi->fdtable.size()) {
                wasi->fdtable.push_back(wasi->fdtable[fd]);
            } else {
                wasi->fdtable[to] = wasi->fdtable[fd];
            }

            if (fd == wasi->fdtable.size() - 1) {
                wasi->fdtable.pop_back();
                while (!wasi->fdtable.empty() && wasi->fdtable.back().type == wasi_fd_type::VACANT) {
                    assert(!wasi->vacant_fds.empty() && wasi->vacant_fds.maximum() == wasi->fdtable.size() - 1);
                    wasi->vacant_fds.pop_maximum();
                    wasi->fdtable.pop_back();
                }
            } else {
                wasi->fdtable[fd] = {nullptr, wasi_fd_type::VACANT};
                wasi->vacant_fds.push(fd);
            }

            return WASIP1_ESUCCESS;
    }

    return WASIP1_EBADF;
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bmethod0x5Ddirectory0x2Dentry0x2Dstream0x2Eread0x2Ddirectory0x2Dentry(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[method]directory-entry-stream.read-directory-entry(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 5 * sizeof(wasm_ptr_t));

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
            return;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_FILESYSTEM_ERROR_INVALID;
            return;

        case wasi_fd_type::FSDIRSTREAM:
            wasi->ref<uint8_t>(result) = false;
            if (wasi->fdtable[fd].dir_stream()->entries.empty()) {
                wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = false;
            } else {
                const std::pair<std::string, enum PHYSFS_FileType> &entry = wasi->fdtable[fd].dir_stream()->entries.front();
                wasm_ptr_t buf = wasi->cabi_alloc<char>(entry.first.length());
                wasi->arycpy(buf, entry.first.c_str(), entry.first.length());
                wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = true;
                wasi->ref<uint8_t>(result + 2 * sizeof(wasm_ptr_t)) = entry.second == PHYSFS_FILETYPE_DIRECTORY ? WASI_FILESYSTEM_DESCRIPTOR_TYPE_DIRECTORY : WASI_FILESYSTEM_DESCRIPTOR_TYPE_REGULAR_FILE;
                wasi->ref<wasm_ptr_t>(result + 3 * sizeof(wasm_ptr_t)) = buf;
                wasi->ref<wasm_size_t>(result + 4 * sizeof(wasm_ptr_t)) = entry.first.length();
                wasi->fdtable[fd].dir_stream()->entries.pop_front();
            }
            return;
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR;
}

extern "C" void w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0_0x5Bresource0x2Ddrop0x5Ddirectory0x2Dentry0x2Dstream(struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 *wasi, wasm_resource_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:filesystem/types@0.2.0::[resource-drop]directory-entry-stream(%u)\n", (unsigned int)fd);

    if (fd >= wasi->fdtable.size()) {
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::AISTREAM:
            return;

        case wasi_fd_type::FSDIRSTREAM:
            wasi->deallocate_file_descriptor(fd);
            return;
    }
}

extern "C" void w2c_wasi0x3Afilesystem0x2Fpreopens0x4000x2E20x2E0_get0x2Ddirectories(struct w2c_wasi0x3Afilesystem0x2Fpreopens0x4000x2E20x2E0 *wasi, wasm_ptr_t result) {
    LOG_PRINT(RETRO_LOG_DEBUG, "wasi:filesystem/preopens@0.2.0::get-directories()\n");

    wasi->check_bounds(result, 2 * sizeof(wasm_ptr_t));

    wasm_size_t num_preopens = 0;
    for (const struct wasi_file_entry &entry : wasi->fdtable) {
        if (entry.type == wasi_fd_type::FS) {
            ++num_preopens;
        } else if (entry.type != wasi_fd_type::STDIN && entry.type != wasi_fd_type::STDOUT && entry.type != wasi_fd_type::STDERR) {
            break;
        }
    }

    MKXPZ_FORCED_ASSERT(num_preopens * (wasm_size_t)(3 * sizeof(wasm_ptr_t)) >= num_preopens);

    wasm_ptr_t buf = wasi->cabi_alloc<wasm_ptr_t>(num_preopens * (wasm_size_t)(3 * sizeof(wasm_ptr_t)));
    wasi->ref<wasm_ptr_t>(result) = buf;
    wasi->ref<wasm_size_t>(result + sizeof(wasm_ptr_t)) = num_preopens;

    wasm_size_t i = 0;
    for (const struct wasi_file_entry &entry : wasi->fdtable) {
        if (entry.type == wasi_fd_type::FS) {
            LOG_PRINTF(RETRO_LOG_DEBUG, "WASI resource created: descriptor(%u) -> preopened directory \"%s\"\n", (unsigned int)i, entry.dir_handle()->path.c_str());
            wasi->ref<wasm_resource_t>(buf) = i;
            buf += sizeof(wasm_ptr_t);

            MKXPZ_FORCED_ASSERT(entry.dir_handle()->path.length() <= (wasm_size_t)-1);
            wasm_ptr_t str = wasi->cabi_alloc<char>(entry.dir_handle()->path.length());
            wasi->arycpy(str, entry.dir_handle()->path.c_str(), entry.dir_handle()->path.length());
            wasi->ref<wasm_ptr_t>(buf) = str;
            buf += sizeof(wasm_ptr_t);
            wasi->ref<wasm_size_t>(buf) = entry.dir_handle()->path.length();
            buf += sizeof(wasm_ptr_t);
        } else if (entry.type != wasi_fd_type::STDIN && entry.type != wasi_fd_type::STDOUT && entry.type != wasi_fd_type::STDERR) {
            break;
        }
        ++i;
    }
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_prestat_dir_name(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, wasm_ptr_t path, uint32_t path_len) {
    MKXPZ_FORCED_ASSERT(path_len + (wasm_size_t)1 > path_len);
    wasi->check_bounds(path, path_len + 1);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::fd_prestat_dir_name(%u, 0x%08llx (%u))\n", (unsigned int)fd, (unsigned long long)path, (unsigned int)path_len);

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::FS:
            wasi->strncpy_s(path, wasi->fdtable[fd].dir_handle()->path.c_str(), path_len + 1);
            return WASIP1_ESUCCESS;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_EINVAL;
    }

    return WASIP1_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_prestat_get(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::fd_prestat_get(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 8);

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::FS:
            wasi->ref<uint32_t>(result) = 0;
            wasi->ref<uint32_t>(result + 4) = wasi->fdtable[fd].dir_handle()->path.length();
            return WASIP1_ESUCCESS;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_EINVAL;
    }

    return WASIP1_EBADF;
}

////////////////////////////////////////////////////////////////////////////////
// wasi:io
////////////////////////////////////////////////////////////////////////////////

extern "C" void w2c_wasi0x3Aio0x2Ferror0x4000x2E20x2E0_0x5Bresource0x2Ddrop0x5Derror(struct w2c_wasi0x3Aio0x2Ferror0x4000x2E20x2E0 *wasi, wasm_resource_t self) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:io/error@0.2.0::[resource-drop]error(%u)\n", (unsigned int)self);
}

static void stream_read_impl(struct wasi_instance *wasi, uint32_t fd, uint64_t len, wasm_ptr_t result) {
    MKXPZ_FORCED_ASSERT(len <= (wasm_size_t)-1);
    wasi->check_bounds(result, 3 * sizeof(wasm_ptr_t));

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_STREAMS_ERROR_CLOSED;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_STREAMS_ERROR_CLOSED;
            return;

        case wasi_fd_type::STDIN:
            wasi->ref<uint8_t>(result) = false;
            wasi->ref<wasm_ptr_t>(result + sizeof(wasm_ptr_t)) = wasi->cabi_alloc<uint8_t>(0);
            wasi->ref<wasm_size_t>(result + 2 * sizeof(wasm_ptr_t)) = 0;
            return;

        case wasi_fd_type::FSFILESTREAM:
            {
                uint32_t file_fd = wasi->fdtable[fd].file_stream()->root;
                if (file_fd >= wasi->fdtable.size() || wasi->fdtable[file_fd].type != wasi_fd_type::FSFILE) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_STREAMS_ERROR_CLOSED;
                    return;
                }
                if (!wasi->fdtable[file_fd].file_handle()->file.is_read_open()) {
                    close_file_stream(wasi, fd);
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_STREAMS_ERROR_LAST_OPERATION_FAILED;
                    wasi->ref<uint32_t>(result + sizeof(wasm_ptr_t) + 4) = WASI_FILESYSTEM_ERROR_ACCESS;
                    return;
                }
                if (!PHYSFS_seek(wasi->fdtable[file_fd].file_handle()->file.get_read(), wasi->fdtable[fd].file_stream()->offset)) {
                    close_file_stream(wasi, fd);
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_STREAMS_ERROR_LAST_OPERATION_FAILED;
                    wasi->ref<uint32_t>(result + sizeof(wasm_ptr_t) + 4) = WASI_FILESYSTEM_ERROR_IO;
                    return;
                }
                std::vector<uint8_t> buffer(len);
                uint64_t n = PHYSFS_readBytes(wasi->fdtable[file_fd].file_handle()->file.get_read(), buffer.data(), len);
                if (n == (uint64_t)-1) {
                    close_file_stream(wasi, fd);
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_STREAMS_ERROR_LAST_OPERATION_FAILED;
                    wasi->ref<uint32_t>(result + sizeof(wasm_ptr_t) + 4) = WASI_FILESYSTEM_ERROR_IO;
                    return;
                }
                if (n == 0) {
                    close_file_stream(wasi, fd);
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_STREAMS_ERROR_CLOSED;
                    return;
                }
                wasi->fdtable[fd].file_stream()->offset += n;
                wasm_ptr_t buf = wasi->cabi_alloc<uint8_t>(n);
                wasi->arycpy(buf, buffer.data(), n);
                wasi->ref<uint8_t>(result) = false;
                wasi->ref<wasm_ptr_t>(result + sizeof(wasm_ptr_t)) = buf;
                wasi->ref<wasm_size_t>(result + 2 * sizeof(wasm_ptr_t)) = n;
                return;
            }
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_STREAMS_ERROR_CLOSED;
}

extern "C" void w2c_wasi0x3Aio0x2Fstreams0x4000x2E20x2E0_0x5Bmethod0x5Dinput0x2Dstream0x2Eread(struct w2c_wasi0x3Aio0x2Fstreams0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint64_t len, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:io/streams@0.2.0::[method]input-stream.read(%u, %llu)\n", (unsigned int)fd, (unsigned long long)len);
    stream_read_impl(wasi, fd, len, result);
}

extern "C" void w2c_wasi0x3Aio0x2Fstreams0x4000x2E20x2E0_0x5Bmethod0x5Dinput0x2Dstream0x2Eblocking0x2Dread(struct w2c_wasi0x3Aio0x2Fstreams0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint64_t len, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:io/streams@0.2.0::[method]input-stream.blocking-read(%u, %llu)\n", (unsigned int)fd, (unsigned long long)len);
    stream_read_impl(wasi, fd, len, result);
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_read(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, wasm_ptr_t iovs, uint32_t iovs_len, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::fd_read(%u, 0x%08llx (%u))\n", (unsigned int)fd, (unsigned long long)iovs, (unsigned int)iovs_len);

    MKXPZ_FORCED_ASSERT(8 * (wasm_size_t)iovs_len >= (wasm_size_t)iovs_len);
    wasi->check_bounds(iovs, 8 * (wasm_size_t)iovs_len);
    wasi->check_bounds(result, 4);

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
            wasi->ref<uint32_t>(result) = 0;
            return WASIP1_ESUCCESS;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_EINVAL;

        case wasi_fd_type::FSFILE:
            {
                if (!wasi->fdtable[fd].file_handle()->file.is_read_open()) {
                    return WASIP1_EACCES;
                }
                if (!PHYSFS_seek(wasi->fdtable[fd].file_handle()->file.get_read(), wasi->fdtable[fd].file_handle()->offset)) {
                    return WASIP1_EIO;
                }
                uint32_t size = 0;
                while (iovs_len > 0) {
                    uint32_t ptr = wasi->ref<uint32_t>(iovs);
                    uint32_t length = wasi->ref<uint32_t>(iovs + 4);
                    wasi->check_bounds(ptr, length);
#ifdef MKXPZ_BIG_ENDIAN
                    uint8_t *buffer = &wasi->ref<uint8_t>(ptr, std::max(length, (uint32_t)1) - 1);
#else
                    uint8_t *buffer = &wasi->ref<uint8_t>(ptr);
#endif // MKXPZ_BIG_ENDIAN
                    uint64_t n = PHYSFS_readBytes(wasi->fdtable[fd].file_handle()->file.get_read(), buffer, length);
                    if (n == (uint64_t)-1) {
                        return WASIP1_EIO;
                    }
#ifdef MKXPZ_BIG_ENDIAN
                    std::reverse(buffer, buffer + length);
#endif // MKXPZ_BIG_ENDIAN
                    size += n;
                    wasi->fdtable[fd].file_handle()->offset += (uint64_t)n;
                    iovs += 8;
                    --iovs_len;
                }
                wasi->ref<uint32_t>(result) = size;
                return WASIP1_ESUCCESS;
            }
    }

    return WASIP1_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_seek(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, uint64_t offset, uint32_t whence, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::fd_seek(%u, %llu, %u)\n", (unsigned int)fd, (unsigned long long)offset, (unsigned int)whence);

    wasi->check_bounds(result, 8);

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_EINVAL;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
            return WASIP1_ESUCCESS;

        case wasi_fd_type::FSFILE:
            {
                switch (whence) {
                    case 2:
                        if (wasi->fdtable[fd].file_handle()->file.is_read_open() || wasi->fdtable[fd].file_handle()->file.is_write_open()) {
                            uint64_t length = PHYSFS_fileLength(wasi->fdtable[fd].file_handle()->file.is_read_open() ? wasi->fdtable[fd].file_handle()->file.get_read() : wasi->fdtable[fd].file_handle()->file.get_write());
                            if (length == (uint64_t)-1) {
                                return WASIP1_EIO;
                            }
                            offset += length;
                        }
                    case 0:
                        wasi->fdtable[fd].file_handle()->offset = offset;
                        break;
                    case 1:
                        wasi->fdtable[fd].file_handle()->offset += offset;
                        break;
                    default:
                        return WASIP1_EINVAL;
                }
                wasi->ref<uint64_t>(result) = wasi->fdtable[fd].file_handle()->offset;
            }
            return WASIP1_ESUCCESS;
    }

    return WASIP1_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_tell(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::fd_tell(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 8);

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_EINVAL;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
            wasi->ref<uint64_t>(result) = 0;
            return WASIP1_ESUCCESS;

        case wasi_fd_type::FSFILE:
            wasi->ref<uint64_t>(result) = wasi->fdtable[fd].file_handle()->offset;
            return WASIP1_ESUCCESS;
    }

    return WASIP1_EBADF;
}

extern "C" wasm_resource_t w2c_wasi0x3Aio0x2Fstreams0x4000x2E20x2E0_0x5Bmethod0x5Dinput0x2Dstream0x2Esubscribe(struct w2c_wasi0x3Aio0x2Fstreams0x4000x2E20x2E0 *wasi, wasm_resource_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:io/streams@0.2.0::[method]input-stream.subscribe(%u)\n", (unsigned int)fd);
    LOG_PRINTF(RETRO_LOG_DEBUG, "WASI resource created: pollable(%u) -> input stream %u\n", (unsigned int)fd, (unsigned int)fd);
    return fd;
}

static void close_stream_impl(struct wasi_instance *wasi, uint32_t fd) {
    if (fd >= wasi->fdtable.size()) {
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::AISTREAM:
            return;

        case wasi_fd_type::FSFILESTREAM:
            wasi->deallocate_file_descriptor(fd);
            return;
    }
}

extern "C" void w2c_wasi0x3Aio0x2Fstreams0x4000x2E20x2E0_0x5Bresource0x2Ddrop0x5Dinput0x2Dstream(struct w2c_wasi0x3Aio0x2Fstreams0x4000x2E20x2E0 *wasi, wasm_resource_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:io/streams@0.2.0::[resource-drop]input-stream(%u)\n", (unsigned int)fd);
    close_stream_impl(wasi, fd);
}

extern "C" void w2c_wasi0x3Aio0x2Fstreams0x4000x2E20x2E0_0x5Bmethod0x5Doutput0x2Dstream0x2Echeck0x2Dwrite(struct w2c_wasi0x3Aio0x2Fstreams0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:io/streams@0.2.0::[method]output-stream.check-write(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 16);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 8) = WASI_STREAMS_ERROR_CLOSED;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
        case wasi_fd_type::STDIN:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 8) = WASI_STREAMS_ERROR_CLOSED;
            return;

        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
            wasi->ref<uint8_t>(result) = false;
            wasi->ref<uint64_t>(result + 8) = (wasm_size_t)-1;
            return;

        case wasi_fd_type::FSFILESTREAM:
            {
                uint32_t file_fd = wasi->fdtable[fd].file_stream()->root;
                if (file_fd >= wasi->fdtable.size() || wasi->fdtable[file_fd].type != wasi_fd_type::FSFILE) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 8) = WASI_STREAMS_ERROR_CLOSED;
                    return;
                }
                if (!wasi->fdtable[file_fd].file_handle()->file.is_write_open()) {
                    close_file_stream(wasi, fd);
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 8) = WASI_STREAMS_ERROR_LAST_OPERATION_FAILED;
                    wasi->ref<uint32_t>(result + 12) = WASI_FILESYSTEM_ERROR_ACCESS;
                    return;
                }
                wasi->ref<uint8_t>(result) = false;
                wasi->ref<uint64_t>(result + 8) = (wasm_size_t)-1;
                return;
            }
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 8) = WASI_STREAMS_ERROR_CLOSED;
}

extern "C" void w2c_wasi0x3Aio0x2Fstreams0x4000x2E20x2E0_0x5Bmethod0x5Doutput0x2Dstream0x2Ewrite(struct w2c_wasi0x3Aio0x2Fstreams0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t contents, wasm_size_t contents_len, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:io/streams@0.2.0::[method]output-stream.write(%u, 0x%08llx (%llu))\n", (unsigned int)fd, (unsigned long long)contents, (unsigned long long)contents_len);

    wasi->check_bounds(contents, contents_len);
    wasi->check_bounds(result, 12);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 4) = WASI_STREAMS_ERROR_CLOSED;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
        case wasi_fd_type::STDIN:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 4) = WASI_STREAMS_ERROR_CLOSED;
            return;

        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
            {
#ifdef MKXPZ_BIG_ENDIAN
                std::string buf;
                if (contents_len > 0) {
                    std::reverse(&wasi->ref<char>(contents) - (contents_len - 1), &wasi->ref<char>(contents) - 1);
                    buf.append(&wasi->ref<char>(contents) - (contents_len - 1), contents_len);
                    std::reverse(&wasi->ref<char>(contents) - (contents_len - 1), &wasi->ref<char>(contents) - 1);
                }
                const char *str = buf.data();
#else
                const char *str = &wasi->ref<char>(contents);
#endif // MKXPZ_BIG_ENDIAN
                std::string &line_buffer = wasi->stdio_line_buffers[wasi->fdtable[fd].type == wasi_fd_type::STDOUT ? 0 : 1];
                size_t line_start_index = 0;
                for (size_t i = 0; i < contents_len; ++i) {
                    if (str[i] == '\n') {
                        mkxp_retro_log_printf(
                            wasi->fdtable[fd].type == wasi_fd_type::STDOUT ? RETRO_LOG_INFO : RETRO_LOG_WARN,
                            wasi->fdtable[fd].type == wasi_fd_type::STDOUT ? "[mkxp-z stdout] %.*s\n" : "[mkxp-z stderr] %.*s\n",
                            std::min(line_buffer.length() + (i - line_start_index), (size_t)INT_MAX),
                            line_buffer.empty() ? str + line_start_index : line_buffer.append(str + line_start_index, i - line_start_index).c_str()
                        );
                        line_buffer.clear();
                        line_start_index = i + 1;
                    }
                }
                if ((wasm_ptr_t)line_buffer.size() + (wasm_ptr_t)(contents_len - line_start_index) >= (wasm_ptr_t)line_buffer.size()) {
                    line_buffer.append(str + line_start_index, contents_len - line_start_index);
                }
                wasi->ref<uint8_t>(result) = false;
                return;
            }

        case wasi_fd_type::FSFILESTREAM:
            {
                uint32_t file_fd = wasi->fdtable[fd].file_stream()->root;
                if (file_fd >= wasi->fdtable.size() || wasi->fdtable[file_fd].type != wasi_fd_type::FSFILE) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 4) = WASI_STREAMS_ERROR_CLOSED;
                    return;
                }
                if (!wasi->fdtable[file_fd].file_handle()->file.is_write_open()) {
                    close_file_stream(wasi, fd);
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 4) = WASI_STREAMS_ERROR_LAST_OPERATION_FAILED;
                    wasi->ref<uint32_t>(result + 8) = WASI_FILESYSTEM_ERROR_ACCESS;
                    return;
                }
                if (!PHYSFS_seek(wasi->fdtable[file_fd].file_handle()->file.get_write(), wasi->fdtable[fd].file_stream()->offset)) {
                    close_file_stream(wasi, fd);
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 4) = WASI_STREAMS_ERROR_LAST_OPERATION_FAILED;
                    wasi->ref<uint32_t>(result + 8) = WASI_FILESYSTEM_ERROR_IO;
                    return;
                }
#ifdef MKXPZ_BIG_ENDIAN
                uint8_t *buf = &wasi->ref<uint8_t>(contents, std::max(contents_len, (wasm_size_t)1) - 1);
                std::reverse(buf, buf + contents_len);
#else
                uint8_t *buf = &wasi->ref<uint8_t>(contents);
#endif // MKXPZ_BIG_ENDIAN
                uint64_t n = PHYSFS_writeBytes(wasi->fdtable[file_fd].file_handle()->file.get_write(), buf, contents_len);
#ifdef MKXPZ_BIG_ENDIAN
                std::reverse(buf, buf + contents_len);
#endif // MKXPZ_BIG_ENDIAN
                if (n == (uint64_t)-1) {
                    close_file_stream(wasi, fd);
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 4) = WASI_STREAMS_ERROR_LAST_OPERATION_FAILED;
                    wasi->ref<uint32_t>(result + 8) = WASI_FILESYSTEM_ERROR_IO;
                    return;
                }
                if (n == 0) {
                    close_file_stream(wasi, fd);
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 4) = WASI_STREAMS_ERROR_CLOSED;
                    return;
                }
                wasi->fdtable[fd].file_stream()->offset += n;
                wasi->ref<uint8_t>(result) = false;
                return;
            }
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 4) = WASI_STREAMS_ERROR_CLOSED;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_write(struct w2c_wasi__snapshot__preview1 *wasi, uint32_t fd, wasm_ptr_t iovs, uint32_t iovs_len, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::fd_write(%u, 0x%08llx (%u))\n", (unsigned int)fd, (unsigned long long)iovs, (unsigned int)iovs_len);

    MKXPZ_FORCED_ASSERT(8 * (wasm_size_t)iovs_len >= (wasm_size_t)iovs_len);
    wasi->check_bounds(iovs, 8 * (wasm_size_t)iovs_len);
    wasi->check_bounds(result, 4);

    if (fd >= wasi->fdtable.size()) {
        return WASIP1_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASIP1_EBADF;

        case wasi_fd_type::STDIN:
            wasi->ref<uint32_t>(result) = 0;
            return WASIP1_ESUCCESS;

        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
            {
                uint32_t size = 0;
                std::string buf;
                while (iovs_len > 0) {
                    wasm_ptr_t str = wasi->ref<uint32_t>(iovs);
                    uint32_t n = wasi->ref<uint32_t>(iovs + 4);
                    wasi->check_bounds(str, n);
#ifdef MKXPZ_BIG_ENDIAN
                    if (n > 0) {
                        std::reverse(&wasi->ref<char>(str) - (n - 1), &wasi->ref<char>(str) - 1);
                        buf.append(&wasi->ref<char>(str) - (n - 1), n);
                        std::reverse(&wasi->ref<char>(str) - (n - 1), &wasi->ref<char>(str) - 1);
                    }
#else
                    buf.append(&wasi->ref<char>(str), n);
#endif // MKXPZ_BIG_ENDIAN
                    size += n;
                    iovs += 8;
                    --iovs_len;
                }
                std::string &line_buffer = wasi->stdio_line_buffers[wasi->fdtable[fd].type == wasi_fd_type::STDOUT ? 0 : 1];
                size_t line_start_index = 0, i = 0;
                for (char c : buf) {
                    if (c == '\n') {
                        mkxp_retro_log_printf(
                            wasi->fdtable[fd].type == wasi_fd_type::STDOUT ? RETRO_LOG_INFO : RETRO_LOG_WARN,
                            wasi->fdtable[fd].type == wasi_fd_type::STDOUT ? "[mkxp-z stdout] %.*s\n" : "[mkxp-z stderr] %.*s\n",
                            std::min(line_buffer.length() + (i - line_start_index), (size_t)INT_MAX),
                            line_buffer.empty() ? buf.data() + line_start_index : line_buffer.append(buf.data() + line_start_index, i - line_start_index).c_str()
                        );
                        line_buffer.clear();
                        line_start_index = i + 1;
                    }
                    ++i;
                }
                if ((wasm_ptr_t)line_buffer.size() + (wasm_ptr_t)(buf.size() - line_start_index) >= (wasm_ptr_t)line_buffer.size()) {
                    line_buffer.append(buf.begin() + line_start_index, buf.end());
                }
                wasi->ref<uint32_t>(result) = size;
                return WASIP1_ESUCCESS;
            }

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
        case wasi_fd_type::AISTREAM:
            return WASIP1_EINVAL;

        case wasi_fd_type::FSFILE:
            {
                if (!wasi->fdtable[fd].file_handle()->file.is_write_open()) {
                    return WASIP1_EACCES;
                }
                if (!PHYSFS_seek(wasi->fdtable[fd].file_handle()->file.get_write(), wasi->fdtable[fd].file_handle()->offset)) {
                    return WASIP1_EIO;
                }
                uint32_t size = 0;
                while (iovs_len > 0) {
                    uint32_t ptr = wasi->ref<uint32_t>(iovs);
                    uint32_t length = wasi->ref<uint32_t>(iovs + 4);
                    wasi->check_bounds(ptr, length);
#ifdef MKXPZ_BIG_ENDIAN
                    uint8_t *buffer = &wasi->ref<uint8_t>(ptr, std::max(length, (uint32_t)1) - 1);
                    std::reverse(buffer, buffer + length);
#else
                    uint8_t *buffer = &wasi->ref<uint8_t>(ptr);
#endif // MKXPZ_BIG_ENDIAN
                    uint64_t n = PHYSFS_writeBytes(wasi->fdtable[fd].file_handle()->file.get_write(), buffer, length);
#ifdef MKXPZ_BIG_ENDIAN
                    std::reverse(buffer, buffer + length);
#endif // MKXPZ_BIG_ENDIAN
                    if (n == (uint64_t)-1) {
                        return WASIP1_EIO;
                    }
                    size += n;
                    wasi->fdtable[fd].file_handle()->offset += (uint64_t)n;
                    iovs += 8;
                    --iovs_len;
                }
                wasi->ref<uint32_t>(result) = size;
                return WASIP1_ESUCCESS;
            }
    }

    return WASIP1_EBADF;
}

extern "C" void w2c_wasi0x3Aio0x2Fstreams0x4000x2E20x2E0_0x5Bmethod0x5Doutput0x2Dstream0x2Eblocking0x2Dflush(struct w2c_wasi0x3Aio0x2Fstreams0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:io/streams@0.2.0::[method]output-stream.blocking-flush(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 12);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 4) = WASI_STREAMS_ERROR_CLOSED;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
        case wasi_fd_type::STDIN:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::AISTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 4) = WASI_STREAMS_ERROR_CLOSED;
            return;

        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
            wasi->ref<uint8_t>(result) = false;
            return;

        case wasi_fd_type::FSFILESTREAM:
            {
                uint32_t file_fd = wasi->fdtable[fd].file_stream()->root;
                if (file_fd >= wasi->fdtable.size() || wasi->fdtable[file_fd].type != wasi_fd_type::FSFILE) {
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 4) = WASI_STREAMS_ERROR_CLOSED;
                    return;
                }
                if (!wasi->fdtable[file_fd].file_handle()->file.is_write_open()) {
                    wasi->ref<uint8_t>(result) = false;
                } else if (PHYSFS_flush(wasi->fdtable[file_fd].file_handle()->file.get_write()) == 0) {
                    close_file_stream(wasi, fd);
                    wasi->ref<uint8_t>(result) = true;
                    wasi->ref<uint8_t>(result + 4) = WASI_STREAMS_ERROR_LAST_OPERATION_FAILED;
                    wasi->ref<uint32_t>(result + 8) = WASI_FILESYSTEM_ERROR_IO;
                } else {
                    wasi->ref<uint8_t>(result) = false;
                }
                return;
            }
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 4) = WASI_STREAMS_ERROR_CLOSED;
}

extern "C" wasm_resource_t w2c_wasi0x3Aio0x2Fstreams0x4000x2E20x2E0_0x5Bmethod0x5Doutput0x2Dstream0x2Esubscribe(struct w2c_wasi0x3Aio0x2Fstreams0x4000x2E20x2E0 *wasi, wasm_resource_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:io/streams@0.2.0::[method]output-stream.subscribe(%u)\n", (unsigned int)fd);
    LOG_PRINTF(RETRO_LOG_DEBUG, "WASI resource created: pollable(%u) -> output stream %u\n", (unsigned int)fd, (unsigned int)fd);
    return fd;
}

extern "C" void w2c_wasi0x3Aio0x2Fstreams0x4000x2E20x2E0_0x5Bresource0x2Ddrop0x5Doutput0x2Dstream(struct w2c_wasi0x3Aio0x2Fstreams0x4000x2E20x2E0 *wasi, wasm_resource_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:io/streams@0.2.0::[resource-drop]output-stream(%u)\n", (unsigned int)fd);
    close_stream_impl(wasi, fd);
}

extern "C" void w2c_wasi0x3Aio0x2Fpoll0x4000x2E20x2E0_0x5Bmethod0x5Dpollable0x2Eblock(struct w2c_wasi0x3Aio0x2Fpoll0x4000x2E20x2E0 *wasi, wasm_resource_t self) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:io/poll@0.2.0::[method]pollable.block(%u)\n", (unsigned int)self);
}

extern "C" void w2c_wasi0x3Aio0x2Fpoll0x4000x2E20x2E0_poll(struct w2c_wasi0x3Aio0x2Fpoll0x4000x2E20x2E0 *wasi, wasm_ptr_t in, wasm_size_t in_len, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:io/poll@0.2.0::poll(0x%08llx (%llu))\n", (unsigned long long)in, (unsigned long long)in_len);

    wasi->check_bounds(result, 2 * sizeof(wasm_ptr_t));

    MKXPZ_FORCED_ASSERT(in_len * (wasm_size_t)4 >= in_len);
    wasi->check_bounds(in, in_len * (wasm_size_t)4);
    MKXPZ_FORCED_ASSERT(in_len != 0 && in_len <= (uint32_t)-1);

    wasi->ref<wasm_ptr_t>(result) = wasi->cabi_alloc<wasm_resource_t>(0);
    wasi->ref<wasm_size_t>(result + sizeof(wasm_ptr_t)) = 0;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_poll_oneoff(struct w2c_wasi__snapshot__preview1 *wasi, wasm_ptr_t in, wasm_ptr_t out, uint32_t nsubscriptions, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::poll_oneoff(0x%08llx, 0x%08llx, %u)\n", (unsigned long long)in, (unsigned long long)out, (unsigned int)nsubscriptions);

    wasi->check_bounds(result, 4);

    return WASIP1_ENOTSUP;
}

extern "C" void w2c_wasi0x3Aio0x2Fpoll0x4000x2E20x2E0_0x5Bresource0x2Ddrop0x5Dpollable(struct w2c_wasi0x3Aio0x2Fpoll0x4000x2E20x2E0 *wasi, wasm_resource_t self) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:io/poll@0.2.0::[resource-drop]pollable(%u)\n", (unsigned int)self);
}

////////////////////////////////////////////////////////////////////////////////
// wasi:random
////////////////////////////////////////////////////////////////////////////////

static void get_random_impl(struct wasi_instance *wasi, wasm_ptr_t buf, wasm_size_t buf_len) noexcept {
    while (buf_len > 0) {
        if (wasi->prng_buffer_size == 0) {
            wasi->prng_buffer_size = 4;

            // PCG32 XSH RR (based on https://github.com/imneme/pcg-cpp, licensed MIT)
            uint64_t state = wasi->prng_state;
            wasi->prng_state = wasi->prng_state * (uint64_t)6364136223846793005U + (uint64_t)1442695040888963407U; // Advance state before computing output to improve instruction-level parallelism
            uint32_t xsh = (state ^ (state >> 18U)) >> 27U;
            uint32_t rot = state >> 59U;
            uint32_t out = xsh >> rot | xsh << ((uint32_t)31U - rot);
#ifdef MKXPZ_BIG_ENDIAN
            // Byte swap the output on big-endian machines to preserve state state compatibility across machines with different endiannesses
            std::reverse_copy((uint8_t *)&out, (uint8_t *)&out + 4, wasi->prng_buffer);
#else
            std::memcpy(wasi->prng_buffer, &out, 4);
#endif // MKXPZ_BIG_ENDIAN
        } else {
            wasm_size_t n = std::min(buf_len, wasi->prng_buffer_size);
            wasi->arycpy(buf, wasi->prng_buffer + ((wasm_size_t)4 - n), n);
            buf += n;
            buf_len -= n;
            wasi->prng_buffer_size -= n;
        }
    }
}

extern "C" void w2c_wasi0x3Arandom0x2Frandom0x4000x2E20x2E0_get0x2Drandom0x2Dbytes(struct w2c_wasi0x3Arandom0x2Frandom0x4000x2E20x2E0 *wasi, uint64_t len, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:random/random@0.2.0::get-random-bytes(%lld)\n", (unsigned long long)len);

    MKXPZ_FORCED_ASSERT(len <= (wasm_size_t)-1);
    wasi->check_bounds(result, 2 * sizeof(wasm_ptr_t));

    wasm_ptr_t buf = wasi->cabi_alloc<uint8_t>(len);
    get_random_impl(wasi, buf, len);
    wasi->ref<wasm_ptr_t>(result) = buf;
    wasi->ref<wasm_size_t>(result + sizeof(wasm_ptr_t)) = len;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_random_get(struct w2c_wasi__snapshot__preview1 *wasi, wasm_ptr_t buf, uint32_t buf_len) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi_snapshot_preview1::random_get(0x%08llx (%u))\n", (unsigned long long)buf, (unsigned int)buf_len);

    wasi->check_bounds(buf, buf_len);
    get_random_impl(wasi, buf, buf_len);
    return WASIP1_ESUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
// wasi:sockets
////////////////////////////////////////////////////////////////////////////////

extern "C" wasm_resource_t w2c_wasi0x3Asockets0x2Finstance0x2Dnetwork0x4000x2E20x2E0_instance0x2Dnetwork(struct w2c_wasi0x3Asockets0x2Finstance0x2Dnetwork0x4000x2E20x2E0 *wasi) {
    LOG_PRINT(RETRO_LOG_DEBUG, "wasi:sockets/instance-network@0.2.0::instance-network()\n");
    LOG_PRINT(RETRO_LOG_DEBUG, "WASI resource created: network(1)\n");
    return 1;
}

extern "C" void w2c_wasi0x3Asockets0x2Fnetwork0x4000x2E20x2E0_0x5Bresource0x2Ddrop0x5Dnetwork(struct w2c_wasi0x3Asockets0x2Fip0x2Dname0x2Dlookup0x4000x2E20x2E0 *wasi, wasm_resource_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/network@0.2.0::[resource-drop]network(%u)\n", (unsigned int)fd);
}

extern "C" void w2c_wasi0x3Asockets0x2Fip0x2Dname0x2Dlookup0x4000x2E20x2E0_resolve0x2Daddresses(struct w2c_wasi0x3Asockets0x2Fip0x2Dname0x2Dlookup0x4000x2E20x2E0 *wasi, wasm_resource_t network, wasm_ptr_t name, wasm_size_t name_len, wasm_ptr_t result) {
    wasi->check_bounds(name, name_len);
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/ip-name-lookup@0.2.0::resolve-addresses(%u, \"%.*s\")\n", (unsigned int)network, (int)std::min(name_len, (wasm_size_t)INT_MAX), (const char *)wasi->str(name, name_len));

    wasi->check_bounds(result, 8);

    const struct sandbox_str_guard name_guard = wasi->str(name, name_len);

    if (name_len == 9 && !std::strncmp(name_guard, "localhost", 9)) {
        static const uint8_t ipv4[4] = {127, 0, 0, 1};
        static const uint16_t ipv6[8] = {0, 0, 0, 0, 0, 0, 0, 1};

        std::deque<struct ai_stream_entry> deque(2);

        deque[0].is_ipv6 = false;
        std::memcpy(deque[0].inner.ipv4, ipv4, sizeof ipv4);

        deque[1].is_ipv6 = true;
        std::memcpy(deque[1].inner.ipv6, ipv6, sizeof ipv6);

        wasm_resource_t aistream = wasi->allocate_file_descriptor(wasi_fd_type::AISTREAM, new ai_stream {std::move(deque)});
        LOG_PRINTF(RETRO_LOG_DEBUG, "WASI resource created: resolve-address-stream(%u) -> localhost (127.0.0.1, ::1)\n", (unsigned int)aistream);
        wasi->ref<uint8_t>(result) = false;
        wasi->ref<wasm_resource_t>(result + 4) = aistream;
        return;
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 4) = WASI_NETWORK_ERROR_NAME_UNRESOLVABLE;
}

extern "C" void w2c_wasi0x3Asockets0x2Fip0x2Dname0x2Dlookup0x4000x2E20x2E0_0x5Bmethod0x5Dresolve0x2Daddress0x2Dstream0x2Eresolve0x2Dnext0x2Daddress(struct w2c_wasi0x3Asockets0x2Fip0x2Dname0x2Dlookup0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/ip-name-lookup@0.2.0::[method]resolve-address-stream.resolve-next-address(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 22);

    if (fd >= wasi->fdtable.size()) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 2) = WASI_NETWORK_ERROR_INVALID_ARGUMENT;
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
            wasi->ref<uint8_t>(result) = true;
            wasi->ref<uint8_t>(result + 2) = WASI_NETWORK_ERROR_INVALID_ARGUMENT;
            return;

        case wasi_fd_type::AISTREAM:
            {
                std::deque<struct ai_stream_entry> &entries = wasi->fdtable[fd].ai_stream()->entries;
                wasi->ref<uint8_t>(result) = false;
                if (entries.empty()) {
                    wasi->ref<uint8_t>(result + 2) = false;
                } else {
                    wasi->ref<uint8_t>(result + 2) = true;
                    const struct ai_stream_entry &entry = entries.front();
                    wasi->ref<uint8_t>(result + 4) = entry.is_ipv6;
                    if (!entry.is_ipv6) {
                        wasi->arycpy(result + 6, entry.inner.ipv4, 4);
                    } else {
                        wasi->arycpy(result + 6, entry.inner.ipv6, 8);
                    }
                    entries.pop_front();
                }
            }
            return;
    }

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 2) = WASI_NETWORK_ERROR_INVALID_ARGUMENT;
}

extern "C" wasm_resource_t w2c_wasi0x3Asockets0x2Fip0x2Dname0x2Dlookup0x4000x2E20x2E0_0x5Bmethod0x5Dresolve0x2Daddress0x2Dstream0x2Esubscribe(struct w2c_wasi0x3Asockets0x2Fip0x2Dname0x2Dlookup0x4000x2E20x2E0 *wasi, wasm_resource_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/ip-name-lookup@0.2.0::[method]resolve-address-stream.subscribe(%u)\n", (unsigned int)fd);
    LOG_PRINTF(RETRO_LOG_DEBUG, "WASI resource created: pollable(%u) -> resolve address stream %u\n", (unsigned int)fd, (unsigned int)fd);
    return fd;
}

extern "C" void w2c_wasi0x3Asockets0x2Fip0x2Dname0x2Dlookup0x4000x2E20x2E0_0x5Bresource0x2Ddrop0x5Dresolve0x2Daddress0x2Dstream(struct w2c_wasi0x3Asockets0x2Fip0x2Dname0x2Dlookup0x4000x2E20x2E0 *wasi, wasm_resource_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/ip-name-lookup@0.2.0::[resource-drop]resolve-address-stream(%u)\n", (unsigned int)fd);

    if (fd >= wasi->fdtable.size()) {
        return;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSFILE:
        case wasi_fd_type::FSDIRSTREAM:
        case wasi_fd_type::FSFILESTREAM:
            return;

        case wasi_fd_type::AISTREAM:
            wasi->deallocate_file_descriptor(fd);
            return;
    }
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x2Dcreate0x2Dsocket0x4000x2E20x2E0_create0x2Dtcp0x2Dsocket(struct w2c_wasi0x3Asockets0x2Ftcp0x2Dcreate0x2Dsocket0x4000x2E20x2E0 *wasi, uint32_t address_family, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp-create-socket@0.2.0::create-tcp-socket(%u)\n", (unsigned int)address_family);

    wasi->check_bounds(result, 8);

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 4) = WASI_NETWORK_ERROR_NOT_SUPPORTED;
}

/*
 * If the local address is IPv4:
 *   - local_address_is_ipv6 is 0
 *   - local_address_port is the TCP port number the socket is binding to
 *   - local_address_0 is the first byte of the address the socket is binding to (e.g. if the address is 192.168.0.1, then this is 192)
 *   - local_address_1 is the second byte of the address the socket is binding to
 *   - local_address_2 is the third byte of the address the socket is binding to
 *   - local_address_3 is the fourth byte of the address the socket is binding to
 * If the local address is IPv6:
 *   - local_address_is_ipv6 is 1
 *   - local_address_port is the TCP port number the socket is binding to
 *   - local_address_0 is equivalent to the sin6_flowinfo field of the sockaddr_in6 struct in both POSIX sockets and Winsock
 *   - local_address_1 is the first nibble of the address the socket is binding to (e.g. if the address is 2001:0db8:0000:0000:0000:0000:0000:0000, then this is 0x2001 = 8193)
 *   - local_address_2 is the second nibble of the address the socket is binding to
 *   - local_address_3 is the third nibble of the address the socket is binding to
 *   - local_address_4 is the fourth nibble of the address the socket is binding to
 *   - local_address_5 is the fifth nibble of the address the socket is binding to
 *   - local_address_6 is the sixth nibble of the address the socket is binding to
 *   - local_address_7 is the seventh nibble of the address the socket is binding to
 *   - local_address_8 is the eighth nibble of the address the socket is binding to
 *   - local_address_scope_id is equivalent to the sin6_scope_id field of the sockaddr_in6 struct in both POSIX sockets and Winsock
 */
extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Estart0x2Dbind(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_resource_t network, uint32_t local_address_is_ipv6, uint32_t local_address_port, uint32_t local_address_0, uint32_t local_address_1, uint32_t local_address_2, uint32_t local_address_3, uint32_t local_address_4, uint32_t local_address_5, uint32_t local_address_6, uint32_t local_address_7, uint32_t local_address_8, uint32_t local_address_scope_id, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.start-bind(%u, %u, %u, (%u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u))\n", (unsigned int)fd, (unsigned int)network, (unsigned int)local_address_is_ipv6, (unsigned int)local_address_port, (unsigned int)local_address_0, (unsigned int)local_address_1, (unsigned int)local_address_2, (unsigned int)local_address_3, (unsigned int)local_address_4, (unsigned int)local_address_5, (unsigned int)local_address_6, (unsigned int)local_address_7, (unsigned int)local_address_8, (unsigned int)local_address_scope_id);

    wasi->check_bounds(result, 2);

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 1) = WASI_NETWORK_ERROR_INVALID_STATE;
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Efinish0x2Dbind(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.finish-bind(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 2);

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 1) = WASI_NETWORK_ERROR_INVALID_STATE;
}

/*
 * If the remote address is IPv4:
 *   - remote_address_is_ipv6 is 0
 *   - remote_address_port is the TCP port number the socket is connecting to
 *   - remote_address_0 is the first byte of the address the socket is connecting to (e.g. if the address is 192.168.0.1, then this is 192)
 *   - remote_address_1 is the second byte of the address the socket is connecting to
 *   - remote_address_2 is the third byte of the address the socket is connecting to
 *   - remote_address_3 is the fourth byte of the address the socket is connecting to
 * If the remote address is IPv6:
 *   - remote_address_is_ipv6 is 1
 *   - remote_address_port is the TCP port number the socket is connecting to
 *   - remote_address_0 is equivalent to the sin6_flowinfo field of the sockaddr_in6 struct in both POSIX sockets and Winsock
 *   - remote_address_1 is the first nibble of the address the socket is connecting to (e.g. if the address is 2001:0db8:0000:0000:0000:0000:0000:0001, then this is 0x2001 = 8193)
 *   - remote_address_2 is the second nibble of the address the socket is connecting to
 *   - remote_address_3 is the third nibble of the address the socket is connecting to
 *   - remote_address_4 is the fourth nibble of the address the socket is connecting to
 *   - remote_address_5 is the fifth nibble of the address the socket is connecting to
 *   - remote_address_6 is the sixth nibble of the address the socket is connecting to
 *   - remote_address_7 is the seventh nibble of the address the socket is connecting to
 *   - remote_address_8 is the eighth nibble of the address the socket is connecting to
 *   - remote_address_scope_id is equivalent to the sin6_scope_id field of the sockaddr_in6 struct in both POSIX sockets and Winsock
 */
extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Estart0x2Dconnect(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_resource_t network, uint32_t remote_address_is_ipv6, uint32_t remote_address_port, uint32_t remote_address_0, uint32_t remote_address_1, uint32_t remote_address_2, uint32_t remote_address_3, uint32_t remote_address_4, uint32_t remote_address_5, uint32_t remote_address_6, uint32_t remote_address_7, uint32_t remote_address_8, uint32_t remote_address_scope_id, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.start-connect(%u, %u, %u, (%u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u))\n", (unsigned int)fd, (unsigned int)network, (unsigned int)remote_address_is_ipv6, (unsigned int)remote_address_port, (unsigned int)remote_address_0, (unsigned int)remote_address_1, (unsigned int)remote_address_2, (unsigned int)remote_address_3, (unsigned int)remote_address_4, (unsigned int)remote_address_5, (unsigned int)remote_address_6, (unsigned int)remote_address_7, (unsigned int)remote_address_8, (unsigned int)remote_address_scope_id);

    wasi->check_bounds(result, 2);

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 1) = WASI_NETWORK_ERROR_INVALID_STATE;
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Efinish0x2Dconnect(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.finish-connect(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 12);

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 4) = WASI_NETWORK_ERROR_INVALID_STATE;
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Estart0x2Dlisten(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.start-listen(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 2);

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 1) = WASI_NETWORK_ERROR_INVALID_STATE;
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Efinish0x2Dlisten(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.finish-listen(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 2);

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 1) = WASI_NETWORK_ERROR_INVALID_STATE;
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Eaccept(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.accept(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 16);

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 4) = WASI_NETWORK_ERROR_INVALID_STATE;
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Elocal0x2Daddress(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.local-address(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 36);

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 4) = WASI_NETWORK_ERROR_INVALID_STATE;
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Eremote0x2Daddress(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.remote-address(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 36);

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 4) = WASI_NETWORK_ERROR_INVALID_STATE;
}

extern "C" uint32_t w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Eis0x2Dlistening(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.is-listening(%u)\n", (unsigned int)fd);

    return false;
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Eset0x2Dlisten0x2Dbacklog0x2Dsize(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint64_t value, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.set-listen-backlog-size(%u, %llu)\n", (unsigned int)fd, (unsigned long long)value);

    wasi->check_bounds(result, 2);

    if (value == 0) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 1) = WASI_NETWORK_ERROR_INVALID_ARGUMENT;
    } else {
        wasi->ref<uint8_t>(result) = false;
    }
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Ekeep0x2Dalive0x2Denabled(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.keep-alive-enabled(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 2);

    wasi->ref<uint8_t>(result) = false;
    wasi->ref<uint8_t>(result + 1) = false;
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Eset0x2Dkeep0x2Dalive0x2Denabled(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint32_t value, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.set-keep-alive-enabled(%u, %s)\n", (unsigned int)fd, value ? "true" : "false");

    wasi->check_bounds(result, 2);

    wasi->ref<uint8_t>(result) = false;
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Ekeep0x2Dalive0x2Didle0x2Dtime(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.keep-alive-idle-time(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 16);

    wasi->ref<uint8_t>(result) = false;
    wasi->ref<uint64_t>(result + 8) = 7200ULL * 1000000000ULL;
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Ekeep0x2Dalive0x2Dinterval(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.keep-alive-interval(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 16);

    wasi->ref<uint8_t>(result) = false;
    wasi->ref<uint64_t>(result + 8) = 75ULL * 1000000000ULL;
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Eset0x2Dkeep0x2Dalive0x2Dinterval(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint64_t value, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.set-keep-alive-interval(%u, %llu)\n", (unsigned int)fd, (unsigned long long)value);

    wasi->check_bounds(result, 2);

    if (value == 0) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 1) = WASI_NETWORK_ERROR_INVALID_ARGUMENT;
    } else {
        wasi->ref<uint8_t>(result) = false;
    }
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Ekeep0x2Dalive0x2Dcount(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.keep-alive-count(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 8);

    wasi->ref<uint8_t>(result) = false;
    wasi->ref<uint32_t>(result + 4) = 9;
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Eset0x2Dkeep0x2Dalive0x2Dcount(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint32_t value, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.set-keep-alive-count(%u, %u)\n", (unsigned int)fd, (unsigned int)value);

    wasi->check_bounds(result, 2);

    if (value == 0) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 1) = WASI_NETWORK_ERROR_INVALID_ARGUMENT;
    } else {
        wasi->ref<uint8_t>(result) = false;
    }
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Eset0x2Dkeep0x2Dalive0x2Didle0x2Dtime(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint64_t value, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.set-keep-alive-idle-time(%u, %llu)\n", (unsigned int)fd, (unsigned long long)value);

    wasi->check_bounds(result, 2);

    if (value == 0) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 1) = WASI_NETWORK_ERROR_INVALID_ARGUMENT;
    } else {
        wasi->ref<uint8_t>(result) = false;
    }
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Ehop0x2Dlimit(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.hop-limit(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 2);

    wasi->ref<uint8_t>(result) = false;
    wasi->ref<uint8_t>(result + 1) = 64;
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Eset0x2Dhop0x2Dlimit(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint32_t value, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.set-hop-limit(%u, %u)\n", (unsigned int)fd, (unsigned int)value);

    wasi->check_bounds(result, 2);

    if (value == 0) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 1) = WASI_NETWORK_ERROR_INVALID_ARGUMENT;
    } else {
        wasi->ref<uint8_t>(result) = false;
    }
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Ereceive0x2Dbuffer0x2Dsize(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.receive-buffer-size(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 16);

    wasi->ref<uint8_t>(result) = false;
    wasi->ref<uint64_t>(result + 8) = 4096;
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Eset0x2Dreceive0x2Dbuffer0x2Dsize(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint64_t value, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.set-receive-buffer-size(%u, %llu)\n", (unsigned int)fd, (unsigned long long)value);

    wasi->check_bounds(result, 2);

    if (value == 0) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 1) = WASI_NETWORK_ERROR_INVALID_ARGUMENT;
    } else {
        wasi->ref<uint8_t>(result) = false;
    }
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Esend0x2Dbuffer0x2Dsize(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.send-buffer-size(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 16);

    wasi->ref<uint8_t>(result) = false;
    wasi->ref<uint64_t>(result + 8) = 4096;
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Eset0x2Dsend0x2Dbuffer0x2Dsize(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint64_t value, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.set-send-buffer-size(%u, %llu)\n", (unsigned int)fd, (unsigned long long)value);

    wasi->check_bounds(result, 2);

    if (value == 0) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 1) = WASI_NETWORK_ERROR_INVALID_ARGUMENT;
    } else {
        wasi->ref<uint8_t>(result) = false;
    }
}

extern "C" wasm_resource_t w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Esubscribe(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.subscribe(%u)\n", (unsigned int)fd);
    LOG_PRINTF(RETRO_LOG_DEBUG, "WASI resource created: pollable(%u) -> TCP socket %u\n", (unsigned int)fd, (unsigned int)fd);
    return fd;
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bmethod0x5Dtcp0x2Dsocket0x2Eshutdown(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint32_t shutdown_type, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[method]tcp-socket.shutdown(%u, %u)\n", (unsigned int)fd, (unsigned int)shutdown_type);

    wasi->check_bounds(result, 2);

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 1) = WASI_NETWORK_ERROR_INVALID_STATE;
}

extern "C" void w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0_0x5Bresource0x2Ddrop0x5Dtcp0x2Dsocket(struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 *wasi, wasm_resource_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/tcp@0.2.0::[resource-drop]tcp-socket(%u)\n", (unsigned int)fd);
}

extern "C" void w2c_wasi0x3Asockets0x2Fudp0x2Dcreate0x2Dsocket0x4000x2E20x2E0_create0x2Dudp0x2Dsocket(struct w2c_wasi0x3Asockets0x2Fudp0x2Dcreate0x2Dsocket0x4000x2E20x2E0 *wasi, uint32_t address_family, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp-create-socket@0.2.0::create-udp-socket(%u)\n", (unsigned int)address_family);

    wasi->check_bounds(result, 8);

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 4) = WASI_NETWORK_ERROR_NOT_SUPPORTED;
}

/*
 * If the local address is IPv4:
 *   - local_address_is_ipv6 is 0
 *   - local_address_port is the UDP port number the socket is binding to
 *   - local_address_0 is the first byte of the address the socket is binding to (e.g. if the address is 192.168.0.1, then this is 192)
 *   - local_address_1 is the second byte of the address the socket is binding to
 *   - local_address_2 is the third byte of the address the socket is binding to
 *   - local_address_3 is the fourth byte of the address the socket is binding to
 * If the local address is IPv6:
 *   - local_address_is_ipv6 is 1
 *   - local_address_port is the UDP port number the socket is binding to
 *   - local_address_0 is equivalent to the sin6_flowinfo field of the sockaddr_in6 struct in both POSIX sockets and Winsock
 *   - local_address_1 is the first nibble of the address the socket is binding to (e.g. if the address is 2001:0db8:0000:0000:0000:0000:0000:0000, then this is 0x2001 = 8193)
 *   - local_address_2 is the second nibble of the address the socket is binding to
 *   - local_address_3 is the third nibble of the address the socket is binding to
 *   - local_address_4 is the fourth nibble of the address the socket is binding to
 *   - local_address_5 is the fifth nibble of the address the socket is binding to
 *   - local_address_6 is the sixth nibble of the address the socket is binding to
 *   - local_address_7 is the seventh nibble of the address the socket is binding to
 *   - local_address_8 is the eighth nibble of the address the socket is binding to
 *   - local_address_scope_id is equivalent to the sin6_scope_id field of the sockaddr_in6 struct in both POSIX sockets and Winsock
 */
extern "C" void w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0_0x5Bmethod0x5Dudp0x2Dsocket0x2Estart0x2Dbind(struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_resource_t network, uint32_t local_address_is_ipv6, uint32_t local_address_port, uint32_t local_address_0, uint32_t local_address_1, uint32_t local_address_2, uint32_t local_address_3, uint32_t local_address_4, uint32_t local_address_5, uint32_t local_address_6, uint32_t local_address_7, uint32_t local_address_8, uint32_t local_address_scope_id, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp@0.2.0::[method]udp-socket.start-bind(%u, %u, %u, (%u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u))\n", (unsigned int)fd, (unsigned int)network, (unsigned int)local_address_is_ipv6, (unsigned int)local_address_port, (unsigned int)local_address_0, (unsigned int)local_address_1, (unsigned int)local_address_2, (unsigned int)local_address_3, (unsigned int)local_address_4, (unsigned int)local_address_5, (unsigned int)local_address_6, (unsigned int)local_address_7, (unsigned int)local_address_8, (unsigned int)local_address_scope_id);

    wasi->check_bounds(result, 2);

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 1) = WASI_NETWORK_ERROR_INVALID_STATE;
}

extern "C" void w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0_0x5Bmethod0x5Dudp0x2Dsocket0x2Efinish0x2Dbind(struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp@0.2.0::[method]udp-socket.finish-bind(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 2);

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 1) = WASI_NETWORK_ERROR_INVALID_STATE;
}

/*
 * If the remote address is IPv4:
 *   - remote_address_is_ipv6 is 0
 *   - remote_address_port is the UDP port number the socket is connecting to
 *   - remote_address_0 is the first byte of the address the socket is connecting to (e.g. if the address is 192.168.0.1, then this is 192)
 *   - remote_address_1 is the second byte of the address the socket is connecting to
 *   - remote_address_2 is the third byte of the address the socket is connecting to
 *   - remote_address_3 is the fourth byte of the address the socket is connecting to
 * If the remote address is IPv6:
 *   - remote_address_is_ipv6 is 1
 *   - remote_address_port is the UDP port number the socket is connecting to
 *   - remote_address_0 is equivalent to the sin6_flowinfo field of the sockaddr_in6 struct in both POSIX sockets and Winsock
 *   - remote_address_1 is the first nibble of the address the socket is connecting to (e.g. if the address is 2001:0db8:0000:0000:0000:0000:0000:0001, then this is 0x2001 = 8193)
 *   - remote_address_2 is the second nibble of the address the socket is connecting to
 *   - remote_address_3 is the third nibble of the address the socket is connecting to
 *   - remote_address_4 is the fourth nibble of the address the socket is connecting to
 *   - remote_address_5 is the fifth nibble of the address the socket is connecting to
 *   - remote_address_6 is the sixth nibble of the address the socket is connecting to
 *   - remote_address_7 is the seventh nibble of the address the socket is connecting to
 *   - remote_address_8 is the eighth nibble of the address the socket is connecting to
 *   - remote_address_scope_id is equivalent to the sin6_scope_id field of the sockaddr_in6 struct in both POSIX sockets and Winsock
 */
extern "C" void w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0_0x5Bmethod0x5Dudp0x2Dsocket0x2Estream(struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_resource_t network, uint32_t remote_address_is_ipv6, uint32_t remote_address_port, uint32_t remote_address_0, uint32_t remote_address_1, uint32_t remote_address_2, uint32_t remote_address_3, uint32_t remote_address_4, uint32_t remote_address_5, uint32_t remote_address_6, uint32_t remote_address_7, uint32_t remote_address_8, uint32_t remote_address_scope_id, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp@0.2.0::[method]udp-socket.stream(%u, %u, %u, (%u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u))\n", (unsigned int)fd, (unsigned int)network, (unsigned int)remote_address_is_ipv6, (unsigned int)remote_address_port, (unsigned int)remote_address_0, (unsigned int)remote_address_1, (unsigned int)remote_address_2, (unsigned int)remote_address_3, (unsigned int)remote_address_4, (unsigned int)remote_address_5, (unsigned int)remote_address_6, (unsigned int)remote_address_7, (unsigned int)remote_address_8, (unsigned int)remote_address_scope_id);

    wasi->check_bounds(result, 12);

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 4) = WASI_NETWORK_ERROR_NOT_SUPPORTED;
}

extern "C" void w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0_0x5Bmethod0x5Dudp0x2Dsocket0x2Elocal0x2Daddress(struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp@0.2.0::[method]udp-socket.local-address(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 36);

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 4) = WASI_NETWORK_ERROR_INVALID_STATE;
}

extern "C" void w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0_0x5Bmethod0x5Dudp0x2Dsocket0x2Eremote0x2Daddress(struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp@0.2.0::[method]udp-socket.remote-address(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 36);

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 4) = WASI_NETWORK_ERROR_INVALID_STATE;
}

extern "C" void w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0_0x5Bmethod0x5Dudp0x2Dsocket0x2Eunicast0x2Dhop0x2Dlimit(struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp@0.2.0::[method]udp-socket.unicast-hop-limit(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 2);

    wasi->ref<uint8_t>(result) = false;
    wasi->ref<uint8_t>(result + 1) = 64;
}

extern "C" void w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0_0x5Bmethod0x5Dudp0x2Dsocket0x2Eset0x2Dunicast0x2Dhop0x2Dlimit(struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint32_t value, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp@0.2.0::[method]udp-socket.set-unicast-hop-limit(%u, %u)\n", (unsigned int)fd, (unsigned int)value);

    wasi->check_bounds(result, 2);

    if (value == 0) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 1) = WASI_NETWORK_ERROR_INVALID_ARGUMENT;
    } else {
        wasi->ref<uint8_t>(result) = false;
    }
}

extern "C" void w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0_0x5Bmethod0x5Dudp0x2Dsocket0x2Ereceive0x2Dbuffer0x2Dsize(struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp@0.2.0::[method]udp-socket.receive-buffer-size(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 16);

    wasi->ref<uint8_t>(result) = false;
    wasi->ref<uint64_t>(result + 8) = 4096;
}

extern "C" void w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0_0x5Bmethod0x5Dudp0x2Dsocket0x2Eset0x2Dreceive0x2Dbuffer0x2Dsize(struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint64_t value, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp@0.2.0::[method]udp-socket.set-receive-buffer-size(%u, %llu)\n", (unsigned int)fd, (unsigned long long)value);

    wasi->check_bounds(result, 2);

    if (value == 0) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 1) = WASI_NETWORK_ERROR_INVALID_ARGUMENT;
    } else {
        wasi->ref<uint8_t>(result) = false;
    }
}

extern "C" void w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0_0x5Bmethod0x5Dudp0x2Dsocket0x2Esend0x2Dbuffer0x2Dsize(struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp@0.2.0::[method]udp-socket.send-buffer-size(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 16);

    wasi->ref<uint8_t>(result) = false;
    wasi->ref<uint64_t>(result + 8) = 4096;
}

extern "C" void w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0_0x5Bmethod0x5Dudp0x2Dsocket0x2Eset0x2Dsend0x2Dbuffer0x2Dsize(struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint64_t value, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp@0.2.0::[method]udp-socket.set-send-buffer-size(%u, %llu)\n", (unsigned int)fd, (unsigned long long)value);

    wasi->check_bounds(result, 2);

    if (value == 0) {
        wasi->ref<uint8_t>(result) = true;
        wasi->ref<uint8_t>(result + 1) = WASI_NETWORK_ERROR_INVALID_ARGUMENT;
    } else {
        wasi->ref<uint8_t>(result) = false;
    }
}

extern "C" wasm_resource_t w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0_0x5Bmethod0x5Dudp0x2Dsocket0x2Esubscribe(struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 *wasi, wasm_resource_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp@0.2.0::[method]udp-socket.subscribe(%u)\n", (unsigned int)fd);
    LOG_PRINTF(RETRO_LOG_DEBUG, "WASI resource created: pollable(%u) -> TCP socket %u\n", (unsigned int)fd, (unsigned int)fd);
    return fd;
}

extern "C" void w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0_0x5Bresource0x2Ddrop0x5Dudp0x2Dsocket(struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 *wasi, wasm_resource_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp@0.2.0::[resource-drop]udp-socket(%u)\n", (unsigned int)fd);
}

extern "C" void w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0_0x5Bmethod0x5Dincoming0x2Ddatagram0x2Dstream0x2Ereceive(struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, uint64_t max_results, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp@0.2.0::[method]incoming-datagram-stream.receive(%u, %llu)\n", (unsigned int)fd, (unsigned long long)max_results);

    wasi->check_bounds(result, 3 * sizeof(wasm_ptr_t));

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + sizeof(wasm_ptr_t)) = WASI_NETWORK_ERROR_NOT_SUPPORTED;
}

extern "C" wasm_resource_t w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0_0x5Bmethod0x5Dincoming0x2Ddatagram0x2Dstream0x2Esubscribe(struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 *wasi, wasm_resource_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp@0.2.0::[method]incoming-datagram-stream.subscribe(%u)\n", (unsigned int)fd);
    LOG_PRINTF(RETRO_LOG_DEBUG, "WASI resource created: pollable(%u) -> incoming datagram stream %u\n", (unsigned int)fd, (unsigned int)fd);
    return fd;
}

extern "C" void w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0_0x5Bresource0x2Ddrop0x5Dincoming0x2Ddatagram0x2Dstream(struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 *wasi, wasm_resource_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp@0.2.0::[resource-drop]incoming-datagram-stream(%u)", (unsigned int)fd);
}

extern "C" void w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0_0x5Bmethod0x5Doutgoing0x2Ddatagram0x2Dstream0x2Echeck0x2Dsend(struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp@0.2.0::[method]outgoing-datagram-stream.check-send(%u)\n", (unsigned int)fd);

    wasi->check_bounds(result, 16);

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 8) = WASI_NETWORK_ERROR_NOT_SUPPORTED;
}

extern "C" void w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0_0x5Bmethod0x5Doutgoing0x2Ddatagram0x2Dstream0x2Esend(struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 *wasi, wasm_resource_t fd, wasm_ptr_t datagrams, wasm_size_t datagrams_len, wasm_ptr_t result) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp@0.2.0::[method]outgoing-datagram-stream.check-send(%u, 0x%08llx (%llu))\n", (unsigned int)fd, (unsigned long long)datagrams, (unsigned long long)datagrams_len);

    wasi->check_bounds(result, 16);

    wasi->ref<uint8_t>(result) = true;
    wasi->ref<uint8_t>(result + 8) = WASI_NETWORK_ERROR_NOT_SUPPORTED;
}

extern "C" wasm_resource_t w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0_0x5Bmethod0x5Doutgoing0x2Ddatagram0x2Dstream0x2Esubscribe(struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 *wasi, wasm_resource_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp@0.2.0::[method]outgoing-datagram-stream.subscribe(%u)\n", (unsigned int)fd);
    LOG_PRINTF(RETRO_LOG_DEBUG, "WASI resource created: pollable(%u) -> outgoing datagram stream %u\n", (unsigned int)fd, (unsigned int)fd);
    return fd;
}

extern "C" void w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0_0x5Bresource0x2Ddrop0x5Doutgoing0x2Ddatagram0x2Dstream(struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 *wasi, wasm_resource_t fd) {
    LOG_PRINTF(RETRO_LOG_DEBUG, "wasi:sockets/udp@0.2.0::[resource-drop]outgoing-datagram-stream(%u)", (unsigned int)fd);
}

#ifndef MKXPZ_SANDBOX_SERIAL_WASI_H
#define MKXPZ_SANDBOX_SERIAL_WASI_H
#include "sandbox-serial-wasi.h"
#endif // MKXPZ_SANDBOX_SERIAL_WASI_H
