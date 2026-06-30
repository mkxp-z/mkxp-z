/*
** wasm-rt.h
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

#ifndef MKXPZ_SANDBOX_WASM_RT_H
#define MKXPZ_SANDBOX_WASM_RT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifndef __GNUC__
#  include <string.h>
#endif

#include <mkxp-sandbox-ruby-func-types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MKXPZ_BIG_ENDIAN
#  define WABT_BIG_ENDIAN 1
#endif

#ifndef LIKELY
#  ifdef __GNUC__
#    define LIKELY(x) __builtin_expect(x, 1)
#  else
#    define LIKELY(x) (x)
#  endif
#endif

#ifndef UNLIKELY
#  ifdef __GNUC__
#    define UNLIKELY(x) __builtin_expect(x, 0)
#  else
#    define UNLIKELY(x) (x)
#  endif
#endif

#ifdef __GNUC__
#  define wasm_rt_memcpy __builtin_memcpy
#else
#  define wasm_rt_memcpy memcpy
#endif

#ifdef _MSC_VER
#  define WASM_RT_NO_RETURN __declspec(noreturn)
#else
#  define WASM_RT_NO_RETURN __attribute__((noreturn))
#endif

/* Don't define this as an enum. It causes builds using devkitARM or Vita SDK to successfully compile but crash on startup. */
typedef int wasm_rt_type_t;
#define WASM_RT_I32 0
#define WASM_RT_I64 1
#define WASM_RT_F32 2
#define WASM_RT_F64 3
#define WASM_RT_FUNCREF 4
#define WASM_RT_EXTERNREF 5

typedef int wasm_rt_trap_t;
#define WASM_RT_TRAP_NONE 0
#define WASM_RT_TRAP_OOB 1
#define WASM_RT_TRAP_INT_OVERFLOW 2
#define WASM_RT_TRAP_DIV_BY_ZERO 3
#define WASM_RT_TRAP_INVALID_CONVERSION 4
#define WASM_RT_TRAP_UNREACHABLE 5
#define WASM_RT_TRAP_CALL_INDIRECT 6
#define WASM_RT_TRAP_UNCAUGHT_EXCEPTION 7
#define WASM_RT_TRAP_EXHAUSTION 8

typedef const char *wasm_rt_func_type_t;

typedef void (*wasm_rt_function_ptr_t)(void);

typedef struct {
    void *fn;
} wasm_rt_tailcallee_t;

typedef struct {
    wasm_rt_func_type_t func_type;
    wasm_rt_function_ptr_t func;
    wasm_rt_tailcallee_t func_tailcallee;
    void *module_instance;
} wasm_rt_funcref_t;

#define wasm_rt_funcref_null_value (wasm_rt_funcref_t){NULL, NULL, {NULL}, NULL};

typedef struct {
    uint8_t *private_data;
    uint8_t *data;
    uint64_t pages;
    uint64_t size;
    uint64_t capacity;
} wasm_rt_memory_t;

typedef struct {
    void *private_data;
    wasm_rt_funcref_t *data;
    uint32_t size;
} wasm_rt_funcref_table_t;

typedef void *wasm_rt_externref_t;

#define wasm_rt_externref_null_value NULL;

typedef struct {
    wasm_rt_externref_t *data;
    uint32_t size;
} wasm_rt_externref_table_t;

#define wasm_rt_is_initialized mkxp_wasm_rt_is_initialized
bool wasm_rt_is_initialized(void);

#define wasm_rt_trap mkxp_wasm_rt_trap
WASM_RT_NO_RETURN void wasm_rt_trap(wasm_rt_trap_t error);

#define wasm_rt_allocate_memory mkxp_wasm_rt_allocate_memory
void wasm_rt_allocate_memory(wasm_rt_memory_t *memory, uint32_t initial_pages, uint32_t max_pages, bool is64, uint32_t page_size);

#define wasm_rt_replace_memory mkxp_wasm_rt_replace_memory
void wasm_rt_replace_memory(wasm_rt_memory_t *memory, size_t size, size_t capacity);

#define wasm_rt_grow_memory mkxp_wasm_rt_grow_memory
uint32_t wasm_rt_grow_memory(wasm_rt_memory_t *memory, uint32_t pages);

#define wasm_rt_free_memory mkxp_wasm_rt_free_memory
void wasm_rt_free_memory(wasm_rt_memory_t *memory);

#define wasm_rt_allocate_funcref_table mkxp_wasm_rt_allocate_funcref_table
void wasm_rt_allocate_funcref_table(wasm_rt_funcref_table_t *table, uint32_t elements, uint32_t max_elements);

#define wasm_rt_free_funcref_table mkxp_wasm_rt_free_funcref_table
void wasm_rt_free_funcref_table(wasm_rt_funcref_table_t *table);

#define wasm_rt_push_funcref mkxp_wasm_rt_push_funcref
uint32_t wasm_rt_push_funcref(wasm_rt_funcref_table_t *table, wasm_rt_funcref_t funcref);

#ifdef __cplusplus
}
#endif

#endif /* MKXPZ_SANDBOX_WASM_RT_H */
