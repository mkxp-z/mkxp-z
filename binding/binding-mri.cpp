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
#include "util/ruby-source.h"

#include "config.h"

#include "binding-util.h"
#include "binding.h"

#include "sharedstate.h"
#include "patcher.h"
#include "eventthread.h"
#include "display/gl/glstate.h"

#include <vector>
#include <regex>
#include <algorithm>
#include <cstdio>
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#include "app_bridge.h"
#include "util/rapidcsv.h"
#include "script-bootstrap.h"

extern "C" {
#include <ruby.h>

#if RAPI_FULL >= 190
#include <ruby/encoding.h>
#endif

/* Ruby 1.8 / 1.9 statically-linked extension init function.
 *
 * Init_ext() lives in our hand-rolled extinit.c (see
 * ios/Dependencies/ruby{18,19}/extinit.c) and calls ruby_init_ext()
 * for each bundled extension (zlib, stringio, etc), which both runs
 * the extension's Init_X and registers it with Ruby's `loaded`
 * features list so subsequent `require 'X'` is a no-op.
 *
 * Without this call, the extensions never initialize and game
 * scripts that reference Zlib::Inflate (or StringIO, etc.) fail
 * with NameError "uninitialized constant Zlib".
 *
 * Ruby 3.0+ doesn't need this: its `ruby_setup`/`ruby_options`
 * path calls Init_ext automatically. Ruby 1.8 / 1.9 only call it
 * from `require_libraries`, which fires only when -r command-line
 * options are present - we don't go through that path. */
#if RAPI_FULL < 200
extern "C" void Init_ext(void);
#endif

/* Ruby 1.8 statically-linked extension init functions */
#if RAPI_FULL <= 187
void Init_zlib(void);
void Init_stringio(void);
void Init_strscan(void);
void Init_thread(void);
void Init_digest(void);
void Init_fcntl(void);

/* Ruby 1.8 GC stack base; defined in gc.c. */
extern VALUE *rb_gc_stack_start;
/* Caches gc.c's stack-growth direction on first use. Not declared in
 * any Ruby header, but it is a plain external symbol. */
int rb_stack_growup_p(volatile VALUE *addr);
#endif
}

#include <assert.h>
#include <pthread.h>
#include <string>
#include <zlib.h>

#include <SDL_cpuinfo.h>
#include <SDL_filesystem.h>
#include <SDL_loadso.h>
#include <SDL_messagebox.h>
#include <SDL_power.h>

static void mriBindingExecute();
static void mriBindingTerminate();
static void mriBindingReset();

ScriptBinding scriptBindingImpl = {mriBindingExecute, mriBindingTerminate,
    mriBindingReset};

ScriptBinding *scriptBinding = &scriptBindingImpl;

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

void httpBindingInit();
void cryptoBindingInit();

void hmode7BindingInit();

RB_METHOD(mkxpDelta);
RB_METHOD(mriPrint);
RB_METHOD(mriP);
RB_METHOD(mkxpKernelPrint);
RB_METHOD(mkxpDataDirectory);
RB_METHOD(mkxpSharedFontsPath);
RB_METHOD(mkxpSetTitle);
RB_METHOD(mkxpGetTitle);
RB_METHOD(mkxpDesensitize);
RB_METHOD(mkxpPuts);
RB_METHOD(mkxpReportSaveLoad);
RB_METHOD(mkxpReportSaveRepaired);

RB_METHOD(mkxpPlatform);
RB_METHOD(mkxpIsMacHost);
RB_METHOD(mkxpIsWindowsHost);
RB_METHOD(mkxpIsLinuxHost);
RB_METHOD(mkxpIsIOSHost);
RB_METHOD(mkxpIsUsingRosetta);
RB_METHOD(mkxpIsUsingWine);
RB_METHOD(mkxpIsReallyMacHost);
RB_METHOD(mkxpIsReallyLinuxHost);
RB_METHOD(mkxpIsReallyWindowsHost);

RB_METHOD(mkxpCheatsEnabled);
RB_METHOD(mkxpUseInGameKeyboard);
RB_METHOD(mkxpJoiplayCompat);
RB_METHOD(mkxpSystemDebugLogEnabled);
RB_METHOD(mkxpNetworkEnabled);
RB_METHOD(mkxpManagedConfigDir);
RB_METHOD(mkxpSyntaxTransformTarget);
RB_METHOD(mkxpApplyOverrides);
RB_METHOD(mkxpRpgVersion);
RB_METHOD(mkxpRubyVersion);

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
RB_METHOD(mkxpResolveCasePath);
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
RB_METHOD(mkxpStringAref);
RB_METHOD(mkxpStringAset);

VALUE json2rb(json5pp::value const &v);
json5pp::value rb2json(VALUE v);

RB_METHOD(mkxpParseCSV);

#ifdef MKXPZ_HAVE_SYNTAX_TRANSFORM_PATCHES
#if RAPI_MAJOR > 1 || (RAPI_MAJOR == 1 && RAPI_MINOR > 8)
/* True when the run's syntax transform target can satisfy a gate at
 * major.minor. The target is a whole-run setting, so a stub that fails this
 * test can only raise. Module#method_defined? still reports it as present, and
 * a game that guards its own copy with that idiom then keeps the dead one:
 *
 *   if !Array.method_defined?(:nitems)
 *     class Array
 *       def nitems
 *         count { |x| !x.nil? }
 *       end
 *     end
 *   end
 *
 * Pokemon Rejuvenation writes that, and every Poke Ball throw failed. */
static bool mkxp_legacy_stub_can_run(unsigned int major, unsigned int minor) {
    extern unsigned int mkxp_syntax_transform_target_ruby_version_major,
                        mkxp_syntax_transform_target_ruby_version_minor;
    if (mkxp_syntax_transform_target_ruby_version_major != major)
        return mkxp_syntax_transform_target_ruby_version_major < major;
    return mkxp_syntax_transform_target_ruby_version_minor <= minor;
}

static VALUE legacy_array_choice(int argc, VALUE *argv, VALUE self) {
    if (!mkxp_ec_is_syntax_transform_active(1, 8, -1))
        mkxp_raise_no_method_exception(self, "choice");
    return rb_funcallv(self, rb_intern("sample"), argc, argv);
}

static VALUE legacy_array_nitems(VALUE self) {
    if (!mkxp_ec_is_syntax_transform_active(1, 8, -1))
        mkxp_raise_no_method_exception(self, "nitems");
    return rb_funcall(rb_funcall(self, rb_intern("compact"), 0),
                      rb_intern("length"), 0);
}

static VALUE legacy_array_indexes(int argc, VALUE *argv, VALUE self) {
    if (!mkxp_ec_is_syntax_transform_active(1, 8, -1))
        mkxp_raise_no_method_exception(self, "indexes");
    rb_warn("Array#indexes is deprecated; use Array#values_at");
    return rb_funcallv(self, rb_intern("values_at"), argc, argv);
}

static VALUE legacy_array_indices(int argc, VALUE *argv, VALUE self) {
    if (!mkxp_ec_is_syntax_transform_active(1, 8, -1))
        mkxp_raise_no_method_exception(self, "indices");
    rb_warn("Array#indices is deprecated; use Array#values_at");
    return rb_funcallv(self, rb_intern("values_at"), argc, argv);
}

static VALUE legacy_hash_indexes(int argc, VALUE *argv, VALUE self) {
    if (!mkxp_ec_is_syntax_transform_active(1, 8, -1))
        mkxp_raise_no_method_exception(self, "indexes");
    rb_warn("Hash#indexes is deprecated; use Hash#values_at");
    return rb_funcallv(self, rb_intern("values_at"), argc, argv);
}

static VALUE legacy_hash_indices(int argc, VALUE *argv, VALUE self) {
    if (!mkxp_ec_is_syntax_transform_active(1, 8, -1))
        mkxp_raise_no_method_exception(self, "indices");
    rb_warn("Hash#indices is deprecated; use Hash#values_at");
    return rb_funcallv(self, rb_intern("values_at"), argc, argv);
}

static VALUE legacy_kernel_id(VALUE self) {
    if (!mkxp_ec_is_syntax_transform_active(1, 8, -1))
        mkxp_raise_no_method_exception(self, "id");
    rb_warn("Object#id will be deprecated; use Object#object_id");
    return rb_obj_id(self);
}

static VALUE legacy_kernel_type(VALUE self) {
    if (!mkxp_ec_is_syntax_transform_active(1, 8, -1))
        mkxp_raise_no_method_exception(self, "type");
    rb_warn("Object#type is deprecated; use Object#class");
    return rb_obj_class(self);
}

static VALUE legacy_symbol_to_i(VALUE self) {
    if (!mkxp_ec_is_syntax_transform_active(1, 8, -1))
        mkxp_raise_no_method_exception(self, "to_i");
    return RB_LONG2FIX(rb_sym2id(self));
}

static VALUE legacy_symbol_to_int(VALUE self) {
    if (!mkxp_ec_is_syntax_transform_active(1, 8, -1))
        mkxp_raise_no_method_exception(self, "to_int");
    rb_warning("treating Symbol as an integer");
    return RB_LONG2FIX(rb_sym2id(self));
}
#endif // RAPI_MAJOR > 1 || (RAPI_MAJOR == 1 && RAPI_MINOR > 8)

#if RAPI_MAJOR > 2 || (RAPI_MAJOR == 2 && RAPI_MINOR > 7)
static VALUE legacy_hash_index(int argc, VALUE *argv, VALUE self) {
    if (!mkxp_ec_is_syntax_transform_active(2, 7, -1))
        mkxp_raise_no_method_exception(self, "index");
    rb_warn("Hash#index is deprecated; use Hash#key instead");
    return rb_funcallv(self, rb_intern("key"), argc, argv);
}
#endif // RAPI_MAJOR > 2 || (RAPI_MAJOR == 2 && RAPI_MINOR > 7)

#if RAPI_MAJOR > 3 || (RAPI_MAJOR == 3 && RAPI_MINOR > 1)
static VALUE legacy_dir_exists(VALUE self, VALUE path) {
    if (!mkxp_ec_is_syntax_transform_active(3, 1, -1))
        mkxp_raise_no_method_exception(self, "exists?");
    return rb_funcall(rb_cDir, rb_intern("exist?"), 1, path);
}

static VALUE legacy_file_exists(VALUE self, VALUE path) {
    if (!mkxp_ec_is_syntax_transform_active(3, 1, -1))
        mkxp_raise_no_method_exception(self, "exists?");
    return rb_funcall(rb_cFile, rb_intern("exist?"), 1, path);
}

static VALUE legacy_file_test_exists(VALUE self, VALUE path) {
    if (!mkxp_ec_is_syntax_transform_active(3, 1, -1))
        mkxp_raise_no_method_exception(self, "exists?");
    return rb_funcall(rb_mFileTest, rb_intern("exist?"), 1, path);
}

static VALUE legacy_kernel_match(VALUE self, VALUE other) {
    if (mkxp_ec_is_syntax_transform_active(1, 8, -1))
        return Qfalse;
    else if (!mkxp_ec_is_syntax_transform_active(3, 1, -1))
        mkxp_raise_no_method_exception(self, "=~");
    return Qnil;
}
#endif // RAPI_MAJOR > 3 || (RAPI_MAJOR == 3 && RAPI_MINOR > 1)
#endif // MKXPZ_HAVE_SYNTAX_TRANSFORM_PATCHES

/* Idempotent wrapper around rb_define_alias. Defensive: callers
 * may run the same alias-then-redefine pattern more than once if
 * the engine ever loads them through a path that re-evaluates
 * the binding init. Skip the alias if it's already defined. */
