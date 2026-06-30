/*
** window-binding.cpp
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

#include "window-binding.h"
#include "bitmap-binding.h"
#include "disposable-binding.h"
#include "etc-binding.h"
#include "viewportelement-binding.h"
#include "window.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::window_class;
static struct bindings::rb_data_type window_type;

SANDBOX_DEF_ALLOC(window_type);

static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                GFX_LOCK;

                SANDBOX_AWAIT(viewportelement_initialize<Window>, argc, argv, self);
                SANDBOX_GUARD(SANDBOX_AWAIT(wrap_property, self, &get_private_data<Window>(self)->getCursorRect(sb().e), "cursor_rect", rect_class));
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
                SANDBOX_GUARD_L(get_private_data<Window>(self)->update(sb().e));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

SANDBOX_DEF_GFX_PROP_OBJ_REF(Window, Bitmap, bitmap_class, Windowskin, windowskin);
SANDBOX_DEF_GFX_PROP_OBJ_REF(Window, Bitmap, bitmap_class, Contents, contents);
SANDBOX_DEF_GFX_PROP_B(Window, Stretch, stretch);
SANDBOX_DEF_GFX_PROP_OBJ_VAL(Window, Rect, rect_class, CursorRect, cursor_rect);
SANDBOX_DEF_GFX_PROP_B(Window, Active, active);
SANDBOX_DEF_GFX_PROP_B(Window, Pause, pause);
SANDBOX_DEF_GFX_PROP_I(Window, X, x);
SANDBOX_DEF_GFX_PROP_I(Window, Y, y);
SANDBOX_DEF_GFX_PROP_I(Window, Width, width);
SANDBOX_DEF_GFX_PROP_I(Window, Height, height);
SANDBOX_DEF_GFX_PROP_I(Window, OX, ox);
SANDBOX_DEF_GFX_PROP_I(Window, OY, oy);
SANDBOX_DEF_GFX_PROP_I(Window, Opacity, opacity);
SANDBOX_DEF_GFX_PROP_I(Window, BackOpacity, back_opacity);
SANDBOX_DEF_GFX_PROP_I(Window, ContentsOpacity, contents_opacity);

void window_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        window_type = sb()->rb_data_type("Window", nullptr, dfree, nullptr, nullptr, 0, 0, 0);
        SANDBOX_AWAIT_R(window_class, rb_define_class, "Window", sb()->rb_cObject());
        SANDBOX_AWAIT(rb_define_alloc_func, window_class, alloc);
        SANDBOX_AWAIT(rb_define_method, window_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
        SANDBOX_AWAIT(disposable_binding_init<Window>, window_class);
        SANDBOX_AWAIT(viewportelement_binding_init<Window>, window_class);

        SANDBOX_AWAIT(rb_define_method, window_class, "update", (VALUE (*)(ANYARGS))update, 0);

        SANDBOX_INIT_PROP_BIND(window_class, windowskin);
        SANDBOX_INIT_PROP_BIND(window_class, contents);
        SANDBOX_INIT_PROP_BIND(window_class, stretch);
        SANDBOX_INIT_PROP_BIND(window_class, cursor_rect);
        SANDBOX_INIT_PROP_BIND(window_class, active);
        SANDBOX_INIT_PROP_BIND(window_class, pause);
        SANDBOX_INIT_PROP_BIND(window_class, x);
        SANDBOX_INIT_PROP_BIND(window_class, y);
        SANDBOX_INIT_PROP_BIND(window_class, width);
        SANDBOX_INIT_PROP_BIND(window_class, height);
        SANDBOX_INIT_PROP_BIND(window_class, ox);
        SANDBOX_INIT_PROP_BIND(window_class, oy);
        SANDBOX_INIT_PROP_BIND(window_class, opacity);
        SANDBOX_INIT_PROP_BIND(window_class, back_opacity);
        SANDBOX_INIT_PROP_BIND(window_class, contents_opacity);
    }
}
