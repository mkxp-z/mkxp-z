// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

// app_bridge.cpp - Minimal C bridge for host overlay to query engine state.
// Kept separate from engine code. No UIKit imports. No game logic.

#include "app_bridge.h"

// On non-mobile platforms app_bridge.h defines the whole API as
// inline no-op stubs. This file must compile to nothing so the two
// never clash if a desktop build sweeps it up.
#if MKXPZ_MOBILE

#include "sharedstate.h"
#include "graphics.h"
#include "audio.h"
#include "eventthread.h"
#include "config.h"
#include "util/awayclock.h"
#include "util/debugwriter.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <pthread.h>
#include <condition_variable>
#include <string>
#include <vector>

#include <SDL.h>
#include <SDL_syswm.h>

#if MKXPZ_MOBILE
#include <CoreFoundation/CoreFoundation.h>
#endif

static std::atomic<bool> s_gameReady{false};

// Debug log path for Ruby errors (empty = disabled).
static std::mutex s_debugLogMutex;
static std::string s_debugLogPath;
static FILE *s_debugLogFile = nullptr;

// Game path selection: Library sets the path, engine waits for it.
static std::mutex s_pathMutex;
static std::string s_gamePath;
static std::atomic<bool> s_pathSet{false};

// Engine terminated flag: set by engine after full teardown.
static std::atomic<bool> s_engineTerminated{false};
static std::atomic<bool> s_terminateRequested{false};
// Set when the session ended because Ruby raised SystemExit
// (e.g. the game's "Exit to desktop" menu). The Swift layer reads
// this to skip the "didn't exit cleanly" alert and return silently.
static std::atomic<bool> s_engineExitedCleanly{false};
// Set when the RGSS thread didn't ack a termination request in time.
// `mkxp_isEngineHung()` makes the UI surface a "close from app
// switcher" alert. We can't recover the engine in-process.
static std::atomic<bool> s_engineHung{false};
static std::atomic<bool> s_glContextBroken{false};

// Game viewport rect in logical points, updated by the engine each frame.
static std::atomic<float> s_gameRectX{0};
static std::atomic<float> s_gameRectY{0};
static std::atomic<float> s_gameRectW{0};
static std::atomic<float> s_gameRectH{0};

// Cached safe area insets in logical points, pushed from UIKit.
static std::atomic<float> s_safeAreaTop{0};
static std::atomic<float> s_safeAreaBottom{0};
static std::atomic<float> s_safeAreaLeft{0};
static std::atomic<float> s_safeAreaRight{0};
static std::atomic<bool>  s_safeAreaInsetsChanged{false};

// Per-game settings: vertical alignment and postload toggle.
static std::atomic<int>   s_verticalAlignment{1};  // 0=top, 1=top-center, 2=center
static std::atomic<bool>  s_postloadEnabled{true};

// Per-game syntax-transform mode. The host sets this before each
// session so the choice stays out of mkxp.json (it's a host concept,
// not part of the developer's engine config). See app_bridge.h for
// the enum.
static std::atomic<int>   s_syntaxTransformMode{MKXP_SYNTAX_TRANSFORM_UNSET};

// Per-game "force in-game keyboard scene" toggle. Default false
// (iOS soft keyboard handles name entry). Read by `pokemon_input.rb`
// when set to fall back to the game's own keyboard scene.
static std::atomic<bool>  s_useInGameKeyboard{false};

// Per-game JoiPlay-compat toggle. Default false. Read by
// `platform_compat.rb` to decide whether to set `$joiplay = true`.
static std::atomic<bool>  s_joiplayCompat{false};

// Per-game network access toggle. Default false (offline illusion
// preserved unless the host opts in). Read by the preload layer via
// `System.network_enabled?`.
static std::atomic<bool>  s_networkEnabled{false};

// Debug: tint the area outside the game viewport.
static std::atomic<bool>  s_showViewportBounds{false};
static std::atomic<float> s_vpBoundsR{0};
static std::atomic<float> s_vpBoundsG{0};
static std::atomic<float> s_vpBoundsB{0};
static std::atomic<float> s_vpBoundsA{1};

// Cheat menu toggle. The cheat scripts read it through
// MKXP.cheats_enabled? each time the player opens the menu.
static std::atomic<bool>  s_cheatsEnabled{false};

// When false, EventThread discards SDL_CONTROLLER* events so a host
// layer can inject keyboard events without double-applying presses.
static std::atomic<bool>  s_gameControllerCaptureEnabled{true};

// When false, EventThread discards touch-synthesized SDL mouse events.
static std::atomic<bool>  s_touchMouseEnabled{false};

// Input bridge: cached SDL window ID for event injection.
static std::atomic<uint32_t> s_sdlWindowID{0};

// Holder for a callback + userdata pair. The previous split between
// `atomic<Fn>` and a bare `void *` could let a reader observe a torn
// pair on teardown. Packing both into a shared_ptr keeps set and
// fire on a consistent tuple.
template <typename Fn>
struct BridgeCallback {
    struct Slot {
        Fn fn;
        void *userdata;
    };

    // shared_ptr<const Slot> so the setter can CAS a new slot while an
    // in-flight caller still holds a reference to the old one.
    std::shared_ptr<const Slot> slot;
    std::mutex slotMutex;

    void set(Fn fn, void *userdata) noexcept {
        auto next = std::make_shared<const Slot>(Slot{fn, userdata});
        std::lock_guard<std::mutex> lock(slotMutex);
        slot = next;
    }

    // Snapshot the current slot and hand it to the caller. The caller
    // holds a strong ref so even a concurrent set() can't pull the
    // slot out from under them.
    std::shared_ptr<const Slot> snapshot() noexcept {
        std::lock_guard<std::mutex> lock(slotMutex);
        return slot;
    }

    // Convenience: load + fire in one go. Args are forwarded straight
    // to the stored function pointer. Userdata is appended.
    template <typename... Args>
    void fire(Args&&... args) noexcept {
        auto s = snapshot();
        if (s && s->fn) {
            s->fn(std::forward<Args>(args)..., s->userdata);
        }
    }

    void clear() noexcept {
        std::lock_guard<std::mutex> lock(slotMutex);
        slot.reset();
    }

    bool isSet() noexcept {
        auto s = snapshot();
        return s && s->fn;
    }
};

// Key event callback: engine -> UI notification for hardware key events.
static BridgeCallback<mkxp_KeyEventCallback> s_keyEventCb;
static bool s_keyWatcherInstalled = false;

// Text-input mode change callback: engine -> UI notification when
// SDL_StartTextInput / SDL_StopTextInput fires inside EventThread.
static BridgeCallback<mkxp_TextInputModeCallback> s_textInputModeCb;

