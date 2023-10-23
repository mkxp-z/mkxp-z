/*
 ** binding-mri.cpp
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

#include "audio/audio.h"
#include "filesystem/filesystem.h"
#include "display/graphics.h"
#include "display/font.h"
#include "system/system.h"

#include "util/util.h"
#include "util/sdl-util.h"
#include "util/debugwriter.h"
#include "util/boost-hash.h"
#include "util/exception.h"
#include "util/encoding.h"

#include "config.h"

#include "binding-util.h"
#include "binding.h"

#include "sharedstate.h"
#include "eventthread.h"

#include <vector>

extern "C" {
#include <ruby.h>

#if RAPI_FULL >= 190
#include <ruby/encoding.h>
#endif
}

#include "binding-mri.h"

#ifdef __WIN32__
#include "binding-mri-win32.h"
#endif

#include "ConfigManager.h"
#include "ThreadManager.h"

#include <assert.h>
#include <string>
#include <zlib.h>

#include <SDL_cpuinfo.h>
#include <SDL_filesystem.h>
#include <SDL_loadso.h>
#include <SDL_power.h>

extern const char module_rpg1[];
extern const char module_rpg2[];
extern const char module_rpg3[];

void tableBindingInit();

void etcBindingInit();

void fontBindingInit();

void bitmapBindingInit();

void spriteBindingInit();

void viewportBindingInit();

void planeBindingInit();

void windowBindingInit();

void tilemapBindingInit();

void windowVXBindingInit();

void tilemapVXBindingInit();

void inputBindingInit();

void audioBindingInit();

void graphicsBindingInit();

void fileIntBindingInit();

#ifdef MKXPZ_MINIFFI
void MiniFFIBindingInit();
#endif

#ifdef MKXPZ_STEAM
void CUSLBindingInit();
#endif

void httpBindingInit();

void mkxpzBindingInit();

RB_METHOD(mkxpDelta);

RB_METHOD(mriPrint);

RB_METHOD(mriP);

RB_METHOD(mkxpDataDirectory);

RB_METHOD(mkxpSetTitle);

RB_METHOD(mkxpGetTitle);

RB_METHOD(mkxpDesensitize);

RB_METHOD(mkxpPuts);

RB_METHOD(mkxpPlatform);

RB_METHOD(mkxpIsMacHost);

RB_METHOD(mkxpIsWindowsHost);

RB_METHOD(mkxpIsLinuxHost);

RB_METHOD(mkxpIsUsingRosetta);

RB_METHOD(mkxpIsUsingWine);

RB_METHOD(mkxpIsReallyMacHost);

RB_METHOD(mkxpIsReallyLinuxHost);

RB_METHOD(mkxpIsReallyWindowsHost);

RB_METHOD(mkxpUserLanguage);

RB_METHOD(mkxpUserName);

RB_METHOD(mkxpGameTitle);

RB_METHOD(mkxpPowerState);

RB_METHOD(mkxpSettingsMenu);

RB_METHOD(mkxpCpuCount);

RB_METHOD(mkxpSystemMemory);

RB_METHOD(mkxpReloadPathCache);

RB_METHOD(mkxpAddPath);

RB_METHOD(mkxpRemovePath);

RB_METHOD(mkxpFileExists);

RB_METHOD(mkxpLaunch);

RB_METHOD(mkxpGetJSONSetting);

RB_METHOD(mkxpSetJSONSetting);

RB_METHOD(mkxpGetAllJSONSettings);

RB_METHOD(mkxpSetDefaultFontFamily);

RB_METHOD(mriRgssMain);

RB_METHOD(mriRgssStop);

RB_METHOD(_kernelCaller);

RB_METHOD(mkxpStringToUTF8);

RB_METHOD(mkxpStringToUTF8Bang);

VALUE json2rb(json5pp::value const &v);

json5pp::value rb2json(VALUE v);

extern "C" {
RUBY_FUNC_EXPORTED void initBindings(void) {
    Debug() << "Loading MKXP-Z!";

    std::vector<std::string> launchArgs;
    auto nativeResult = rb_eval_string("MKXP_Z.get_launch_args rescue []");
    if (TYPE(nativeResult) == T_ARRAY) {
        long len = rb_array_len(nativeResult);
        for (long i = 0; i < len; i++) {
            auto a = rb_ary_entry(nativeResult, i);
            if (TYPE(a) == T_STRING) {
                launchArgs.emplace_back(rb_string_value_cstr(&a));
            }
        }
    }

    // TODO: Change this to actually get the name of the exe
    auto &cm =  ConfigManager::getInstance();
    cm.initConfig("Ruby.exe", launchArgs);

    tableBindingInit();
    etcBindingInit();
    fontBindingInit();
    bitmapBindingInit();
    spriteBindingInit();
    viewportBindingInit();
    planeBindingInit();
    
    if (rgssVer == 1) {
        windowBindingInit();
        tilemapBindingInit();
    } else {
        windowVXBindingInit();
        tilemapVXBindingInit();
    }
    
    inputBindingInit();
    audioBindingInit();
    graphicsBindingInit();

    fileIntBindingInit();

#ifdef MKXPZ_MINIFFI
    MiniFFIBindingInit();
#endif

#ifdef MKXPZ_STEAM
    CUSLBindingInit();
#endif
    
    //httpBindingInit();

    mkxpzBindingInit();
    
    if (rgssVer >= 3) {
        _rb_define_module_function(rb_mKernel, "rgss_main", mriRgssMain);
        _rb_define_module_function(rb_mKernel, "rgss_stop", mriRgssStop);

        _rb_define_module_function(rb_mKernel, "msgbox", mriPrint);
        _rb_define_module_function(rb_mKernel, "msgbox_p", mriP);

        rb_define_global_const("RGSS_VERSION", rb_utf8_str_new_cstr("3.0.1"));
    } else {
        _rb_define_module_function(rb_mKernel, "print", mriPrint);
        _rb_define_module_function(rb_mKernel, "p", mriP);

        rb_define_alias(rb_singleton_class(rb_mKernel), "_mkxp_kernel_caller_alias",
                        "caller");
        _rb_define_module_function(rb_mKernel, "caller", _kernelCaller);
    }

    if (rgssVer == 1)
        rb_eval_string(module_rpg1);
    else if (rgssVer == 2)
        rb_eval_string(module_rpg2);
    else if (rgssVer == 3)
        rb_eval_string(module_rpg3);
    else
        assert(!"unreachable");
    
    VALUE mod = rb_define_module("System");
    _rb_define_module_function(mod, "delta", mkxpDelta);
    _rb_define_module_function(mod, "uptime", mkxpDelta);
    _rb_define_module_function(mod, "data_directory", mkxpDataDirectory);
    _rb_define_module_function(mod, "set_window_title", mkxpSetTitle);
    _rb_define_module_function(mod, "window_title", mkxpGetTitle);
    _rb_define_module_function(mod, "window_title=", mkxpSetTitle);
    _rb_define_module_function(mod, "show_settings", mkxpSettingsMenu);
    _rb_define_module_function(mod, "puts", mkxpPuts);
    _rb_define_module_function(mod, "desensitize", mkxpDesensitize);
    _rb_define_module_function(mod, "platform", mkxpPlatform);

    _rb_define_module_function(mod, "is_mac?", mkxpIsMacHost);
    _rb_define_module_function(mod, "is_rosetta?", mkxpIsUsingRosetta);

    _rb_define_module_function(mod, "is_linux?", mkxpIsLinuxHost);

    _rb_define_module_function(mod, "is_windows?", mkxpIsWindowsHost);
    _rb_define_module_function(mod, "is_wine?", mkxpIsUsingWine);
    _rb_define_module_function(mod, "is_really_mac?", mkxpIsReallyMacHost);
    _rb_define_module_function(mod, "is_really_linux?", mkxpIsReallyLinuxHost);
    _rb_define_module_function(mod, "is_really_windows?", mkxpIsReallyWindowsHost);


    _rb_define_module_function(mod, "user_language", mkxpUserLanguage);
    _rb_define_module_function(mod, "user_name", mkxpUserName);
    _rb_define_module_function(mod, "game_title", mkxpGameTitle);
    _rb_define_module_function(mod, "power_state", mkxpPowerState);
    _rb_define_module_function(mod, "nproc", mkxpCpuCount);
    _rb_define_module_function(mod, "memory", mkxpSystemMemory);
    _rb_define_module_function(mod, "reload_cache", mkxpReloadPathCache);
    _rb_define_module_function(mod, "mount", mkxpAddPath);
    _rb_define_module_function(mod, "unmount", mkxpRemovePath);
    _rb_define_module_function(mod, "file_exist?", mkxpFileExists);
    _rb_define_module_function(mod, "launch", mkxpLaunch);

    _rb_define_module_function(mod, "default_font_family=", mkxpSetDefaultFontFamily);

    _rb_define_method(rb_cString, "to_utf8", mkxpStringToUTF8);
    _rb_define_method(rb_cString, "to_utf8!", mkxpStringToUTF8Bang);

    /*
    VALUE cmod = rb_define_module("CFG");
    _rb_define_module_function(cmod, "[]", mkxpGetJSONSetting);
    _rb_define_module_function(cmod, "[]=", mkxpSetJSONSetting);
    _rb_define_module_function(cmod, "to_hash", mkxpGetAllJSONSettings);
     */

    /* Load global constants */
    rb_gv_set("MKXP", Qtrue);
    
    VALUE debug = rb_bool_new(shState->config()->editor.debug);
    if (rgssVer == 1)
        rb_gv_set("DEBUG", debug);
    else if (rgssVer >= 2)
        rb_gv_set("TEST", debug);
    
    rb_gv_set("BTEST", rb_bool_new(shState->config()->editor.battleTest));
    
