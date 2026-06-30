/*
** serializable-binding.h
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

#ifndef MKXPZ_SANDBOX_SERIALIZABLE_BINDING_H
#define MKXPZ_SANDBOX_SERIALIZABLE_BINDING_H

#include <algorithm>
#include "binding-util.h"

namespace mkxp_sandbox {
    template <class C> VALUE serializable_load(VALUE klass, VALUE src) {
        struct coro : boost::asio::coroutine {
            typedef decl_slots<wasm_ptr_t, wasm_size_t, VALUE> slots;

            VALUE operator()(VALUE klass, VALUE src) {
                BOOST_ASIO_CORO_REENTER (this) {
                    SANDBOX_AWAIT_S(2, rb_obj_alloc, klass);
                    SANDBOX_AWAIT_S(0, rb_string_value_ptr, &src);
                    SANDBOX_AWAIT_S(1, get_bytesize, src);
                    if (SANDBOX_SLOT(1) > 0) {
                        sb()->check_bounds(SANDBOX_SLOT(0), SANDBOX_SLOT(1));
#ifdef MKXPZ_BIG_ENDIAN
                        std::reverse(&sb()->ref<char>(SANDBOX_SLOT(0), SANDBOX_SLOT(1) - 1), &sb()->ref<char>(SANDBOX_SLOT(0), SANDBOX_SLOT(1) - 1) + SANDBOX_SLOT(1));
                        SANDBOX_GUARD(set_private_data(sb().e, SANDBOX_SLOT(2), C::deserialize(sb().e, &sb()->ref<char>(SANDBOX_SLOT(0), SANDBOX_SLOT(1) - 1), SANDBOX_SLOT(1))));
                        std::reverse(&sb()->ref<char>(SANDBOX_SLOT(0), SANDBOX_SLOT(1) - 1), &sb()->ref<char>(SANDBOX_SLOT(0), SANDBOX_SLOT(1) - 1) + SANDBOX_SLOT(1));
#else
                        SANDBOX_GUARD(set_private_data(sb().e, SANDBOX_SLOT(2), C::deserialize(sb().e, &sb()->ref<char>(SANDBOX_SLOT(0)), SANDBOX_SLOT(1))));
#endif // MKXPZ_BIG_ENDIAN
                    }
                }

                return SANDBOX_SLOT(2);
            }
        };

        return sb()->bind<struct coro>()()(klass, src);
    }

    template <class C> VALUE serializable_dump(VALUE self, VALUE depth) {
        struct coro : boost::asio::coroutine {
            typedef decl_slots<wasm_ptr_t, wasm_size_t, VALUE> slots;

            VALUE operator()(VALUE self) {
                BOOST_ASIO_CORO_REENTER (this) {
                    SANDBOX_SLOT(1) = get_private_data<C>(self)->serialSize();
                    SANDBOX_AWAIT_S(2, rb_str_new_cstr, "");
                    SANDBOX_AWAIT(rb_str_resize, SANDBOX_SLOT(2), SANDBOX_SLOT(1));
                    SANDBOX_AWAIT_S(0, rb_string_value_ptr, &SANDBOX_SLOT(2));
                    if (SANDBOX_SLOT(1) > 0) {
                        sb()->check_bounds(SANDBOX_SLOT(0), SANDBOX_SLOT(1));
#ifdef MKXPZ_BIG_ENDIAN
                        get_private_data<C>(self)->serialize(&sb()->ref<char>(SANDBOX_SLOT(0), SANDBOX_SLOT(1) - 1));
                        std::reverse(&sb()->ref<char>(SANDBOX_SLOT(0), SANDBOX_SLOT(1) - 1), &sb()->ref<char>(SANDBOX_SLOT(0), SANDBOX_SLOT(1) - 1) + SANDBOX_SLOT(1));
#else
                        get_private_data<C>(self)->serialize(&sb()->ref<char>(SANDBOX_SLOT(0)));
#endif // MKXPZ_BIG_ENDIAN
                    }
                }

                return SANDBOX_SLOT(2);
            }
        };

        return sb()->bind<struct coro>()()(self);
    }

    template <class C> struct serializable_binding_init : boost::asio::coroutine {
        void operator()(VALUE klass) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(rb_define_singleton_method, klass, "_load", (VALUE (*)(ANYARGS))serializable_load<C>, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "_dump", (VALUE (*)(ANYARGS))serializable_dump<C>, 1);
            }
        }
    };
}

#endif // MKXPZ_SANDBOX_SERIALIZABLE_BINDING_H
