/*
** sceneelement-binding.h
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

#ifndef MKXPZ_SANDBOX_SCENEELEMENT_BINDING_H
#define MKXPZ_SANDBOX_SCENEELEMENT_BINDING_H

#include "binding-util.h"

namespace mkxp_sandbox {
    template <class C> struct sceneelement_binding_init : boost::asio::coroutine {
    private:
        SANDBOX_DEF_GFX_PROP_I(C, Z, z);
        SANDBOX_DEF_GFX_PROP_B(C, Visible, visible);

    public:
        void operator()(VALUE klass) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_INIT_PROP_BIND(klass, z);
                SANDBOX_INIT_PROP_BIND(klass, visible);
            }
        }
    };
}

#endif // MKXPZ_SANDBOX_SCENEELEMENT_BINDING_H