#ifdef MKXPZ_BUILD_XCODE
    std::string version = std::string(MKXPZ_VERSION "/") + getPlistValue("GIT_COMMIT_HASH");
    VALUE vers = rb_utf8_str_new_cstr(version.c_str());
#else
    VALUE vers = rb_utf8_str_new_cstr(MKXPZ_VERSION "/" MKXPZ_GIT_HASH);
#endif
    rb_str_freeze(vers);
    rb_define_const(mod, "VERSION", vers);

    // Automatically load zlib if it's present -- the correct way this time
    int state;
    rb_eval_string_protect("require('zlib') if !Kernel.const_defined?(:Zlib)", &state);
    if (state) {
        Debug() << "Could not load Zlib. If this is important, make sure Ruby was built with static extensions, or that"
                << ((MKXPZ_PLATFORM == MKXPZ_PLATFORM_MACOS) ? "zlib.bundle" : "zlib.so")
                << "is present and reachable by Ruby's loadpath.";
    }

    // Set $stdout and its ilk accordingly on Windows
    // I regret teaching you that word
#ifdef __WIN32__
    if (cm.getConfig()->winConsole)
        configureWindowsStreams();
#endif
}
}

static void showMsg(const std::string &msg) {
    shState->eThread().showMessageBox(msg.c_str());
}

