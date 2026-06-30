/*
** table-binding.cpp
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

#include "table-binding.h"
#include "serializable-binding.h"
#include "table.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::table_class;
static struct bindings::rb_data_type table_type;

SANDBOX_DEF_ALLOC_WITH_INIT(table_type, new Table(0, 0, 0))

static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t, int32_t, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(check_arity, argc, 1, 3);

                SANDBOX_SLOT(1) = SANDBOX_SLOT(2) = 1;

                SANDBOX_AWAIT_S(0, rb_num2int, sb()->ref<VALUE>(argv, 0));
                SANDBOX_SLOT(0) = std::max(SANDBOX_SLOT(0), (int32_t)0);
                if (argc >= 2) {
                    SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 1));
                    SANDBOX_SLOT(1) = std::max(SANDBOX_SLOT(1), (int32_t)0);
                    if (argc >= 3) {
                        SANDBOX_AWAIT_S(2, rb_num2int, sb()->ref<VALUE>(argv, 2));
                        SANDBOX_SLOT(2) = std::max(SANDBOX_SLOT(2), (int32_t)0);
                    }
                }

                Table *table = get_private_data<Table>(self);
                if (table != nullptr) {
                    table->resize(SANDBOX_SLOT(0), SANDBOX_SLOT(1), SANDBOX_SLOT(2));
                } else {
                    table = new Table(SANDBOX_SLOT(0), SANDBOX_SLOT(1), SANDBOX_SLOT(2));
                    set_private_data(self, table);
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE initialize_copy(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (self != value) {
                    SANDBOX_AWAIT(rb_obj_init_copy, self, value);
                    set_private_data(self, new Table(*get_private_data<Table>(value)));
                }
            }

            return self;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE resize(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t, int32_t, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(check_arity, argc, 1, 3);

                SANDBOX_SLOT(1) = SANDBOX_SLOT(2) = 1;

                SANDBOX_AWAIT_S(0, rb_num2int, sb()->ref<VALUE>(argv, 0));
                SANDBOX_SLOT(0) = std::max(SANDBOX_SLOT(0), (int32_t)0);
                if (argc >= 2) {
                    SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 1));
                    SANDBOX_SLOT(1) = std::max(SANDBOX_SLOT(1), (int32_t)0);
                    if (argc >= 3) {
                        SANDBOX_AWAIT_S(2, rb_num2int, sb()->ref<VALUE>(argv, 2));
                        SANDBOX_SLOT(2) = std::max(SANDBOX_SLOT(2), (int32_t)0);
                    }
                }

                get_private_data<Table>(self)->resize(SANDBOX_SLOT(0), SANDBOX_SLOT(1), SANDBOX_SLOT(2));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE xsize(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(get_private_data<Table>(self)->xSize());
}

static VALUE ysize(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(get_private_data<Table>(self)->ySize());
}

static VALUE zsize(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(get_private_data<Table>(self)->zSize());
}

static VALUE get(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, int32_t, int32_t, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(check_arity, argc, 1, 3);

                SANDBOX_SLOT(2) = SANDBOX_SLOT(3) = 0;

                SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 0));
                if (argc >= 2) {
                    SANDBOX_AWAIT_S(2, rb_num2int, sb()->ref<VALUE>(argv, 1));
                    if (argc >= 3) {
                        SANDBOX_AWAIT_S(3, rb_num2int, sb()->ref<VALUE>(argv, 2));
                    }
                }

                if (SANDBOX_SLOT(1) < 0 || SANDBOX_SLOT(1) >= get_private_data<Table>(self)->xSize() || SANDBOX_SLOT(2) < 0 || SANDBOX_SLOT(2) >= get_private_data<Table>(self)->ySize() || SANDBOX_SLOT(3) < 0 || SANDBOX_SLOT(3) >= get_private_data<Table>(self)->zSize()) {
                    SANDBOX_SLOT(0) = SANDBOX_NIL;
                } else {
                    SANDBOX_AWAIT_S(0, rb_ll2inum, get_private_data<Table>(self)->get(SANDBOX_SLOT(1), SANDBOX_SLOT(2), SANDBOX_SLOT(3)));
                }
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE set(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t, int32_t, int32_t, int16_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(check_arity, argc, 2, 4);

                SANDBOX_SLOT(1) = SANDBOX_SLOT(2) = 0;

                SANDBOX_AWAIT_S(0, rb_num2int, sb()->ref<VALUE>(argv, 0));
                if (argc >= 3) {
                    SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 1));
                    if (argc >= 4) {
                        SANDBOX_AWAIT_S(2, rb_num2int, sb()->ref<VALUE>(argv, 2));
                    }
                }

                SANDBOX_AWAIT_S(3, rb_num2int, sb()->ref<VALUE>(argv, std::min(argc, (int32_t)4) - 1));
                get_private_data<Table>(self)->set(SANDBOX_SLOT(3), SANDBOX_SLOT(0), SANDBOX_SLOT(1), SANDBOX_SLOT(2));

                return sb()->ref<VALUE>(argv, std::min(argc, (int32_t)4) - 1);
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

void table_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        table_type = sb()->rb_data_type("Table", nullptr, dfree, nullptr, nullptr, 0, 0, 0);
        SANDBOX_AWAIT_R(table_class, rb_define_class, "Table", sb()->rb_cObject());
        SANDBOX_AWAIT(rb_define_alloc_func, table_class, alloc);
        SANDBOX_AWAIT(rb_define_method, table_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
        SANDBOX_AWAIT(rb_define_method, table_class, "initialize_copy", (VALUE (*)(ANYARGS))initialize_copy, 1);
        SANDBOX_AWAIT(serializable_binding_init<Table>, table_class);
        SANDBOX_AWAIT(rb_define_method, table_class, "resize", (VALUE (*)(ANYARGS))resize, -1);
        SANDBOX_AWAIT(rb_define_method, table_class, "xsize", (VALUE (*)(ANYARGS))xsize, 0);
        SANDBOX_AWAIT(rb_define_method, table_class, "ysize", (VALUE (*)(ANYARGS))ysize, 0);
        SANDBOX_AWAIT(rb_define_method, table_class, "zsize", (VALUE (*)(ANYARGS))zsize, 0);
        SANDBOX_AWAIT(rb_define_method, table_class, "[]", (VALUE (*)(ANYARGS))get, -1);
        SANDBOX_AWAIT(rb_define_method, table_class, "[]=", (VALUE (*)(ANYARGS))set, -1);
    }
}
