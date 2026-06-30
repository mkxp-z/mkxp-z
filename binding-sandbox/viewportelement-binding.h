/*
** viewportelement-binding.h
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

#ifndef MKXPZ_SANDBOX_VIEWPORTELEMENT_BINDING_H
#define MKXPZ_SANDBOX_VIEWPORTELEMENT_BINDING_H

#include "binding-util.h"
#include "sceneelement-binding.h"
#include "viewport-binding.h"
#include "viewport.h"

namespace mkxp_sandbox {
    template <class C> struct viewportelement_initialize : boost::asio::coroutine {
        typedef decl_slots<VALUE> slots;

        void operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_SLOT(0) = SANDBOX_NIL;

                if (argc > 0 && sb()->ref<VALUE>(argv, 0) != SANDBOX_NIL) {
                    SANDBOX_AWAIT(check_type, sb()->ref<VALUE>(argv, 0), viewport_class);
                }

                {
                    Viewport *viewport = nullptr;
                    if (argc > 0) {
                        SANDBOX_SLOT(0) = sb()->ref<VALUE>(argv, 0);
                        if (SANDBOX_SLOT(0) != SANDBOX_NIL) {
                            viewport = get_private_data<Viewport>(SANDBOX_SLOT(0));
                        }
                    }

                    /* Construct object */
                    C *c = new C(viewport);
                    set_private_data(self, c);

                    /* Wrap property objects */
                    c->initDynAttribs();
                }

                /* Set property objects */
                SANDBOX_AWAIT(rb_iv_set, self, "viewport", SANDBOX_SLOT(0));
            }
        }
    };

    template <class C> struct viewportelement_binding_init : boost::asio::coroutine {
    private:
        SANDBOX_DEF_PROP_OBJ_REF(C, Viewport, viewport_class, Viewport, viewport);

        static VALUE viewport(VALUE self) {
            struct coro : boost::asio::coroutine {
                typedef decl_slots<VALUE> slots;

                VALUE operator()(VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        if (get_private_data<C>(self)->isDisposed()) {
                            SANDBOX_AWAIT(raise_disposed_access, self);
                        }

                        SANDBOX_AWAIT_S(0, rb_iv_get, self, "viewport");
                    }

                    return SANDBOX_SLOT(0);
                }
            };

            return sb()->bind<struct coro>()()(self);
        }

    public:
        void operator()(VALUE klass) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(sceneelement_binding_init<C>, klass);
                SANDBOX_AWAIT(rb_define_method, klass, "viewport", (VALUE (*)(ANYARGS))viewport, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "viewport=", (VALUE (*)(ANYARGS))set_viewport, 1);
            }
        }
    };
}

#endif // MKXPZ_SANDBOX_VIEWPORTELEMENT_BINDING_H
