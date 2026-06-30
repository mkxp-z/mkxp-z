/*
** binding-sandbox.cpp
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

#include "binding-sandbox.h"
#include "mkxp-polyfill.h" // std::strtol, std::to_string
#include <string>
#include <libretro.h>
#include <zlib.h>
#include "sharedstate.h"
#include "core.h"
#include "encoding.h"
#include "git-hash.h"
#include "audio-binding.h"
#include "bitmap-binding.h"
#include "etc-binding.h"
#include "font-binding.h"
#include "graphics-binding.h"
#include "input-binding.h"
#include "plane-binding.h"
#include "sprite-binding.h"
#include "table-binding.h"
#include "tilemap-binding.h"
#include "tilemapvx-binding.h"
#include "viewport-binding.h"
#include "window-binding.h"
#include "windowvx-binding.h"

using namespace mkxp_sandbox;

extern const char module_rpg1[];
extern const char module_rpg2[];
extern const char module_rpg3[];

static VALUE top_self;
static VALUE marshal_module;
static VALUE win32api_class;

static VALUE eval_script_func(VALUE arg) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, VALUE, ID> slots;

        VALUE operator()(VALUE arg) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_ary_entry, arg, 0);
                SANDBOX_AWAIT_S(1, rb_ary_entry, arg, 1);
                SANDBOX_AWAIT_S(2, rb_intern, "eval");
                SANDBOX_AWAIT(rb_funcall, top_self, SANDBOX_SLOT(2), 3, SANDBOX_SLOT(0), SANDBOX_NIL, SANDBOX_SLOT(1));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(arg);
}

static VALUE eval_script_rescue(VALUE arg, VALUE exception) {
    return exception;
}

VALUE eval_script::operator()(VALUE string, VALUE filename) {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_S(0, rb_ary_new_capa, 2);
        SANDBOX_AWAIT(rb_ary_store, SANDBOX_SLOT(0), 0, string);
        SANDBOX_AWAIT(rb_ary_store, SANDBOX_SLOT(0), 1, filename);
        SANDBOX_AWAIT_S(0, rb_rescue2, eval_script_func, SANDBOX_SLOT(0), eval_script_rescue, SANDBOX_NIL, reset_class, 0);
    }

    return SANDBOX_SLOT(0);
}

namespace mkxp_sandbox {
    struct run_custom_script : boost::asio::coroutine {
        typedef decl_slots<VALUE, VALUE, ID> slots;

        VALUE operator()(const char *filename) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_str_new_cstr, mkxp_retro::fs->normalize(filename, false, true, "/Game").c_str());
                SANDBOX_AWAIT_S(2, rb_intern, "read");
                SANDBOX_AWAIT_S(0, rb_funcall, sb()->rb_cFile(), SANDBOX_SLOT(2), 1, SANDBOX_SLOT(0));
                SANDBOX_AWAIT_S(1, rb_utf8_str_new_cstr, filename);
                SANDBOX_AWAIT_S(0, eval_script, SANDBOX_SLOT(0), SANDBOX_SLOT(1));
            }

            return SANDBOX_SLOT(0);
        }
    };
}

struct marshal_load : boost::asio::coroutine {
    typedef decl_slots<VALUE, ID> slots;

    VALUE operator()(VALUE obj) {
        BOOST_ASIO_CORO_REENTER (this) {
            SANDBOX_AWAIT_S(1, rb_intern, "load");
            SANDBOX_AWAIT_S(0, rb_funcall, marshal_module, SANDBOX_SLOT(1), 1, obj);
        }

        return SANDBOX_SLOT(0);
    }
};

static VALUE load_data(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t, VALUE, VALUE, ID> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(check_arity, argc, 1, -1);
                SANDBOX_AWAIT_S(0, rb_string_value_cstr, &sb()->ref<VALUE>(argv, 0));
                SANDBOX_AWAIT_S(1, rb_file_open, sb()->str(SANDBOX_SLOT(0)), "rb");
                if (argc < 2 || !SANDBOX_VALUE_TO_BOOL(sb()->ref<VALUE>(argv, 1))) {
                    SANDBOX_AWAIT_S(2, marshal_load, SANDBOX_SLOT(1));
                } else {
                    SANDBOX_AWAIT_S(3, rb_intern, "read");
                    SANDBOX_AWAIT_S(2, rb_funcall, SANDBOX_SLOT(1), SANDBOX_SLOT(3), 0);
                }
                SANDBOX_AWAIT(rb_io_close, SANDBOX_SLOT(1));
            }

            return SANDBOX_SLOT(2);
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE save_data(VALUE self, VALUE obj, VALUE filename) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t, VALUE> slots;

        VALUE operator()(VALUE self, VALUE obj, VALUE filename) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_string_value_cstr, &filename);
                SANDBOX_AWAIT_S(1, rb_file_open, sb()->str(SANDBOX_SLOT(0)), "wb");
                SANDBOX_AWAIT(rb_marshal_dump, obj, SANDBOX_SLOT(1));
                SANDBOX_AWAIT(rb_io_close, SANDBOX_SLOT(1));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, obj, filename);
}

static VALUE rgss_main(VALUE self) {
    struct coro : boost::asio::coroutine {
        static VALUE func(VALUE block) {
            struct coro : boost::asio::coroutine {
                typedef decl_slots<ID> slots;

                VALUE operator()(VALUE block) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_S(0, rb_intern, "call");
                        SANDBOX_AWAIT(rb_funcall, block, SANDBOX_SLOT(0), 0);
                    }

                    return SANDBOX_UNDEF;
                }
            };

            return sb()->bind<struct coro>()()(block);
        }

        static VALUE rescue(VALUE arg, VALUE exception) {
            return exception;
        }

        typedef decl_slots<VALUE, VALUE> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                // Execute postload scripts
                for (SANDBOX_SLOT(0) = 0; SANDBOX_SLOT(0) < shState->config().postloadScripts.size(); ++SANDBOX_SLOT(0)) {
                    SANDBOX_AWAIT_S(1, run_custom_script, shState->config().postloadScripts[SANDBOX_SLOT(0)].c_str());
                    if (SANDBOX_SLOT(1) != SANDBOX_UNDEF) {
                        SANDBOX_AWAIT(rb_exc_raise, SANDBOX_SLOT(1));
                    }
                }

                while (true) {
                    SANDBOX_AWAIT_S(0, rb_block_proc);
                    SANDBOX_AWAIT_S(0, rb_rescue2, func, SANDBOX_SLOT(0), rescue, SANDBOX_NIL, reset_class, 0);

                    // Stop unless a reset was requested
                    if (SANDBOX_SLOT(0) == SANDBOX_UNDEF) {
                        return SANDBOX_NIL;
                    }

                    SANDBOX_AWAIT(audio_reset);
                    SANDBOX_AWAIT(graphics_reset);
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE rgss_stop(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                while (true) {
                    SANDBOX_GUARD_L(SANDBOX_SLOT(0) = shState->graphics().update(sb().e));
                    if (SANDBOX_SLOT(0)) {
                        SANDBOX_YIELD;
                    }
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static void msgbox_impl(wasm_ptr_t message_ptr) {
    const struct sandbox_str_guard message = sb()->str(message_ptr);
    wasm_size_t message_len = sb()->strlen(message_ptr);
    const char *ptr = (const char *)message;
    const char *line_start = ptr;
    for (wasm_size_t i = 0; i < message_len;) {
        if (++i == message_len || *ptr == '\n') {
            mkxp_retro_log_printf(RETRO_LOG_WARN, "[mkxp-z msgbox] %.*s\n", (int)std::min(ptr - line_start, (ptrdiff_t)INT_MAX), line_start);
            line_start = ++ptr;
        } else {
            ++ptr;
        }
    }
    mkxp_retro::display_message(RETRO_LOG_WARN, message);
}

static VALUE msgbox(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t, VALUE, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                for (SANDBOX_SLOT(2) = 0; SANDBOX_SLOT(2) < argc; ++SANDBOX_SLOT(2)) {
                    SANDBOX_AWAIT_S(1, rb_obj_as_string, sb()->ref<VALUE>(argv, SANDBOX_SLOT(2)));
                    SANDBOX_AWAIT_S(0, rb_string_value_cstr, &SANDBOX_SLOT(1));
                    msgbox_impl(SANDBOX_SLOT(0));
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE msgbox_p(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t, VALUE, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                for (SANDBOX_SLOT(2) = 0; SANDBOX_SLOT(2) < argc; ++SANDBOX_SLOT(2)) {
                    SANDBOX_AWAIT_S(1, rb_inspect, sb()->ref<VALUE>(argv, SANDBOX_SLOT(2)));
                    SANDBOX_AWAIT_S(0, rb_string_value_cstr, &SANDBOX_SLOT(1));
                    msgbox_impl(SANDBOX_SLOT(0));
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE kernel_caller(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_size_t, VALUE, VALUE, VALUE, VALUE, ID> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(5, rb_intern, "_mkxp_kernel_caller_alias");
                SANDBOX_AWAIT_S(1, rb_funcall, sb()->rb_mKernel(), SANDBOX_SLOT(5), 0);

                SANDBOX_AWAIT_S(2, rb_obj_is_kind_of, SANDBOX_SLOT(1), sb()->rb_cArray());
                if (!SANDBOX_VALUE_TO_BOOL(SANDBOX_SLOT(2))) {
                    return SANDBOX_SLOT(1);
                }

                SANDBOX_AWAIT_S(0, get_length, SANDBOX_SLOT(1));
                if (SANDBOX_SLOT(0) < 2) {
                    return SANDBOX_SLOT(1);
                }

                /* Remove useless "ruby:1:in 'eval'" */
                SANDBOX_AWAIT(rb_ary_pop, SANDBOX_SLOT(1));

                /* Also remove trace of this helper function */
                SANDBOX_AWAIT(rb_ary_shift, SANDBOX_SLOT(1));

                SANDBOX_SLOT(0) -= 2;

                if (SANDBOX_SLOT(0) == 0) {
                    return SANDBOX_SLOT(1);
                }

                /* RMXP does this, not sure if specific or 1.8 related */
                SANDBOX_AWAIT_S(2, rb_ary_entry, SANDBOX_SLOT(1), -1);
                SANDBOX_AWAIT_S(3, rb_utf8_str_new_cstr, ":in `<main>'");
                SANDBOX_AWAIT_S(4, rb_utf8_str_new_cstr, "");
                SANDBOX_AWAIT_S(5, rb_intern, "gsub!");
                SANDBOX_AWAIT(rb_funcall, SANDBOX_SLOT(2), SANDBOX_SLOT(5), 2, SANDBOX_SLOT(3), SANDBOX_SLOT(4));
            }

            return SANDBOX_SLOT(1);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE delta(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<double, VALUE> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_SLOT(0) = shState->runTime();
                SANDBOX_AWAIT_S(1, rb_float_new, SANDBOX_SLOT(0));
            }

            return SANDBOX_SLOT(1);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE data_directory(VALUE self) {
    return sb()->bind<struct rb_str_new_cstr>()()("/Save/");
}

