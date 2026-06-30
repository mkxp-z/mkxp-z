/*
** graphics-binding.cpp
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

#include "graphics-binding.h"
#include "bitmap-binding.h"
#include "bitmap.h"
#include "graphics.h"
#include "sharedstate.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::graphics_module;

void graphics_reset::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_GUARD_L(shState->graphics().reset(sb().e));
    }
}

static VALUE delta(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<double, VALUE> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                GFX_LOCK;
                SANDBOX_SLOT(0) = shState->graphics().getDelta();
                GFX_UNLOCK;
                SANDBOX_AWAIT_S(1, rb_float_new, SANDBOX_SLOT(0));
            }

            return SANDBOX_SLOT(1);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE update(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                bool needs_yield;
                SANDBOX_GUARD(needs_yield = shState->graphics().update(sb().e));
                if (needs_yield) {
                    SANDBOX_YIELD;
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE freeze(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD_L(shState->graphics().freeze(sb().e));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE transition(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t, int32_t, int32_t, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                sb().trans_map = nullptr;
                sb().transitioning = true;
                SANDBOX_SLOT(1) = 8;
                SANDBOX_SLOT(2) = 40;

                GFX_LOCK;

                if (!shState->graphics().frozen()) {
                    return SANDBOX_NIL;
                }

                if (argc >= 1) {
                    SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 0));
                    if (argc >= 2) {
                        SANDBOX_AWAIT_S(0, rb_string_value_cstr, &sb()->ref<VALUE>(argv, 1));
                        if (*sb()->str(SANDBOX_SLOT(0))) {
                            SANDBOX_GUARD(sb().trans_map = new Bitmap(sb().e, sb()->str(SANDBOX_SLOT(0))));
                        }
                        if (argc >= 3) {
                            SANDBOX_AWAIT_S(2, rb_num2int, sb()->ref<VALUE>(argv, 2));
                        }
                    }
                }

                if (SANDBOX_SLOT(1) <= 0) {
                    SANDBOX_GUARD(shState->graphics().transition(sb().e, SANDBOX_SLOT(1), sb().trans_map, SANDBOX_SLOT(2), 0, 0));
                } else {
                    for (SANDBOX_SLOT(3) = 0; SANDBOX_SLOT(3) < SANDBOX_SLOT(1); ++SANDBOX_SLOT(3)) {
                        SANDBOX_GUARD(shState->graphics().transition(sb().e, SANDBOX_SLOT(1), sb().trans_map, SANDBOX_SLOT(2), SANDBOX_SLOT(3), SANDBOX_SLOT(3)));
                        SANDBOX_YIELD;
                    }
                }
            }

            return SANDBOX_NIL;
        }

        void end() noexcept {
            GFX_UNLOCK;
            sb().transitioning = false;
            if (sb().trans_map != nullptr) {
                delete sb().trans_map;
                sb().trans_map = nullptr;
            }
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE frame_reset(VALUE self) {
    GFX_LOCK;
    shState->graphics().frameReset();
    GFX_UNLOCK;
    return SANDBOX_NIL;
}

static VALUE screenshot(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t> slots;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_string_value_cstr, &value);
                SANDBOX_GUARD_L(shState->graphics().screenshot(sb().e, sb()->str(SANDBOX_SLOT(0))));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

SANDBOX_DEF_GRA_PROP_I(FrameCount, frame_count);
SANDBOX_DEF_GRA_PROP_I(FrameRate, frame_rate);

static VALUE average_frame_rate(VALUE self) {
    return sb()->bind<struct rb_float_new>()()(shState->graphics().averageFrameRate());
}

static VALUE width(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(shState->graphics().width());
}

static VALUE height(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(shState->graphics().height());
}

static VALUE display_width(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(shState->graphics().displayWidth());
}

static VALUE display_height(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(shState->graphics().displayHeight());
}

static VALUE wait_(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t, int32_t> slots;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                GFX_LOCK;

                SANDBOX_AWAIT_S(0, rb_num2int, value);

                for (SANDBOX_SLOT(1) = 0; SANDBOX_SLOT(1) < SANDBOX_SLOT(0); ++SANDBOX_SLOT(1)) {
                    SANDBOX_GUARD(shState->graphics().wait(sb().e, SANDBOX_SLOT(0), SANDBOX_SLOT(1), SANDBOX_SLOT(1)));
                    SANDBOX_YIELD;
                }
            }

            return SANDBOX_NIL;
        }

        void end() noexcept {
            GFX_UNLOCK;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE fadeout(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t, int32_t, int32_t> slots;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                GFX_LOCK;

                SANDBOX_AWAIT_S(0, rb_num2int, value);
                SANDBOX_SLOT(2) = shState->graphics().getBrightness();

                for (SANDBOX_SLOT(1) = 0; SANDBOX_SLOT(1) < SANDBOX_SLOT(0); ++SANDBOX_SLOT(1)) {
                    SANDBOX_GUARD(shState->graphics().fadeout(sb().e, SANDBOX_SLOT(0), SANDBOX_SLOT(1), SANDBOX_SLOT(1), SANDBOX_SLOT(2)));
                    SANDBOX_YIELD;
                }
            }

            return SANDBOX_NIL;
        }

        void end() noexcept {
            GFX_UNLOCK;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE fadein(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t, int32_t, int32_t> slots;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                GFX_LOCK;

                SANDBOX_AWAIT_S(0, rb_num2int, value);
                SANDBOX_SLOT(2) = shState->graphics().getBrightness();

                for (SANDBOX_SLOT(1) = 0; SANDBOX_SLOT(1) < SANDBOX_SLOT(0); ++SANDBOX_SLOT(1)) {
                    SANDBOX_GUARD(shState->graphics().fadein(sb().e, SANDBOX_SLOT(0), SANDBOX_SLOT(1), SANDBOX_SLOT(1), SANDBOX_SLOT(2)));
                    SANDBOX_YIELD;
                }
            }

            return SANDBOX_NIL;
        }

        void end() noexcept {
            GFX_UNLOCK;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE snap_to_bitmap(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_obj_alloc, bitmap_class);
                SANDBOX_GUARD_L(set_private_data(sb().e, SANDBOX_SLOT(0), shState->graphics().snapToBitmap(sb().e)));
                SANDBOX_AWAIT(bitmap_init_props, SANDBOX_SLOT(0));
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE resize_screen(VALUE self, VALUE width, VALUE height) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t, int32_t> slots;

        VALUE operator()(VALUE self, VALUE width, VALUE height) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_num2int, width);
                SANDBOX_AWAIT_S(1, rb_num2int, height);
                GFX_LOCK;
                shState->graphics().resizeScreen(SANDBOX_SLOT(0), SANDBOX_SLOT(1));
                GFX_UNLOCK;
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, width, height);
}

static VALUE resize_window(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(check_arity, argc, 2, -1);

                SANDBOX_AWAIT_S(0, rb_num2int, sb()->ref<VALUE>(argv, 0));
                SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 1));
                GFX_LOCK;
                if (argc >= 3) {
                    shState->graphics().resizeWindow(SANDBOX_SLOT(0), SANDBOX_SLOT(1), SANDBOX_VALUE_TO_BOOL(sb()->ref<VALUE>(argv, 2)));
                } else {
                    shState->graphics().resizeWindow(SANDBOX_SLOT(0), SANDBOX_SLOT(1));
                }
                GFX_UNLOCK;
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE center(VALUE self) {
    shState->graphics().center();
    return SANDBOX_NIL;
}

SANDBOX_DEF_GRA_PROP_I(Brightness, brightness);

static VALUE play_movie(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t, int32_t, uint8_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                GFX_LOCK;

                SANDBOX_AWAIT(check_arity, argc, 1, -1);

                SANDBOX_AWAIT_S(0, rb_string_value_cstr, &sb()->ref<VALUE>(argv, 0));
                if (argc >= 2) {
                    SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 1));
                } else {
                    SANDBOX_SLOT(1) = 100;
                }
                SANDBOX_SLOT(2) = argc >= 3 ? SANDBOX_VALUE_TO_BOOL(sb()->ref<VALUE>(argv, 2)) : false;

                SANDBOX_GUARD(sb().set_movie(shState->graphics().playMovie(sb().e, sb()->str(SANDBOX_SLOT(0)), SANDBOX_SLOT(1), SANDBOX_SLOT(2))));
                while (sb().get_movie_from_main_thread() != nullptr) {
                    SANDBOX_YIELD;
                    sb().set_movie(shState->graphics().playMovie(sb().get_movie_from_main_thread()));
                }
            }

            return SANDBOX_NIL;
        }

        void end() noexcept {
            sb().set_movie(nullptr);
            GFX_UNLOCK;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

SANDBOX_DEF_GRA_PROP_B(Fullscreen, fullscreen);
SANDBOX_DEF_GRA_PROP_B(ShowCursor, show_cursor);
SANDBOX_DEF_GRA_PROP_D(Scale, scale);
SANDBOX_DEF_GRA_PROP_B(Frameskip, frameskip);
SANDBOX_DEF_GRA_PROP_B(FixedAspectRatio, fixed_aspect_ratio);
SANDBOX_DEF_GRA_PROP_B(SmoothScaling, smooth_scaling);
SANDBOX_DEF_GRA_PROP_B(IntegerScaling, integer_scaling);
SANDBOX_DEF_GRA_PROP_B(LastMileScaling, last_mile_scaling);
SANDBOX_DEF_GRA_PROP_B(Threadsafe, threadsafe);

void graphics_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_R(graphics_module, rb_define_module, "Graphics");
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "delta", (VALUE (*)(ANYARGS))delta, 0);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "update", (VALUE (*)(ANYARGS))update, 0);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "freeze", (VALUE (*)(ANYARGS))freeze, 0);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "transition", (VALUE (*)(ANYARGS))transition, -1);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "frame_reset", (VALUE (*)(ANYARGS))frame_reset, 0);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "screenshot", (VALUE (*)(ANYARGS))screenshot, 1);

        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, frame_count);
        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, frame_rate);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "average_frame_rate", (VALUE (*)(ANYARGS))average_frame_rate, 0);

        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "width", (VALUE (*)(ANYARGS))width, 0);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "height", (VALUE (*)(ANYARGS))height, 0);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "display_width", (VALUE (*)(ANYARGS))display_width, 0);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "display_height", (VALUE (*)(ANYARGS))display_height, 0);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "wait", (VALUE (*)(ANYARGS))wait_, 1);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "fadeout", (VALUE (*)(ANYARGS))fadeout, 1);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "fadein", (VALUE (*)(ANYARGS))fadein, 1);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "snap_to_bitmap", (VALUE (*)(ANYARGS))snap_to_bitmap, 0);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "resize_screen", (VALUE (*)(ANYARGS))resize_screen, 2);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "resize_window", (VALUE (*)(ANYARGS))resize_window, -1);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "center", (VALUE (*)(ANYARGS))center, 0);

        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, brightness);

        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "play_movie", (VALUE (*)(ANYARGS))play_movie, -1);

        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, fullscreen);
        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, show_cursor);
        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, scale);
        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, frameskip);
        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, fixed_aspect_ratio);
        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, smooth_scaling);
        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, integer_scaling);
        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, last_mile_scaling);
        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, threadsafe);
    }
}
