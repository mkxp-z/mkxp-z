// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

// app_bridge.h - C-linkage bridge between the mkxp-z engine and the
// host UI layer. The only interface between them: UI never imports
// engine headers (SDL, SharedState), engine never imports UI headers
// (UIKit, SwiftUI). All communication goes through these functions.
//
// On non-mobile platforms the entire API degrades to inline no-op
// stubs (see the bottom of this file), so engine code may call
// mkxp_* functions unconditionally. No #ifdefs at call sites, no
// link dependency on app_bridge.cpp, and zero runtime cost where no
// host app exists.

#ifndef IOS_BRIDGE_H
#define IOS_BRIDGE_H

#include <stdbool.h>

// MKXPZ_MOBILE - 1 on platforms where a host app embeds the engine
// (iOS, iPadOS, tvOS and macOS), 0 elsewhere. Overridable from the
// build system.
#ifdef __APPLE__
#  include <TargetConditionals.h>
#endif
#ifndef MKXPZ_MOBILE
#  ifdef __APPLE__
#    define MKXPZ_MOBILE 1
#  else
#    define MKXPZ_MOBILE 0
#  endif
#endif

// Scancode constants. Values match SDL_Scancode (USB HID usage page
// 0x07) so the engine passes them through without translation. UI
// code uses these instead of importing SDL headers.

enum {
    MKXP_SCANCODE_UNKNOWN   = 0,

    // Letters
    MKXP_SCANCODE_A = 4,  MKXP_SCANCODE_B = 5,  MKXP_SCANCODE_C = 6,
    MKXP_SCANCODE_D = 7,  MKXP_SCANCODE_E = 8,  MKXP_SCANCODE_F = 9,
    MKXP_SCANCODE_G = 10, MKXP_SCANCODE_H = 11, MKXP_SCANCODE_I = 12,
    MKXP_SCANCODE_J = 13, MKXP_SCANCODE_K = 14, MKXP_SCANCODE_L = 15,
    MKXP_SCANCODE_M = 16, MKXP_SCANCODE_N = 17, MKXP_SCANCODE_O = 18,
    MKXP_SCANCODE_P = 19, MKXP_SCANCODE_Q = 20, MKXP_SCANCODE_R = 21,
    MKXP_SCANCODE_S = 22, MKXP_SCANCODE_T = 23, MKXP_SCANCODE_U = 24,
    MKXP_SCANCODE_V = 25, MKXP_SCANCODE_W = 26, MKXP_SCANCODE_X = 27,
    MKXP_SCANCODE_Y = 28, MKXP_SCANCODE_Z = 29,

    // Digits
    MKXP_SCANCODE_1 = 30, MKXP_SCANCODE_2 = 31, MKXP_SCANCODE_3 = 32,
    MKXP_SCANCODE_4 = 33, MKXP_SCANCODE_5 = 34, MKXP_SCANCODE_6 = 35,
    MKXP_SCANCODE_7 = 36, MKXP_SCANCODE_8 = 37, MKXP_SCANCODE_9 = 38,
    MKXP_SCANCODE_0 = 39,

    // Control / whitespace
    MKXP_SCANCODE_RETURN    = 40,
    MKXP_SCANCODE_ESCAPE    = 41,
    MKXP_SCANCODE_BACKSPACE = 42,
    MKXP_SCANCODE_TAB       = 43,
    MKXP_SCANCODE_SPACE     = 44,

    // Punctuation / symbols
    MKXP_SCANCODE_MINUS        = 45,
    MKXP_SCANCODE_EQUALS       = 46,
    MKXP_SCANCODE_LEFTBRACKET  = 47,
    MKXP_SCANCODE_RIGHTBRACKET = 48,
    MKXP_SCANCODE_BACKSLASH    = 49,
    MKXP_SCANCODE_SEMICOLON    = 51,
    MKXP_SCANCODE_APOSTROPHE   = 52,
    MKXP_SCANCODE_GRAVE        = 53,
    MKXP_SCANCODE_COMMA        = 54,
    MKXP_SCANCODE_PERIOD       = 55,
    MKXP_SCANCODE_SLASH        = 56,

    // Function keys
    MKXP_SCANCODE_F1  = 58, MKXP_SCANCODE_F2  = 59, MKXP_SCANCODE_F3  = 60,
    MKXP_SCANCODE_F4  = 61, MKXP_SCANCODE_F5  = 62, MKXP_SCANCODE_F6  = 63,
    MKXP_SCANCODE_F7  = 64, MKXP_SCANCODE_F8  = 65, MKXP_SCANCODE_F9  = 66,
    MKXP_SCANCODE_F10 = 67, MKXP_SCANCODE_F11 = 68, MKXP_SCANCODE_F12 = 69,

    // Arrow keys
    MKXP_SCANCODE_RIGHT = 79,
    MKXP_SCANCODE_LEFT  = 80,
    MKXP_SCANCODE_DOWN  = 81,
    MKXP_SCANCODE_UP    = 82,

    // Modifiers
    MKXP_SCANCODE_LCTRL  = 224,
    MKXP_SCANCODE_LSHIFT = 225,
    MKXP_SCANCODE_LALT   = 226,

    // Navigation
    MKXP_SCANCODE_HOME   = 74,
};

// ---------------------------------------------------------------------------
// Shared types - visible on every platform (both the live bridge and
// the no-op stubs use them).
// ---------------------------------------------------------------------------

