/*
** core.cpp
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

#include "core.h"
#include "core-options.h"
#include "binding-sandbox.h"
#include "sandbox-serial-util.h"

#include <atomic>
#include <cctype>
#include <cstdarg>
#include <cstring>
#include <functional>
#include <string>
#include <tuple>
#include <utility>

#include <boost/optional.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <alc.h>
#include <alext.h>
#include <fluidsynth.h>
#include <punycode.h>

#include "mkxp-polyfill.h" // std::mutex, std::stof, std::strtoul, std::to_string
#include "git-hash.h"
#include "binding-sandbox-hash.h"

#include "al-util.h"
#include "audio.h"
#include "encoding.h"
#include "eventthread.h"
#include "filesystem.h"
#include "forced-assert.h"
#include "gl-fun.h"
#include "glstate.h"
#include "graphics.h"
#include "input.h"
#include "sharedmidistate.h"
#include "sharedstate.h"

#define THREADED_AUDIO_SAMPLES (((size_t)sample_rate * (size_t)AUDIO_SLEEP) / (size_t)1000)

#define DEF_SCREEN_W (rgssVer == 1 ? 640 : 544)
#define DEF_SCREEN_H (rgssVer == 1 ? 480 : 416)
#define DEF_FRAMERATE (rgssVer == 1 ? 40 : 60)

using namespace mkxp_retro;
using namespace mkxp_sandbox;

struct lock_guard {
    std::mutex &mutex;

    lock_guard(std::mutex &mutex) : mutex(mutex) {
        mutex.lock();
    }

    lock_guard(const struct lock_guard &guard) = delete;

    lock_guard(struct lock_guard &&guard) noexcept = delete;

    struct lock_guard &operator=(const struct lock_guard &guard) = delete;

    struct lock_guard &operator=(struct lock_guard &&guard) noexcept = delete;

    ~lock_guard() {
        mutex.unlock();
    }
};

template <typename T> struct atomic {
#ifndef MKXPZ_NO_THREADED_AUDIO
    std::atomic<T> atom;
#else
    T atom;
#endif // MKXPZ_NO_THREADED_AUDIO

    atomic() {}

    atomic(T value) : atom(value) {}

    atomic(const struct atomic &guard) = delete;

    atomic(struct atomic &&guard) noexcept = delete;

    struct atomic &operator=(const struct atomic &guard) = delete;

    struct atomic &operator=(struct atomic &&guard) noexcept = delete;

    T load_relaxed() const noexcept {
#ifndef MKXPZ_NO_THREADED_AUDIO
        return atom.load(std::memory_order_relaxed);
#else
        return atom;
#endif // MKXPZ_NO_THREADED_AUDIO
    }

    operator T() const noexcept {
#ifndef MKXPZ_NO_THREADED_AUDIO
        return atom.load(std::memory_order_seq_cst);
#else
        return atom;
#endif // MKXPZ_NO_THREADED_AUDIO
    }

    void operator=(T value) noexcept {
#ifndef MKXPZ_NO_THREADED_AUDIO
        atom.store(value, std::memory_order_seq_cst);
#else
        atom = value;
#endif // MKXPZ_NO_THREADED_AUDIO
    }

    void operator+=(T value) noexcept {
#ifndef MKXPZ_NO_THREADED_AUDIO
        atom.fetch_add(value, std::memory_order_seq_cst);
#else
        atom += value;
#endif // MKXPZ_NO_THREADED_AUDIO
    }
};

#if !defined(MKXPZ_NO_THREADED_AUDIO) && defined(MKXPZ_NO_STD_ATOMIC_UINT64_T)
template <> struct atomic<uint64_t> {
    mutable std::mutex mutex;
    uint64_t atom;

    atomic() {}

    atomic(uint64_t value) : atom(value) {}

    atomic(const struct atomic &guard) = delete;

    atomic(struct atomic &&guard) noexcept = delete;

    struct atomic &operator=(const struct atomic &guard) = delete;

    struct atomic &operator=(struct atomic &&guard) noexcept = delete;

    uint64_t load_relaxed() const noexcept {
        return atom;
    }

    operator uint64_t() const noexcept {
        struct lock_guard guard(mutex);
        return atom;
    }

    void operator=(uint64_t value) noexcept {
        struct lock_guard guard(mutex);
        atom = value;
    }

    void operator+=(uint64_t value) noexcept {
        struct lock_guard guard(mutex);
        atom += value;
    }
};
#endif // !defined(MKXPZ_NO_THREADED_AUDIO) && defined(MKXPZ_NO_STD_ATOMIC_UINT64_T)

int mkxp_physfs_allow_duplicates = false;

struct physfs_allow_duplicates_guard {
    bool old_value;

    physfs_allow_duplicates_guard() : old_value(mkxp_physfs_allow_duplicates) {
        mkxp_physfs_allow_duplicates = true;
    }

    physfs_allow_duplicates_guard(const struct physfs_allow_duplicates_guard &guard) = delete;

    physfs_allow_duplicates_guard(struct physfs_allow_duplicates_guard &&guard) noexcept = delete;

    struct physfs_allow_duplicates_guard &operator=(const struct physfs_allow_duplicates_guard &guard) = delete;

    struct physfs_allow_duplicates_guard &operator=(struct physfs_allow_duplicates_guard &&guard) noexcept = delete;

    ~physfs_allow_duplicates_guard() {
        mkxp_physfs_allow_duplicates = old_value;
    }
};

static uint64_t frame_count;
static struct atomic<uint64_t> frame_time;
static uint64_t frame_time_remainder;
static uint64_t retro_run_count;

extern const uint8_t dist_zip[];
extern const size_t dist_zip_len;
extern const uint8_t preload_zip[];
extern const size_t preload_zip_len;

static ALCdevice *al_device = nullptr;
static ALCcontext *al_context = nullptr;
static LPALCRENDERSAMPLESSOFT alcRenderSamplesSOFT = nullptr;
static LPALCLOOPBACKOPENDEVICESOFT alcLoopbackOpenDeviceSOFT = nullptr;
static int16_t *sound_buf = nullptr;
static bool retro_framebuffer_supported;
static bool dupe_supported;
static size_t save_state_size = 0;
static retro_system_av_info av_info;
static struct retro_audio_callback audio_callback;
static struct retro_frame_time_callback frame_time_callback = {
    [](retro_usec_t delta) {
        frame_time += delta;
        frame_time_remainder += delta;
    },
};
static std::mutex threaded_audio_mutex;
static bool threaded_audio_enabled = false;
static bool frame_time_callback_enabled = false;
static struct atomic<bool> shared_state_initialized(false);

static unsigned int screen_width;
static unsigned int screen_height;

struct retro_vfs_interface_info mkxp_vfs;

static unsigned int message_interface_version;

static bool texture_sync_is_eager = false;

static bool message_displayed_recently = false;

retro_log_printf_t mkxp_retro_log_printf;

namespace mkxp_retro {
    retro_video_refresh_t video_refresh;
    retro_audio_sample_batch_t audio_sample_batch;
    retro_environment_t environment;
    retro_input_poll_t input_poll;
    retro_input_state_t input_state;
    struct retro_hw_render_callback hw_render;
    bool keyboard_state[RETROK_LAST];
    bool input_polled;
    unsigned int sample_rate;

    uint8_t ruby_revision[20];

    uint64_t get_ticks_ms() noexcept {
        return frame_time / 1000;
    }

    uint64_t get_ticks_us() noexcept {
        return frame_time;
    }

    double get_refresh_rate() noexcept {
        return av_info.timing.fps;
    }

    bool using_threaded_audio() noexcept {
        return threaded_audio_enabled;
    }

    void request_resize(unsigned int width, unsigned int height) noexcept {
        if (width == av_info.geometry.base_width && height == av_info.geometry.base_height) {
            return;
        }
        av_info.geometry.max_width = av_info.geometry.base_width = width;
        av_info.geometry.max_height = av_info.geometry.base_height = height;
        if ((float)width / (float)height != av_info.geometry.aspect_ratio) {
            av_info.geometry.aspect_ratio = (float)width / (float)height;
            environment(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av_info);
        } else if (!environment(RETRO_ENVIRONMENT_SET_GEOMETRY, &av_info.geometry)) {
            environment(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av_info);
        }
    }

    void display_message(enum retro_log_level log_level, const char *msg) noexcept {
        message_displayed_recently = true;
        switch (message_interface_version) {
            default:
                {
                    const struct retro_message_ext message {
                        msg,
                        8000,
                        log_level == RETRO_LOG_ERROR ? 3U : log_level == RETRO_LOG_WARN ? 2U : log_level == RETRO_LOG_INFO ? 1U : 0U,
                        log_level,
                        RETRO_MESSAGE_TARGET_OSD,
                        RETRO_MESSAGE_TYPE_NOTIFICATION,
                        -1,
                    };
                    environment(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, (void *)&message);
                }
                break;

            case 0:
                {
                    const struct retro_message message {
                        msg,
                        480,
                    };
                    environment(RETRO_ENVIRONMENT_SET_MESSAGE, (void *)&message);
                }
                break;
        }
    }
}

static void fallback_log(enum retro_log_level level, const char *fmt, ...) {
    std::va_list va;
    va_start(va, fmt);
    std::vfprintf(stderr, fmt, va);
    va_end(va);
}

static void fluid_log(int level, const char *message, void *data) {
    switch (level) {
        case FLUID_PANIC:
            LOG_PRINTF(RETRO_LOG_ERROR, "fluidsynth: panic: %s\n", message);
            break;
        case FLUID_ERR:
            LOG_PRINTF(RETRO_LOG_ERROR, "fluidsynth: error: %s\n", message);
            break;
        case FLUID_WARN:
            LOG_PRINTF(RETRO_LOG_WARN, "fluidsynth: warning: %s\n", message);
            break;
        case FLUID_INFO:
            LOG_PRINTF(RETRO_LOG_INFO, "fluidsynth: %s\n", message);
            break;
        case FLUID_DBG:
            LOG_PRINTF(RETRO_LOG_DEBUG, "fluidsynth: debug: %s\n", message);
            break;
    }
}

static uint32_t *frame_buf;
boost::optional<struct sandbox> mkxp_retro::sandbox;
boost::optional<Audio> mkxp_retro::audio;
boost::optional<Input> mkxp_retro::input;
boost::optional<FileSystem> mkxp_retro::fs;
static boost::optional<Config> conf;
static boost::optional<RGSSThreadData> thread_data;
static std::string game_path;

static void audio_render(size_t samples) {
    audio->render();
    alcRenderSamplesSOFT(al_device, sound_buf, samples);
    for (size_t n = 0; n < samples;) {
        n += audio_sample_batch(sound_buf + 2 * n, samples - n);
    }
}

static const char *get_core_option(const char *key) {
    struct retro_variable variable = {
        key,
        "",
    };
    return environment(RETRO_ENVIRONMENT_GET_VARIABLE, &variable) && variable.value != nullptr ? variable.value : "";
}

struct init : boost::asio::coroutine {
    typedef decl_slots<VALUE> slots;

    static VALUE func(VALUE arg) {
        struct coro : boost::asio::coroutine {
            VALUE operator()() {
                BOOST_ASIO_CORO_REENTER (this) {
                    SANDBOX_AWAIT(sandbox_binding_init);
                }

                return SANDBOX_TRUE;
            }
        };

        return sb()->bind<struct coro>()()();
    }

    static VALUE rescue(VALUE arg, VALUE exception) {
        struct coro : boost::asio::coroutine {
            VALUE operator()(VALUE exception) {
                BOOST_ASIO_CORO_REENTER (this) {
                    SANDBOX_AWAIT(log_backtrace, exception);
                }

                return SANDBOX_FALSE;
            }
        };

        return sb()->bind<struct coro>()()(exception);
    }

    bool operator()() {
        BOOST_ASIO_CORO_REENTER (this) {
            SANDBOX_AWAIT_S(0, rb_rescue2, func, SANDBOX_NIL, rescue, SANDBOX_NIL, sb()->rb_eException(), 0);
            return SANDBOX_VALUE_TO_BOOL(SANDBOX_SLOT(0));
        }

        return false;
    }
};

struct main : boost::asio::coroutine {
    typedef decl_slots<VALUE> slots;

    static VALUE func(VALUE arg) {
        struct coro : boost::asio::coroutine {
            VALUE operator()() {
                BOOST_ASIO_CORO_REENTER (this) {
                    SANDBOX_AWAIT(sandbox_run_rmxp_scripts);
                }

                return SANDBOX_TRUE;
            }
        };

        return sb()->bind<struct coro>()()();
    }

    static VALUE rescue(VALUE arg, VALUE exception) {
        struct coro : boost::asio::coroutine {
            typedef decl_slots<VALUE> slots;

            VALUE operator()(VALUE exception) {
                BOOST_ASIO_CORO_REENTER (this) {
                    // Ignore SystemExit exceptions
                    SANDBOX_AWAIT_S(0, rb_obj_is_kind_of, exception, sb()->rb_eSystemExit());
                    if (SANDBOX_VALUE_TO_BOOL(SANDBOX_SLOT(0))) {
                        return SANDBOX_TRUE;
                    }

                    SANDBOX_AWAIT(log_backtrace, exception);
                }

                return SANDBOX_FALSE;
            }
        };

        return sb()->bind<struct coro>()()(exception);
    }

    bool operator()() {
        BOOST_ASIO_CORO_REENTER (this) {
            SANDBOX_AWAIT_S(0, rb_rescue2, func, SANDBOX_NIL, rescue, SANDBOX_NIL, sb()->rb_eException(), 0);
            return SANDBOX_VALUE_TO_BOOL(SANDBOX_SLOT(0));
        }

        return false;
    }
};

static void deinit_sandbox() {
    bool shared_state_was_initialized = shared_state_initialized.load_relaxed();
    shared_state_initialized = false;
    struct lock_guard guard(threaded_audio_mutex); // Wait for the audio thread to stop rendering audio

    if (sound_buf != nullptr) {
        mkxp_aligned_free(sound_buf, 16);
        sound_buf = nullptr;
    }

    mkxp_retro::sandbox.reset();

    thread_data.reset();

    input.reset();

    audio.reset();
    if (al_context != nullptr) {
        alcDestroyContext(al_context);
        al_context = nullptr;
    }
    if (al_device != nullptr) {
        alcCloseDevice(al_device);
        al_device = nullptr;
    }

    if (shared_state_was_initialized) {
        SharedState::finiInstance();
    }

    conf.reset();

    fs.reset();
}

static bool init_shared_state() {
    Exception e;
    SharedState::initInstance(e, &thread_data.get());
    if (e.is_error()) {
        LOG_PRINTF(RETRO_LOG_ERROR, "Error initializing shared state: %s\n", e.what());
        display_message(RETRO_LOG_ERROR, (std::string("Failed to initialize the mkxp-z game engine: Error initializing shared state: ") + e.what()).c_str());
        deinit_sandbox();
        return false;
    } else {
        shared_state_initialized = true;
        return true;
    }
}

static void update_simple_core_options() {
    {
        const char *value = get_core_option("mkxp-z_subImageFix");
        if (!std::strcmp(value, "default")) {
#ifdef __DEVKITA64__
            conf->subImageFix.setOverride(true);
#else
            conf->subImageFix.setOverride(hw_render.context_type == RETRO_HW_CONTEXT_OPENGLES2 || hw_render.context_type == RETRO_HW_CONTEXT_OPENGLES3 || hw_render.context_type == RETRO_HW_CONTEXT_OPENGLES_VERSION);
#endif // __DEVKITA64__
        } else if (!std::strcmp(value, "enabled")) {
            conf->subImageFix.setOverride(true);
        } else if (!std::strcmp(value, "disabled")) {
            conf->subImageFix.setOverride(false);
        } else {
            conf->subImageFix.clearOverride();
        }
    }

    {
        const char *value = get_core_option("mkxp-z_enableBlitting");
        if (!std::strcmp(value, "default")) {
#ifdef _WIN32
            conf->enableBlitting.setOverride(false);
#else
            conf->enableBlitting.setOverride(true);
#endif // _WIN32
        } else if (!std::strcmp(value, "enabled")) {
            conf->enableBlitting.setOverride(true);
        } else if (!std::strcmp(value, "disabled")) {
            conf->enableBlitting.setOverride(false);
        } else {
            conf->enableBlitting.clearOverride();
        }
    }

    {
        const char *value = get_core_option("mkxp-z_textureSync");
        texture_sync_is_eager = !std::strcmp(value, "eager");
    }

    {
        float value_num = std::stof(get_core_option("mkxp-z_fontScale"));
        if (std::isnormal(value_num) && value_num > 0.0f) {
            conf->fontScale.setOverride(value_num);
        } else {
            conf->fontScale.clearOverride();
        }
    }

    {
        const char *value = get_core_option("mkxp-z_fontKerning");
        if (!std::strcmp(value, "default")) {
            conf->fontKerning.setOverride(true);
        } else if (!std::strcmp(value, "enabled")) {
            conf->fontKerning.setOverride(true);
        } else if (!std::strcmp(value, "disabled")) {
            conf->fontKerning.setOverride(false);
        } else {
            conf->fontKerning.clearOverride();
        }
    }

    {
        const char *value = get_core_option("mkxp-z_fontHinting");
        if (!std::strcmp(value, "inherit")) {
            conf->fontHinting.clearOverride();
        } else if (!std::strcmp(value, "default")) {
            conf->fontHinting.setOverride(3);
        } else {
            unsigned long value_num = std::strtoul(value, nullptr, 10);
            if (value_num >= 0 && value_num <= 3) {
                conf->fontHinting.setOverride(value_num);
            } else {
                conf->fontHinting.clearOverride();
            }
        }
    }

    {
        const char *value = get_core_option("mkxp-z_fontHeightReporting");
        if (!std::strcmp(value, "inherit")) {
            conf->fontHeightReporting.clearOverride();
        } else if (!std::strcmp(value, "default")) {
            conf->fontHeightReporting.setOverride(0);
        } else {
            unsigned long value_num = std::strtoul(value, nullptr, 10);
            if (value_num >= 0 && value_num <= 1) {
                conf->fontHeightReporting.setOverride(value_num);
            } else {
                conf->fontHeightReporting.clearOverride();
            }
        }
    }

    {
        const char *value = get_core_option("mkxp-z_fontOutlineCrop");
        if (!std::strcmp(value, "default")) {
            conf->fontOutlineCrop.setOverride(true);
        } else if (!std::strcmp(value, "enabled")) {
            conf->fontOutlineCrop.setOverride(true);
        } else if (!std::strcmp(value, "disabled")) {
            conf->fontOutlineCrop.setOverride(false);
        } else {
            conf->fontOutlineCrop.clearOverride();
        }
    }

    {
        const char *value = get_core_option("mkxp-z_loadFontsIntoMemory");
        conf->loadFontsIntoMemory =
#ifdef __EMSCRIPTEN__
            std::strcmp(value, "disabled");
#else
            !std::strcmp(value, "enabled");
#endif // __EMSCRIPTEN__
    }

    for (size_t i = 0; i < NUM_BUTTONCODES; ++i) {
        if (mkxpDefaultButtonMapping[i].type == DEFAULT) {
            LOG_PRINTF(RETRO_LOG_DEBUG, "Skipping nonexistent button code %llu\n", (unsigned long long)i);
            continue;
        }
        const char *value = get_core_option((std::string("mkxp-z_button-") + std::to_string(i)).c_str());
        unsigned char id, port;
        if (std::strcmp(value, "none") == 0) {
            mkxpButtonMapping[i] = {NONE};
        } else if (std::sscanf(value, "joypad-%hhu-%hhu\n", &id, &port) == 2) {
            mkxpButtonMapping[i] = {JOYPAD, id, port};
        } else if (std::sscanf(value, "lightgun-%hhu-%hhu\n", &id, &port) == 2) {
            mkxpButtonMapping[i] = {LIGHTGUN, id, port};
        } else if (std::sscanf(value, "mouse-%hhu-%hhu\n", &id, &port) == 2) {
            mkxpButtonMapping[i] = {MOUSE, id, port};
        } else {
            mkxpButtonMapping[i] = {};
        }
    }

    for (size_t i = 0; i < NUM_SCANCODES; ++i) {
        if (mkxpDefaultScancodeMapping[i].type == DEFAULT) {
            LOG_PRINTF(RETRO_LOG_DEBUG, "Skipping nonexistent scancode %llu\n", (unsigned long long)i);
            continue;
        }
        const char *value = get_core_option((std::string("mkxp-z_scancode-") + std::to_string(i)).c_str());
        unsigned char id, port;
        if (std::strcmp(value, "none") == 0) {
            mkxpScancodeMapping[i] = {NONE};
        } else if (std::sscanf(value, "button-%hhu\n", &id) == 1) {
            mkxpScancodeMapping[i] = {BUTTON, id};
        } else if (std::sscanf(value, "joypad-%hhu-%hhu\n", &id, &port) == 2) {
            mkxpScancodeMapping[i] = {JOYPAD, id, port};
        } else if (std::sscanf(value, "lightgun-%hhu-%hhu\n", &id, &port) == 2) {
            mkxpScancodeMapping[i] = {LIGHTGUN, id, port};
        } else if (std::sscanf(value, "mouse-%hhu-%hhu\n", &id, &port) == 2) {
            mkxpScancodeMapping[i] = {MOUSE, id, port};
        } else {
            mkxpScancodeMapping[i] = {};
        }
    }

    for (size_t i = 0; i < NUM_CONTROLLER_BUTTONS; ++i) {
        if (mkxpDefaultControllerMapping[i].type == DEFAULT) {
            LOG_PRINTF(RETRO_LOG_DEBUG, "Skipping nonexistent controller button %llu\n", (unsigned long long)i);
            continue;
        }
        const char *value = get_core_option((std::string("mkxp-z_controller-") + std::to_string(i)).c_str());
        unsigned char id, port;
        if (std::strcmp(value, "none") == 0) {
            mkxpControllerMapping[i] = {NONE};
        } else if (std::sscanf(value, "button-%hhu\n", &id) == 1) {
            mkxpControllerMapping[i] = {BUTTON, id};
        } else if (std::sscanf(value, "joypad-%hhu-%hhu\n", &id, &port) == 2) {
            mkxpControllerMapping[i] = {JOYPAD, id, port};
        } else if (std::sscanf(value, "lightgun-%hhu-%hhu\n", &id, &port) == 2) {
            mkxpControllerMapping[i] = {LIGHTGUN, id, port};
        } else if (std::sscanf(value, "mouse-%hhu-%hhu\n", &id, &port) == 2) {
            mkxpControllerMapping[i] = {MOUSE, id, port};
        } else {
            mkxpControllerMapping[i] = {};
        }
    }
}

static bool script_is_enabled_by_default(const char *script_name, bool is_postload) {
    return !is_postload
        && (
            std::strcmp(script_name, "kgl2_wrap.rb") == 0
                || std::strcmp(script_name, "win32_wrap.rb") == 0
        );
}

static std::string get_script_core_option_name(const char *script_name, bool is_postload) {
    static const char hex[] = "0123456789abcdef";
    std::string option_name(is_postload ? "mkxp-z_postload-" : "mkxp-z_preload-");
    for (;; ++script_name) {
        char c = *script_name;
        if (c == 0) {
            break;
        }
        option_name.push_back(hex[c / 16]);
        option_name.push_back(hex[c % 16]);
    }
    return option_name;
}

static void set_script_core_option_definition(std::vector<std::string> &key_buffer, retro_core_option_v2_definition &definition, const char *script_name, bool is_postload) {
    key_buffer.push_back(get_script_core_option_name(script_name, is_postload));
    std::string &key = key_buffer.back();
    definition.key = key.c_str();
    definition.desc = script_name;
    definition.desc_categorized = nullptr;
    definition.info = nullptr;
    definition.info_categorized = nullptr;
    definition.category_key = is_postload ? "postload" : "preload";
    definition.values[0] = {"default", script_is_enabled_by_default(script_name, is_postload) ? "Default (Enabled)" : "Default (Disabled)"};
    definition.values[1] = {"enabled", "Enabled"};
    definition.values[2] = {"disabled", "Disabled"};
    definition.values[3] = {nullptr, nullptr};
    definition.default_value = "default";
}

static void set_core_options(Config &config, std::vector<std::string> &preload_scripts, std::vector<std::string> &postload_scripts) {
    constexpr size_t num_core_option_definitions = sizeof core_option_definitions / sizeof *core_option_definitions;
    size_t num_core_option_definitions_with_scripts = num_core_option_definitions + preload_scripts.size() + postload_scripts.size();

    std::vector<struct retro_core_option_v2_definition> core_option_definitions_with_scripts(num_core_option_definitions_with_scripts);
    std::memcpy(core_option_definitions_with_scripts.data(), core_option_definitions, sizeof core_option_definitions);
    std::memcpy(&core_option_definitions_with_scripts.back(), &core_option_definitions[num_core_option_definitions - 1], sizeof *core_option_definitions);

    // Fill out core options for preload and postload scripts
    std::vector<std::string> key_buffer;
    key_buffer.reserve(preload_scripts.size() + postload_scripts.size());
    {
        std::vector<struct retro_core_option_v2_definition>::iterator it = core_option_definitions_with_scripts.begin() + (num_core_option_definitions - 1);
        for (const std::string &script_name : preload_scripts) {
            set_script_core_option_definition(key_buffer, *it++, script_name.c_str(), false);
        }
        for (const std::string &script_name : postload_scripts) {
            set_script_core_option_definition(key_buffer, *it++, script_name.c_str(), true);
        }
    }

    // Convert the core options to the libretro frontend's maximum supported core options version
    unsigned int core_options_version;
    if (!environment(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &core_options_version)) {
        core_options_version = 0;
    }
    switch (core_options_version) {
        default:
            {
                const struct retro_core_options_v2 core_options = {
                    (struct retro_core_option_v2_category *)core_option_categories,
                    core_option_definitions_with_scripts.data(),
                };
                if (environment(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, (void *)&core_options)) {
                    break;
                }
            }

        case 1:
            {
                std::vector<struct retro_core_option_definition> core_options(num_core_option_definitions_with_scripts);
                for (size_t i = 0; i < num_core_option_definitions_with_scripts; ++i) {
                    core_options[i].key = core_option_definitions[i].key;
                    core_options[i].desc = core_option_definitions[i].desc;
                    core_options[i].info = core_option_definitions[i].info;
                    size_t num_values = 0;
                    for (const struct retro_core_option_value *value = core_option_definitions[i].values; value->value != nullptr; ++value) {
                        ++num_values;
                    }
                    std::memcpy(core_options[i].values, core_option_definitions[i].values, (1 + num_values) * sizeof *core_option_definitions[i].values);
                    core_options[i].default_value = core_option_definitions[i].default_value;
                }
                if (environment(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, core_options.data())) {
                    break;
                }
            }

        case 0:
            {
                std::vector<struct retro_variable> core_options(num_core_option_definitions_with_scripts);
                std::vector<std::string> values(num_core_option_definitions_with_scripts);
                size_t i;
                for (i = 0; i < num_core_option_definitions_with_scripts - 1; ++i) {
                    core_options[i].key = core_option_definitions[i].key;
                    size_t values_length = 0;
                    for (const struct retro_core_option_value *value = core_option_definitions[i].values; value->value != nullptr; ++value) {
                        values_length += 1 + std::strlen(value->value);
                    }
                    values[i].reserve(std::strlen(core_option_definitions[i].desc) + 1 + values_length);
                    values[i] = core_option_definitions[i].desc;
                    values[i].append("; ");
                    for (const struct retro_core_option_value *value = core_option_definitions[i].values; value->value != nullptr; ++value) {
                        if (std::strcmp(value->value, core_option_definitions[i].default_value)) {
                            continue;
                        }
                        values[i].append(value->value);
                        break;
                    }
                    for (const struct retro_core_option_value *value = core_option_definitions[i].values; value->value != nullptr; ++value) {
                        if (!std::strcmp(value->value, core_option_definitions[i].default_value)) {
                            continue;
                        }
                        values[i].push_back('|');
                        values[i].append(value->value);
                    }
                    core_options[i].value = values[i].c_str();
                }
                core_options[i].key = nullptr;
                core_options[i].value = nullptr;
                environment(RETRO_ENVIRONMENT_SET_VARIABLES, core_options.data());
            }
    }

    save_state_size = (size_t)std::strtoul(get_core_option("mkxp-z_saveStateSize"), nullptr, 10) * (size_t)0x100000;
    if (save_state_size == 0) {
        save_state_size = (size_t)(100 * 0x100000);
    }
    save_state_size = std::max(save_state_size, (size_t)(64 * 0x100000));

    config.editor.debug = std::strcmp(get_core_option("mkxp-z_debug"), "enabled") == 0;
    config.editor.battleTest = std::strcmp(get_core_option("mkxp-z_battleTest"), "enabled") == 0;

    // Prepend the preload scripts enabled via core options to the list of preload scripts
    std::vector<std::string> enabled_preload_scripts;
    for (const std::string &script_name : preload_scripts) {
        const char *value = get_core_option(get_script_core_option_name(script_name.c_str(), false).c_str());
        if (script_is_enabled_by_default(script_name.c_str(), false) ? std::strcmp(value, "disabled") : !std::strcmp(value, "enabled")) {
            enabled_preload_scripts.emplace_back(std::string("/System/Scripts/Preload/") + script_name);
        }
    }
    enabled_preload_scripts.insert(enabled_preload_scripts.end(), std::make_move_iterator(config.preloadScripts.begin()), std::make_move_iterator(config.preloadScripts.end()));
    config.preloadScripts = std::move(enabled_preload_scripts);

    // Append the postload scripts enabled via core options to the list of postload scripts
    for (const std::string &script_name : postload_scripts) {
        const char *value = get_core_option(get_script_core_option_name(script_name.c_str(), false).c_str());
        if (script_is_enabled_by_default(script_name.c_str(), true) ? std::strcmp(value, "disabled") : !std::strcmp(value, "enabled")) {
            config.postloadScripts.emplace_back(std::string("/System/Scripts/Postload/") + script_name);
        }
    }
}

static bool init_sandbox() {
    deinit_sandbox();

    fs.emplace(nullptr, false);

    std::string system_path;
    std::vector<std::string> preload_scripts;
    std::vector<std::string> postload_scripts;

    // Mount /Dist
    PHYSFS_mountMemory(dist_zip, dist_zip_len, nullptr, "/dist.zip", "/Dist", true);

    // Mount /System
    {
        const char *path;
        if (environment(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &path) && path != nullptr) {
            system_path = path;
#ifdef _WIN32
            for (size_t i = 0; i < system_path.length(); ++i) {
                if (system_path[i] == '\\') {
                    system_path[i] = '/';
                }
            }
#endif // _WIN32
            PHYSFS_setWriteDir(system_path.c_str());

            // Create the "/mkxp-z" subdirectory of the libretro system directory if it doesn't already exist
            std::string system_path_subdir(system_path);
            system_path_subdir.append("/mkxp-z");
            if (!PHYSFS_mkdir(system_path_subdir.c_str() + system_path.length() + 1)) {
                LOG_PRINTF(RETRO_LOG_ERROR, "Failed to create directory at \"%s\": %s\n", system_path_subdir.c_str(), PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
                display_message(RETRO_LOG_ERROR, (std::string("Failed to initialize the mkxp-z game engine: Failed to create directory at \"") + system_path_subdir + "\": " + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())).c_str());
                deinit_sandbox();
                return false;
            }

            system_path = system_path_subdir;
            PHYSFS_setWriteDir(system_path.c_str());

            // Create the Preload directory if needed
            std::string preload_path(system_path);
            preload_path.append("/Scripts/Preload");
            if (!PHYSFS_mkdir(preload_path.c_str() + system_path.length() + 1)) {
                LOG_PRINTF(RETRO_LOG_ERROR, "Failed to create directory at \"%s\": %s\n", preload_path.c_str(), PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
                display_message(RETRO_LOG_ERROR, (std::string("Failed to initialize the mkxp-z game engine: Failed to create directory at \"") + preload_path + "\": " + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())).c_str());
                deinit_sandbox();
                return false;
            }

            // Create the Postload directory if needed
            std::string postload_path(system_path);
            postload_path.append("/Scripts/Postload");
            if (!PHYSFS_mkdir(postload_path.c_str() + system_path.length() + 1)) {
                LOG_PRINTF(RETRO_LOG_ERROR, "Failed to create directory at \"%s\": %s\n", postload_path.c_str(), PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
                display_message(RETRO_LOG_ERROR, (std::string("Failed to initialize the mkxp-z game engine: Failed to create directory at \"") + postload_path + "\": " + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())).c_str());
                deinit_sandbox();
                return false;
            }

            PHYSFS_mount(system_path.c_str(), "/System", true);
            PHYSFS_mountMemory(preload_zip, preload_zip_len, nullptr, "/preload.zip", "/System/Scripts/Preload", true);

            // Get the list of preload scripts
            {
                std::set<std::string> script_set;
                PHYSFS_enumerate("/System/Scripts/Preload", [](void *data, const char *origdir, const char *fname) {
                    if (fname[0] == '.') {
                        // Exclude dotfiles
                        return PHYSFS_ENUM_OK;
                    }
                    {
                        PHYSFS_Stat stat;
                        PHYSFS_stat((std::string(origdir) + "/" + fname).c_str(), &stat);
                        if (stat.filetype != PHYSFS_FILETYPE_REGULAR) {
                            // Exclude everything that's not a regular file
                            return PHYSFS_ENUM_OK;
                        }
                    }
                    ((std::set<std::string> *)data)->emplace(fname);
                    return PHYSFS_ENUM_OK;
                }, &script_set);
                preload_scripts.insert(preload_scripts.end(), std::make_move_iterator(script_set.begin()), std::make_move_iterator(script_set.end()));
            }

            // Get the list of postload scripts
            {
                std::set<std::string> script_set;
                PHYSFS_enumerate("/System/Scripts/Postload", [](void *data, const char *origdir, const char *fname) {
                    if (fname[0] == '.') {
                        // Exclude dotfiles
                        return PHYSFS_ENUM_OK;
                    }
                    {
                        PHYSFS_Stat stat;
                        PHYSFS_stat((std::string(origdir) + "/" + fname).c_str(), &stat);
                        if (stat.filetype != PHYSFS_FILETYPE_REGULAR) {
                            // Exclude everything that's not a regular file
                            return PHYSFS_ENUM_OK;
                        }
                    }
                    ((std::set<std::string> *)data)->emplace(fname);
                    return PHYSFS_ENUM_OK;
                }, &script_set);
                postload_scripts.insert(postload_scripts.end(), std::make_move_iterator(script_set.begin()), std::make_move_iterator(script_set.end()));
            }
        }
    }

    {
        std::string parsed_game_path(game_path);
        std::string parsed_game_path_lower(game_path);
        for (char &c : parsed_game_path_lower) {
            c = std::tolower(c);
        }

        // If the game path doesn't end with ".mkxpz", ".zip" or ".7z", remove the last component from the path since we want to mount the directory that the file is in, not the file itself.
        if (
            !(parsed_game_path_lower.length() >= sizeof ".mkxpz" - 1 && std::strcmp(parsed_game_path_lower.c_str() + (parsed_game_path_lower.length() - (sizeof ".mkxpz" - 1)), ".mkxpz") == 0)
                && !(parsed_game_path_lower.length() >= sizeof ".zip" - 1 && std::strcmp(parsed_game_path_lower.c_str() + (parsed_game_path_lower.length() - (sizeof ".zip" - 1)), ".zip") == 0)
                && !(parsed_game_path_lower.length() >= sizeof ".7z" - 1 && std::strcmp(parsed_game_path_lower.c_str() + (parsed_game_path_lower.length() - (sizeof ".7z" - 1)), ".7z") == 0)
        ) {
            size_t last_slash_index = parsed_game_path.find_last_of('/');
#ifdef _WIN32
            size_t last_backslash_index = parsed_game_path.find_last_of('\\');
            if (last_backslash_index != std::string::npos) {
                last_slash_index = last_slash_index == std::string::npos ? last_backslash_index : std::max(last_slash_index, last_backslash_index);
            }
#endif // _WIN32
            if (last_slash_index == std::string::npos) {
                last_slash_index = 0;
            }
            parsed_game_path = parsed_game_path.substr(0, last_slash_index);
        }

        if (!PHYSFS_mount(parsed_game_path.c_str(), "/Game", false)) {
            const char *what = PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());
            LOG_PRINTF(RETRO_LOG_ERROR, "%s\n", what);
            display_message(RETRO_LOG_ERROR, (std::string("Failed to initialize the mkxp-z game engine: ") + what).c_str());
            deinit_sandbox();
            return false;
        }

        conf.emplace();
        set_core_options(*conf, preload_scripts, postload_scripts);

        {
            const char *value = get_core_option("mkxp-z_rgssVersion");
            if (!std::strcmp(value, "default")) {
                conf->read(0, nullptr, 0);
            } else {
                unsigned long value_num = std::strtoul(value, nullptr, 10);
                if (value_num == 1 || value_num == 2 || value_num == 3) {
                    conf->read(0, nullptr, value_num);
                } else {
                    conf->read(0, nullptr);
                }
            }
        }

        {
            const char *value = get_core_option("mkxp-z_frameSkip");
            if (!std::strcmp(value, "enabled")) {
                conf->frameSkip.setOverride(true);
            } else if (!std::strcmp(value, "disabled")) {
                conf->frameSkip.setOverride(false);
            } else {
                conf->frameSkip.clearOverride();
            }
        }

        {
            const char *value = get_core_option("mkxp-z_midiChorus");
            if (!std::strcmp(value, "enabled")) {
                conf->midi.chorus.setOverride(true);
            } else if (!std::strcmp(value, "disabled")) {
                conf->midi.chorus.setOverride(false);
            } else {
                conf->midi.chorus.clearOverride();
            }
        }

        {
            const char *value = get_core_option("mkxp-z_midiReverb");
            if (!std::strcmp(value, "enabled")) {
                conf->midi.reverb.setOverride(true);
            } else if (!std::strcmp(value, "disabled")) {
                conf->midi.reverb.setOverride(false);
            } else {
                conf->midi.reverb.clearOverride();
            }
        }

        {
            unsigned long value_num = std::strtoul(get_core_option("mkxp-z_SESourceCount"), nullptr, 10);
            if (value_num >= 6 && value_num <= 64) {
                conf->SE.sourceCount.setOverride(value_num);
            } else {
                conf->SE.sourceCount.clearOverride();
            }
        }

        update_simple_core_options();

        SharedState::rgssVersion = conf->rgssVersion;
        thread_data.emplace(nullptr, nullptr, nullptr, nullptr, 60, 1, *conf);

        PHYSFS_File *rgssad;
        if ((rgssad = PHYSFS_openRead(("/Game/" + conf->execName + ".rgssad").c_str())) != nullptr) {
            PHYSFS_mountHandle(rgssad, ('/' + conf->execName + ".rgssad").c_str(), "/Game", false);
        } else if ((rgssad = PHYSFS_openRead(("/Game/" + conf->execName + ".rgss2a").c_str())) != nullptr) {
            PHYSFS_mountHandle(rgssad, ('/' + conf->execName + ".rgss2a").c_str(), "/Game", false);
        } else if ((rgssad = PHYSFS_openRead(("/Game/" + conf->execName + ".rgss3a").c_str())) != nullptr) {
            PHYSFS_mountHandle(rgssad, ('/' + conf->execName + ".rgss3a").c_str(), "/Game", false);
        }
    }

    if (!system_path.empty()) {
        std::string rtp_root_path(system_path);
        rtp_root_path.append("/RTP");

        // Create the RTP root directory if needed
        if (!PHYSFS_mkdir(rtp_root_path.c_str() + system_path.length() + 1)) {
            LOG_PRINTF(RETRO_LOG_ERROR, "Failed to create directory at \"%s\": %s\n", rtp_root_path.c_str(), PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
            display_message(RETRO_LOG_ERROR, (std::string("Failed to initialize the mkxp-z game engine: Failed to create directory at \"") + rtp_root_path + "\": " + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())).c_str());
            deinit_sandbox();
            return false;
        }

        // Mount each RTP declared in mkxp.json to the game directory
        for (const std::string &rtp : conf->rtps) {
            std::string path(fs->normalize(rtp.c_str(), false, true, "/System/RTP"));

            if (path != "/System" && std::strncmp(path.c_str(), "/System/", sizeof "/System/" - 1)) {
                LOG_PRINTF(RETRO_LOG_WARN, "Failed to mount RTP \"%s\" because mounting RTPs from outside of the libretro system directory is not supported\n", rtp.c_str());
                display_message(RETRO_LOG_WARN, (std::string("Failed to locate run time package \"") + rtp + "\" required by the game").c_str());
                continue;
            }

            std::string rtp_path(system_path.c_str());
            rtp_path.push_back('/');
            rtp_path.append(path.c_str() + sizeof "/System/" - 1);

            // Check if this is a file or directory
            PHYSFS_Stat stat;
            if (!PHYSFS_stat(path.c_str(), &stat) || (stat.filetype != PHYSFS_FILETYPE_DIRECTORY && stat.filetype != PHYSFS_FILETYPE_REGULAR)) {
                goto fail;
            }

            if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
                // If it's a directory, just mount the path directly
                if (!PHYSFS_mount(rtp_path.c_str(), "/Game", true)) {
                    goto fail;
                }
            } else {
                // If it's a file, try to open it as an archive and then mount it
                PHYSFS_File *file = PHYSFS_openRead(path.c_str());
                if (file == nullptr) {
                    goto fail;
                }
                if (!PHYSFS_mountHandle(file, path.c_str(), "/Game", true)) {
                    PHYSFS_close(file);
                    goto fail;
                }
            }

            LOG_PRINTF(RETRO_LOG_INFO, "Mounted RTP \"%s\" from \"%s\"\n", rtp.c_str(), rtp_path.c_str());
            continue;

        fail:
            LOG_PRINTF(RETRO_LOG_WARN, "Failed to mount RTP \"%s\" because \"%s\" was not found\n", rtp.c_str(), rtp_path.c_str());
            display_message(RETRO_LOG_WARN, (std::string("Failed to locate run time package \"") + rtp + "\" required by the game").c_str());
            continue;
        }

        // Mount each RTP declared in Game.ini to the game directory
        for (const std::string &rtp : conf->game.rtps) {
            struct data {
                std::string rtp_root_path;
                std::string rtp;
                std::string rtp_lowercase;
                bool found;
            } data = {
                rtp_root_path,
                rtp,
                rtp,
                false,
            };
            for (char &c : data.rtp_lowercase) {
                c = std::tolower(c);
            }

            PHYSFS_enumerate("/System/RTP", [](void *data_, const char *origdir, const char *fname) {
                struct data &data = *(struct data *)data_;
                std::string rtp(fname);
                for (char &c : rtp) {
                    c = std::tolower(c);
                }

                // Make sure this file/directory has a filename that matches the one we're looking for (case-insensitive)
                if (std::strncmp(rtp.c_str(), data.rtp_lowercase.c_str(), data.rtp_lowercase.length()) || (rtp[data.rtp_lowercase.length()] != '.' && rtp[data.rtp_lowercase.length()] != 0)) {
                    return PHYSFS_ENUM_OK;
                }

                // Check if this is a file or directory
                std::string fullpath(origdir);
                fullpath.push_back('/');
                fullpath.append(fname);
                PHYSFS_Stat stat;
                if (!PHYSFS_stat(fullpath.c_str(), &stat) || (stat.filetype != PHYSFS_FILETYPE_DIRECTORY && stat.filetype != PHYSFS_FILETYPE_REGULAR)) {
                    return PHYSFS_ENUM_OK;
                }

                std::string rtp_path(data.rtp_root_path);
                rtp_path.push_back('/');
                rtp_path.append(fname);

                if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
                    // If it's a directory, just mount the path directly
                    if (!PHYSFS_mount(rtp_path.c_str(), "/Game", true)) {
                        return PHYSFS_ENUM_OK;
                    }
                } else {
                    // If it's a file, try to open it as an archive and then mount it
                    std::string path(origdir);
                    path.push_back('/');
                    path.append(fname);
                    PHYSFS_File *file = PHYSFS_openRead(path.c_str());
                    if (file == nullptr) {
                        return PHYSFS_ENUM_OK;
                    }
                    if (!PHYSFS_mountHandle(file, path.c_str(), "/Game", true)) {
                        PHYSFS_close(file);
                        return PHYSFS_ENUM_OK;
                    }
                }

                data.found = true;
                LOG_PRINTF(RETRO_LOG_INFO, "Mounted RTP \"%s\" from \"%s\"\n", data.rtp.c_str(), rtp_path.c_str());
                return PHYSFS_ENUM_STOP;
            }, &data);

            if (!data.found) {
                LOG_PRINTF(RETRO_LOG_WARN, "Failed to mount RTP \"%s\" because \"%s/%s\" was not found\n", rtp.c_str(), rtp_root_path.c_str(), rtp.c_str());
                display_message(RETRO_LOG_WARN, (std::string("Failed to locate run time package \"") + rtp + "\" required by the game").c_str());
            }
        }

        std::string fonts_path(system_path);
        fonts_path.append("/Fonts");

        // Create the Fonts directory if needed
        if (!PHYSFS_mkdir(fonts_path.c_str() + system_path.length() + 1)) {
            LOG_PRINTF(RETRO_LOG_ERROR, "Failed to create directory at \"%s\": %s\n", fonts_path.c_str(), PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
            display_message(RETRO_LOG_ERROR, (std::string("Failed to initialize the mkxp-z game engine: Failed to create directory at \"") + fonts_path + "\": " + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())).c_str());
            deinit_sandbox();
            return false;
        }

        // Mount the Fonts directory
        PHYSFS_mount(fonts_path.c_str(), "/Game/Fonts", true);
    }

    {
        const char *save_path;
        if (environment(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_path) && save_path != nullptr) {
            // Save to the subdirectory of the save directory corresponding to the game's name set in Game.ini
            std::string save_path_subdir(save_path);
#ifdef _WIN32
            for (size_t i = 0; i < save_path_subdir.length(); ++i) {
                if (save_path_subdir[i] == '\\') {
                    save_path_subdir[i] = '/';
                }
            }
#endif // _WIN32
            PHYSFS_setWriteDir(save_path_subdir.c_str());

            {
                std::string game_title;
                if (!conf->windowTitle.empty()) {
                    game_title = conf->windowTitle;
                } else if (!conf->game.title.empty()) {
                    game_title = conf->game.title;
                } else {
                    game_title = "Game";
                }

                game_title = Encoding::convertStringToUtf32(game_title);
                if (game_title.empty()) {
                    game_title = Encoding::convertStringToUtf32("Game");
                }
                assert(!game_title.empty() && game_title.length() % 4 == 0);

                std::vector<uint32_t> input(game_title.length() / 4);
                std::memcpy(input.data(), game_title.c_str(), game_title.length());

                // Sanitize forbidden characters in the game title
                for (uint32_t &c : input) {
                    if (c < 32 || c == '/' || c == '\\' || c == '*' || c == '?' || c == '|') {
                        c = '_';
                    } else if (c == '"') {
                        c = '\'';
                    } else if (c == ':') {
                        c = ';';
                    } else if (c == '<') {
                        c = '(';
                    } else if (c == '>') {
                        c = ')';
                    }
                }

                // Convert to punycode
                size_t output_length = input.size() * 4;
                std::vector<char> output(output_length);
                for (;;) {
                    int result = punycode_encode(input.size(), input.data(), nullptr, &output_length, output.data());
                    if (result == PUNYCODE_SUCCESS) {
                        break;
                    }
                    MKXPZ_FORCED_ASSERT(result == PUNYCODE_BIG_OUTPUT);
                    if (output_length * 2 < output_length) {
                        MKXPZ_THROW(std::bad_alloc());
                    }
                    output_length *= 2;
                    output.resize(output_length);
                }

                save_path_subdir.append("/mkxp-z/Saves/");
                save_path_subdir.append(output.data(), output_length);
            }

            // Create the subdirectory if needed
            if (!PHYSFS_mkdir(save_path_subdir.c_str() + std::strlen(save_path) + 1)) {
                LOG_PRINTF(RETRO_LOG_ERROR, "Failed to create directory at \"%s\": %s\n", save_path_subdir.c_str(), PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
                display_message(RETRO_LOG_ERROR, (std::string("Failed to initialize the mkxp-z game engine: Failed to create directory at \"") + save_path_subdir + "\": " + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())).c_str());
                deinit_sandbox();
                return false;
            }

            // Mount the subdirectory
            PHYSFS_setWriteDir(save_path_subdir.c_str());
            Exception exception(Exception::Ok, "");
            if (!PHYSFS_mount(save_path_subdir.c_str(), "/Save", false)) {
                const char *what = PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());
                LOG_PRINTF(RETRO_LOG_ERROR, "Failed to mount game save directory from \"%s\": %s\n", save_path_subdir.c_str(), what);
                display_message(RETRO_LOG_ERROR, (std::string("Failed to initialize the mkxp-z game engine: Failed to mount game save directory from \"") + save_path_subdir + "\": " + what).c_str());
                deinit_sandbox();
                return false;
            }
            {
                // PhysFS won't normally allow us to mount the save directory in two locations at once,
                // so we temporarily disable the duplicate detection here
                struct physfs_allow_duplicates_guard guard;
                if (!PHYSFS_mount(save_path_subdir.c_str(), "/Game", false)) {
                    const char *what = PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());
                    LOG_PRINTF(RETRO_LOG_ERROR, "Failed to mount game save directory from \"%s\": %s\n", save_path_subdir.c_str(), what);
                    display_message(RETRO_LOG_ERROR, (std::string("Failed to initialize the mkxp-z game engine: Failed to mount game save directory from \"") + save_path_subdir + "\": " + what).c_str());
                    deinit_sandbox();
                    return false;
                }
            }

            LOG_PRINTF(RETRO_LOG_INFO, "Mounted game save directory from \"%s\"\n", save_path_subdir.c_str());
        }
    }

    if (conf->pathCache) {
        fs->createPathCache();
    }

    {
        float refresh_rate;
        if (environment(RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE, &refresh_rate) && refresh_rate > 0.0f) {
            av_info.timing.fps = refresh_rate;
        } else {
            av_info.timing.fps = 60;
        }
    }

    if (!environment(RETRO_ENVIRONMENT_GET_TARGET_SAMPLE_RATE, &sample_rate) || sample_rate == 0) {
        sample_rate = SYNTH_SAMPLERATE;
    }

    alcLoopbackOpenDeviceSOFT = (LPALCLOOPBACKOPENDEVICESOFT)alcGetProcAddress(nullptr, "alcLoopbackOpenDeviceSOFT");
    if (alcLoopbackOpenDeviceSOFT == nullptr) {
        LOG_PRINT(RETRO_LOG_ERROR, "OpenAL implementation does not support `alcLoopbackOpenDeviceSOFT`\n");
        display_message(RETRO_LOG_ERROR, "Failed to initialize the mkxp-z game engine: OpenAL implementation does not support `alcLoopbackOpenDeviceSOFT`");
        deinit_sandbox();
        return false;
    }

    alcRenderSamplesSOFT = (LPALCRENDERSAMPLESSOFT)alcGetProcAddress(nullptr, "alcRenderSamplesSOFT");
    if (alcRenderSamplesSOFT == nullptr) {
        LOG_PRINT(RETRO_LOG_ERROR, "OpenAL implementation does not support `alcRenderSamplesSOFT`\n");
        display_message(RETRO_LOG_ERROR, "Failed to initialize the mkxp-z game engine: OpenAL implementation does not support `alcRenderSamplesSOFT`");
        deinit_sandbox();
        return false;
    }

    al_device = alcLoopbackOpenDeviceSOFT(nullptr);
    if (al_device == nullptr) {
        LOG_PRINT(RETRO_LOG_ERROR, "Failed to initialize OpenAL loopback device\n");
        display_message(RETRO_LOG_ERROR, "Failed to initialize the mkxp-z game engine: Failed to initialize OpenAL loopback device");
        deinit_sandbox();
        return false;
    }

    const ALCint al_attrs[] = {
        ALC_FORMAT_CHANNELS_SOFT,
        ALC_STEREO_SOFT,
        ALC_FORMAT_TYPE_SOFT,
        ALC_SHORT_SOFT,
        ALC_FREQUENCY,
        (ALCint)sample_rate,
        0,
    };
    al_context = alcCreateContext(al_device, al_attrs);
    if (al_context == nullptr || alcMakeContextCurrent(al_context) == AL_FALSE) {
        LOG_PRINT(RETRO_LOG_ERROR, "Failed to create OpenAL context\n");
        display_message(RETRO_LOG_ERROR, "Failed to initialize the mkxp-z game engine: Failed to create OpenAL context");
        deinit_sandbox();
        return false;
    }

    fluid_set_log_function(FLUID_PANIC, fluid_log, nullptr);
    fluid_set_log_function(FLUID_ERR, fluid_log, nullptr);
    fluid_set_log_function(FLUID_WARN, fluid_log, nullptr);
    fluid_set_log_function(FLUID_INFO, fluid_log, nullptr);
    fluid_set_log_function(FLUID_DBG, fluid_log, nullptr);

    audio.emplace(*thread_data);

    input.emplace();

    mkxp_retro::sandbox.emplace(*conf);
    Font::initDefaultDynAttribs();

    av_info.geometry.base_width = screen_width = conf->enableHires ? (int)lround(conf->framebufferScalingFactor * DEF_SCREEN_W) : DEF_SCREEN_W;
    av_info.geometry.base_height = screen_height = conf->enableHires ? (int)lround(conf->framebufferScalingFactor * DEF_SCREEN_H) : DEF_SCREEN_H;
    av_info.geometry.max_width = av_info.geometry.base_width;
    av_info.geometry.max_height = av_info.geometry.base_height;
    av_info.geometry.aspect_ratio = (float)av_info.geometry.base_width / (float)av_info.geometry.base_height;
    av_info.timing.sample_rate = sample_rate;
    frame_time_callback.reference = 1000000 / DEF_FRAMERATE;
    frame_time_callback_enabled = environment(RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK, &frame_time_callback);

    sound_buf = (int16_t *)mkxp_aligned_malloc(16, (threaded_audio_enabled ? THREADED_AUDIO_SAMPLES : (size_t)std::ceil(av_info.timing.sample_rate / av_info.timing.fps)) * 2 * sizeof(int16_t));
    if (sound_buf == nullptr) {
        MKXPZ_THROW(std::bad_alloc());
    }

    frame_count = 0;
    frame_time = 0;
    frame_time_remainder = 0;
    retro_run_count = 0;

    while (!sb().run<struct init>().has_value());

    return true;
}

extern "C" RETRO_API void retro_set_environment(retro_environment_t cb) {
    environment = cb;
}

extern "C" RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb) {
    video_refresh = cb;
}

extern "C" RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb) {

}

extern "C" RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
    audio_sample_batch = cb;
}

extern "C" RETRO_API void retro_set_input_poll(retro_input_poll_t cb) {
    input_poll = cb;
}

extern "C" RETRO_API void retro_set_input_state(retro_input_state_t cb) {
    input_state = cb;
}

extern "C" RETRO_API void retro_init() {
    {
        struct retro_log_callback log;
        if (environment(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log)) {
            mkxp_retro_log_printf = log.log;
        } else {
            mkxp_retro_log_printf = fallback_log;
        }
    }

    LOG_PRINT(RETRO_LOG_INFO, "mkxp-z version " MKXPZ_VERSION "/" MKXPZ_GIT_HASH "\n");

    if (!environment(RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION, &message_interface_version)) {
        message_interface_version = 0;
    }

    mkxp_vfs.required_interface_version = 3;
    if (!environment(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &mkxp_vfs)) {
        mkxp_vfs.required_interface_version = 0;
        mkxp_vfs.iface = nullptr;
    }

    static const struct retro_keyboard_callback keyboard = {
        [](bool down, unsigned int keycode, uint32_t character, uint16_t key_modifiers) {
            if (keycode < RETROK_LAST) {
                keyboard_state[keycode] = down;
            }
        }
    };
    std::memset(keyboard_state, 0, sizeof keyboard_state);
    environment(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, (void *)&keyboard);
}

extern "C" RETRO_API void retro_deinit() {

}

extern "C" RETRO_API unsigned int retro_api_version() {
    return RETRO_API_VERSION;
}

extern "C" RETRO_API void retro_get_system_info(struct retro_system_info *info) {
    std::memset(info, 0, sizeof *info);
    info->library_name = "mkxp-z";
    info->library_version = MKXPZ_VERSION "/" MKXPZ_GIT_HASH;
    info->valid_extensions = "ini|json|rxproj|rvproj|rvproj2|mkxpz|zip|7z";
    info->need_fullpath = true;
    info->block_extract = true;
}

extern "C" RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info) {
    *info = av_info;
}

extern "C" RETRO_API void retro_set_controller_port_device(unsigned int port, unsigned int device) {

}

extern "C" RETRO_API void retro_reset() {
    init_sandbox();
}

extern "C" RETRO_API void retro_run() {
    // We need to wait until the graphics context is initialized before initializing shared state,
    // which is why we initialize it here instead of in `retro_load_game()`
    if (mkxp_retro::sandbox.has_value() && !shared_state_initialized.load_relaxed()) {
        init_shared_state();
    }
    assert(mkxp_retro::sandbox.has_value() == shared_state_initialized.load_relaxed());

    bool should_render = mkxp_retro::sandbox.has_value() && (frame_count == 0 || frame_time_remainder >= (uint64_t)frame_time_callback.reference);

    if (should_render) {
        frame_time_remainder %= (uint64_t)frame_time_callback.reference;
    }

    if (!frame_time_callback_enabled) {
        uint64_t reference = 1000000 / av_info.timing.fps;
        frame_time += reference;
        frame_time_remainder += reference;
    }

    input_polled = false;
    message_displayed_recently = false;

    if (hw_render.context_type != RETRO_HW_CONTEXT_NONE && (should_render || (!dupe_supported && mkxp_retro::sandbox.has_value()))) {
        glState.refresh();
    }

    {
        bool core_options_updated;
        if (mkxp_retro::sandbox.has_value() && environment(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &core_options_updated) && core_options_updated) {
            {
                const char *value = get_core_option("mkxp-z_frameSkip");
                if (!std::strcmp(value, "enabled")) {
                    conf->frameSkip.setOverride(true);
                } else if (!std::strcmp(value, "disabled")) {
                    conf->frameSkip.setOverride(false);
                } else {
                    conf->frameSkip.clearOverride();
                }
                if (conf->frameSkip != shState->graphics().getFrameskip()) {
                    shState->graphics().setFrameskip(conf->frameSkip);
                }
            }

            {
                const char *value = get_core_option("mkxp-z_midiChorus");
                if (!std::strcmp(value, "enabled")) {
                    conf->midi.chorus.setOverride(true);
                } else if (!std::strcmp(value, "disabled")) {
                    conf->midi.chorus.setOverride(false);
                } else {
                    conf->midi.chorus.clearOverride();
                }
                if (shState->midiState().inited) {
                    fluid.settings_setint(shState->midiState().flSettings, "synth.chorus.active", conf->midi.chorus);
                }
            }

            {
                const char *value = get_core_option("mkxp-z_midiReverb");
                if (!std::strcmp(value, "enabled")) {
                    conf->midi.reverb.setOverride(true);
                } else if (!std::strcmp(value, "disabled")) {
                    conf->midi.reverb.setOverride(false);
                } else {
                    conf->midi.reverb.clearOverride();
                }
                if (shState->midiState().inited) {
                    fluid.settings_setint(shState->midiState().flSettings, "synth.reverb.active", conf->midi.reverb);
                }
            }

            update_simple_core_options();
        }
    }

    if (should_render) {
        boost::optional<bool> result = sb().run<struct main>();
        if (texture_sync_is_eager) {
            Bitmap::syncDiffs();
        }
        if (result.has_value()) {
            if (*result) {
                LOG_PRINT(RETRO_LOG_INFO, "Game exited; terminating\n");
            } else {
                LOG_PRINT(RETRO_LOG_ERROR, "Game threw an exception; terminating\n");
            }
            // Heuristic: If the sandbox terminates, we send a shutdown signal to the frontend (which causes the frontend to quit to the menu),
            // but only if the game has been running for sufficiently long, and the game exited normally without raising an exception, and
            // the game did not show a message box in the same frame when it exited.
            // This is to try to make it so that a shutdown signal is only sent if the player deliberately exited a game, and not when
            // the game encountered an error.
            // Some games may catch errors and display them as a message box before exiting normally, which is why the message box heuristic is needed.
            if (frame_count >= 128 && *result && !message_displayed_recently) {
                environment(RETRO_ENVIRONMENT_SHUTDOWN, nullptr);
            }
            deinit_sandbox();
            should_render = false;
        }
    } else if (!dupe_supported && mkxp_retro::sandbox.has_value()) {
        shState->graphics().repaint(sb().transitioning);
    }

    bool movie_dupe_frame = should_render && mkxp_retro::sandbox->get_movie_from_main_thread() != nullptr && Graphics::getMovieDupeFrame(mkxp_retro::sandbox->get_movie_from_main_thread());

    if (!dupe_supported && movie_dupe_frame) {
        shState->graphics().repaint(sb().transitioning);
    }

    // We need to call `input_poll()` at least once every time `retro_run()` is called
    if (!input_polled) {
        input_poll();
    }

    void *fb;
    if (dupe_supported && (!should_render || movie_dupe_frame)) {
        fb = nullptr;
    } else if (hw_render.context_type != RETRO_HW_CONTEXT_NONE) {
        gl.UseProgram(0);
        gl.ActiveTexture(GL_TEXTURE0);
        gl.BindTexture(GL_TEXTURE_2D, 0);
        if (gl.BindVertexArray != nullptr) {
            gl.BindVertexArray(0);
        }
        gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
        gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        gl.BindBuffer(GL_ARRAY_BUFFER, 0);
        fb = RETRO_HW_FRAME_BUFFER_VALID;
    } else if (!retro_framebuffer_supported) {
        fb = frame_buf;
    } else {
        struct retro_framebuffer retro_framebuffer;
        if (environment(RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER, &retro_framebuffer) && retro_framebuffer.format == RETRO_PIXEL_FORMAT_XRGB8888) {
            fb = retro_framebuffer.data;
        } else {
            retro_framebuffer_supported = false;
            fb = frame_buf;
        }
    }
    if (mkxp_retro::sandbox.has_value()) {
        screen_width = shState->graphics().displayContentWidth();
        screen_height = shState->graphics().displayContentHeight();
    }
    video_refresh(fb, screen_width, screen_height, screen_width * 4);

    if (!threaded_audio_enabled && mkxp_retro::sandbox.has_value()) {
        audio_render((uint64_t)std::ceil((double)((uint64_t)sample_rate * (retro_run_count + 1)) / av_info.timing.fps) - (uint64_t)std::ceil((double)((uint64_t)sample_rate * retro_run_count) / av_info.timing.fps));
    }

    if (mkxp_retro::sandbox.has_value()) {
        retro_usec_t new_reference = 1000000 / (sb().get_movie_from_main_thread() != nullptr ? av_info.timing.fps : shState->graphics().getFrameRate());
        if (new_reference != frame_time_callback.reference) {
            frame_time_callback.reference = new_reference;
            frame_time_callback_enabled = environment(RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK, &frame_time_callback);
        }
    }

    if (should_render) {
        ++frame_count;
    }
    ++retro_run_count;
}

extern "C" RETRO_API size_t retro_serialize_size() {
#ifdef MKXPZ_RETRO_NO_SAVE_STATES
    return 0;
#else
    return save_state_size;
#endif // MKXPZ_RETRO_NO_SAVE_STATES
}

extern "C" RETRO_API void retro_cheat_reset() {

}

extern "C" RETRO_API void retro_cheat_set(unsigned int index, bool enabled, const char *code) {
    if (!enabled || !mkxp_retro::sandbox.has_value()) {
        return;
    }
    if ((wasm_size_t)sb().cheats.size() + 1 > (wasm_size_t)sb().cheats.size()) {
        sb().cheats.emplace_back(index, code);
    }
}

extern "C" RETRO_API bool retro_load_game(const struct retro_game_info *info) {
    if (info == nullptr || info->path == nullptr) {
        LOG_PRINT(RETRO_LOG_ERROR, "This core cannot start without a game\n");
        return false;
    }
    game_path = info->path;

    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    if (!environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
        LOG_PRINT(RETRO_LOG_ERROR, "XRGB8888 is not supported\n");
        return false;
    }

    std::memset(&hw_render, 0, sizeof hw_render);
    hw_render.context_reset = []() {
        Exception e;
        initGLFunctions(e);
        if (e.is_error()) {
            LOG_PRINTF(RETRO_LOG_ERROR, "%s\n", e.what());
            display_message(RETRO_LOG_ERROR, (std::string("Failed to initialize the mkxp-z game engine: ") + e.what()).c_str());
            deinit_sandbox();
        } else if (shared_state_initialized.load_relaxed()) {
            glState.refresh();
            shState->sandbox_reinit();
            shState->graphics().sandbox_reinit();
            for (const auto &object : sb()->objects) {
                if (object.typenum > 0) {
                    MKXPZ_FORCED_ASSERT(object.typenum <= SANDBOX_NUM_TYPENUMS);
                    typenum_table[object.typenum - 1].reinit(object.ptr);
                }
            }
        }
    };
    hw_render.context_destroy = []() {
        Bitmap::syncDiffs();
    };
    hw_render.cache_context = false;
    hw_render.bottom_left_origin = false;
    if (false) {
#ifndef __DEVKITA64__ // The Nintendo Switch currently only supports up to OpenGL 4.3 but RetroArch incorrectly thinks it supports OpenGL 4.6
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 4, hw_render.version_minor = 6, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        LOG_PRINT(RETRO_LOG_INFO, "Using OpenGL 4.6 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 4, hw_render.version_minor = 5, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        LOG_PRINT(RETRO_LOG_INFO, "Using OpenGL 4.5 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 4, hw_render.version_minor = 4, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        LOG_PRINT(RETRO_LOG_INFO, "Using OpenGL 4.4 graphics driver\n");
#endif // __DEVKITA64__
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 4, hw_render.version_minor = 3, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        LOG_PRINT(RETRO_LOG_INFO, "Using OpenGL 4.3 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 4, hw_render.version_minor = 2, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        LOG_PRINT(RETRO_LOG_INFO, "Using OpenGL 4.2 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 4, hw_render.version_minor = 1, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        LOG_PRINT(RETRO_LOG_INFO, "Using OpenGL 4.1 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 4, hw_render.version_minor = 0, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        LOG_PRINT(RETRO_LOG_INFO, "Using OpenGL 4.0 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 3, hw_render.version_minor = 3, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        LOG_PRINT(RETRO_LOG_INFO, "Using OpenGL 3.3 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 3, hw_render.version_minor = 2, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        LOG_PRINT(RETRO_LOG_INFO, "Using OpenGL 3.2 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGLES_VERSION, hw_render.version_major = 3, hw_render.version_minor = 2, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        LOG_PRINT(RETRO_LOG_INFO, "Using OpenGL ES 3.2 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 3, hw_render.version_minor = 1, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        LOG_PRINT(RETRO_LOG_INFO, "Using OpenGL 3.1 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGLES_VERSION, hw_render.version_major = 3, hw_render.version_minor = 1, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        LOG_PRINT(RETRO_LOG_INFO, "Using OpenGL ES 3.1 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 3, hw_render.version_minor = 0, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        LOG_PRINT(RETRO_LOG_INFO, "Using OpenGL 3.0 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGLES3, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        LOG_PRINT(RETRO_LOG_INFO, "Using OpenGL ES 3.x graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        LOG_PRINT(RETRO_LOG_INFO, "Using OpenGL 2.x graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGLES2, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        LOG_PRINT(RETRO_LOG_INFO, "Using OpenGL ES 2.0 graphics driver\n");
    } else {
        // TODO: Support software rendering again
        //LOG_PRINT(RETRO_LOG_WARN, "Hardware-accelerated graphics not supported; falling back to software rendering\n");
        //hw_render.context_type = RETRO_HW_CONTEXT_NONE;
        //environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render);
        LOG_PRINT(RETRO_LOG_ERROR, "Error: Hardware-accelerated graphics not supported\n");
        return false;
    }

    {
        bool value;
        dupe_supported = environment(RETRO_ENVIRONMENT_GET_CAN_DUPE, &value) && value;
    }

    retro_framebuffer_supported = true;

    if (!init_sandbox()) {
        return false;
    }

#ifndef MKXPZ_NO_THREADED_AUDIO
    audio_callback.callback = []() {
        if (!shared_state_initialized) {
            return;
        }

        struct lock_guard guard(threaded_audio_mutex);

        if (!shared_state_initialized) {
            return;
        }

        audio_render(THREADED_AUDIO_SAMPLES);
    };
    audio_callback.set_state = nullptr;
    bool threaded_audio_allowed;
    {
        const char *value = get_core_option("mkxp-z_threadedAudio");
        if (!std::strcmp(value, "disabled")) {
            threaded_audio_allowed = false;
        } else {
            threaded_audio_allowed = true;
        }
    }
    if (threaded_audio_allowed) {
        threaded_audio_enabled = environment(RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK, &audio_callback);
        LOG_PRINT(RETRO_LOG_INFO, threaded_audio_enabled ? "Using threaded audio driver\n" : "Not using threaded audio driver because the frontend does not support it\n");
    } else {
        threaded_audio_enabled = false;
        LOG_PRINT(RETRO_LOG_INFO, "Not using threaded audio driver because threaded audio is disabled in the core options\n");
    }
#else
    LOG_PRINT(RETRO_LOG_INFO, "Not using threaded audio driver because multithreading is not supported on this platform\n");
#endif // MKXPZ_NO_THREADED_AUDIO

    return true;
}

extern "C" RETRO_API bool retro_load_game_special(unsigned int type, const struct retro_game_info *info, size_t num) {
    return false;
}

extern "C" RETRO_API void retro_unload_game() {
    deinit_sandbox();
}

extern "C" RETRO_API unsigned int retro_get_region() {
    return RETRO_REGION_NTSC;
}

extern "C" RETRO_API void *retro_get_memory_data(unsigned int id) {
#ifdef MKXPZ_BIG_ENDIAN
    return nullptr;
#else
    return (id & RETRO_MEMORY_MASK) == RETRO_MEMORY_SYSTEM_RAM && mkxp_retro::sandbox.has_value() ? sb()->instance().w2c_memory.data : nullptr;
#endif // MKXPZ_BIG_ENDIAN
}

extern "C" RETRO_API size_t retro_get_memory_size(unsigned int id) {
#ifdef MKXPZ_BIG_ENDIAN
    return 0;
#else
    return (id & RETRO_MEMORY_MASK) == RETRO_MEMORY_SYSTEM_RAM && mkxp_retro::sandbox.has_value() ? sb()->memory_size() : 0;
#endif // MKXPZ_BIG_ENDIAN
}

#ifndef MKXPZ_SANDBOX_SERIAL_CORE_H
#define MKXPZ_SANDBOX_SERIAL_CORE_H
#include "sandbox-serial-core.h"
#endif // MKXPZ_SANDBOX_SERIAL_CORE_H