// Managed-config directory the host sets before each session. Engine
// modules look here first for files they used to read from cwd
// (mkxp.json, patches.json). Empty = no override.
static std::mutex s_managedConfigMutex;
static std::string s_managedConfigDir;

// In-memory mkxp.json overlay merged after the base config file.
static std::mutex s_configOverlayMutex;
static std::string s_configOverlayJSON;

// Per-game UserData directory the host sets before each session.
static std::mutex s_saveDirMutex;
static std::string s_saveDir;
static std::mutex s_sharedFontsDirMutex;
static std::string s_sharedFontsDir;

// Launcher identity the host sets once before engine boot. Script
// bootstrap reads it to define the `$userAgent` / `$userAgentVersion`
// / `$<name>` globals.
static std::mutex s_launcherIdentityMutex;
static std::string s_launcherIdentity;
static std::string s_launcherVersion;

// PEM CA bundle used for TLS server verification (native HTTP client
// + Ruby openssl via SSL_CERT_FILE). Set once before engine boot.
static std::mutex s_caBundlePathMutex;
static std::string s_caBundlePath;

// Game-side text-input intent. Set when the engine processes
// REQUEST_TEXTMODE(1) (game called `Input.text_input = true`),
// cleared on REQUEST_TEXTMODE(0). Distinct from
// `SDL_IsTextInputActive()` because SDL's iOS backend auto-stops
// text input on keyboard-hide notifications - which fire briefly
// even when our own UITextField becomes first responder, leaving
// SDL's state OFF while the game's Ruby loop still expects input.
// `mkxp_isTextInputActive()` reads this flag instead of asking SDL.
static std::atomic<bool> s_gameTextInputIntent{false};

// Lifecycle callbacks: engine -> UI notifications for state changes.
static BridgeCallback<mkxp_EngineTerminatedCallback> s_engineTerminatedCb;
static BridgeCallback<mkxp_GameRectChangedCallback> s_gameRectChangedCb;

// Error message routing: engine -> UI.
static std::mutex s_errorMsgMutex;
static std::string s_errorMessage;
static BridgeCallback<mkxp_ErrorMessageCallback> s_errorMsgCb;

// Info message routing: engine -> UI. Deliberate game dialogs
// (msgbox / p) that are not errors. Separate channel so the host
// can present them without error framing.
static BridgeCallback<mkxp_InfoMessageCallback> s_infoMsgCb;

static BridgeCallback<mkxp_SaveEventCallback> s_saveEventCb;

#if MKXPZ_MOBILE
static std::mutex s_errorDismissMutex;
static std::condition_variable s_errorDismissCV;
static std::atomic<bool> s_errorDismissed{false};
static std::atomic<bool> s_errorAwaitingDismiss{false};

static std::mutex s_infoDismissMutex;
static std::condition_variable s_infoDismissCV;
static std::atomic<bool> s_infoDismissed{false};
static std::atomic<bool> s_infoAwaitingDismiss{false};
#endif

// Pause / Resume state.
static std::atomic<bool> s_pauseRequested{false};
static std::atomic<bool> s_paused{false};
static std::mutex s_pauseMutex;
static std::condition_variable s_pauseCV;

static std::mutex s_awayMutex;
static AwayClock s_awayClock{AwayClock::Clock::now()};

static void leaveEngine() {
    std::lock_guard<std::mutex> lock(s_awayMutex);
    s_awayClock.leave(AwayClock::Clock::now());
}

static void comeBackToEngine() {
    std::lock_guard<std::mutex> lock(s_awayMutex);
    s_awayClock.comeBack(AwayClock::Clock::now());
}

/* The flag and the clock change under one lock. Else a resume between
 * the two starts a clock that still runs, and the pause then stops it
 * for good. */
static void setPauseRequested(bool requested) {
    std::lock_guard<std::mutex> lock(s_awayMutex);
    if (s_pauseRequested.exchange(requested, std::memory_order_acq_rel) == requested)
        return;
    if (requested)
        s_awayClock.leave(AwayClock::Clock::now());
    else
        s_awayClock.comeBack(AwayClock::Clock::now());
}

double mkxp_engineSeconds(void) {
    std::lock_guard<std::mutex> lock(s_awayMutex);
    return s_awayClock.seconds(AwayClock::Clock::now());
}

unsigned mkxp_engineComebacks(void) {
    std::lock_guard<std::mutex> lock(s_awayMutex);
    return s_awayClock.comebacks();
}

// Snapshot buffer: owned by the bridge, written by engine, read by UI.
static std::mutex s_snapshotMutex;
static std::vector<unsigned char> s_snapshotData;
static int s_snapshotWidth = 0;
static int s_snapshotHeight = 0;

// Pause/resume callbacks.
static BridgeCallback<mkxp_PausedCallback> s_pausedCb;
static BridgeCallback<mkxp_ResumedCallback> s_resumedCb;

// Frame-rendered-after-resume: one-shot signal so the UI knows
// the live SDL surface is visible and the snapshot can fade out.
static std::atomic<bool> s_needsFrameRenderedSignal{false};
static BridgeCallback<mkxp_FrameRenderedCallback> s_frameRenderedCb;

extern "C" {

void mkxp_setGameReady(void) {
    s_gameReady.store(true, std::memory_order_release);
    // Arm the frame-rendered signal so the UI knows when the first
    // frame has been swapped to the screen.
    s_needsFrameRenderedSignal.store(true, std::memory_order_release);
}

int mkxp_isGameReady(void) {
    return s_gameReady.load(std::memory_order_acquire) ? 1 : 0;
}

void mkxp_setGamePath(const char *path) {
    std::lock_guard<std::mutex> lock(s_pathMutex);
    s_gamePath = path ? path : "";
    s_pathSet.store(true, std::memory_order_release);
    // A fresh session: clear any clean-exit flag set by the
    // previous session so the termination callback for this one
    // starts from the default "unclean unless proven clean" state.
    s_engineExitedCleanly.store(false, std::memory_order_release);
}

const char *mkxp_waitForGamePath(void) {
#if MKXPZ_MOBILE
    // On iOS, SDL_main runs on the main thread. We must pump the main
    // run loop while waiting so that UIKit can render the Library UI
    // and dispatch_async blocks can execute.
    while (!s_pathSet.load(std::memory_order_acquire)) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, true);
    }
#else
    while (!s_pathSet.load(std::memory_order_acquire)) {
        // spin (non-iOS fallback, shouldn't be reached)
    }
#endif
    // Copy under lock so the returned c_str() remains valid until the
    // next call from this thread (thread_local lifetime).
    thread_local std::string pathCopy;
    std::lock_guard<std::mutex> lock(s_pathMutex);
    pathCopy = s_gamePath;
    return pathCopy.c_str();
}

