/*
** input-binding.cpp
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

#include "input-binding.h"
#include "input.h"
#include "core.h"
#include "sharedstate.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::input_module;
VALUE mkxp_sandbox::input_controller_module;

static VALUE symhash;

struct {
    const char *str;
    Input::ButtonCode val;
} static codes[] = {
    {"DOWN", Input::Down},
    {"LEFT", Input::Left},
    {"RIGHT", Input::Right},
    {"UP", Input::Up},
    {"C", Input::C},
    {"Z", Input::Z},
    {"A", Input::A},
    {"B", Input::B},
    {"X", Input::X},
    {"Y", Input::Y},
    {"L", Input::L},
    {"R", Input::R},
    {"SHIFT", Input::Shift},
    {"CTRL", Input::Ctrl},
    {"ALT", Input::Alt},
    {"F5", Input::F5},
    {"F6", Input::F6},
    {"F7", Input::F7},
    {"F8", Input::F8},
    {"F9", Input::F9},
    {"MOUSELEFT", Input::MouseLeft},
    {"MOUSEMIDDLE", Input::MouseMiddle},
    {"MOUSERIGHT", Input::MouseRight},
    {"MOUSEX1", Input::MouseX1},
    {"MOUSEX2", Input::MouseX2},
};

struct get_button_arg : boost::asio::coroutine {
    typedef decl_slots<VALUE, int32_t> slots;

    int32_t operator()(VALUE arg) {
        BOOST_ASIO_CORO_REENTER (this) {
            SANDBOX_AWAIT_S(0, rb_obj_is_kind_of, arg, sb()->rb_cInteger());
            if (SANDBOX_VALUE_TO_BOOL(SANDBOX_SLOT(0))) {
                SANDBOX_AWAIT_S(1, rb_num2int, arg);
            } else {
                SANDBOX_AWAIT_S(0, rb_obj_is_kind_of, arg, sb()->rb_cSymbol());
                if (SANDBOX_VALUE_TO_BOOL(SANDBOX_SLOT(0)) && rgssVer >= 3) {
                    SANDBOX_AWAIT_S(0, rb_ll2inum, Input::None);
                    SANDBOX_AWAIT_S(0, rb_hash_lookup2, symhash, arg, SANDBOX_SLOT(0));
                    SANDBOX_AWAIT_S(1, rb_num2int, SANDBOX_SLOT(0));
                } else {
                    // FIXME: RMXP allows only few more types that
                    // don't make sense (symbols in pre 3, floats)
                    SANDBOX_SLOT(1) = 0;
                }
            }
        }

        return SANDBOX_SLOT(1);
    }
};

static VALUE delta(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<double, VALUE> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_SLOT(0) = mkxp_retro::input->getDelta();
                SANDBOX_AWAIT_S(1, rb_float_new, SANDBOX_SLOT(0));
            }

            return SANDBOX_SLOT(1);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE update(VALUE self) {
    mkxp_retro::input->update();
    return SANDBOX_NIL;
}

static VALUE press(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t> slots;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, get_button_arg, code);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->isPressed(SANDBOX_SLOT(0)));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE trigger(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t> slots;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, get_button_arg, code);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->isTriggered(SANDBOX_SLOT(0)));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE repeat(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t> slots;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, get_button_arg, code);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->isRepeated(SANDBOX_SLOT(0)));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE release(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t> slots;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, get_button_arg, code);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->isReleased(SANDBOX_SLOT(0)));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE count(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, int32_t, int32_t> slots;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(1, get_button_arg, code);
                SANDBOX_SLOT(2) = mkxp_retro::input->count(SANDBOX_SLOT(1));
                SANDBOX_AWAIT_S(0, rb_ll2inum, SANDBOX_SLOT(2));
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE time_(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<double, VALUE, int32_t> slots;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(2, get_button_arg, code);
                SANDBOX_SLOT(0) = mkxp_retro::input->repeatTime(SANDBOX_SLOT(2));
                SANDBOX_AWAIT_S(1, rb_float_new, SANDBOX_SLOT(0));
            }

            return SANDBOX_SLOT(1);
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE pressex(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t> slots;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, get_button_arg, code);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->isPressedEx(SANDBOX_SLOT(0), false));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE triggerex(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t> slots;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, get_button_arg, code);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->isTriggeredEx(SANDBOX_SLOT(0), false));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE repeatex(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t> slots;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, get_button_arg, code);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->isRepeatedEx(SANDBOX_SLOT(0), false));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE releaseex(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t> slots;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, get_button_arg, code);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->isReleasedEx(SANDBOX_SLOT(0), false));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE repeatcount(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, int32_t, int32_t> slots;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(1, get_button_arg, code);
                SANDBOX_SLOT(2) = mkxp_retro::input->repeatcount(SANDBOX_SLOT(1), false);
                SANDBOX_AWAIT_S(0, rb_ll2inum, SANDBOX_SLOT(2));
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE timeex(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<double, VALUE, int32_t> slots;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(2, get_button_arg, code);
                SANDBOX_SLOT(0) = mkxp_retro::input->repeatTimeEx(SANDBOX_SLOT(2), false);
                SANDBOX_AWAIT_S(1, rb_float_new, SANDBOX_SLOT(0));
            }

            return SANDBOX_SLOT(1);
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE dir4(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(mkxp_retro::input->dir4Value());
}

static VALUE dir8(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(mkxp_retro::input->dir8Value());
}

static VALUE mouse_x(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(mkxp_retro::input->mouseX());
}

static VALUE mouse_y(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(mkxp_retro::input->mouseY());
}

static VALUE scroll_v(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(mkxp_retro::input->scrollV());
}

static VALUE mouse_in_window(VALUE self) {
    return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->mouseInWindow());
}

static VALUE raw_key_states(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, uint32_t, uint32_t> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_SLOT(2) = mkxp_retro::input->rawKeyStatesLength();
                SANDBOX_AWAIT_S(0, rb_ary_new_capa, SANDBOX_SLOT(2));
                for (SANDBOX_SLOT(1) = 0; SANDBOX_SLOT(1) < SANDBOX_SLOT(2); ++SANDBOX_SLOT(1)) {
                    SANDBOX_AWAIT(rb_ary_push, SANDBOX_SLOT(0), SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->rawKeyStates()[SANDBOX_SLOT(1)]));
                }
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE controller_connected(VALUE self) {
    return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->getControllerConnected());
}

static VALUE controller_name(VALUE self) {
    return sb()->bind<struct rb_utf8_str_new_cstr>()()(mkxp_retro::input->getControllerName());
}

#define POWERCASE(power) \
    if (SANDBOX_SLOT(2) == (int32_t)SDL_JOYSTICK_POWER_##power) { \
        SANDBOX_AWAIT_S(1, rb_intern, #power); \
        SANDBOX_AWAIT_S(0, rb_id2sym, SANDBOX_SLOT(1)); \
        return SANDBOX_SLOT(0); \
    }

static VALUE controller_power_level(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, ID, int32_t> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_SLOT(2) = mkxp_retro::input->getControllerPowerLevel();
                POWERCASE(UNKNOWN);
                POWERCASE(EMPTY);
                POWERCASE(LOW);
                POWERCASE(MEDIUM);
                POWERCASE(FULL);
                POWERCASE(WIRED);
                SANDBOX_SLOT(0) = SANDBOX_NIL;
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

#define DEF_AXES(enum1, enum2, name) \
    static VALUE controller_axes_##name(VALUE self) { \
        struct coro : boost::asio::coroutine { \
            typedef decl_slots<VALUE, VALUE> slots; \
            VALUE operator()(VALUE self) { \
                BOOST_ASIO_CORO_REENTER (this) { \
                    SANDBOX_AWAIT_S(0, rb_ary_new_capa, 2); \
                    SANDBOX_AWAIT_S(1, rb_ll2inum, mkxp_retro::input->getControllerAxisValue(SDL_CONTROLLER_AXIS_##enum1)); \
                    SANDBOX_AWAIT(rb_ary_push, SANDBOX_SLOT(0), SANDBOX_SLOT(1)); \
                    SANDBOX_AWAIT_S(1, rb_ll2inum, mkxp_retro::input->getControllerAxisValue(SDL_CONTROLLER_AXIS_##enum2)); \
                    SANDBOX_AWAIT(rb_ary_push, SANDBOX_SLOT(0), SANDBOX_SLOT(1)); \
                } \
                return SANDBOX_SLOT(0); \
            } \
        }; \
        return sb()->bind<struct coro>()()(self); \
    }

DEF_AXES(LEFTX, LEFTY, left);
DEF_AXES(RIGHTX, RIGHTY, right);
DEF_AXES(TRIGGERLEFT, TRIGGERRIGHT, trigger);

static VALUE controller_raw_button_states(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, uint32_t, uint32_t> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_SLOT(2) = mkxp_retro::input->rawButtonStatesLength();
                SANDBOX_AWAIT_S(0, rb_ary_new_capa, SANDBOX_SLOT(2));
                for (SANDBOX_SLOT(1) = 0; SANDBOX_SLOT(1) < SANDBOX_SLOT(2); ++SANDBOX_SLOT(1)) {
                    SANDBOX_AWAIT(rb_ary_push, SANDBOX_SLOT(0), SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->rawButtonStates()[SANDBOX_SLOT(1)]));
                }
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE controller_raw_axes(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, VALUE, uint32_t, uint32_t> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_SLOT(3) = mkxp_retro::input->rawAxesLength();
                SANDBOX_AWAIT_S(0, rb_ary_new_capa, SANDBOX_SLOT(3));
                for (SANDBOX_SLOT(2) = 0; SANDBOX_SLOT(2) < SANDBOX_SLOT(3); ++SANDBOX_SLOT(2)) {
                    SANDBOX_AWAIT_S(1, rb_float_new, mkxp_retro::input->rawAxes()[SANDBOX_SLOT(2)] / 32767.0);
                    SANDBOX_AWAIT(rb_ary_push, SANDBOX_SLOT(0), SANDBOX_SLOT(1));
                }
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE controller_pressex(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t> slots;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, get_button_arg, code);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->controllerIsPressedEx(SANDBOX_SLOT(0)));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE controller_triggerex(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t> slots;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, get_button_arg, code);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->controllerIsTriggeredEx(SANDBOX_SLOT(0)));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE controller_repeatex(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t> slots;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, get_button_arg, code);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->controllerIsRepeatedEx(SANDBOX_SLOT(0)));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE controller_releaseex(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t> slots;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, get_button_arg, code);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->controllerIsReleasedEx(SANDBOX_SLOT(0)));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE controller_repeatcount(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, int32_t, int32_t> slots;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(1, get_button_arg, code);
                SANDBOX_SLOT(2) = mkxp_retro::input->controllerRepeatcount(SANDBOX_SLOT(1));
                SANDBOX_AWAIT_S(0, rb_ll2inum, SANDBOX_SLOT(2));
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE controller_timeex(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<double, VALUE, int32_t> slots;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(2, get_button_arg, code);
                SANDBOX_SLOT(0) = mkxp_retro::input->controllerRepeatTimeEx(SANDBOX_SLOT(2));
                SANDBOX_AWAIT_S(1, rb_float_new, SANDBOX_SLOT(0));
            }

            return SANDBOX_SLOT(1);
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE get_text_input(VALUE self) {
    return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->getTextInputMode());
}

static VALUE set_text_input(VALUE self, VALUE value) {
    mkxp_retro::input->setTextInputMode(value);
    return value;
}

static VALUE gets_(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_utf8_str_new_cstr, mkxp_retro::input->getText());
                mkxp_retro::input->clearText();
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE get_clipboard(VALUE self) {
    return sb()->bind<struct rb_str_new_cstr>()()(mkxp_retro::input->getClipboardText().c_str());
}

static VALUE set_clipboard(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t> slots;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_string_value_cstr, &value);
                mkxp_retro::input->setClipboardText(sb()->str(SANDBOX_SLOT(0)));
            }

            return value;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

void input_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_R(input_module, rb_define_module, "Input");

        SANDBOX_AWAIT(rb_define_module_function, input_module, "delta", (VALUE (*)(ANYARGS))delta, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "update", (VALUE (*)(ANYARGS))update, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "press?", (VALUE (*)(ANYARGS))press, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "trigger?", (VALUE (*)(ANYARGS))trigger, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "repeat?", (VALUE (*)(ANYARGS))repeat, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "release?", (VALUE (*)(ANYARGS))release, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "count", (VALUE (*)(ANYARGS))count, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "time?", (VALUE (*)(ANYARGS))time_, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "pressex?", (VALUE (*)(ANYARGS))pressex, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "triggerex?", (VALUE (*)(ANYARGS))triggerex, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "repeatex?", (VALUE (*)(ANYARGS))repeatex, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "releaseex?", (VALUE (*)(ANYARGS))releaseex, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "repeatcount", (VALUE (*)(ANYARGS))repeatcount, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "timeex?", (VALUE (*)(ANYARGS))timeex, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "dir4", (VALUE (*)(ANYARGS))dir4, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "dir8", (VALUE (*)(ANYARGS))dir8, 0);

        SANDBOX_AWAIT(rb_define_module_function, input_module, "mouse_x", (VALUE (*)(ANYARGS))mouse_x, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "mouse_y", (VALUE (*)(ANYARGS))mouse_y, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "scroll_v", (VALUE (*)(ANYARGS))scroll_v, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "mouse_in_window", (VALUE (*)(ANYARGS))mouse_in_window, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "mouse_in_window?", (VALUE (*)(ANYARGS))mouse_in_window, 0);

        SANDBOX_AWAIT(rb_define_module_function, input_module, "raw_key_states", (VALUE (*)(ANYARGS))raw_key_states, 0);

        SANDBOX_AWAIT_R(input_controller_module, rb_define_module_under, input_module, "Controller");
        SANDBOX_AWAIT(rb_define_module_function, input_controller_module, "connected?", (VALUE (*)(ANYARGS))controller_connected, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_controller_module, "name", (VALUE (*)(ANYARGS))controller_name, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_controller_module, "power_level", (VALUE (*)(ANYARGS))controller_power_level, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_controller_module, "axes_left", (VALUE (*)(ANYARGS))controller_axes_left, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_controller_module, "axes_right", (VALUE (*)(ANYARGS))controller_axes_right, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_controller_module, "axes_trigger", (VALUE (*)(ANYARGS))controller_axes_trigger, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_controller_module, "raw_button_states", (VALUE (*)(ANYARGS))controller_raw_button_states, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_controller_module, "raw_axes", (VALUE (*)(ANYARGS))controller_raw_axes, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_controller_module, "pressex?", (VALUE (*)(ANYARGS))controller_pressex, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_controller_module, "triggerex?", (VALUE (*)(ANYARGS))controller_triggerex, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_controller_module, "repeatex?", (VALUE (*)(ANYARGS))controller_repeatex, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_controller_module, "releaseex?", (VALUE (*)(ANYARGS))controller_releaseex, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_controller_module, "repeatcount", (VALUE (*)(ANYARGS))controller_repeatcount, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_controller_module, "timeex?", (VALUE (*)(ANYARGS))controller_timeex, 1);

        SANDBOX_INIT_MODULE_PROP_BIND(input_module, text_input);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "gets", (VALUE (*)(ANYARGS))gets_, 0);

        SANDBOX_INIT_MODULE_PROP_BIND(input_module, clipboard);

        if (rgssVer >= 3) {
            SANDBOX_AWAIT_R(symhash, rb_hash_new);

            for (SANDBOX_SLOT(0) = 0; SANDBOX_SLOT(0) < sizeof(codes) / sizeof(*codes); ++SANDBOX_SLOT(0)) {
                SANDBOX_AWAIT_S(3, rb_intern, codes[SANDBOX_SLOT(0)].str);
                SANDBOX_AWAIT_S(1, rb_ll2inum, codes[SANDBOX_SLOT(0)].val);

                /* In RGSS3 all Input::XYZ constants are equal to :XYZ symbols,
                 * to be compatible with the previous convention */
                SANDBOX_AWAIT_S(2, rb_id2sym, SANDBOX_SLOT(3));
                SANDBOX_AWAIT(rb_const_set, input_module, SANDBOX_SLOT(3), SANDBOX_SLOT(2));
                SANDBOX_AWAIT(rb_hash_aset, symhash, SANDBOX_SLOT(2), SANDBOX_SLOT(1));
            }

            SANDBOX_AWAIT(rb_iv_set, input_module, "buttoncodes", symhash);
        } else {
            for (SANDBOX_SLOT(0) = 0; SANDBOX_SLOT(0) < sizeof(codes) / sizeof(*codes); ++SANDBOX_SLOT(0)) {
                SANDBOX_AWAIT_S(3, rb_intern, codes[SANDBOX_SLOT(0)].str);
                SANDBOX_AWAIT_S(1, rb_ll2inum, codes[SANDBOX_SLOT(0)].val);
                SANDBOX_AWAIT(rb_const_set, input_module, SANDBOX_SLOT(3), SANDBOX_SLOT(1));
            }
        }
    }
}