// Lifecycle callbacks (Engine -> UI). Fire on the engine thread. UI
// must dispatch to main for any UI updates.
typedef void (*mkxp_EngineTerminatedCallback)(void *userdata);
typedef void (*mkxp_GameRectChangedCallback)(float x, float y, float w, float h, void *userdata);

// Key event callback (Engine -> UI, fires on background thread)
typedef void (*mkxp_KeyEventCallback)(int scancode, int pressed, void *userdata);

// Text-input mode callback (Engine -> UI). See the text-input bridge
// section below.
typedef void (*mkxp_TextInputModeCallback)(int active, void *userdata);

typedef void (*mkxp_ErrorMessageCallback)(const char *message, void *userdata);
typedef void (*mkxp_InfoMessageCallback)(const char *message, void *userdata);

// Fires on engine thread when paused (snapshot captured, audio suspended).
typedef void (*mkxp_PausedCallback)(void *userdata);
typedef void (*mkxp_ResumedCallback)(void *userdata);

// One-shot: fires on engine thread after first frame is swapped post-resume
// (or fresh start). UI uses this to fade the snapshot / dismiss loading.
typedef void (*mkxp_FrameRenderedCallback)(void *userdata);

typedef enum {
    MKXP_VALIGN_TOP        = 0,
    MKXP_VALIGN_TOP_CENTER = 1,
    MKXP_VALIGN_CENTER     = 2,
} MKXPVerticalAlignment;

// Per-game `syntaxTransform` override.
//
// The host calls the setter on every game selection so the
// developer's mkxp.json stays free of host-managed keys. Default is
// `UNSET` so desktop / test-harness builds that never call the
// setter keep the legacy mkxp.json-driven path. Numeric values match
// Config::syntaxTransform (0/1/2). The typed enum exists so callers
// don't sprinkle magic numbers.
typedef enum {
    MKXP_SYNTAX_TRANSFORM_UNSET     = -1,
    MKXP_SYNTAX_TRANSFORM_DISABLED  = 0,  // Ruby 3 strict
    MKXP_SYNTAX_TRANSFORM_CUSTOM    = 1,  // syntaxTransformCustomVersion* from mkxp.json
    MKXP_SYNTAX_TRANSFORM_LEGACY    = 2,  // Ruby 1.9 for RGSS3, Ruby 1.8 for RGSS<3
} MKXPSyntaxTransformMode;

// Per-game Ruby interpreter version selection.
//
// Each Ruby version's libruby + binding is compiled separately and
// merged into a relocatable .o with hidden symbols. At engine boot
// the host calls the setter to pick which version to dispatch to.
// main.cpp looks up `_mkxp_get_script_binding_<NN>()` from the
// matching merged .o.
//
// Lets a vintage PE game run on actual Ruby 1.8's parser + VM
// instead of a Ruby 3 parser with syntax-transform patches.
//
// `MKXP_RUBY_UNSET` falls back to the build's default. Numeric values
// are MMmm (3.0 -> 30, 1.8 -> 18), matching JoiPlay's libmkxpNN.so
// filename convention.
//
// `MKXP_RUBY_30` is retained for back-compat with metadata.json
// values written by older builds (when a native 3.0 binding shipped
// in the merged.o set). New builds route 30 to the 3.1 binding +
// Legacy syntax-transform mode at dispatch time. Keeping the enum
// value here keeps old `rubyVersion: 30` JSON decoding correctly.
typedef enum {
    MKXP_RUBY_UNSET = -1,
    MKXP_RUBY_18    = 18,
    MKXP_RUBY_19    = 19,
    MKXP_RUBY_30    = 30,
    MKXP_RUBY_31    = 31,
} MKXPRubyVersion;

// Phase of a host-injected pointer event. Pointer injection lets a
// host drive game mouse input from its own views instead of the SDL
// view. A host that never injects keeps the platform behaviour.
typedef enum {
    MKXP_POINTER_DOWN   = 0,
    MKXP_POINTER_MOVE   = 1,
    MKXP_POINTER_UP     = 2,
    MKXP_POINTER_CANCEL = 3,
} MKXPPointerPhase;

typedef struct {
    const char *managedConfigDir;
    const char *userDataDirectory;
    const char *sharedFontsDirectory;
    MKXPVerticalAlignment verticalAlignment;
} MKXPSessionConfig;

#if MKXPZ_MOBILE

// ---------------------------------------------------------------------------
// Live bridge (mobile). Implemented in app_bridge.cpp and the
// platform .mm files.
// ---------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// Process entry (Launcher -> Engine)

// Runs the engine on the calling thread, which must be the main
// thread. It returns when the game ends.
//
// Do not call it from a block on the main dispatch queue. It holds the
// thread for the whole session, so the queue would never drain, and the
// RGSS thread deadlocks on the first mkxp_getScreenScale, which
// dispatch_syncs to that queue. Use a run loop timer, the way
// SDLUIKitDelegate reaches SDL_main (SDL_uikitappdelegate.m:467).
//
// A launcher that links the engine statically does not need this:
// libSDL2main.a supplies main(), and SDLUIKitDelegate calls the
// engine's SDL_main after UIApplicationMain. A launcher that opens
// the engine with dlopen can use neither. A dynamic image holds no
// process entry point, and the ObjC runtime cannot find the class
// named SDLUIKitDelegate before that image loads. This runs what the
// delegate runs, without the launch screen and the idle-timer hint,
// which stay with the launcher.
int         mkxp_run_app(int argc, char **argv);