static void printP(int argc, VALUE *argv, const char *convMethod,
                   const char *sep) {
    VALUE dispString = rb_str_buf_new(128);
    ID conv = rb_intern(convMethod);

    for (int i = 0; i < argc; ++i) {
        VALUE str = rb_funcall2(argv[i], conv, 0, NULL);
        rb_str_buf_append(dispString, str);

        if (i < argc)
                    rb_str_buf_cat2 (dispString, sep);
    }

    showMsg(RSTRING_PTR(dispString));
}


RB_METHOD(mriPrint) {
    RB_UNUSED_PARAM;

    printP(argc, argv, "to_s", "");

    return Qnil;
}

RB_METHOD(mriP) {
    RB_UNUSED_PARAM;

    printP(argc, argv, "inspect", "\n");

    return Qnil;
}

RB_METHOD(mkxpDelta) {
    RB_UNUSED_PARAM;
    return rb_float_new(shState->runTime());
}

RB_METHOD(mkxpDataDirectory) {
    RB_UNUSED_PARAM;

    const std::string &path = shState->config()->customDataPath;
    const char *s = path.empty() ? "." : path.c_str();

    std::string s_nml = shState->filesystem()->normalize(s, 1, 1);
    VALUE ret = rb_utf8_str_new_cstr(s_nml.c_str());

    return ret;
}

