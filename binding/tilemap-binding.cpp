/*
 ** tilemap-binding.cpp
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

#include "bitmap.h"
#include "table.h"
#include "tilemap.h"
#include "viewport.h"

#include "binding-types.h"
#include "binding-util.h"
#include "disposable-binding.h"

#if RAPI_FULL > 187
DEF_TYPE_CUSTOMFREE(TilemapAutotiles, freeInstance<Tilemap::Autotiles>);
#else
DEF_ALLOCFUNC_CUSTOMFREE(TilemapAutotiles, freeInstance<Tilemap::Autotiles>);
#endif

RB_METHOD(tilemapAutotilesSet) {
    Tilemap::Autotiles *a = getPrivateDataNoRaise<Tilemap::Autotiles>(self);
    
    if (!a)
        return self;
    
    int i;
    VALUE bitmapObj;
    
    rb_get_args(argc, argv, "io", &i, &bitmapObj RB_ARG_END);
    
    Bitmap *bitmap = getPrivateDataCheck<Bitmap>(bitmapObj, BitmapType);
    
    GFX_LOCK;
    a->set(i, bitmap);
    
    VALUE ary = rb_iv_get(self, "array");
    rb_ary_store(ary, i, bitmapObj);
    GFX_UNLOCK;
    return self;
}

RB_METHOD(tilemapAutotilesGet) {
    int i;
    rb_get_args(argc, argv, "i", &i RB_ARG_END);
    
    if (i < 0 || i > 6)
        return Qnil;
    
    VALUE ary = rb_iv_get(self, "array");
    
    return rb_ary_entry(ary, i);
}

#if RAPI_FULL > 187
DEF_TYPE(Tilemap);
#else
DEF_ALLOCFUNC(Tilemap);
#endif

RB_METHOD(tilemapInitialize) {
    Tilemap *t;
    
    /* Get parameters */
    VALUE viewportObj = Qnil;
    Viewport *viewport = 0;
    
    rb_get_args(argc, argv, "|o", &viewportObj RB_ARG_END);
    
    if (!NIL_P(viewportObj))
        viewport = getPrivateDataCheck<Viewport>(viewportObj, ViewportType);
    
    GFX_LOCK;
    /* Construct object */
    t = new Tilemap(viewport);
    
    rb_iv_set(self, "viewport", viewportObj);
    
    setPrivateData(self, t);
    
    t->initDynAttribs();
    
    /* Dispose the old autotiles if we're reinitializing.
     * See the comment in setPrivateData for more info. */
    VALUE autotilesObj = rb_iv_get(self, "autotiles");
    if (autotilesObj != Qnil)
        setPrivateData(autotilesObj, 0);
    
    BINDING_GUARD_F(GFX_UNLOCK, wrapProperty(self, t->getAutotiles(e), "autotiles", TilemapAutotilesType));
    
    BINDING_GUARD_F(GFX_UNLOCK, wrapProperty(self, &t->getColor(e), "color", ColorType));
    BINDING_GUARD_F(GFX_UNLOCK, wrapProperty(self, &t->getTone(e), "tone", ToneType));
    
    autotilesObj = rb_iv_get(self, "autotiles");
    
    VALUE ary = rb_ary_new2(7);
    for (int i = 0; i < 7; ++i)
        rb_ary_push(ary, Qnil);
    
    rb_iv_set(autotilesObj, "array", ary);
    
    /* Circular reference so both objects are always
     * alive at the same time */
    rb_iv_set(autotilesObj, "tilemap", self);
    
    GFX_UNLOCK;
    return self;
}

RB_METHOD(tilemapGetAutotiles) {
    RB_UNUSED_PARAM;
    
    return rb_iv_get(self, "autotiles");
}

RB_METHOD(tilemapUpdate) {
    RB_UNUSED_PARAM;
    
    Tilemap *t = getPrivateData<Tilemap>(self);
    
    BINDING_GUARD_L(t->update(e));
    
    return Qnil;
}

RB_METHOD(tilemapGetViewport) {
    RB_UNUSED_PARAM;
    
    checkDisposed<Tilemap>(self);
    
    return rb_iv_get(self, "viewport");
}

DEF_GFX_PROP_OBJ_REF(Tilemap, Bitmap, Tileset, "tileset")
DEF_GFX_PROP_OBJ_REF(Tilemap, Table, MapData, "map_data")
DEF_GFX_PROP_OBJ_REF(Tilemap, Table, FlashData, "flash_data")
DEF_GFX_PROP_OBJ_REF(Tilemap, Table, Priorities, "priorities")

DEF_GFX_PROP_OBJ_VAL(Tilemap, Color, Color, "color")
DEF_GFX_PROP_OBJ_VAL(Tilemap, Tone, Tone, "tone")

DEF_GFX_PROP_B(Tilemap, Visible)

DEF_GFX_PROP_I(Tilemap, OX)
DEF_GFX_PROP_I(Tilemap, OY)

DEF_GFX_PROP_I(Tilemap, Opacity)
DEF_GFX_PROP_I(Tilemap, BlendType)

void tilemapBindingInit() {
    VALUE klass = rb_define_class("TilemapAutotiles", rb_cObject);
#if RAPI_FULL > 187
    rb_define_alloc_func(klass, classAllocate<&TilemapAutotilesType>);
#else
    rb_define_alloc_func(klass, TilemapAutotilesAllocate);
#endif
    
    _rb_define_method(klass, "[]=", tilemapAutotilesSet);
    _rb_define_method(klass, "[]", tilemapAutotilesGet);
    
    klass = rb_define_class("Tilemap", rb_cObject);
#if RAPI_FULL > 187
    rb_define_alloc_func(klass, classAllocate<&TilemapType>);
#else
    rb_define_alloc_func(klass, TilemapAllocate);
#endif
    
    disposableBindingInit<Tilemap>(klass);
    
    _rb_define_method(klass, "initialize", tilemapInitialize);
    _rb_define_method(klass, "autotiles", tilemapGetAutotiles);
    _rb_define_method(klass, "update", tilemapUpdate);
    
    _rb_define_method(klass, "viewport", tilemapGetViewport);
    
    INIT_PROP_BIND(Tilemap, Tileset, "tileset");
    INIT_PROP_BIND(Tilemap, MapData, "map_data");
    INIT_PROP_BIND(Tilemap, FlashData, "flash_data");
    INIT_PROP_BIND(Tilemap, Priorities, "priorities");
    INIT_PROP_BIND(Tilemap, Visible, "visible");
    INIT_PROP_BIND(Tilemap, OX, "ox");
    INIT_PROP_BIND(Tilemap, OY, "oy");
    
    INIT_PROP_BIND(Tilemap, Opacity, "opacity");
    INIT_PROP_BIND(Tilemap, BlendType, "blend_type");
    INIT_PROP_BIND(Tilemap, Color, "color");
    INIT_PROP_BIND(Tilemap, Tone, "tone");
}
