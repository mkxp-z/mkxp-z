/*
** wasm-rt.cpp
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

#include <cassert>
#include <cstdlib>
#include <vector>
#include "core.h"
#include "forced-assert.h"
#include "wasm-rt.h"

//#define WASM_RT_DEBUG

#define WASM_PAGE_SIZE ((uint64_t)65536U)

#define WASM_MIN_PAGES ((uint32_t)1536U)

#define WASM_MAX_SIZE std::min((uint64_t)(mkxp_sandbox::wasm_size_t)-1, (uint64_t)SIZE_MAX)

extern "C" bool wasm_rt_is_initialized(void) {
    return true;
}

extern "C" WASM_RT_NO_RETURN void wasm_rt_trap(wasm_rt_trap_t error) {
#ifdef WASM_RT_DEBUG
    switch (error) {
        case WASM_RT_TRAP_OOB:
            MKXPZ_FORCED_ASSERT_WITH_MESSAGE(false, "Sandbox error 1: OOB (out-of-bounds memory access)");
            break;
        case WASM_RT_TRAP_INT_OVERFLOW:
            MKXPZ_FORCED_ASSERT_WITH_MESSAGE(false, "Sandbox error 2: INT_OVERFLOW (arithmetic overflow)");
            break;
        case WASM_RT_TRAP_DIV_BY_ZERO:
            MKXPZ_FORCED_ASSERT_WITH_MESSAGE(false, "Sandbox error 3: DIV_BY_ZERO (division by zero)");
            break;
        case WASM_RT_TRAP_INVALID_CONVERSION:
            MKXPZ_FORCED_ASSERT_WITH_MESSAGE(false, "Sandbox error 4: INVALID_CONVERSION (invalid integer cast)");
            break;
        case WASM_RT_TRAP_UNREACHABLE:
            MKXPZ_FORCED_ASSERT_WITH_MESSAGE(false, "Sandbox error 5: UNREACHABLE (hit an `unreachable' instruction in WebAssembly)");
            break;
        case WASM_RT_TRAP_CALL_INDIRECT:
            MKXPZ_FORCED_ASSERT_WITH_MESSAGE(false, "Sandbox error 6: CALL_INDIRECT (attempted to call a function pointer with the wrong call signature)");
            break;
        case WASM_RT_TRAP_UNCAUGHT_EXCEPTION:
            MKXPZ_FORCED_ASSERT_WITH_MESSAGE(false, "Sandbox error 7: UNCAUGHT_EXCEPTION (uncaught WebAssembly exception)");
            break;
        case WASM_RT_TRAP_EXHAUSTION:
            MKXPZ_FORCED_ASSERT_WITH_MESSAGE(false, "Sandbox error 8: EXHAUSTION (out of stack space)");
            break;
        default:
            MKXPZ_FORCED_ASSERT_WITH_MESSAGE(false, "Unknown sandbox error");
            break;
    }
#else
    MKXPZ_FORCED_ASSERT_WITH_MESSAGE(false, "Sandbox trap");
#endif // WASM_RT_DEBUG
}

extern "C" void wasm_rt_allocate_memory(wasm_rt_memory_t *memory, uint32_t initial_pages, uint32_t max_pages, bool is64, uint32_t page_size) {
    if (page_size != WASM_PAGE_SIZE) {
        MKXPZ_THROW(std::bad_alloc());
    }
    if ((memory->size = (uint64_t)initial_pages * WASM_PAGE_SIZE) > WASM_MAX_SIZE) {
        MKXPZ_THROW(std::bad_alloc());
    }
    if ((memory->capacity = (uint64_t)WASM_MIN_PAGES * (uint64_t)WASM_PAGE_SIZE) > WASM_MAX_SIZE) {
        MKXPZ_THROW(std::bad_alloc());
    }
    LOG_PRINTF(RETRO_LOG_DEBUG, "VM memory initialized with capacity %llu bytes (%u pages)\n", (unsigned long long)memory->capacity, (unsigned int)WASM_MIN_PAGES);
    memory->private_data = (uint8_t *)std::malloc(std::max((size_t)memory->size, (size_t)WASM_MIN_PAGES * (size_t)WASM_PAGE_SIZE));
    if (memory->private_data == nullptr) {
        MKXPZ_THROW(std::bad_alloc());
    }
    memory->pages = initial_pages;
#ifdef MKXPZ_BIG_ENDIAN
    memory->data = memory->private_data + std::max((size_t)memory->size, (size_t)WASM_MIN_PAGES * (size_t)WASM_PAGE_SIZE) - (size_t)memory->size;
#else
    memory->data = memory->private_data;
#endif
    std::memset(memory->data, 0, memory->size);
}

extern "C" void wasm_rt_replace_memory(wasm_rt_memory_t *memory, size_t size, size_t capacity) {
    size = size / WASM_PAGE_SIZE * WASM_PAGE_SIZE;
    capacity = capacity / WASM_PAGE_SIZE * WASM_PAGE_SIZE;

    if (size > capacity || capacity > WASM_MAX_SIZE) {
        MKXPZ_THROW(std::bad_alloc());
    }

    if (capacity != memory->capacity) {
        std::free(memory->private_data);
        if ((memory->private_data = (uint8_t *)std::malloc(capacity)) == nullptr) {
            MKXPZ_THROW(std::bad_alloc());
        }
        memory->capacity = capacity;
    }

    memory->pages = size / WASM_PAGE_SIZE;
    memory->size = size;
#ifdef MKXPZ_BIG_ENDIAN
    memory->data = memory->private_data + std::max((size_t)memory->size, (size_t)WASM_MIN_PAGES * (size_t)WASM_PAGE_SIZE) - (size_t)memory->size;
#else
    memory->data = memory->private_data;
#endif
    std::memset(memory->data, 0, memory->size);
}

extern "C" uint32_t wasm_rt_grow_memory(wasm_rt_memory_t *memory, uint32_t pages) {
    uint32_t new_pages = memory->pages + pages;
    if (new_pages < memory->pages) { // Unsigned integer overflow
        return -1;
    }

    uint64_t new_size = (uint64_t)new_pages * WASM_PAGE_SIZE;
    if (new_size > WASM_MAX_SIZE) {
        return -1;
    }

    LOG_PRINTF(RETRO_LOG_DEBUG, "VM memory grown to %llu bytes (%u pages)\n", (unsigned long long)new_size, (unsigned int)new_pages);

    if (new_size > memory->capacity) {
        size_t new_capacity = memory->capacity;
        if (new_capacity < memory->capacity) { // Unsigned integer overflow
            return -1;
        }
        while (new_size > new_capacity) {
            // Increase capacity by 12.5%
            new_capacity += new_capacity >> 3;
            if (new_capacity < memory->capacity) { // Unsigned integer overflow
                return -1;
            }
        }
        LOG_PRINTF(RETRO_LOG_DEBUG, "VM memory reallocation changed memory capacity from %llu bytes to %llu bytes\n", (unsigned long long)memory->capacity, (unsigned long long)new_capacity);
        uint8_t *new_private_data = (uint8_t *)std::realloc(memory->private_data, new_capacity);
        if (new_private_data == nullptr) {
            return -1;
        }
#ifdef MKXPZ_BIG_ENDIAN
        uint8_t *old_data = new_private_data + ((size_t)memory->capacity - (size_t)memory->size);
        std::memmove(old_data + ((size_t)new_capacity - (size_t)memory->capacity), old_data, memory->size);
#endif // MKXPZ_BIG_ENDIAN
        memory->capacity = new_capacity;
        memory->private_data = new_private_data;
    }

#ifdef MKXPZ_BIG_ENDIAN
    memory->data = memory->private_data + ((size_t)memory->capacity - (size_t)new_size);
    std::memset(memory->data, 0, new_size - memory->size);
#else
    memory->data = memory->private_data;
    std::memset(memory->data + memory->size, 0, new_size - memory->size);
#endif // MKXPZ_BIG_ENDIAN

    uint32_t old_pages = memory->pages;
    memory->pages = new_pages;
    memory->size = new_size;
    return old_pages;
}

extern "C" void wasm_rt_free_memory(wasm_rt_memory_t *memory) {
    std::free(memory->private_data);
}

extern "C" void wasm_rt_allocate_funcref_table(wasm_rt_funcref_table_t *table, uint32_t elements, uint32_t max_elements) {
    table->private_data = new std::vector<wasm_rt_funcref_t>(elements);
    table->data = ((std::vector<wasm_rt_funcref_t> *)table->private_data)->data();
    table->size = elements;
}

extern "C" void wasm_rt_free_funcref_table(wasm_rt_funcref_table_t *table) {
    delete (std::vector<wasm_rt_funcref_t> *)table->private_data;
}

extern "C" uint32_t wasm_rt_push_funcref(wasm_rt_funcref_table_t *table, wasm_rt_funcref_t funcref) {
    if (table->size == (uint32_t)-1) {
        MKXPZ_THROW(std::bad_alloc());
    }
    ((std::vector<wasm_rt_funcref_t> *)table->private_data)->push_back(funcref);
    table->data = ((std::vector<wasm_rt_funcref_t> *)table->private_data)->data();
    return table->size++;
}
