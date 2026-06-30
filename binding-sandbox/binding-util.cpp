/*
** binding-util.cpp
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

#include "binding-util.h"
#include "binding-sandbox.h"
#include "core.h"
#include "sharedstate.h"

using namespace mkxp_sandbox;

void mkxp_sandbox::dfree(wasm_objkey_t key) {
    sb()->destroy_object(key);
}

static VALUE run_cheat_func(VALUE arg) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, VALUE> slots;

        VALUE operator()(VALUE arg) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_ary_entry, arg, 0);
                SANDBOX_AWAIT_S(1, rb_ary_entry, arg, 1);
                SANDBOX_AWAIT_S(0, eval_script, SANDBOX_SLOT(0), SANDBOX_SLOT(1));
                if (SANDBOX_SLOT(0) != SANDBOX_UNDEF) {
                    SANDBOX_AWAIT(rb_exc_raise, SANDBOX_SLOT(0));
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(arg);
}

static VALUE run_cheat_rescue(VALUE arg, VALUE exception) {
    sb()->bind<struct log_backtrace>()()(exception);
    return SANDBOX_NIL;
}

void _sandbox_yield_run_cheat::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        for (SANDBOX_SLOT(2) = 0; SANDBOX_SLOT(2) < sb().cheats.size(); ++SANDBOX_SLOT(2)) {
            LOG_PRINTF(RETRO_LOG_INFO, "Executing cheat #%u (%llu bytes)\n", sb().cheats[SANDBOX_SLOT(2)].first, (unsigned long long)sb().cheats[SANDBOX_SLOT(2)].second.size());
            SANDBOX_AWAIT_S(0, rb_ary_new_capa, 2);
            SANDBOX_AWAIT_S(1, rb_str_new_cstr, ("<cheat #" + std::to_string(sb().cheats[SANDBOX_SLOT(2)].first) + ">").c_str());
            SANDBOX_AWAIT(rb_ary_store, SANDBOX_SLOT(0), 1, SANDBOX_SLOT(1));
            SANDBOX_AWAIT_S(1, rb_str_new_cstr, sb().cheats[SANDBOX_SLOT(2)].second.c_str());
            SANDBOX_AWAIT(rb_ary_store, SANDBOX_SLOT(0), 0, SANDBOX_SLOT(1));
            SANDBOX_AWAIT(rb_rescue2, run_cheat_func, SANDBOX_SLOT(0), run_cheat_rescue, SANDBOX_NIL, sb()->rb_eException(), 0);
        }
    }
}

void _sandbox_yield_run_cheat::end() noexcept {
    sb().cheats.clear();
}

wasm_size_t get_length::operator()(VALUE obj) {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_S(0, rb_intern, "length");
        SANDBOX_AWAIT_S(1, rb_funcall, obj, SANDBOX_SLOT(0), 0);
        SANDBOX_AWAIT_S(2, rb_num2ulong, SANDBOX_SLOT(1));
    }

    return SANDBOX_SLOT(2);
}

wasm_size_t get_bytesize::operator()(VALUE obj) {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_S(0, rb_intern, "bytesize");
        SANDBOX_AWAIT_S(1, rb_funcall, obj, SANDBOX_SLOT(0), 0);
        SANDBOX_AWAIT_S(2, rb_num2ulong, SANDBOX_SLOT(1));
    }

    return SANDBOX_SLOT(2);
}

void log_backtrace::operator()(VALUE exception) {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_S(8, rb_intern, "message");
        SANDBOX_AWAIT_S(5, rb_funcall, exception, SANDBOX_SLOT(8), 0);
        SANDBOX_AWAIT_S(0, rb_string_value_cstr, &SANDBOX_SLOT(5));
        mkxp_retro::display_message(RETRO_LOG_ERROR, (const char *)sb()->str(SANDBOX_SLOT(0)));
        SANDBOX_AWAIT_S(8, rb_intern, "backtrace");
        SANDBOX_AWAIT_S(6, rb_funcall, exception, SANDBOX_SLOT(8), 0);
        SANDBOX_AWAIT_S(3, get_length, SANDBOX_SLOT(6));
        if (SANDBOX_SLOT(3) == 0) {
            SANDBOX_AWAIT_S(7, rb_str_new_cstr, "");
            SANDBOX_AWAIT(rb_ary_push, SANDBOX_SLOT(6), SANDBOX_SLOT(7));
            SANDBOX_SLOT(3) = 1;
        }
        for (SANDBOX_SLOT(4) = 0; SANDBOX_SLOT(4) < SANDBOX_SLOT(3); ++SANDBOX_SLOT(4)) {
            SANDBOX_AWAIT_S(7, rb_ary_entry, SANDBOX_SLOT(6), SANDBOX_SLOT(4));
            SANDBOX_AWAIT_S(0, rb_string_value_cstr, &SANDBOX_SLOT(7));
            if (SANDBOX_SLOT(4) == 0) {
                SANDBOX_AWAIT_S(1, rb_string_value_cstr, &SANDBOX_SLOT(5));
                SANDBOX_AWAIT_S(2, rb_obj_classname, exception);
                const struct sandbox_str_guard message = sb()->str(SANDBOX_SLOT(1));
                wasm_size_t message_len = sb()->strlen(SANDBOX_SLOT(1));
                const char *ptr = (const char *)message;
                const char *line_start = ptr;
                for (wasm_size_t i = 0; i < message_len;) {
                    if (++i == message_len || *ptr == '\n') {
                        ptrdiff_t size = *ptr == '\n' ? ptr - line_start : ptr - line_start + 1;
                        if (line_start == message) {
                            mkxp_retro_log_printf(RETRO_LOG_ERROR, "[mkxp-z exception] %s: %.*s (%s)\n", (const char *)sb()->str(SANDBOX_SLOT(0)), (int)std::min(size, (ptrdiff_t)INT_MAX), line_start, (const char *)sb()->str(SANDBOX_SLOT(2)));
                        } else {
                            mkxp_retro_log_printf(RETRO_LOG_ERROR, "[mkxp-z exception] %.*s\n", (int)std::min(size, (ptrdiff_t)INT_MAX), line_start);
                        }
                        line_start = ++ptr;
                    } else {
                        ++ptr;
                    }
                }
            } else {
                mkxp_retro_log_printf(RETRO_LOG_ERROR, "[mkxp-z exception]        from %s\n", (const char *)sb()->str(SANDBOX_SLOT(0)));
            }
        }
    }
}

VALUE mkxp_sandbox::mkxp_error_class;
VALUE mkxp_sandbox::physfs_error_class;
VALUE mkxp_sandbox::sdl_error_class;
VALUE mkxp_sandbox::rgss_error_class;
VALUE mkxp_sandbox::reset_class;
VALUE mkxp_sandbox::enoent_class;

void exception_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_R(mkxp_error_class, rb_define_class, "MKXPError", sb()->rb_eException());
        SANDBOX_AWAIT_R(physfs_error_class, rb_define_class, "PHYSFSError", sb()->rb_eException());
        SANDBOX_AWAIT_R(sdl_error_class, rb_define_class, "SDLError", sb()->rb_eException());
        SANDBOX_AWAIT_R(rgss_error_class, rb_define_class, "RGSSError", sb()->rb_eStandardError());
        SANDBOX_AWAIT_R(reset_class, rb_define_class, rgssVer >= 3 ? "RGSSReset" : "Reset", sb()->rb_eException());
        SANDBOX_AWAIT_S(0, rb_intern, "ENOENT");
        SANDBOX_AWAIT_R(enoent_class, rb_const_get, sb()->rb_mErrno(), SANDBOX_SLOT(0));
    }
}

void exception_raise::operator()(const Exception &exception) {
    BOOST_ASIO_CORO_REENTER (this) {
        if (exception.type == Exception::Ok) {
            return;
        }

        SANDBOX_AWAIT_S(0, rb_str_new_cstr, exception.msg.c_str());

        if (exception.type == Exception::RGSSError) {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), rgss_error_class);
        } else if (exception.type == Exception::Reset) {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), reset_class);
        } else if (exception.type == Exception::NoFileError) {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), enoent_class);
        } else if (exception.type == Exception::IOError) {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), sb()->rb_eIOError());
        } else if (exception.type == Exception::TypeError) {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), sb()->rb_eTypeError());
        } else if (exception.type == Exception::ArgumentError) {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), sb()->rb_eArgError());
        } else if (exception.type == Exception::SystemExit) {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), sb()->rb_eSystemExit());
        } else if (exception.type == Exception::RuntimeError) {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), sb()->rb_eRuntimeError());
        } else if (exception.type == Exception::PHYSFSError) {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), physfs_error_class);
        } else if (exception.type == Exception::SDLError) {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), sdl_error_class);
        } else {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), mkxp_error_class);
        }

        SANDBOX_AWAIT(rb_exc_raise, SANDBOX_SLOT(0));
    }
}

void check_arity::operator()(int32_t argc, int32_t min_argc, int32_t max_argc) {
    BOOST_ASIO_CORO_REENTER (this) {
        if (argc < min_argc || (max_argc != -1 && argc > max_argc)) {
            SANDBOX_AWAIT(rb_error_arity, argc, min_argc, max_argc);
        }
    }
}

void check_type::operator()(VALUE obj, VALUE klass) {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_S(0, rb_obj_is_kind_of, obj, klass);
        if (!SANDBOX_VALUE_TO_BOOL(SANDBOX_SLOT(0))) {
            SANDBOX_AWAIT_S(0, rb_str_new_cstr, "no implicit conversion of ");
            SANDBOX_AWAIT_S(1, rb_obj_class, obj);
            SANDBOX_AWAIT_S(1, rb_class_name, SANDBOX_SLOT(1));
            SANDBOX_AWAIT(rb_str_append, SANDBOX_SLOT(0), SANDBOX_SLOT(1));
            SANDBOX_AWAIT(rb_str_cat_cstr, SANDBOX_SLOT(0), " into ");
            SANDBOX_AWAIT_S(1, rb_class_name, klass);
            SANDBOX_AWAIT(rb_str_append, SANDBOX_SLOT(0), SANDBOX_SLOT(1));
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), sb()->rb_eTypeError());
            SANDBOX_AWAIT(rb_exc_raise, SANDBOX_SLOT(0));
        }
    }
}

void raise_disposed_access::operator()(VALUE obj) {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_S(1, rb_obj_class, obj);
        SANDBOX_AWAIT_S(1, rb_class_name, SANDBOX_SLOT(1));
        SANDBOX_AWAIT_S(2, rb_intern, "downcase!");
        SANDBOX_AWAIT(rb_funcall, SANDBOX_SLOT(1), SANDBOX_SLOT(2), 0);
        SANDBOX_AWAIT_S(0, rb_str_new_cstr, "disposed ");
        SANDBOX_AWAIT(rb_str_append, SANDBOX_SLOT(0), SANDBOX_SLOT(1));
        SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), rgss_error_class);
        SANDBOX_AWAIT(rb_exc_raise, SANDBOX_SLOT(0));
    }
}
