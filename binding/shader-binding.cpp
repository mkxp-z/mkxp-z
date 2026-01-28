/*
 ** shader-binding.cpp
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
#include "disposable-binding.h"
#include "sharedstate.h"
#include "display/customshader.h"

#if RAPI_FULL > 187
DEF_TYPE(Shader);
#else
DEF_ALLOCFUNC(Shader);
#endif

RB_METHOD(shaderInitialize) {
    const char *filename;
    rb_get_args(argc, argv, "z", &filename RB_ARG_END);
    
    CustomShader *s = 0;
    
    GFX_LOCK;
    
    s = new CustomShader(filename);
    
    GFX_UNLOCK;
    
    setPrivateData(self, s);
    
    return self;
}

RB_METHOD_GUARD(shaderGetFilename) {
    RB_UNUSED_PARAM;
    
    CustomShader *s = getPrivateData<CustomShader>(self);
    
    return rb_utf8_str_new_cstr(s->getFilename().c_str());
}
RB_METHOD_GUARD_END

void shaderBindingInit() {
    VALUE klass = rb_define_class("Shader", rb_cObject);
#if RAPI_FULL > 187
    rb_define_alloc_func(klass, classAllocate<&ShaderType>);
#else
    rb_define_alloc_func(klass, ShaderAllocate);
#endif
    
    disposableBindingInit<CustomShader>(klass);
    
    _rb_define_method(klass, "initialize", shaderInitialize);
    _rb_define_method(klass, "filename", shaderGetFilename);
}
