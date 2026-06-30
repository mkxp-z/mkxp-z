/*
** flashable-binding.h
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

#ifndef MKXPZ_SANDBOX_FLASHABLE_BINDING_H
#define MKXPZ_SANDBOX_FLASHABLE_BINDING_H

#include "binding-util.h"
#include "etc-binding.h"
#include "etc.h"

namespace mkxp_sandbox {
    template <class C> struct flashable_binding_init : boost::asio::coroutine {
    private:
        static VALUE flash(VALUE self, VALUE obj, VALUE value) {
            struct coro : boost::asio::coroutine {
                typedef decl_slots<int32_t> slots;

                VALUE operator()(VALUE self, VALUE obj, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_S(0, rb_num2int, value);
                        if (obj != SANDBOX_NIL) {
                            SANDBOX_AWAIT(check_type, obj, color_class);
                        }
                        get_private_data<C>(self)->flash(obj == SANDBOX_NIL ? nullptr : &get_private_data<Color>(obj)->norm, SANDBOX_SLOT(0));
                    }

                    return SANDBOX_NIL;
                }
            };

            return sb()->bind<struct coro>()()(self, obj, value);
        }

        static VALUE update(VALUE self) {
            struct coro : boost::asio::coroutine {
                VALUE operator()(VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_GUARD(get_private_data<C>(self)->update(sb().e));
                    }

                    return SANDBOX_NIL;
                }
            };

            return sb()->bind<struct coro>()()(self);
        }

    public:
        void operator()(VALUE klass) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(rb_define_method, klass, "flash", (VALUE (*)(ANYARGS))flash, 2);
                SANDBOX_AWAIT(rb_define_method, klass, "update", (VALUE (*)(ANYARGS))update, 0);
            }
        }
    };
}

#endif // MKXPZ_SANDBOX_FLASHABLE_BINDING_H