// Game lifecycle

void        mkxp_setGameReady(void);
int         mkxp_isGameReady(void);

// Game selection (Library -> Engine)

void        mkxp_setGamePath(const char *path);
const char *mkxp_waitForGamePath(void);

// Engine termination

void        mkxp_requestTerminate(void);
int         mkxp_isEngineTerminated(void);
void        mkxp_setEngineTerminated(void);

// Set when the session ends because Ruby raised SystemExit (e.g. the
// game's "Exit to desktop" menu). The UI checks this in its
// engine-terminated callback to skip the "didn't exit cleanly" alert.
int         mkxp_didEngineExitCleanly(void);
void        mkxp_setEngineExitedCleanly(void);

// Set when the RGSS thread failed to respond to a termination
// request. Engine is unrecoverable. The UI surfaces a "close from
// app switcher" alert because we can't recover in-process.
int         mkxp_isEngineHung(void);
void        mkxp_setEngineHung(void);

void        mkxp_setEngineTerminatedCallback(mkxp_EngineTerminatedCallback cb, void *userdata);
void        mkxp_setGameRectChangedCallback(mkxp_GameRectChangedCallback cb, void *userdata);

// Input injection (UI -> Engine)

// scancode: MKXP_SCANCODE_* value. pressed: 1=down, 0=up.
void        mkxp_injectKeyEvent(int scancode, int pressed);

void        mkxp_setKeyEventCallback(mkxp_KeyEventCallback cb, void *userdata);

// Pushes one left-button pointer event in top-left window points.
//
// The host owns the policy: which finger owns the pointer, and how
// coordinates are clamped. This call pushes what it is given. The
// events carry `SDL_TOUCH_MOUSEID`, so `mkxp_setTouchMouseEnabled`
// still gates whether the game sees them.
void        mkxp_injectPointerEvent(int x, int y, MKXPPointerPhase phase);

// Managed-config directory (UI -> Engine).
//
// The host may keep generated per-game state (mkxp.json,
// patches.json, save metadata) outside the imported game folder so
// the game directory stays a faithful mirror of what the user
// imported. The host calls `mkxp_setManagedConfigDir` with the
// per-game state path before each session. Engine modules that used
// to load from cwd (Config::read, Patcher auto-discovery) check this
// directory first and fall back to cwd only if the file isn't found.
//
// Pass NULL/"" to clear the override (cwd-only behavior, matches
// desktop builds).
void        mkxp_setManagedConfigDir(const char *path);
const char *mkxp_getManagedConfigDir(void);

// In-memory config overlay (UI -> Engine).
//
// A JSON object the host merges over the base mkxp.json at config-
// read time. Overlay keys win per TOP-LEVEL key (shallow merge: an
// overlay `bindingNames` replaces the whole object). JSON null
// values neutralize a key so the engine's guarded reads fall back to
// defaults. Lets hosts override config keys per session without
// mutating the developer's on-disk file. Same idea as the existing
// per-session bridge setters, generalized.
//
// Set before each session start. NULL or "" clears any previous
// overlay. Reject inputs over 1 MiB (warn log, keep previous state).
void        mkxp_setConfigOverlayJSON(const char *jsonUTF8);
const char *mkxp_getConfigOverlayJSON(void);

// Per-game UserData directory (UI -> Engine).
//
// On iOS, hosts store game writable payload in a per-game container
// (e.g. `Documents/Games/<id>/UserData/`) so saves and companion
// files are visible in the Files app and travel with the rest of the
// imported container. Games that use app-data helpers
// (`System.data_directory`, `MKXP.data_directory`, fake APPDATA env)
// and games that use relative RGSS save filenames are both routed
// here by the engine + preload compatibility layer.
void        mkxp_setUserDataDirectory(const char *path);
const char *mkxp_getUserDataDirectory(void);

// Shared fonts directory (UI -> Engine).
//
// A host-wide font pool, shared by every game the way the Windows
// system font folder is. The engine mounts it under the virtual
// "Fonts" mountpoint (game-own files keep priority) so its fonts
// load for every game, and the preload compatibility layer routes
// Windows-style "<SystemRoot>\Fonts\<file>" writes (Essentials'
// FontInstaller) into it. Unset = no shared pool. Per-game fonts
// only.
void        mkxp_setSharedFontsDirectory(const char *path);
const char *mkxp_getSharedFontsDirectory(void);

// Launcher identity (UI -> Engine).
//
// Name of the host launcher embedding the engine, exposed to game
// scripts before any preload/game code runs:
//
//   $userAgent        = "<name>"
//   $userAgentVersion = "<version>"  (frozen, "unknown" when the
//                                     version is NULL or "")
//   $<name>           = true         (only when the name is a valid
//                                     Ruby identifier:
//                                     [A-Za-z_][A-Za-z0-9_]*)
//
// This is the same detection contract JoiPlay established with its
// `$joiplay` global. Each host declares its own name so games and
// patches can branch on the specific launcher. Set once before the
// engine boots. Pass a NULL or "" name to clear (no globals are
// defined).
void        mkxp_setLauncherIdentity(const char *name, const char *version);
const char *mkxp_getLauncherIdentity(void);
const char *mkxp_getLauncherVersion(void);