RB_METHOD(mkxpSetTitle) {
    RB_UNUSED_PARAM;

    VALUE s;
    rb_scan_args(argc, argv, "1", &s);
    SafeStringValue(s);

    shState->eThread().requestWindowRename(RSTRING_PTR(s));
    return s;
}

RB_METHOD(mkxpGetTitle) {
    RB_UNUSED_PARAM;

    rb_check_argc(argc, 0);

    return rb_utf8_str_new_cstr(SDL_GetWindowTitle(shState->sdlWindow().get()));
}

RB_METHOD(mkxpDesensitize) {
    RB_UNUSED_PARAM;

    VALUE filename;
    rb_scan_args(argc, argv, "1", &filename);
    SafeStringValue(filename);

    return rb_utf8_str_new_cstr(
            shState->filesystem()->desensitize(RSTRING_PTR(filename)));
}

RB_METHOD(mkxpPuts) {
    RB_UNUSED_PARAM;

    const char *str;
    rb_get_args(argc, argv, "z", &str RB_ARG_END);

    Debug() << str;

    return Qnil;
}

RB_METHOD(mkxpPlatform) {
    RB_UNUSED_PARAM;

#if MKXPZ_PLATFORM == MKXPZ_PLATFORM_MACOS
    std::string platform("macOS");
    
    if (mkxp_sys::isRosetta())
        platform += " (Rosetta)";

#elif MKXPZ_PLATFORM == MKXPZ_PLATFORM_WINDOWS
    std::string platform("Windows");

    if (mkxp_sys::isWine()) {
        platform += " (Wine - ";
        switch (mkxp_sys::getRealHostType()) {
            case mkxp_sys::WineHostType::Mac:
                platform += "macOS)";
                break;
            default:
                platform += "Linux)";
                break;
        }
    }
#else
    std::string platform("Linux");
#endif

    return rb_utf8_str_new_cstr(platform.c_str());
}

RB_METHOD(mkxpIsMacHost) {
    RB_UNUSED_PARAM;

    return rb_bool_new(MKXPZ_PLATFORM == MKXPZ_PLATFORM_MACOS);
}

RB_METHOD(mkxpIsUsingRosetta) {
    RB_UNUSED_PARAM;

    return rb_bool_new(mkxp_sys::isRosetta());
}

RB_METHOD(mkxpIsLinuxHost) {
    RB_UNUSED_PARAM;

    return rb_bool_new(MKXPZ_PLATFORM == MKXPZ_PLATFORM_LINUX);
}

RB_METHOD(mkxpIsWindowsHost) {
    RB_UNUSED_PARAM;

    return rb_bool_new(MKXPZ_PLATFORM == MKXPZ_PLATFORM_WINDOWS);
}

RB_METHOD(mkxpIsUsingWine) {
    RB_UNUSED_PARAM;
    return rb_bool_new(mkxp_sys::isWine());
}

RB_METHOD(mkxpIsReallyMacHost) {
    RB_UNUSED_PARAM;
    return rb_bool_new(mkxp_sys::getRealHostType() == mkxp_sys::WineHostType::Mac);
}

RB_METHOD(mkxpIsReallyLinuxHost) {
    RB_UNUSED_PARAM;
    return rb_bool_new(mkxp_sys::getRealHostType() == mkxp_sys::WineHostType::Linux);
}

RB_METHOD(mkxpIsReallyWindowsHost) {
    RB_UNUSED_PARAM;
    return rb_bool_new(mkxp_sys::getRealHostType() == mkxp_sys::WineHostType::Windows);
}

RB_METHOD(mkxpUserLanguage) {
    RB_UNUSED_PARAM;

    return rb_utf8_str_new_cstr(mkxp_sys::getSystemLanguage().c_str());
}

RB_METHOD(mkxpUserName) {
    RB_UNUSED_PARAM;

    // Using the Windows API isn't working with usernames that involve Unicode
    // characters for some dumb reason
#ifdef __WIN32__
    VALUE env = rb_const_get(rb_mKernel, rb_intern("ENV"));
    return rb_funcall(env, rb_intern("[]"), 1, rb_str_new_cstr("USERNAME"));
#else
    return rb_utf8_str_new_cstr(mkxp_sys::getUserName().c_str());
#endif
}

RB_METHOD(mkxpGameTitle) {
    RB_UNUSED_PARAM;

    return rb_utf8_str_new_cstr(shState->config()->game.title.c_str());
}

