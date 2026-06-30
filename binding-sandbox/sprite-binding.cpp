/*
** sprite-binding.cpp
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

#include "sprite-binding.h"
#include "bitmap-binding.h"
#include "disposable-binding.h"
#include "etc-binding.h"
#include "flashable-binding.h"
#include "viewportelement-binding.h"
#include "sprite.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::sprite_class;
static struct bindings::rb_data_type sprite_type;

SANDBOX_DEF_ALLOC(sprite_type);

static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                GFX_LOCK;

                SANDBOX_AWAIT(viewportelement_initialize<Sprite>, argc, argv, self);
                SANDBOX_GUARD(SANDBOX_AWAIT(wrap_property, self, &get_private_data<Sprite>(self)->getSrcRect(sb().e), "src_rect", rect_class));
                SANDBOX_GUARD(SANDBOX_AWAIT(wrap_property, self, &get_private_data<Sprite>(self)->getColor(sb().e), "color", color_class));
                SANDBOX_GUARD(SANDBOX_AWAIT(wrap_property, self, &get_private_data<Sprite>(self)->getTone(sb().e), "tone", tone_class));
            }

            return SANDBOX_NIL;
        }

        void end() noexcept {
            GFX_UNLOCK;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

SANDBOX_DEF_GFX_PROP_OBJ_REF(Sprite, Bitmap, bitmap_class, Bitmap, bitmap);
SANDBOX_DEF_GFX_PROP_OBJ_VAL(Sprite, Rect, rect_class, SrcRect, src_rect);
SANDBOX_DEF_GFX_PROP_I(Sprite, X, x);
SANDBOX_DEF_GFX_PROP_I(Sprite, Y, y);
SANDBOX_DEF_GFX_PROP_I(Sprite, OX, ox);
SANDBOX_DEF_GFX_PROP_I(Sprite, OY, oy);
SANDBOX_DEF_GFX_PROP_F(Sprite, ZoomX, zoom_x);
SANDBOX_DEF_GFX_PROP_F(Sprite, ZoomY, zoom_y);
SANDBOX_DEF_GFX_PROP_F(Sprite, Angle, angle);
SANDBOX_DEF_GFX_PROP_B(Sprite, Mirror, mirror);
SANDBOX_DEF_GFX_PROP_I(Sprite, BushDepth, bush_depth);
SANDBOX_DEF_GFX_PROP_I(Sprite, Opacity, opacity);
SANDBOX_DEF_GFX_PROP_I(Sprite, BlendType, blend_type);
SANDBOX_DEF_GFX_PROP_OBJ_VAL(Sprite, Color, color_class, Color, color);
SANDBOX_DEF_GFX_PROP_OBJ_VAL(Sprite, Tone, tone_class, Tone, tone);

static VALUE width(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD(SANDBOX_AWAIT_S(0, rb_ull2inum, get_private_data<Sprite>(self)->getWidth(sb().e)));
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE height(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD(SANDBOX_AWAIT_S(0, rb_ull2inum, get_private_data<Sprite>(self)->getHeight(sb().e)));
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

SANDBOX_DEF_GFX_PROP_I(Sprite, BushOpacity, bush_opacity);

SANDBOX_DEF_GFX_PROP_OBJ_REF(Sprite, Bitmap, bitmap_class, Pattern, pattern);
SANDBOX_DEF_GFX_PROP_I(Sprite, PatternBlendType, pattern_blend_type);
SANDBOX_DEF_GFX_PROP_B(Sprite, PatternTile, pattern_tile);
SANDBOX_DEF_GFX_PROP_I(Sprite, PatternOpacity, pattern_opacity);
SANDBOX_DEF_GFX_PROP_I(Sprite, PatternScrollX, pattern_scroll_x);
SANDBOX_DEF_GFX_PROP_I(Sprite, PatternScrollY, pattern_scroll_y);
SANDBOX_DEF_GFX_PROP_F(Sprite, PatternZoomX, pattern_zoom_x);
SANDBOX_DEF_GFX_PROP_F(Sprite, PatternZoomY, pattern_zoom_y);
SANDBOX_DEF_GFX_PROP_B(Sprite, Invert, invert);

SANDBOX_DEF_GFX_PROP_I(Sprite, WaveAmp, wave_amp);
SANDBOX_DEF_GFX_PROP_I(Sprite, WaveLength, wave_length);
SANDBOX_DEF_GFX_PROP_I(Sprite, WaveSpeed, wave_speed);
SANDBOX_DEF_GFX_PROP_F(Sprite, WavePhase, wave_phase);

void sprite_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        sprite_type = sb()->rb_data_type("Sprite", nullptr, dfree, nullptr, nullptr, 0, 0, 0);
        SANDBOX_AWAIT_R(sprite_class, rb_define_class, "Sprite", sb()->rb_cObject());
        SANDBOX_AWAIT(rb_define_alloc_func, sprite_class, alloc);
        SANDBOX_AWAIT(rb_define_method, sprite_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
        SANDBOX_AWAIT(disposable_binding_init<Sprite>, sprite_class);
        SANDBOX_AWAIT(flashable_binding_init<Sprite>, sprite_class);
        SANDBOX_AWAIT(viewportelement_binding_init<Sprite>, sprite_class);

        SANDBOX_INIT_PROP_BIND(sprite_class, bitmap);
        SANDBOX_INIT_PROP_BIND(sprite_class, src_rect);
        SANDBOX_INIT_PROP_BIND(sprite_class, x);
        SANDBOX_INIT_PROP_BIND(sprite_class, y);
        SANDBOX_INIT_PROP_BIND(sprite_class, ox);
        SANDBOX_INIT_PROP_BIND(sprite_class, oy);
        SANDBOX_INIT_PROP_BIND(sprite_class, zoom_x);
        SANDBOX_INIT_PROP_BIND(sprite_class, zoom_y);
        SANDBOX_INIT_PROP_BIND(sprite_class, angle);
        SANDBOX_INIT_PROP_BIND(sprite_class, mirror);
        SANDBOX_INIT_PROP_BIND(sprite_class, bush_depth);
        SANDBOX_INIT_PROP_BIND(sprite_class, opacity);
        SANDBOX_INIT_PROP_BIND(sprite_class, blend_type);
        SANDBOX_INIT_PROP_BIND(sprite_class, color);
        SANDBOX_INIT_PROP_BIND(sprite_class, tone);

        SANDBOX_AWAIT(rb_define_method, sprite_class, "width", (VALUE (*)(ANYARGS))width, 0);
        SANDBOX_AWAIT(rb_define_method, sprite_class, "height", (VALUE (*)(ANYARGS))height, 0);

        SANDBOX_INIT_PROP_BIND(sprite_class, bush_opacity);

        SANDBOX_INIT_PROP_BIND(sprite_class, pattern);
        SANDBOX_INIT_PROP_BIND(sprite_class, pattern_blend_type);
        SANDBOX_INIT_PROP_BIND(sprite_class, pattern_tile);
        SANDBOX_INIT_PROP_BIND(sprite_class, pattern_opacity);
        SANDBOX_INIT_PROP_BIND(sprite_class, pattern_scroll_x);
        SANDBOX_INIT_PROP_BIND(sprite_class, pattern_scroll_y);
        SANDBOX_INIT_PROP_BIND(sprite_class, pattern_zoom_x);
        SANDBOX_INIT_PROP_BIND(sprite_class, pattern_zoom_y);
        SANDBOX_INIT_PROP_BIND(sprite_class, invert);

        SANDBOX_INIT_PROP_BIND(sprite_class, wave_amp);
        SANDBOX_INIT_PROP_BIND(sprite_class, wave_length);
        SANDBOX_INIT_PROP_BIND(sprite_class, wave_speed);
        SANDBOX_INIT_PROP_BIND(sprite_class, wave_phase);
    }
}