static void mkxp_define_alias_once(VALUE klass, const char *aliasName,
                                   const char *origName) {
    if (rb_method_boundp(klass, rb_intern(aliasName), /*ex=*/0)) return;
    rb_define_alias(klass, aliasName, origName);
}

#ifdef MKXPZ_HAVE_SYNTAX_TRANSFORM_PATCHES
/* --- Honest respond_to? for the legacy stubs ----------------------
 *
 * The legacy stubs above stay defined on the core classes for every
 * script, but they raise NoMethodError unless the calling script runs
 * under the older syntax mode. Stock respond_to? only reports the
 * definition, so it answers true for a method that cannot run. Pokemon
 * Essentials forks feature-test with exactly that idiom:
 *
 *   item[0] = item[0].id if item[0].respond_to?(:id)
 *
 * Kernel#id makes the guard pass on a Symbol, the call then raises,
 * and the game dies (Pokemon Anil, save load screen). Answer false for
 * a stub the current script cannot call, so the guard skips it.
 */
struct MkxpLegacyStub {
    VALUE recv;    /* class or module that holds the stub */
    ID name;
    bool singleton; /* true = the stub is a singleton method on recv */
    int major;     /* version gate the stub tests before it runs */
    int minor;
};

static MkxpLegacyStub mkxp_legacy_stubs[32];
static size_t mkxp_legacy_stub_count = 0;
static ID mkxp_legacy_respond_to_orig = 0;

static void mkxp_register_legacy_stub(VALUE recv, const char *name,
                                      bool singleton, int major, int minor) {
    if (mkxp_legacy_stub_count >= ARRAY_SIZE(mkxp_legacy_stubs)) return;
    MkxpLegacyStub &stub = mkxp_legacy_stubs[mkxp_legacy_stub_count++];
    stub.recv = recv;
    stub.name = rb_intern(name);
    stub.singleton = singleton;
    stub.major = major;
    stub.minor = minor;
}

/* The stub is a C method, so it has no source location. A game that
 * defines its own version of the name in Ruby gets one; that method is
 * real and must keep the true answer. */
static bool mkxp_legacy_stub_still_c(VALUE self, ID name) {
    VALUE method = rb_funcall(self, rb_intern("method"), 1, ID2SYM(name));
    return NIL_P(rb_funcall(method, rb_intern("source_location"), 0));
}

/* True when `name` on `self` resolves to a stub that would raise. */
static bool mkxp_legacy_stub_is_dead(VALUE self, ID name) {
    for (size_t i = 0; i < mkxp_legacy_stub_count; ++i) {
        const MkxpLegacyStub &stub = mkxp_legacy_stubs[i];
        if (stub.name != name) continue;
        if (stub.singleton) {
            if (self != stub.recv) continue;
        } else if (!RTEST(rb_obj_is_kind_of(self, stub.recv))) {
            continue;
        }
        if (mkxp_ec_is_syntax_transform_active(stub.major, stub.minor, -1))
            return false;
        return mkxp_legacy_stub_still_c(self, name);
    }
    return false;
}

static VALUE mkxp_legacy_respond_to(int argc, VALUE *argv, VALUE self) {
    VALUE name = Qnil, includeAll = Qfalse;
    rb_scan_args(argc, argv, "11", &name, &includeAll);
    ID id = rb_check_id(&name);
    if (id && mkxp_legacy_stub_is_dead(self, id)) return Qfalse;
    return rb_funcallv(self, mkxp_legacy_respond_to_orig, argc, argv);
}

static void mkxp_install_legacy_respond_to() {
    if (mkxp_legacy_stub_count == 0) return;
    mkxp_define_alias_once(rb_mKernel, "_mkxp_respond_to_p", "respond_to?");
    mkxp_legacy_respond_to_orig = rb_intern("_mkxp_respond_to_p");
    rb_define_method(rb_mKernel, "respond_to?", mkxp_legacy_respond_to, -1);
}
#endif // MKXPZ_HAVE_SYNTAX_TRANSFORM_PATCHES

static void mriBindingInit() {
#ifdef MKXPZ_HAVE_SYNTAX_TRANSFORM_PATCHES
#if RAPI_MAJOR > 1 || (RAPI_MAJOR == 1 && RAPI_MINOR > 8)
    if (mkxp_legacy_stub_can_run(1, 8)) {
        rb_define_method(rb_cArray, "choice", legacy_array_choice, -1);
        rb_define_method(rb_cArray, "nitems", legacy_array_nitems, 0);
        rb_define_method(rb_cArray, "indexes", legacy_array_indexes, -1);
        rb_define_method(rb_cArray, "indices", legacy_array_indices, -1);
        rb_define_method(rb_cHash, "indexes", legacy_hash_indexes, -1);
        rb_define_method(rb_cHash, "indices", legacy_hash_indices, -1);
        rb_define_method(rb_mKernel, "id", legacy_kernel_id, 0);
        rb_define_method(rb_mKernel, "type", legacy_kernel_type, 0);
        rb_define_method(rb_cSymbol, "to_i", legacy_symbol_to_i, 0);
        rb_define_method(rb_cSymbol, "to_int", legacy_symbol_to_int, 0);
        mkxp_register_legacy_stub(rb_cArray, "choice", false, 1, 8);
        mkxp_register_legacy_stub(rb_cArray, "nitems", false, 1, 8);
        mkxp_register_legacy_stub(rb_cArray, "indexes", false, 1, 8);
        mkxp_register_legacy_stub(rb_cArray, "indices", false, 1, 8);
        mkxp_register_legacy_stub(rb_cHash, "indexes", false, 1, 8);
        mkxp_register_legacy_stub(rb_cHash, "indices", false, 1, 8);
        mkxp_register_legacy_stub(rb_mKernel, "id", false, 1, 8);
        mkxp_register_legacy_stub(rb_mKernel, "type", false, 1, 8);
        mkxp_register_legacy_stub(rb_cSymbol, "to_i", false, 1, 8);
        mkxp_register_legacy_stub(rb_cSymbol, "to_int", false, 1, 8);
    }
#endif // RAPI_MAJOR > 1 || (RAPI_MAJOR == 1 && RAPI_MINOR > 8)
#if RAPI_MAJOR > 2 || (RAPI_MAJOR == 2 && RAPI_MINOR > 7)
    if (mkxp_legacy_stub_can_run(2, 7)) {
        rb_define_method(rb_cHash, "index", legacy_hash_index, -1);
        mkxp_register_legacy_stub(rb_cHash, "index", false, 2, 7);
    }
#endif // RAPI_MAJOR > 2 || (RAPI_MAJOR == 2 && RAPI_MINOR > 7)
#if RAPI_MAJOR > 3 || (RAPI_MAJOR == 3 && RAPI_MINOR > 1)
    if (mkxp_legacy_stub_can_run(3, 1)) {
        rb_define_singleton_method(rb_cDir, "exists?", legacy_dir_exists, 1);
        rb_define_singleton_method(rb_cFile, "exists?", legacy_file_exists, 1);
        rb_define_module_function(rb_mFileTest, "exists?", legacy_file_test_exists, 1);
        rb_define_method(rb_mKernel, "=~", legacy_kernel_match, 1);
        mkxp_register_legacy_stub(rb_cDir, "exists?", true, 3, 1);
        mkxp_register_legacy_stub(rb_cFile, "exists?", true, 3, 1);
        mkxp_register_legacy_stub(rb_mFileTest, "exists?", true, 3, 1);
        /* Kernel#=~ stays out of the table. Stock Ruby keeps Object#=~
         * through 3.1, so a false answer would be the wrong one. */
    }
#endif // RAPI_MAJOR > 3 || (RAPI_MAJOR == 3 && RAPI_MINOR > 1)
    mkxp_install_legacy_respond_to();
#endif // MKXPZ_HAVE_SYNTAX_TRANSFORM_PATCHES

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

    httpBindingInit();
    cryptoBindingInit();

    /* H-Mode7 native binding. Depends on bitmapBindingInit +
     * tableBindingInit having run; registers the HM7::Native
     * module that the postload shim forwards HM7.self.xxx
     * method calls to. See binding/hmode7-binding.cpp. */
    hmode7BindingInit();
    
    if (rgssVer >= 3) {
        _rb_define_module_function(rb_mKernel, "rgss_main", mriRgssMain);
        _rb_define_module_function(rb_mKernel, "rgss_stop", mriRgssStop);
        
        rb_define_global_const("RGSS_VERSION", mkxp_str_new_cstr("3.0.1"));
    } else {
        // print -> debug log (NOT a dialog; see mkxpKernelPrint).
        // p stays bound to mriP -> showMessageBox; that's a debug
        // method games rarely use in production and a dialog is
        // a reasonable surface for it.
        _rb_define_module_function(rb_mKernel, "print", mkxpKernelPrint);
        _rb_define_module_function(rb_mKernel, "p", mriP);
        
        mkxp_define_alias_once(rb_singleton_class(rb_mKernel),
                               "_mkxp_kernel_caller_alias", "caller");
        _rb_define_module_function(rb_mKernel, "caller", _kernelCaller);
    }

    // PE 20+ fangames call msgbox even when detected as RGSS1
    // (Scripts.rxdata). Bind on every RGSS version so boot notices
    // and pbCriticalCode error dialogs work.
    _rb_define_module_function(rb_mKernel, "msgbox", mriPrint);
    _rb_define_module_function(rb_mKernel, "msgbox_p", mriP);
    
    mkxp::ScriptBootstrap::loadRpgModule(rgssVer);
    
    VALUE mod = rb_define_module("System");
    _rb_define_module_function(mod, "delta", mkxpDelta);
    _rb_define_module_function(mod, "uptime", mkxpDelta);
    _rb_define_module_function(mod, "data_directory", mkxpDataDirectory);
    _rb_define_module_function(mod, "shared_fonts_path", mkxpSharedFontsPath);
    _rb_define_module_function(mod, "set_window_title", mkxpSetTitle);
    _rb_define_module_function(mod, "window_title", mkxpGetTitle);
    _rb_define_module_function(mod, "window_title=", mkxpSetTitle);
    _rb_define_module_function(mod, "show_settings", mkxpSettingsMenu);
    _rb_define_module_function(mod, "puts", mkxpPuts);
    _rb_define_module_function(mod, "report_save_load", mkxpReportSaveLoad);
    _rb_define_module_function(mod, "report_save_repaired", mkxpReportSaveRepaired);
    _rb_define_module_function(mod, "desensitize", mkxpDesensitize);
    _rb_define_module_function(mod, "platform", mkxpPlatform);
    
    _rb_define_module_function(mod, "is_mac?", mkxpIsMacHost);
    _rb_define_module_function(mod, "is_rosetta?", mkxpIsUsingRosetta);

    _rb_define_module_function(mod, "is_linux?", mkxpIsLinuxHost);

    _rb_define_module_function(mod, "is_windows?", mkxpIsWindowsHost);
    _rb_define_module_function(mod, "is_ios?", mkxpIsIOSHost);
    _rb_define_module_function(mod, "is_wine?", mkxpIsUsingWine);
    _rb_define_module_function(mod, "is_really_mac?", mkxpIsReallyMacHost);
    _rb_define_module_function(mod, "is_really_linux?", mkxpIsReallyLinuxHost);
    _rb_define_module_function(mod, "is_really_windows?", mkxpIsReallyWindowsHost);

    _rb_define_module_function(mod, "cheats_enabled?", mkxpCheatsEnabled);
    _rb_define_module_function(mod, "use_in_game_keyboard?", mkxpUseInGameKeyboard);
    _rb_define_module_function(mod, "joiplay_compat?", mkxpJoiplayCompat);
    _rb_define_module_function(mod, "debug_log?", mkxpSystemDebugLogEnabled);
    _rb_define_module_function(mod, "network_enabled?", mkxpNetworkEnabled);
    _rb_define_module_function(mod, "managed_config_dir", mkxpManagedConfigDir);
    _rb_define_module_function(mod, "apply_overrides", mkxpApplyOverrides);
    _rb_define_module_function(mod, "rpg_version", mkxpRpgVersion);
    _rb_define_module_function(mod, "ruby_version", mkxpRubyVersion);
    _rb_define_module_function(mod, "syntax_transform_target", mkxpSyntaxTransformTarget);
    
    
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
    _rb_define_module_function(mod, "resolve_case_path", mkxpResolveCasePath);
    _rb_define_module_function(mod, "launch", mkxpLaunch);
    
    _rb_define_module_function(mod, "default_font_family=", mkxpSetDefaultFontFamily);
    
    _rb_define_module_function(mod, "parse_csv", mkxpParseCSV);
    
    _rb_define_method(rb_cString, "to_utf8", mkxpStringToUTF8);
    _rb_define_method(rb_cString, "to_utf8!", mkxpStringToUTF8Bang);
    
    /* Ruby 1.8 String#[] / String#[]= compatibility (C-level for $~ safety) */
#if RAPI_FULL > 187
    mkxp_define_alias_once(rb_cString, "_mkxp_c_aref",    "[]");
    mkxp_define_alias_once(rb_cString, "_mkxp_c_aset",    "[]=");
    mkxp_define_alias_once(rb_cString, "_mkxp_c_getbyte", "getbyte");
    rb_define_method(rb_cString, "[]",  RUBY_METHOD_FUNC(mkxpStringAref), -1);
    rb_define_method(rb_cString, "[]=", RUBY_METHOD_FUNC(mkxpStringAset), -1);
#endif
    
    VALUE cmod = rb_define_module("CFG");
    _rb_define_module_function(cmod, "[]", mkxpGetJSONSetting);
    _rb_define_module_function(cmod, "[]=", mkxpSetJSONSetting);
    _rb_define_module_function(cmod, "to_hash", mkxpGetAllJSONSettings);
    
    /* Load global constants */
    rb_gv_set("MKXP", Qtrue);
    rb_gv_set("MKXP_MAX_TEX_SIZE", INT2NUM(glState.caps.realMaxTexSize));

#if RAPI_FULL < 200
    /* Give $DEBUG storage of its own on Ruby 1.8 and 1.9.
     *
     * Those two bind $DEBUG to the interpreter's own `ruby_debug`,
     * which changes how strict Ruby is. sprintf.c then raises
     * "too many arguments for format string" for a call that passes
     * a spare argument, eval.c prints every exception, and a thread
     * that raises kills the process. RGSS games use $DEBUG as a game
     * flag for debug menus, and Pokemon Essentials passes a spare
     * argument in many sprintf calls, so a game that turns $DEBUG on
     * dies at the first one (issue #67, the "Logros" script of
     * Pokemon Z). Ruby 3.1 in this engine leaves sprintf alone, so
     * the same game runs there.
     *
     * Point $DEBUG and its alias $-d at our own value. Game code
     * reads and writes it as before, and `ruby_debug` stays false.
     *
     * A host that starts a second VM in one process needs the
     * reset below. The static still holds the object the last game
     * assigned, and the new VM would mark that dead address. */
    static VALUE legacyDebugFlag = Qfalse;
    legacyDebugFlag = Qfalse;
    rb_define_variable("$DEBUG", &legacyDebugFlag);
    rb_define_variable("$-d", &legacyDebugFlag);
#endif

    VALUE debug = rb_bool_new(shState->config().editor.debug);
    if (rgssVer == 1)
        rb_gv_set("DEBUG", debug);
    else if (rgssVer >= 2)
        rb_gv_set("TEST", debug);
    
    rb_gv_set("BTEST", rb_bool_new(shState->config().editor.battleTest));
    
    std::string version = std::string(MKXPZ_VERSION "/") + getPlistValue("GIT_COMMIT_HASH");
    VALUE vers = mkxp_str_new_cstr(version.c_str());
    rb_str_freeze(vers);
    rb_define_const(mod, "VERSION", vers);
    
    // Automatically load zlib if it's present -- the correct way this time
    int state;
    rb_eval_string_protect("require('zlib') if !defined?(Zlib)", &state);
    if (state) {
        Debug() << "Could not load Zlib. If this is important, make sure Ruby was built with static extensions, or that"
        << ((MKXPZ_PLATFORM == MKXPZ_PLATFORM_MACOS) ? "zlib.bundle" : "zlib.so")
        << "is present and reachable by Ruby's loadpath.";
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
            rb_str_buf_cat2(dispString, sep);
    }
    
    /* Deliberate game dialog (msgbox / p), not an error: the
     * INFORMATION flag routes it to the host's info channel so
     * the alert doesn't carry "restart the app" framing. */
    shState->eThread().showMessageBox(RSTRING_PTR(dispString),
                                      SDL_MESSAGEBOX_INFORMATION);
}


