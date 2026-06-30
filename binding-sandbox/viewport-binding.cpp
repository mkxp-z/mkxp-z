/*
** viewport-binding.cpp
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

#include "viewport-binding.h"
#include "disposable-binding.h"
#include "etc-binding.h"
#include "flashable-binding.h"
#include "sceneelement-binding.h"
#include "viewport.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::viewport_class;
static struct bindings::rb_data_type viewport_type;

SANDBOX_DEF_ALLOC(viewport_type);

static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t, int32_t, int32_t, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                {
                    Viewport *viewport;

                    if (argc == 0 && rgssVer >= 3) {
                        GFX_LOCK;
                        viewport = new Viewport;
                    } else if (argc == 1) {
                        GFX_LOCK;
                        SANDBOX_AWAIT(check_type, sb()->ref<VALUE>(argv, 0), rect_class);
                        viewport = new Viewport(get_private_data<Rect>(sb()->ref<VALUE>(argv, 0)));
                    } else {
                        SANDBOX_AWAIT_S(0, rb_num2int, sb()->ref<VALUE>(argv, 0));
                        SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 1));
                        SANDBOX_AWAIT_S(2, rb_num2int, sb()->ref<VALUE>(argv, 2));
                        SANDBOX_AWAIT_S(3, rb_num2int, sb()->ref<VALUE>(argv, 3));
                        GFX_LOCK;
                        viewport = new Viewport(SANDBOX_SLOT(0), SANDBOX_SLOT(1), SANDBOX_SLOT(2), SANDBOX_SLOT(3));
                    }

                    set_private_data(self, viewport);

                    viewport->initDynAttribs();
                }

                SANDBOX_GUARD(SANDBOX_AWAIT(wrap_property, self, &get_private_data<Viewport>(self)->getRect(sb().e), "rect", rect_class));
                SANDBOX_GUARD(SANDBOX_AWAIT(wrap_property, self, &get_private_data<Viewport>(self)->getColor(sb().e), "color", color_class));
                SANDBOX_GUARD(SANDBOX_AWAIT(wrap_property, self, &get_private_data<Viewport>(self)->getTone(sb().e), "tone", tone_class));

                GFX_UNLOCK
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

SANDBOX_DEF_GFX_PROP_OBJ_VAL(Viewport, Rect, rect_class, Rect, rect);
SANDBOX_DEF_GFX_PROP_I(Viewport, OX, ox);
SANDBOX_DEF_GFX_PROP_I(Viewport, OY, oy);
SANDBOX_DEF_GFX_PROP_OBJ_VAL(Viewport, Color, color_class, Color, color);
SANDBOX_DEF_GFX_PROP_OBJ_VAL(Viewport, Tone, tone_class, Tone, tone);

void viewport_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        viewport_type = sb()->rb_data_type("Viewport", nullptr, dfree, nullptr, nullptr, 0, 0, 0);
        SANDBOX_AWAIT_R(viewport_class, rb_define_class, "Viewport", sb()->rb_cObject());
        SANDBOX_AWAIT(rb_define_alloc_func, viewport_class, alloc);
        SANDBOX_AWAIT(rb_define_method, viewport_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
        SANDBOX_AWAIT(disposable_binding_init<Viewport>, viewport_class);
        SANDBOX_AWAIT(flashable_binding_init<Viewport>, viewport_class);
        SANDBOX_AWAIT(sceneelement_binding_init<Viewport>, viewport_class);

        SANDBOX_INIT_PROP_BIND(viewport_class, rect);
        SANDBOX_INIT_PROP_BIND(viewport_class, ox);
        SANDBOX_INIT_PROP_BIND(viewport_class, oy);
        SANDBOX_INIT_PROP_BIND(viewport_class, color);
        SANDBOX_INIT_PROP_BIND(viewport_class, tone);
    }
}