static VALUE get_window_title(VALUE self) {
    return sb()->bind<struct rb_utf8_str_new_cstr>()()("");
}

static VALUE set_window_title(VALUE self, VALUE value) {
    return value;
}

static VALUE show_settings(VALUE self) {
    return SANDBOX_NIL;
}

static VALUE desensitize(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t, VALUE> slots;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_string_value_cstr, &value);
                SANDBOX_AWAIT_S(1, rb_utf8_str_new_cstr, mkxp_retro::fs->desensitize(sb()->str(SANDBOX_SLOT(0))));
            }

            return SANDBOX_SLOT(1);
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE platform(VALUE self) {
    return sb()->bind<struct rb_utf8_str_new_cstr>()()("libretro");
}

static VALUE is_not_libretro(VALUE self) {
    return SANDBOX_FALSE;
}

static VALUE is_libretro(VALUE self) {
    return SANDBOX_TRUE;
}

static VALUE user_language(VALUE self) {
    const char *str;

    enum retro_language language;
    if (!mkxp_retro::environment(RETRO_ENVIRONMENT_GET_LANGUAGE, &language)) {
        language = RETRO_LANGUAGE_ENGLISH;
    }
    switch (language) {
        default: case RETRO_LANGUAGE_ENGLISH: str = "en_US"; break;
        case RETRO_LANGUAGE_JAPANESE: str = "ja_JP"; break;
        case RETRO_LANGUAGE_FRENCH: str = "fr_FR"; break;
        case RETRO_LANGUAGE_SPANISH: str = "es_ES"; break;
        case RETRO_LANGUAGE_GERMAN: str = "de_DE"; break;
        case RETRO_LANGUAGE_ITALIAN: str = "it_IT"; break;
        case RETRO_LANGUAGE_DUTCH: str = "nl_NL"; break;
        case RETRO_LANGUAGE_PORTUGUESE_BRAZIL: str = "pt_BR"; break;
        case RETRO_LANGUAGE_PORTUGUESE_PORTUGAL: str = "pt_PT"; break;
        case RETRO_LANGUAGE_RUSSIAN: str = "ru_RU"; break;
        case RETRO_LANGUAGE_KOREAN: str = "ko_KR"; break;
        case RETRO_LANGUAGE_CHINESE_TRADITIONAL: str = "zh_TW"; break;
        case RETRO_LANGUAGE_CHINESE_SIMPLIFIED: str = "zh_CN"; break;
        case RETRO_LANGUAGE_ESPERANTO: str = "eo"; break;
        case RETRO_LANGUAGE_POLISH: str = "pl_PL"; break;
        case RETRO_LANGUAGE_VIETNAMESE: str = "vi_VN"; break;
        case RETRO_LANGUAGE_ARABIC: str = "ar_SA"; break;
        case RETRO_LANGUAGE_GREEK: str = "el_GR"; break;
        case RETRO_LANGUAGE_TURKISH: str = "tr_TR"; break;
        case RETRO_LANGUAGE_SLOVAK: str = "sk_SK"; break;
        case RETRO_LANGUAGE_PERSIAN: str = "fa_IR"; break;
        case RETRO_LANGUAGE_HEBREW: str = "he_IL"; break;
        case RETRO_LANGUAGE_ASTURIAN: str = "ast_ES"; break;
        case RETRO_LANGUAGE_FINNISH: str = "fi_FI"; break;
        case RETRO_LANGUAGE_INDONESIAN: str = "id_ID"; break;
        case RETRO_LANGUAGE_SWEDISH: str = "sv_SE"; break;
        case RETRO_LANGUAGE_UKRAINIAN: str = "uk_UA"; break;
        case RETRO_LANGUAGE_CZECH: str = "cs_CZ"; break;
        case RETRO_LANGUAGE_CATALAN_VALENCIA: str = "ca_ES"; break;
        case RETRO_LANGUAGE_CATALAN: str = "ca_AD"; break;
        case RETRO_LANGUAGE_BRITISH_ENGLISH: str = "en_GB"; break;
        case RETRO_LANGUAGE_HUNGARIAN: str = "hu_HU"; break;
        case RETRO_LANGUAGE_BELARUSIAN: str = "be_BY"; break;
        case RETRO_LANGUAGE_GALICIAN: str = "gl_ES"; break;
        case RETRO_LANGUAGE_NORWEGIAN: str = "no_NO"; break;
        case RETRO_LANGUAGE_IRISH: str = "ga_IE"; break;
        case RETRO_LANGUAGE_THAI: str = "th_TH"; break;
    }

    return sb()->bind<struct rb_utf8_str_new_cstr>()()(str);
}