void mkxp_requestTerminate(void) {
    s_terminateRequested.store(true, std::memory_order_release);

    // If the engine is paused on the condvar, unblock it so it can
    // process SDL_QUIT. This avoids needing a separate resume call.
    {
        std::lock_guard<std::mutex> lock(s_pauseMutex);
        s_paused.store(false, std::memory_order_release);
        setPauseRequested(false);
    }
    s_pauseCV.notify_one();

    // Push SDL_QUIT into the event queue. The engine's event loop
    // will pick it up and initiate normal shutdown.
    SDL_Event event;
    event.type = SDL_QUIT;
    SDL_PushEvent(&event);
}

int mkxp_isEngineTerminated(void) {
    return s_engineTerminated.load(std::memory_order_acquire) ? 1 : 0;
}

int mkxp_didEngineExitCleanly(void) {
    return s_engineExitedCleanly.load(std::memory_order_acquire) ? 1 : 0;
}

void mkxp_setEngineExitedCleanly(void) {
    s_engineExitedCleanly.store(true, std::memory_order_release);
}

int mkxp_isEngineHung(void) {
    return s_engineHung.load(std::memory_order_acquire) ? 1 : 0;
}

void mkxp_setEngineHung(void) {
    s_engineHung.store(true, std::memory_order_release);
}

void mkxp_setEngineTerminated(void) {
    s_engineTerminated.store(true, std::memory_order_release);
    s_engineTerminatedCb.fire();
}

double mkxp_getAverageFPS(void) {
    if (s_engineTerminated.load(std::memory_order_acquire)) return 0.0;
    if (!SharedState::instance) return 0.0;
    return SharedState::instance->graphics().averageFrameRate();
}

int mkxp_getTargetFPS(void) {
    if (s_engineTerminated.load(std::memory_order_acquire)) return 0;
    if (!SharedState::instance) return 0;
    // A pinned rate overrides the game's own request, the same way
    // Graphics::setFrameRate does, so report the rate the limiter
    // really enforces.
    const int pinned = SharedState::instance->rtData().config.fixedFramerate;
    if (pinned > 0) return pinned;
    return SharedState::instance->graphics().getFrameRate();
}

int mkxp_getRGSSVersion(void) {
    if (s_engineTerminated.load(std::memory_order_acquire)) return 0;
    if (!SharedState::instance) return 0;
    return SharedState::instance->rtData().config.rgssVersion;
}

int mkxp_getSupportedRGSSVersionMask(void) {
    // RGSS1 and RGSS2 run fine on any Ruby (1.8-era syntax). RGSS3
    // needs Ruby 1.9+, which here means Ruby 3.1 with the
    // syntax-transform patches. The build system defines
    // MKXPZ_HAVE_SYNTAX_TRANSFORM_PATCHES only when patched Ruby is
    // linked.
#if defined(MKXPZ_HAVE_SYNTAX_TRANSFORM_PATCHES)
    return (1 << 0) | (1 << 1) | (1 << 2);
#else
    return (1 << 0) | (1 << 1);
#endif
}

const char *mkxp_getRubyVersion(void) {
    // Returns the major.minor of the Ruby actually dispatched for
    // the active game, NOT the build-time MKXPZ_RUBY_VERSION of the
    // engine host (which would mis-report any game routed to a
    // Ruby other than the host TU's compile flags).
    switch (mkxp_getActiveRubyVersion()) {
        case MKXP_RUBY_18: return "1.8";
        case MKXP_RUBY_19: return "1.9";
        /* MKXP_RUBY_30 is deprecated; the binding routes it to the
         * 3.1 build at runtime. Keep the enum value for old metadata
         * decode but report the runtime version honestly. */
        case MKXP_RUBY_30: return "3.1";
        case MKXP_RUBY_31: return "3.1";
        case MKXP_RUBY_UNSET:
        default:           break;
    }

    // No active version set. Fall back to the host build-time
    // define so desktop / test-harness builds still get a useful
    // answer. The "(fallback)" suffix surfaces the case in the UI:
    // on multi-Ruby iOS, hitting this path means the host forgot
    // to call mkxp_setActiveRubyVersion.
#if defined(MKXPZ_RUBY_VERSION)
    return MKXPZ_RUBY_VERSION " (fallback)";
#else
    static std::atomic<bool> warned{false};
    bool expected = false;
    if (warned.compare_exchange_strong(expected, true)) {
        mkxp_debugLog("BRIDGE", "app_bridge.cpp [C++]",
                      "mkxp_setActiveRubyVersion not called and "
                      "MKXPZ_RUBY_VERSION not defined; reporting "
                      "\"unknown\" to the UI.");
    }
    return "unknown";
#endif
}

const char *mkxp_getGameTitle(void) {
    if (s_engineTerminated.load(std::memory_order_acquire)) return "";
    if (!SharedState::instance) return "";
    return SharedState::instance->rtData().config.game.title.c_str();
}

void mkxp_setGameRect(float x, float y, float w, float h) {
    float oldX = s_gameRectX.exchange(x, std::memory_order_relaxed);
    float oldY = s_gameRectY.exchange(y, std::memory_order_relaxed);
    float oldW = s_gameRectW.exchange(w, std::memory_order_relaxed);
    float oldH = s_gameRectH.exchange(h, std::memory_order_relaxed);
    if (x != oldX || y != oldY || w != oldW || h != oldH) {
        s_gameRectChangedCb.fire(x, y, w, h);
    }
}

void mkxp_getSafeAreaInsets(float *top, float *bottom, float *left, float *right) {
    if (top)    *top    = s_safeAreaTop.load(std::memory_order_relaxed);
    if (bottom) *bottom = s_safeAreaBottom.load(std::memory_order_relaxed);
    if (left)   *left   = s_safeAreaLeft.load(std::memory_order_relaxed);
    if (right)  *right  = s_safeAreaRight.load(std::memory_order_relaxed);
}

void mkxp_setSafeAreaInsets(float top, float bottom, float left, float right) {
    float oldTop    = s_safeAreaTop.load(std::memory_order_relaxed);
    float oldBottom = s_safeAreaBottom.load(std::memory_order_relaxed);
    float oldLeft   = s_safeAreaLeft.load(std::memory_order_relaxed);
    float oldRight  = s_safeAreaRight.load(std::memory_order_relaxed);
    s_safeAreaTop.store(top, std::memory_order_relaxed);
    s_safeAreaBottom.store(bottom, std::memory_order_relaxed);
    s_safeAreaLeft.store(left, std::memory_order_relaxed);
    s_safeAreaRight.store(right, std::memory_order_relaxed);
    if (top != oldTop || bottom != oldBottom || left != oldLeft || right != oldRight) {
        s_safeAreaInsetsChanged.store(true, std::memory_order_release);
    }
}

