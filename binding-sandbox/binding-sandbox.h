/*
** binding-sandbox.h
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

#ifndef MKXPZ_BINDING_SANDBOX_H
#define MKXPZ_BINDING_SANDBOX_H

#include "binding-util.h"

namespace mkxp_sandbox {
    struct eval_script : boost::asio::coroutine {
        typedef decl_slots<VALUE> slots;
        VALUE operator()(VALUE string, VALUE filename);
    };

    struct sandbox_binding_init : boost::asio::coroutine {
        typedef decl_slots<VALUE, ID> slots;
        void operator()();
    };

    struct sandbox_run_rmxp_scripts : boost::asio::coroutine {
        typedef decl_slots<VALUE, VALUE, wasm_size_t, wasm_size_t, VALUE, wasm_ptr_t, wasm_ptr_t, wasm_size_t, wasm_size_t, VALUE, VALUE> slots;
        void operator()();
        void end() noexcept;
    };
}

#endif // MKXPZ_BINDING_SANDBOX_H