// CA certificate bundle (UI -> Engine).
//
// Absolute path to a PEM CA bundle (e.g. the Mozilla root store)
// used to verify TLS server certificates for all engine-side
// networking: the native HTTPLite client, and Ruby's openssl ext
// (exported as SSL_CERT_FILE before the VM boots). Set once before
// the engine starts, like the launcher identity.
//
// When unset, TLS connections fail closed (certificate verification
// has no roots to succeed against). Plain http still works.
void        mkxp_setCABundlePath(const char *path);
const char *mkxp_getCABundlePath(void);

// Text-input bridge (UI <-> Engine).
//
// Games request text input via `Input.text_input = true`, which calls
// `SDL_StartTextInput()` inside EventThread. The mode callback fires
// from the main thread on state changes. iOS uses it to auto-show
// the system keyboard.
//
// `mkxp_pushTextInput` is the inverse: the soft keyboard's
// UITextField delegate forwards typed UTF-8 strings here, wrapped as
// SDL_TEXTINPUT events and read by Ruby `Input.gets`. Strings longer
// than SDL's 32-byte per-event limit are chunked at UTF-8 boundaries.
//
// `mkxp_isTextInputActive()` lets the UI skip pushing events when SDL
// text mode is off (otherwise the buffer fills with input nobody reads).
void        mkxp_setTextInputModeCallback(mkxp_TextInputModeCallback cb, void *userdata);
void        mkxp_pushTextInput(const char *utf8);
int         mkxp_isTextInputActive(void);

// Engine state queries

double      mkxp_getAverageFPS(void);

// The frame rate the game asks for (`Graphics.frame_rate`). Games
// set their own cap: RPG Maker XP games usually run at 40, VX and
// VX Ace games at 60. The host compares the average FPS against
// this number to tell "full speed" from "slow". Returns 0 when no
// game runs.
int         mkxp_getTargetFPS(void);

int         mkxp_getRGSSVersion(void);
const char *mkxp_getGameTitle(void);

// Bitmask of supported RGSS versions for this build.
// Bit 0 = RGSS1 (XP), bit 1 = RGSS2 (VX), bit 2 = RGSS3 (VX Ace).
// Determined at compile time by which Ruby runtime is linked.
int         mkxp_getSupportedRGSSVersionMask(void);

// Ruby runtime version the engine was built against.
// Example: "3.1" for Ruby 3.1, "1.8" for Ruby 1.8.
// The returned pointer is a static string literal, do not free.
const char *mkxp_getRubyVersion(void);

// ANGLE version string, extracted from GL_VERSION at engine init.
// Example: "2.1.0.abcdef1234". Returns "unknown" before the engine
// has initialized GL, or if the GL_VERSION string didn't match the
// expected ANGLE format. The returned pointer is a process-lifetime
// buffer, do not free.
const char *mkxp_getANGLEVersion(void);

// Metal device name, extracted from GL_RENDERER at engine init.
// Example: "Apple A15 GPU" or "Apple A17 Pro GPU". Returns "unknown"
// before the engine has initialized GL, or if the GL_RENDERER string
// didn't match the expected ANGLE format. The returned pointer is a
// process-lifetime buffer, do not free.
const char *mkxp_getMetalDeviceName(void);

// Game viewport rect (logical points)

void        mkxp_setGameRect(float x, float y, float w, float h);

// Safe area insets (logical points, cached atomics)

void        mkxp_getSafeAreaInsets(float *top, float *bottom, float *left, float *right);

// Push from UIKit main thread. Sets a "needs relayout" flag.
void        mkxp_setSafeAreaInsets(float top, float bottom, float left, float right);

// Returns true (once) if insets changed since last check.
bool        mkxp_consumeSafeAreaInsetsChanged(void);

// Host viewport region (dimensionless window fractions)
//
// The host may confine the game picture to a sub-rectangle of the
// window. x/y/w/h are fractions of the window in [0,1], top-left
// origin. isPortrait tags the orientation the region was computed
// for. The engine draws automatic placement while the window
// orientation does not match (rotation safety). With fixed aspect
// ratio on (the default), the picture aspect-fits centered inside
// the region. With it off, the picture stretches to fill the
// region, mirroring the no-region path. The vertical-alignment
// preset and the safe-area insets apply only on the no-region path.
// Sets the relayout flag. The engine applies the region at its next
// relayout poll.
void        mkxp_setHostViewportRegion(float x, float y, float w, float h,
                                       bool isPortrait);

// Back to automatic placement, as if no region was ever set.
void        mkxp_clearHostViewportRegion(void);

// Engine-side read. Returns false when no region is set.
bool        mkxp_getHostViewportRegion(float *x, float *y, float *w, float *h,
                                       bool *isPortrait);

// Screen scale factor (e.g. 3.0 on iPhone Pro).
float       mkxp_getScreenScale(void);

// The UIWindow* (iOS) or NSWindow* (macOS) the game draws in, or NULL
// before the engine creates it. Owned by the engine. Do not retain.
// For embedding host controls in the same window stack as the game
// view.
void       *mkxp_getGameWindow(void);

// Engine-internal: sets the window that mkxp_getGameWindow reports.
// NOT for UI.
struct SDL_Window;
void        mkxp_setGameSDLWindow(struct SDL_Window *window);

