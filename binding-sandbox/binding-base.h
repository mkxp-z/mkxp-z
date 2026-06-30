/*
** binding-base.h
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

#ifndef MKXPZ_SANDBOX_BINDING_BASE_H
#define MKXPZ_SANDBOX_BINDING_BASE_H

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <memory>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <priority_deque.hpp>
#include <boost/type_traits/is_detected.hpp>
#include <boost/container_hash/hash.hpp>
#include <boost/asio/coroutine.hpp>
#include <mkxp-sandbox-ruby.h>
#include "wasm-types.h"
#include "mkxp-polyfill.h"
#include "forced-assert.h"

// LLVM uses a stack alignment of 16 on WebAssembly targets
#define WASMSTACKALIGN 16

// Rounds a number up to the nearest multiple of the WebAssembly stack alignment
#define CEIL_WASMSTACKALIGN(x) (((wasm_size_t)(x) + (wasm_size_t)(WASMSTACKALIGN - 1)) & ~(wasm_size_t)(WASMSTACKALIGN - 1))

namespace mkxp_sandbox {
    template <typename...> struct decl_slots {};

    template <typename> struct get_num_slots;
    template <> struct get_num_slots<struct decl_slots<>> {
        static constexpr wasm_size_t value = 0;
    };
    template <typename Head, typename... Tail> struct get_num_slots<struct decl_slots<Head, Tail...>> {
        static constexpr wasm_size_t value = 1 + get_num_slots<struct decl_slots<Tail...>>::value;
    };

    // typename concat_slots<decl_slots<x1, x2, ... xn>, decl_slots<y1, y2, ..., ym>>::type -> decl_slots<x1, x2, ..., xn, y1, y2, ..., ym>
    template <typename, typename> struct concat_slots;
    template <typename... Head, typename... Tail> struct concat_slots<struct decl_slots<Head...>, struct decl_slots<Tail...>> {
        using type = decl_slots<Head..., Tail...>;
    };

    // typename get_last_slot<decl_slots<x1, x2, ..., xn>>::type -> xn
    template <typename> struct get_last_slot;
    template <typename Tail> struct get_last_slot<struct decl_slots<Tail>> {
        using type = Tail;
    };
    template <typename Head, typename... Tail> struct get_last_slot<struct decl_slots<Head, Tail...>> {
        using type = typename get_last_slot<decl_slots<Tail...>>::type;
    };

    // typename pop_last_slot<decl_slots<x1, x2, ..., xn-1, xn>>::type -> decl_slots<x1, x2, ..., xn-1>
    template <typename> struct pop_last_slot;
    template <typename Tail> struct pop_last_slot<struct decl_slots<Tail>> {
        using type = decl_slots<>;
    };
    template <typename Head, typename... Tail> struct pop_last_slot<struct decl_slots<Head, Tail...>> {
        using type = typename concat_slots<struct decl_slots<Head>, typename pop_last_slot<struct decl_slots<Tail...>>::type>::type;
    };

    // `slot_type<i, slots>::type` is the type of the `i`th slot.
    // For example:
    //     typedef decl_slots<uint64_t, uint32_t, uint16_t, uint8_t> slots;
    //     slot_type<0, slots>::type var0; // this variable should be of type `uint64_t`
    //     slot_type<1, slots>::type var1; // this variable should be of type `uint32_t`
    //     slot_type<2, slots>::type var2; // this variable should be of type `uint16_t`
    //     slot_type<3, slots>::type var3; // this variable should be of type `uint8_t`
    template <wasm_size_t Index, typename Slots> struct slot_type;
    template <typename Head, typename... Tail> struct slot_type<0, struct decl_slots<Head, Tail...>> {
	static_assert(std::is_arithmetic<Head>::value, "slots must have numeric types");
        typedef Head type;
    };
    template <wasm_size_t Index, typename Head, typename... Tail> struct slot_type<Index, struct decl_slots<Head, Tail...>> : slot_type<Index - 1, struct decl_slots<Tail...>> {};

    // `slots_size<slots>::value` is the total number of bytes required to store all the slots, including padding bytes between the slots but not including padding bytes after the last slot.
    // For example:
    //     typedef decl_slots<uint64_t, uint32_t, uint16_t, uint8_t> slots;
    //     constexpr wasm_size_t size = slots_size<slots>::value; // should be 15
    template <typename Slots> struct slots_size;
    template <> struct slots_size<struct decl_slots<>> {
        static constexpr wasm_size_t value = 0;
    };
    template <typename Head, typename... Tail> struct slots_size<struct decl_slots<Head, Tail...>> {
	static_assert(std::is_arithmetic<typename get_last_slot<struct decl_slots<Head, Tail...>>::type>::value, "slots must have numeric types");
    private:
        static constexpr wasm_size_t last_size = sizeof(typename get_last_slot<struct decl_slots<Head, Tail...>>::type);
        static constexpr wasm_size_t rest_size = slots_size<typename pop_last_slot<struct decl_slots<Head, Tail...>>::type>::value;
        static constexpr wasm_size_t rest_size_aligned_to_last_size = (rest_size - 1 + last_size) / last_size * last_size;
    public:
        static constexpr wasm_size_t value = rest_size_aligned_to_last_size + last_size;
    };

    template <wasm_size_t Index, typename> struct slot_offset_nothrow;
    template <wasm_size_t Index> struct slot_offset_nothrow<Index, struct decl_slots<>> {
        static constexpr wasm_size_t value = 0;
    };
    template <wasm_size_t Index, typename Head, typename... Tail> struct slot_offset_nothrow<Index, struct decl_slots<Head, Tail...>> {
        static constexpr wasm_size_t value = get_num_slots<struct decl_slots<Head, Tail...>>::value <= Index
            ? slots_size<struct decl_slots<Head, Tail...>>::value
            : slot_offset_nothrow<Index, typename pop_last_slot<struct decl_slots<Head, Tail...>>::type>::value;
    };

    // `slot_offset<i, slots>::value` is the byte offset of the `i`th slot.
    // For example:
    //     typedef decl_slots<uint64_t, uint32_t, uint16_t, uint8_t> slots;
    //     constexpr wasm_size_t slot0_offset = slot_offset<0, slots>::value; // should be 0
    //     constexpr wasm_size_t slot1_offset = slot_offset<1, slots>::value; // should be 8
    //     constexpr wasm_size_t slot2_offset = slot_offset<2, slots>::value; // should be 12
    //     constexpr wasm_size_t slot3_offset = slot_offset<3, slots>::value; // should be 14
    template <wasm_size_t Index, typename Slots> struct slot_offset;
    template <wasm_size_t Index, typename Head, typename... Tail> struct slot_offset<Index, struct decl_slots<Head, Tail...>> {
	static_assert(Index < get_num_slots<struct decl_slots<Head, Tail...>>::value, "index out of range");
	static constexpr wasm_size_t value = slot_offset_nothrow<Index, struct decl_slots<Head, Tail...>>::value;
    };

    // If the type `T::slots` exists,
    // then `declared_slots_size<T>::value` is equal to `slots_size<typename T::slots>::value` (i.e. the total size of the slots used by `T`).
    // Otherwise, it's equal to 0.
    template <typename T, typename Dummy = void> struct declared_slots_size;
    template <typename T> using slots_declaration = typename T::slots;
    template <typename T> struct declared_slots_size<T, typename std::enable_if<boost::is_detected<slots_declaration, T>::value>::type> {
        static constexpr wasm_size_t value = slots_size<typename T::slots>::value;
    };
    template <typename T> struct declared_slots_size<T, typename std::enable_if<!boost::is_detected<slots_declaration, T>::value>::type> {
        static constexpr wasm_size_t value = 0;
    };

    // Gets a pointer to the given address in sandbox memory.
    // Unlike `sandbox_ref`, the address does not need to be aligned.
    template <typename T> void *sandbox_ptr_unaligned(struct w2c_ruby &instance, wasm_ptr_t address) noexcept {
        static_assert(std::is_arithmetic<T>::value, "can only get references to numeric values in the sandbox");
        MKXPZ_FORCED_ASSERT(address + (wasm_ptr_t)sizeof(T) >= address && address + (wasm_ptr_t)sizeof(T) <= instance.w2c_memory.size);
#ifdef MKXPZ_BIG_ENDIAN
        return instance.w2c_memory.data + (instance.w2c_memory.size - address - sizeof(T));
#else
        return instance.w2c_memory.data + address;
#endif // MKXPZ_BIG_ENDIAN
    }

    // Gets a pointer to the given index in the array at a given address in sandbox memory.
    // Unlike `sandbox_ref`, the address does not need to be aligned.
    template <typename T> void *sandbox_ptr_unaligned(struct w2c_ruby &instance, wasm_ptr_t array_address, wasm_size_t array_index) noexcept {
        MKXPZ_FORCED_ASSERT(array_address + array_index >= array_address);
        return sandbox_ptr_unaligned<T>(instance, array_address + array_index * sizeof(T));
    }

    // Gets a reference to the value stored at a given address in sandbox memory.
    // Make sure the address is aligned, or this function will abort.
    template <typename T> T &sandbox_ref(struct w2c_ruby &instance, wasm_ptr_t address) noexcept {
        MKXPZ_FORCED_ASSERT(address % sizeof(T) == 0);
        return *(T *)sandbox_ptr_unaligned<T>(instance, address);
    }

    // Gets a reference to the value stored at the given index in the array at a given address in sandbox memory.
    template <typename T> T &sandbox_ref(struct w2c_ruby &instance, wasm_ptr_t array_address, wasm_size_t array_index) noexcept {
        MKXPZ_FORCED_ASSERT(array_index <= array_index * (wasm_size_t)sizeof(T));
        MKXPZ_FORCED_ASSERT(array_address + array_index * (wasm_size_t)sizeof(T) >= array_address);
        return sandbox_ref<T>(array_address + array_index * sizeof(T));
    }

    // Checks if the array with the given address and size in bytes is within the bounds of sandbox memory.
    // If it isn't, aborts. Otherwise, does nothing.
    void sandbox_check_bounds(struct w2c_ruby &instance, wasm_ptr_t address, wasm_size_t size) noexcept;

    // Gets the length of a string stored at a given address in sandbox memory.
    wasm_size_t sandbox_strlen(struct w2c_ruby &instance, wasm_ptr_t address) noexcept;

    struct sandbox_str_guard {
    private:
#ifdef MKXPZ_BIG_ENDIAN
        std::string str;
#endif // MKXPZ_BIG_ENDIAN
        const char *ptr;

    public:
#ifdef MKXPZ_BIG_ENDIAN
        inline sandbox_str_guard(std::string &&str) : str(str), ptr(nullptr) {}
#endif // MKXPZ_BIG_ENDIAN
        inline sandbox_str_guard(const char *str) : ptr(str) {}
        inline operator const char *() const {
#ifdef MKXPZ_BIG_ENDIAN
            if (ptr == nullptr) {
                return str.c_str();
            } else {
                return ptr;
            }
#else
            return ptr;
#endif // MKXPZ_BIG_ENDIAN
        }
    };

    // Gets a string stored at a given address in sandbox memory.
    struct sandbox_str_guard sandbox_str(struct w2c_ruby &instance, wasm_ptr_t address) noexcept;

    // Gets a string stored at a given address in sandbox memory.
    struct sandbox_str_guard sandbox_str(struct w2c_ruby &instance, wasm_ptr_t address, wasm_size_t max_size) noexcept;

    // Copies a string into a sandbox memory address.
    void sandbox_strcpy(struct w2c_ruby &instance, wasm_ptr_t dst_address, const char *src) noexcept;

    // Copies a string into a sandbox memory address.
    void sandbox_strncpy_s(struct w2c_ruby &instance, wasm_ptr_t dst_address, const char *src, wasm_size_t max_size) noexcept;

    // Copies an array of length `num_elements` into a sandbox memory address.
    template <typename T> void sandbox_arycpy(struct w2c_ruby &instance, wasm_ptr_t dst_address, const T *src, wasm_size_t num_elements) noexcept {
        MKXPZ_FORCED_ASSERT(dst_address < instance.w2c_memory.size && instance.w2c_memory.size - dst_address >= num_elements * sizeof(T));
#ifdef MKXPZ_BIG_ENDIAN
        for (wasm_size_t i = 0; i < num_elements; ++i) {
            std::memcpy(sandbox_ptr_unaligned<T>(instance, dst_address, i), src + i, sizeof(T));
        }
#else
        if (num_elements > 0) {
            std::memcpy(sandbox_ptr_unaligned<T>(instance, dst_address), src, num_elements * sizeof(T));
        }
#endif
    }

    struct typenum_table_entry {
        void *(*construct)();
        void (*destroy)(void *self);
        void (*dispose)(void *self);
        bool (*is_disposed)(void *self);
        bool is_disposable;
        bool (*serialize)(const void *self, void *&data, wasm_size_t &max_size);
        bool (*deserialize)(void *self, const void *&data, wasm_size_t &max_size);
        void (*deserialize_begin)(void *self, bool is_new);
        void (*deserialize_end)(void *self, bool is_sandbox_object);
        void (*reinit)(void *self);
    };

    extern const struct typenum_table_entry typenum_table[];
    extern const wasm_size_t typenum_table_size;

    struct binding_base {
    private:
        typedef std::tuple<wasm_ptr_t, wasm_ptr_t, wasm_ptr_t, wasm_ptr_t> key_t;

        struct deser_stack_frame {
            deser_stack_frame(wasm_ptr_t stack_ptr, int32_t state);
            const wasm_ptr_t stack_ptr;
            const int32_t state;
        };

        struct stack_frame {
            friend struct binding_base;
            stack_frame(void *coroutine, void (*end)(void *coroutine), void (*destroy)(void *coroutine), wasm_ptr_t stack_ptr);
            stack_frame(const struct stack_frame &frame) = delete;
            stack_frame(struct stack_frame &&frame) noexcept;
            struct stack_frame &operator=(const struct stack_frame &frame) = delete;
            struct stack_frame &operator=(struct stack_frame &&frame) noexcept;
            ~stack_frame();
            inline operator int32_t() const noexcept {
                boost::asio::detail::coroutine_ref ref = (boost::asio::coroutine *)coroutine;
                int result = ref;
                ref = result; // Prevents the destructor of `boost::asio::detail::coroutine_ref` from messing up the coroutine state
                return (int32_t)result;
            }
            inline void operator=(int32_t value) noexcept {
                (boost::asio::detail::coroutine_ref)(boost::asio::coroutine *)coroutine = (int)value;
            }
            inline wasm_ptr_t get_stack_pointer() const noexcept {
                return stack_ptr;
            }
            inline void forget_end() noexcept {
                end = nullptr;
            }
        private:
            void *coroutine;
            void (*end)(void *coroutine);
            void (*destroy)(void *coroutine);
            wasm_ptr_t stack_ptr;
        };

        struct fiber {
            friend struct binding_base;
            fiber(key_t key) : key(key), stack_index(0) {};
        public:
            const key_t key;
            wasm_size_t stack_index;
            std::vector<struct deser_stack_frame> deser_stack;
            std::vector<struct stack_frame> stack;
        };

        struct object {
            void *ptr;
            // If this is a free object, this is 0.
            // Otherwise, this is a number corresponding to the type of the object.
            wasm_size_t typenum;

            object();
            object(wasm_size_t typenum, void *ptr);
            object(const struct object &object) = delete;
            object(struct object &&object) noexcept;
            struct object &operator=(const struct object &object) = delete;
            struct object &operator=(struct object &&object) noexcept;
            ~object();
        };

    public:
        std::vector<struct object> objects;
        boost::container::priority_deque<wasm_ptr_t> vacant_object_keys;
    private:
        std::shared_ptr<struct w2c_ruby> _instance;
        wasm_ptr_t stack_ptr;

    public:
        std::list<struct fiber> fiber_list;
        std::unordered_map<key_t, decltype(fiber_list)::iterator, boost::hash<key_t>> fiber_map;

        binding_base(std::shared_ptr<struct w2c_ruby> m);
        ~binding_base();
        struct w2c_ruby &instance() const noexcept;
        wasm_ptr_t sandbox_malloc(wasm_size_t);
        void sandbox_free(wasm_ptr_t ptr);
        wasm_ptr_t rtypeddata_data(VALUE obj) const noexcept;
        void rtypeddata_dmark(wasm_ptr_t data, wasm_ptr_t ptr);
        void rtypeddata_dfree(wasm_ptr_t data, wasm_ptr_t ptr);
        wasm_size_t rtypeddata_dsize(wasm_ptr_t data, wasm_ptr_t ptr);
        void rtypeddata_dcompact(wasm_ptr_t data, wasm_ptr_t ptr);

        // Serialization functions
        wasm_size_t memory_capacity() const noexcept;
        wasm_size_t memory_size() const noexcept;
        void copy_memory_to(void *ptr) const noexcept;
        void copy_memory_from(const void *ptr, wasm_size_t size, wasm_size_t capacity, bool swap_bytes) noexcept;

        // Gets a pointer to the given address in sandbox memory.
        // Unlike `sandbox_ref`, the address does not need to be aligned.
        template <typename T> void *ptr_unaligned(wasm_ptr_t address) const noexcept {
            return sandbox_ptr_unaligned<T>(instance(), address);
        }

        // Gets a pointer to the given index in the array at a given address in sandbox memory.
        // Unlike `sandbox_ref`, the address does not need to be aligned.
        template <typename T> void *ptr_unaligned(wasm_ptr_t array_address, wasm_size_t array_index) const noexcept {
            return sandbox_ptr_unaligned<T>(instance(), array_address, array_index);
        }

        // Gets a reference to the value stored at a given address in sandbox memory.
        // Make sure the address is aligned, or this function will abort.
        template <typename T> T &ref(wasm_ptr_t address) const noexcept {
            return sandbox_ref<T>(instance(), address);
        }

        // Gets a reference to the value stored at the given index in the array at a given address in sandbox memory.
        // Make sure the address is aligned, or this function will abort.
        template <typename T> T &ref(wasm_ptr_t array_address, wasm_size_t array_index) const noexcept {
            return ref<T>(array_address + array_index * sizeof(T));
        }

        // Checks if the array with the given address and size in bytes is within the bounds of sandbox memory.
        // If it isn't, aborts. Otherwise, does nothing.
        void check_bounds(wasm_ptr_t address, wasm_size_t size) const noexcept;

        // Gets the length of a string stored at a given address in sandbox memory.
        wasm_size_t strlen(wasm_ptr_t address) const noexcept;

        // Gets a string stored at a given address in sandbox memory.
        struct sandbox_str_guard str(wasm_ptr_t address) const noexcept;

        // Gets a string stored at a given address in sandbox memory.
        struct sandbox_str_guard str(wasm_ptr_t address, wasm_size_t max_size) const noexcept;

        // Copies a string into a sandbox memory address.
        void strcpy(wasm_ptr_t dst_address, const char *src) const noexcept;

        // Copies a string into a sandbox memory address.
        void strncpy_s(wasm_ptr_t dst_address, const char *src, wasm_size_t max_size) const noexcept;

        // Copies an array of length `num_elements` into a sandbox memory address.
        template <typename T> void arycpy(wasm_ptr_t dst_address, const T *src, wasm_size_t num_elements) const noexcept {
            return sandbox_arycpy(instance(), dst_address, src, num_elements);
        }

        // Creates a new object and returns its key.
        wasm_objkey_t create_object(wasm_size_t typenum, void *ptr);

        // Gets the object with the given key.
        void *get_object(wasm_objkey_t key) const;

        // Returns true if the typenum of the object with the given key matches the given typenum, otherwise false.
        bool check_object_type(wasm_objkey_t key, wasm_size_t typenum) const;

        // Destroys the object with the given key, calling its destructor and freeing its key for use by future objects.
        void destroy_object(wasm_objkey_t key);

        template <typename T> struct stack_frame_guard {
            static_assert(std::is_base_of<boost::asio::coroutine, T>::value, "`T` must be a subclass of `boost::asio::coroutine`");
            static_assert(std::is_trivially_destructible<T>::value, "`T` must not have a custom destructor; define a `void end() noexcept` method instead, which will be run on stack frame destruction");
            friend struct binding_base;

        private:
            T *coroutine;
            struct binding_base *bind;
            struct fiber *fiber;

            template <typename U> using coroutine_end_declaration = decltype(std::declval<U *>()->end());

            template <typename U> static typename std::enable_if<boost::is_detected<coroutine_end_declaration, U>::value>::type end_coroutine(void *coroutine) {
                ((U *)coroutine)->end();
            }

            template <typename U> static typename std::enable_if<!boost::is_detected<coroutine_end_declaration, U>::value>::type end_coroutine(void *coroutine) {}

            static void destroy_coroutine(void *coroutine) {
                delete (T *)coroutine;
            }

            static struct fiber &init_fiber(struct binding_base &bind) {
                key_t key = {
                    bind.ref<wasm_ptr_t>(bind.instance().w2c_mkxp_sandbox_thread),
                    bind.ref<wasm_ptr_t>(bind.instance().w2c_mkxp_sandbox_fiber_entry_point),
                    bind.ref<wasm_ptr_t>(bind.instance().w2c_mkxp_sandbox_fiber_arg0),
                    bind.ref<wasm_ptr_t>(bind.instance().w2c_mkxp_sandbox_fiber_arg1),
                };
                const auto it = bind.fiber_map.find(key);
                if (it != bind.fiber_map.end()) {
                    return *it->second;
                } else {
                    return *bind.fiber_map.emplace(key, bind.fiber_list.emplace(bind.fiber_list.end(), key)).first->second;
                }
            }

            template <typename U> static typename std::enable_if<std::is_constructible<U, struct binding_base &>::value, U *>::type construct_coroutine(struct binding_base &bind) {
                return new U(bind);
            }

            template <typename U> static typename std::enable_if<!std::is_constructible<U, struct binding_base &>::value, U *>::type construct_coroutine(struct binding_base &bind) {
                return new U;
            }

            stack_frame_guard(struct binding_base &b) : bind(&b), fiber(&init_fiber(b)) {
                uint32_t state = w2c_ruby_asyncify_get_state(&b.instance());

                MKXPZ_FORCED_ASSERT(fiber->stack_index <= std::max(fiber->stack.size(), fiber->deser_stack.size()));

                // If Asyncify is rewinding, restore the stack frame from before Asyncify started unwinding
                if (state == 2) {
                    // Restore stack frame from the libretro save state if available
                    if (fiber->stack_index == fiber->stack.size()) {
                        MKXPZ_FORCED_ASSERT(fiber->stack_index != fiber->deser_stack.size());
                        struct deser_stack_frame &deser_frame = fiber->deser_stack[fiber->stack_index++];
                        if (fiber->stack_index == 0) {
                            MKXPZ_THROW(std::bad_alloc());
                        }
                        b.stack_ptr = deser_frame.stack_ptr;
                        coroutine = construct_coroutine<T>(b);
                        fiber->stack.emplace_back(
                            coroutine,
                            end_coroutine<T>,
                            destroy_coroutine,
                            b.stack_ptr
                        );
                        fiber->stack.back() = deser_frame.state;
                        return;
                    }

                    struct stack_frame &frame = fiber->stack[fiber->stack_index++];
                    if (fiber->stack_index == 0) {
                        MKXPZ_THROW(std::bad_alloc());
                    }
                    b.stack_ptr = frame.stack_ptr;
                    coroutine = (T *)frame.coroutine;
                    return;
                }

                // Otherwise, create a new stack frame
                MKXPZ_FORCED_ASSERT(state == 0);

                while (fiber->deser_stack.size() > fiber->stack_index) {
                    fiber->deser_stack.pop_back();
                }
                while (fiber->stack.size() > fiber->stack_index) {
                    bind->stack_ptr = fiber->stack.back().stack_ptr;
                    fiber->stack.pop_back();
                }

                if (++fiber->stack_index == 0) {
                    MKXPZ_THROW(std::bad_alloc());
                }

                b.stack_ptr = w2c_ruby_rb_wasm_get_stack_pointer(&b.instance()) - CEIL_WASMSTACKALIGN(declared_slots_size<T>::value);
                assert(b.stack_ptr % sizeof(VALUE) == 0);
                assert(b.stack_ptr % WASMSTACKALIGN == 0);
                if (declared_slots_size<T>::value != 0) {
                    w2c_ruby_rb_wasm_set_stack_pointer(&b.instance(), b.stack_ptr);
                }
                coroutine = construct_coroutine<T>(b);
                fiber->stack.emplace_back(
                    coroutine,
                    end_coroutine<T>,
                    destroy_coroutine,
                    b.stack_ptr
                );
            }

        public:
            stack_frame_guard(const stack_frame_guard &frame) = delete;

            stack_frame_guard(stack_frame_guard &&frame) noexcept : coroutine(std::exchange(frame.coroutine, nullptr)), bind(std::exchange(frame.bind, nullptr)), fiber(std::exchange(frame.fiber, nullptr)) {}

            struct stack_frame_guard &operator=(const stack_frame_guard &frame) = delete;

            struct stack_frame_guard &operator=(stack_frame_guard &&frame) noexcept {
                coroutine = std::exchange(frame.coroutine, nullptr);
                bind = std::exchange(frame.bind, nullptr);
                fiber = std::exchange(frame.fiber, nullptr);
                return *this;
            }

            ~stack_frame_guard() {
                if (fiber == nullptr) {
                    return;
                }

                assert(fiber->stack_index > 0);
                assert(fiber->stack_index - 1 < fiber->stack.size());

                if (get()->is_complete()) {
                    while (fiber->deser_stack.size() > fiber->stack_index) {
                        fiber->deser_stack.pop_back();
                    }
                    while (fiber->stack.size() > fiber->stack_index) {
                        bind->stack_ptr = fiber->stack.back().stack_ptr;
                        fiber->stack.pop_back();
                    }

                    assert(fiber->stack.size() == fiber->stack_index);

                    w2c_ruby_rb_wasm_set_stack_pointer(&bind->instance(), fiber->stack.back().stack_ptr + CEIL_WASMSTACKALIGN(declared_slots_size<T>::value));
                    bind->stack_ptr = fiber->stack.back().stack_ptr;
                    fiber->stack.pop_back();
                }

                if (--fiber->stack_index > 0) {
                    bind->stack_ptr = fiber->stack[fiber->stack_index - 1].stack_ptr;
                }

                if (fiber->stack.empty()) {
                    const auto map_it = bind->fiber_map.find(fiber->key);
                    assert(map_it != bind->fiber_map.end());
                    const auto list_it = map_it->second;
                    bind->fiber_map.erase(map_it);
                    bind->fiber_list.erase(list_it);
                }
            }

            inline T *get() const noexcept {
                return coroutine;
            }

            inline T &operator()() const noexcept {
                return *get();
            }
        };

        template <typename T> struct stack_frame_guard<T> bind() {
            return *this;
        }

        inline wasm_ptr_t stack_pointer() const noexcept {
            return stack_ptr;
        }

        wasm_ptr_t get_machine_stack_pointer() const noexcept;
        void set_machine_stack_pointer(wasm_ptr_t) noexcept;
        uint8_t get_asyncify_state() const noexcept;
        void set_asyncify_state(uint8_t) noexcept;
        wasm_ptr_t get_asyncify_data() const noexcept;
        void set_asyncify_data(wasm_ptr_t) noexcept;
    };
}

#endif // MKXPZ_SANDBOX_BINDING_BASE_H