RB_METHOD_GUARD(mriPrint) {
    RB_UNUSED_PARAM;
    
    printP(argc, argv, "to_s", "");
    
    shState->checkShutdown();
    shState->checkReset();
    
    return Qnil;
}
RB_METHOD_GUARD_END

/* Kernel#print routed to the debug log instead of `showMessageBox`.
 *
 * mkxp historically bound `Kernel#print` to a Win32-style modal
 * message box, which on iOS surfaces in the host as the same alert
 * we use for engine errors ("Something went wrong"). Some games
 * call `print` for benign informational notices (Pokemon Infinite
 * Fusion's PokemonLoadScreen#copyKeybindings is the trigger we
 * found, but the pattern is general) and the resulting alert
 * forces the user to dismiss + restart the game even though
 * nothing has gone wrong.
 *
 * Routing `print` to `mkxp_debugLog` matches Ruby's mainline
 * convention (Kernel#print writes to stdout, no UI). Games that
 * explicitly want a dialog can call `msgbox` instead, which
 * stays bound to the original `mriPrint`/`showMessageBox` path.
 *
 * Exception: Pokemon Essentials' pbPrintException ends with
 * `print`, not `msgbox`. Detect that banner and route it through
 * the host error dialog so handled script failures are visible.
 *
 * Same arg semantics as Ruby's Kernel#print: each arg is converted
 * via `to_s`, joined without separator, no trailing newline.
 */
/* Last Kernel#print payload and the Graphics frame count at the time
 * it was printed. Boot-gate scripts (e.g. Pokemon Reborn's
 * ScriptLoader refusing to run the Windows build outside desktop)
 * print their reason and immediately call `exit`; with print routed
 * to the debug log the user would only see the generic "game has
 * ended" alert. When the session ends cleanly and no frame was
 * rendered since the last print, that print is the game's parting
 * message - surface it through the host error dialog. */
static std::string s_lastKernelPrintText;
static int s_lastKernelPrintFrame = -1;

static bool mkxpLooksLikePEExceptionPrint(const char *text) {
    if (!text || !text[0])
        return false;
    if (!strstr(text, "Exception:"))
        return false;
    return strstr(text, "Backtrace:") ||
           strstr(text, "Pokémon Essentials version") ||
           strstr(text, "Pokemon Essentials version");
}