RB_METHOD(mkxpPowerState) {
    RB_UNUSED_PARAM;

    int secs, pct;
    SDL_PowerState ps = SDL_GetPowerInfo(&secs, &pct);

    VALUE hash = rb_hash_new();

    rb_hash_aset(hash, ID2SYM(rb_intern("seconds")),
                 (secs > -1) ? INT2NUM(secs) : RUBY_Qnil);

    rb_hash_aset(hash, ID2SYM(rb_intern("percent")),
                 (pct > -1) ? INT2NUM(pct) : RUBY_Qnil);

    rb_hash_aset(hash, ID2SYM(rb_intern("discharging")),
                 rb_bool_new(ps == SDL_POWERSTATE_ON_BATTERY));

    return hash;
}

RB_METHOD(mkxpSettingsMenu) {
    RB_UNUSED_PARAM;

    shState->eThread().requestSettingsMenu();

    return Qnil;
}

RB_METHOD(mkxpCpuCount) {
    RB_UNUSED_PARAM;

    return INT2NUM(SDL_GetCPUCount());
}

RB_METHOD(mkxpSystemMemory) {
    RB_UNUSED_PARAM;

    return INT2NUM(SDL_GetSystemRAM());
}

RB_METHOD(mkxpReloadPathCache) {
    RB_UNUSED_PARAM;

    shState->filesystem()->reloadPathCache();
    return Qnil;
}

RB_METHOD(mkxpAddPath) {
    RB_UNUSED_PARAM;

    VALUE path, mountpoint, reload;
    rb_scan_args(argc, argv, "12", &path, &mountpoint, &reload);
    SafeStringValue(path);
    if (mountpoint != Qnil) SafeStringValue(mountpoint);

    const char *mp = (mountpoint == Qnil) ? 0 : RSTRING_PTR(mountpoint);

    try {
        bool rl = true;
        if (reload != Qnil)
            rb_bool_arg(reload, &rl);

        shState->filesystem()->addPath(RSTRING_PTR(path), mp, rl);
    } catch (Exception &e) {
        raiseRbExc(e);
    }
    return path;
}

RB_METHOD(mkxpRemovePath) {
    RB_UNUSED_PARAM;

    VALUE path, reload;
    rb_scan_args(argc, argv, "11", &path, &reload);
    SafeStringValue(path);

    try {
        bool rl = true;
        if (reload != Qnil)
            rb_bool_arg(reload, &rl);

        shState->filesystem()->removePath(RSTRING_PTR(path), rl);
    } catch (Exception &e) {
        raiseRbExc(e);
    }
    return path;
}

RB_METHOD(mkxpFileExists) {
    RB_UNUSED_PARAM;

    VALUE path;
    rb_scan_args(argc, argv, "1", &path);
    SafeStringValue(path);

    if (shState->filesystem()->exists(RSTRING_PTR(path)))
        return Qtrue;
    return Qfalse;
}

RB_METHOD(mkxpSetDefaultFontFamily) {
    RB_UNUSED_PARAM;

    VALUE familyV;
    rb_scan_args(argc, argv, "1", &familyV);
    SafeStringValue(familyV);

    std::string family(RSTRING_PTR(familyV));
    shState->fontState().setDefaultFontFamily(family);

    return Qnil;
}

RB_METHOD(mkxpStringToUTF8) {
    RB_UNUSED_PARAM;

    rb_check_argc(argc, 0);

    std::string ret(RSTRING_PTR(self), RSTRING_LEN(self));
    GUARD_EXC(ret = Encoding::convertString(ret););

    return rb_utf8_str_new(ret.c_str(), ret.length());
}

RB_METHOD(mkxpStringToUTF8Bang) {
    RB_UNUSED_PARAM;

    rb_check_argc(argc, 0);

    std::string ret(RSTRING_PTR(self), RSTRING_LEN(self));
    GUARD_EXC(ret = Encoding::convertString(ret););

    rb_str_resize(self, ret.length());
    memcpy(RSTRING_PTR(self), ret.c_str(), RSTRING_LEN(self));

#if RAPI_FULL >= 190
    rb_funcall(self, rb_intern("force_encoding"), 1, rb_enc_from_encoding(rb_utf8_encoding()));
#endif

    return self;
}

