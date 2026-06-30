/*
** plane-binding.cpp
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

#include "plane-binding.h"
#include "bitmap-binding.h"
#include "disposable-binding.h"
#include "etc-binding.h"
#include "viewportelement-binding.h"
#include "etc.h"
#include "plane.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::plane_class;
static struct bindings::rb_data_type plane_type;

SANDBOX_DEF_ALLOC(plane_type);

static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                GFX_LOCK;

                SANDBOX_AWAIT(viewportelement_initialize<Plane>, argc, argv, self);
                SANDBOX_GUARD(SANDBOX_AWAIT(wrap_property, self, &get_private_data<Plane>(self)->getColor(sb().e), "color", color_class));
                SANDBOX_GUARD(SANDBOX_AWAIT(wrap_property, self, &get_private_data<Plane>(self)->getTone(sb().e), "tone", tone_class));
            }

            return SANDBOX_NIL;
        }

        void end() noexcept {
            GFX_UNLOCK;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

SANDBOX_DEF_GFX_PROP_OBJ_REF(Plane, Bitmap, bitmap_class, Bitmap, bitmap);
SANDBOX_DEF_GFX_PROP_OBJ_VAL(Plane, Color, color_class, Color, color);
SANDBOX_DEF_GFX_PROP_OBJ_VAL(Plane, Tone, tone_class, Tone, tone);
SANDBOX_DEF_GFX_PROP_I(Plane, OX, ox);
SANDBOX_DEF_GFX_PROP_I(Plane, OY, oy);
SANDBOX_DEF_GFX_PROP_F(Plane, ZoomX, zoom_x);
SANDBOX_DEF_GFX_PROP_F(Plane, ZoomY, zoom_y);
SANDBOX_DEF_GFX_PROP_I(Plane, Opacity, opacity);
SANDBOX_DEF_GFX_PROP_I(Plane, BlendType, blend_type);

void plane_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        plane_type = sb()->rb_data_type("Plane", nullptr, dfree, nullptr, nullptr, 0, 0, 0);
        SANDBOX_AWAIT_R(plane_class, rb_define_class, "Plane", sb()->rb_cObject());
        SANDBOX_AWAIT(rb_define_alloc_func, plane_class, alloc);
        SANDBOX_AWAIT(rb_define_method, plane_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
        SANDBOX_AWAIT(disposable_binding_init<Plane>, plane_class);
        SANDBOX_AWAIT(viewportelement_binding_init<Plane>, plane_class);

        SANDBOX_INIT_PROP_BIND(plane_class, bitmap);
        SANDBOX_INIT_PROP_BIND(plane_class, color);
        SANDBOX_INIT_PROP_BIND(plane_class, tone);
        SANDBOX_INIT_PROP_BIND(plane_class, ox);
        SANDBOX_INIT_PROP_BIND(plane_class, oy);
        SANDBOX_INIT_PROP_BIND(plane_class, zoom_x);
        SANDBOX_INIT_PROP_BIND(plane_class, zoom_y);
        SANDBOX_INIT_PROP_BIND(plane_class, opacity);
        SANDBOX_INIT_PROP_BIND(plane_class, blend_type);
    }
}
