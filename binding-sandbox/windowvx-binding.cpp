/*
** windowvx-binding.cpp
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

#include "windowvx-binding.h"
#include "bitmap-binding.h"
#include "disposable-binding.h"
#include "etc-binding.h"
#include "viewportelement-binding.h"
#include "bitmap.h"
#include "windowvx.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::windowvx_class;
static struct bindings::rb_data_type windowvx_type;

SANDBOX_DEF_ALLOC(windowvx_type);

static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, int32_t, int32_t, int32_t, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                GFX_LOCK;

                SANDBOX_SLOT(1) = SANDBOX_SLOT(2) = SANDBOX_SLOT(3) = SANDBOX_SLOT(4) = 0;

                if (rgssVer >= 3) {
                    if (argc == 4) {
                        SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 0));
                        SANDBOX_AWAIT_S(2, rb_num2int, sb()->ref<VALUE>(argv, 1));
                        SANDBOX_AWAIT_S(3, rb_num2int, sb()->ref<VALUE>(argv, 2));
                        SANDBOX_AWAIT_S(4, rb_num2int, sb()->ref<VALUE>(argv, 3));
                    }
                    WindowVX *window = new WindowVX(SANDBOX_SLOT(1), SANDBOX_SLOT(2), SANDBOX_SLOT(3), SANDBOX_SLOT(4));
                    set_private_data(self, window);
                    window->initDynAttribs();
                } else {
                    SANDBOX_AWAIT(viewportelement_initialize<WindowVX>, argc, argv, self);
                }

                SANDBOX_GUARD(SANDBOX_AWAIT(wrap_property, self, &get_private_data<WindowVX>(self)->getCursorRect(sb().e), "cursor_rect", rect_class));

                if (rgssVer >= 3) {
                    SANDBOX_GUARD(SANDBOX_AWAIT(wrap_property, self, &get_private_data<WindowVX>(self)->getTone(sb().e), "tone", tone_class));
                }

                SANDBOX_AWAIT_S(0, rb_obj_alloc, bitmap_class);
                SANDBOX_GUARD(set_private_data(sb().e, SANDBOX_SLOT(0), new Bitmap(sb().e, 1, 1)));
                SANDBOX_AWAIT(bitmap_init_props, SANDBOX_SLOT(0));
                SANDBOX_AWAIT(rb_iv_set, self, "contents", SANDBOX_SLOT(0));
            }

            return SANDBOX_NIL;
        }

        void end() noexcept {
            GFX_UNLOCK;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE update(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD_L(get_private_data<WindowVX>(self)->update(sb().e));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

SANDBOX_DEF_GFX_PROP_OBJ_REF(WindowVX, Bitmap, bitmap_class, Windowskin, windowskin);
SANDBOX_DEF_GFX_PROP_OBJ_REF(WindowVX, Bitmap, bitmap_class, Contents, contents);
SANDBOX_DEF_GFX_PROP_OBJ_VAL(WindowVX, Rect, rect_class, CursorRect, cursor_rect);
SANDBOX_DEF_GFX_PROP_B(WindowVX, Active, active);
SANDBOX_DEF_GFX_PROP_B(WindowVX, Pause, pause);
SANDBOX_DEF_GFX_PROP_I(WindowVX, X, x);
SANDBOX_DEF_GFX_PROP_I(WindowVX, Y, y);
SANDBOX_DEF_GFX_PROP_I(WindowVX, Width, width);
SANDBOX_DEF_GFX_PROP_I(WindowVX, Height, height);
SANDBOX_DEF_GFX_PROP_I(WindowVX, OX, ox);
SANDBOX_DEF_GFX_PROP_I(WindowVX, OY, oy);
SANDBOX_DEF_GFX_PROP_I(WindowVX, Opacity, opacity);
SANDBOX_DEF_GFX_PROP_I(WindowVX, BackOpacity, back_opacity);
SANDBOX_DEF_GFX_PROP_I(WindowVX, ContentsOpacity, contents_opacity);
SANDBOX_DEF_GFX_PROP_I(WindowVX, Openness, openness);

static VALUE move(VALUE self, VALUE x, VALUE y, VALUE w, VALUE h) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t, int32_t, int32_t, int32_t> slots;

        VALUE operator()(VALUE self, VALUE x, VALUE y, VALUE w, VALUE h) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_num2int, x);
                SANDBOX_AWAIT_S(1, rb_num2int, y);
                SANDBOX_AWAIT_S(2, rb_num2int, w);
                SANDBOX_AWAIT_S(3, rb_num2int, h);
                SANDBOX_GUARD_L(get_private_data<WindowVX>(self)->move(sb().e, SANDBOX_SLOT(0), SANDBOX_SLOT(1), SANDBOX_SLOT(2), SANDBOX_SLOT(3)));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, x, y, w, h);
}

static VALUE is_open(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<uint8_t> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD(SANDBOX_SLOT(0) = get_private_data<WindowVX>(self)->isOpen(sb().e));
            }

            return SANDBOX_BOOL_TO_VALUE(SANDBOX_SLOT(0));
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE is_closed(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<uint8_t> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD(SANDBOX_SLOT(0) = get_private_data<WindowVX>(self)->isClosed(sb().e));
            }

            return SANDBOX_BOOL_TO_VALUE(SANDBOX_SLOT(0));
        }
    };

    return sb()->bind<struct coro>()()(self);
}

SANDBOX_DEF_GFX_PROP_B(WindowVX, ArrowsVisible, arrows_visible);
SANDBOX_DEF_GFX_PROP_I(WindowVX, Padding, padding);
SANDBOX_DEF_GFX_PROP_I(WindowVX, PaddingBottom, padding_bottom);
SANDBOX_DEF_GFX_PROP_OBJ_VAL(WindowVX, Tone, tone_class, Tone, tone);

void windowvx_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        windowvx_type = sb()->rb_data_type("Window", nullptr, dfree, nullptr, nullptr, 0, 0, 0);
        SANDBOX_AWAIT_R(windowvx_class, rb_define_class, "Window", sb()->rb_cObject());
        SANDBOX_AWAIT(rb_define_alloc_func, windowvx_class, alloc);
        SANDBOX_AWAIT(rb_define_method, windowvx_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
        SANDBOX_AWAIT(disposable_binding_init<WindowVX>, windowvx_class);
        SANDBOX_AWAIT(viewportelement_binding_init<WindowVX>, windowvx_class);

        SANDBOX_AWAIT(rb_define_method, windowvx_class, "update", (VALUE (*)(ANYARGS))update, 0);

        SANDBOX_INIT_PROP_BIND(windowvx_class, windowskin);
        SANDBOX_INIT_PROP_BIND(windowvx_class, contents);
        SANDBOX_INIT_PROP_BIND(windowvx_class, cursor_rect);
        SANDBOX_INIT_PROP_BIND(windowvx_class, active);
        SANDBOX_INIT_PROP_BIND(windowvx_class, pause);
        SANDBOX_INIT_PROP_BIND(windowvx_class, x);
        SANDBOX_INIT_PROP_BIND(windowvx_class, y);
        SANDBOX_INIT_PROP_BIND(windowvx_class, width);
        SANDBOX_INIT_PROP_BIND(windowvx_class, height);
        SANDBOX_INIT_PROP_BIND(windowvx_class, ox);
        SANDBOX_INIT_PROP_BIND(windowvx_class, oy);
        SANDBOX_INIT_PROP_BIND(windowvx_class, opacity);
        SANDBOX_INIT_PROP_BIND(windowvx_class, back_opacity);
        SANDBOX_INIT_PROP_BIND(windowvx_class, contents_opacity);
        SANDBOX_INIT_PROP_BIND(windowvx_class, openness);

        if (rgssVer >= 3) {
            SANDBOX_AWAIT(rb_define_method, windowvx_class, "move", (VALUE (*)(ANYARGS))move, 4);
            SANDBOX_AWAIT(rb_define_method, windowvx_class, "open?", (VALUE (*)(ANYARGS))is_open, 0);
            SANDBOX_AWAIT(rb_define_method, windowvx_class, "close?", (VALUE (*)(ANYARGS))is_closed, 0);

            SANDBOX_INIT_PROP_BIND(windowvx_class, arrows_visible);
            SANDBOX_INIT_PROP_BIND(windowvx_class, padding);
            SANDBOX_INIT_PROP_BIND(windowvx_class, padding_bottom);
            SANDBOX_INIT_PROP_BIND(windowvx_class, tone);
        }
    }
}
