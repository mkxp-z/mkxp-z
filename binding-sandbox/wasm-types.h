/*
** wasm-types.h
**
** This file is part of mkxp.
**
** Copyright (C) 2024 - 2026 The mkxp-z authors
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

#ifndef MKXPZ_SANDBOX_WASM_TYPES_H
#define MKXPZ_SANDBOX_WASM_TYPES_H

#include <cstdint>

namespace mkxp_sandbox {
#ifdef MKXPZ_RETRO_MEMORY64
    typedef int64_t wasm_ssize_t;
    typedef uint64_t wasm_size_t;
#else
    typedef int32_t wasm_ssize_t;
    typedef uint32_t wasm_size_t;
#endif

#define ANYARGS ...
    typedef uint32_t wasm_resource_t;
    typedef wasm_size_t wasm_ptr_t;
    typedef wasm_ptr_t wasm_objkey_t;
    typedef wasm_ptr_t VALUE;
    typedef wasm_ptr_t ID;
}

#endif // MKXPZ_SANDBOX_WASM_TYPES_H
