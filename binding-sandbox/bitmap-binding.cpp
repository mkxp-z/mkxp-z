/*
** bitmap-binding.cpp
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

#include "bitmap-binding.h"
#include "disposable-binding.h"
#include "etc-binding.h"
#include "font-binding.h"
#include "bitmap.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::bitmap_class;
static struct bindings::rb_data_type bitmap_type;

SANDBOX_DEF_ALLOC(bitmap_type);

void bitmap_init_props::operator()(VALUE self) {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_S(0, rb_class_new_instance, 0, nullptr, font_class);
        SANDBOX_AWAIT(rb_iv_set, self, "font", SANDBOX_SLOT(0));

        // Leave property as default nil if hasHires() is false.
        GFX_LOCK;
        bool hasHires;
        SANDBOX_GUARD_F(GFX_UNLOCK, hasHires = get_private_data<Bitmap>(self)->getHasHires(sb().e));
        if (hasHires) {
            get_private_data<Bitmap>(self)->assumeRubyGC();
            SANDBOX_GUARD_F(GFX_UNLOCK, SANDBOX_AWAIT_S(1, wrap_property, self, get_private_data<Bitmap>(self)->getHires(sb().e), "hires", bitmap_class));

            SANDBOX_AWAIT_S(2, rb_class_new_instance, 0, nullptr, font_class);
            SANDBOX_AWAIT(rb_iv_set, SANDBOX_SLOT(1), "font", SANDBOX_SLOT(2));
            Bitmap *hires;
            SANDBOX_GUARD_F(GFX_UNLOCK, hires = get_private_data<Bitmap>(self)->getHires(sb().e));
            hires->setInitFont(get_private_data<Font>(SANDBOX_SLOT(2)));
        }
        get_private_data<Bitmap>(self)->setInitFont(get_private_data<Font>(SANDBOX_SLOT(0)));
        GFX_UNLOCK;
    }
}

static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t, wasm_size_t, wasm_size_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(check_arity, argc, 1, -1);

                if (argc == 1) {
                    SANDBOX_AWAIT_S(0, rb_string_value_cstr, &sb()->ref<VALUE>(argv, 0));
                } else {
                    SANDBOX_AWAIT_S(1, rb_num2ulong, sb()->ref<VALUE>(argv, 0));
                    SANDBOX_AWAIT_S(2, rb_num2ulong, sb()->ref<VALUE>(argv, 1));
                }

                if (argc == 1) {
                    SANDBOX_GUARD_L(set_private_data(sb().e, self, new Bitmap(sb().e, sb()->str(SANDBOX_SLOT(0)))));
                } else {
                    SANDBOX_GUARD_L(set_private_data(sb().e, self, new Bitmap(sb().e, SANDBOX_SLOT(1), SANDBOX_SLOT(2))));
                }

                SANDBOX_AWAIT(bitmap_init_props, self);
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
                if (self == value) {
                    return self;
                }

                SANDBOX_AWAIT(rb_obj_init_copy, self, value);

                SANDBOX_GUARD_L(set_private_data(sb().e, self, new Bitmap(sb().e, *get_private_data<Bitmap>(value))));

                SANDBOX_AWAIT(bitmap_init_props, self);
                Font *font;
                SANDBOX_GUARD(font = &get_private_data<Bitmap>(value)->getFont(sb().e));
                get_private_data<Bitmap>(self)->setFont(*font);
            }

            return self;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE width(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD(SANDBOX_AWAIT_S(0, rb_ll2inum, get_private_data<Bitmap>(self)->getWidth(sb().e)));
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE height(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD(SANDBOX_AWAIT_S(0, rb_ll2inum, get_private_data<Bitmap>(self)->getHeight(sb().e)));
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

SANDBOX_DEF_GFX_PROP_OBJ_REF(Bitmap, Bitmap, bitmap_class, Hires, hires);

static VALUE rect(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_obj_alloc, rect_class);
                SANDBOX_GUARD(set_private_data(sb().e, SANDBOX_SLOT(0), new Rect(get_private_data<Bitmap>(self)->getRect(sb().e))));
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE blt(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, VALUE, int32_t, int32_t, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(check_arity, argc, 4, -1);

                SANDBOX_AWAIT_S(2, rb_num2int, sb()->ref<VALUE>(argv, 0));
                SANDBOX_AWAIT_S(3, rb_num2int, sb()->ref<VALUE>(argv, 1));
                SANDBOX_SLOT(0) = sb()->ref<VALUE>(argv, 2);
                SANDBOX_SLOT(1) = sb()->ref<VALUE>(argv, 3);
                if (argc > 4) {
                    SANDBOX_AWAIT_S(4, rb_num2int, sb()->ref<VALUE>(argv, 4));
                }

                SANDBOX_AWAIT(check_type, SANDBOX_SLOT(0), bitmap_class);
                if (has_private_data<Bitmap>(SANDBOX_SLOT(0))) {
                    SANDBOX_AWAIT(check_type, SANDBOX_SLOT(1), rect_class);
                    if (argc > 4) {
                        SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->blt(sb().e, SANDBOX_SLOT(2), SANDBOX_SLOT(3), *get_private_data<Bitmap>(SANDBOX_SLOT(0)), get_private_data<Rect>(SANDBOX_SLOT(1))->toIntRect(), SANDBOX_SLOT(4)));
                    } else {
                        SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->blt(sb().e, SANDBOX_SLOT(2), SANDBOX_SLOT(3), *get_private_data<Bitmap>(SANDBOX_SLOT(0)), get_private_data<Rect>(SANDBOX_SLOT(1))->toIntRect()));
                    }
                }
            }

            return self;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE stretch_blt(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, VALUE, VALUE, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(check_arity, argc, 3, -1);

                SANDBOX_SLOT(0) = sb()->ref<VALUE>(argv, 0);
                SANDBOX_SLOT(1) = sb()->ref<VALUE>(argv, 1);
                SANDBOX_SLOT(2) = sb()->ref<VALUE>(argv, 2);
                if (argc > 3) {
                    SANDBOX_AWAIT_S(3, rb_num2int, sb()->ref<VALUE>(argv, 3));
                }

                SANDBOX_AWAIT(check_type, SANDBOX_SLOT(1), bitmap_class);
                if (has_private_data<Bitmap>(SANDBOX_SLOT(1))) {
                    SANDBOX_AWAIT(check_type, SANDBOX_SLOT(2), rect_class);
                    if (argc > 4) {
                        SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->stretchBlt(sb().e, get_private_data<Rect>(SANDBOX_SLOT(0))->toIntRect(), *get_private_data<Bitmap>(SANDBOX_SLOT(1)), get_private_data<Rect>(SANDBOX_SLOT(2))->toIntRect(), SANDBOX_SLOT(3)););
                    } else {
                        SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->stretchBlt(sb().e, get_private_data<Rect>(SANDBOX_SLOT(0))->toIntRect(), *get_private_data<Bitmap>(SANDBOX_SLOT(1)), get_private_data<Rect>(SANDBOX_SLOT(2))->toIntRect()););
                    }
                }
            }

            return self;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE fill_rect(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t, int32_t, int32_t, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (argc == 2) {
                    SANDBOX_AWAIT(check_type, sb()->ref<VALUE>(argv, 0), rect_class);
                    SANDBOX_AWAIT(check_type, sb()->ref<VALUE>(argv, 1), color_class);
                    SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->fillRect(sb().e, get_private_data<Rect>(sb()->ref<VALUE>(argv, 0))->toIntRect(), get_private_data<Color>(sb()->ref<VALUE>(argv, 1))->norm));
                } else {
                    SANDBOX_AWAIT(check_arity, argc, 5, -1);

                    SANDBOX_AWAIT_S(0, rb_num2int, sb()->ref<VALUE>(argv, 0));
                    SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 1));
                    SANDBOX_AWAIT_S(2, rb_num2int, sb()->ref<VALUE>(argv, 2));
                    SANDBOX_AWAIT_S(3, rb_num2int, sb()->ref<VALUE>(argv, 3));
                    SANDBOX_AWAIT(check_type, sb()->ref<VALUE>(argv, 4), color_class);
                    SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->fillRect(sb().e, SANDBOX_SLOT(0), SANDBOX_SLOT(1), SANDBOX_SLOT(2), SANDBOX_SLOT(3), get_private_data<Color>(sb()->ref<VALUE>(argv, 4))->norm));
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE clear(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->clear(sb().e));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE get_pixel(VALUE self, VALUE x, VALUE y) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, int32_t, int32_t> slots;

        VALUE operator()(VALUE self, VALUE x, VALUE y) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(1, rb_num2int, x);
                SANDBOX_AWAIT_S(2, rb_num2int, y);

                SANDBOX_AWAIT_S(0, rb_obj_alloc, color_class);

                {
                    if (get_private_data<Bitmap>(self)->surface() != nullptr || get_private_data<Bitmap>(self)->megaSurface() != nullptr) {
                        SANDBOX_GUARD(sb().bitmap_pixel_buffer = get_private_data<Bitmap>(self)->getPixel(sb().e, SANDBOX_SLOT(1), SANDBOX_SLOT(2)));
                    } else {
                        SANDBOX_GUARD_L(sb().bitmap_pixel_buffer = get_private_data<Bitmap>(self)->getPixel(sb().e, SANDBOX_SLOT(1), SANDBOX_SLOT(2)));
                    }
                    Color *color = new Color(sb().bitmap_pixel_buffer);
                    set_private_data(SANDBOX_SLOT(0), color);
                }
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self, x, y);
}

static VALUE set_pixel(VALUE self, VALUE x, VALUE y, VALUE colorObj) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t, int32_t> slots;

        VALUE operator()(VALUE self, VALUE x, VALUE y, VALUE colorObj) {
            BOOST_ASIO_CORO_REENTER (this) {

                SANDBOX_AWAIT_S(0, rb_num2int, x);
                SANDBOX_AWAIT_S(1, rb_num2int, y);

                SANDBOX_AWAIT(check_type, colorObj, color_class);
                SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->setPixel(sb().e, SANDBOX_SLOT(0), SANDBOX_SLOT(1), *get_private_data<Color>(colorObj)));
            }

            return self;
        }
    };

    return sb()->bind<struct coro>()()(self, x, y, colorObj);
}

static VALUE hue_change(VALUE self, VALUE hueval) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t> slots;

        VALUE operator()(VALUE self, VALUE hueval) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_num2int, hueval);

                SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->hueChange(sb().e, SANDBOX_SLOT(0)));
            }

            return self;
        }
    };

    return sb()->bind<struct coro>()()(self, hueval);
}

static VALUE draw_text(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t, VALUE, int32_t, int32_t, int32_t, int32_t, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (argc == 2 || argc == 3) {
                    if (rgssVer >= 2) {
                        SANDBOX_AWAIT_S(1, rb_obj_as_string, sb()->ref<VALUE>(argv, 1));
                        SANDBOX_AWAIT_S(0, rb_string_value_cstr, &SANDBOX_SLOT(1));
                    } else {
                        SANDBOX_AWAIT_S(0, rb_string_value_cstr, &sb()->ref<VALUE>(argv, 1));
                    }
                    SANDBOX_AWAIT(check_type, sb()->ref<VALUE>(argv, 0), rect_class);
                    if (argc == 2) {
                        SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->drawText(sb().e, get_private_data<Rect>(sb()->ref<VALUE>(argv, 0))->toIntRect(), sb()->str(SANDBOX_SLOT(0))););
                    } else {
                        SANDBOX_AWAIT_S(2, rb_num2int, sb()->ref<VALUE>(argv, 2));
                        SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->drawText(sb().e, get_private_data<Rect>(sb()->ref<VALUE>(argv, 0))->toIntRect(), sb()->str(SANDBOX_SLOT(0)), SANDBOX_SLOT(2)););
                    }
                } else {
                    SANDBOX_AWAIT(check_arity, argc, 5, -1);

                    SANDBOX_AWAIT_S(3, rb_num2int, sb()->ref<VALUE>(argv, 0));
                    SANDBOX_AWAIT_S(4, rb_num2int, sb()->ref<VALUE>(argv, 1));
                    SANDBOX_AWAIT_S(5, rb_num2int, sb()->ref<VALUE>(argv, 2));
                    SANDBOX_AWAIT_S(6, rb_num2int, sb()->ref<VALUE>(argv, 3));
                    if (rgssVer >= 2) {
                        SANDBOX_AWAIT_S(1, rb_obj_as_string, sb()->ref<VALUE>(argv, 4));
                        SANDBOX_AWAIT_S(0, rb_string_value_cstr, &SANDBOX_SLOT(1));
                    } else {
                        SANDBOX_AWAIT_S(0, rb_string_value_cstr, &sb()->ref<VALUE>(argv, 4));
                    }
                    if (argc < 6) {
                        SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->drawText(sb().e, SANDBOX_SLOT(3), SANDBOX_SLOT(4), SANDBOX_SLOT(5), SANDBOX_SLOT(6), sb()->str(SANDBOX_SLOT(0))););
                    } else {
                        SANDBOX_AWAIT_S(2, rb_num2int, sb()->ref<VALUE>(argv, 5));
                        SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->drawText(sb().e, SANDBOX_SLOT(3), SANDBOX_SLOT(4), SANDBOX_SLOT(5), SANDBOX_SLOT(6), sb()->str(SANDBOX_SLOT(0)), SANDBOX_SLOT(2)););
                    }
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE text_size(VALUE self, VALUE text) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t, VALUE> slots;

        VALUE operator()(VALUE self, VALUE text) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (rgssVer >= 2) {
                    SANDBOX_AWAIT_S(1, rb_obj_as_string, text);
                    SANDBOX_AWAIT_S(0, rb_string_value_cstr, &SANDBOX_SLOT(1));
                } else {
                    SANDBOX_AWAIT_S(0, rb_string_value_cstr, &text);
                }
                SANDBOX_AWAIT_S(1, rb_obj_alloc, rect_class);
                SANDBOX_GUARD(set_private_data(sb().e, SANDBOX_SLOT(1), new Rect(get_private_data<Bitmap>(self)->textSize(sb().e, sb()->str(SANDBOX_SLOT(0))))));
            }

            return SANDBOX_SLOT(1);
        }
    };

    return sb()->bind<struct coro>()()(self, text);
}

static VALUE get_raw_data(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t, VALUE, int32_t> slots;

        VALUE operator()(VALUE self) {
            Bitmap *bitmap = get_private_data<Bitmap>(self);

            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD(SANDBOX_SLOT(2) = bitmap->getWidth(sb().e) * bitmap->getHeight(sb().e) * 4);
                SANDBOX_AWAIT_S(1, rb_str_new_cstr, "");
                SANDBOX_AWAIT(rb_str_resize, SANDBOX_SLOT(1), SANDBOX_SLOT(2));
                SANDBOX_AWAIT_S(0, rb_string_value_ptr, &SANDBOX_SLOT(1));
                if (SANDBOX_SLOT(2) > 0) {
                    sb()->check_bounds(SANDBOX_SLOT(0), SANDBOX_SLOT(2));
#ifdef MKXPZ_BIG_ENDIAN
                    SANDBOX_GUARD_L(bitmap->getRaw(sb().e, &sb()->ref<uint8_t>(SANDBOX_SLOT(0), SANDBOX_SLOT(2) - 1), SANDBOX_SLOT(2)));
                    std::reverse(&sb()->ref<uint8_t>(SANDBOX_SLOT(0), SANDBOX_SLOT(2) - 1), &sb()->ref<uint8_t>(SANDBOX_SLOT(0), SANDBOX_SLOT(2) - 1) + SANDBOX_SLOT(2));
#else
                    SANDBOX_GUARD_L(bitmap->getRaw(sb().e, &sb()->ref<uint8_t>(SANDBOX_SLOT(0)), SANDBOX_SLOT(2)));
#endif // MKXPZ_BIG_ENDIAN
                }
            }

            return SANDBOX_SLOT(1);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE set_raw_data(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t, int32_t> slots;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_string_value_ptr, &value);
                SANDBOX_AWAIT_S(1, get_bytesize, value);
                if (SANDBOX_SLOT(1) > 0) {
                    sb()->check_bounds(SANDBOX_SLOT(0), SANDBOX_SLOT(1));
#ifdef MKXPZ_BIG_ENDIAN
                    SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->replaceRaw(sb().e, &sb()->ref<uint8_t>(SANDBOX_SLOT(0), SANDBOX_SLOT(1) - 1), SANDBOX_SLOT(1)));
                    std::reverse(&sb()->ref<uint8_t>(SANDBOX_SLOT(0), SANDBOX_SLOT(1) - 1), &sb()->ref<uint8_t>(SANDBOX_SLOT(0), SANDBOX_SLOT(1) - 1) + SANDBOX_SLOT(1));
#else
                    SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->replaceRaw(sb().e, &sb()->ref<uint8_t>(SANDBOX_SLOT(0)), SANDBOX_SLOT(1)));
#endif // MKXPZ_BIG_ENDIAN
                }
            }

            return self;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE to_file(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t> slots;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_string_value_cstr, &value);
                SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->saveToFile(sb().e, sb()->str(SANDBOX_SLOT(0))));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE snap_to_bitmap(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (argv > 0) {
                    SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 0));
                } else {
                    SANDBOX_SLOT(1) = -1;
                }

                SANDBOX_AWAIT_S(0, rb_obj_alloc, bitmap_class);

                SANDBOX_GUARD_L(set_private_data(sb().e, SANDBOX_SLOT(0), new Bitmap(sb().e, *get_private_data<Bitmap>(self), SANDBOX_SLOT(1))));

                SANDBOX_AWAIT(bitmap_init_props, SANDBOX_SLOT(0));
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE gradient_fill_rect(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t, int32_t, int32_t, int32_t, uint8_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (argc == 3 || argc == 4) {
                    if (argc == 4) {
                        SANDBOX_SLOT(4) = SANDBOX_VALUE_TO_BOOL(sb()->ref<VALUE>(argv, 3));
                    } else {
                        SANDBOX_SLOT(4) = false;
                    }
                    SANDBOX_AWAIT(check_type, sb()->ref<VALUE>(argv, 0), rect_class);
                    SANDBOX_AWAIT(check_type, sb()->ref<VALUE>(argv, 1), color_class);
                    SANDBOX_AWAIT(check_type, sb()->ref<VALUE>(argv, 2), color_class);
                    SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->gradientFillRect(sb().e, get_private_data<Rect>(sb()->ref<VALUE>(argv, 0))->toIntRect(), get_private_data<Color>(sb()->ref<VALUE>(argv, 1))->norm, get_private_data<Color>(sb()->ref<VALUE>(argv, 2))->norm, SANDBOX_SLOT(4)));
                } else {
                    SANDBOX_AWAIT(check_arity, argc, 6, -1);

                    SANDBOX_AWAIT_S(0, rb_num2int, sb()->ref<VALUE>(argv, 0));
                    SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 1));
                    SANDBOX_AWAIT_S(2, rb_num2int, sb()->ref<VALUE>(argv, 2));
                    SANDBOX_AWAIT_S(3, rb_num2int, sb()->ref<VALUE>(argv, 3));
                    if (argc >= 7) {
                        SANDBOX_SLOT(4) = SANDBOX_VALUE_TO_BOOL(sb()->ref<VALUE>(argv, 6));
                    } else {
                        SANDBOX_SLOT(4) = false;
                    }
                    SANDBOX_AWAIT(check_type, sb()->ref<VALUE>(argv, 4), color_class);
                    SANDBOX_AWAIT(check_type, sb()->ref<VALUE>(argv, 5), color_class);
                    SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->gradientFillRect(sb().e, SANDBOX_SLOT(0), SANDBOX_SLOT(1), SANDBOX_SLOT(2), SANDBOX_SLOT(3), get_private_data<Color>(sb()->ref<VALUE>(argv, 4))->norm, get_private_data<Color>(sb()->ref<VALUE>(argv, 5))->norm, SANDBOX_SLOT(4)));
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE clear_rect(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t, int32_t, int32_t, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (argc == 1) {
                    SANDBOX_AWAIT(check_type, sb()->ref<VALUE>(argv, 0), rect_class);
                    SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->clearRect(sb().e, get_private_data<Rect>(sb()->ref<VALUE>(argv, 0))->toIntRect()));
                } else {
                    SANDBOX_AWAIT(check_arity, argc, 4, -1);

                    SANDBOX_AWAIT_S(0, rb_num2int, sb()->ref<VALUE>(argv, 0));
                    SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 1));
                    SANDBOX_AWAIT_S(2, rb_num2int, sb()->ref<VALUE>(argv, 2));
                    SANDBOX_AWAIT_S(3, rb_num2int, sb()->ref<VALUE>(argv, 3));
                    SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->clearRect(sb().e, SANDBOX_SLOT(0), SANDBOX_SLOT(1), SANDBOX_SLOT(2), SANDBOX_SLOT(3)));
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE blur(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->blur(sb().e));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE radial_blur(VALUE self, VALUE angle, VALUE divisions) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t, int32_t> slots;

        VALUE operator()(VALUE self, VALUE angle, VALUE divisions) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_num2int, angle);
                SANDBOX_AWAIT_S(1, rb_num2int, divisions);
                SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->radialBlur(sb().e, SANDBOX_SLOT(0), SANDBOX_SLOT(1)));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, angle, divisions);
}

static VALUE mega(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self) {
            typedef decl_slots<uint8_t> slots;
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD_L(SANDBOX_SLOT(0) = get_private_data<Bitmap>(self)->getIsMega(sb().e));
            }
            return SANDBOX_BOOL_TO_VALUE(SANDBOX_SLOT(0));
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE max_size(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(Bitmap::maxSize());
}

static VALUE get_animated(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self) {
            typedef decl_slots<uint8_t> slots;
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD_L(SANDBOX_SLOT(0) = get_private_data<Bitmap>(self)->getIsAnimated(sb().e));
            }
            return SANDBOX_BOOL_TO_VALUE(SANDBOX_SLOT(0));
        }
    };

    return sb()->bind<struct coro>()()(self);
}

SANDBOX_DEF_GFX_PROP_B(Bitmap, Playing, playing);

static VALUE play(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->play(sb().e));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE stop(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->stop(sb().e));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE goto_and_play(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t> slots;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_num2int, value);
                SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->gotoAndPlay(sb().e, SANDBOX_SLOT(0)));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE goto_and_stop(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t> slots;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_num2int, value);
                SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->gotoAndStop(sb().e, SANDBOX_SLOT(0)));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE frame_count(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self) {
            typedef decl_slots<VALUE> slots;
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD(SANDBOX_AWAIT_S(0, rb_ll2inum, get_private_data<Bitmap>(self)->numFrames(sb().e)));
            }
            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE current_frame(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self) {
            typedef decl_slots<VALUE> slots;
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD(SANDBOX_AWAIT_S(0, rb_ll2inum, get_private_data<Bitmap>(self)->currentFrameI(sb().e)));
            }
            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE add_frame(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(check_arity, argc, 1, -1);
                if (argc < 2) {
                    SANDBOX_SLOT(1) = -1;
                } else {
                    SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 1));
                    if (SANDBOX_SLOT(1) < 0) {
                        SANDBOX_SLOT(1) = 0;
                    }
                }
                if (argc >= 1) {
                    SANDBOX_AWAIT(check_type, sb()->ref<VALUE>(argv, 0), bitmap_class);
                    if (!has_private_data<Bitmap>(sb()->ref<VALUE>(argv, 0))) {
                        SANDBOX_AWAIT(raise_disposed_access, sb()->ref<VALUE>(argv, 0));
                    }
                    SANDBOX_GUARD_L(SANDBOX_SLOT(1) = get_private_data<Bitmap>(self)->addFrame(sb().e, *get_private_data<Bitmap>(sb()->ref<VALUE>(argv, 0)), SANDBOX_SLOT(1)));
                }
                SANDBOX_AWAIT_S(0, rb_ll2inum, SANDBOX_SLOT(1));
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE remove_frame(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (argc < 1) {
                    SANDBOX_SLOT(0) = -1;
                } else {
                    SANDBOX_AWAIT_S(0, rb_num2int, sb()->ref<VALUE>(argv, 0));
                    if (SANDBOX_SLOT(0) < 0) {
                        SANDBOX_SLOT(0) = 0;
                    }
                }
                SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->removeFrame(sb().e, SANDBOX_SLOT(0)));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE next_frame(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self) {
            typedef decl_slots<VALUE> slots;
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->nextFrame(sb().e));
                SANDBOX_GUARD(SANDBOX_AWAIT_S(0, rb_ll2inum, get_private_data<Bitmap>(self)->currentFrameI(sb().e)));
            }
            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE previous_frame(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self) {
            typedef decl_slots<VALUE> slots;
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD_L(get_private_data<Bitmap>(self)->previousFrame(sb().e));
                SANDBOX_GUARD(SANDBOX_AWAIT_S(0, rb_ll2inum, get_private_data<Bitmap>(self)->currentFrameI(sb().e)));
            }
            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

SANDBOX_DEF_GFX_PROP_F(Bitmap, AnimationFPS, frame_rate);
SANDBOX_DEF_GFX_PROP_B(Bitmap, Looping, looping);

static VALUE get_font(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (get_private_data<Bitmap>(self)->isDisposed()) {
                    SANDBOX_AWAIT(raise_disposed_access, self);
                }

                SANDBOX_AWAIT_S(0, rb_iv_get, self, "font");
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE set_font(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, VALUE> slots;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(check_type, value, font_class);
                if (has_private_data<Font>(value)) {
                    get_private_data<Bitmap>(self)->setFont(*get_private_data<Font>(value));

                    SANDBOX_AWAIT_S(0, rb_iv_get, self, "font");
                    SANDBOX_AWAIT_S(1, rb_iv_get, value, "name");
                    SANDBOX_AWAIT(rb_iv_set, SANDBOX_SLOT(0), "name", SANDBOX_SLOT(1));
                    SANDBOX_AWAIT_S(1, rb_iv_get, value, "size");
                    SANDBOX_AWAIT(rb_iv_set, SANDBOX_SLOT(0), "size", SANDBOX_SLOT(1));
                    SANDBOX_AWAIT_S(1, rb_iv_get, value, "bold");
                    SANDBOX_AWAIT(rb_iv_set, SANDBOX_SLOT(0), "bold", SANDBOX_SLOT(1));
                    SANDBOX_AWAIT_S(1, rb_iv_get, value, "italic");
                    SANDBOX_AWAIT(rb_iv_set, SANDBOX_SLOT(0), "italic", SANDBOX_SLOT(1));

                    if (rgssVer >= 2) {
                        SANDBOX_AWAIT_S(1, rb_iv_get, value, "shadow");
                        SANDBOX_AWAIT(rb_iv_set, SANDBOX_SLOT(0), "shadow", SANDBOX_SLOT(1));
                    }

                    if (rgssVer >= 3) {
                        SANDBOX_AWAIT_S(1, rb_iv_get, value, "outline");
                        SANDBOX_AWAIT(rb_iv_set, SANDBOX_SLOT(0), "outline", SANDBOX_SLOT(1));
                    }
                }
            }

            return value;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE _kgl_invert(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD(get_private_data<Bitmap>(self)->kglInvert(sb().e));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE _kgl_compress_alpha(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_GUARD(get_private_data<Bitmap>(self)->kglCompressAlpha(sb().e));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE _kgl_subtract_rect(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t, int32_t, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(check_arity, argc, 4, 5);

                SANDBOX_AWAIT_S(0, rb_num2int, sb()->ref<VALUE>(argv, 0));
                SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 1));
                SANDBOX_AWAIT(check_type, sb()->ref<VALUE>(argv, 2), bitmap_class);
                SANDBOX_AWAIT(check_type, sb()->ref<VALUE>(argv, 3), rect_class);
                if (argc > 4) {
                    SANDBOX_AWAIT_S(2, rb_num2int, sb()->ref<VALUE>(argv, 4));
                } else {
                    SANDBOX_SLOT(2) = 255;
                }

                SANDBOX_GUARD_L(
                    get_private_data<Bitmap>(self)->stretchBlt(
                        sb().e,
                        IntRect(
                            SANDBOX_SLOT(0),
                            SANDBOX_SLOT(1),
                            std::abs(get_private_data<Rect>(sb()->ref<VALUE>(argv, 3))->toIntRect().w),
                            std::abs(get_private_data<Rect>(sb()->ref<VALUE>(argv, 3))->toIntRect().h)
                        ),
                        *get_private_data<Bitmap>(sb()->ref<VALUE>(argv, 2)),
                        get_private_data<Rect>(sb()->ref<VALUE>(argv, 3))->toIntRect(),
                        SANDBOX_SLOT(2),
                        false,
                        Bitmap::KGL_SUBTRACT
                    )
                );
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE _kgl_shadow_shader_h(VALUE self, VALUE x1_value, VALUE x2_value, VALUE y_value, VALUE soft_value) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, int32_t, int32_t, int32_t> slots;

        VALUE operator()(VALUE self, VALUE x1_value, VALUE x2_value, VALUE y_value, VALUE soft_value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(1, rb_num2int, x1_value);
                SANDBOX_AWAIT_S(2, rb_num2int, x2_value);
                SANDBOX_AWAIT_S(3, rb_num2int, y_value);

                SANDBOX_GUARD(
                    SANDBOX_SLOT(1) = get_private_data<Bitmap>(self)->kglShadowShaderH(
                        sb().e,
                        SANDBOX_SLOT(1),
                        SANDBOX_SLOT(2),
                        SANDBOX_SLOT(3),
                        SANDBOX_VALUE_TO_BOOL(soft_value)
                    )
                );

                SANDBOX_AWAIT_S(0, rb_ll2inum, SANDBOX_SLOT(1));
                return SANDBOX_SLOT(0);
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, x1_value, x2_value, y_value, soft_value);
}

static VALUE _kgl_shadow_shader_v(VALUE self, VALUE y1_value, VALUE y2_value, VALUE x_value, VALUE wall_value, VALUE soft_value) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, int32_t, int32_t, int32_t> slots;

        VALUE operator()(VALUE self, VALUE y1_value, VALUE y2_value, VALUE x_value, VALUE wall_value, VALUE soft_value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(1, rb_num2int, y1_value);
                SANDBOX_AWAIT_S(2, rb_num2int, y2_value);
                SANDBOX_AWAIT_S(3, rb_num2int, x_value);

                SANDBOX_GUARD(
                    SANDBOX_SLOT(1) = get_private_data<Bitmap>(self)->kglShadowShaderV(
                        sb().e,
                        SANDBOX_SLOT(1),
                        SANDBOX_SLOT(2),
                        SANDBOX_SLOT(3),
                        SANDBOX_VALUE_TO_BOOL(wall_value),
                        SANDBOX_VALUE_TO_BOOL(soft_value)
                    )
                );

                SANDBOX_AWAIT_S(0, rb_ll2inum, SANDBOX_SLOT(1));
                return SANDBOX_SLOT(0);
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, y1_value, y2_value, x_value, wall_value, soft_value);
}

void bitmap_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        bitmap_type = sb()->rb_data_type("Bitmap", nullptr, dfree, nullptr, nullptr, 0, 0, 0);
        SANDBOX_AWAIT_R(bitmap_class, rb_define_class, "Bitmap", sb()->rb_cObject());
        SANDBOX_AWAIT(rb_define_alloc_func, bitmap_class, alloc);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "initialize_copy", (VALUE (*)(ANYARGS))initialize_copy, 1);
        SANDBOX_AWAIT(disposable_binding_init<Bitmap>, bitmap_class);

        SANDBOX_AWAIT(rb_define_method, bitmap_class, "width", (VALUE (*)(ANYARGS))width, 0);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "height", (VALUE (*)(ANYARGS))height, 0);

        SANDBOX_INIT_PROP_BIND(bitmap_class, hires);

        SANDBOX_AWAIT(rb_define_method, bitmap_class, "rect", (VALUE (*)(ANYARGS))rect, 0);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "blt", (VALUE (*)(ANYARGS))blt, -1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "stretch_blt", (VALUE (*)(ANYARGS))stretch_blt, -1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "fill_rect", (VALUE (*)(ANYARGS))fill_rect, -1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "clear", (VALUE (*)(ANYARGS))clear, 0);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "get_pixel", (VALUE (*)(ANYARGS))get_pixel, 2);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "set_pixel", (VALUE (*)(ANYARGS))set_pixel, 3);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "hue_change", (VALUE (*)(ANYARGS))hue_change, 1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "draw_text", (VALUE (*)(ANYARGS))draw_text, -1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "text_size", (VALUE (*)(ANYARGS))text_size, 1);

        SANDBOX_INIT_PROP_BIND(bitmap_class, raw_data);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "to_file", (VALUE (*)(ANYARGS))to_file, 1);

        SANDBOX_AWAIT(rb_define_method, bitmap_class, "gradient_fill_rect", (VALUE (*)(ANYARGS))gradient_fill_rect, -1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "clear_rect", (VALUE (*)(ANYARGS))clear_rect, -1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "blur", (VALUE (*)(ANYARGS))blur, 0);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "radial_blur", (VALUE (*)(ANYARGS))radial_blur, 2);

        SANDBOX_AWAIT(rb_define_method, bitmap_class, "mega?", (VALUE (*)(ANYARGS))mega, 0);
        SANDBOX_AWAIT(rb_define_singleton_method, bitmap_class, "max_size", (VALUE (*)(ANYARGS))max_size, 0);

        SANDBOX_AWAIT(rb_define_method, bitmap_class, "animated?", (VALUE (*)(ANYARGS))get_animated, 0);
        SANDBOX_INIT_PROP_BIND(bitmap_class, playing);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "play", (VALUE (*)(ANYARGS))play, 0);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "stop", (VALUE (*)(ANYARGS))stop, 0);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "goto_and_play", (VALUE (*)(ANYARGS))goto_and_play, 1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "goto_and_stop", (VALUE (*)(ANYARGS))goto_and_stop, 1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "frame_count", (VALUE (*)(ANYARGS))frame_count, 0);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "current_frame", (VALUE (*)(ANYARGS))current_frame, 0);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "add_frame", (VALUE (*)(ANYARGS))add_frame, -1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "remove_frame", (VALUE (*)(ANYARGS))remove_frame, -1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "next_frame", (VALUE (*)(ANYARGS))next_frame, 0);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "previous_frame", (VALUE (*)(ANYARGS))previous_frame, 0);
        SANDBOX_INIT_PROP_BIND(bitmap_class, frame_rate);
        SANDBOX_INIT_PROP_BIND(bitmap_class, looping);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "snap_to_bitmap", (VALUE (*)(ANYARGS))snap_to_bitmap, -1);

        SANDBOX_INIT_PROP_BIND(bitmap_class, font);

        SANDBOX_AWAIT(rb_define_method, bitmap_class, "_kgl_invert", (VALUE (*)(ANYARGS))_kgl_invert, 0);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "_kgl_compress_alpha", (VALUE (*)(ANYARGS))_kgl_compress_alpha, 0);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "_kgl_subtract_rect", (VALUE (*)(ANYARGS))_kgl_subtract_rect, -1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "_kgl_shadow_shader_h", (VALUE (*)(ANYARGS))_kgl_shadow_shader_h, 4);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "_kgl_shadow_shader_v", (VALUE (*)(ANYARGS))_kgl_shadow_shader_v, 5);
    }
}