bool mkxp_consumeSafeAreaInsetsChanged(void) {
    return s_safeAreaInsetsChanged.exchange(false, std::memory_order_acquire);
}

// Host viewport region: ONE packed word so a reader can never see a
// torn rect mid-drag (the four-atomics gameRect pattern can tear
// across a frame). Layout: bit 63 = set, bit 62 = portrait tag,
// bits 45-59 / 30-44 / 15-29 / 0-14 = x / y / w / h as 15-bit
// fractions (0..32767, <= 0.1 px error on a 3000 px window).
// Ordering: the word stores relaxed, then the relayout flag stores
// release - the same values-then-flag order as the insets above, so
// the acquire in mkxp_consumeSafeAreaInsetsChanged() publishes it.
static std::atomic<uint64_t> s_hostViewportRegion{0};

static inline uint64_t packRegionField(float v) {
    /* !(v >= 0) also catches NaN - a raw NaN would hit an undefined
     * float-to-int cast below. */
    if (!(v >= 0.0f)) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return (uint64_t)(v * 32767.0f + 0.5f) & 0x7FFF;
}

void mkxp_setHostViewportRegion(float x, float y, float w, float h,
                                bool isPortrait) {
    uint64_t word = (1ULL << 63)
                  | (isPortrait ? (1ULL << 62) : 0)
                  | (packRegionField(x) << 45)
                  | (packRegionField(y) << 30)
                  | (packRegionField(w) << 15)
                  | packRegionField(h);
    s_hostViewportRegion.store(word, std::memory_order_relaxed);
    s_safeAreaInsetsChanged.store(true, std::memory_order_release);
}

void mkxp_clearHostViewportRegion(void) {
    s_hostViewportRegion.store(0, std::memory_order_relaxed);
    s_safeAreaInsetsChanged.store(true, std::memory_order_release);
}

bool mkxp_getHostViewportRegion(float *x, float *y, float *w, float *h,
                                bool *isPortrait) {
    uint64_t word = s_hostViewportRegion.load(std::memory_order_relaxed);
    if (!(word >> 63)) return false;
    if (x) *x = ((word >> 45) & 0x7FFF) / 32767.0f;
    if (y) *y = ((word >> 30) & 0x7FFF) / 32767.0f;
    if (w) *w = ((word >> 15) & 0x7FFF) / 32767.0f;
    if (h) *h = (word & 0x7FFF) / 32767.0f;
    if (isPortrait) *isPortrait = (word >> 62) & 1;
    return true;
}

// The first game rect fires from inside the SharedState constructor,
// before SharedState::instance is set. The window exists by then.
static std::atomic<SDL_Window *> s_gameSDLWindow{nullptr};

void mkxp_setGameSDLWindow(SDL_Window *window) {
    s_gameSDLWindow.store(window, std::memory_order_release);
}

void *mkxp_getGameWindow(void) {
    SDL_Window *win = s_gameSDLWindow.load(std::memory_order_acquire);
    if (!win) return nullptr;

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(win, &info)) return nullptr;
#if TARGET_OS_IPHONE
    return info.info.uikit.window;
#else
    return info.info.cocoa.window;
#endif
}

// Input bridge

static Uint32 injectedWindowID() {
    if (s_sdlWindowID.load(std::memory_order_relaxed) == 0) {
        SDL_Window *w = SDL_GetGrabbedWindow();
        if (w) {
            s_sdlWindowID.store(SDL_GetWindowID(w), std::memory_order_relaxed);
        } else {
            // Single-window app: SDL window IDs start at 1
            s_sdlWindowID.store(1, std::memory_order_relaxed);
        }
    }
    return s_sdlWindowID.load(std::memory_order_relaxed);
}

void mkxp_injectKeyEvent(int scancode, int pressed) {
    SDL_Event event{};
    event.type              = pressed ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.timestamp     = SDL_GetTicks();
    event.key.windowID      = injectedWindowID();
    event.key.state         = pressed ? SDL_PRESSED : SDL_RELEASED;
    event.key.repeat        = 0;
    event.key.keysym.scancode = (SDL_Scancode)scancode;
    event.key.keysym.sym    = SDL_GetKeyFromScancode((SDL_Scancode)scancode);
    event.key.keysym.mod    = KMOD_NONE;
    SDL_PushEvent(&event);
}

/* Mirrors what SDL's own touch-to-mouse synthesis emits
 * (SDL_touch.c), so an injecting host and the SDL view produce the
 * same event stream:
 *   - a press sends motion first, then the button, at one point
 *   - a release sends the button only
 * Coordinates arrive in top-left window points. The host decides
 * which finger owns the pointer and how to clamp. */
void mkxp_injectPointerEvent(int x, int y, MKXPPointerPhase phase) {
    const Uint32 windowID = injectedWindowID();
    const bool pressed    = (phase == MKXP_POINTER_DOWN);
    const bool released   = (phase == MKXP_POINTER_UP ||
                             phase == MKXP_POINTER_CANCEL);

    if (pressed || phase == MKXP_POINTER_MOVE) {
        SDL_Event motion{};
        motion.type             = SDL_MOUSEMOTION;
        motion.motion.timestamp = SDL_GetTicks();
        motion.motion.windowID  = windowID;
        motion.motion.which     = SDL_TOUCH_MOUSEID;
        motion.motion.state     = pressed ? 0 : SDL_BUTTON_LMASK;
        motion.motion.x         = x;
        motion.motion.y         = y;
        SDL_PushEvent(&motion);
    }

    if (pressed || released) {
        SDL_Event button{};
        button.type             = pressed ? SDL_MOUSEBUTTONDOWN
                                          : SDL_MOUSEBUTTONUP;
        button.button.timestamp = SDL_GetTicks();
        button.button.windowID  = windowID;
        button.button.which     = SDL_TOUCH_MOUSEID;
        button.button.button    = SDL_BUTTON_LEFT;
        button.button.state     = pressed ? SDL_PRESSED : SDL_RELEASED;
        button.button.clicks    = 1;
        button.button.x         = x;
        button.button.y         = y;
        SDL_PushEvent(&button);
    }
}

static int keyEventWatcherFn(void * /*userdata*/, SDL_Event *event) {
    if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP) {
        int pressed = (event->type == SDL_KEYDOWN) ? 1 : 0;
        int sc = (int)event->key.keysym.scancode;
        s_keyEventCb.fire(sc, pressed);
    }
    return 1; // keep processing the event
}

void mkxp_setKeyEventCallback(mkxp_KeyEventCallback cb, void *userdata) {
    s_keyEventCb.set(cb, userdata);
    if (!s_keyWatcherInstalled) {
        SDL_AddEventWatch(keyEventWatcherFn, NULL);
        s_keyWatcherInstalled = true;
    }
}

