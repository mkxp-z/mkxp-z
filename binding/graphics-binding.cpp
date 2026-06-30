/*
 ** graphics-binding.cpp
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

#include "config.h"
#include "graphics.h"
#include "bitmap.h"
#include "sharedstate.h"
#include "binding-util.h"
#include "binding-types.h"
#include "exception.h"

RB_METHOD(graphicsDelta) {
    RB_UNUSED_PARAM;
    GFX_LOCK;
    VALUE ret = rb_float_new(shState->graphics().getDelta());
    GFX_UNLOCK;
    return ret;
}

RB_METHOD_GUARD(graphicsUpdate)
{
    RB_UNUSED_PARAM;
#if RAPI_MAJOR >= 2
    drop_gvl_guard([](void*) -> void* {
        BINDING_GUARD_L(shState->graphics().update(e));
        return 0;
    }, 0, 0, 0);
#else
    BINDING_GUARD(shState->graphics().update(e));
#endif
    return Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD(graphicsAverageFrameRate)
{
    RB_UNUSED_PARAM;
    GFX_LOCK;
    VALUE ret = rb_float_new(shState->graphics().averageFrameRate());
    GFX_UNLOCK;
    return ret;
}

RB_METHOD_GUARD(graphicsFreeze)
{
    RB_UNUSED_PARAM;
    
#if RAPI_MAJOR >= 2
    drop_gvl_guard([](void*) -> void* {
        BINDING_GUARD_L(shState->graphics().freeze(e));
        return 0;
    }, 0, 0, 0);
#else
    BINDING_GUARD(shState->graphics().freeze(e));
#endif
    
    return Qnil;
}
RB_METHOD_GUARD_END

typedef struct {
    int duration;
    Bitmap *transMap;
    int vague;
} TransitionArgs;

RB_METHOD_GUARD(graphicsTransition)
{
    RB_UNUSED_PARAM;
    
    int duration = 8;
    const char *filename = "";
    int vague = 40;
    
    rb_get_args(argc, argv, "|izi", &duration, &filename, &vague RB_ARG_END);
    
    Bitmap *transMap;
    BINDING_GUARD_LF(delete transMap, transMap = *filename ? new Bitmap(e, filename) : 0);
    TransitionArgs args = {duration, transMap, vague};
    
#if RAPI_MAJOR >= 2
    drop_gvl_guard([](void *args) -> void* {
        TransitionArgs &a = *((TransitionArgs*)args);
        BINDING_GUARD_L(shState->graphics().transition(e,
                                                       a.duration,
                                                       a.transMap,
                                                       a.vague
                                                      ));
        return 0;
    }, &args, 0, 0);
#else
    BINDING_GUARD_L(shState->graphics().transition(e, duration, transMap, vague));
#endif
    
    return Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD(graphicsFrameReset)
{
    RB_UNUSED_PARAM;
    
    GFX_LOCK;
    shState->graphics().frameReset();
    GFX_UNLOCK;
    
    return Qnil;
}

#define DEF_GRA_PROP_I(PropName) \
RB_METHOD(graphics##Get##PropName) \
{ \
RB_UNUSED_PARAM; \
return rb_fix_new(shState->graphics().get##PropName()); \
} \
RB_METHOD(graphics##Set##PropName) \
{ \
RB_UNUSED_PARAM; \
int value; \
rb_get_args(argc, argv, "i", &value RB_ARG_END); \
GFX_LOCK; \
shState->graphics().set##PropName(value); \
GFX_UNLOCK; \
return rb_fix_new(value); \
}

#define DEF_GRA_PROP_B(PropName) \
RB_METHOD(graphics##Get##PropName) \
{ \
RB_UNUSED_PARAM; \
return rb_bool_new(shState->graphics().get##PropName()); \
} \
RB_METHOD(graphics##Set##PropName) \
{ \
RB_UNUSED_PARAM; \
bool value; \
rb_get_args(argc, argv, "b", &value RB_ARG_END); \
GFX_LOCK; \
shState->graphics().set##PropName(value); \
GFX_UNLOCK; \
return rb_bool_new(value); \
}

#define DEF_GRA_PROP_F(PropName) \
RB_METHOD(graphics##Get##PropName) \
{ \
RB_UNUSED_PARAM; \
return rb_float_new(shState->graphics().get##PropName()); \
} \
RB_METHOD(graphics##Set##PropName) \
{ \
RB_UNUSED_PARAM; \
double value; \
rb_get_args(argc, argv, "f", &value RB_ARG_END); \
GFX_LOCK; \
shState->graphics().set##PropName(value); \
GFX_UNLOCK; \
return rb_float_new(value); \
}

RB_METHOD(graphicsWidth)
{
    RB_UNUSED_PARAM;
    
    return rb_fix_new(shState->graphics().width());
}

RB_METHOD(graphicsHeight)
{
    RB_UNUSED_PARAM;
    
    return rb_fix_new(shState->graphics().height());
}

RB_METHOD(graphicsDisplayWidth)
{
    RB_UNUSED_PARAM;
    
    return rb_fix_new(shState->graphics().displayWidth());
}

RB_METHOD(graphicsDisplayHeight)
{
    RB_UNUSED_PARAM;
    
    return rb_fix_new(shState->graphics().displayHeight());
}

RB_METHOD_GUARD(graphicsWait)
{
    RB_UNUSED_PARAM;
    
    int duration;
    rb_get_args(argc, argv, "i", &duration RB_ARG_END);
#if RAPI_MAJOR >= 2
    drop_gvl_guard([](void* d) -> void* {
        BINDING_GUARD_L(shState->graphics().wait(e, *(int*)d));
        return 0;
    }, (int*)&duration, 0, 0);
#else
    BINDING_GUARD(shState->graphics().wait(e, duration));
#endif
    return Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(graphicsFadeout)
{
    RB_UNUSED_PARAM;
    
    int duration;
    rb_get_args(argc, argv, "i", &duration RB_ARG_END);
    
#if RAPI_MAJOR >= 2
    drop_gvl_guard([](void* d) -> void* {
        BINDING_GUARD_L(shState->graphics().fadeout(e, *(int*)d));
        return 0;
    }, (int*)&duration, 0, 0);
#else
    BINDING_GUARD(shState->graphics().fadeout(e, duration));
#endif
    
    return Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(graphicsFadein)
{
    RB_UNUSED_PARAM;
    
    int duration;
    rb_get_args(argc, argv, "i", &duration RB_ARG_END);
    
#if RAPI_MAJOR >= 2
    drop_gvl_guard([](void* d) -> void* {
        BINDING_GUARD_L(shState->graphics().fadein(e, *(int*)d));
        return 0;
    }, (int*)&duration, 0, 0);
#else
    BINDING_GUARD(shState->graphics().fadein(e, duration));
#endif
    
    return Qnil;
}
RB_METHOD_GUARD_END

void bitmapInitProps(Bitmap *b, VALUE self);

RB_METHOD_GUARD(graphicsSnapToBitmap)
{
    RB_UNUSED_PARAM;
    
    Bitmap *result = 0;
    
    BINDING_GUARD_L(result = shState->graphics().snapToBitmap(e));
    
    VALUE obj = wrapObject(result, BitmapType);
    bitmapInitProps(result, obj);
    
    return obj;
}
RB_METHOD_GUARD_END

RB_METHOD(graphicsResizeScreen)
{
    RB_UNUSED_PARAM;
    
    int width, height;
    rb_get_args(argc, argv, "ii", &width, &height RB_ARG_END);
    
    GFX_LOCK;
    shState->graphics().resizeScreen(width, height);
    GFX_UNLOCK;
    
    return Qnil;
}

RB_METHOD(graphicsResizeWindow)
{
    RB_UNUSED_PARAM;
    
    int width, height;
    bool center = false;
    rb_get_args(argc, argv, "ii|b", &width, &height, &center RB_ARG_END);
    
    
    GFX_LOCK;
    shState->graphics().resizeWindow(width, height, center);
    GFX_UNLOCK;
    
    return Qnil;
}

RB_METHOD_GUARD(graphicsReset)
{
    RB_UNUSED_PARAM;
    
    BINDING_GUARD_L(shState->graphics().reset(e));
    
    return Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD(graphicsCenter)
{
    RB_UNUSED_PARAM;
    
    shState->graphics().center();
    return Qnil;
}

typedef struct {
    const char *filename;
    int volume;
    bool skippable;
} PlayMovieArgs;

void *playMovieInternal(void *args) {
    PlayMovieArgs *a = (PlayMovieArgs*)args;
    BINDING_GUARD_L(shState->graphics().playMovie(e, a->filename, a->volume, a->skippable));
    
    // Signals for shutdown or reset only make playMovie quit early,
    // so check again
    shState->checkShutdown();
    shState->checkReset();
    
    return 0;
}

RB_METHOD_GUARD(graphicsPlayMovie)
{
    RB_UNUSED_PARAM;
    
    VALUE filename, volumeArg, skippable;
    rb_scan_args(argc, argv, "12", &filename, &volumeArg, &skippable);
    SafeStringValue(filename);
    
    bool skip;
    rb_bool_arg(skippable, &skip);

    // TODO: Video control inputs (e.g. skip, pause)

    PlayMovieArgs args{};
    args.filename = RSTRING_PTR(filename);
    args.volume = (volumeArg == Qnil) ? 100 : NUM2INT(volumeArg);;
    args.skippable = skip;
#if RAPI_MAJOR >= 2
    drop_gvl_guard(playMovieInternal, &args, 0, 0);
#else
    playMovieInternal(&args);
#endif
    
    return Qnil;
}
RB_METHOD_GUARD_END

void graphicsScreenshotInternal(const char *filename)
{
    BINDING_GUARD_L(shState->graphics().screenshot(e, filename));
}

RB_METHOD_GUARD(graphicsScreenshot)
{
    RB_UNUSED_PARAM;

    VALUE filename;
    rb_scan_args(argc, argv, "1", &filename);
    SafeStringValue(filename);
    
#if RAPI_MAJOR >= 2
    drop_gvl_guard([](void* fn) -> void* {
        graphicsScreenshotInternal((const char*)fn);
        return 0;
    }, (void*)RSTRING_PTR(filename), 0, 0);
#else
    graphicsScreenshotInternal(RSTRING_PTR(filename));
#endif
    return Qnil;
}
RB_METHOD_GUARD_END

DEF_GRA_PROP_I(FrameRate)
DEF_GRA_PROP_I(FrameCount)
DEF_GRA_PROP_I(Brightness)

DEF_GRA_PROP_B(Fullscreen)
DEF_GRA_PROP_B(ShowCursor)
DEF_GRA_PROP_F(Scale)
DEF_GRA_PROP_B(Frameskip)
DEF_GRA_PROP_B(FixedAspectRatio)
DEF_GRA_PROP_I(SmoothScaling)
DEF_GRA_PROP_B(IntegerScaling)
DEF_GRA_PROP_B(LastMileScaling)
DEF_GRA_PROP_B(Threadsafe)

#define INIT_GRA_PROP_BIND(PropName, prop_name_s) \
{ \
_rb_define_module_function(module, prop_name_s, graphics##Get##PropName); \
_rb_define_module_function(module, prop_name_s "=", graphics##Set##PropName); \
}

void graphicsBindingInit()
{
    VALUE module = rb_define_module("Graphics");
    
    _rb_define_module_function(module, "delta", graphicsDelta);
    _rb_define_module_function(module, "update", graphicsUpdate);
    _rb_define_module_function(module, "freeze", graphicsFreeze);
    _rb_define_module_function(module, "transition", graphicsTransition);
    _rb_define_module_function(module, "frame_reset", graphicsFrameReset);
    _rb_define_module_function(module, "screenshot", graphicsScreenshot);
    
    _rb_define_module_function(module, "__reset__", graphicsReset);
    
    INIT_GRA_PROP_BIND( FrameRate,  "frame_rate"  );
    INIT_GRA_PROP_BIND( FrameCount, "frame_count" );
    _rb_define_module_function(module, "average_frame_rate", graphicsAverageFrameRate);

    _rb_define_module_function(module, "width", graphicsWidth);
    _rb_define_module_function(module, "height", graphicsHeight);
    _rb_define_module_function(module, "display_width", graphicsDisplayWidth);
    _rb_define_module_function(module, "display_height", graphicsDisplayHeight);
    _rb_define_module_function(module, "wait", graphicsWait);
    _rb_define_module_function(module, "fadeout", graphicsFadeout);
    _rb_define_module_function(module, "fadein", graphicsFadein);
    _rb_define_module_function(module, "snap_to_bitmap", graphicsSnapToBitmap);
    _rb_define_module_function(module, "resize_screen", graphicsResizeScreen);
    _rb_define_module_function(module, "resize_window", graphicsResizeWindow);
    _rb_define_module_function(module, "center", graphicsCenter);
        
    INIT_GRA_PROP_BIND( Brightness, "brightness" );

    // end
    
    //if (rgssVer >= 3)
    //{
    _rb_define_module_function(module, "play_movie", graphicsPlayMovie);
    //}
    
    INIT_GRA_PROP_BIND( Fullscreen,       "fullscreen"         );
    INIT_GRA_PROP_BIND( ShowCursor,       "show_cursor"        );
    INIT_GRA_PROP_BIND( Scale,            "scale"              );
    INIT_GRA_PROP_BIND( Frameskip,        "frameskip"          );
    INIT_GRA_PROP_BIND( FixedAspectRatio, "fixed_aspect_ratio" );
    INIT_GRA_PROP_BIND( SmoothScaling,    "smooth_scaling"     );
    INIT_GRA_PROP_BIND( IntegerScaling,   "integer_scaling"    );
    INIT_GRA_PROP_BIND( LastMileScaling,  "last_mile_scaling"  );
    INIT_GRA_PROP_BIND( Threadsafe,       "thread_safe"        );
}
