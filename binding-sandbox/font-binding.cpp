/*
** font-binding.cpp
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

#include "font-binding.h"
#include "etc-binding.h"
#include "font.h"
#include "sharedstate.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::font_class;
static struct bindings::rb_data_type font_type;

SANDBOX_DEF_ALLOC(font_type);

struct collect_strings : boost::asio::coroutine {
    typedef decl_slots<wasm_ptr_t, wasm_size_t, wasm_size_t, VALUE, VALUE> slots;

    void operator()(VALUE obj) {
        BOOST_ASIO_CORO_REENTER (this) {
            sb().font_names_buffer.clear();
            SANDBOX_AWAIT_S(3, rb_obj_is_kind_of, obj, sb()->rb_cString());
            if (SANDBOX_VALUE_TO_BOOL(SANDBOX_SLOT(3))) {
                SANDBOX_AWAIT_S(0, rb_string_value_cstr, &obj);
                sb().font_names_buffer.push_back((const char *)sb()->str(SANDBOX_SLOT(0)));
            } else {
                SANDBOX_AWAIT_S(3, rb_obj_is_kind_of, obj, sb()->rb_cArray());
                if (SANDBOX_VALUE_TO_BOOL(SANDBOX_SLOT(3))) {
                    SANDBOX_AWAIT_S(2, get_length, obj);
                    for (SANDBOX_SLOT(1) = 0; SANDBOX_SLOT(1) < SANDBOX_SLOT(2); ++SANDBOX_SLOT(1)) {
                        SANDBOX_AWAIT_S(4, rb_ary_entry, obj, SANDBOX_SLOT(1));
                        /* Non-string objects are tolerated (ignored) */
                        SANDBOX_AWAIT_S(3, rb_obj_is_kind_of, SANDBOX_SLOT(4), sb()->rb_cString());
                        if (SANDBOX_VALUE_TO_BOOL(SANDBOX_SLOT(3))) {
                            SANDBOX_AWAIT_S(0, rb_string_value_cstr, &SANDBOX_SLOT(4));
                            sb().font_names_buffer.push_back((const char *)sb()->str(SANDBOX_SLOT(0)));
                        }
                    }
                }
            }
        }
    }
};

static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (argc == 0) {
                    SANDBOX_AWAIT_S(0, rb_iv_get, font_class, "default_name");
                    if (has_private_data<Font>(self)) {
                        Font *f = new Font;
                        *get_private_data<Font>(self) = *f;
                        delete f;
                    } else {
                        set_private_data(self, new Font);
                    }
                } else if (argc == 1) {
                    SANDBOX_SLOT(0) = sb()->ref<VALUE>(argv, 0);
                    SANDBOX_AWAIT(collect_strings, SANDBOX_SLOT(0));
                    if (has_private_data<Font>(self)) {
                        Font *f = new Font(&sb().font_names_buffer);
                        *get_private_data<Font>(self) = *f;
                        delete f;
                    } else {
                        set_private_data(self, new Font(&sb().font_names_buffer));
                    }
                } else {
                    SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 1));
                    SANDBOX_SLOT(0) = sb()->ref<VALUE>(argv, 0);
                    SANDBOX_AWAIT(collect_strings, SANDBOX_SLOT(0));
                    if (has_private_data<Font>(self)) {
                        Font *f = new Font(&sb().font_names_buffer, SANDBOX_SLOT(1));
                        *get_private_data<Font>(self) = *f;
                        delete f;
                    } else {
                        set_private_data(self, new Font(&sb().font_names_buffer, SANDBOX_SLOT(1)));
                    }
                }
                sb().font_names_buffer.clear();

                get_private_data<Font>(self)->initDynAttribs();

                /* This is semantically wrong; the new Font object should take
                 * a dup'ed object here in case of an array. Ditto for the setters.
                 * However the same bug/behavior exists in all RM versions. */
                SANDBOX_AWAIT(rb_iv_set, self, "name", SANDBOX_SLOT(0));

                SANDBOX_AWAIT(wrap_property, self, &get_private_data<Font>(self)->getColor(), "color", color_class);

                if (rgssVer >= 3) {
                    SANDBOX_AWAIT(wrap_property, self, &get_private_data<Font>(self)->getOutColor(), "out_color", color_class);
                }
            }

            return SANDBOX_NIL;
        }

        void end() noexcept {
            sb().font_names_buffer.clear();
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE initialize_copy(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (self == value) {
                    return self;
                }

                SANDBOX_AWAIT(rb_obj_init_copy, self, value);
                set_private_data(self, new Font(*get_private_data<Font>(value)));

                get_private_data<Font>(self)->initDynAttribs();

                SANDBOX_AWAIT(wrap_property, self, &get_private_data<Font>(self)->getColor(), "color", color_class);

                if (rgssVer >= 3) {
                    SANDBOX_AWAIT(wrap_property, self, &get_private_data<Font>(self)->getOutColor(), "out_color", color_class);
                }
            }

            return self;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE get_name(VALUE self) {
    return sb()->bind<struct rb_iv_get>()()(self, "name");
}

static VALUE set_name(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(collect_strings, value);
                get_private_data<Font>(self)->setName(sb().font_names_buffer);
                SANDBOX_AWAIT(rb_iv_set, self, "name", value);
            }

            return value;
        }

        void end() noexcept {
            sb().font_names_buffer.clear();
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE get_size(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(get_private_data<Font>(self)->getSize());
}

static VALUE set_size(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self, VALUE value) {
            typedef decl_slots<int32_t> slots;

            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_num2int, value);
                SANDBOX_GUARD(get_private_data<Font>(self)->setSize(sb().e, SANDBOX_SLOT(0)));
            }
            return value;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