// Text-input bridge

void mkxp_setTextInputModeCallback(mkxp_TextInputModeCallback cb, void *userdata) {
    s_textInputModeCb.set(cb, userdata);
}

// Called by EventThread::process whenever SDL_StartTextInput /
// SDL_StopTextInput is dispatched. Lives here so the BridgeCallback
// machinery stays internal to this TU. Also flips the
// `s_gameTextInputIntent` flag UI queries via
// `mkxp_isTextInputActive`.
void mkxp_fireTextInputModeCallback(int active) {
    s_gameTextInputIntent.store(active != 0, std::memory_order_release);
    s_textInputModeCb.fire(active);
}

// Managed-config directory accessors.

void mkxp_setManagedConfigDir(const char *path) {
    std::lock_guard<std::mutex> lock(s_managedConfigMutex);
    s_managedConfigDir = (path && *path) ? std::string(path) : std::string();
}

const char *mkxp_getManagedConfigDir(void) {
    std::lock_guard<std::mutex> lock(s_managedConfigMutex);
    /* Pointer is valid until s_managedConfigDir is reassigned;
     * callers should copy into std::string if they need to keep it. */
    return s_managedConfigDir.c_str();
}

void mkxp_setConfigOverlayJSON(const char *jsonUTF8) {
    std::lock_guard<std::mutex> lock(s_configOverlayMutex);
    if (!jsonUTF8 || !jsonUTF8[0]) {
        s_configOverlayJSON.clear();
        return;
    }
    constexpr size_t kMaxOverlayBytes = 1024 * 1024;
    const size_t len = std::strlen(jsonUTF8);
    if (len > kMaxOverlayBytes) {
        mkxp_debugLog("BRIDGE", "app_bridge.cpp",
                      "config overlay JSON exceeds 1 MiB; keeping previous state");
        return;
    }
    s_configOverlayJSON.assign(jsonUTF8, len);
}

const char *mkxp_getConfigOverlayJSON(void) {
    std::lock_guard<std::mutex> lock(s_configOverlayMutex);
    return s_configOverlayJSON.c_str();
}

void mkxp_setUserDataDirectory(const char *path) {
    std::lock_guard<std::mutex> lock(s_saveDirMutex);
    s_saveDir = (path && *path) ? std::string(path) : std::string();
}

const char *mkxp_getUserDataDirectory(void) {
    std::lock_guard<std::mutex> lock(s_saveDirMutex);
    return s_saveDir.c_str();
}

void mkxp_setSharedFontsDirectory(const char *path) {
    std::lock_guard<std::mutex> lock(s_sharedFontsDirMutex);
    s_sharedFontsDir = (path && *path) ? std::string(path) : std::string();
}

const char *mkxp_getSharedFontsDirectory(void) {
    std::lock_guard<std::mutex> lock(s_sharedFontsDirMutex);
    return s_sharedFontsDir.c_str();
}

void mkxp_setLauncherIdentity(const char *name, const char *version) {
    std::lock_guard<std::mutex> lock(s_launcherIdentityMutex);
    s_launcherIdentity = (name && *name) ? std::string(name) : std::string();
    s_launcherVersion = (version && *version) ? std::string(version) : std::string("unknown");
}

const char *mkxp_getLauncherIdentity(void) {
    std::lock_guard<std::mutex> lock(s_launcherIdentityMutex);
    return s_launcherIdentity.c_str();
}

const char *mkxp_getLauncherVersion(void) {
    std::lock_guard<std::mutex> lock(s_launcherIdentityMutex);
    return s_launcherVersion.c_str();
}

void mkxp_setCABundlePath(const char *path) {
    std::lock_guard<std::mutex> lock(s_caBundlePathMutex);
    s_caBundlePath = (path && *path) ? std::string(path) : std::string();
}

const char *mkxp_getCABundlePath(void) {
    std::lock_guard<std::mutex> lock(s_caBundlePathMutex);
    /* Pointer is valid until s_caBundlePath is reassigned; callers
     * should copy into std::string if they need to keep it. */
    return s_caBundlePath.c_str();
}

int mkxp_isTextInputActive(void) {
    /* Game-side intent (`Input.text_input = true`) rather than
     * SDL's flag, which the iOS backend auto-clears on transient
     * keyboard hides during presentation - even when WE'RE the one
     * presenting. Gating on SDL's flag would drop every keystroke
     * after the first hide flicker. */
    return s_gameTextInputIntent.load(std::memory_order_acquire) ? 1 : 0;
}

void mkxp_pushTextInput(const char *utf8) {
    if (!utf8 || !*utf8) return;

    // SDL_TEXTINPUT.text is char[32] (31 bytes payload + NUL). Split
    // input on UTF-8 character boundaries (never mid-codepoint) and
    // emit one event per chunk. Continuation bytes have
    // (b & 0xC0) == 0x80, so we walk forward 31 bytes then back off
    // to the most recent leading byte.
    constexpr size_t kMaxChunk = 31;

    if (s_sdlWindowID.load(std::memory_order_relaxed) == 0) {
        SDL_Window *w = SDL_GetGrabbedWindow();
        if (w) {
            s_sdlWindowID.store(SDL_GetWindowID(w), std::memory_order_relaxed);
        } else {
            s_sdlWindowID.store(1, std::memory_order_relaxed);
        }
    }
    Uint32 winID = s_sdlWindowID.load(std::memory_order_relaxed);

    const char *p = utf8;
    size_t remaining = std::strlen(utf8);
    while (remaining > 0) {
        size_t chunk = remaining > kMaxChunk ? kMaxChunk : remaining;

        // If we're truncating mid-string, walk back to the last
        // leading byte so we don't split a multi-byte codepoint.
        if (chunk < remaining) {
            while (chunk > 0 && (((unsigned char)p[chunk]) & 0xC0) == 0x80)
                --chunk;
            if (chunk == 0) {
                // Pathological input (continuation bytes only). Bail
                // out rather than infinite-loop.
                return;
            }
        }

        SDL_Event event{};
        event.type            = SDL_TEXTINPUT;
        event.text.timestamp  = SDL_GetTicks();
        event.text.windowID   = winID;
        std::memcpy(event.text.text, p, chunk);
        event.text.text[chunk] = '\0';
        SDL_PushEvent(&event);

        p         += chunk;
        remaining -= chunk;
    }
}

// Lifecycle callbacks

void mkxp_setEngineTerminatedCallback(mkxp_EngineTerminatedCallback cb, void *userdata) {
    s_engineTerminatedCb.set(cb, userdata);
}

void mkxp_setGameRectChangedCallback(mkxp_GameRectChangedCallback cb, void *userdata) {
    s_gameRectChangedCb.set(cb, userdata);
}

// Error message routing