// Per-game settings (UI -> Engine), set by the host before engine
// boot and read by the engine during the run.
//
// Prefer `mkxp_applySessionConfig()` for pre-boot settings. It
// groups the fields the host sets together on every launch. Individual
// setters remain for mid-session toggles and legacy call sites.

void        mkxp_applyPerGameSettings(MKXPVerticalAlignment verticalAlignment,
                                      bool postloadEnabled);

MKXPVerticalAlignment mkxp_getVerticalAlignment(void);
bool        mkxp_getPostloadEnabled(void);

void                    mkxp_setSyntaxTransformMode(MKXPSyntaxTransformMode mode);
MKXPSyntaxTransformMode mkxp_getSyntaxTransformMode(void);

void             mkxp_setActiveRubyVersion(MKXPRubyVersion version);
MKXPRubyVersion  mkxp_getActiveRubyVersion(void);

void        mkxp_applySessionConfig(const MKXPSessionConfig *config);

// A per-game setting that only this core knows (UI -> Engine). The
// host sends each one as text before it calls mkxp_applySessionConfig.
// The keys and their values:
//
//   rubyVersion      "18", "19", "30" or "31". Any other value unsets it.
//   syntaxTransform  "legacy" or "modern". Any other value unsets it.
//   postloadScripts  "1" or "0"
//   inGameKeyboard   "1" or "0"
//   joiplayCompat    "1" or "0"
//   networkEnabled   "1" or "0"
//
// An unknown key writes a warning to the log and changes nothing.
void        mkxp_setSetting(const char *key, const char *value);

// Lines about the running engine for the host's debug overlay, one per
// line, such as the RGSS version, the Ruby version and the renderer.
// Call it on the main thread. The text stays valid until the next call.
const char *mkxp_getDetails(void);

// Adding a new per-boot setting: give it a key in mkxp_setSetting.
// Add a field to MKXPSessionConfig only when every core needs it.

// Force the Pokemon Essentials in-game keyboard scene, overriding
// the iOS soft keyboard. Default false (soft keyboard). Flip on for
// games whose keyboard scene adds custom keys the soft keyboard
// can't drive. `pokemon_input.rb` re-applies the historical
// `USEKEYBOARDTEXTENTRY = false` overrides when this is set.
void mkxp_setUseInGameKeyboard(bool enabled);
bool mkxp_getUseInGameKeyboard(void);

// JoiPlay-compat signal (`$joiplay = true` in the game's Ruby VM).
// Several games ship JoiPlay-specific patches that branch on the
// global. Whether those help or hurt depends on the game, so the
// host decides per boot. Default false. `platform_compat.rb` reads
// this via `System.joiplay_compat?` before game scripts load.
void mkxp_setJoiplayCompat(bool enabled);
bool mkxp_getJoiplayCompat(void);

// Network access (UI -> Engine). When disabled, the game sees the
// equivalent of airplane mode: network libraries load and their
// classes exist, but every connection attempt fails the way it
// would with no connectivity (native client refuses, socket
// connects raise ENETDOWN via the preload layer, downloads report
// failure). Games then take the same offline fallback paths they
// ship for desktop players without internet. Ruby reads this via
// `System.network_enabled?`. Default false (host must opt in).
void mkxp_setNetworkEnabled(bool enabled);
bool mkxp_getNetworkEnabled(void);

void        mkxp_setShowViewportBounds(bool enabled);
bool        mkxp_getShowViewportBounds(void);

// Cheat menu toggle (UI -> Engine). The cheat scripts read the
// current value each time the player opens the menu, so the toggle
// takes effect mid-game.
void        mkxp_setCheatsEnabled(bool enabled);
bool        mkxp_getCheatsEnabled(void);

/* Controls whether the engine consumes SDL game-controller events
 * for its built-in input bindings. Hosts that implement their own
 * physical-controller handling disable this to avoid double input.
 * Default: enabled. Must be set before or during session start.
 * Takes effect immediately. */
void        mkxp_setGameControllerCaptureEnabled(bool enabled);
bool        mkxp_getGameControllerCaptureEnabled(void);

/* Controls whether touch-synthesized SDL mouse events
 * (which == SDL_TOUCH_MOUSEID) are delivered to the engine input
 * layer. Default: false = upstream behavior (touch never synthesizes
 * mouse input). Hosts that want touch-as-mouse set it true. */
void        mkxp_setTouchMouseEnabled(bool enabled);
bool        mkxp_getTouchMouseEnabled(void);

void        mkxp_setViewportBoundsColor(float r, float g, float b, float a);
void        mkxp_getViewportBoundsColor(float *r, float *g, float *b, float *a);

// Error routing (Engine -> UI). `SDL_ShowSimpleMessageBox` is a no-op
// on iOS, so errors come through here for the UI to present.

void        mkxp_setErrorMessage(const char *message);

/* Present an error in the host UI and block the calling thread until
 * the user dismisses the alert. iOS only. No-op elsewhere. */
void        mkxp_presentErrorAndWait(const char *message);

/* Called by the host UI when the user dismisses an error alert that
 * may be blocking an engine thread in mkxp_presentErrorAndWait(). */
void        mkxp_signalErrorDismissed(void);

/* Report a fatal error to the host UI, debug log, and termination
 * path. Safe from any thread once the bridge is live. */
void        mkxp_reportFatalError(const char *message);

void        mkxp_installFatalErrorHandlers(void);

