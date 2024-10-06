/*
 ** bitmap-binding.cpp
 **
 ** This file is part of mkxp.
 **
 ** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
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

#include "binding-types.h"
#include "binding-util.h"
#include "bitmap.h"
#include "disposable-binding.h"
#include "exception.h"
#include "font.h"
#include "sharedstate.h"
#include "graphics.h"

#if RAPI_FULL > 187
DEF_TYPE(Bitmap);
#else
DEF_ALLOCFUNC(Bitmap);
#endif

static const char *objAsStringPtr(VALUE obj) {
    VALUE str = rb_obj_as_string(obj);
    return RSTRING_PTR(str);
}

void bitmapInitProps(Bitmap *b, VALUE self) {
    /* Wrap properties */
    VALUE fontKlass = rb_const_get(rb_cObject, rb_intern("Font"));
    VALUE fontObj = rb_obj_alloc(fontKlass);
    rb_obj_call_init(fontObj, 0, 0);
    
    Font *font = getPrivateData<Font>(fontObj);
    
    rb_iv_set(self, "font", fontObj);

    // Leave property as default nil if hasHires() is false.
    if (b->hasHires()) {
        b->assumeRubyGC();
        wrapProperty(self, b->getHires(), "hires", BitmapType);
        
        VALUE hiresFontObj = rb_obj_alloc(fontKlass);
        rb_obj_call_init(hiresFontObj, 0, 0);
        Font *hiresFont = getPrivateData<Font>(hiresFontObj);
        rb_iv_set(rb_iv_get(self, "hires"), "font", hiresFontObj);
        b->getHires()->setInitFont(hiresFont);
        
    }
    b->setInitFont(font);
}

RB_METHOD_GUARD(bitmapInitialize) {
    Bitmap *b = 0;

    VALUE arg1;
    VALUE arg2;

    VALUE kwargs;
    rb_scan_args(argc, argv, "11:", &arg1, &arg2, &kwargs);

    ID table[1];
    table[0] = rb_intern("device_affinity");
    VALUE deviceAffinity;
    rb_get_kwargs(kwargs, table, 0, 1, &deviceAffinity);

    if (RB_TYPE_P(arg1, RUBY_T_STRING)) {
        // filename constructor, arg1 is filename
        SafeStringValue(arg1);

        int affinity = 0;
        if (deviceAffinity != Qundef && deviceAffinity != Qnil) {
            affinity = NUM2INT(deviceAffinity);
        }

        bool forceMega = false;
        if (affinity == 1) {
            forceMega = true;
        }

        GFX_GUARD_EXC(b = new Bitmap(RSTRING_PTR(arg1), forceMega);)
    }
    else {
        // width,height constructor
        int width = NUM2INT(arg1);
        int height = NUM2INT(arg2);
        GFX_GUARD_EXC(b = new Bitmap(width, height);)
    }
    
    setPrivateData(self, b);
    bitmapInitProps(b, self);
    
    return self;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapWidth) {
    RB_UNUSED_PARAM;
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    int value = 0;
   value = b->width();
    
    return INT2FIX(value);
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapHeight) {
    RB_UNUSED_PARAM;
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    int value = 0;
   value = b->height();
    
    return INT2FIX(value);
}
RB_METHOD_GUARD_END

DEF_GFX_PROP_OBJ_REF(Bitmap, Bitmap, Hires, "hires")