void mkxp_setErrorMessage(const char *message) {
    {
        std::lock_guard<std::mutex> lock(s_errorMsgMutex);
        s_errorMessage = message ? message : "";
    }
    if (message && message[0]) {
        s_errorMsgCb.fire(message);
    }
}

#if MKXPZ_MOBILE
void mkxp_presentErrorAndWait(const char *message) {
    if (!message || !message[0])
        return;

    s_errorDismissed.store(false, std::memory_order_release);
    s_errorAwaitingDismiss.store(true, std::memory_order_release);
    leaveEngine();
    mkxp_setErrorMessage(message);

    if (pthread_main_np()) {
        // The host shows the alert from the main run loop. A blocked
        // main thread shows nothing, and the iOS watchdog stops the
        // app (0x8BADF00D). The dismiss can come from another thread,
        // and a CFRunLoopStop from there is lost when this thread is
        // not yet in the run, so the flag is read again every 50 ms.
        while (!s_errorDismissed.load(std::memory_order_acquire))
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, true);
    } else {
        std::unique_lock<std::mutex> lock(s_errorDismissMutex);
        s_errorDismissCV.wait(lock, [] {
            return s_errorDismissed.load(std::memory_order_acquire);
        });
    }
    comeBackToEngine();
    s_errorAwaitingDismiss.store(false, std::memory_order_release);
}

void mkxp_signalErrorDismissed(void) {
    if (!s_errorAwaitingDismiss.load(std::memory_order_acquire))
        return;
    {
        std::lock_guard<std::mutex> lock(s_errorDismissMutex);
        s_errorDismissed.store(true, std::memory_order_release);
    }
    s_errorDismissCV.notify_all();
}

void mkxp_presentInfoAndWait(const char *message) {
    if (!message || !message[0])
        return;

    // No host listener registered: fall back to the error channel
    // so the message is never silently swallowed.
    if (!s_infoMsgCb.isSet()) {
        mkxp_presentErrorAndWait(message);
        return;
    }

    s_infoDismissed.store(false, std::memory_order_release);
    s_infoAwaitingDismiss.store(true, std::memory_order_release);
    leaveEngine();
    s_infoMsgCb.fire(message);

    {
        std::unique_lock<std::mutex> lock(s_infoDismissMutex);
        s_infoDismissCV.wait(lock, [] {
            return s_infoDismissed.load(std::memory_order_acquire);
        });
    }
    comeBackToEngine();
    s_infoAwaitingDismiss.store(false, std::memory_order_release);
}

void mkxp_signalInfoDismissed(void) {
    if (!s_infoAwaitingDismiss.load(std::memory_order_acquire))
        return;
    {
        std::lock_guard<std::mutex> lock(s_infoDismissMutex);
        s_infoDismissed.store(true, std::memory_order_release);
    }
    s_infoDismissCV.notify_all();
}
#else
void mkxp_presentErrorAndWait(const char *message) {
    mkxp_setErrorMessage(message);
}

void mkxp_signalErrorDismissed(void) {}

void mkxp_presentInfoAndWait(const char *message) {
    if (message && message[0])
        s_infoMsgCb.fire(message);
}

void mkxp_signalInfoDismissed(void) {}
#endif

void mkxp_setInfoMessageCallback(mkxp_InfoMessageCallback cb, void *userdata) {
    s_infoMsgCb.set(cb, userdata);
}

void mkxp_reportSaveEvent(MKXPSaveEvent event, const char *detail) {
    const char *text = detail ? detail : "";
    mkxp_debugLog("SAVE", event == MKXP_SAVE_REPAIRED ? "save repaired" : "save load", text);
    s_saveEventCb.fire(event, text);
}

void mkxp_setSaveEventCallback(mkxp_SaveEventCallback cb, void *userdata) {
    s_saveEventCb.set(cb, userdata);
}

void mkxp_reportFatalError(const char *message) {
    const char *detail =
        (message && message[0]) ? message : "An unexpected error occurred.";
    mkxp_debugLog("FATAL", "app_bridge.cpp", detail);
#if MKXPZ_MOBILE
    mkxp_presentErrorAndWait(detail);
#else
    mkxp_setErrorMessage(detail);
#endif
    mkxp_requestTerminate();
}

void mkxp_setErrorMessageCallback(mkxp_ErrorMessageCallback cb, void *userdata) {
    s_errorMsgCb.set(cb, userdata);
    std::string pending;
    {
        std::lock_guard<std::mutex> lock(s_errorMsgMutex);
        pending = s_errorMessage;
    }
    if (!pending.empty()) {
        s_errorMsgCb.fire(pending.c_str());
    }
}

// Pause / Resume

void mkxp_requestPause(void) {
    setPauseRequested(true);
}

void mkxp_requestResume(void) {
    {
        std::lock_guard<std::mutex> lock(s_pauseMutex);
        setPauseRequested(false);
        s_paused.store(false, std::memory_order_release);
    }
    // Arm the one-shot frame-rendered signal so the UI knows when the
    // first post-resume frame has been swapped to the screen.
    s_needsFrameRenderedSignal.store(true, std::memory_order_release);
    s_pauseCV.notify_one();
}

void mkxp_checkPause(void) {
    if (!s_pauseRequested.load(std::memory_order_acquire))
        return;

    /* A resume can come after the read above. Under the lock it either
     * came first, and the engine runs on, or it wakes the wait below. */
    {
        std::lock_guard<std::mutex> lock(s_pauseMutex);
        if (!s_pauseRequested.load(std::memory_order_acquire))
            return;
        s_paused.store(true, std::memory_order_release);
    }

    /* Pause every AL_PLAYING source. We deliberately don't detach
     * the OpenAL context. Keeping it current avoids the audio blip
     * Apple's implementation produces on alcMakeContextCurrent. */
    if (SharedState::instance)
        SharedState::instance->audio().pauseSources();

    s_pausedCb.fire();

    /* Block until resume is requested. */
    std::unique_lock<std::mutex> lock(s_pauseMutex);
    s_pauseCV.wait(lock, [] {
        return !s_paused.load(std::memory_order_acquire);
    });

    if (!s_terminateRequested.load(std::memory_order_acquire)) {
        /* Normal resume; un-pause the sources we paused. */
        if (SharedState::instance)
            SharedState::instance->audio().resumeSources();
    }
    /* On terminate, sources stay paused until SharedState::finiInstance()
     * deletes them. */

    s_resumedCb.fire();
}

bool mkxp_isPauseRequested(void) {
    return s_pauseRequested.load(std::memory_order_acquire);
}

bool mkxp_isPaused(void) {
    return s_paused.load(std::memory_order_acquire);
}