void        mkxp_setErrorMessageCallback(mkxp_ErrorMessageCallback cb, void *userdata);

// Info-message routing (Engine -> UI). Games call `msgbox` / `p`
// deliberately to show a notice (PE 20+ version banners, plugin
// dialogs) and then keep running. This is NOT an error: the UI
// should present a plain dismissible alert without "restart the app"
// framing. Blocks the engine thread until the user dismisses,
// mirroring mkxp_presentErrorAndWait.

/* Present an informational message in the host UI and block the
 * calling thread until the user dismisses it. iOS only. Fires the
 * info callback without blocking elsewhere. */
void        mkxp_presentInfoAndWait(const char *message);

/* Called by the host UI when the user dismisses an info alert that
 * may be blocking an engine thread in mkxp_presentInfoAndWait(). */
void        mkxp_signalInfoDismissed(void);

void        mkxp_setInfoMessageCallback(mkxp_InfoMessageCallback cb, void *userdata);

// Pause / Resume (UI <-> Engine).
//   1. UI calls `mkxp_requestPause()`
//   2. Engine's `mkxp_checkPause()` (called from Graphics blocking
//      points) pauses audio, fires the paused callback, and blocks
//      on a condvar
//   3. UI calls `mkxp_requestResume()` to unblock

void        mkxp_requestPause(void);
void        mkxp_requestResume(void);

// Engine-internal: checks for pause request, blocks if needed. NOT for UI.
void        mkxp_checkPause(void);

bool        mkxp_isPauseRequested(void);
bool        mkxp_isPaused(void);

// Engine-internal: seconds of run time that leave out the time the
// engine was away. Away is from a pause request to its resume, and
// while a game dialog waits for the player.
double      mkxp_engineSeconds(void);

// Engine-internal: how many times the engine came back from away.
unsigned    mkxp_engineComebacks(void);

// Snapshot: RGBA pixel buffer captured before blocking.
void        mkxp_setSnapshot(const unsigned char *data, int width, int height);

// Copy the snapshot into `dest` (must be at least width*height*4 bytes).
// Returns true if a snapshot was available, false if empty.
bool        mkxp_copySnapshotRGBA(unsigned char *dest, int destSize, int *width, int *height);

// Returns the snapshot dimensions without copying. Use to pre-allocate
// the buffer for mkxp_copySnapshotRGBA.
bool        mkxp_getSnapshotSize(int *width, int *height);

void        mkxp_setPausedCallback(mkxp_PausedCallback cb, void *userdata);
void        mkxp_setResumedCallback(mkxp_ResumedCallback cb, void *userdata);
void        mkxp_setFrameRenderedCallback(mkxp_FrameRenderedCallback cb, void *userdata);

// Engine-internal: fires the one-shot frame-rendered signal. NOT for UI.
void        mkxp_signalFrameRendered(void);

// GL context crash detection (set by SDL layer on caught SIGSEGV/SIGBUS).

void        mkxp_setGLContextBroken(void);
bool        mkxp_isGLContextBroken(void);

// Runtime fast-forward multiplier. When > 1 the FPS limiter scales
// target ticks-per-frame down so the game paces N times faster.
// Toggleable live without restart. Range: 1 (off) or 2-9 (active).
void        mkxp_setFastForwardMultiplier(int multiplier);
int         mkxp_getFastForwardMultiplier(void);

// Reset all per-session host-bridge state to engine defaults. Called
// once before each new game launch so values from the previous
// session (fast-forward multiplier, cheats flag) don't leak.
//
// Per-session = bridge fields that vary per game, stored in
// process-static atomics here. Globally-persistent state (viewport
// bounds debug overlay etc., owned by app-level Settings UI) is NOT
// touched.
void        mkxp_resetSessionState(void);

// Stops the process. A Ruby VM cannot be torn down in place.
void        mkxp_killSession(void);

// Debug logging

// Set log file path for this session (NULL/"" to disable).
void        mkxp_setDebugLogPath(const char *path);

// Append a tagged log line (no-op if disabled).
void        mkxp_debugLog(const char *tag, const char *source, const char *message);

// Fast-path predicate so hot-path callers can skip formatting the
// log message entirely when debug logging is off. Approximate: it
// may race with mkxp_setDebugLogPath but that's acceptable for a
// debug facility.
int         mkxp_debugLogEnabled(void);

#ifdef __cplusplus
}
#endif

#else /* !MKXPZ_MOBILE */

#include <time.h>

// ---------------------------------------------------------------------------
// Inert stubs (desktop and every non-mobile platform).
//
// Engine code calls mkxp_* unconditionally. Here each call collapses
// to an inline no-op the compiler deletes. Getter defaults are chosen
// to reproduce stock desktop mkxp-z behavior, NOT the mobile bridge's
// boot defaults, where the two differ (networkEnabled, vertical
// alignment) the stub comment says why.
// ---------------------------------------------------------------------------

#include <stddef.h>

static inline void        mkxp_setGameReady(void) {}
// Desktop has no launcher handshake: the game is ready the moment the
// process starts, and the game path comes from argv/cwd, not a host.
static inline int         mkxp_isGameReady(void) { return 1; }
static inline void        mkxp_setGamePath(const char *path) { (void)path; }
static inline const char *mkxp_waitForGamePath(void) { return NULL; }