#ifdef __APPLE__
#define OPENCMD "open "
#define OPENARGS "--args"
#elif defined(__linux__)
#define OPENCMD "xdg-open "
#define OPENARGS ""
#else
#define OPENCMD "start /b \"launch\" "
#define OPENARGS ""
#endif

RB_METHOD(mkxpLaunch) {
    RB_UNUSED_PARAM;

    VALUE cmdname, args;

    rb_scan_args(argc, argv, "11", &cmdname, &args);
    SafeStringValue(cmdname);

    std::string command(OPENCMD);
    command += "\"";
    command += RSTRING_PTR(cmdname);
    command += "\"";

    if (args != RUBY_Qnil) {
#ifndef __linux__
        command += " ";
        command += OPENARGS;
        Check_Type(args, T_ARRAY);

        for (int i = 0; i < RARRAY_LEN(args); i++) {
            VALUE arg = rb_ary_entry(args, i);
            SafeStringValue(arg);

            if (RSTRING_LEN(arg) <= 0)
                continue;

            command += " ";
            command += RSTRING_PTR(arg);
        }
#else
        Debug() << command << ":" << "Arguments are not supported with xdg-open. Ignoring.";
#endif
    }

    if (std::system(command.c_str()) != 0) {
        raiseRbExc(Exception(Exception::MKXPError, "Failed to launch \"%s\"", RSTRING_PTR(cmdname)));
    }

    return RUBY_Qnil;
}

json5pp::value loadUserSettings() {
    json5pp::value ret;
    VALUE cpath = rb_utf8_str_new_cstr(shState->config()->userConfPath.c_str());

    if (rb_funcall(rb_cFile, rb_intern("exists?"), 1, cpath) == Qtrue) {
        VALUE f = rb_funcall(rb_cFile, rb_intern("open"), 2, cpath, rb_str_new("r", 1));
        VALUE data = rb_funcall(f, rb_intern("read"), 0);
        rb_funcall(f, rb_intern("close"), 0);
        ret = json5pp::parse5(RSTRING_PTR(data));
    }

    if (!ret.is_object())
        ret = json5pp::object({});

    return ret;
}

void saveUserSettings(json5pp::value &settings) {
    VALUE cpath = rb_utf8_str_new_cstr(shState->config()->userConfPath.c_str());
    VALUE f = rb_funcall(rb_cFile, rb_intern("open"), 2, cpath, rb_str_new("w", 1));
    rb_funcall(f, rb_intern("write"), 1,
               rb_utf8_str_new_cstr(settings.stringify5(json5pp::rule::space_indent<>()).c_str()));
    rb_funcall(f, rb_intern("close"), 0);
}

/*
RB_METHOD(mkxpGetJSONSetting) {
    RB_UNUSED_PARAM;

    VALUE sname;
    rb_scan_args(argc, argv, "1", &sname);
    SafeStringValue(sname);

    auto settings = loadUserSettings();
    auto &s = settings.as_object();

    if (s[RSTRING_PTR(sname)].is_null()) {
        return json2rb(shState->config()->raw.as_object()[RSTRING_PTR(sname)]);
    }

    return json2rb(s[RSTRING_PTR(sname)]);

}

RB_METHOD(mkxpSetJSONSetting) {
    RB_UNUSED_PARAM;

    VALUE sname, svalue;
    rb_scan_args(argc, argv, "2", &sname, &svalue);
    SafeStringValue(sname);

    auto settings = loadUserSettings();
    auto &s = settings.as_object();
    s[RSTRING_PTR(sname)] = rb2json(svalue);
    saveUserSettings(settings);

    return Qnil;
}

RB_METHOD(mkxpGetAllJSONSettings) {
    RB_UNUSED_PARAM;

    return json2rb(shState->config()->raw);
}
 */

static VALUE rgssMainCb(VALUE block) {
    rb_funcall2(block, rb_intern("call"), 0, 0);
    return Qnil;
}

static VALUE rgssMainRescue(VALUE arg, VALUE exc) {
    VALUE *excRet = (VALUE *) arg;

    *excRet = exc;

    return Qnil;
}