void mkxp_setSnapshot(const unsigned char *data, int width, int height) {
    std::lock_guard<std::mutex> lock(s_snapshotMutex);
    size_t size = (size_t)width * height * 4;
    s_snapshotData.assign(data, data + size);
    s_snapshotWidth = width;
    s_snapshotHeight = height;
}

bool mkxp_copySnapshotRGBA(unsigned char *dest, int destSize, int *width, int *height) {
    std::lock_guard<std::mutex> lock(s_snapshotMutex);
    if (s_snapshotData.empty()) return false;
    int size = (int)s_snapshotData.size();
    if (destSize < size) return false;
    memcpy(dest, s_snapshotData.data(), size);
    if (width) *width = s_snapshotWidth;
    if (height) *height = s_snapshotHeight;
    return true;
}

bool mkxp_getSnapshotSize(int *width, int *height) {
    std::lock_guard<std::mutex> lock(s_snapshotMutex);
    if (s_snapshotData.empty()) return false;
    if (width) *width = s_snapshotWidth;
    if (height) *height = s_snapshotHeight;
    return true;
}

void mkxp_setPausedCallback(mkxp_PausedCallback cb, void *userdata) {
    s_pausedCb.set(cb, userdata);
}

void mkxp_setResumedCallback(mkxp_ResumedCallback cb, void *userdata) {
    s_resumedCb.set(cb, userdata);
}

void mkxp_setFrameRenderedCallback(mkxp_FrameRenderedCallback cb, void *userdata) {
    s_frameRenderedCb.set(cb, userdata);
}

void mkxp_signalFrameRendered(void) {
    if (s_needsFrameRenderedSignal.exchange(false, std::memory_order_acq_rel)) {
        s_frameRenderedCb.fire();
    }
}

// Per-game settings

void mkxp_applySessionConfig(const MKXPSessionConfig *config) {
    if (!config) return;
    if (config->managedConfigDir)
        mkxp_setManagedConfigDir(config->managedConfigDir);
    if (config->userDataDirectory)
        mkxp_setUserDataDirectory(config->userDataDirectory);
    if (config->sharedFontsDirectory)
        mkxp_setSharedFontsDirectory(config->sharedFontsDirectory);
    mkxp_applyPerGameSettings(config->verticalAlignment, mkxp_getPostloadEnabled());
}

void mkxp_setSetting(const char *key, const char *value) {
    if (!key || !value) return;
    const std::string k(key);
    const std::string v(value);
    const bool on = v == "1";
    if (k == "rubyVersion") {
        MKXPRubyVersion version = MKXP_RUBY_UNSET;
        if (v == "18") version = MKXP_RUBY_18;
        else if (v == "19") version = MKXP_RUBY_19;
        else if (v == "30") version = MKXP_RUBY_30;
        else if (v == "31") version = MKXP_RUBY_31;
        mkxp_setActiveRubyVersion(version);
    } else if (k == "syntaxTransform") {
        MKXPSyntaxTransformMode mode = MKXP_SYNTAX_TRANSFORM_UNSET;
        if (v == "legacy") mode = MKXP_SYNTAX_TRANSFORM_LEGACY;
        else if (v == "modern") mode = MKXP_SYNTAX_TRANSFORM_DISABLED;
        mkxp_setSyntaxTransformMode(mode);
    } else if (k == "postloadScripts") {
        s_postloadEnabled.store(on, std::memory_order_relaxed);
    } else if (k == "inGameKeyboard") {
        mkxp_setUseInGameKeyboard(on);
    } else if (k == "joiplayCompat") {
        mkxp_setJoiplayCompat(on);
    } else if (k == "networkEnabled") {
        mkxp_setNetworkEnabled(on);
    } else {
        Debug() << "mkxp_setSetting: unknown key" << k;
    }
}

const char *mkxp_getDetails(void) {
    static std::string details;
    details.clear();
    const int rgss = mkxp_getRGSSVersion();
    if (rgss > 0) details += "RGSS" + std::to_string(rgss) + "\n";
    const std::string ruby = mkxp_getRubyVersion();
    details += "Ruby " + ruby + "\n";
    // Only the patched Ruby 3.1 parser applies the syntax transform.
    if (ruby.rfind("3.1", 0) == 0) {
        switch (mkxp_getSyntaxTransformMode()) {
            case MKXP_SYNTAX_TRANSFORM_LEGACY: details += "Compatibility: legacy\n"; break;
            case MKXP_SYNTAX_TRANSFORM_DISABLED: details += "Compatibility: modern\n"; break;
            case MKXP_SYNTAX_TRANSFORM_CUSTOM: details += "Compatibility: custom\n"; break;
            default: break;
        }
    }
    const std::string angle = mkxp_getANGLEVersion();
    details += angle == "unknown" ? "ANGLE (Metal)" : "ANGLE " + angle + " (Metal)";
    const std::string device = mkxp_getMetalDeviceName();
    if (device != "unknown") details += "\n" + device;
    return details.c_str();
}

void mkxp_applyPerGameSettings(MKXPVerticalAlignment verticalAlignment,
                               bool postloadEnabled) {
    int oldAlign = s_verticalAlignment.exchange((int)verticalAlignment, std::memory_order_relaxed);
    if (oldAlign != (int)verticalAlignment) {
        s_safeAreaInsetsChanged.store(true, std::memory_order_release);
    }
    s_postloadEnabled.store(postloadEnabled, std::memory_order_relaxed);
}

MKXPVerticalAlignment mkxp_getVerticalAlignment(void) {
    return (MKXPVerticalAlignment)s_verticalAlignment.load(std::memory_order_relaxed);
}

bool mkxp_getPostloadEnabled(void) {
    return s_postloadEnabled.load(std::memory_order_relaxed);
}

void mkxp_setSyntaxTransformMode(MKXPSyntaxTransformMode mode) {
    s_syntaxTransformMode.store((int)mode, std::memory_order_release);
}

MKXPSyntaxTransformMode mkxp_getSyntaxTransformMode(void) {
    return (MKXPSyntaxTransformMode)s_syntaxTransformMode.load(std::memory_order_acquire);
}

// Per-game active Ruby version. See header for rationale. "Active"
// disambiguates from `mkxp_getRubyVersion()` which returns a string
// for DebugOverlayView - the names look similar. Treat with care.
static std::atomic<int> s_activeRubyVersion{MKXP_RUBY_UNSET};

void mkxp_setActiveRubyVersion(MKXPRubyVersion version) {
    s_activeRubyVersion.store((int)version, std::memory_order_release);
}

MKXPRubyVersion mkxp_getActiveRubyVersion(void) {
    return (MKXPRubyVersion)s_activeRubyVersion.load(std::memory_order_acquire);
}

void mkxp_setUseInGameKeyboard(bool enabled) {
    s_useInGameKeyboard.store(enabled, std::memory_order_release);
}