SANDBOX_DEF_PROP_B(Font, Bold, bold);
SANDBOX_DEF_PROP_B(Font, Italic, italic);
SANDBOX_DEF_PROP_OBJ_VAL(Font, Color, color_class, Color, color);
SANDBOX_DEF_PROP_B(Font, Shadow, shadow);
SANDBOX_DEF_PROP_B(Font, Outline, outline);
SANDBOX_DEF_PROP_OBJ_VAL(Font, Color, color_class, OutColor, out_color);

static VALUE get_default_name(VALUE self) {
    return sb()->bind<struct rb_iv_get>()()(self, "default_name");
}

static VALUE set_default_name(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(collect_strings, value);
                Font::setDefaultName(sb().font_names_buffer, shState->fontState());
                SANDBOX_AWAIT(rb_iv_set, self, "default_name", value);
            }

            return value;
        }

        void end() noexcept {
            sb().font_names_buffer.clear();
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

SANDBOX_DEF_CLASS_PROP_I(Font, DefaultSize, default_size);
SANDBOX_DEF_CLASS_PROP_B(Font, DefaultBold, default_bold);
SANDBOX_DEF_CLASS_PROP_B(Font, DefaultItalic, default_italic);
SANDBOX_DEF_CLASS_PROP_OBJ_VAL(Font, Color, color_class, DefaultColor, default_color);
SANDBOX_DEF_CLASS_PROP_B(Font, DefaultShadow, default_shadow);
SANDBOX_DEF_CLASS_PROP_B(Font, DefaultOutline, default_outline);
SANDBOX_DEF_CLASS_PROP_OBJ_VAL(Font, Color, color_class, DefaultOutColor, default_out_color);

static VALUE exist(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t, VALUE> slots;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(1, rb_obj_is_kind_of, value, sb()->rb_cString());
                if (SANDBOX_VALUE_TO_BOOL(SANDBOX_SLOT(1))) {
                    SANDBOX_AWAIT_S(0, rb_string_value_cstr, &value);
                    return SANDBOX_BOOL_TO_VALUE(Font::doesExist(sb()->str(SANDBOX_SLOT(0))));
                } else {
                    return SANDBOX_BOOL_TO_VALUE(Font::doesExist(nullptr));
                }
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

void font_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        font_type = sb()->rb_data_type("Font", nullptr, dfree, nullptr, nullptr, 0, 0, 0);
        SANDBOX_AWAIT_R(font_class, rb_define_class, "Font", sb()->rb_cObject());
        SANDBOX_AWAIT(rb_define_alloc_func, font_class, alloc);

        SANDBOX_AWAIT(wrap_property, font_class, &Font::getDefaultColor(), "default_color", color_class);

        if (rgssVer >= 3) {
            SANDBOX_AWAIT(wrap_property, font_class, &Font::getDefaultOutColor(), "default_out_color", color_class);
        }

        if (Font::getInitialDefaultNames().size() == 1) {
            SANDBOX_AWAIT_S(1, rb_utf8_str_new_cstr, Font::getInitialDefaultNames()[0].c_str());
        } else {
            SANDBOX_AWAIT_S(1, rb_ary_new_capa, Font::getInitialDefaultNames().size());
            for (SANDBOX_SLOT(0) = 0; SANDBOX_SLOT(0) < Font::getInitialDefaultNames().size(); ++SANDBOX_SLOT(0)) {
                SANDBOX_AWAIT_S(2, rb_utf8_str_new_cstr, Font::getInitialDefaultNames()[SANDBOX_SLOT(0)].c_str());
                SANDBOX_AWAIT(rb_ary_push, SANDBOX_SLOT(1), SANDBOX_SLOT(2));
            }
        }
        SANDBOX_AWAIT(rb_iv_set, font_class, "default_name", SANDBOX_SLOT(1));

        SANDBOX_AWAIT(rb_define_method, font_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
        SANDBOX_AWAIT(rb_define_method, font_class, "initialize_copy", (VALUE (*)(ANYARGS))initialize_copy, 1);

        SANDBOX_INIT_PROP_BIND(font_class, name);
        SANDBOX_INIT_PROP_BIND(font_class, size);
        SANDBOX_INIT_PROP_BIND(font_class, bold);
        SANDBOX_INIT_PROP_BIND(font_class, italic);
        SANDBOX_INIT_PROP_BIND(font_class, color);

        if (rgssVer >= 2) {
            SANDBOX_INIT_PROP_BIND(font_class, shadow);
        }

        if (rgssVer >= 3) {
            SANDBOX_INIT_PROP_BIND(font_class, outline);
            SANDBOX_INIT_PROP_BIND(font_class, out_color);
        }

        SANDBOX_INIT_SINGLETON_PROP_BIND(font_class, default_name);
        SANDBOX_INIT_SINGLETON_PROP_BIND(font_class, default_size);
        SANDBOX_INIT_SINGLETON_PROP_BIND(font_class, default_bold);
        SANDBOX_INIT_SINGLETON_PROP_BIND(font_class, default_italic);
        SANDBOX_INIT_SINGLETON_PROP_BIND(font_class, default_color);

        if (rgssVer >= 2) {
            SANDBOX_INIT_SINGLETON_PROP_BIND(font_class, default_shadow);
        }

        if (rgssVer >= 3) {
            SANDBOX_INIT_SINGLETON_PROP_BIND(font_class, default_outline);
            SANDBOX_INIT_SINGLETON_PROP_BIND(font_class, default_out_color);
        }

        SANDBOX_AWAIT(rb_define_singleton_method, font_class, "exist?", (VALUE (*)(ANYARGS))exist, 1);
    }
}