static inline void        mkxp_requestTerminate(void) {}
static inline int         mkxp_isEngineTerminated(void) { return 0; }
static inline void        mkxp_setEngineTerminated(void) {}
static inline int         mkxp_didEngineExitCleanly(void) { return 0; }
static inline void        mkxp_setEngineExitedCleanly(void) {}
static inline int         mkxp_isEngineHung(void) { return 0; }
static inline void        mkxp_setEngineHung(void) {}

static inline void        mkxp_setEngineTerminatedCallback(mkxp_EngineTerminatedCallback cb, void *userdata) { (void)cb; (void)userdata; }
static inline void        mkxp_setGameRectChangedCallback(mkxp_GameRectChangedCallback cb, void *userdata) { (void)cb; (void)userdata; }

static inline void        mkxp_injectKeyEvent(int scancode, int pressed) { (void)scancode; (void)pressed; }
static inline void        mkxp_injectPointerEvent(int x, int y, MKXPPointerPhase phase) { (void)x; (void)y; (void)phase; }
static inline void        mkxp_setKeyEventCallback(mkxp_KeyEventCallback cb, void *userdata) { (void)cb; (void)userdata; }

// NULL managed-config dir == documented "cwd-only behavior, matches
// desktop builds".
static inline void        mkxp_setManagedConfigDir(const char *path) { (void)path; }
static inline const char *mkxp_getManagedConfigDir(void) { return NULL; }
static inline void        mkxp_setConfigOverlayJSON(const char *jsonUTF8) { (void)jsonUTF8; }
static inline const char *mkxp_getConfigOverlayJSON(void) { return NULL; }
static inline void        mkxp_setUserDataDirectory(const char *path) { (void)path; }
static inline const char *mkxp_getUserDataDirectory(void) { return NULL; }
static inline void        mkxp_setSharedFontsDirectory(const char *path) { (void)path; }
static inline const char *mkxp_getSharedFontsDirectory(void) { return NULL; }
static inline void        mkxp_setLauncherIdentity(const char *name, const char *version) { (void)name; (void)version; }
static inline const char *mkxp_getLauncherIdentity(void) { return NULL; }
static inline const char *mkxp_getLauncherVersion(void) { return NULL; }
static inline void        mkxp_setCABundlePath(const char *path) { (void)path; }
static inline const char *mkxp_getCABundlePath(void) { return NULL; }

static inline void        mkxp_setTextInputModeCallback(mkxp_TextInputModeCallback cb, void *userdata) { (void)cb; (void)userdata; }
static inline void        mkxp_pushTextInput(const char *utf8) { (void)utf8; }
static inline int         mkxp_isTextInputActive(void) { return 0; }

static inline double      mkxp_getAverageFPS(void) { return 0.0; }
static inline int         mkxp_getTargetFPS(void) { return 0; }
static inline int         mkxp_getRGSSVersion(void) { return 0; }
static inline const char *mkxp_getGameTitle(void) { return ""; }
static inline int         mkxp_getSupportedRGSSVersionMask(void) { return 0; }
static inline const char *mkxp_getRubyVersion(void) { return ""; }
static inline const char *mkxp_getANGLEVersion(void) { return "unknown"; }
static inline const char *mkxp_getMetalDeviceName(void) { return "unknown"; }

static inline void        mkxp_setGameRect(float x, float y, float w, float h) { (void)x; (void)y; (void)w; (void)h; }

// Desktop windows have no notch: zero insets, never "changed".
static inline void        mkxp_getSafeAreaInsets(float *top, float *bottom, float *left, float *right)
{ if (top) *top = 0; if (bottom) *bottom = 0; if (left) *left = 0; if (right) *right = 0; }
static inline void        mkxp_setSafeAreaInsets(float top, float bottom, float left, float right) { (void)top; (void)bottom; (void)left; (void)right; }
static inline bool        mkxp_consumeSafeAreaInsetsChanged(void) { return false; }
static inline void        mkxp_setHostViewportRegion(float x, float y, float w, float h, bool isPortrait) { (void)x; (void)y; (void)w; (void)h; (void)isPortrait; }
static inline void        mkxp_clearHostViewportRegion(void) {}
static inline bool        mkxp_getHostViewportRegion(float *x, float *y, float *w, float *h, bool *isPortrait) { (void)x; (void)y; (void)w; (void)h; (void)isPortrait; return false; }
static inline float       mkxp_getScreenScale(void) { return 1.0f; }
static inline void       *mkxp_getGameWindow(void) { return NULL; }
struct SDL_Window;
static inline void        mkxp_setGameSDLWindow(struct SDL_Window *window) { (void)window; }

static inline void        mkxp_applyPerGameSettings(MKXPVerticalAlignment verticalAlignment, bool postloadEnabled) { (void)verticalAlignment; (void)postloadEnabled; }
// CENTER, not the mobile TOP_CENTER default: stock desktop mkxp-z
// centers the game in the window in every orientation.
static inline MKXPVerticalAlignment mkxp_getVerticalAlignment(void) { return MKXP_VALIGN_CENTER; }
// No postload compatibility layer on desktop (stock behavior).
static inline bool        mkxp_getPostloadEnabled(void) { return false; }

static inline void                    mkxp_setSyntaxTransformMode(MKXPSyntaxTransformMode mode) { (void)mode; }
static inline MKXPSyntaxTransformMode mkxp_getSyntaxTransformMode(void) { return MKXP_SYNTAX_TRANSFORM_UNSET; }
static inline void             mkxp_setActiveRubyVersion(MKXPRubyVersion version) { (void)version; }
static inline MKXPRubyVersion  mkxp_getActiveRubyVersion(void) { return MKXP_RUBY_UNSET; }