static void processReset() {
    shState->graphics().reset();
    shState->audio().reset();

    shState->rtData()->rqReset.clear();
    shState->graphics().repaintWait(shState->rtData()->rqResetFinish, false);
}

RB_METHOD(mriRgssMain) {
    RB_UNUSED_PARAM;

    while (true) {
        VALUE exc = Qnil;
#if RAPI_FULL < 270
        rb_rescue2((VALUE(*)(ANYARGS))rgssMainCb, rb_block_proc(),
                   (VALUE(*)(ANYARGS))rgssMainRescue, (VALUE)&exc, rb_eException,
                   (VALUE)0);
#else
        rb_rescue2(rgssMainCb, rb_block_proc(), rgssMainRescue, (VALUE) &exc,
                   rb_eException, (VALUE) 0);
#endif

        if (NIL_P(exc))
            break;

        if (rb_obj_class(exc) == getRbData()->exc[Reset])
            processReset();
        else
            rb_exc_raise(exc);
    }

    return Qnil;
}

RB_METHOD(mriRgssStop) {
    RB_UNUSED_PARAM;

    while (true)
        shState->graphics().update();

    return Qnil;
}

RB_METHOD(_kernelCaller) {
    RB_UNUSED_PARAM;

    VALUE trace =
            rb_funcall2(rb_mKernel, rb_intern("_mkxp_kernel_caller_alias"), 0, 0);

    if (!RB_TYPE_P(trace, RUBY_T_ARRAY))
        return trace;

    long len = RARRAY_LEN(trace);

    if (len < 2)
        return trace;

    /* Remove useless "ruby:1:in 'eval'" */
    rb_ary_pop(trace);

    /* Also remove trace of this helper function */
    rb_ary_shift(trace);

    len -= 2;

    if (len == 0)
        return trace;

    /* RMXP does this, not sure if specific or 1.8 related */
    VALUE args[] = {rb_utf8_str_new_cstr(":in `<main>'"), rb_utf8_str_new_cstr("")};
    rb_funcall2(rb_ary_entry(trace, len - 1), rb_intern("gsub!"), 2, args);

    return trace;
}

#if RAPI_FULL > 187

static VALUE newStringUTF8(const char *string, long length) {
    return rb_enc_str_new(string, length, rb_utf8_encoding());
}

#else
#define newStringUTF8 rb_str_new
#endif

struct evalArg {
    VALUE string;
    VALUE filename;
};

static VALUE evalHelper(evalArg *arg) {
    VALUE argv[] = {arg->string, Qnil, arg->filename};
    return rb_funcall2(Qnil, rb_intern("eval"), ARRAY_SIZE(argv), argv);
}

static VALUE evalString(VALUE string, VALUE filename, int *state) {
    evalArg arg = {string, filename};
    return rb_protect((VALUE(*)(VALUE)) evalHelper, (VALUE) &arg, state);
}

VALUE kernelLoadDataInt(const char *filename, bool rubyExc, bool raw);

struct BacktraceData {
    /* Maps: Ruby visible filename, To: Actual script name */
    BoostHash<std::string, std::string> scriptNames;
};

bool evalScript(VALUE string, const char *filename) {
    int state;
    evalString(string, rb_utf8_str_new_cstr(filename), &state);
    if (state) return false;
    return true;
}


#define SCRIPT_SECTION_FMT (rgssVer >= 3 ? "{%04ld}" : "Section%03ld")

