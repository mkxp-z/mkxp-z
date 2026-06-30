/*
** tilemapvx-binding.cpp
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

#include "tilemapvx-binding.h"
#include "bitmap-binding.h"
#include "disposable-binding.h"
#include "table-binding.h"
#include "viewport-binding.h"
#include "tilemapvx.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::tilemapvx_class;
VALUE mkxp_sandbox::bitmap_array_class;
static struct bindings::rb_data_type tilemapvx_type;
static struct bindings::rb_data_type bitmap_array_type;

struct bitmap_array_binding_init : boost::asio::coroutine {
    SANDBOX_DEF_ALLOC(bitmap_array_type);

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
                    get_private_data<TilemapVX::BitmapArray>(self)->set(SANDBOX_SLOT(0), get_private_data<Bitmap>(obj));
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
            bitmap_array_type = sb()->rb_data_type("BitmapArray", nullptr, dfree, nullptr, nullptr, 0, 0, 0);
            SANDBOX_AWAIT_R(bitmap_array_class, rb_define_class, "BitmapArray", sb()->rb_cObject());
            SANDBOX_AWAIT(rb_define_alloc_func, bitmap_array_class, alloc);

            SANDBOX_AWAIT(rb_define_method, bitmap_array_class, "[]", (VALUE (*)(ANYARGS))get, 1);
            SANDBOX_AWAIT(rb_define_method, bitmap_array_class, "[]=", (VALUE (*)(ANYARGS))set, 2);
        }
    }
};

SANDBOX_DEF_ALLOC(tilemapvx_type);

static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, VALUE, VALUE, uint32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
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

                    GFX_LOCK;
                    set_private_data(self, new TilemapVX(viewport));
                }

                /* Dispose the old bitmap array if we're reinitializing.
                 * See the comment in setPrivateData for more info. */
                SANDBOX_AWAIT_S(1, rb_iv_get, self, "bitmap_array");
                if (SANDBOX_SLOT(1) != SANDBOX_NIL) {
                    set_private_data(SANDBOX_SLOT(1), (TilemapVX::BitmapArray *)nullptr);
                }

                SANDBOX_GUARD(SANDBOX_AWAIT_S(1, wrap_property, self, get_private_data<TilemapVX>(self)->getBitmapArray(sb().e), "bitmap_array", bitmap_array_class));

                SANDBOX_AWAIT_S(2, rb_class_new_instance, 0, nullptr, sb()->rb_cArray());
                for (SANDBOX_SLOT(3) = 0; SANDBOX_SLOT(3) < 9; ++SANDBOX_SLOT(3)) {
                    SANDBOX_AWAIT(rb_ary_push, SANDBOX_SLOT(2), SANDBOX_NIL);
                }

                SANDBOX_AWAIT(rb_iv_set, SANDBOX_SLOT(1), "array", SANDBOX_SLOT(2));

                /* Circular reference so both objects are always
                 * alive at the same time */
                SANDBOX_AWAIT(rb_iv_set, SANDBOX_SLOT(1), "tilemap", self);

                GFX_UNLOCK;
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE bitmaps(VALUE self) {
    return sb()->bind<struct rb_iv_get>()()(self, "bitmap_array");
}

static VALUE update(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD_L(get_private_data<TilemapVX>(self)->update(sb().e));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

SANDBOX_DEF_GFX_PROP_OBJ_REF(TilemapVX, Viewport, viewport_class, Viewport, viewport);
SANDBOX_DEF_GFX_PROP_OBJ_REF(TilemapVX, Table, table_class, MapData, map_data);
SANDBOX_DEF_GFX_PROP_OBJ_REF(TilemapVX, Table, table_class, FlashData, flash_data);
SANDBOX_DEF_GFX_PROP_B(TilemapVX, Visible, visible);
SANDBOX_DEF_GFX_PROP_I(TilemapVX, OX, ox);
SANDBOX_DEF_GFX_PROP_I(TilemapVX, OY, oy);

SANDBOX_DEF_GFX_PROP_OBJ_REF(TilemapVX, Table, table_class, Flags, flags);
SANDBOX_DEF_GFX_PROP_OBJ_REF(TilemapVX, Table, table_class, Flags, passages);

void tilemapvx_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT(bitmap_array_binding_init);

        tilemapvx_type = sb()->rb_data_type("Tilemap", nullptr, dfree, nullptr, nullptr, 0, 0, 0);
        SANDBOX_AWAIT_R(tilemapvx_class, rb_define_class, "Tilemap", sb()->rb_cObject());
        SANDBOX_AWAIT(rb_define_alloc_func, tilemapvx_class, alloc);
        SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
        SANDBOX_AWAIT(disposable_binding_init<TilemapVX>, tilemapvx_class);

        SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "bitmaps", (VALUE (*)(ANYARGS))bitmaps, 0);
        SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "update", (VALUE (*)(ANYARGS))update, 0);

        SANDBOX_INIT_PROP_BIND(tilemapvx_class, viewport);
        SANDBOX_INIT_PROP_BIND(tilemapvx_class, map_data);
        SANDBOX_INIT_PROP_BIND(tilemapvx_class, flash_data);
        SANDBOX_INIT_PROP_BIND(tilemapvx_class, visible);
        SANDBOX_INIT_PROP_BIND(tilemapvx_class, ox);
        SANDBOX_INIT_PROP_BIND(tilemapvx_class, oy);

        if (rgssVer == 3) {
            SANDBOX_INIT_PROP_BIND(tilemapvx_class, flags);
        } else {
            SANDBOX_INIT_PROP_BIND(tilemapvx_class, passages);
        }
    }
}