static inline void        mkxp_applySessionConfig(const MKXPSessionConfig *config) { (void)config; }

// Desktop has no soft keyboard. A game's own keyboard scene is the
// only text-entry path, so "use in-game keyboard" is trivially true.
static inline bool        mkxp_getUseInGameKeyboard(void) { return true; }
static inline void        mkxp_setUseInGameKeyboard(bool enabled) { (void)enabled; }
static inline void        mkxp_setJoiplayCompat(bool enabled) { (void)enabled; }
static inline bool        mkxp_getJoiplayCompat(void) { return false; }
// true, not the mobile opt-in default: stock desktop mkxp-z has no
// network kill-switch, so the stub must not fake airplane mode.
static inline bool        mkxp_getNetworkEnabled(void) { return true; }
static inline void        mkxp_setNetworkEnabled(bool enabled) { (void)enabled; }

static inline void        mkxp_setShowViewportBounds(bool enabled) { (void)enabled; }
static inline bool        mkxp_getShowViewportBounds(void) { return false; }
static inline void        mkxp_setCheatsEnabled(bool enabled) { (void)enabled; }
static inline bool        mkxp_getCheatsEnabled(void) { return false; }
static inline void        mkxp_setGameControllerCaptureEnabled(bool enabled) { (void)enabled; }
// Desktop engine always consumes its own controller events.
static inline bool        mkxp_getGameControllerCaptureEnabled(void) { return true; }
static inline void        mkxp_setTouchMouseEnabled(bool enabled) { (void)enabled; }
// Upstream behavior: touch never synthesizes mouse input.
static inline bool        mkxp_getTouchMouseEnabled(void) { return false; }
static inline void        mkxp_setViewportBoundsColor(float r, float g, float b, float a) { (void)r; (void)g; (void)b; (void)a; }
static inline void        mkxp_getViewportBoundsColor(float *r, float *g, float *b, float *a)
{ if (r) *r = 0; if (g) *g = 0; if (b) *b = 0; if (a) *a = 1; }

// Error/info surfaces: desktop presents through SDL message boxes at
// the call sites that own them. The bridge routes are inert.
static inline void        mkxp_setErrorMessage(const char *message) { (void)message; }
static inline void        mkxp_presentErrorAndWait(const char *message) { (void)message; }
static inline void        mkxp_signalErrorDismissed(void) {}
static inline void        mkxp_reportFatalError(const char *message) { (void)message; }
static inline void        mkxp_installFatalErrorHandlers(void) {}
static inline void        mkxp_setErrorMessageCallback(mkxp_ErrorMessageCallback cb, void *userdata) { (void)cb; (void)userdata; }
static inline void        mkxp_presentInfoAndWait(const char *message) { (void)message; }
static inline void        mkxp_signalInfoDismissed(void) {}
static inline void        mkxp_setInfoMessageCallback(mkxp_InfoMessageCallback cb, void *userdata) { (void)cb; (void)userdata; }

// No host-driven pause on desktop (window focus handling is SDL's).
static inline void        mkxp_requestPause(void) {}
static inline void        mkxp_requestResume(void) {}
static inline void        mkxp_checkPause(void) {}
static inline bool        mkxp_isPauseRequested(void) { return false; }
static inline bool        mkxp_isPaused(void) { return false; }
static inline double      mkxp_engineSeconds(void)
{ struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
static inline unsigned    mkxp_engineComebacks(void) { return 0; }
static inline void        mkxp_setSnapshot(const unsigned char *data, int width, int height) { (void)data; (void)width; (void)height; }
static inline bool        mkxp_copySnapshotRGBA(unsigned char *dest, int destSize, int *width, int *height) { (void)dest; (void)destSize; (void)width; (void)height; return false; }
static inline bool        mkxp_getSnapshotSize(int *width, int *height) { (void)width; (void)height; return false; }
static inline void        mkxp_setPausedCallback(mkxp_PausedCallback cb, void *userdata) { (void)cb; (void)userdata; }
static inline void        mkxp_setResumedCallback(mkxp_ResumedCallback cb, void *userdata) { (void)cb; (void)userdata; }
static inline void        mkxp_setFrameRenderedCallback(mkxp_FrameRenderedCallback cb, void *userdata) { (void)cb; (void)userdata; }
static inline void        mkxp_signalFrameRendered(void) {}

static inline void        mkxp_setGLContextBroken(void) {}
static inline bool        mkxp_isGLContextBroken(void) { return false; }

static inline void        mkxp_setFastForwardMultiplier(int multiplier) { (void)multiplier; }
static inline int         mkxp_getFastForwardMultiplier(void) { return 1; }
static inline void        mkxp_resetSessionState(void) {}

static inline int         mkxp_run_app(int argc, char **argv) { (void)argc; (void)argv; return 0; }

static inline void        mkxp_setDebugLogPath(const char *path) { (void)path; }
static inline void        mkxp_debugLog(const char *tag, const char *source, const char *message) { (void)tag; (void)source; (void)message; }
static inline int         mkxp_debugLogEnabled(void) { return 0; }

#endif /* MKXPZ_MOBILE */

#endif // IOS_BRIDGE_H
