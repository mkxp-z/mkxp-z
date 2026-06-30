/*
** sandbox-serial-util.h
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

#ifndef MKXPZ_SANDBOX_SERIAL_UTIL_H
#define MKXPZ_SANDBOX_SERIAL_UTIL_H

#include "sandbox.h"

#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/size.hpp>
#include <boost/type_traits/is_detected.hpp>

#include "forced-assert.h"

#include "bitmap.h"
#include "etc.h"
#include "font.h"
#include "plane.h"
#include "sprite.h"
#include "table.h"
#include "tilemap.h"
#include "tilemapvx.h"
#include "viewport.h"
#include "window.h"
#include "windowvx.h"

#define SANDBOX_TYPENUM_TYPES \
    (Bitmap) \
    (Color) \
    (ChildPrivate) \
    (Font) \
    (Plane) \
    (Rect) \
    (Sprite) \
    (Table) \
    (Tilemap) \
    (Tilemap::Autotiles) \
    (TilemapVX) \
    (TilemapVX::BitmapArray) \
    (Tone) \
    (Viewport) \
    (Window) \
    (WindowVX) \

#define SANDBOX_NUM_TYPENUMS BOOST_PP_SEQ_SIZE(SANDBOX_TYPENUM_TYPES)

#define _SANDBOX_DEF_GET_TYPENUM_DETAIL(T, num) template <> struct get_typenum<T> { \
    static_assert(num != 0, "typenum should not be 0"); \
    static_assert(num <= SANDBOX_NUM_TYPENUMS, "typenum should not be greater than the number of typenums"); \
    static constexpr wasm_size_t value = num; \
};
#define _SANDBOX_DEF_GET_TYPENUM(_r, _data, T) _SANDBOX_DEF_GET_TYPENUM_DETAIL(T, __COUNTER__ - _get_typenum_counter_start)

namespace mkxp_sandbox {
    template <typename T> struct get_typenum;
    static constexpr wasm_size_t _get_typenum_counter_start = __COUNTER__;
    BOOST_PP_SEQ_FOR_EACH(_SANDBOX_DEF_GET_TYPENUM, _, SANDBOX_TYPENUM_TYPES);

    struct sandbox_swizzle_info {
        template <typename T> sandbox_swizzle_info(T *&ref) : ptr(new std::vector<void **>({(void **)&ref})), typenum(mkxp_sandbox::get_typenum<T>::value), ref_count(1), exists(false) {}
        sandbox_swizzle_info(void *ptr, wasm_size_t typenum);
        sandbox_swizzle_info(const struct sandbox_swizzle_info &) = delete;
        sandbox_swizzle_info(struct sandbox_swizzle_info &&) noexcept;
        struct sandbox_swizzle_info &operator=(const struct sandbox_swizzle_info &) = delete;
        struct sandbox_swizzle_info &operator=(struct sandbox_swizzle_info &&) noexcept;
        ~sandbox_swizzle_info();
        wasm_size_t get_ref_count() const noexcept;
        template <typename T> bool add_ref(T *&ref) {
            if (typenum != mkxp_sandbox::get_typenum<T>::value) {
                return false;
            }
            if (ref_count > 0 && (std::is_same<T, Color>::value || std::is_same<T, Tone>::value || std::is_same<T, Rect>::value || std::is_same<T, Font>::value || std::is_same<T, Tilemap::Autotiles>::value || std::is_same<T, TilemapVX::BitmapArray>::value)) {
                // Don't allow types that are copied by value (Color, Tone, Rect and Font) or autotiles/bitmap arrays to be referenced more than once
                return false;
            }
            if (exists) {
                ref = (T *)ptr;
            } else {
                ref = nullptr;
                ((std::vector<void **> *)ptr)->push_back((void **)&ref);
            }
            ++ref_count;
            return true;
        }
        bool set_ptr(void *ptr, wasm_size_t typenum);
        void *get_ptr() const;
        wasm_size_t get_typenum() const;
        bool get_exists() const;

    private:
        // If `exists` is true, this is a pointer to the object. Otherwise, this is a `std::vector<void **>` of pointers that are waiting to point to the object.
        void *ptr;
        // The type of the object.
        wasm_size_t typenum;
        // The number of times this object is referenced by other objects.
        wasm_size_t ref_count;
        // True if the object has been deserialized, otherwise false.
        bool exists;
    };

    extern std::unordered_map<wasm_size_t, struct sandbox_swizzle_info> swizzle_map;
    extern bool deser_swap_bytes;

    template <typename T> using sandbox_serialize_member_declaration = decltype(std::declval<const T *>()->sandbox_serialize(std::declval<void *&>(), std::declval<wasm_size_t &>()));
    template <typename T> using sandbox_deserialize_member_declaration = decltype(std::declval<T *>()->sandbox_deserialize(std::declval<const void *&>(), std::declval<wasm_size_t &>()));

    template <typename T> typename std::enable_if<std::is_same<T, bool>::value, bool>::type sandbox_serialize(T value, void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<std::is_enum<T>::value, bool>::type sandbox_serialize(T value, void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<!std::is_same<T, bool>::value && !std::is_enum<T>::value && std::is_arithmetic<T>::value, bool>::type sandbox_serialize(T value, void *&data, wasm_size_t &max_size);
    template <typename T> bool sandbox_serialize(const std::vector<T> &value, void *&data, wasm_size_t &max_size);
    template <typename T, typename U> bool sandbox_serialize(const std::pair<T, U> &value, void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<std::is_same<T, char>::value, bool>::type sandbox_serialize(const T *value, void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<std::is_class<T>::value, bool>::type sandbox_serialize(const T *value, void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<std::is_class<T>::value && boost::is_detected<sandbox_serialize_member_declaration, T>::value, bool>::type sandbox_serialize(const T &value, void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<std::is_class<T>::value && !boost::is_detected<sandbox_serialize_member_declaration, T>::value, bool>::type sandbox_serialize(const T &value, void *&data, wasm_size_t &max_size);

    template <typename T> typename std::enable_if<!std::is_const<T>::value && std::is_same<T, bool>::value, bool>::type sandbox_deserialize(T &value, const void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<!std::is_const<T>::value && std::is_enum<T>::value, bool>::type sandbox_deserialize(T &value, const void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<!std::is_const<T>::value && !std::is_same<T, bool>::value && !std::is_enum<T>::value && std::is_arithmetic<T>::value, bool>::type sandbox_deserialize(T &value, const void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<!std::is_const<T>::value, bool>::type sandbox_deserialize(std::vector<T> &value, const void *&data, wasm_size_t &max_size);
    template <typename T, typename U> typename std::enable_if<!std::is_const<T>::value && !std::is_const<U>::value, bool>::type sandbox_deserialize(std::pair<T, U> &value, const void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<!std::is_const<T>::value && std::is_class<T>::value, bool>::type sandbox_deserialize(T *&value, const void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<!std::is_const<T>::value && std::is_class<T>::value && boost::is_detected<sandbox_deserialize_member_declaration, T>::value, bool>::type sandbox_deserialize(T &value, const void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<!std::is_const<T>::value && std::is_class<T>::value && !boost::is_detected<sandbox_deserialize_member_declaration, T>::value, bool>::type sandbox_deserialize(T &value, const void *&data, wasm_size_t &max_size);

    template <typename T> struct sandbox_ptr_map {
    private:
        static std::unordered_map<const T *, mkxp_sandbox::wasm_objkey_t> unswizzle_map;
        static bool is_serializing;
        static bool is_deserializing;

    public:
        static void sandbox_serialize_begin() {
            using namespace mkxp_sandbox;

            MKXPZ_FORCED_ASSERT(!is_deserializing);

            if (is_serializing) {
                return;
            }

            is_serializing = true;
            unswizzle_map.clear();

            wasm_objkey_t key = 0;
            for (const auto &object : sb()->objects) {
                ++key;
                if (object.typenum == get_typenum<T>::value) {
                    unswizzle_map.emplace((T *)object.ptr, key);
                }
            }
        }

        static bool sandbox_serialize(const T *ptr, void *&data, wasm_size_t &max_size) {
            using namespace mkxp_sandbox;

            MKXPZ_FORCED_ASSERT(!is_deserializing);

            if (ptr != nullptr) {
                const auto it = unswizzle_map.find(ptr);
                if (it != unswizzle_map.end()) {
                    if (!mkxp_sandbox::sandbox_serialize(true, data, max_size)) return false;
                    if (!mkxp_sandbox::sandbox_serialize(it->second, data, max_size)) return false;
                } else {
                    if (!mkxp_sandbox::sandbox_serialize(false, data, max_size)) return false;
                }
            } else {
                if (!mkxp_sandbox::sandbox_serialize(false, data, max_size)) return false;
            }

            return true;
        }

        static void sandbox_serialize_end() {
            MKXPZ_FORCED_ASSERT(!is_deserializing);

            is_serializing = false;
            unswizzle_map.clear();
        }

        static void sandbox_deserialize_begin() {
            using namespace mkxp_sandbox;

            MKXPZ_FORCED_ASSERT(!is_serializing);

            if (is_deserializing) {
                return;
            }

            is_deserializing = true;
            swizzle_map.clear();
        }

        static bool sandbox_deserialize(T *&ref, const void *&data, wasm_size_t &max_size) {
            using namespace mkxp_sandbox;

            MKXPZ_FORCED_ASSERT(!is_serializing);

            bool is_not_null;
            if (!mkxp_sandbox::sandbox_deserialize(is_not_null, data, max_size)) return false;

            if (!is_not_null) {
                ref = nullptr;
                return true;
            }

            wasm_objkey_t key;
            if (!mkxp_sandbox::sandbox_deserialize(key, data, max_size)) return false;
            const auto it = swizzle_map.find(key);
            if (it == swizzle_map.end()) {
                swizzle_map.emplace(key, sandbox_swizzle_info(ref));
                return true;
            } else {
                return it->second.add_ref(ref);
            }
        }

        static void sandbox_deserialize_end() {
            MKXPZ_FORCED_ASSERT(!is_serializing);

            is_deserializing = false;
            swizzle_map.clear();
        }
    };

    template <typename T> std::unordered_map<const T *, mkxp_sandbox::wasm_objkey_t> sandbox_ptr_map<T>::unswizzle_map;
    template <typename T> bool sandbox_ptr_map<T>::is_serializing = false;
    template <typename T> bool sandbox_ptr_map<T>::is_deserializing = false;

    template <typename T> typename std::enable_if<std::is_class<T>::value, bool>::type sandbox_serialize(const T *value, void *&data, wasm_size_t &max_size) {
        return sandbox_ptr_map<T>::sandbox_serialize(value, data, max_size);
    }

    template <typename T> typename std::enable_if<!std::is_const<T>::value && std::is_class<T>::value, bool>::type sandbox_deserialize(T *&value, const void *&data, wasm_size_t &max_size) {
        return sandbox_ptr_map<T>::sandbox_deserialize(value, data, max_size);
    }

    template <typename T> bool sandbox_serialize(const std::vector<T> &value, void *&data, wasm_size_t &max_size) {
        if (!sandbox_serialize((wasm_size_t)value.size(), data, max_size)) return false;
        for (const T &item : value) {
            if (!sandbox_serialize(item, data, max_size)) return false;
        }
        return true;
    }

    template <typename T> typename std::enable_if<!std::is_const<T>::value, bool>::type sandbox_deserialize(std::vector<T> &value, const void *&data, wasm_size_t &max_size) {
        wasm_size_t size;
        if (!sandbox_deserialize(size, data, max_size)) return false;
        value.clear();
        value.reserve(size);
        while (size > 0) {
            value.emplace_back();
            if (!sandbox_deserialize(value.back(), data, max_size)) return false;
            --size;
        }
        return true;
    }

    template <typename T, typename U> bool sandbox_serialize(const std::pair<T, U> &value, void *&data, wasm_size_t &max_size) {
        if (!sandbox_serialize(value.first, data, max_size)) return false;
        if (!sandbox_serialize(value.second, data, max_size)) return false;
        return true;
    }

    template <typename T, typename U> typename std::enable_if<!std::is_const<T>::value && !std::is_const<U>::value, bool>::type sandbox_deserialize(std::pair<T, U> &value, const void *&data, wasm_size_t &max_size) {
        if (!sandbox_deserialize(value.first, data, max_size)) return false;
        if (!sandbox_deserialize(value.second, data, max_size)) return false;
        return true;
    }

    template <typename T> typename std::enable_if<std::is_enum<T>::value, bool>::type sandbox_serialize(T value, void *&data, wasm_size_t &max_size) {
        return sandbox_serialize((int32_t)value, data, max_size);
    }

    template <typename T> typename std::enable_if<!std::is_const<T>::value && std::is_enum<T>::value, bool>::type sandbox_deserialize(T &value, const void *&data, wasm_size_t &max_size) {
        int32_t v;
        bool result = sandbox_deserialize(v, data, max_size);
        if (result) {
            value = (T)v;
        }
        return result;
    }

    template <typename T> typename std::enable_if<std::is_class<T>::value && boost::is_detected<sandbox_serialize_member_declaration, T>::value, bool>::type sandbox_serialize(const T &value, void *&data, wasm_size_t &max_size) {
        return value.sandbox_serialize(data, max_size);
    }

    template <typename T> typename std::enable_if<!std::is_const<T>::value && std::is_class<T>::value && boost::is_detected<sandbox_deserialize_member_declaration, T>::value, bool>::type sandbox_deserialize(T &value, const void *&data, wasm_size_t &max_size) {
        return value.sandbox_deserialize(data, max_size);
    }
}

#endif // MKXPZ_SANDBOX_SERIAL_UTIL_H
