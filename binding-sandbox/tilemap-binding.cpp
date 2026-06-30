/*
** tilemap-binding.cpp
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

#include "tilemap-binding.h"
#include "bitmap-binding.h"
#include "disposable-binding.h"
#include "etc-binding.h"
#include "table-binding.h"
#include "viewport-binding.h"
#include "tilemap.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::tilemap_class;
VALUE mkxp_sandbox::tilemap_autotiles_class;
static struct bindings::rb_data_type tilemap_type;
static struct bindings::rb_data_type tilemap_autotiles_type;

struct tilemap_autotiles_binding_init : boost::asio::coroutine {
    SANDBOX_DEF_ALLOC(tilemap_autotiles_type)

    static VALUE get(VALUE self, VALUE i) {
        struct coro : boost::asio::coroutine {
            typedef decl_slots<wasm_size_t, VALUE, VALUE> slots;

            VALUE operator()(VALUE self, VALUE i) {
                BOOST_ASIO_CORO_REENTER (this) {
                    SANDBOX_AWAIT_S(1, rb_iv_get, self, "array");
                    SANDBOX_AWAIT_S(0, rb_num2ulong, i);
                    SANDBOX_AWAIT_S(2, rb_ary_entry, SANDBOX_SLOT(1), SANDBOX_SLOT(0));
                }

                return SANDBOX_SLOT(2);
            }
        };

        return sb()->bind<struct coro>()()(self, i);
    }

    static VALUE set(VALUE self, VALUE i, VALUE obj) {
        struct coro : boost::asio::coroutine {
            typedef decl_slots<wasm_size_t, VALUE> slots;

            VALUE operator()(VALUE self, VALUE i, VALUE obj) {
                BOOST_ASIO_CORO_REENTER (this) {
                    GFX_LOCK;

                    SANDBOX_AWAIT_S(0, rb_num2ulong, i);

                    SANDBOX_AWAIT(check_type, obj, bitmap_class);
                    get_private_data<Tilemap::Autotiles>(self)->set(SANDBOX_SLOT(0), get_private_data<Bitmap>(obj));
                    SANDBOX_AWAIT_S(1, rb_iv_get, self, "array");
                    SANDBOX_AWAIT(rb_ary_store, SANDBOX_SLOT(1), SANDBOX_SLOT(0), obj);
                }

                return self;
            }

            void end() noexcept {
                GFX_UNLOCK;
            }
        };

        return sb()->bind<struct coro>()()(self, i, obj);
    }

    void operator()() {
        BOOST_ASIO_CORO_REENTER (this) {
            tilemap_autotiles_type = sb()->rb_data_type("TilemapAutotiles", nullptr, dfree, nullptr, nullptr, 0, 0, 0);
            SANDBOX_AWAIT_R(tilemap_autotiles_class, rb_define_class, "TilemapAutotiles", sb()->rb_cObject());
            SANDBOX_AWAIT(rb_define_alloc_func, tilemap_autotiles_class, alloc);

            SANDBOX_AWAIT(rb_define_method, tilemap_autotiles_class, "[]", (VALUE (*)(ANYARGS))get, 1);
            SANDBOX_AWAIT(rb_define_method, tilemap_autotiles_class, "[]=", (VALUE (*)(ANYARGS))set, 2);
        }
    }
};

SANDBOX_DEF_ALLOC(tilemap_type)

static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, VALUE, VALUE, uint32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                GFX_LOCK;

                if (argc > 0) {
                    SANDBOX_AWAIT(check_type, sb()->ref<VALUE>(argv, 0), viewport_class);
                }

                {
                    SANDBOX_SLOT(0) = SANDBOX_NIL;
                    Viewport *viewport = nullptr;
                    if (argc > 0) {
                        SANDBOX_SLOT(0) = sb()->ref<VALUE>(argv, 0);
                        if (SANDBOX_SLOT(0) != SANDBOX_NIL) {
                            viewport = get_private_data<Viewport>(SANDBOX_SLOT(0));
                        }
                    }

                    Tilemap *tilemap = new Tilemap(viewport);

                    set_private_data(self, tilemap);

                    tilemap->initDynAttribs();
                }

                /* Dispose the old autotiles if we're reinitializing.
                 * See the comment in setPrivateData for more info. */
                SANDBOX_AWAIT_S(1, rb_iv_get, self, "autotiles");
                if (SANDBOX_SLOT(1) != SANDBOX_NIL) {
                    set_private_data(SANDBOX_SLOT(1), (Tilemap::Autotiles *)nullptr);
                }

                SANDBOX_GUARD(SANDBOX_AWAIT_S(1, wrap_property, self, get_private_data<Tilemap>(self)->getAutotiles(sb().e), "autotiles", tilemap_autotiles_class));

                SANDBOX_GUARD(SANDBOX_AWAIT(wrap_property, self, &get_private_data<Tilemap>(self)->getColor(sb().e), "color", color_class));
                SANDBOX_GUARD(SANDBOX_AWAIT(wrap_property, self, &get_private_data<Tilemap>(self)->getTone(sb().e), "tone", tone_class));

                SANDBOX_AWAIT_S(2, rb_class_new_instance, 0, nullptr, sb()->rb_cArray());
                for (SANDBOX_SLOT(3) = 0; SANDBOX_SLOT(3) < 7; ++SANDBOX_SLOT(3)) {
                    SANDBOX_AWAIT(rb_ary_push, SANDBOX_SLOT(2), SANDBOX_NIL);
                }

                SANDBOX_AWAIT(rb_iv_set, SANDBOX_SLOT(1), "array", SANDBOX_SLOT(2));

                /* Circular reference so both objects are always
                 * alive at the same time */
                SANDBOX_AWAIT(rb_iv_set, SANDBOX_SLOT(1), "tilemap", self);
            }

            return SANDBOX_NIL;
        }

        void end() noexcept {
            GFX_UNLOCK;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE autotiles(VALUE self) {
    return sb()->bind<struct rb_iv_get>()()(self, "autotiles");
}

static VALUE update(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD_L(get_private_data<Tilemap>(self)->update(sb().e));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE viewport(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (get_private_data<Tilemap>(self)->isDisposed()) {
                    SANDBOX_AWAIT(raise_disposed_access, self);
                }

                SANDBOX_AWAIT_S(0, rb_iv_get, self, "viewport");
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

SANDBOX_DEF_GFX_PROP_OBJ_REF(Tilemap, Bitmap, bitmap_class, Tileset, tileset);
SANDBOX_DEF_GFX_PROP_OBJ_REF(Tilemap, Table, table_class, MapData, map_data);
SANDBOX_DEF_GFX_PROP_OBJ_REF(Tilemap, Table, table_class, FlashData, flash_data);
SANDBOX_DEF_GFX_PROP_OBJ_REF(Tilemap, Table, table_class, Priorities, priorities);
SANDBOX_DEF_GFX_PROP_B(Tilemap, Visible, visible);
SANDBOX_DEF_GFX_PROP_I(Tilemap, OX, ox);
SANDBOX_DEF_GFX_PROP_I(Tilemap, OY, oy);

SANDBOX_DEF_GFX_PROP_I(Tilemap, Opacity, opacity);
SANDBOX_DEF_GFX_PROP_I(Tilemap, BlendType, blend_type);
SANDBOX_DEF_GFX_PROP_OBJ_VAL(Tilemap, Color, color_class, Color, color);
SANDBOX_DEF_GFX_PROP_OBJ_VAL(Tilemap, Tone, tone_class, Tone, tone);

void tilemap_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT(tilemap_autotiles_binding_init);

        tilemap_type = sb()->rb_data_type("Tilemap", nullptr, dfree, nullptr, nullptr, 0, 0, 0);
        SANDBOX_AWAIT_R(tilemap_class, rb_define_class, "Tilemap", sb()->rb_cObject());
        SANDBOX_AWAIT(rb_define_alloc_func, tilemap_class, alloc);
        SANDBOX_AWAIT(rb_define_method, tilemap_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
        SANDBOX_AWAIT(disposable_binding_init<Tilemap>, tilemap_class);

        SANDBOX_AWAIT(rb_define_method, tilemap_class, "autotiles", (VALUE (*)(ANYARGS))autotiles, 0);
        SANDBOX_AWAIT(rb_define_method, tilemap_class, "update", (VALUE (*)(ANYARGS))update, 0);

        SANDBOX_AWAIT(rb_define_method, tilemap_class, "viewport", (VALUE (*)(ANYARGS))viewport, 0);

        SANDBOX_INIT_PROP_BIND(tilemap_class, tileset);
        SANDBOX_INIT_PROP_BIND(tilemap_class, map_data);
        SANDBOX_INIT_PROP_BIND(tilemap_class, flash_data);
        SANDBOX_INIT_PROP_BIND(tilemap_class, priorities);
        SANDBOX_INIT_PROP_BIND(tilemap_class, visible);
        SANDBOX_INIT_PROP_BIND(tilemap_class, ox);
        SANDBOX_INIT_PROP_BIND(tilemap_class, oy);

        SANDBOX_INIT_PROP_BIND(tilemap_class, opacity);
        SANDBOX_INIT_PROP_BIND(tilemap_class, blend_type);
        SANDBOX_INIT_PROP_BIND(tilemap_class, color);
        SANDBOX_INIT_PROP_BIND(tilemap_class, tone);
    }
}
