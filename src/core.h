/*
** core.h
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

#ifndef MKXPZ_CORE_H
#define MKXPZ_CORE_H

#include <libretro.h>
#include "sandbox.h"
#include "audio.h"
#include "filesystem.h"
#include "input.h"

extern retro_log_printf_t mkxp_retro_log_printf;

namespace mkxp_retro {
    extern boost::optional<struct mkxp_sandbox::sandbox> sandbox;
    extern boost::optional<Audio> audio;
    extern boost::optional<Input> input;
    extern boost::optional<FileSystem> fs;

    extern retro_video_refresh_t video_refresh;
    extern retro_audio_sample_batch_t audio_sample_batch;
    extern retro_environment_t environment;
    extern retro_input_poll_t input_poll;
    extern retro_input_state_t input_state;
    extern struct retro_hw_render_callback hw_render;
    extern bool keyboard_state[RETROK_LAST];
    extern bool input_polled;
    extern unsigned int sample_rate;

    extern uint8_t ruby_revision[20];

    uint64_t get_ticks_ms() noexcept;
    uint64_t get_ticks_us() noexcept;
    double get_refresh_rate() noexcept;
    bool using_threaded_audio() noexcept;
    void request_resize(unsigned int width, unsigned int height) noexcept;
    void display_message(enum retro_log_level log_level, const char *msg) noexcept;
}

#define _LOG_PRINTF_DETAIL2(x) #x
#define _LOG_PRINTF_DETAIL(x) _LOG_PRINTF_DETAIL2(x)

#define LOG_PRINT(log_level, str) ::mkxp_retro_log_printf(log_level, "[mkxp-z @ " __FILE__ ":" _LOG_PRINTF_DETAIL(__LINE__) "] %s", str)
#define LOG_PRINTF(log_level, fmt, ...) ::mkxp_retro_log_printf(log_level, "[mkxp-z @ " __FILE__ ":" _LOG_PRINTF_DETAIL(__LINE__) "] " fmt, __VA_ARGS__)

#endif // MKXPZ_CORE_H
