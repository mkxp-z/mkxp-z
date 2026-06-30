/*
** sandbox-serial-util.cpp
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

#include "sandbox-serial-util.h"
#include "etc-internal.h"
#include "scene.h"
#include "transform.h"

#ifdef _MSC_VER
#  include <stdlib.h>
#endif // _MSC_VER

using namespace mkxp_sandbox;

#define RESERVE(bytes) do { \
    if (max_size < (bytes)) { \
        return false; \
    } \
} while (0)

#define ADVANCE(bytes) do { \
    data = (uint8_t *)data + (bytes); \
    max_size -= (bytes); \
} while (0)

template <typename T> static typename std::enable_if<std::is_constructible<T>::value, void *>::type construct() {
    return new T;
}

template <typename T> static typename std::enable_if<!std::is_constructible<T>::value && std::is_constructible<T, Exception &>::value, void *>::type construct() {
    Exception e;
    T *obj = new T(e);
    if (e.is_ok()) {
        return obj;
    } else {
        delete obj;
        return nullptr;
    }
}

template <typename T> static void destroy(void *self) {
    if (self != nullptr) {
        delete (T *)self;
    }
}

template <typename T> static typename std::enable_if<std::is_base_of<Disposable, T>::value>::type dispose(void *self) {
    if (self != nullptr) {
        ((T *)self)->dispose();
    }
}

template <typename T> static typename std::enable_if<!std::is_base_of<Disposable, T>::value>::type dispose(void *self) {}

template <typename T> static typename std::enable_if<std::is_base_of<Disposable, T>::value, bool>::type is_disposed(void *self) {
    return self == nullptr || ((T *)self)->isDisposed();
}

template <typename T> static typename std::enable_if<!std::is_base_of<Disposable, T>::value, bool>::type is_disposed(void *self) {
    return self == nullptr;
}

template <typename T> static constexpr bool is_disposable = std::is_base_of<Disposable, T>::value;

template <typename T> static bool serialize(const void *self, void *&data, wasm_size_t &max_size) {
    return ((const T *)self)->sandbox_serialize(data, max_size);
}

template <typename T> static bool deserialize(void *self, const void *&data, wasm_size_t &max_size) {
    return ((T *)self)->sandbox_deserialize(data, max_size);
}

template <typename T> using deserialize_begin_declaration_with_is_new = decltype(std::declval<T *>()->sandbox_deserialize_begin(std::declval<bool>()));
template <typename T> using deserialize_begin_declaration_without_is_new = decltype(std::declval<T *>()->sandbox_deserialize_begin());

template <typename T> static typename std::enable_if<boost::is_detected<deserialize_begin_declaration_with_is_new, T>::value>::type deserialize_begin(void *self, bool is_new) {
    ((T *)self)->sandbox_deserialize_begin(is_new);
}

template <typename T> static typename std::enable_if<!boost::is_detected<deserialize_begin_declaration_with_is_new, T>::value && boost::is_detected<deserialize_begin_declaration_without_is_new, T>::value>::type deserialize_begin(void *self, bool is_new) {
    ((T *)self)->sandbox_deserialize_begin();
}

template <typename T> static typename std::enable_if<!boost::is_detected<deserialize_begin_declaration_with_is_new, T>::value && !boost::is_detected<deserialize_begin_declaration_without_is_new, T>::value>::type deserialize_begin(void *self, bool is_new) {}

template <typename T> using deserialize_end_declaration_with_is_sandbox_object = decltype(std::declval<T *>()->sandbox_deserialize_end(std::declval<bool>()));
template <typename T> using deserialize_end_declaration_without_is_sandbox_object = decltype(std::declval<T *>()->sandbox_deserialize_end());

template <typename T> static typename std::enable_if<boost::is_detected<deserialize_end_declaration_with_is_sandbox_object, T>::value>::type deserialize_end(void *self, bool is_sandbox_object) {
    ((T *)self)->sandbox_deserialize_end(is_sandbox_object);
}

template <typename T> static typename std::enable_if<!boost::is_detected<deserialize_end_declaration_with_is_sandbox_object, T>::value && boost::is_detected<deserialize_end_declaration_without_is_sandbox_object, T>::value>::type deserialize_end(void *self, bool is_sandbox_object) {
    ((T *)self)->sandbox_deserialize_end();
}

template <typename T> static typename std::enable_if<!boost::is_detected<deserialize_end_declaration_with_is_sandbox_object, T>::value && !boost::is_detected<deserialize_end_declaration_without_is_sandbox_object, T>::value>::type deserialize_end(void *self, bool is_sandbox_object) {}

template <typename T> using reinit_declaration = decltype(std::declval<T *>()->sandbox_reinit());

template <typename T> static typename std::enable_if<boost::is_detected<reinit_declaration, T>::value>::type reinit(void *self) {
    ((T *)self)->sandbox_reinit();
}

template <typename T> static typename std::enable_if<!boost::is_detected<reinit_declaration, T>::value>::type reinit(void *self) {}

#define _SANDBOX_DEF_TYPENUM_TABLE_ENTRY(_r, _data, T) { \
    construct<T>, \
    destroy<T>, \
    dispose<T>, \
    is_disposed<T>, \
    is_disposable<T>, \
    serialize<T>, \
    deserialize<T>, \
    deserialize_begin<T>, \
    deserialize_end<T>, \
    reinit<T>, \
},
extern const struct typenum_table_entry mkxp_sandbox::typenum_table[SANDBOX_NUM_TYPENUMS] = {BOOST_PP_SEQ_FOR_EACH(_SANDBOX_DEF_TYPENUM_TABLE_ENTRY, _, SANDBOX_TYPENUM_TYPES)};
extern const wasm_size_t mkxp_sandbox::typenum_table_size = SANDBOX_NUM_TYPENUMS;

std::unordered_map<wasm_size_t, struct sandbox_swizzle_info> mkxp_sandbox::swizzle_map;
bool mkxp_sandbox::deser_swap_bytes = false;

sandbox_swizzle_info::sandbox_swizzle_info(void *ptr, wasm_size_t typenum) : ptr(ptr), typenum(typenum), ref_count(0), exists(true) {}

sandbox_swizzle_info::sandbox_swizzle_info(struct sandbox_swizzle_info &&info) noexcept : ptr(std::exchange(info.ptr, nullptr)), typenum(info.typenum), ref_count(std::exchange(info.ref_count, 1)), exists(std::exchange(info.exists, true)) {}

struct sandbox_swizzle_info &sandbox_swizzle_info::operator=(struct sandbox_swizzle_info &&info) noexcept {
    ptr = std::exchange(info.ptr, nullptr);
    typenum = info.typenum;
    ref_count = std::exchange(info.ref_count, 1);
    exists = std::exchange(info.exists, true);
    return *this;
}

sandbox_swizzle_info::~sandbox_swizzle_info() {
    if (!exists) {
        delete (std::vector<void **> *)ptr;
    }
}

wasm_size_t sandbox_swizzle_info::get_ref_count() const noexcept {
    return ref_count;
}

bool sandbox_swizzle_info::set_ptr(void *ptr, wasm_size_t typenum) {
    if (this->typenum != typenum) {
        // Don't allow pointers of mismatching type
        return false;
    }
    if (exists && ptr != this->ptr) {
        // Don't allow setting the pointer more than once
        return false;
    }
    if (!exists) {
        for (void **ref : *(std::vector<void **> *)this->ptr) {
            *ref = ptr;
        }
        delete (std::vector<void **> *)this->ptr;
        exists = true;
        this->ptr = ptr;
    }
    return true;
}

void *sandbox_swizzle_info::get_ptr() const {
    return exists ? ptr : nullptr;
}

wasm_size_t sandbox_swizzle_info::get_typenum() const {
    return typenum;
}

bool sandbox_swizzle_info::get_exists() const {
    return exists;
}

template <> bool mkxp_sandbox::sandbox_serialize(bool value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint8_t));
    std::memcpy(data, &value, sizeof(uint8_t));
    ADVANCE(sizeof(uint8_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(bool &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint8_t));
    std::memcpy(&value, data, sizeof(uint8_t));
    ADVANCE(sizeof(uint8_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(int8_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(int8_t));
    std::memcpy(data, &value, sizeof(int8_t));
    ADVANCE(sizeof(int8_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(int8_t &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(int8_t));
    std::memcpy(&value, data, sizeof(int8_t));
    ADVANCE(sizeof(int8_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(uint8_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint8_t));
    std::memcpy(data, &value, sizeof(uint8_t));
    ADVANCE(sizeof(uint8_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(uint8_t &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint8_t));
    std::memcpy(&value, data, sizeof(uint8_t));
    ADVANCE(sizeof(uint8_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(int16_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(int16_t));
    std::memcpy(data, &value, sizeof(int16_t));
    ADVANCE(sizeof(int16_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(int16_t &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(int16_t));
    std::memcpy(&value, data, sizeof(int16_t));
    if (deser_swap_bytes) {
#ifdef _MSC_VER
        static_assert(sizeof(unsigned short) == sizeof(int16_t), "unsigned short should be 16 bits");
        value = (int16_t)_byteswap_ushort((unsigned short)value);
#else
        value = (int16_t)__builtin_bswap16((uint16_t)value);
#endif // _MSC_VER
    }
    ADVANCE(sizeof(int16_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(uint16_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint16_t));
    std::memcpy(data, &value, sizeof(uint16_t));
    ADVANCE(sizeof(uint16_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(uint16_t &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint16_t));
    std::memcpy(&value, data, sizeof(uint16_t));
    if (deser_swap_bytes) {
#ifdef _MSC_VER
        static_assert(sizeof(unsigned short) == sizeof(uint16_t), "unsigned short should be 16 bits");
        value = (uint16_t)_byteswap_ushort((unsigned short)value);
#else
        value = __builtin_bswap16(value);
#endif // _MSC_VER
    }
    ADVANCE(sizeof(uint16_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(int32_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(int32_t));
    std::memcpy(data, &value, sizeof(int32_t));
    ADVANCE(sizeof(int32_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(int32_t &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(int32_t));
    std::memcpy(&value, data, sizeof(int32_t));
    if (deser_swap_bytes) {
#ifdef _MSC_VER
        static_assert(sizeof(unsigned long) == sizeof(int32_t), "unsigned long should be 32 bits");
        value = (int32_t)_byteswap_ulong((unsigned long)value);
#else
        value = (int32_t)__builtin_bswap32((uint32_t)value);
#endif // _MSC_VER
    }
    ADVANCE(sizeof(int32_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(uint32_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint32_t));
    std::memcpy(data, &value, sizeof(uint32_t));
    ADVANCE(sizeof(uint32_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(uint32_t &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint32_t));
    std::memcpy(&value, data, sizeof(uint32_t));
    if (deser_swap_bytes) {
#ifdef _MSC_VER
        static_assert(sizeof(unsigned long) == sizeof(uint32_t), "unsigned long should be 32 bits");
        value = (uint32_t)_byteswap_ulong((unsigned long)value);
#else
        value = __builtin_bswap32(value);
#endif // _MSC_VER
    }
    ADVANCE(sizeof(uint32_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(int64_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(int64_t));
    std::memcpy(data, &value, sizeof(int64_t));
    ADVANCE(sizeof(int64_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(int64_t &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(int64_t));
    std::memcpy(&value, data, sizeof(int64_t));
    if (deser_swap_bytes) {
#ifdef _MSC_VER
        value = (int64_t)_byteswap_uint64((unsigned __int64)value);
#else
        value = (int64_t)__builtin_bswap64((uint64_t)value);
#endif // _MSC_VER
    }
    ADVANCE(sizeof(int64_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(uint64_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint64_t));
    std::memcpy(data, &value, sizeof(uint64_t));
    ADVANCE(sizeof(uint64_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(uint64_t &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint64_t));
    std::memcpy(&value, data, sizeof(uint64_t));
    if (deser_swap_bytes) {
#ifdef _MSC_VER
        value = (uint64_t)_byteswap_uint64((unsigned __int64)value);
#else
        value = __builtin_bswap64(value);
#endif // _MSC_VER
    }
    ADVANCE(sizeof(uint64_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(float value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(float));
    std::memcpy(data, &value, sizeof(float));
    ADVANCE(sizeof(float));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(float &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(float));
    if (deser_swap_bytes) {
        uint32_t tmp;
        std::memcpy(&tmp, data, sizeof(float));
#ifdef _MSC_VER
        static_assert(sizeof(unsigned long) == sizeof(float), "unsigned long should be 32 bits");
        tmp = (uint32_t)_byteswap_ulong((unsigned long)tmp);
#else
        tmp = __builtin_bswap32(tmp);
#endif // _MSC_VER
        std::memcpy(&value, &tmp, sizeof(float));
    } else {
        std::memcpy(&value, data, sizeof(float));
    }
    ADVANCE(sizeof(float));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(double value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(double));
    std::memcpy(data, &value, sizeof(double));
    ADVANCE(sizeof(double));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(double &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(double));
    if (deser_swap_bytes) {
        uint64_t tmp;
        std::memcpy(&tmp, data, sizeof(double));
#ifdef _MSC_VER
        tmp = (uint64_t)_byteswap_uint64((unsigned __int64)tmp);
#else
        tmp = __builtin_bswap64(tmp);
#endif // _MSC_VER
        std::memcpy(&value, &tmp, sizeof(double));
    } else {
        std::memcpy(&value, data, sizeof(double));
    }
    ADVANCE(sizeof(double));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const char *value, void *&data, wasm_size_t &max_size) {
    wasm_size_t size = std::strlen(value);
    if (!sandbox_serialize(size, data, max_size)) return false;
    RESERVE(size);
    std::memcpy(data, value, size);
    ADVANCE(size);
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const std::string &value, void *&data, wasm_size_t &max_size) {
    wasm_size_t size = value.length();
    if (!sandbox_serialize(size, data, max_size)) return false;
    RESERVE(size);
    std::memcpy(data, value.c_str(), size);
    ADVANCE(size);
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(std::string &value, const void *&data, wasm_size_t &max_size) {
    wasm_size_t size;
    if (!sandbox_deserialize(size, data, max_size)) return false;
    RESERVE(size);
    value.clear();
    value.resize(size);
    char *str = &value[0];
    std::memcpy(str, data, size);
    ADVANCE(size);
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const NormValue &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize((int32_t)value.unNorm, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(NormValue &value, const void *&data, wasm_size_t &max_size) {
    if (!sandbox_deserialize((int32_t &)value.unNorm, data, max_size)) return false;
    value.unNorm = clamp(value.unNorm, 0, 255);
    value.norm = value.unNorm / 255.0f;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const Vec2 &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.x, data, max_size)) return false;
    if (!sandbox_serialize(value.y, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(Vec2 &value, const void *&data, wasm_size_t &max_size) {
    if (!sandbox_deserialize(value.x, data, max_size)) return false;
    if (!sandbox_deserialize(value.y, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const Vec4 &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.x, data, max_size)) return false;
    if (!sandbox_serialize(value.y, data, max_size)) return false;
    if (!sandbox_serialize(value.z, data, max_size)) return false;
    if (!sandbox_serialize(value.w, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(Vec4 &value, const void *&data, wasm_size_t &max_size) {
    if (!sandbox_deserialize(value.x, data, max_size)) return false;
    if (!sandbox_deserialize(value.y, data, max_size)) return false;
    if (!sandbox_deserialize(value.z, data, max_size)) return false;
    if (!sandbox_deserialize(value.w, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const Vec2i &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize((int32_t)value.x, data, max_size)) return false;
    if (!sandbox_serialize((int32_t)value.y, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(Vec2i &value, const void *&data, wasm_size_t &max_size) {
    if (!sandbox_deserialize((int32_t &)value.x, data, max_size)) return false;
    if (!sandbox_deserialize((int32_t &)value.y, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const FloatRect &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.x, data, max_size)) return false;
    if (!sandbox_serialize(value.y, data, max_size)) return false;
    if (!sandbox_serialize(value.w, data, max_size)) return false;
    if (!sandbox_serialize(value.h, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(FloatRect &value, const void *&data, wasm_size_t &max_size) {
    if (!sandbox_deserialize(value.x, data, max_size)) return false;
    if (!sandbox_deserialize(value.y, data, max_size)) return false;
    if (!sandbox_deserialize(value.w, data, max_size)) return false;
    if (!sandbox_deserialize(value.h, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const IntRect &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize((int32_t)value.x, data, max_size)) return false;
    if (!sandbox_serialize((int32_t)value.y, data, max_size)) return false;
    if (!sandbox_serialize((int32_t)value.w, data, max_size)) return false;
    if (!sandbox_serialize((int32_t)value.h, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(IntRect &value, const void *&data, wasm_size_t &max_size) {
    if (!sandbox_deserialize((int32_t &)value.x, data, max_size)) return false;
    if (!sandbox_deserialize((int32_t &)value.y, data, max_size)) return false;
    if (!sandbox_deserialize((int32_t &)value.w, data, max_size)) return false;
    if (!sandbox_deserialize((int32_t &)value.h, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const Scene::Geometry &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.rect, data, max_size)) return false;
    if (!sandbox_serialize(value.orig, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(Scene::Geometry &value, const void *&data, wasm_size_t &max_size) {
    if (!sandbox_deserialize(value.rect, data, max_size)) return false;
    if (!sandbox_deserialize(value.orig, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const Transform &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.getPosition(), data, max_size)) return false;
    if (!sandbox_serialize(value.getOrigin(), data, max_size)) return false;
    if (!sandbox_serialize(value.getScale(), data, max_size)) return false;
    if (!sandbox_serialize(value.getGlobalOffset(), data, max_size)) return false;
    if (!sandbox_serialize(value.getRotation(), data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(Transform &value, const void *&data, wasm_size_t &max_size) {
    {
        Vec2 position;
        if (!sandbox_deserialize(position, data, max_size)) return false;
        if (position != value.getPosition()) {
            value.setPosition(position);
        }
    }
    {
        Vec2 origin;
        if (!sandbox_deserialize(origin, data, max_size)) return false;
        if (origin != value.getOrigin()) {
            value.setOrigin(origin);
        }
    }
    {
        Vec2 scale;
        if (!sandbox_deserialize(scale, data, max_size)) return false;
        if (scale != value.getScale()) {
            value.setScale(scale);
        }
    }
    {
        Vec2i offset;
        if (!sandbox_deserialize(offset, data, max_size)) return false;
        if (offset != value.getGlobalOffset()) {
            value.setGlobalOffset(offset);
        }
    }
    {
        float rotation;
        if (!sandbox_deserialize(rotation, data, max_size)) return false;
        if (rotation != value.getRotation()) {
            value.setRotation(rotation);
        }
    }
    return true;
}