RB_METHOD_GUARD(bitmapRect) {
    RB_UNUSED_PARAM;
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    IntRect rect;
   rect = b->rect();
    
    Rect *r = new Rect(rect);
    
    return wrapObject(r, RectType);
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapBlt) {
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    int x, y;
    VALUE srcObj;
    VALUE srcRectObj;
    int opacity = 255;
    
    Bitmap *src;
    Rect *srcRect;
    
    rb_get_args(argc, argv, "iioo|i", &x, &y, &srcObj, &srcRectObj,
                &opacity RB_ARG_END);
    
    src = getPrivateDataCheck<Bitmap>(srcObj, BitmapType);
    if (src) {
        srcRect = getPrivateDataCheck<Rect>(srcRectObj, RectType);
    
        GFX_GUARD_EXC(b->blt(x, y, *src, srcRect->toIntRect(), opacity););
    }
    
    return self;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapStretchBlt) {
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    VALUE destRectObj;
    VALUE srcObj;
    VALUE srcRectObj;
    int opacity = 255;
    
    Bitmap *src;
    Rect *destRect, *srcRect;
    
    rb_get_args(argc, argv, "ooo|i", &destRectObj, &srcObj, &srcRectObj,
                &opacity RB_ARG_END);
    
    src = getPrivateDataCheck<Bitmap>(srcObj, BitmapType);
    if (src) {
        destRect = getPrivateDataCheck<Rect>(destRectObj, RectType);
        srcRect = getPrivateDataCheck<Rect>(srcRectObj, RectType);
        
        GFX_GUARD_EXC(b->stretchBlt(destRect->toIntRect(), *src, srcRect->toIntRect(),
                                    opacity););
    }
    
    return self;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapFillRect) {
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    VALUE colorObj;
    Color *color;
    
    if (argc == 2) {
        VALUE rectObj;
        Rect *rect;
        
        rb_get_args(argc, argv, "oo", &rectObj, &colorObj RB_ARG_END);
        
        rect = getPrivateDataCheck<Rect>(rectObj, RectType);
        color = getPrivateDataCheck<Color>(colorObj, ColorType);
        
        GFX_GUARD_EXC(b->fillRect(rect->toIntRect(), color->norm););
    } else {
        int x, y, width, height;
        
        rb_get_args(argc, argv, "iiiio", &x, &y, &width, &height,
                    &colorObj RB_ARG_END);
        
        color = getPrivateDataCheck<Color>(colorObj, ColorType);
        
        GFX_GUARD_EXC(b->fillRect(x, y, width, height, color->norm););
    }
    
    return self;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapClear) {
    RB_UNUSED_PARAM;
    
    Bitmap *b = getPrivateData<Bitmap>(self);

    GFX_GUARD_EXC(b->clear();)
    
    return self;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapGetPixel) {
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    int x, y;
    
    rb_get_args(argc, argv, "ii", &x, &y RB_ARG_END);
    
    Color value;
    if (b->surface() || b->megaSurface())
        value = b->getPixel(x, y);
    else
        GFX_GUARD_EXC(value = b->getPixel(x, y););
    
    Color *color = new Color(value);
    
    return wrapObject(color, ColorType);
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapSetPixel) {
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    int x, y;
    VALUE colorObj;
    
    Color *color;
    
    rb_get_args(argc, argv, "iio", &x, &y, &colorObj RB_ARG_END);
    
    color = getPrivateDataCheck<Color>(colorObj, ColorType);
    
    GFX_GUARD_EXC(b->setPixel(x, y, *color););
    
    return self;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapHueChange) {
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    int hue;
    
    rb_get_args(argc, argv, "i", &hue RB_ARG_END);
    
    GFX_GUARD_EXC(b->hueChange(hue););
    
    return self;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapDrawText) {
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    const char *str;
    int align = Bitmap::Left;
    
    if (argc == 2 || argc == 3) {
        VALUE rectObj;
        Rect *rect;
        
        if (rgssVer >= 2) {
            VALUE strObj;
            rb_get_args(argc, argv, "oo|i", &rectObj, &strObj, &align RB_ARG_END);
            
            str = objAsStringPtr(strObj);
        } else {
            rb_get_args(argc, argv, "oz|i", &rectObj, &str, &align RB_ARG_END);
        }
        
        rect = getPrivateDataCheck<Rect>(rectObj, RectType);
        
        GFX_GUARD_EXC(b->drawText(rect->toIntRect(), str, align););
    } else {
        int x, y, width, height;
        
        if (rgssVer >= 2) {
            VALUE strObj;
            rb_get_args(argc, argv, "iiiio|i", &x, &y, &width, &height, &strObj,
                        &align RB_ARG_END);
            
            str = objAsStringPtr(strObj);
        } else {
            rb_get_args(argc, argv, "iiiiz|i", &x, &y, &width, &height, &str,
                        &align RB_ARG_END);
        }
        
        GFX_GUARD_EXC(b->drawText(x, y, width, height, str, align););
    }
    
    return self;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapTextSize) {
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    const char *str;
    
    if (rgssVer >= 2) {
        VALUE strObj;
        rb_get_args(argc, argv, "o", &strObj RB_ARG_END);
        
        str = objAsStringPtr(strObj);
    } else {
        rb_get_args(argc, argv, "z", &str RB_ARG_END);
    }
    
    IntRect value;
    value = b->textSize(str);
    
    Rect *rect = new Rect(value);
    
    return wrapObject(rect, RectType);
}
RB_METHOD_GUARD_END