RB_METHOD_GUARD(mkxpKernelPrint) {
    RB_UNUSED_PARAM;
    
    VALUE buf = rb_str_buf_new(128);
    ID conv = rb_intern("to_s");
    for (int i = 0; i < argc; ++i) {
        VALUE str = rb_funcall2(argv[i], conv, 0, NULL);
        rb_str_buf_append(buf, str);
    }
    
    const char *text = RSTRING_PTR(buf);
    Debug() << text;
    mkxp_debugLog("SCRIPT", "Kernel.print [Ruby]", text);
    s_lastKernelPrintText = text;
    s_lastKernelPrintFrame = shState->graphics().getFrameCount();
    if (mkxpLooksLikePEExceptionPrint(text))
        shState->eThread().showMessageBox(text);
    
    shState->checkShutdown();
    shState->checkReset();
    
    return Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(mriP) {
    RB_UNUSED_PARAM;
    
    printP(argc, argv, "inspect", "\n");
    
    shState->checkShutdown();
    shState->checkReset();
    
    return Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD(mkxpDelta) {
    RB_UNUSED_PARAM;
    return rb_float_new(shState->runTime());
}

/* System.data_directory - per-game writable directory. On iOS the
   host points this at a per-game container so saves and any
   companion app-data files are visible in the Files app and stay
   inside the imported game container.

   Always ends with '/' (same contract as SDL_GetPrefPath). Pokemon
   Essentials and other PE forks concatenate with string + rather than
   File.join (`System.data_directory + "Game.rxdata"`); without the
   separator that becomes `.../UserDataGame.rxdata` at the container
   root. Foundation's URLByStandardizingPath strips trailing slashes,
   so we re-add one after normalize. */
RB_METHOD(mkxpDataDirectory) {
    RB_UNUSED_PARAM;
    
    const std::string &path = shState->config().customDataPath;
    const char *s = path.empty() ? "." : path.c_str();
    
    std::string s_nml = shState->fileSystem().normalize(s, 1, 1);
    if (s_nml.empty() || s_nml == ".")
        return mkxp_str_new_cstr("./");
    if (s_nml.back() != '/')
        s_nml += '/';
    
    return mkxp_str_new_cstr(s_nml.c_str());
}

/* System.shared_fonts_path - host-wide shared font pool, or nil
   when the host declares none. Trailing '/' for the same string +
   concat contract as System.data_directory. The preload layer
   routes Windows-style "<SystemRoot>\\Fonts\\<file>" writes (the
   stock Essentials FontInstaller) here, so installed fonts serve
   every game the way the Windows system font folder does. */
RB_METHOD(mkxpSharedFontsPath) {
    RB_UNUSED_PARAM;

    const std::string &path = shState->config().sharedFontsPath;
    if (path.empty())
        return Qnil;

    std::string s_nml = shState->fileSystem().normalize(path.c_str(), 1, 1);
    if (s_nml.empty())
        return Qnil;
    if (s_nml.back() != '/')
        s_nml += '/';

    return mkxp_str_new_cstr(s_nml.c_str());
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
    
    return mkxp_str_new_cstr(SDL_GetWindowTitle(shState->sdlWindow()));
}

RB_METHOD(mkxpDesensitize) {
    RB_UNUSED_PARAM;
    
    VALUE filename;
    rb_scan_args(argc, argv, "1", &filename);
    SafeStringValue(filename);
    
    return mkxp_str_new_cstr(shState->fileSystem().desensitize(RSTRING_PTR(filename)));
}

RB_METHOD(mkxpPuts) {
    RB_UNUSED_PARAM;
    
    const char *str;
    rb_get_args(argc, argv, "z", &str RB_ARG_END);
    
    Debug() << str;
    // Mirror to the debug log so game scripts can trace behavior that
    // lives inside big RGSS scripts (like Main). Useful when tracking
    // down hangs.
    mkxp_debugLog("SCRIPT", "System.puts [Ruby]", str);
    
    return Qnil;
}

RB_METHOD(mkxpReportSaveLoad) {
    RB_UNUSED_PARAM;

    const char *name;
    rb_get_args(argc, argv, "z", &name RB_ARG_END);
    mkxp_reportSaveEvent(MKXP_SAVE_LOAD, name);
    return Qnil;
}

RB_METHOD(mkxpReportSaveRepaired) {
    RB_UNUSED_PARAM;

    const char *detail;
    rb_get_args(argc, argv, "z", &detail RB_ARG_END);
    mkxp_reportSaveEvent(MKXP_SAVE_REPAIRED, detail);
    return Qnil;
}

/* Platform methods exposed to Ruby scripts. This fork runs exclusively
 * on iOS / iPadOS / tvOS, so all legacy desktop predicates return false
 * and the host string is hardcoded. Methods are preserved (rather than
 * removed) so existing game scripts that call `System::is_mac?` etc.
 * continue to load without raising NoMethodError; they take
 * their "unknown platform" fallback path. `System::is_ios?` is
 * provided as the new positive check. */
RB_METHOD(mkxpPlatform) {
    RB_UNUSED_PARAM;
    return mkxp_str_new_cstr("iOS");
}

RB_METHOD(mkxpIsMacHost) {
    RB_UNUSED_PARAM;
    return rb_bool_new(false);
}

RB_METHOD(mkxpIsUsingRosetta) {
    RB_UNUSED_PARAM;
    return rb_bool_new(false);
}

RB_METHOD(mkxpIsLinuxHost) {
    RB_UNUSED_PARAM;
    return rb_bool_new(false);
}

RB_METHOD(mkxpIsWindowsHost) {
    RB_UNUSED_PARAM;
    return rb_bool_new(false);
}

RB_METHOD(mkxpIsIOSHost) {
    RB_UNUSED_PARAM;
    return rb_bool_new(true);
}

RB_METHOD(mkxpIsUsingWine) {
    RB_UNUSED_PARAM;
    return rb_bool_new(false);
}

RB_METHOD(mkxpIsReallyMacHost) {
    RB_UNUSED_PARAM;
    return rb_bool_new(false);
}

RB_METHOD(mkxpIsReallyLinuxHost) {
    RB_UNUSED_PARAM;
    return rb_bool_new(false);
}

RB_METHOD(mkxpIsReallyWindowsHost) {
    RB_UNUSED_PARAM;
    return rb_bool_new(false);
}

RB_METHOD(mkxpCheatsEnabled) {
    RB_UNUSED_PARAM;
    return rb_bool_new(mkxp_getCheatsEnabled());
}

/* MKXP.use_in_game_keyboard? - true when the host requested that
   Pokemon Essentials text-entry scenes use the game's own
   keyboard scene instead of the iOS soft keyboard. Read by
   `pokemon_input.rb` to decide whether to force-disable
   `USEKEYBOARDTEXTENTRY`. */
RB_METHOD(mkxpUseInGameKeyboard) {
    RB_UNUSED_PARAM;
    return rb_bool_new(mkxp_getUseInGameKeyboard());
}

/* System.joiplay_compat? - true when the host wants game scripts to
   see `$joiplay = true` and take their JoiPlay-specific code paths.
   Read by `platform_compat.rb` before game scripts load. */
RB_METHOD(mkxpJoiplayCompat) {
    RB_UNUSED_PARAM;
    return rb_bool_new(mkxp_getJoiplayCompat());
}

/* System.network_enabled? - true when the host allows this game to
   reach the network. When false the preload layer keeps up the
   offline illusion (network stdlib requires are swallowed, HTTP
   download shims fake success, online-feature stubs stay active). */
RB_METHOD(mkxpNetworkEnabled) {
    RB_UNUSED_PARAM;
    return rb_bool_new(mkxp_getNetworkEnabled());
}

/* MKXP.managed_config_dir - host-managed per-game state directory
   (see `mkxp_setManagedConfigDir`). Postload scripts use this
   to drop runtime-detection marker files that survive across
   sessions, e.g. `pokemon_input.rb` writes a
   `.pokemon_essentials_detected` marker so the next launch's
   GameSettings UI defaults the In-game keyboard toggle ON for
   PE games whose scripts live inside an rgssad archive (where
   filesystem-only detection from the host can't see PE
   signatures). Empty string when the host hasn't configured the
   bridge (desktop / non-iOS builds). */
RB_METHOD(mkxpManagedConfigDir) {
    RB_UNUSED_PARAM;
    const char *dir = mkxp_getManagedConfigDir();
    return rb_utf8_str_new_cstr(dir ? dir : "");
}

/* System.apply_overrides(str) - runs the JoiPlay-compatible
   script text-patcher over `str` and returns the rewritten text.
   Reborn and other PE fangames call MKXP.apply_overrides on each
   script section before Kernel#eval so config-driven fixes
   (from the `scriptPatches` mkxp.json field) can ship as data. When
   no patches are loaded this is effectively `str.dup`. */
RB_METHOD(mkxpApplyOverrides) {
    VALUE arg;
    rb_scan_args(argc, argv, "01", &arg);

    if (NIL_P(arg) || !RB_TYPE_P(arg, T_STRING))
        return arg;

    std::string data(RSTRING_PTR(arg), RSTRING_LEN(arg));
    shState->patcher().apply(data);
    VALUE ret = rb_str_new(data.data(), (long)data.size());
#if RAPI_FULL >= 190
    /* Keep the caller's string encoding. rb_str_new tags the result
       ASCII-8BIT; games that round-trip their script sources through
       apply_overrides before eval (Reborn-style ScriptLoader) would
       then eval binary-encoded source, turning every string literal
       binary — and per-character text drawing splits multi-byte
       UTF-8 glyphs into replacement boxes. */
    rb_enc_associate_index(ret, rb_enc_get_index(arg));
#endif
    return ret;
}

/* System.rpg_version - integer RGSS version (1 = XP, 2 = VX, 3 = VX
   Ace, 0 = unknown). JoiPlay's postload.rb branches on this to
   decide whether VX-Ace-only compat patches apply; some fangames
   feature-detect via MKXP.rpg_version too. */
RB_METHOD(mkxpRpgVersion) {
    RB_UNUSED_PARAM;
    int v = shState->rtData().config.rgssVersion;
    if (v < 1 || v > 3) v = 0;
    return INT2NUM(v);
}

/* System.ruby_version - "MAJOR.MINOR" string of the embedded MRI. A
   handful of plugins branch on `MKXP.ruby_version.to_f < 2.0` to
   decide between 1.8 and 1.9+ syntax. */
RB_METHOD(mkxpRubyVersion) {
    RB_UNUSED_PARAM;
    std::string v = std::to_string(RUBY_API_VERSION_MAJOR) + "." +
                    std::to_string(RUBY_API_VERSION_MINOR);
    return rb_str_new_cstr(v.c_str());
}

/* System.syntax_transform_target -> [major, minor, teeny,
   ec_active_at_1_8]. UINT_MAX in any slot = disabled. The fourth
   element matches the same check String#[] uses internally. */
#ifdef MKXPZ_HAVE_SYNTAX_TRANSFORM_PATCHES
RB_METHOD(mkxpSyntaxTransformTarget) {
    RB_UNUSED_PARAM;
    extern unsigned int mkxp_syntax_transform_target_ruby_version_major,
                        mkxp_syntax_transform_target_ruby_version_minor,
                        mkxp_syntax_transform_target_ruby_version_teeny;
    VALUE ary = rb_ary_new_capa(4);
    rb_ary_push(ary, UINT2NUM(mkxp_syntax_transform_target_ruby_version_major));
    rb_ary_push(ary, UINT2NUM(mkxp_syntax_transform_target_ruby_version_minor));
    rb_ary_push(ary, UINT2NUM(mkxp_syntax_transform_target_ruby_version_teeny));
    rb_ary_push(ary, mkxp_ec_is_syntax_transform_active(1, 8, (unsigned int)-1) ? Qtrue : Qfalse);
    return ary;
}
#else
RB_METHOD(mkxpSyntaxTransformTarget) {
    RB_UNUSED_PARAM;
    VALUE ary = rb_ary_new_capa(4);
    rb_ary_push(ary, Qnil);
    rb_ary_push(ary, Qnil);
    rb_ary_push(ary, Qnil);
    rb_ary_push(ary, Qfalse);
    return ary;
}
#endif

RB_METHOD(mkxpUserLanguage) {
    RB_UNUSED_PARAM;
    
    return mkxp_str_new_cstr(mkxp_sys::getSystemLanguage().c_str());
}

RB_METHOD(mkxpUserName) {
    RB_UNUSED_PARAM;
    
    return mkxp_str_new_cstr(mkxp_sys::getUserName().c_str());
}

RB_METHOD(mkxpGameTitle) {
    RB_UNUSED_PARAM;
    
    return mkxp_str_new_cstr(shState->config().game.title.c_str());
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

RB_METHOD_GUARD(mkxpReloadPathCache) {
    RB_UNUSED_PARAM;
    
    shState->fileSystem().reloadPathCache();
    return Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(mkxpAddPath) {
    RB_UNUSED_PARAM;
    
    VALUE path, mountpoint, reload;
    rb_scan_args(argc, argv, "12", &path, &mountpoint, &reload);
    SafeStringValue(path);
    if (mountpoint != Qnil) SafeStringValue(mountpoint);
    
    const char *mp = (mountpoint == Qnil) ? 0 : RSTRING_PTR(mountpoint);
    
    bool rl = true;
    if (reload != Qnil)
        rb_bool_arg(reload, &rl);
    
    shState->fileSystem().addPath(RSTRING_PTR(path), mp, rl);
    
    return path;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(mkxpRemovePath) {
    RB_UNUSED_PARAM;
    
    VALUE path, reload;
    rb_scan_args(argc, argv, "11", &path, &reload);
    SafeStringValue(path);
    
    bool rl = true;
    if (reload != Qnil)
        rb_bool_arg(reload, &rl);
    
    shState->fileSystem().removePath(RSTRING_PTR(path), rl);
    
    return path;
}
RB_METHOD_GUARD_END

RB_METHOD(mkxpFileExists) {
    RB_UNUSED_PARAM;

    VALUE path;
    rb_scan_args(argc, argv, "1", &path);
    SafeStringValue(path);

    if (shState->fileSystem().exists(RSTRING_PTR(path)))
        return Qtrue;
    return Qfalse;
}

/* System.resolve_case_path(path) - resolves a game-relative path
   case-insensitively, through the path cache or, with the cache
   off, through live folder listings, and returns
   the actual on-disk spelling, or nil when nothing matches. Lets
   Ruby-side wrappers retry raw file APIs (File.open etc.) that
   fail on iOS's case-sensitive filesystem with a name whose case
   only matches on Windows. */
RB_METHOD(mkxpResolveCasePath) {
    RB_UNUSED_PARAM;

    VALUE path;
    rb_scan_args(argc, argv, "1", &path);
    SafeStringValue(path);

    std::string resolved = shState->fileSystem().resolvePath(RSTRING_PTR(path));
    if (resolved.empty())
        return Qnil;
    return rb_utf8_str_new(resolved.c_str(), resolved.size());
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

#if RAPI_FULL > 187
/*
 * C-level String#[] override for Ruby 1.8 compatibility.
 *
 * Ruby 1.8: str[integer] returned the byte value (Integer).
 * Ruby 3.x: str[integer] returns a one-character String.
 *
 * We MUST implement this at the C level (not Ruby) because String#[regexp]
 * sets $~ (Regexp.last_match) via rb_backref_set(), which only propagates
 * to the nearest RUBY frame on the call stack.  C (CFUNC) frames are
 * transparent to $~ propagation.  A Ruby-level wrapper would create a new
 * RUBY frame, trapping $~ inside the wrapper and making it invisible to
 * the caller; breaking patterns like:
 *     while text[/regexp/]
 *       $~.pre_match   # => nil:NilClass (NoMethodError) if wrapped in Ruby
 *
 * Syntax-transform gate: the integer-byte branch ONLY fires when the
 * syntax transform is targeting Ruby <= 1.8. Without this gate we'd
 * break Ruby-3 source that does `name[idx].match?(...)` and expects
 * a String (hit in Infinite Fusion's FusionSprites.rb:401 where
 * `sprite_name[spriteName.length]` expects a 1-char String). The
 * underlying `_mkxp_c_aref` alias points at the original `rb_str_aref_m`,
 * which has its own transform-aware patch - so for the ungated default
 * path we just delegate and let that patch do the right thing.
 */
RB_METHOD(mkxpStringAref) {
    RB_UNUSED_PARAM;

#ifdef MKXPZ_HAVE_SYNTAX_TRANSFORM_PATCHES
    /* Ruby 1.8 byte-return semantics only when the transform targets
     * Ruby <= 1.8 AND the single argument is an Integer. We call the
     * saved `_mkxp_c_getbyte` alias (not `rb_str_getbyte` directly)
     * to tolerate games that rebind `getbyte` themselves (e.g. Pokemon
     * Z's "Map - Klein"). */
    if (mkxp_ec_is_syntax_transform_active(1, 8, (unsigned int)-1) &&
        argc == 1 && RB_INTEGER_TYPE_P(argv[0])) {
        return rb_funcall(self, rb_intern("_mkxp_c_getbyte"), 1, argv[0]);
    }
#endif

    /* Everything else (Regexp, Range, String, [i,len], AND integer
     * args when the transform is disabled) → delegate to the original
     * `rb_str_aref_m` via the `_mkxp_c_aref` alias so $~ propagates
     * and the upstream transform-gated patch handles any 1.8 fallback. */
    return rb_funcallv(self, rb_intern("_mkxp_c_aref"), argc, argv);
}

/* C-level String#[]= override for Ruby 1.8 byte-write compatibility.
 * Ruby 1.8: str[int] = int  set the byte at that index.
 * Ruby 3.x: str[int] = val  expects a String replacement.
 *
 * Asymmetry vs mkxpStringAref: the READ case (`str[i]`) is
 * ambiguous - 1.8 code expects Integer, modern Ruby 3 code expects
 * String, and the argument types are identical, so we have to gate
 * on the syntax-transform target. The WRITE case (`str[i] = X`) is
 * UNAMBIGUOUS: if `X` is Integer, the caller is unmistakably 1.8
 * code (Ruby 3 code only ever does `str[i] = SomeString`). We
 * therefore keep the byte-write branch unconditional when `argv[1]`
 * is Integer - this preserves the legacy idiom used by our own
 * preload scripts (`win32_wrap.rb:237: dst[i] = b` where `b` comes
 * from `each_byte`) regardless of the per-game transform setting.
 *
 * Engine preload iseqs compile with `mkxp_syntax_transform_flag = 0`
 * (no transform-aware parser hook), so an `ec_active(1,8)`-style
 * gate would have returned FALSE inside our preloads on a modern-
 * Ruby game and failed the byte-write with `TypeError: no implicit
 * conversion of Integer into String` - the regression that hit
 * Insurgence / Z / Uranium after the matched gate was applied to
 * both aref and aset.
 */
RB_METHOD(mkxpStringAset) {
    RB_UNUSED_PARAM;

    /* str[int] = int → set byte (Ruby 1.8 semantics). Always.
     * Modern Ruby 3 code never reaches this branch because it
     * passes a String as the value. */
    if (argc == 2 && RB_INTEGER_TYPE_P(argv[0]) && RB_INTEGER_TYPE_P(argv[1])) {
        unsigned char byte = (unsigned char)(NUM2INT(argv[1]) & 0xFF);
        VALUE replacement = rb_str_new((const char *)&byte, 1);
        VALUE args[2] = { argv[0], replacement };
        return rb_funcallv(self, rb_intern("_mkxp_c_aset"), 2, args);
    }

    /* Everything else → original */
    return rb_funcallv(self, rb_intern("_mkxp_c_aset"), argc, argv);
}
#endif /* RAPI_FULL > 187 - String#[] overrides */

RB_METHOD_GUARD(mkxpStringToUTF8) {
    RB_UNUSED_PARAM;
    
    rb_check_argc(argc, 0);
    
    std::string ret(RSTRING_PTR(self), RSTRING_LEN(self));
    ret = Encoding::convertString(ret);
    
    return mkxp_str_new(ret.c_str(), ret.length());
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(mkxpStringToUTF8Bang) {
    RB_UNUSED_PARAM;
    
    rb_check_argc(argc, 0);
    
    std::string ret(RSTRING_PTR(self), RSTRING_LEN(self));
    ret = Encoding::convertString(ret);
    
    rb_str_resize(self, ret.length());
    memcpy(RSTRING_PTR(self), ret.c_str(), RSTRING_LEN(self));
    
#if RAPI_FULL >= 190
    rb_enc_set_index(self, rb_utf8_encindex());
#endif
    
    return self;
}
RB_METHOD_GUARD_END

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

RB_METHOD_GUARD(mkxpLaunch) {
    RB_UNUSED_PARAM;
    
    VALUE cmdname, args;
    
    rb_scan_args(argc, argv, "11", &cmdname, &args);
    SafeStringValue(cmdname);
    
    std::string command(OPENCMD);
    command += "\""; command += RSTRING_PTR(cmdname); command += "\"";
    
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
    
    // system() is unavailable on iOS
    throw Exception(Exception::MKXPError, "system() not available on iOS for \"%s\"", RSTRING_PTR(cmdname));
    
    return RUBY_Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(mkxpParseCSV) {
    RB_UNUSED_PARAM;
    
    VALUE str;
    rb_scan_args(argc, argv, "1", &str);
    SafeStringValue(str);
    
    VALUE ret = rb_ary_new();
    std::stringstream stream(RSTRING_PTR(str));
    try {
        rapidcsv::Document doc(stream, rapidcsv::LabelParams(-1,-1), rapidcsv::SeparatorParams(',', false, true, true, true));
        for (int r = 0; r < doc.GetRowCount(); r++) {
            VALUE col = rb_ary_new();
            for (int c = 0; c < doc.GetColumnCount(); c++) {
                std::string str = doc.GetCell<std::string>(c, r);
                rb_ary_push(col, mkxp_str_new(str.c_str(), str.length()));
            }
            rb_ary_push(ret, col);
        }
    } catch (std::exception &e) {
        throw Exception(Exception::MKXPError, "Failed to parse CSV: %s", e.what());
    }

    return ret;
}
RB_METHOD_GUARD_END

json5pp::value loadUserSettings() {
    json5pp::value ret;
    VALUE cpath = mkxp_str_new_cstr(shState->config().userConfPath.c_str());
    
    if (rb_funcall(rb_cFile, rb_intern("exist?"), 1, cpath) == Qtrue) {
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
    VALUE cpath = mkxp_str_new_cstr(shState->config().userConfPath.c_str());
    VALUE f = rb_funcall(rb_cFile, rb_intern("open"), 2, cpath, rb_str_new("w", 1));
    rb_funcall(f, rb_intern("write"), 1, mkxp_str_new_cstr(settings.stringify5(json5pp::rule::space_indent<>()).c_str()));
    rb_funcall(f, rb_intern("close"), 0);
}

RB_METHOD(mkxpGetJSONSetting) {
    RB_UNUSED_PARAM;
    
    VALUE sname;
    rb_scan_args(argc, argv, "1", &sname);
    SafeStringValue(sname);
    
    auto settings = loadUserSettings();
    auto &s = settings.as_object();
    
    if (s[RSTRING_PTR(sname)].is_null()) {
        return json2rb(shState->config().raw.as_object()[RSTRING_PTR(sname)]);
    }
    
    return json2rb(s[RSTRING_PTR(sname)]);
    
}

RB_METHOD_GUARD(mkxpSetJSONSetting) {
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
RB_METHOD_GUARD_END

RB_METHOD(mkxpGetAllJSONSettings) {
    RB_UNUSED_PARAM;
    
    return json2rb(shState->config().raw);
}

static VALUE rgssMainCb(VALUE block) {
    rb_funcall2(block, rb_intern("call"), 0, 0);
    return Qnil;
}

static VALUE rgssMainRescue(VALUE arg, VALUE exc) {
    VALUE *excRet = (VALUE *)arg;
    
    *excRet = exc;
    
    return Qnil;
}

static bool processReset(bool rubyExc) {
	const char *str = "Audio.__reset__; Graphics.__reset__;";
	
	if (rubyExc) {
		rb_eval_string(str);
	} else {
		int state;
		rb_eval_string_protect(str, &state);
		return state;
	}
	
	return 0;
}


RB_METHOD_GUARD(mriRgssMain) {
    RB_UNUSED_PARAM;

    mkxp::ScriptBootstrap::loadConfigPostloadScripts(shState->rtData().config);

    while (true) {
        VALUE exc = Qnil;
#if RAPI_FULL < 270
        rb_rescue2((VALUE(*)(ANYARGS))rgssMainCb, rb_block_proc(),
                   (VALUE(*)(ANYARGS))rgssMainRescue, (VALUE)&exc, rb_eException,
                   (VALUE)0);
#else
        rb_rescue2(rgssMainCb, rb_block_proc(), rgssMainRescue, (VALUE)&exc,
                   rb_eException, (VALUE)0);
#endif
        
        if (NIL_P(exc))
            break;
        
        if (rb_obj_class(exc) == getRbData()->exc[Reset])
            processReset(true);
        else
            rb_exc_raise(exc);
    }
    
    return Qnil;
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(mriRgssStop) {
    RB_UNUSED_PARAM;
    
    while (true)
        shState->graphics().update();
    
    return Qnil;
}
RB_METHOD_GUARD_END

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
    VALUE args[] = {mkxp_str_new_cstr(":in `<main>'"), mkxp_str_new_cstr("")};
    rb_funcall2(rb_ary_entry(trace, len - 1), rb_intern("gsub!"), 2, args);
    
    return trace;
}

VALUE kernelLoadDataInt(const char *filename, bool rubyExc, bool raw);

struct BacktraceData {
    /* Maps: Ruby visible filename, To: Actual script name */
    BoostHash<std::string, std::string> scriptNames;
};

bool evalScript(VALUE string, const char *filename)
{
    int state;
    mkxp::ScriptBootstrap::evalRubyString((void *)string, (void *)filename, &state);
    return state == 0;
}


#define SCRIPT_SECTION_FMT (rgssVer >= 3 ? "{%04ld}" : "Section%03ld")

// Declared in app_bridge.cpp - returns the debug log path set by the UI,
// or empty string if debug logging is disabled.
std::string mkxp_getDebugLogPath(void);
extern "C" void mkxp_debugLog(const char *tag, const char *source, const char *message);

static void logRubyError(const char *type, const char *detail) {
    mkxp_debugLog(type, "binding-mri.cpp [C++]", detail);
}

RB_METHOD(mkxpSystemDebugLogEnabled) {
    RB_UNUSED_PARAM;
    return rb_bool_new(mkxp_debugLogEnabled());
}

/* Counts script sections that ran to completion without error during
 * the current session. Reset at the top of runRMXPScripts and read
 * by mriBindingExecute to decide whether a no-pending-exception
 * shutdown should be classified as a clean exit. A session where
 * every script was skipped (e.g. all required missing native libs)
 * leaves this at 0 and the UI shows the recovery alert instead of
 * the "game has ended" message. */
static int s_scriptsExecutedThisSession = 0;

// Some games end with empty sections after Main (Hello Charlotte EP1).
// Main never returns, so postloads must run before the last section
// that has code, not only comments.
static long lastCodeSection(VALUE scriptArray) {
    long index = RARRAY_LEN(scriptArray) - 1;
    while (index > 0) {
        VALUE script = rb_ary_entry(scriptArray, index);
        if (RB_TYPE_P(script, RUBY_T_ARRAY)) {
            VALUE code = rb_ary_entry(script, 3);
            if (RB_TYPE_P(code, RUBY_T_STRING) &&
                rubySourceHasCode(RSTRING_PTR(code), RSTRING_LEN(code)))
                break;
        }
        --index;
    }
    return index;
}

static VALUE expandScriptFolderBody(VALUE args) {
    VALUE mod = rb_const_get(rb_cObject, rb_intern("MKXPScriptFolder"));
    return rb_funcall(mod, rb_intern("expand"), 2,
                      rb_ary_entry(args, 0), rb_ary_entry(args, 1));
}

// See scripts/preload/script_folder_split.rb. Returns nil when the
// section at mainIndex is not the Data/Scripts loader.
static VALUE expandScriptFolderLoader(VALUE scriptArray, long mainIndex) {
    if (!rb_const_defined(rb_cObject, rb_intern("MKXPScriptFolder")))
        return Qnil;

    int state = 0;
    VALUE expanded = rb_protect(expandScriptFolderBody,
                                rb_assoc_new(scriptArray, LONG2NUM(mainIndex)),
                                &state);
    if (state) {
        VALUE exc = rb_gv_get("$!");
        VALUE msg = exc == Qnil ? Qnil : rb_funcall(exc, rb_intern("message"), 0);
        char message[512];
        snprintf(message, sizeof(message),
                 "Data/Scripts loader split failed, the section runs whole: %s: %.*s",
                 exc == Qnil ? "unknown error" : rb_class2name(rb_obj_class(exc)),
                 RB_TYPE_P(msg, RUBY_T_STRING) ? (int)RSTRING_LEN(msg) : 0,
                 RB_TYPE_P(msg, RUBY_T_STRING) ? RSTRING_PTR(msg) : "");
        logRubyError("SCRIPTS", message);
        rb_set_errinfo(Qnil);
        return Qnil;
    }
    if (!RB_TYPE_P(expanded, RUBY_T_ARRAY))
        return Qnil;

    char message[128];
    snprintf(message, sizeof(message),
             "split the Data/Scripts loader into %ld sections",
             RARRAY_LEN(expanded) - RARRAY_LEN(scriptArray) + 1);
    mkxp_debugLog("SCRIPTS", "binding-mri.cpp [C++]", message);
    return expanded;
}

static void runRMXPScripts(BacktraceData &btData) {
    const Config &conf = shState->rtData().config;
    const std::string &scriptPack = conf.game.scripts;

    s_scriptsExecutedThisSession = 0;

    if (scriptPack.empty()) {
        showMsg("No script file has been specified. Check the game's INI and try again.");
        return;
    }
    
    if (!shState->fileSystem().exists(scriptPack.c_str())) {
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
        
        /* Most modern Pokemon Essentials forks ship valid UTF-8
         * script sources and expect ordinary string literals to stay
         * UTF-8 at runtime. Only force binary parsing for the small
         * subset of legacy scripts that actually contains byte-range
         * regex escapes like `\x7f-\x9f`, which Ruby 3.1 otherwise
         * rejects with "invalid multibyte escape". Blanket-tagging
         * every script as ASCII-8BIT fixes those regexes but turns
         * normal text literals (item names, UI strings) binary too,
         * which later explodes in `_INTL`, battle text, debug logs,
         * etc. */
        {
            const char *src = decodeBuffer.c_str();
            size_t srcLen = bufferLen;
            bool hasEncoding = false;
            bool needsBinaryEncoding = false;
            if (srcLen >= 11 && (strncmp(src, "# encoding:", 11) == 0 ||
                                  strncmp(src, "#encoding:", 10) == 0 ||
                                  strncmp(src, "# coding:", 9) == 0 ||
                                  strncmp(src, "#coding:", 8) == 0)) {
                hasEncoding = true;
            }
            if (!hasEncoding) {
                static const char *binaryRegexMarkers[] = {
                    "\\x7f-\\x9f",
                    "\\x7F-\\x9F",
                    "\\x80-\\x9f",
                    "\\x80-\\x9F",
                    "\\x80-\\xFF",
                    "\\x81-\\x9f",
                    "\\x81-\\x9F",
                    nullptr
                };
                for (const char **marker = binaryRegexMarkers; *marker; ++marker) {
                    if (strstr(src, *marker) != nullptr) {
                        needsBinaryEncoding = true;
                        break;
                    }
                }
            }
            if (!hasEncoding && needsBinaryEncoding) {
                std::string prefixed = "# encoding: ASCII-8BIT\n";
                prefixed.append(src, srcLen);
                rb_ary_store(script, 3, mkxp_str_new(prefixed.c_str(), prefixed.size()));
            } else {
                rb_ary_store(script, 3, mkxp_str_new(src, srcLen));
            }
        }
    }
    
    
    mkxp::ScriptBootstrap::loadEnginePreloads();
    mkxp::ScriptBootstrap::loadConfigPreloadScripts(conf);
    
    VALUE exc = rb_gv_get("$!");
    if (exc != Qnil)
        return;

    long mainIndex = lastCodeSection(scriptArray);
    VALUE expanded = expandScriptFolderLoader(scriptArray, mainIndex);
    if (expanded != Qnil) {
        // $RGSS_SCRIPTS keeps the packed sections: the Essentials
        // translation tool inflates each one's [2].
        // No Ruby object holds the split array. Without a root, the
        // GC can free it while the postloads run.
        static VALUE splitSections = Qnil;
        static bool splitSectionsRooted = false;
        if (!splitSectionsRooted) {
            rb_gc_register_address(&splitSections);
            splitSectionsRooted = true;
        }
        splitSections = expanded;
        scriptArray = expanded;
        scriptCount = RARRAY_LEN(scriptArray);
        mainIndex = lastCodeSection(scriptArray);
    }
    
    while (true) {
        for (long i = 0; i < scriptCount; ++i) {
            if (shState->rtData().rqTerm)
                break;
            
            if (i == mainIndex) {
                mkxp::ScriptBootstrap::loadEnginePostloadsBeforeMain();
                mkxp::ScriptBootstrap::loadCheatPostload();
            }
            
            VALUE script = rb_ary_entry(scriptArray, i);
            VALUE scriptDecoded = rb_ary_entry(script, 3);
            // Apply JoiPlay-style script text patches from
            // config.scriptPatches before handing text to the
            // Ruby VM. The patcher is a no-op when the patch
            // list is empty, so the hot path cost is one
            // std::string copy per script section.
            std::string rawScript(RSTRING_PTR(scriptDecoded),
                                  RSTRING_LEN(scriptDecoded));
            shState->patcher().apply(rawScript);
            VALUE string = mkxp_str_new(rawScript.data(), rawScript.size());
            
            VALUE fname;
            const char *scriptName = RSTRING_PTR(rb_ary_entry(script, 1));
            char buf[512];
            int len;
            
            if (conf.useScriptNames)
                len = snprintf(buf, sizeof(buf), "%03ld:%s", i, scriptName);
            else
                len = snprintf(buf, sizeof(buf), SCRIPT_SECTION_FMT, i);
            
            fname = mkxp_str_new(buf, len);
            btData.scriptNames.insert(buf, scriptName);
            
            
            // Per-platform script gating (|W|/|M|/|L| name prefix,
            // |!W| for negation) is unimplemented; the original block
            // sat here commented out and was removed.

            int state;
            bool wasSkipped = false;

            // Per-script trace. Useful when a game hangs inside a script:
            // the last TRACE line points at the culprit.
            {
                char trace[600];
                snprintf(trace, sizeof(trace), "enter %03ld %s", i,
                         scriptName[0] ? scriptName : "(unnamed)");
                mkxp_debugLog("TRACE", "binding-mri.cpp [C++]", trace);
            }

            {
#ifdef MKXPZ_HAVE_SYNTAX_TRANSFORM_PATCHES
                extern __thread int mkxp_syntax_transform_next_eval;
                struct SyntaxTransformGuard {
                    SyntaxTransformGuard() {
                        mkxp_syntax_transform_next_eval = 1;
                    }
                    ~SyntaxTransformGuard() {
                        mkxp_syntax_transform_next_eval = 0;
                    }
                };
                SyntaxTransformGuard guard;
#endif // MKXPZ_HAVE_SYNTAX_TRANSFORM_PATCHES
                mkxp::ScriptBootstrap::evalRubyString((void *)string, (void *)fname, &state);
            }

            /* On iOS, native DLL/library loading (LoadError) and missing
             * native methods (NoMethodError from DLL-provided extensions)
             * are expected to fail. Many game scripts have optional native
             * extensions (e.g. RGSS Linker, F-mod, screenshot DLLs).
             * Skip these errors and continue to the next script section -
             * the game typically has fallback code paths. */
            if (state) {
                VALUE exc = rb_gv_get("$!");
                if (exc != Qnil) {
                bool shouldSkip = false;
                if (rb_obj_is_kind_of(exc, rb_eLoadError) ||
                    rb_obj_is_kind_of(exc, rb_eSyntaxError)) {
                    shouldSkip = true;
                } else if (rb_obj_is_kind_of(exc, rb_eNoMethodError)) {
                    /* Only skip NoMethodError from missing DLL/native methods.
                     * These are on Module/Class receivers (e.g. Kernel:Module).
                     * Game logic errors (nil:NilClass, false:FalseClass, or
                     * any object instance) must NOT be skipped. */
                    VALUE msg = rb_funcall(exc, rb_intern("message"), 0);
                    const char *msgStr = StringValueCStr(msg);
                    shouldSkip = strstr(msgStr, ":Module") ||
                                 strstr(msgStr, ":Class");
                } else if (rb_obj_is_kind_of(exc, rb_eTypeError)) {
                    /* Skip "superclass mismatch for class X" errors.
                     * Common in community PE plugins that redefine a
                     * Window_* class against a different parent than
                     * the running PE version uses. The original
                     * hierarchy survives, so the game's stock UI still
                     * works; only the plugin's bonus features get
                     * dropped. Other TypeErrors (real bugs) stay fatal. */
                    VALUE msg = rb_funcall(exc, rb_intern("message"), 0);
                    const char *msgStr = StringValueCStr(msg);
                    shouldSkip = strstr(msgStr, "superclass mismatch") != NULL;
                }
                if (shouldSkip) {
                    VALUE msg = rb_funcall(exc, rb_intern("message"), 0);
                    Debug() << "Skipping" << rb_class2name(rb_obj_class(exc))
                            << "in script" << scriptName
                            << ":" << StringValueCStr(msg);
                    {
                        char buf[512];
                        snprintf(buf, sizeof(buf), "Script '%s': %s - %s",
                                 scriptName, rb_class2name(rb_obj_class(exc)),
                                 StringValueCStr(msg));
                        logRubyError("SKIPPED", buf);
                    }
                    rb_set_errinfo(Qnil);
                    state = 0;
                    wasSkipped = true;
                }
                }
            }
            if (state)
                break;

            if (!wasSkipped)
                s_scriptsExecutedThisSession++;

            {
                char trace[600];
                snprintf(trace, sizeof(trace), "exit  %03ld %s", i,
                         scriptName[0] ? scriptName : "(unnamed)");
                mkxp_debugLog("TRACE", "binding-mri.cpp [C++]", trace);
            }
        }
        
        VALUE exc = rb_gv_get("$!");
        if (rb_obj_class(exc) != getRbData()->exc[Reset])
            break;
        
        if (processReset(false))
            break;
    }
}

/* The error-display path runs after script execution, outside every
 * protective tag. A raise there has no handler to unwind to: the VM
 * hits a null tag, calls rb_bug and aborts the whole process. Every
 * Ruby call on this path must therefore go through rb_protect. */
struct ProtectedCallArgs {
    VALUE recv;
    ID mid;
    int argc;
    const VALUE *argv;
};

static VALUE protectedCallBody(VALUE arg) {
    const ProtectedCallArgs *a = (const ProtectedCallArgs *)arg;
    return rb_funcall2(a->recv, a->mid, a->argc, a->argv);
}

/* Call a method and swallow any exception it raises; returns Qnil
 * when the callee raised. */
static VALUE protectedCall(VALUE recv, ID mid, int argc, const VALUE *argv) {
    ProtectedCallArgs args = {recv, mid, argc, argv};
    int state = 0;
    VALUE ret = rb_protect(protectedCallBody, (VALUE)&args, &state);
    if (state) {
        rb_set_errinfo(Qnil);
        return Qnil;
    }
    return ret;
}

/* Dump the raise ring buffer (scripts/preload/raise_trace.rb) to
 * the debug log. Runs once when script execution ends. The buffer
 * holds exceptions that game code rescued and dropped; on a masked
 * boot failure these lines are the only evidence. Callers must
 * capture and restore $! themselves - the drain call goes through
 * rb_protect and clears errinfo on failure. */
static void drainRaiseTrace() {
    if (!mkxp_debugLogEnabled())
        return;
    if (!rb_const_defined(rb_cObject, rb_intern("MKXPRaiseTrace")))
        return;

    VALUE mod = rb_const_get(rb_cObject, rb_intern("MKXPRaiseTrace"));
    VALUE lines = protectedCall(mod, rb_intern("drain"), 0, NULL);
    if (!RB_TYPE_P(lines, T_ARRAY))
        return;

    long count = RARRAY_LEN(lines);
    for (long i = 0; i < count; ++i) {
        VALUE line = rb_ary_entry(lines, i);
        if (RB_TYPE_P(line, T_STRING))
            mkxp_debugLog("RAISETRACE", "binding-mri.cpp [C++]",
                          RSTRING_PTR(line));
    }
}

/* Convert a value to a display string without raising; a failed or
 * non-string #to_s yields the fallback instead. */
static std::string safeToStdString(VALUE v, const char *fallback) {
    if (NIL_P(v))
        return fallback;
    VALUE s = RB_TYPE_P(v, T_STRING)
                  ? v
                  : protectedCall(v, rb_intern("to_s"), 0, NULL);
    if (NIL_P(s) || !RB_TYPE_P(s, T_STRING))
        return fallback;
    return std::string(RSTRING_PTR(s), RSTRING_LEN(s));
}

// Best-effort human-readable detail for a Ruby exception. Pokemon
// Essentials' EventScriptError stores its formatted script error in
// @event_message and passes nil to Exception#initialize, so .message
// is always empty even though the real text exists.
static VALUE exceptionDetailMessage(VALUE exc) {
    VALUE em = rb_iv_get(exc, "@event_message");
    if (!NIL_P(em) && RB_TYPE_P(em, T_STRING) && RSTRING_LEN(em) > 0)
        return em;

    if (rb_respond_to(exc, rb_intern("event_message"))) {
        em = protectedCall(exc, rb_intern("event_message"), 0, NULL);
        if (!NIL_P(em) && RB_TYPE_P(em, T_STRING) && RSTRING_LEN(em) > 0)
            return em;
    }

    // Pokemon Essentials defines pbGetExceptionMessage at the top
    // level, which makes it a private method of Object.
    ID getMessage = rb_intern("pbGetExceptionMessage");
    if (rb_method_boundp(rb_cObject, getMessage, 0)) {
        VALUE detail = protectedCall(rb_cObject, getMessage, 1, &exc);
        if (RB_TYPE_P(detail, T_STRING) && RSTRING_LEN(detail) > 0)
            return detail;
    }

    /* Exception#full_message takes keyword arguments only; a
     * positional argument raises ArgumentError. Build the call in
     * Ruby so the keywords pass through every supported API. */
    if (rb_respond_to(exc, rb_intern("full_message"))) {
        int state = 0;
        rb_gv_set("$__mkxp_exc", exc);
        VALUE fm = rb_eval_string_protect(
            "begin; $__mkxp_exc.full_message(highlight: false, order: :top); "
            "ensure; $__mkxp_exc = nil; end",
            &state);
        if (state)
            rb_set_errinfo(Qnil);
        else if (RB_TYPE_P(fm, T_STRING) && RSTRING_LEN(fm) > 0)
            return fm;
    }

    VALUE msg = protectedCall(exc, rb_intern("message"), 0, NULL);
    if (!NIL_P(msg) && RB_TYPE_P(msg, T_STRING) && RSTRING_LEN(msg) > 0)
        return msg;
    return protectedCall(exc, rb_intern("to_s"), 0, NULL);
}

static VALUE classPathBody(VALUE exc) {
    return rb_class_path(rb_obj_class(exc));
}

/* Class path of the exception without raising; Qnil on failure.
 * rb_class_path allocates and can raise on an anonymous class. */
static VALUE protectedClassPath(VALUE exc) {
    int state = 0;
    VALUE name = rb_protect(classPathBody, exc, &state);
    if (state) {
        rb_set_errinfo(Qnil);
        return Qnil;
    }
    return name;
}

static void showExc(VALUE exc, const BacktraceData &btData) {
    VALUE bt = protectedCall(exc, rb_intern("backtrace"), 0, NULL);
    if (!RB_TYPE_P(bt, T_ARRAY))
        bt = Qnil;
    VALUE msg = exceptionDetailMessage(exc);
    VALUE bt0 = NIL_P(bt) ? Qnil : rb_ary_entry(bt, 0);
    VALUE name = protectedClassPath(exc);

    /* Build the report from plain C++ strings: rb_sprintf and
     * StringValueCStr can raise, and a raise here kills the app. */
    std::string bt0Str = safeToStdString(bt0, "(unknown):?");
    std::string excStr = safeToStdString(exc, "(no message)");
    std::string nameStr = safeToStdString(name, "Error");
    std::string msgStr =
        safeToStdString(msg, "(could not retrieve the error message)");

    std::string ds = bt0Str + ": " + excStr + " (" + nameStr + ")";
    /* omit "useless" last entry (from ruby:1:in `eval') */
    if (!NIL_P(bt))
        for (long i = 1, btlen = RARRAY_LEN(bt) - 1; i < btlen; ++i)
            ds += "\n\tfrom " + safeToStdString(rb_ary_entry(bt, i), "(unknown)");
    Debug() << ds;
    // Also send the full backtrace to the per-game debug log so we can
    // inspect it post-crash without attaching a debugger.
    mkxp_debugLog("BACKTRACE", "binding-mri.cpp [C++]", ds.c_str());

    /* bt0 format: "SectionXXX:YY: in 'blabla'" - the file is the part
     * before the second-to-last colon, the line sits between the two. */
    std::string file = "(unknown)";
    std::string line = "?";
    size_t e = bt0Str.rfind(':');
    if (e != std::string::npos && e > 0) {
        size_t p = bt0Str.rfind(':', e - 1);
        if (p != std::string::npos) {
            file = bt0Str.substr(0, p);
            line = bt0Str.substr(p + 1, e - p - 1);
        } else {
            file = bt0Str.substr(0, e);
            line = bt0Str.substr(e + 1);
        }
    }
    file = btData.scriptNames.value(file, file);

    std::string ms = std::string("Script '") + file + "' line " + line + ": " +
                     nameStr + " occurred.\n\n" + msgStr;

    logRubyError("FATAL", ms.c_str());
    showMsg(ms);
}

struct ShowExcArgs {
    VALUE exc;
    const BacktraceData *btData;
};

static VALUE showExcBody(VALUE arg) {
    const ShowExcArgs *a = (const ShowExcArgs *)arg;
    showExc(a->exc, *a->btData);
    return Qnil;
}

/* Backstop for the whole display path: if showExc still raises, show
 * a plain alert instead of letting the raise abort the process. */
static void showExcSafe(VALUE exc, const BacktraceData &btData) {
    ShowExcArgs args = {exc, &btData};
    int state = 0;
    rb_protect(showExcBody, (VALUE)&args, &state);
    if (!state)
        return;
    rb_set_errinfo(Qnil);
    std::string nameStr = safeToStdString(protectedClassPath(exc), "unknown");
    std::string ms = std::string("Script error (") + nameStr +
                     "). The error details could not be formatted.";
    logRubyError("FATAL", ms.c_str());
    showMsg(ms);
}


/* Single-shot game session: ruby_init + script execute + return.
 *
 * Called once per app process by `getActiveScriptBinding()->execute()`
 * in main.cpp's rgssThreadFun. Once this returns, the engine is dead
 * for the remainder of the process — App Store guideline 2.5.1 stops
 * us from terminating ourselves, so the user has to close + reopen
 * the app to play another game. Ruby's VM has process-global state
 * (signal handlers, atexit, parser, symbol table) that doesn't
 * unwind cleanly via ruby_cleanup, and a second ruby_init in the
 * same process would crash anyway. */
static void mriBindingExecute() {
    Config &conf = shState->rtData().config;

    /* Point Ruby's openssl ext at the host-provided CA bundle before
     * the VM boots; the baked-in openssldir from the cross-compile
     * doesn't exist on device, so without this every TLS handshake
     * fails verification even with a valid server cert. */
    {
        const char *caPath = mkxp_getCABundlePath();
        if (caPath && caPath[0])
            setenv("SSL_CERT_FILE", caPath, 1);
    }

#if RAPI_MAJOR >= 2
    /* Normally only a ruby executable would do a sysinit,
     * but not doing it will lead to crashes due to closed
     * stdio streams on some platforms (eg. Windows) */
    {
    int argc = 0;
    char **argv = 0;
    ruby_sysinit(&argc, &argv);
    }

    RUBY_INIT_STACK;
    ruby_init();
    
    std::vector<const char*> rubyArgsC{"mkxp-z"};
#if RAPI_FULL >= 190
    rubyArgsC.push_back(mkxpUsingRuby18Encoding() ? "-EASCII-8BIT" : "-EUTF-8");
#endif // RAPI_FULL >= 190
    rubyArgsC.push_back("-e ");
    void *node;
    if (conf.jit.enabled) {
#if RAPI_FULL >= 310
        // Ruby v3.1.0 renamed the --jit options to --mjit.
        std::string verboseLevel("--mjit-verbose=");
        std::string maxCache("--mjit-max-cache=");
        std::string minCalls("--mjit-min-calls=");
        rubyArgsC.push_back("--mjit");
#else
        std::string verboseLevel("--jit-verbose=");
        std::string maxCache("--jit-max-cache=");
        std::string minCalls("--jit-min-calls=");
        rubyArgsC.push_back("--jit");
#endif
        verboseLevel += std::to_string(conf.jit.verboseLevel);
        maxCache += std::to_string(conf.jit.maxCache);
        minCalls += std::to_string(conf.jit.minCalls);

        rubyArgsC.push_back(verboseLevel.c_str());
        rubyArgsC.push_back(maxCache.c_str());
        rubyArgsC.push_back(minCalls.c_str());
        node = ruby_options(rubyArgsC.size(), const_cast<char**>(rubyArgsC.data()));
    } else if (conf.yjit.enabled) {
        rubyArgsC.push_back("--yjit");
        // TODO: Maybe support --yjit-exec-mem-size, --yjit-call-threshold
        node = ruby_options(rubyArgsC.size(), const_cast<char**>(rubyArgsC.data()));
    } else {
        node = ruby_options(rubyArgsC.size(), const_cast<char**>(rubyArgsC.data()));
    }
    
    int state;
    bool valid = ruby_executable_node(node, &state);
    if (valid)
        state = ruby_exec_node(node);
    if (state || !valid) {
        // The message is formatted for and automatically spits
        // out to the terminal, so let's leave it that way for now
        /*
         VALUE exc = rb_errinfo();
         #if RAPI_FULL >= 250
         VALUE msg = rb_funcall(exc, rb_intern("full_message"), 0);
         #else
         VALUE msg = rb_funcall(exc, rb_intern("message"), 0);
         #endif
         */
        showMsg("An error occurred while initializing Ruby. (Invalid JIT settings?)");
        ruby_cleanup(state);
        shState->rtData().rqTermAck.set();
        return;
    }
#else
#if RAPI_FULL <= 187
    /* Settle Ruby 1.8's stack-growth direction from a clean call site
     * BEFORE ruby_init, because the first caller wins and caches it.
     *
     * This build configures STACK_GROW_DIRECTION as 0 (config.h), so
     * gc.c decides at runtime by comparing the address of a local in
     * stack_grow_direction() against an address from its caller. When
     * that comparison is made from an inlined context the two
     * addresses share a frame, their order is arbitrary, and arm64
     * can be read as growing upwards.
     *
     * The length ruby_stack_length() returns stays correct either way
     * (its STACK_LENGTH is a runtime conditional), but the position
     * it reports does not:
     *
     *   *p = STACK_UPPER(STACK_END, rb_gc_stack_start, STACK_END);
     *
     * Read as upward, that hands back the stack BASE instead of the
     * current SP. rb_thread_save_context then memcpys from the base
     * for the length of the live stack - upwards, off the top of the
     * thread stack and into the next thread's guard page. The result
     * is SIGBUS inside rb_bug and an abort(): no Ruby exception, so
     * nothing in the engine or the raise tracer can report it. Every
     * 1.8 game that calls Thread.new is exposed; Xenoverse does it to
     * relaunch itself after installing fonts (issue #91).
     *
     * Calling through the external rb_stack_growup_p keeps the two
     * addresses in genuinely separate frames: `probe` belongs to this
     * function, and gc.c's own local sits below it. The direction is
     * cached from that comparison, so the real one is correct. */
    {
        volatile VALUE probe = Qnil;
        int growsUp = rb_stack_growup_p(&probe);
        char detail[128];
        snprintf(detail, sizeof(detail),
                 "ruby1.8 stack direction: %s",
                 growsUp ? "UP (unexpected on arm64)" : "down");
        mkxp_debugLog("INFO", "binding-mri.cpp [C++]", detail);
    }
#endif

    ruby_init();

#if RAPI_FULL <= 187
    /* Pin Ruby 1.8's machine-stack base to this frame.
     *
     * 1.8 stores one base (`rb_gc_stack_start`) and derives a LENGTH
     * from it every time it snapshots the stack for a green thread
     * (eval.c, rb_thread_save_context):
     *
     *   len = ruby_stack_length(&pos);         // base - current SP
     *   MEMCPY(th->stk_ptr, pos, VALUE, len);  // copies [SP, base)
     *
     * A base above the rgss thread's stack makes that copy run past
     * the top into the next thread's guard page. The result is a
     * SIGBUS inside rb_bug, then abort(): no Ruby exception, so the
     * engine never sees it and the process dies with no error UI.
     * Every 1.8 game that calls Thread.new is exposed - Xenoverse
     * does it to relaunch the game after installing fonts.
     *
     * ruby_init() seeds the base from its own frame, and
     * ruby_init_stack (gc.c) only ever RAISES it: it keeps the
     * highest address it has ever seen, so a base that drifts too
     * high cannot be corrected through the public API. Assign it
     * directly instead. This frame is the outermost one that runs
     * Ruby, so every later frame sits below the anchor. The 2.0+
     * path above gets the same guarantee from RUBY_INIT_STACK.
     *
     * Hardening only: the observed crash came from the direction
     * above, not from a bad base. The log line records both against
     * the real stack bounds. */
    {
        /* The frame address, not a local: it sits above every local
         * in this frame, so the conservative GC still scans them. */
        VALUE *seeded = rb_gc_stack_start;
        rb_gc_stack_start = (VALUE *)__builtin_frame_address(0);

        /* Report both bases against the real stack bounds so a debug
         * log shows whether the seeded value was out of range. */
        char *stackTop = (char *)pthread_get_stackaddr_np(pthread_self());
        size_t stackSize = pthread_get_stacksize_np(pthread_self());
        char detail[256];
        snprintf(detail, sizeof(detail),
                 "ruby1.8 stack base %p -> %p (rgss stack %p..%p)",
                 (void *)seeded, (void *)rb_gc_stack_start,
                 (void *)(stackTop - stackSize), (void *)stackTop);
        mkxp_debugLog("INFO", "binding-mri.cpp [C++]", detail);
    }
#endif
#endif

    /* Ruby 1.8 / 1.9: explicitly initialize statically-linked
     * extensions. Must run after ruby_init() (so Ruby's globals
     * exist) but before any game script runs. See declaration
     * above for full rationale.
     *
     * Ruby 3.0+ initializes extensions automatically via its
     * setup path; this block is no-op-compiled out there. */
#if RAPI_FULL < 200
    Init_ext();
#endif

#if RAPI_FULL < 200
    /* RPG Maker XP and VX run Ruby 1.8.1, and VX Ace runs Ruby 1.9.2.
     * These Rubies are 1.8.8 and 1.9.3. Zeus Video Player takes any
     * version other than 1.8.1 for VX Ace and calls
     * String#force_encoding, which Ruby 1.8 does not have. Grim's
     * console script finds VX Ace only by RUBY_VERSION == "1.9.2". */
    {
#if RAPI_FULL <= 187
        VALUE rgssVersion = rb_obj_freeze(rb_str_new2("1.8.1"));
        rb_const_remove(rb_cObject, rb_intern("VERSION"));
        rb_define_global_const("VERSION", rgssVersion);
#else
        VALUE rgssVersion = rb_obj_freeze(rb_usascii_str_new_cstr("1.9.2"));
#endif
        rb_const_remove(rb_cObject, rb_intern("RUBY_VERSION"));
        rb_define_global_const("RUBY_VERSION", rgssVersion);
    }
#endif

    // Set the default encoding for regular expressions to UTF-8 when using syntax transform targeting Ruby <= 1.8
    rb_gv_set("$KCODE", rb_str_new_cstr("UTF8"));
    rb_gv_set("$-K", rb_str_new_cstr("UTF8"));

    mkxp::ScriptBootstrap::setEvalReceiver(
        (void *)(rgssVer == 1 ? Qnil : rb_eval_string("self")));

    /* RbData must be live before mriBindingInit: inputBindingInit on
     * RGSS3 writes getRbData()->buttoncodeHash. */
    RbData rbData;
    shState->setBindingData(&rbData);
    BacktraceData btData;

    mriBindingInit();

#if RAPI_FULL > 187
    VALUE rbArgv = rb_get_argv();
#else
    VALUE rbArgv = rb_argv;
#endif
    for (const auto &str : conf.launchArgs)
        rb_ary_push(rbArgv, mkxp_str_new_cstr(str.c_str()));
    
    // Duplicates get pushed for some reason
    rb_funcall(rbArgv, rb_intern("uniq!"), 0);
    
    VALUE lpaths = rb_gv_get(":");
    rb_ary_clear(lpaths);
    
#ifdef MKXPZ_BUILD_XCODE
    /* Host-bundled pure-Ruby stdlib (net/http, uri, openssl.rb, ...).
     * Directory names follow each Ruby's own rubylibdir convention
     * so the trees can be copied verbatim from a Ruby checkout.
     * Always on the load path, network toggle included: with the
     * toggle off the device behaves like airplane mode (libraries
     * load, connections fail), not like a build without the
     * libraries. */
    {
        std::string resPath = mkxp_fs::getResourcePath();
#if RAPI_MAJOR >= 2
        resPath += "/Ruby/" + std::to_string(RAPI_MAJOR) + "." + std::to_string(RAPI_MINOR) + ".0";
#elif RAPI_FULL >= 190
        resPath += "/Ruby/1.9.1";
#else
        resPath += "/Ruby/1.8";
#endif
        rb_ary_push(lpaths, rb_str_new(resPath.c_str(), resPath.size()));
    }
#endif
    
    if (!conf.rubyLoadpaths.empty()) {
        /* Setup custom load paths */
        for (size_t i = 0; i < conf.rubyLoadpaths.size(); ++i) {
            std::string &path = conf.rubyLoadpaths[i];
            
            VALUE pathv = mkxp_str_new(path.c_str(), path.size());
            rb_ary_push(lpaths, pathv);
        }
    }
    else {
        rb_ary_push(lpaths, mkxp_str_new_cstr(mkxp_fs::getCurrentDirectory().c_str()));
    }
    
    mkxp::ScriptBootstrap::setLauncherIdentityGlobals();

    std::string &customScript = conf.customScript;
    if (!customScript.empty()) {
        mkxp::ScriptBootstrap::runConfigScript(customScript, true);
    } else {
        runRMXPScripts(btData);
    }
    
#if RAPI_FULL > 187
    VALUE exc = rb_errinfo();
#else
    VALUE exc = rb_gv_get("$!");
#endif

    /* Drain after capturing exc: the drain path may touch $!, and
     * the error display plus the clean-exit gate below must see the
     * exception the scripts actually ended with. */
    drainRaiseTrace();
    rb_set_errinfo(exc);

    if (!NIL_P(exc) && !rb_obj_is_kind_of(exc, rb_eSystemExit))
        showExcSafe(exc, btData);

    /* Flag the session as a clean exit whenever control reaches
     * here without a pending exception, or with a SystemExit - both
     * mean Ruby scripts finished normally (including the game's
     * own "Exit to desktop" menu calling Kernel.exit). Only real
     * crashes short-circuit past this path, so the default clean=
     * false state from mkxp_setGamePath stays false in those cases
     * and the UI shows the recovery alert.
     *
     * Additional gate for the no-exception case: at least one
     * script section must have executed without being skipped. If
     * every script bailed via the LoadError/SyntaxError/
     * NoMethodError tolerance path (e.g. a game whose Main does
     * nothing but `require 'socket'`), the user never got a
     * running game and shouldn't see the "game has ended or
     * requested a restart" message - that text only makes sense
     * for sessions where the game actually ran.
     *
     * SystemExit is exempt from that gate: `exit` in the very
     * first script section (Reborn's ScriptLoader boot checks
     * bail this way before the section can finish and be counted)
     * is still the game deliberately ending itself, not a crash. */
    if ((!NIL_P(exc) && rb_obj_is_kind_of(exc, rb_eSystemExit)) ||
        (NIL_P(exc) && s_scriptsExecutedThisSession > 0)) {
        mkxp_setEngineExitedCleanly();

        /* If the game printed something and then exited without
         * rendering another frame, the print is its parting message
         * (boot-gate pattern: "print reason; exit"). Route it to the
         * host so the exit alert shows the game's own words instead
         * of the generic clean-exit text. */
        if (!s_lastKernelPrintText.empty() &&
            s_lastKernelPrintFrame == shState->graphics().getFrameCount())
            mkxp_setErrorMessage(s_lastKernelPrintText.c_str());
    }

    /* No ruby_cleanup: Ruby's VM has process-global state (signal
     * handlers, atexit, parser, symbol table) that doesn't unwind
     * cleanly. The whole process exits when the user closes the app
     * from the app switcher, which is what does the actual cleanup. */

    shState->rtData().rqTermAck.set();
}

static void mriBindingTerminate() { throw Exception(Exception::SystemExit, " "); }

static void mriBindingReset() { throw Exception(Exception::Reset, " "); }