static VALUE user_name(VALUE self) {
    const char *str;

    if (!mkxp_retro::environment(RETRO_ENVIRONMENT_GET_USERNAME, &str) || str == nullptr) {
        str = "";
    }

    return sb()->bind<struct rb_utf8_str_new_cstr>()()(str);
}

static VALUE game_title(VALUE self) {
    return sb()->bind<struct rb_utf8_str_new_cstr>()()(shState->config().game.title.c_str());
}

static VALUE power_state(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, VALUE, VALUE, ID> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (!mkxp_retro::environment(RETRO_ENVIRONMENT_GET_DEVICE_POWER, &sb().device_power)) {
                    sb().device_power.state = RETRO_POWERSTATE_UNKNOWN;
                    sb().device_power.seconds = RETRO_POWERSTATE_NO_ESTIMATE;
                    sb().device_power.percent = RETRO_POWERSTATE_NO_ESTIMATE;
                }

                SANDBOX_AWAIT_S(0, rb_hash_new);

                SANDBOX_AWAIT_S(3, rb_intern, "seconds");
                SANDBOX_AWAIT_S(1, rb_id2sym, SANDBOX_SLOT(3));
                if (sb().device_power.seconds != RETRO_POWERSTATE_NO_ESTIMATE) {
                    SANDBOX_AWAIT_S(2, rb_ll2inum, sb().device_power.seconds);
                } else {
                    SANDBOX_SLOT(2) = SANDBOX_NIL;
                }
                SANDBOX_AWAIT(rb_hash_aset, SANDBOX_SLOT(0), SANDBOX_SLOT(1), SANDBOX_SLOT(2));

                SANDBOX_AWAIT_S(3, rb_intern, "percent");
                SANDBOX_AWAIT_S(1, rb_id2sym, SANDBOX_SLOT(3));
                if (sb().device_power.percent != RETRO_POWERSTATE_NO_ESTIMATE) {
                    SANDBOX_AWAIT_S(2, rb_ll2inum, sb().device_power.percent);
                } else {
                    SANDBOX_SLOT(2) = SANDBOX_NIL;
                }
                SANDBOX_AWAIT(rb_hash_aset, SANDBOX_SLOT(0), SANDBOX_SLOT(1), SANDBOX_SLOT(2));

                SANDBOX_AWAIT_S(3, rb_intern, "discharging");
                SANDBOX_AWAIT_S(1, rb_id2sym, SANDBOX_SLOT(3));
                if (sb().device_power.state != RETRO_POWERSTATE_UNKNOWN) {
                    SANDBOX_SLOT(2) = SANDBOX_BOOL_TO_VALUE(sb().device_power.state == RETRO_POWERSTATE_DISCHARGING);
                } else {
                    SANDBOX_SLOT(2) = SANDBOX_NIL;
                }
                SANDBOX_AWAIT(rb_hash_aset, SANDBOX_SLOT(0), SANDBOX_SLOT(1), SANDBOX_SLOT(2));
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE nproc(VALUE self) {
    return sb()->bind<struct rb_num2int>()()(1);
}

static VALUE memory(VALUE self) {
    return sb()->bind<struct rb_num2int>()()(0);
}

static VALUE reload_cache(VALUE self) {
    mkxp_retro::fs->reloadPathCache();
    return SANDBOX_NIL;
}

static VALUE mount(int32_t argc, wasm_ptr_t argv, VALUE self) {
    sb()->bind<struct exception_raise>()()(Exception(Exception::PHYSFSError, "Failed to mount (operation not supported in libretro builds)"));
    return SANDBOX_NIL;
}

static VALUE unmount(int32_t argc, wasm_ptr_t argv, VALUE self) {
    sb()->bind<struct exception_raise>()()(Exception(Exception::PHYSFSError, "Failed to unmount (operation not supported in libretro builds)"));
    return SANDBOX_NIL;
}

static VALUE file_exists(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t> slots;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_string_value_cstr, &value);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::fs->exists(sb()->str(SANDBOX_SLOT(0))));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE launch(VALUE self, VALUE cmdname, VALUE args) {
    sb()->bind<struct exception_raise>()()(Exception(Exception::MKXPError, "Failed to launch (operation not supported in libretro builds)"));
    return SANDBOX_NIL;
}