static void runRMXPScripts(BacktraceData &btData) {
    const Config &conf = *shState->rtData()->config;
    const std::string &scriptPack = conf.game.scripts;

    if (scriptPack.empty()) {
        showMsg("No script file has been specified. Check the game's INI and try again.");
        return;
    }

    if (!shState->filesystem()->exists(scriptPack.c_str())) {
        showMsg("Unable to load scripts from '" + scriptPack + "'");
        return;
    }

    VALUE scriptArray;

    /* We checked if Scripts.rxdata exists, but something might
     * still go wrong */
    try {
        scriptArray = kernelLoadDataInt(scriptPack.c_str(), false, false);
    } catch (const Exception &e) {
        showMsg(std::string("Failed to read script data: ") + e.msg);
        return;
    }

    if (!RB_TYPE_P(scriptArray, RUBY_T_ARRAY)) {
        showMsg("Failed to read script data");
        return;
    }

    rb_gv_set("$RGSS_SCRIPTS", scriptArray);

    long scriptCount = RARRAY_LEN(scriptArray);

    std::string decodeBuffer;
    decodeBuffer.resize(0x1000);

    for (long i = 0; i < scriptCount; ++i) {
        VALUE script = rb_ary_entry(scriptArray, i);

        if (!RB_TYPE_P(script, RUBY_T_ARRAY))
            continue;

        VALUE scriptName = rb_ary_entry(script, 1);
        VALUE scriptString = rb_ary_entry(script, 2);

        int result = Z_OK;
        unsigned long bufferLen;

        while (true) {
            unsigned char *bufferPtr = reinterpret_cast<unsigned char *>(
                    const_cast<char *>(decodeBuffer.c_str()));
            const unsigned char *sourcePtr =
                    reinterpret_cast<const unsigned char *>(RSTRING_PTR(scriptString));

            bufferLen = decodeBuffer.length();

            result = uncompress(bufferPtr, &bufferLen, sourcePtr,
                                RSTRING_LEN(scriptString));

            bufferPtr[bufferLen] = '\0';

            if (result != Z_BUF_ERROR)
                break;

            decodeBuffer.resize(decodeBuffer.size() * 2);
        }

        if (result != Z_OK) {
            static char buffer[256];
            snprintf(buffer, sizeof(buffer), "Error decoding script %ld: '%s'", i,
                     RSTRING_PTR(scriptName));

            showMsg(buffer);

            break;
        }

        rb_ary_store(script, 3, rb_utf8_str_new_cstr(decodeBuffer.c_str()));
    }

    /* Execute preloaded scripts */
    /*
    for (std::vector<std::string>::const_iterator i = conf.preloadScripts.begin();
         i != conf.preloadScripts.end(); ++i)
        runCustomScript(*i);
        */
}

static void showExc(VALUE exc, const BacktraceData &btData) {
    VALUE bt = rb_funcall2(exc, rb_intern("backtrace"), 0, NULL);
    VALUE msg = rb_funcall2(exc, rb_intern("message"), 0, NULL);
    VALUE bt0 = rb_ary_entry(bt, 0);
    VALUE name = rb_class_path(rb_obj_class(exc));

    VALUE ds = rb_sprintf("%" PRIsVALUE ": %" PRIsVALUE " (%" PRIsVALUE ")",
#if RAPI_MAJOR >= 2
                          bt0, exc, name);
#else
    // Ruby 1.9's version of this function needs char*
    RSTRING_PTR(bt0), RSTRING_PTR(exc), RSTRING_PTR(name));
#endif
    /* omit "useless" last entry (from ruby:1:in `eval') */
    for (long i = 1, btlen = RARRAY_LEN(bt) - 1; i < btlen; ++i)
        rb_str_catf(ds, "\n\tfrom %" PRIsVALUE,
#if RAPI_MAJOR >= 2
                    rb_ary_entry(bt, i));
#else
    RSTRING_PTR(rb_ary_entry(bt, i)));
#endif
    Debug() << StringValueCStr(ds);

    char *s = RSTRING_PTR(bt0);

    char line[16];
    std::string file(512, '\0');

    char *p = s + strlen(s);
    char *e;

    while (p != s)
        if (*--p == ':')
            break;

    e = p;

    while (p != s)
        if (*--p == ':')
            break;

    /* s         p  e
     * SectionXXX:YY: in 'blabla' */

    *e = '\0';
    strncpy(line, *p ? p + 1 : p, sizeof(line));
    line[sizeof(line) - 1] = '\0';
    *e = ':';
    e = p;

    /* s         e
     * SectionXXX:YY: in 'blabla' */

    *e = '\0';
    strncpy(&file[0], s, file.size());
    *e = ':';

    /* Shrink to fit */
    file.resize(strlen(file.c_str()));
    file = btData.scriptNames.value(file, file);

    std::string ms(640, '\0');
    snprintf(&ms[0], ms.size(), "Script '%s' line %s: %s occured.\n\n%s",
             file.c_str(), line, RSTRING_PTR(name), RSTRING_PTR(msg));

    showMsg(ms);
}