bool mkxp_getUseInGameKeyboard(void) {
    return s_useInGameKeyboard.load(std::memory_order_acquire);
}

void mkxp_setJoiplayCompat(bool enabled) {
    s_joiplayCompat.store(enabled, std::memory_order_release);
}

bool mkxp_getJoiplayCompat(void) {
    return s_joiplayCompat.load(std::memory_order_acquire);
}

void mkxp_setNetworkEnabled(bool enabled) {
    s_networkEnabled.store(enabled, std::memory_order_release);
}

bool mkxp_getNetworkEnabled(void) {
    return s_networkEnabled.load(std::memory_order_acquire);
}

void mkxp_setShowViewportBounds(bool enabled) {
    s_showViewportBounds.store(enabled, std::memory_order_relaxed);
}

bool mkxp_getShowViewportBounds(void) {
    return s_showViewportBounds.load(std::memory_order_relaxed);
}

void mkxp_setCheatsEnabled(bool enabled) {
    s_cheatsEnabled.store(enabled, std::memory_order_relaxed);
}

bool mkxp_getCheatsEnabled(void) {
    return s_cheatsEnabled.load(std::memory_order_relaxed);
}

void mkxp_setGameControllerCaptureEnabled(bool enabled) {
    s_gameControllerCaptureEnabled.store(enabled, std::memory_order_release);
}

bool mkxp_getGameControllerCaptureEnabled(void) {
    return s_gameControllerCaptureEnabled.load(std::memory_order_acquire);
}

void mkxp_setTouchMouseEnabled(bool enabled) {
    s_touchMouseEnabled.store(enabled, std::memory_order_release);
}

bool mkxp_getTouchMouseEnabled(void) {
    return s_touchMouseEnabled.load(std::memory_order_acquire);
}

void mkxp_setViewportBoundsColor(float r, float g, float b, float a) {
    s_vpBoundsR.store(r, std::memory_order_relaxed);
    s_vpBoundsG.store(g, std::memory_order_relaxed);
    s_vpBoundsB.store(b, std::memory_order_relaxed);
    s_vpBoundsA.store(a, std::memory_order_relaxed);
}

void mkxp_getViewportBoundsColor(float *r, float *g, float *b, float *a) {
    if (r) *r = s_vpBoundsR.load(std::memory_order_relaxed);
    if (g) *g = s_vpBoundsG.load(std::memory_order_relaxed);
    if (b) *b = s_vpBoundsB.load(std::memory_order_relaxed);
    if (a) *a = s_vpBoundsA.load(std::memory_order_relaxed);
}

void mkxp_setGLContextBroken(void) {
    s_glContextBroken.store(true, std::memory_order_release);
}

bool mkxp_isGLContextBroken(void) {
    return s_glContextBroken.load(std::memory_order_acquire);
}

// Runtime fast-forward multiplier. Read by
// Graphics::FPSLimiter::delay() each frame; > 1 scales target
// ticks-per-frame down so the engine paces N times faster. UI caps
// at 9 (deeper values can corrupt audio/input timers).
static std::atomic<int> s_fastForwardMultiplier{1};

void mkxp_setFastForwardMultiplier(int multiplier) {
    int clamped = multiplier > 1 ? multiplier : 1;
    s_fastForwardMultiplier.store(clamped, std::memory_order_release);
    if (mkxp_debugLogEnabled()) {
        char msg[64];
        std::snprintf(msg, sizeof(msg),
                      "fastForwardMultiplier set to %d (requested %d)",
                      clamped, multiplier);
        mkxp_debugLog("INFO", "fast-forward", msg);
    }
}

int mkxp_getFastForwardMultiplier(void) {
    return s_fastForwardMultiplier.load(std::memory_order_acquire);
}

// A Ruby VM cannot be torn down and started again in one process, so
// this core cannot kill a game. A host must not call this.
void mkxp_killSession(void) {
    fprintf(stderr, "[mkxp] killSession: this core cannot kill a game\n");
    abort();
}

// Per-session bridge state reset. Each entry maps to one of the
// per-game atomics declared above that would otherwise carry across
// game launches (the host process keeps running, only the RGSS
// thread re-spawns). When adding a new per-session bridge, reset it
// here alongside the existing block.
//
// Globally-owned state (viewport bounds debug overlay + color) is
// not reset here. Bridges already re-set on every launch by the
// host (managed config dir, syntax transform mode, in-game keyboard,
// per-game settings via applyPerGameSettings, debug log path) don't
// need an entry either - the per-launch write is a stronger
// guarantee than a "back to default" reset.
void mkxp_resetSessionState(void) {
    s_fastForwardMultiplier.store(1, std::memory_order_release);
    s_cheatsEnabled.store(false, std::memory_order_release);
    // A crash-terminated session must not leak its viewport region
    // into the next game's boot. The host re-sets it per session.
    s_hostViewportRegion.store(0, std::memory_order_relaxed);
    s_safeAreaInsetsChanged.store(true, std::memory_order_release);
}

void mkxp_setDebugLogPath(const char *path) {
    std::lock_guard<std::mutex> lock(s_debugLogMutex);
    // Close previous file handle if open
    if (s_debugLogFile) {
        fclose(s_debugLogFile);
        s_debugLogFile = nullptr;
    }
    s_debugLogPath = (path && path[0]) ? path : "";
    // Open new file handle for the session
    if (!s_debugLogPath.empty()) {
        s_debugLogFile = fopen(s_debugLogPath.c_str(), "a");
    }
}

// Fast path so hot-path callers (resize handler, frame boundary)
// skip formatting when logging is off.
int mkxp_debugLogEnabled(void) {
    // Reading s_debugLogFile lock-free is fine: the worst case is a
    // dropped or duplicated log line if the file is opening/closing
    // on another thread, which is acceptable for a debug log.
    return s_debugLogFile ? 1 : 0;
}

void mkxp_debugLog(const char *tag, const char *source, const char *message) {
    std::lock_guard<std::mutex> lock(s_debugLogMutex);
    if (!s_debugLogFile) return;
    fprintf(s_debugLogFile, "[%s] (%s) %s\n", tag, source, message);
    // Flush immediately: without it, RGSS thread writes sit in
    // stdio's per-FILE buffer until the next session's
    // setDebugLogPath closes the file, hiding diagnostics from any
    // mid-session inspection.
    fflush(s_debugLogFile);
}

} // extern "C"

// Internal helper. Called by binding-mri.cpp, not part of the C bridge.
// Returns empty string if logging is disabled.
std::string mkxp_getDebugLogPath(void) {
    std::lock_guard<std::mutex> lock(s_debugLogMutex);
    return s_debugLogPath;
}

#endif /* MKXPZ_MOBILE */