static VALUE default_font_family(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t> slots;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_string_value_cstr, &value);
                shState->fontState().setDefaultFontFamily(std::string(sb()->str(SANDBOX_SLOT(0))));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE system_puts(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, ID> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(1, rb_intern, "puts");
                SANDBOX_AWAIT_S(0, rb_funcallv, sb()->rb_mKernel(), SANDBOX_SLOT(1), argc, argv);
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE to_utf8(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t, VALUE> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_string_value_cstr, &self);
                SANDBOX_AWAIT_S(1, rb_utf8_str_new_cstr, Encoding::convertString((const char *)sb()->str(SANDBOX_SLOT(0))).c_str());
            }

            return SANDBOX_SLOT(1);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE to_utf8_bang(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t, int32_t> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_string_value_cstr, &self);
                sb().convert_string_buffer = Encoding::convertString((const char *)sb()->str(SANDBOX_SLOT(0)));
                SANDBOX_AWAIT(rb_str_resize, self, sb().convert_string_buffer.length());
                SANDBOX_AWAIT_S(0, rb_string_value_cstr, &self);
                sb()->strcpy(SANDBOX_SLOT(0), sb().convert_string_buffer.c_str());
                SANDBOX_AWAIT_S(1, rb_utf8_encindex);
                SANDBOX_AWAIT(rb_enc_set_index, self, SANDBOX_SLOT(1));
            }

            return self;
        }

        void end() noexcept {
            sb().convert_string_buffer.clear();
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE win32api_initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(check_arity, argc, 2, 4);
                SANDBOX_AWAIT_S(0, rb_str_new_cstr, "Failed loading ");
                SANDBOX_AWAIT_S(0, rb_str_concat, SANDBOX_SLOT(0), sb()->ref<VALUE>(argv, 0));
                SANDBOX_AWAIT_S(0, rb_str_cat_cstr, SANDBOX_SLOT(0), ": Win32API is not supported in libretro builds of mkxp-z");
                SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), sb()->rb_eRuntimeError());
                SANDBOX_AWAIT(rb_exc_raise, SANDBOX_SLOT(0));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE win32api_call(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_str_new_cstr, "Win32API is not supported in libretro builds of mkxp-z");
                SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), sb()->rb_eRuntimeError());
                SANDBOX_AWAIT(rb_exc_raise, SANDBOX_SLOT(0));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