RB_METHOD(BitmapGetFont) {
    RB_UNUSED_PARAM;
    checkDisposed<Bitmap>(self);
    return rb_iv_get(self, "font");
}
RB_METHOD_GUARD(BitmapSetFont) {
    rb_check_argc(argc, 1);
    Bitmap *b = getPrivateData<Bitmap>(self);
    VALUE propObj = *argv;
    
    Font *prop = getPrivateDataCheck<Font>(propObj, FontType);
    if (prop) {
        GFX_GUARD_EXC(b->setFont(*prop);)
        
        VALUE f = rb_iv_get(self, "font");
        if (f) {
            rb_iv_set(f, "name", rb_iv_get(propObj, "name"));
            rb_iv_set(f, "size", rb_iv_get(propObj, "size"));
            rb_iv_set(f, "bold", rb_iv_get(propObj, "bold"));
            rb_iv_set(f, "italic", rb_iv_get(propObj, "italic"));

            if (rgssVer >= 2) {
                rb_iv_set(f, "shadow", rb_iv_get(propObj, "shadow"));
            }

            if (rgssVer >= 3) {
                rb_iv_set(f, "outline", rb_iv_get(propObj, "outline"));
            }
        }
    }
    
    return propObj;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapGradientFillRect) {
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    VALUE color1Obj, color2Obj;
    Color *color1, *color2;
    bool vertical = false;
    
    if (argc == 3 || argc == 4) {
        VALUE rectObj;
        Rect *rect;
        
        rb_get_args(argc, argv, "ooo|b", &rectObj, &color1Obj, &color2Obj,
                    &vertical RB_ARG_END);
        
        rect = getPrivateDataCheck<Rect>(rectObj, RectType);
        color1 = getPrivateDataCheck<Color>(color1Obj, ColorType);
        color2 = getPrivateDataCheck<Color>(color2Obj, ColorType);
        
        GFX_GUARD_EXC(b->gradientFillRect(rect->toIntRect(), color1->norm, color2->norm,
                                      vertical););
    } else {
        int x, y, width, height;
        
        rb_get_args(argc, argv, "iiiioo|b", &x, &y, &width, &height, &color1Obj,
                    &color2Obj, &vertical RB_ARG_END);
        
        color1 = getPrivateDataCheck<Color>(color1Obj, ColorType);
        color2 = getPrivateDataCheck<Color>(color2Obj, ColorType);
        
        GFX_GUARD_EXC(b->gradientFillRect(x, y, width, height, color1->norm,
                                      color2->norm, vertical););
    }
    
    return self;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapClearRect) {
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    if (argc == 1) {
        VALUE rectObj;
        Rect *rect;
        
        rb_get_args(argc, argv, "o", &rectObj RB_ARG_END);
        
        rect = getPrivateDataCheck<Rect>(rectObj, RectType);
        
        GFX_GUARD_EXC(b->clearRect(rect->toIntRect()););
    } else {
        int x, y, width, height;
        
        rb_get_args(argc, argv, "iiii", &x, &y, &width, &height RB_ARG_END);
        
        GFX_GUARD_EXC(b->clearRect(x, y, width, height););
    }
    
    return self;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapBlur) {
    RB_UNUSED_PARAM;
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    GFX_GUARD_EXC( b->blur(); );
    
    return Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapRadialBlur) {
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    int angle, divisions;
    rb_get_args(argc, argv, "ii", &angle, &divisions RB_ARG_END);
    
    GFX_GUARD_EXC( b->radialBlur(angle, divisions); );
    
    return Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapGetRawData) {
    RB_UNUSED_PARAM;
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    int size = b->width() * b->height() * 4;
    VALUE ret = rb_str_new(0, size);
    
    GFX_GUARD_EXC(b->getRaw(RSTRING_PTR(ret), size););
    
    return ret;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapSetRawData) {
    RB_UNUSED_PARAM;
    
    VALUE str;
    rb_scan_args(argc, argv, "1", &str);
    SafeStringValue(str);
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    GFX_GUARD_EXC(b->replaceRaw(RSTRING_PTR(str), RSTRING_LEN(str)););
    
    return self;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapSaveToFile) {
    RB_UNUSED_PARAM;
    
    VALUE str;
    rb_scan_args(argc, argv, "1", &str);
    SafeStringValue(str);
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    GFX_GUARD_EXC(b->saveToFile(RSTRING_PTR(str)););
    
    return RUBY_Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapGetMega){
    RB_UNUSED_PARAM;
    
    rb_check_argc(argc, 0);
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    VALUE ret;
    
    GFX_GUARD_EXC(ret = rb_bool_new(b->isMega()););
    
    return ret;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapGetAnimated){
    RB_UNUSED_PARAM;
    
    rb_check_argc(argc, 0);
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    VALUE ret;
    
    GFX_GUARD_EXC(ret = rb_bool_new(b->isAnimated()););
    
    return ret;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapGetPlaying){
    RB_UNUSED_PARAM;
    
    rb_check_argc(argc, 0);
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    return rb_bool_new(b->isPlaying());
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapSetPlaying){
    RB_UNUSED_PARAM;
    
    bool play;
    
    rb_get_args(argc, argv, "b", &play RB_ARG_END);
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    GFX_GUARD_EXC((play) ? b->play() : b->stop(););
    
    return RUBY_Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapPlay){
    RB_UNUSED_PARAM;
    
    rb_check_argc(argc, 0);
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    GFX_GUARD_EXC(b->play(););
    
    return RUBY_Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapStop){
    RB_UNUSED_PARAM;
    
    rb_check_argc(argc, 0);
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    GFX_GUARD_EXC(b->stop(););
    
    return RUBY_Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapGotoStop){
    RB_UNUSED_PARAM;
    
    int frame;
    
    rb_get_args(argc, argv, "i", &frame RB_ARG_END);
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    GFX_GUARD_EXC(b->gotoAndStop(frame););
    
    return RUBY_Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapGotoPlay){
    RB_UNUSED_PARAM;
    
    int frame;
    
    rb_get_args(argc, argv, "i", &frame RB_ARG_END);
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    GFX_GUARD_EXC(b->gotoAndPlay(frame););
    
    return RUBY_Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapFrames){
    RB_UNUSED_PARAM;
    
    rb_check_argc(argc, 0);
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    return INT2NUM(b->numFrames());
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapCurrentFrame){
    RB_UNUSED_PARAM;
    
    rb_check_argc(argc, 0);
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    return INT2NUM(b->currentFrameI());
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapAddFrame){
    RB_UNUSED_PARAM;
    
    VALUE srcBitmap;
    VALUE position;
    
    rb_scan_args(argc, argv, "11", &srcBitmap, &position);
    
    Bitmap *src = getPrivateDataCheck<Bitmap>(srcBitmap, BitmapType);
    if (!src)
        raiseDisposedAccess(srcBitmap);
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    int pos = -1;
    if (position != Qnil) {
        pos = NUM2INT(position);
        if (pos < 0) pos = 0;
    }
    
    int ret;
    
    GFX_GUARD_EXC(ret = b->addFrame(*src, pos););
    
    return INT2NUM(ret);
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapRemoveFrame){
    RB_UNUSED_PARAM;
    
    VALUE position;
    
    rb_scan_args(argc, argv, "01", &position);
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    int pos = -1;
    if (position != Qnil) {
        pos = NUM2INT(position);
        if (pos < 0) pos = 0;
    }
    
    GFX_GUARD_EXC(b->removeFrame(pos););
    
    return RUBY_Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapNextFrame){
    RB_UNUSED_PARAM;
    
    rb_check_argc(argc, 0);
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    GFX_GUARD_EXC(b->nextFrame(););
    
    return INT2NUM(b->currentFrameI());
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapPreviousFrame){
    RB_UNUSED_PARAM;
    
    rb_check_argc(argc, 0);
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    GFX_GUARD_EXC(b->previousFrame(););
    
    return INT2NUM(b->currentFrameI());
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapSetFPS){
    RB_UNUSED_PARAM;
    
    VALUE fps;
    rb_scan_args(argc, argv, "1", &fps);
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    GFX_GUARD_EXC(
              if (RB_TYPE_P(fps, RUBY_T_FLOAT)) {
        b->setAnimationFPS(RFLOAT_VALUE(fps));
    }
              else {
        b->setAnimationFPS(NUM2INT(fps));
    }
              );
    
    return RUBY_Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapGetFPS){
    RB_UNUSED_PARAM;
    
    rb_check_argc(argc, 0);
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    float ret;
    
    ret = b->getAnimationFPS();
    
    return rb_float_new(ret);
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapSetLooping){
    RB_UNUSED_PARAM;
    
    bool loop;
    rb_get_args(argc, argv, "b", &loop RB_ARG_END);
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    GFX_GUARD_EXC(b->setLooping(loop););
    
    return rb_bool_new(loop);
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(bitmapGetLooping){
    RB_UNUSED_PARAM;
    
    rb_check_argc(argc, 0);
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    bool ret;
    ret = b->getLooping();
    return rb_bool_new(ret);
}
RB_METHOD_GUARD_END

// Captures the Bitmap's current frame data to a new Bitmap
RB_METHOD_GUARD(bitmapSnapToBitmap) {
    RB_UNUSED_PARAM;
    
    VALUE position;
    rb_scan_args(argc, argv, "01", &position);
    
    Bitmap *b = getPrivateData<Bitmap>(self);
    
    Bitmap *newbitmap = 0;
    int pos = (position == RUBY_Qnil) ? -1 : NUM2INT(position);
    
    GFX_GUARD_EXC(newbitmap = new Bitmap(*b, pos););
    
    VALUE ret = rb_obj_alloc(rb_class_of(self));
    
    bitmapInitProps(newbitmap, ret);
    setPrivateData(ret, newbitmap);
    
    return ret;
}
RB_METHOD_GUARD_END

RB_METHOD(bitmapGetMaxSize){
    RB_UNUSED_PARAM;
    
    rb_check_argc(argc, 0);
    
    return INT2NUM(Bitmap::maxSize());
}

RB_METHOD_GUARD(bitmapInitializeCopy) {
    rb_check_argc(argc, 1);
    VALUE origObj = argv[0];
    
    if (!OBJ_INIT_COPY(self, origObj))
        return self;
    
    Bitmap *orig = getPrivateData<Bitmap>(origObj);
    Bitmap *b = 0;
    
    GFX_GUARD_EXC(b = new Bitmap(*orig););
    
    bitmapInitProps(b, self);
    b->setFont(orig->getFont());
    setPrivateData(self, b);
    
    return self;
}
RB_METHOD_GUARD_END

void bitmapBindingInit() {
    VALUE klass = rb_define_class("Bitmap", rb_cObject);
#if RAPI_FULL > 187
    rb_define_alloc_func(klass, classAllocate<&BitmapType>);
#else
    rb_define_alloc_func(klass, BitmapAllocate);
#endif
    
    disposableBindingInit<Bitmap>(klass);
    
    _rb_define_method(klass, "initialize", bitmapInitialize);
    _rb_define_method(klass, "initialize_copy", bitmapInitializeCopy);
    
    _rb_define_method(klass, "width", bitmapWidth);
    _rb_define_method(klass, "height", bitmapHeight);

    INIT_PROP_BIND(Bitmap, Hires, "hires");

    _rb_define_method(klass, "rect", bitmapRect);
    _rb_define_method(klass, "blt", bitmapBlt);
    _rb_define_method(klass, "stretch_blt", bitmapStretchBlt);
    _rb_define_method(klass, "fill_rect", bitmapFillRect);
    _rb_define_method(klass, "clear", bitmapClear);
    _rb_define_method(klass, "get_pixel", bitmapGetPixel);
    _rb_define_method(klass, "set_pixel", bitmapSetPixel);
    _rb_define_method(klass, "hue_change", bitmapHueChange);
    _rb_define_method(klass, "draw_text", bitmapDrawText);
    _rb_define_method(klass, "text_size", bitmapTextSize);
    
    _rb_define_method(klass, "raw_data", bitmapGetRawData);
    _rb_define_method(klass, "raw_data=", bitmapSetRawData);
    _rb_define_method(klass, "to_file", bitmapSaveToFile);
    
    _rb_define_method(klass, "gradient_fill_rect", bitmapGradientFillRect);
    _rb_define_method(klass, "clear_rect", bitmapClearRect);
    _rb_define_method(klass, "blur", bitmapBlur);
    _rb_define_method(klass, "radial_blur", bitmapRadialBlur);
    
    _rb_define_method(klass, "mega?", bitmapGetMega);
    rb_define_singleton_method(klass, "max_size", RUBY_METHOD_FUNC(bitmapGetMaxSize), -1);
    
    _rb_define_method(klass, "animated?", bitmapGetAnimated);
    _rb_define_method(klass, "playing", bitmapGetPlaying);
    _rb_define_method(klass, "playing=", bitmapSetPlaying);
    _rb_define_method(klass, "play", bitmapPlay);
    _rb_define_method(klass, "stop", bitmapStop);
    _rb_define_method(klass, "goto_and_stop", bitmapGotoStop);
    _rb_define_method(klass, "goto_and_play", bitmapGotoPlay);
    _rb_define_method(klass, "frame_count", bitmapFrames);
    _rb_define_method(klass, "current_frame", bitmapCurrentFrame);
    _rb_define_method(klass, "add_frame", bitmapAddFrame);
    _rb_define_method(klass, "remove_frame", bitmapRemoveFrame);
    _rb_define_method(klass, "next_frame", bitmapNextFrame);
    _rb_define_method(klass, "previous_frame", bitmapPreviousFrame);
    _rb_define_method(klass, "frame_rate", bitmapGetFPS);
    _rb_define_method(klass, "frame_rate=", bitmapSetFPS);
    _rb_define_method(klass, "looping", bitmapGetLooping);
    _rb_define_method(klass, "looping=", bitmapSetLooping);
    _rb_define_method(klass, "snap_to_bitmap", bitmapSnapToBitmap);
    
    INIT_PROP_BIND(Bitmap, Font, "font");
}
