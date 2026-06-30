/*
** sandbox-serial-core.h
**
** This file is part of mkxp.
**
** Copyright (C) 2026 The mkxp-z authors
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

#ifndef MKXPZ_SANDBOX_SERIAL_CORE_H
#define MKXPZ_SANDBOX_SERIAL_CORE_H
#include "core.cpp"
#endif // MKXPZ_SANDBOX_SERIAL_CORE_H

#define RESERVE(bytes) do { \
    if (max_size < (bytes)) { \
        return false; \
    } \
} while (0)

#define RESERVE_SER_FAIL(bytes) do { \
    if (max_size < (bytes)) { \
        SER_FAIL; \
    } \
} while (0)

#define RESERVE_DESER_FAIL(bytes) do { \
    if (max_size < (bytes)) { \
        DESER_FAIL; \
    } \
} while (0)

#define ADVANCE(bytes) do { \
    data = (uint8_t *)data + (bytes); \
    max_size -= (bytes); \
} while (0)

#define SER_FAIL do { ser_fail(); return false; } while (0)
#define SER_OBJECTS_BEGIN_DETAIL(_r, _data, T) sandbox_ptr_map<T>::sandbox_serialize_begin();
#define SER_OBJECTS_BEGIN do { BOOST_PP_SEQ_FOR_EACH(SER_OBJECTS_BEGIN_DETAIL, _, SANDBOX_TYPENUM_TYPES) } while (0)
#define SER_OBJECTS_END_DETAIL(_r, _data, T) sandbox_ptr_map<T>::sandbox_serialize_end();
#define SER_OBJECTS_END do { BOOST_PP_SEQ_FOR_EACH(SER_OBJECTS_END_DETAIL, _, SANDBOX_TYPENUM_TYPES) } while (0)
#define SER_OBJECTS_END_FAIL do { SER_OBJECTS_END; return false; } while (0)

#define DESER_FAIL do { deinit_sandbox(); return false; } while (0)
#define DESER_OBJECTS_BEGIN_DETAIL(_r, _data, T) sandbox_ptr_map<T>::sandbox_deserialize_begin();
#define DESER_OBJECTS_BEGIN do { BOOST_PP_SEQ_FOR_EACH(DESER_OBJECTS_BEGIN_DETAIL, _, SANDBOX_TYPENUM_TYPES) } while (0)
#define DESER_OBJECTS_END_DETAIL(_r, _data, T) sandbox_ptr_map<T>::sandbox_deserialize_end();
#define DESER_OBJECTS_END do { BOOST_PP_SEQ_FOR_EACH(DESER_OBJECTS_END_DETAIL, _, SANDBOX_TYPENUM_TYPES) } while (0)
#define DESER_OBJECTS_END_FAIL do { DESER_OBJECTS_END; sb()->vacant_object_keys.clear(); sb()->objects.clear(); DESER_FAIL; } while (0)

static void ser_fail() {
    std::string message("This game is using more memory (" + std::to_string(((unsigned long long)sb()->memory_size() + 1048575) / 1048576) + " mebibytes) than the maximum save state size; increase the maximum save state size in the core options");
    LOG_PRINTF(RETRO_LOG_ERROR, "%s\n", message.c_str());
    display_message(RETRO_LOG_ERROR, message.c_str());
}

extern "C" RETRO_API bool retro_serialize(void *data, size_t len) {
#ifdef MKXPZ_RETRO_NO_SAVE_STATES
    return false;
#else
    if (mkxp_retro::sandbox.has_value() && !shared_state_initialized.load_relaxed()) {
        init_shared_state();
    }
    assert(mkxp_retro::sandbox.has_value() == shared_state_initialized.load_relaxed());

    if (!mkxp_retro::sandbox.has_value()) {
        return false;
    }

    wasm_size_t max_size = len;

    // Write 4-byte magic number: "MKXP" for big-endian platforms, "mkxp" for little-endian platforms
    RESERVE_SER_FAIL(4);
#ifdef MKXPZ_BIG_ENDIAN
    std::memcpy(data, "MKXP", 4);
#else
    std::memcpy(data, "mkxp", 4);
#endif // MKXPZ_BIG_ENDIAN
    ADVANCE(4);

    // Write 4-byte version: 1
    if (!sandbox_serialize((uint32_t)1, data, max_size)) SER_FAIL;

    // Write mkxp-z version
    if (!sandbox_serialize(MKXPZ_VERSION "/" MKXPZ_GIT_HASH, data, max_size)) SER_FAIL;

    // Write 20-byte Ruby revision
    RESERVE_SER_FAIL(sizeof ruby_revision);
    std::memcpy(data, ruby_revision, sizeof ruby_revision);
    ADVANCE(sizeof ruby_revision);

    // Write 32-byte hash of binding-sandbox source files
    RESERVE_SER_FAIL(sizeof MKXPZ_BINDING_SANDBOX_HASH - 1);
    std::memcpy(data, MKXPZ_BINDING_SANDBOX_HASH, sizeof MKXPZ_BINDING_SANDBOX_HASH - 1);
    ADVANCE(sizeof MKXPZ_BINDING_SANDBOX_HASH - 1);

    // Write the capacity of the VM memory
    if (!sandbox_serialize(sb()->memory_capacity(), data, max_size)) SER_FAIL;

    {
        // Write the size of the VM memory
        wasm_size_t memory_size = sb()->memory_size();
        if (!sandbox_serialize(memory_size, data, max_size)) SER_FAIL;

        // Write the VM memory itself
        RESERVE_SER_FAIL(memory_size);
        sb()->copy_memory_to(data);
        ADVANCE(memory_size);
    }

    // Write the number of sandbox fibers
    if (!sandbox_serialize((wasm_size_t)sb()->fiber_list.size(), data, max_size)) SER_FAIL;

    for (const auto &fiber : sb()->fiber_list) {
        // Write the key of the fiber
        if (!sandbox_serialize(std::get<0>(fiber.key), data, max_size)) SER_FAIL;
        if (!sandbox_serialize(std::get<1>(fiber.key), data, max_size)) SER_FAIL;
        if (!sandbox_serialize(std::get<2>(fiber.key), data, max_size)) SER_FAIL;
        if (!sandbox_serialize(std::get<3>(fiber.key), data, max_size)) SER_FAIL;

        // Write the stack index of the fiber
        if (!sandbox_serialize(fiber.stack_index, data, max_size)) SER_FAIL;

        // Write the number of frames in the fiber
        if (!sandbox_serialize(std::max((wasm_size_t)fiber.stack.size(), (wasm_size_t)fiber.deser_stack.size()), data, max_size)) SER_FAIL;

        // Write the stack pointer and state of each frame
        for (const auto &frame : fiber.stack) {
            if (!sandbox_serialize(frame.get_stack_pointer(), data, max_size)) SER_FAIL;
            if (!sandbox_serialize((int32_t)frame, data, max_size)) SER_FAIL;
        }
        if (fiber.deser_stack.size() > fiber.stack.size()) {
            for (auto it = fiber.deser_stack.begin() + fiber.stack.size(); it != fiber.deser_stack.end(); ++it) {
                if (!sandbox_serialize(it->stack_ptr, data, max_size)) SER_FAIL;
                if (!sandbox_serialize(it->state, data, max_size)) SER_FAIL;
            }
        }
    }

    // Write the sandbox state
    if (!sandbox_serialize(sb()->get_machine_stack_pointer(), data, max_size)) SER_FAIL;
    if (!sandbox_serialize(sb()->get_asyncify_state(), data, max_size)) SER_FAIL;
    if (!sandbox_serialize(sb()->get_asyncify_data(), data, max_size)) SER_FAIL;
    if (!sandbox_serialize(frame_count, data, max_size)) SER_FAIL;
    if (!sandbox_serialize(frame_time.load_relaxed(), data, max_size)) SER_FAIL;
    if (!sandbox_serialize(frame_time_remainder, data, max_size)) SER_FAIL;
    if (!sandbox_serialize(retro_run_count, data, max_size)) SER_FAIL;
    if (!sandbox_serialize(sb().sandbox_yield_state, data, max_size)) SER_FAIL;
    if (!sandbox_serialize(sb().cheats, data, max_size)) SER_FAIL;

    // Write the pseudorandom number generator state and open WASI file descriptors
    if (!sb().sandbox_serialize_wasi(data, max_size)) SER_FAIL;

    Bitmap::syncDiffs();
    SER_OBJECTS_BEGIN;

    // Write the number of objects, then each object
    if (!sandbox_serialize((wasm_size_t)sb()->objects.size(), data, max_size)) SER_OBJECTS_END_FAIL;
    wasm_size_t num_free_objects = 0;
    for (const auto &object : sb()->objects) {
        if (object.typenum == 0) {
            ++num_free_objects;
        } else {
            MKXPZ_FORCED_ASSERT(object.typenum <= SANDBOX_NUM_TYPENUMS);
            if (num_free_objects > 0) {
                if (!sandbox_serialize((wasm_size_t)0, data, max_size)) SER_OBJECTS_END_FAIL;
                if (!sandbox_serialize(num_free_objects, data, max_size)) SER_OBJECTS_END_FAIL;
                num_free_objects = 0;
            }
            if (!sandbox_serialize(object.typenum, data, max_size)) SER_OBJECTS_END_FAIL;
            if (typenum_table[object.typenum - 1].is_disposable) {
                bool is_disposed = typenum_table[object.typenum - 1].is_disposed(object.ptr);
                if (!sandbox_serialize(is_disposed, data, max_size)) SER_OBJECTS_END_FAIL;
                if (!is_disposed) {
                    if (!typenum_table[object.typenum - 1].serialize(object.ptr, data, max_size)) SER_OBJECTS_END_FAIL;
                }
            } else {
                if (!typenum_table[object.typenum - 1].serialize(object.ptr, data, max_size)) SER_OBJECTS_END_FAIL;
            }
        }
    }
    if (num_free_objects > 0) {
        if (!sandbox_serialize((wasm_size_t)0, data, max_size)) SER_OBJECTS_END_FAIL;
        if (!sandbox_serialize(num_free_objects, data, max_size)) SER_OBJECTS_END_FAIL;
        num_free_objects = 0;
    }

    // Write the transition map and movie, if applicable
    if (!sandbox_serialize(sb().transitioning, data, max_size)) SER_FAIL;
    if (!sandbox_serialize(sb().trans_map != nullptr, data, max_size)) SER_FAIL;
    if (sb().trans_map != nullptr) {
        MKXPZ_FORCED_ASSERT(!sb().trans_map->isDisposed());
        if (!sb().trans_map->sandbox_serialize_without_hires(data, max_size)) SER_FAIL;
        Exception e;
        Bitmap *hires = sb().trans_map->getHires(e);
        MKXPZ_FORCED_ASSERT(e.is_ok());
        if (!sandbox_serialize(hires != nullptr, data, max_size)) SER_FAIL;
        if (hires != nullptr) {
            if (!hires->sandbox_serialize_without_hires(data, max_size)) SER_FAIL;
        }
    }
    if (!sandbox_serialize(sb().get_movie_from_main_thread() != nullptr, data, max_size)) SER_FAIL;
    if (sb().get_movie_from_main_thread() != nullptr) {
        if (!Graphics::sandbox_serialize_movie(sb().get_movie_from_main_thread(), data, max_size)) SER_FAIL;
    }

    // Write the default font
    if (!Font::sandbox_serialize_default(data, max_size)) SER_FAIL;

    SER_OBJECTS_END;

    // Write the graphics state
    if (!sandbox_serialize((int32_t)shState->graphics().width(), data, max_size)) SER_FAIL;
    if (!sandbox_serialize((int32_t)shState->graphics().height(), data, max_size)) SER_FAIL;
    if (!sandbox_serialize((uint32_t)av_info.geometry.base_width, data, max_size)) SER_FAIL;
    if (!sandbox_serialize((uint32_t)av_info.geometry.base_height, data, max_size)) SER_FAIL;
    if (!sandbox_serialize((int32_t)shState->graphics().getFrameRate(), data, max_size)) SER_FAIL;
    if (!sandbox_serialize((int32_t)shState->graphics().getFrameCount(), data, max_size)) SER_FAIL;
    if (!sandbox_serialize((int32_t)shState->graphics().getBrightness(), data, max_size)) SER_FAIL;
    if (!sandbox_serialize(shState->graphics().getFullscreen(), data, max_size)) SER_FAIL;
    if (!sandbox_serialize(shState->graphics().getShowCursor(), data, max_size)) SER_FAIL;
    if (!sandbox_serialize(shState->graphics().getScale(), data, max_size)) SER_FAIL;
    if (!sandbox_serialize(shState->graphics().getFrameskip(), data, max_size)) SER_FAIL;
    if (!sandbox_serialize(shState->graphics().getFixedAspectRatio(), data, max_size)) SER_FAIL;
    if (!sandbox_serialize((int32_t)shState->graphics().getSmoothScaling(), data, max_size)) SER_FAIL;
    if (!sandbox_serialize(shState->graphics().getIntegerScaling(), data, max_size)) SER_FAIL;
    if (!sandbox_serialize(shState->graphics().getLastMileScaling(), data, max_size)) SER_FAIL;
    if (!sandbox_serialize(shState->graphics().getThreadsafe(), data, max_size)) SER_FAIL;
    if (!sandbox_serialize(shState->graphics().frozen(), data, max_size)) SER_FAIL;
    if (shState->graphics().frozen()) {
        RESERVE_SER_FAIL((size_t)4 * shState->graphics().frozenPixels.size());
        std::memcpy(data, shState->graphics().frozenPixels.data(), (size_t)4 * shState->graphics().frozenPixels.size());
        ADVANCE((size_t)4 * shState->graphics().frozenPixels.size());
    }

    // Write the audio state
    if (!audio->sandbox_serialize(data, max_size)) SER_FAIL;

    std::memset(data, 0, max_size);
    return true;
#endif // MKXPZ_RETRO_NO_SAVE_STATES
}

extern "C" RETRO_API bool retro_unserialize(const void *data, size_t len) {
#ifdef MKXPZ_RETRO_NO_SAVE_STATES
    return false;
#else
    if (mkxp_retro::sandbox.has_value() && !shared_state_initialized.load_relaxed()) {
        init_shared_state();
    }
    assert(mkxp_retro::sandbox.has_value() == shared_state_initialized.load_relaxed());

    if (!mkxp_retro::sandbox.has_value()) {
        return false;
    }

    wasm_size_t max_size = len;

    // Check endianness of save state, and enable byte swapping if it's not the same as that of the current machine
    RESERVE(4);
#ifdef MKXPZ_BIG_ENDIAN
    if (!std::memcmp(data, "MKXP", 4))
#else
    if (!std::memcmp(data, "mkxp", 4))
#endif // MKXPZ_BIG_ENDIAN
        deser_swap_bytes = false;
#ifdef MKXPZ_BIG_ENDIAN
    else if (!std::memcmp(data, "mkxp", 4))
#else
    else if (!std::memcmp(data, "MKXP", 4))
#endif // MKXPZ_BIG_ENDIAN
        deser_swap_bytes = true;
    else
        return false;
    ADVANCE(4);

    // Check version
    {
        uint32_t version;
        if (!sandbox_deserialize(version, data, max_size)) return false;
        if (version != 1) return false;
    }

    // Read mkxp-z version that the save state was created by
    std::string mkxpz_version;
    if (!sandbox_deserialize(mkxpz_version, data, max_size)) return false;

    // Make sure the Ruby revision matches that of that version of mkxp-z, since save state compatibility breaks when the Ruby version changes
    RESERVE(sizeof ruby_revision);
    if (std::memcmp(data, ruby_revision, sizeof ruby_revision)) {
        LOG_PRINTF(RETRO_LOG_ERROR, "Failed to load save state because it uses a different Ruby version than the current version of mkxp-z; try using mkxp-z version %s to load this save state instead\n", mkxpz_version.c_str());
        display_message(RETRO_LOG_ERROR, (std::string("Incompatible save state; use mkxp-z version ") + mkxpz_version + " to load this save state instead").c_str());
        return false;
    }
    ADVANCE(sizeof ruby_revision);

    // Make sure the hash of the binding-sandbox source files matches that of that version of mkxp-z, since save state compatibility breaks when the sandbox bindings are modified
    RESERVE(sizeof MKXPZ_BINDING_SANDBOX_HASH - 1);
    if (std::memcmp(data, MKXPZ_BINDING_SANDBOX_HASH, sizeof MKXPZ_BINDING_SANDBOX_HASH - 1)) {
        LOG_PRINTF(RETRO_LOG_ERROR, "Failed to load save state because the sandbox bindings used are incompatible with the current version of mkxp-z; try using mkxp-z version %s to load this save state instead\n", mkxpz_version.c_str());
        display_message(RETRO_LOG_ERROR, (std::string("Incompatible save state; use mkxp-z version ") + mkxpz_version + " to load this save state instead").c_str());
        return false;
    }
    ADVANCE(sizeof MKXPZ_BINDING_SANDBOX_HASH - 1);

    {
        // Read the VM memory capacity and size
        wasm_size_t memory_capacity;
        if (!sandbox_deserialize(memory_capacity, data, max_size)) return false;
        wasm_size_t memory_size;
        if (!sandbox_deserialize(memory_size, data, max_size)) return false;
        RESERVE(memory_size);
        const void *memory = data;
        ADVANCE(memory_size);

        // Read sandbox fibers
        wasm_size_t num_fibers;
        if (!sandbox_deserialize(num_fibers, data, max_size)) DESER_FAIL;

        for (auto &fiber : sb()->fiber_list) {
            for (auto &frame : fiber.stack) {
                // Make sure the `end()` methods of the existing stack frames don't run when we call `sb()->fiber_list.clear()` a few lines from now
                frame.forget_end();
            }
        }
        sb()->fiber_map.clear();
        sb()->fiber_list.clear();
        sb()->fiber_map.reserve(num_fibers);

        while (num_fibers > 0) {
            // Read the key of the fiber
            std::remove_const<decltype(sb()->fiber_list.back().key)>::type key;
            if (!sandbox_deserialize(std::get<0>(key), data, max_size)) DESER_FAIL;
            if (!sandbox_deserialize(std::get<1>(key), data, max_size)) DESER_FAIL;
            if (!sandbox_deserialize(std::get<2>(key), data, max_size)) DESER_FAIL;
            if (!sandbox_deserialize(std::get<3>(key), data, max_size)) DESER_FAIL;

            // Construct the fiber
            auto &fiber = *sb()->fiber_map.emplace(key, sb()->fiber_list.emplace(sb()->fiber_list.end(), key)).first->second;

            // Read the stack index of the fiber
            if (!sandbox_deserialize(fiber.stack_index, data, max_size)) DESER_FAIL;

            // Read sandbox frames
            wasm_size_t num_frames;
            if (!sandbox_deserialize(num_frames, data, max_size)) DESER_FAIL;
            fiber.deser_stack.reserve(num_frames);
            while (num_frames > 0) {
                wasm_ptr_t stack_pointer;
                if (!sandbox_deserialize(stack_pointer, data, max_size)) DESER_FAIL;
                int32_t state;
                if (!sandbox_deserialize(state, data, max_size)) DESER_FAIL;
                fiber.deser_stack.emplace_back(stack_pointer, state);
                --num_frames;
            }

            --num_fibers;
        }

        // Read the VM memory
        sb()->copy_memory_from(memory, memory_size, memory_capacity, deser_swap_bytes);
    }

    // Read the sandbox state
    {
        wasm_ptr_t value;
        if (!sandbox_deserialize(value, data, max_size)) DESER_FAIL;
        sb()->set_machine_stack_pointer(value);
    }
    {
        uint8_t value;
        if (!sandbox_deserialize(value, data, max_size)) DESER_FAIL;
        sb()->set_asyncify_state(value);
    }
    {
        wasm_ptr_t value;
        if (!sandbox_deserialize(value, data, max_size)) DESER_FAIL;
        sb()->set_asyncify_data(value);
    }
    if (!sandbox_deserialize(frame_count, data, max_size)) DESER_FAIL;
    {
        uint64_t value;
        if (!sandbox_deserialize(value, data, max_size)) DESER_FAIL;
        frame_time = value;
    }
    if (!sandbox_deserialize(frame_time_remainder, data, max_size)) DESER_FAIL;
    if (!sandbox_deserialize(retro_run_count, data, max_size)) DESER_FAIL;
    if (!sandbox_deserialize(sb().sandbox_yield_state, data, max_size)) DESER_FAIL;
    if (!sandbox_deserialize(sb().cheats, data, max_size)) DESER_FAIL;

    // Read the pseudorandom number generator state and open WASI file descriptors
    if (!sb().sandbox_deserialize_wasi(data, max_size)) DESER_FAIL;

    Bitmap::syncDiffs();
    DESER_OBJECTS_BEGIN;
    for (const auto &object : sb()->objects) {
        if (object.typenum > 0) {
            typenum_table[object.typenum - 1].deserialize_begin(object.ptr, false);
        }
    }
    if (sb().trans_map != nullptr) {
        sb().trans_map->sandbox_deserialize_begin(false);
    }

    // Read objects
    sb()->vacant_object_keys.clear();
    std::vector<wasm_objkey_t> vacant_object_keys;
    wasm_objkey_t object_key = 1;
    wasm_size_t num_objects;
    if (!sandbox_deserialize(num_objects, data, max_size)) DESER_OBJECTS_END_FAIL;
    sb()->objects.resize(num_objects);
    while (object_key <= num_objects) {
        wasm_size_t typenum;
        if (!sandbox_deserialize(typenum, data, max_size)) DESER_OBJECTS_END_FAIL;
        if (typenum == 0) {
            wasm_size_t num_free_objects;
            if (!::sandbox_deserialize(num_free_objects, data, max_size)) DESER_OBJECTS_END_FAIL;
            if (object_key - 1 + num_free_objects > num_objects || object_key + num_free_objects < object_key) DESER_OBJECTS_END_FAIL;

            // Destroy objects that currently exist but don't exist in the save state
            for (wasm_size_t i = object_key; i < object_key + num_free_objects; ++i) {
                auto &object = sb()->objects[i - 1];
                if (object.typenum > 0) {
                    MKXPZ_FORCED_ASSERT(object.typenum <= SANDBOX_NUM_TYPENUMS);
                    typenum_table[object.typenum - 1].destroy(object.ptr);
                    object.typenum = 0;
                }
                vacant_object_keys.push_back(i);
            }

            object_key += num_free_objects;
        } else {
            if (typenum > SANDBOX_NUM_TYPENUMS) DESER_OBJECTS_END_FAIL;

            bool should_be_disposed;
            if (typenum_table[typenum - 1].is_disposable) {
                if (!sandbox_deserialize(should_be_disposed, data, max_size)) DESER_OBJECTS_END_FAIL;
            } else {
                should_be_disposed = false;
            }

            // Destroy and recreate objects that don't match the type in the save state, or are currently disposed but not disposed in the save state
            auto &object = sb()->objects[object_key - 1];
            bool is_currently_disposed = object.typenum == 0 || typenum_table[object.typenum - 1].is_disposed(object.ptr);
            bool should_create = object.typenum != typenum || (is_currently_disposed && !should_be_disposed);
            bool should_destroy = should_create && object.typenum > 0;
            if (should_destroy) {
                typenum_table[object.typenum - 1].destroy(object.ptr);
            }
            if (should_create) {
                object.typenum = typenum;
                object.ptr = typenum_table[typenum - 1].construct();
                if (object.ptr == nullptr) DESER_OBJECTS_END_FAIL;
                is_currently_disposed = false;
                typenum_table[typenum - 1].deserialize_begin(object.ptr, true);
            }

            // Deserialize the object
            if (!should_be_disposed) {
                if (!typenum_table[typenum - 1].deserialize(object.ptr, data, max_size)) DESER_OBJECTS_END_FAIL;
            } else if (!is_currently_disposed) {
                typenum_table[typenum - 1].dispose(object.ptr);
            }

            // Add it to the swizzle map so that other objects that reference this one will be able to see it
            auto it = swizzle_map.find(object_key);
            if (it == swizzle_map.end()) {
                swizzle_map.emplace(object_key, sandbox_swizzle_info(object.ptr, typenum));
            } else {
                it->second.set_ptr(object.ptr, typenum);
            }
            ++object_key;
        }
    }

    // Read transition map and movie
    if (!sandbox_deserialize(sb().transitioning, data, max_size)) DESER_OBJECTS_END_FAIL;
    {
        bool have_trans_map;
        if (!sandbox_deserialize(have_trans_map, data, max_size)) DESER_OBJECTS_END_FAIL;
        if (have_trans_map) {
            if (!sb().transitioning) {
                DESER_OBJECTS_END_FAIL;
            }
            Exception e;
            bool is_new = sb().trans_map == nullptr;
            if (is_new) {
                sb().trans_map = new Bitmap(e, 1, 1, true);
                if (e.is_error()) {
                    DESER_OBJECTS_END_FAIL;
                }
                sb().trans_map->sandbox_deserialize_begin(true);
            }
            Bitmap *hires = sb().trans_map->getHires(e);
            if (e.is_error()) {
                DESER_OBJECTS_END_FAIL;
            }
            if (hires != nullptr) {
                hires->sandbox_deserialize_begin(is_new);
            }
            if (!sb().trans_map->sandbox_deserialize_without_hires(data, max_size)) DESER_OBJECTS_END_FAIL;
            bool have_trans_map_hires;
            if (!sandbox_deserialize(have_trans_map_hires, data, max_size)) DESER_OBJECTS_END_FAIL;
            if (e.is_error()) {
                DESER_OBJECTS_END_FAIL;
            }
            if (have_trans_map_hires && hires == nullptr) {
                hires = new Bitmap(e, 1, 1, true);
                if (e.is_error()) {
                    DESER_OBJECTS_END_FAIL;
                }
                hires->sandbox_deserialize_begin(true);
            } else if (!have_trans_map_hires && hires != nullptr) {
                delete hires;
                hires = nullptr;
            }
            sb().trans_map->setHiresRaw(e, hires);
            if (hires != nullptr) {
                if (!hires->sandbox_deserialize_without_hires(data, max_size)) DESER_OBJECTS_END_FAIL;
            }
        } else {
            if (sb().trans_map != nullptr) {
                delete sb().trans_map;
            }
            sb().trans_map = nullptr;
        }
    }
    {
        // TODO: movie
        bool have_movie;
        if (!sandbox_deserialize(have_movie, data, max_size)) DESER_OBJECTS_END_FAIL;
        if (have_movie) DESER_OBJECTS_END_FAIL;
    }

    // Read the default font
    if (!Font::sandbox_deserialize_default(data, max_size)) return false;

    // Make sure every pointer in the save state has been swizzled
    for (const auto &pair : swizzle_map) {
        if (!pair.second.get_exists()) {
            DESER_OBJECTS_END_FAIL;
        }
    }

    if (sb().trans_map != nullptr) {
        sb().trans_map->sandbox_deserialize_end(false);
        Exception e;
        Bitmap *hires = sb().trans_map->getHires(e);
        if (e.is_error()) {
            DESER_OBJECTS_END_FAIL;
        }
        if (hires != nullptr) {
            hires->sandbox_deserialize_end(false);
        }
    }
    for (const auto &object : sb()->objects) {
        if (object.typenum > 0) {
            typenum_table[object.typenum - 1].deserialize_end(object.ptr, true);
        }
    }
    sb()->vacant_object_keys = boost::container::priority_deque<wasm_objkey_t>(std::less<wasm_objkey_t>(), std::move(vacant_object_keys));
    DESER_OBJECTS_END;

    // Read the graphics state
    {
        int32_t screen_width;
        int32_t screen_height;
        if (!sandbox_deserialize(screen_width, data, max_size)) DESER_FAIL;
        if (!sandbox_deserialize(screen_height, data, max_size)) DESER_FAIL;
        screen_width = std::max((int32_t)1, screen_width);
        screen_height = std::max((int32_t)1, screen_height);
        if (screen_width != shState->graphics().width() || screen_height != shState->graphics().height()) {
            shState->graphics().resizeScreen(screen_width, screen_height, false);
        }
    }
    {
        uint32_t window_width;
        uint32_t window_height;
        if (!sandbox_deserialize(window_width, data, max_size)) DESER_FAIL;
        if (!sandbox_deserialize(window_height, data, max_size)) DESER_FAIL;
        window_width = std::max((uint32_t)1, window_width);
        window_height = std::max((uint32_t)1, window_height);
        if (window_width != av_info.geometry.base_width || window_height != av_info.geometry.base_height) {
            shState->graphics().resizeWindow(window_width, window_height, false);
        }
    }
    {
        int32_t value;
        if (!sandbox_deserialize(value, data, max_size)) DESER_FAIL;
        value = std::max((int32_t)1, value);
        if (value != shState->graphics().getFrameRate()) {
            shState->graphics().setFrameRate(value);
        }
    }
    {
        int32_t value;
        if (!sandbox_deserialize(value, data, max_size)) DESER_FAIL;
        if (value != shState->graphics().getFrameCount()) {
            shState->graphics().setFrameCount(value);
        }
    }
    {
        int32_t value;
        if (!sandbox_deserialize(value, data, max_size)) DESER_FAIL;
        if (value != shState->graphics().getBrightness()) {
            shState->graphics().setBrightness(value);
        }
    }
    {
        bool value;
        if (!sandbox_deserialize(value, data, max_size)) DESER_FAIL;
        if (value != shState->graphics().getFullscreen()) {
            shState->graphics().setFullscreen(value);
        }
    }
    {
        bool value;
        if (!sandbox_deserialize(value, data, max_size)) DESER_FAIL;
        if (value != shState->graphics().getShowCursor()) {
            shState->graphics().setShowCursor(value);
        }
    }
    {
        double value;
        if (!sandbox_deserialize(value, data, max_size)) DESER_FAIL;
        if (value != shState->graphics().getScale()) {
            shState->graphics().setScale(value);
        }
    }
    {
        bool value;
        if (!sandbox_deserialize(value, data, max_size)) DESER_FAIL;
        if (value != shState->graphics().getFrameskip()) {
            shState->graphics().setFrameskip(value);
        }
    }
    {
        bool value;
        if (!sandbox_deserialize(value, data, max_size)) DESER_FAIL;
        if (value != shState->graphics().getFixedAspectRatio()) {
            shState->graphics().setFixedAspectRatio(value);
        }
    }
    {
        int32_t value;
        if (!sandbox_deserialize(value, data, max_size)) DESER_FAIL;
        if (value != shState->graphics().getSmoothScaling()) {
            shState->graphics().setSmoothScaling(value);
        }
    }
    {
        bool value;
        if (!sandbox_deserialize(value, data, max_size)) DESER_FAIL;
        if (value != shState->graphics().getIntegerScaling()) {
            shState->graphics().setIntegerScaling(value);
        }
    }
    {
        bool value;
        if (!sandbox_deserialize(value, data, max_size)) DESER_FAIL;
        if (value != shState->graphics().getLastMileScaling()) {
            shState->graphics().setLastMileScaling(value);
        }
    }
    {
        bool value;
        if (!sandbox_deserialize(value, data, max_size)) DESER_FAIL;
        if (value != shState->graphics().getThreadsafe()) {
            shState->graphics().setThreadsafe(value);
        }
    }
    if (!sandbox_deserialize(shState->graphics().frozen(), data, max_size)) DESER_FAIL;
    if (shState->graphics().frozen()) {
        RESERVE_DESER_FAIL((size_t)4 * (size_t)shState->graphics().width() * (size_t)shState->graphics().height());
        if (shState->graphics().frozenPixels.size() != (size_t)shState->graphics().width() * (size_t)shState->graphics().height()) {
            shState->graphics().frozenPixels.clear();
            shState->graphics().frozenPixels.resize((size_t)shState->graphics().width() * (size_t)shState->graphics().height());
        }
        std::memcpy(shState->graphics().frozenPixels.data(), data, (size_t)4 * (size_t)shState->graphics().width() * (size_t)shState->graphics().height());
        shState->graphics().uploadFrozenPixels();
        ADVANCE((size_t)4 * (size_t)shState->graphics().width() * (size_t)shState->graphics().height());
    }

    // Read the audio state
    if (!audio->sandbox_deserialize(data, max_size)) DESER_FAIL;

    return true;
#endif // MKXPZ_RETRO_NO_SAVE_STATES
}