void sandbox_binding_init::operator()() {
    static VALUE system_module;
    static VALUE cfg_module;

    struct register_ruby_revision : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t, VALUE, ID> slots;

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(2, rb_intern, "RUBY_REVISION");
                SANDBOX_AWAIT_S(1, rb_const_get, sb()->rb_cObject(), SANDBOX_SLOT(2));
                SANDBOX_AWAIT_S(0, rb_string_value_cstr, &SANDBOX_SLOT(1));

                struct sandbox_str_guard str = sb()->str(SANDBOX_SLOT(0));
                MKXPZ_FORCED_ASSERT(std::strlen(str) == 2 * sizeof mkxp_retro::ruby_revision);
                for (size_t i = 0; i < sizeof mkxp_retro::ruby_revision; ++i) {
                    mkxp_retro::ruby_revision[i] = std::strtol(std::string(str + 2 * i, 2).c_str(), nullptr, 16);
                }
            }
        }
    };

    struct marshal_binding_init : boost::asio::coroutine {
        static VALUE string_force_utf8(VALUE yielded_arg, VALUE callback_arg, int32_t argc, wasm_ptr_t argv, VALUE blockarg) {
            struct coro : boost::asio::coroutine {
                typedef decl_slots<VALUE, ID, int32_t, int32_t> slots;

                VALUE operator()(VALUE yielded_arg, VALUE callback_arg, int32_t argc, wasm_ptr_t argv, VALUE blockarg) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_S(0, rb_obj_is_kind_of, yielded_arg, sb()->rb_cString());
                        if (SANDBOX_VALUE_TO_BOOL(SANDBOX_SLOT(0))) {
                            SANDBOX_AWAIT_S(2, rb_enc_get_index, yielded_arg);
                            SANDBOX_AWAIT_S(3, rb_ascii8bit_encindex);
                            if (SANDBOX_SLOT(2) == SANDBOX_SLOT(3)) {
                                SANDBOX_AWAIT_S(2, rb_utf8_encindex);
                                SANDBOX_AWAIT(rb_enc_associate_index, yielded_arg, SANDBOX_SLOT(2));
                            }
                        }
                        if (callback_arg != SANDBOX_UNDEF) {
                            SANDBOX_AWAIT_S(1, rb_intern, "call");
                            SANDBOX_AWAIT_S(0, rb_funcall, callback_arg, SANDBOX_SLOT(1), 1, yielded_arg);
                        } else {
                            SANDBOX_SLOT(0) = yielded_arg;
                        }
                    }

                    return SANDBOX_SLOT(0);
                }
            };

            return sb()->bind<struct coro>()()(yielded_arg, callback_arg, argc, argv, blockarg);
        }

        static VALUE marshal_load(int32_t argc, wasm_ptr_t argv, VALUE self) {
            struct coro : boost::asio::coroutine {
                typedef decl_slots<VALUE, ID> slots;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT(check_arity, argc, 1, 2);
                        if (argc > 1) {
                            SANDBOX_AWAIT_S(0, rb_proc_new, string_force_utf8, sb()->ref<VALUE>(argv, 0));
                        } else {
                            SANDBOX_AWAIT_S(0, rb_proc_new, string_force_utf8, SANDBOX_UNDEF);
                        }
                        SANDBOX_AWAIT_S(1, rb_intern, "_mkxp_load_alias");
                        SANDBOX_AWAIT_S(0, rb_funcall, marshal_module, SANDBOX_SLOT(1), 2, sb()->ref<VALUE>(argv, 0), SANDBOX_SLOT(0));
                    }

                    return SANDBOX_SLOT(0);
                }
            };

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        typedef decl_slots<VALUE, ID> slots;

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                /* We overload the built-in 'Marshal::load()' function to silently
                 * insert our utf8proc that ensures all read strings will be
                 * UTF-8 encoded */
                SANDBOX_AWAIT_S(1, rb_intern, "Marshal");
                SANDBOX_AWAIT_R(marshal_module, rb_const_get, sb()->rb_cObject(), SANDBOX_SLOT(1));
                SANDBOX_AWAIT_S(0, rb_singleton_class, marshal_module);
                SANDBOX_AWAIT(rb_define_alias, SANDBOX_SLOT(0), "_mkxp_load_alias", "load");
                SANDBOX_AWAIT(rb_define_module_function, marshal_module, "load", (VALUE (*)(ANYARGS))marshal_load, -1);
            }
        }
    };

    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT(register_ruby_revision);
        if (rgssVer == 1) {
            top_self = SANDBOX_NIL;
        } else {
            SANDBOX_AWAIT_R(top_self, rb_eval_string, "self");
        }
        SANDBOX_AWAIT(exception_binding_init);

        SANDBOX_AWAIT(table_binding_init);
        SANDBOX_AWAIT(etc_binding_init);
        SANDBOX_AWAIT(font_binding_init);
        SANDBOX_AWAIT(bitmap_binding_init);
        SANDBOX_AWAIT(sprite_binding_init);
        SANDBOX_AWAIT(viewport_binding_init);
        SANDBOX_AWAIT(plane_binding_init);

        if (rgssVer == 1) {
            SANDBOX_AWAIT(window_binding_init);
            SANDBOX_AWAIT(tilemap_binding_init);
        } else {
            SANDBOX_AWAIT(windowvx_binding_init);
            SANDBOX_AWAIT(tilemapvx_binding_init);
        }

        SANDBOX_AWAIT(input_binding_init);
        SANDBOX_AWAIT(audio_binding_init);
        SANDBOX_AWAIT(graphics_binding_init);

        SANDBOX_AWAIT(marshal_binding_init);

        SANDBOX_AWAIT(rb_define_module_function, sb()->rb_mKernel(), "load_data", (VALUE (*)(ANYARGS))load_data, -1);
        SANDBOX_AWAIT(rb_define_module_function, sb()->rb_mKernel(), "save_data", (VALUE (*)(ANYARGS))save_data, 2);

        SANDBOX_AWAIT_R(win32api_class, rb_define_class, "Win32API", sb()->rb_cObject());
        SANDBOX_AWAIT(rb_define_method, win32api_class, "initialize", (VALUE (*)(ANYARGS))win32api_initialize, -1);
        SANDBOX_AWAIT(rb_define_method, win32api_class, "call", (VALUE (*)(ANYARGS))win32api_call, -1);
        SANDBOX_AWAIT(rb_define_alias, win32api_class, "Call", "call");

        if (rgssVer >= 3) {
            SANDBOX_AWAIT(rb_define_module_function, sb()->rb_mKernel(), "rgss_main", (VALUE (*)(ANYARGS))rgss_main, 0);
            SANDBOX_AWAIT(rb_define_module_function, sb()->rb_mKernel(), "rgss_stop", (VALUE (*)(ANYARGS))rgss_stop, 0);

            SANDBOX_AWAIT(rb_define_module_function, sb()->rb_mKernel(), "msgbox", (VALUE (*)(ANYARGS))msgbox, -1);
            SANDBOX_AWAIT(rb_define_module_function, sb()->rb_mKernel(), "msgbox_p", (VALUE (*)(ANYARGS))msgbox_p, -1);

            SANDBOX_AWAIT_S(0, rb_utf8_str_new_cstr, "3.0.1");
            SANDBOX_AWAIT(rb_define_global_const, "RGSS_VERSION", SANDBOX_SLOT(0));
        } else {
            SANDBOX_AWAIT(rb_define_module_function, sb()->rb_mKernel(), "print", (VALUE (*)(ANYARGS))msgbox, -1);
            SANDBOX_AWAIT(rb_define_module_function, sb()->rb_mKernel(), "p", (VALUE (*)(ANYARGS))msgbox_p, -1);

            SANDBOX_AWAIT_S(0, rb_singleton_class, sb()->rb_mKernel());
            SANDBOX_AWAIT(rb_define_alias, SANDBOX_SLOT(0), "_mkxp_kernel_caller_alias", "caller");
            SANDBOX_AWAIT(rb_define_module_function, sb()->rb_mKernel(), "caller", (VALUE (*)(ANYARGS))kernel_caller, 0);
        }

        if (rgssVer == 1) {
            SANDBOX_AWAIT(rb_eval_string, module_rpg1);
        } else if (rgssVer == 2) {
            SANDBOX_AWAIT(rb_eval_string, module_rpg2);
        } else if (rgssVer == 3) {
            SANDBOX_AWAIT(rb_eval_string, module_rpg3);
        } else {
            MKXPZ_FORCED_ASSERT_WITH_MESSAGE(false, "RGSS version is something other than 1, 2 or 3");
        }

        SANDBOX_AWAIT_R(system_module, rb_define_module, "System");

        SANDBOX_AWAIT(rb_define_module_function, system_module, "delta", (VALUE (*)(ANYARGS))delta, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "uptime", (VALUE (*)(ANYARGS))delta, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "data_directory", (VALUE (*)(ANYARGS))data_directory, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "set_window_title", (VALUE (*)(ANYARGS))set_window_title, 1);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "window_title", (VALUE (*)(ANYARGS))get_window_title, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "window_title=", (VALUE (*)(ANYARGS))set_window_title, 1);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "show_settings", (VALUE (*)(ANYARGS))show_settings, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "desensitize", (VALUE (*)(ANYARGS))desensitize, 1);

        SANDBOX_AWAIT(rb_define_module_function, system_module, "platform", (VALUE (*)(ANYARGS))platform, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "is_mac?", (VALUE (*)(ANYARGS))is_not_libretro, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "is_rosetta?", (VALUE (*)(ANYARGS))is_not_libretro, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "is_linux?", (VALUE (*)(ANYARGS))is_libretro, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "is_windows?", (VALUE (*)(ANYARGS))is_not_libretro, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "is_wine?", (VALUE (*)(ANYARGS))is_not_libretro, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "is_really_mac?", (VALUE (*)(ANYARGS))is_not_libretro, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "is_really_linux?", (VALUE (*)(ANYARGS))is_not_libretro, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "is_really_windows?", (VALUE (*)(ANYARGS))is_not_libretro, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "is_libretro?", (VALUE (*)(ANYARGS))is_libretro, 0);

        SANDBOX_AWAIT(rb_define_module_function, system_module, "user_language", (VALUE (*)(ANYARGS))user_language, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "user_name", (VALUE (*)(ANYARGS))user_name, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "game_title", (VALUE (*)(ANYARGS))game_title, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "power_state", (VALUE (*)(ANYARGS))power_state, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "nproc", (VALUE (*)(ANYARGS))nproc, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "memory", (VALUE (*)(ANYARGS))memory, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "reload_cache", (VALUE (*)(ANYARGS))reload_cache, 0);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "mount", (VALUE (*)(ANYARGS))mount, -1);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "unmount", (VALUE (*)(ANYARGS))unmount, -1);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "file_exists?", (VALUE (*)(ANYARGS))file_exists, 1);
        SANDBOX_AWAIT(rb_define_module_function, system_module, "launch", (VALUE (*)(ANYARGS))launch, 2);

        SANDBOX_AWAIT(rb_define_module_function, system_module, "default_font_family=", (VALUE (*)(ANYARGS))default_font_family, 1);

        SANDBOX_AWAIT(rb_define_module_function, system_module, "puts", (VALUE (*)(ANYARGS))system_puts, -1);

        SANDBOX_AWAIT(rb_define_method, sb()->rb_cString(), "to_utf8", (VALUE (*)(ANYARGS))to_utf8, 0);
        SANDBOX_AWAIT(rb_define_method, sb()->rb_cString(), "to_utf8!", (VALUE (*)(ANYARGS))to_utf8_bang, 0);

        SANDBOX_AWAIT_R(cfg_module, rb_define_module, "CFG");

        // TODO
        //SANDBOX_AWAIT(rb_define_module_function, cfg_module, "[]", (VALUE (*)(ANYARGS))get_json_setting, 1);
        //SANDBOX_AWAIT(rb_define_module_function, cfg_module, "[]=", (VALUE (*)(ANYARGS))set_json_setting, 2);
        //SANDBOX_AWAIT(rb_define_module_function, cfg_module, "to_hash", (VALUE (*)(ANYARGS))get_all_json_settings, 0);

        SANDBOX_AWAIT(rb_gv_set, "MKXP", SANDBOX_TRUE);

        SANDBOX_AWAIT_S(0, rb_utf8_str_new_cstr, MKXPZ_VERSION "/" MKXPZ_GIT_HASH);
        SANDBOX_AWAIT(rb_str_freeze, SANDBOX_SLOT(0));
        SANDBOX_AWAIT(rb_define_const, system_module, "VERSION", SANDBOX_SLOT(0));

        SANDBOX_AWAIT_S(1, rb_intern, "Zlib");
        SANDBOX_AWAIT_S(0, rb_const_defined, sb()->rb_mKernel(), SANDBOX_SLOT(1));
        if (!SANDBOX_VALUE_TO_BOOL(SANDBOX_SLOT(0))) {
            SANDBOX_AWAIT(rb_require, "zlib");
        }
    }
}

void sandbox_run_rmxp_scripts::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT(rb_gv_set, rgssVer >= 2 ? "TEST" : "DEBUG", SANDBOX_BOOL_TO_VALUE(shState->config().editor.debug));
        SANDBOX_AWAIT(rb_gv_set, "BTEST", SANDBOX_BOOL_TO_VALUE(shState->config().editor.battleTest));

        // Unmarshal the script array into slot 1
        if (rgssVer == 1) {
            SANDBOX_AWAIT_S(0, rb_file_open, "Data/Scripts.rxdata", "rb");
        } else if (rgssVer == 2) {
            SANDBOX_AWAIT_S(0, rb_file_open, "Data/Scripts.rvdata", "rb");
        } else if (rgssVer == 3) {
            SANDBOX_AWAIT_S(0, rb_file_open, "Data/Scripts.rvdata2", "rb");
        } else {
            MKXPZ_FORCED_ASSERT_WITH_MESSAGE(false, "RGSS version is something other than 1, 2 or 3");
        }
        SANDBOX_AWAIT_S(1, marshal_load, SANDBOX_SLOT(0));
        SANDBOX_AWAIT(rb_io_close, SANDBOX_SLOT(0));

        // Assign it to the "$RGSS_SCRIPTS" global variable
        SANDBOX_AWAIT(rb_gv_set, "$RGSS_SCRIPTS", SANDBOX_SLOT(1));

        // Get the number of scripts into slot 2
        SANDBOX_AWAIT_S(2, get_length, SANDBOX_SLOT(1));

        sb().script_decode_buffer.resize(0x1000);

        for (SANDBOX_SLOT(3) = 0; SANDBOX_SLOT(3) < SANDBOX_SLOT(2); ++SANDBOX_SLOT(3)) {
            // Get the script into slot 4
            SANDBOX_AWAIT_S(4, rb_ary_entry, SANDBOX_SLOT(1), SANDBOX_SLOT(3));
            // Skip this script object if it's not an array
            SANDBOX_AWAIT_S(0, rb_obj_is_kind_of, SANDBOX_SLOT(4), sb()->rb_cArray());
            if (!SANDBOX_VALUE_TO_BOOL(SANDBOX_SLOT(0))) {
                continue;
            }

            // Get the name of the script, the zlib-compressed script contents of the script and the length of said contents into slots 5, 6 and 7, respectively
            SANDBOX_AWAIT_S(0, rb_ary_entry, SANDBOX_SLOT(4), 1);
            SANDBOX_AWAIT_S(5, rb_string_value_cstr, &SANDBOX_SLOT(0));
            SANDBOX_AWAIT_S(0, rb_ary_entry, SANDBOX_SLOT(4), 2);
            SANDBOX_AWAIT_S(6, rb_string_value_ptr, &SANDBOX_SLOT(0));
            SANDBOX_AWAIT_S(7, get_bytesize, SANDBOX_SLOT(0));

            // Decompress the script contents
            {
                int zlib_result = Z_OK;
                unsigned long buffer_len = sb().script_decode_buffer.size() - 1;
                while (true) {
                    zlib_result = uncompress(
                        sb().script_decode_buffer.data(),
                        &buffer_len,
                        (const unsigned char *)(const char *)sb()->str(SANDBOX_SLOT(6)),
                        SANDBOX_SLOT(7)
                    );
                    sb().script_decode_buffer[buffer_len] = 0;

                    if (zlib_result != Z_BUF_ERROR) {
                        break;
                    }

                    if (2UL * (unsigned long)sb().script_decode_buffer.size() <= (unsigned long)sb().script_decode_buffer.size()) {
                        MKXPZ_THROW(std::bad_alloc());
                    }
                    sb().script_decode_buffer.resize(2 * sb().script_decode_buffer.size());
                    buffer_len = sb().script_decode_buffer.size() - 1;
                }

                if (zlib_result != Z_OK) {
                    LOG_PRINTF(RETRO_LOG_ERROR, "Error decoding script %zu: '%s'\n", SANDBOX_SLOT(3), (const char *)sb()->str(SANDBOX_SLOT(5)));
                    break;
                }
            }

            SANDBOX_AWAIT_S(0, rb_utf8_str_new_cstr, (const char *)sb().script_decode_buffer.data());
            SANDBOX_AWAIT(rb_ary_store, SANDBOX_SLOT(4), 3, SANDBOX_SLOT(0));
        }

        sb().script_decode_buffer.clear();

        // Execute preload scripts
        for (SANDBOX_SLOT(3) = 0; SANDBOX_SLOT(3) < shState->config().preloadScripts.size(); ++SANDBOX_SLOT(3)) {
            SANDBOX_AWAIT_S(9, run_custom_script, shState->config().preloadScripts[SANDBOX_SLOT(3)].c_str());
            if (SANDBOX_SLOT(9) != SANDBOX_UNDEF) {
                return;
            }
        }

        while (true) {
            for (SANDBOX_SLOT(9) = SANDBOX_UNDEF, SANDBOX_SLOT(3) = 0; SANDBOX_SLOT(9) == SANDBOX_UNDEF && SANDBOX_SLOT(3) < SANDBOX_SLOT(2); ++SANDBOX_SLOT(3)) {
                // Get the script into slot 4
                SANDBOX_AWAIT_S(4, rb_ary_entry, SANDBOX_SLOT(1), SANDBOX_SLOT(3));
                // Skip this script object if it's not an array
                SANDBOX_AWAIT_S(0, rb_obj_is_kind_of, SANDBOX_SLOT(4), sb()->rb_cArray());
                if (!SANDBOX_VALUE_TO_BOOL(SANDBOX_SLOT(0))) {
                    continue;
                }

                SANDBOX_AWAIT_S(9, rb_str_new_cstr, (std::string("000").substr(std::min((size_t)3, std::to_string(SANDBOX_SLOT(3)).length())) + std::to_string(SANDBOX_SLOT(3)) + ":").c_str());
                SANDBOX_AWAIT_S(10, rb_ary_entry, SANDBOX_SLOT(4), 1);
                SANDBOX_AWAIT_S(10, rb_str_concat, SANDBOX_SLOT(9), SANDBOX_SLOT(10));
                SANDBOX_AWAIT_S(9, rb_ary_entry, SANDBOX_SLOT(4), 3);

                SANDBOX_AWAIT_S(9, eval_script, SANDBOX_SLOT(9), SANDBOX_SLOT(10));
            }

            // Stop unless a reset was requested
            if (SANDBOX_SLOT(9) == SANDBOX_UNDEF) {
                break;
            }

            SANDBOX_AWAIT(audio_reset);
            SANDBOX_AWAIT(graphics_reset);
        }
    }
}

void sandbox_run_rmxp_scripts::end() noexcept {
    sb().script_decode_buffer.clear();
}
