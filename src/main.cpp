/*
** main.cpp
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

#include <atomic>

#include <alc.h>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_sound.h>
#include <SDL_ttf.h>

#include <assert.h>
#include <pthread/qos.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <regex>
#include <climits>

#include "app_bridge.h"
#include "ios_fatal_report.h"
#include <CoreFoundation/CoreFoundation.h>
#include <EGL/egl.h>

#include "binding.h"
#include "sharedstate.h"
#include "eventthread.h"
#include "display/graphics.h"
#include "util/debugwriter.h"
#include "util/exception.h"
#include "display/gl/gl-debug.h"
#include "display/gl/gl-fun.h"
#include "display/gl/gl-util.h"

#include "filesystem/filesystem.h"

#include "system/system.h"

#include <Availability.h>
#include <TargetConditionals.h>

static void rgssThreadError(RGSSThreadData *rtData, const std::string &msg);
static void showInitError(const std::string &msg);
static bool initANGLE(SDL_Window *win);
static void teardownANGLE();

static inline const char *glGetStringInt(GLenum name) {
  return (const char *)gl.GetString(name);
}

/* Process-lifetime buffers populated by printGLInfo() once the EGL
 * context is current. Exposed to the app layer via
 * mkxp_getANGLEVersion() / mkxp_getMetalDeviceName() so the SwiftUI
 * debug overlay can show them without re-parsing GL strings. */
static char s_angleVersion[128]    = "unknown";
static char s_metalDeviceName[128] = "unknown";

extern "C" const char *mkxp_getANGLEVersion(void) {
    return s_angleVersion;
}

extern "C" const char *mkxp_getMetalDeviceName(void) {
    return s_metalDeviceName;
}

static void printGLInfo() {
    const std::string renderer(glGetStringInt(GL_RENDERER));
    const std::string version(glGetStringInt(GL_VERSION));
    std::regex rgx("ANGLE \\((.+), ANGLE Metal Renderer: (.+), Version (.+)\\)");

    std::smatch matches;
    if (std::regex_search(renderer, matches, rgx)) {

        Debug() << "Backend           :" << "Metal";
        Debug() << "Metal Device      :" << matches[2] << "(" + matches[1].str() + ")";
        Debug() << "Renderer Version  :" << matches[3].str();

        /* Cache the Metal device name for the debug overlay.
         * matches[2] is the device string (e.g. "Apple A15 GPU"). */
        snprintf(s_metalDeviceName, sizeof(s_metalDeviceName), "%s",
                 matches[2].str().c_str());

        std::smatch vmatches;
        if (std::regex_search(version, vmatches, std::regex("\\(ANGLE (.+) git hash: .+\\)"))) {
            Debug() << "ANGLE Version     :" << vmatches[1].str();
            /* Cache the ANGLE version for the debug overlay. */
            snprintf(s_angleVersion, sizeof(s_angleVersion), "%s",
                     vmatches[1].str().c_str());
        }
    } else {
      Debug() << "Backend      :" << "OpenGL";
      Debug() << "GL Vendor    :" << glGetStringInt(GL_VENDOR);
      Debug() << "GL Renderer  :" << renderer;
      Debug() << "GL Version   :" << version;
      Debug() << "GLSL Version :" << glGetStringInt(GL_SHADING_LANGUAGE_VERSION);
    }

    GLint maxTexSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    Debug() << "Max Tex Size :" << maxTexSize;
}

/* ANGLE is the only renderer on iOS. The screen FBO is captured
 * once during initANGLE() (typically 0 under Metal) and reused for
 * all sessions, since the window and EGL context persist.
 * Re-querying GL_FRAMEBUFFER_BINDING on subsequent sessions would
 * be unsafe because SharedState::finiInstance deletes all game
 * FBOs and the binding may not be what we expect. */
static GLuint s_screenFBO = 0;
EGLDisplay s_eglDisplay = EGL_NO_DISPLAY;
EGLSurface s_eglSurface = EGL_NO_SURFACE;
EGLContext s_eglContext = EGL_NO_CONTEXT;
static SDL_MetalView s_metalView = nullptr;

/* Thin wrappers over EGL that preserve the old SDL_GL_* signatures.
 * `ctx` is a sentinel: the EGL context pointer cast to SDL_GLContext
 * so it can be stored in the RGSSThreadData struct. */
static void mkxpGL_MakeCurrent(SDL_Window * /*win*/, SDL_GLContext ctx) {
    if (ctx)
        eglMakeCurrent(s_eglDisplay, s_eglSurface, s_eglSurface, s_eglContext);
    else
        eglMakeCurrent(s_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

static void mkxpGL_SwapWindow(SDL_Window * /*win*/) {
    eglSwapBuffers(s_eglDisplay, s_eglSurface);
}

// Query the EGL surface for real pixel dimensions. SDL_GL_GetDrawableSize
// would return logical points under ANGLE (no SDL GL context is bound).
void mkxpGL_GetDrawableSize(SDL_Window * /*win*/, int *w, int *h) {
    EGLint eglW = 0, eglH = 0;
    eglQuerySurface(s_eglDisplay, s_eglSurface, EGL_WIDTH, &eglW);
    eglQuerySurface(s_eglDisplay, s_eglSurface, EGL_HEIGHT, &eglH);
    if (w) *w = eglW;
    if (h) *h = eglH;
}

// Called on rotation / resize. eglQuerySurface returns a drawable size
// cached during the last obtainNextDrawable call (pre-rotation). SDL's
// Metal view updates the layer's drawable size in layoutSubviews, which
// UIKit runs before SDL sends the resize event.
void mkxpGL_RefreshDrawableSize(SDL_Window *win, int *w, int *h) {
    SDL_Metal_GetDrawableSize(win, w, h);
}

/* Single-shot RGSS thread.
 *
 * Runs the game session once and exits. Once the user returns to
 * the Library, the engine is fully torn down and the user must
 * close + reopen the host app to play another game (per App Store
 * guideline 2.5.1, we can't terminate the process programmatically).
 *
 * Ruby's VM has process-global state (signal handlers, atexit,
 * parser, symbol table) that doesn't unwind cleanly via
 * `ruby_cleanup`, so even if the persistent-thread architecture
 * worked we couldn't ruby_init twice in one process. Single-shot
 * matches what actually happens at runtime. */
static void rgssThreadError(RGSSThreadData *rtData, const std::string &msg);
static void rgssThreadShutdown(RGSSThreadData *threadData);

extern "C" void mkxp_noteRgssThreadFailure(void *userdata, const char *message) {
  if (!userdata)
    return;
  const char *detail =
      message ? message : "An unexpected error occurred.";
#if MKXPZ_MOBILE
  mkxp_presentErrorAndWait(detail);
#endif
  rgssThreadError(static_cast<RGSSThreadData *>(userdata), detail);
}

extern "C" void mkxp_rgssThreadShutdownAfterFailure(void *userdata) {
  if (!userdata)
    return;
  rgssThreadShutdown(static_cast<RGSSThreadData *>(userdata));
}

static void rgssThreadShutdown(RGSSThreadData *threadData) {
  ALCcontext *alcCtx = threadData->alcCtx;
  if (SharedState::instance) {
    shState->graphics().detachAllDisposables();
    alcMakeContextCurrent(alcCtx);
    SharedState::finiInstance();
  }
  mkxpGL_MakeCurrent(threadData->window, NULL);
  alcMakeContextCurrent(NULL);
}

static int rgssThreadFunImpl(void *userdata) {
  RGSSThreadData *threadData = static_cast<RGSSThreadData *>(userdata);

  mkxpGL_MakeCurrent(threadData->window, threadData->glContext);

  /* Set the screen framebuffer ID and reset the binding tracker. */
  FBO::screenFramebufferID = FBO::ID(s_screenFBO);
  gl.BindFramebuffer(GL_FRAMEBUFFER, FBO::screenFramebufferID.gl);
  FBO::boundFramebufferID = FBO::screenFramebufferID;

  /* AL context; activate on this thread. */
  ALCcontext *alcCtx = threadData->alcCtx;
  alcMakeContextCurrent(alcCtx);

  SharedState::initInstance(threadData);

  mkxp_setGameReady();

#ifdef MKXPZ_BUILD_XCODE
  /* Log which Ruby this session dispatches to before any Ruby code
   * runs. Useful for verifying that `mkxp_setActiveRubyVersion()`
   * from the host side reaches the right per-version binding. */
  {
      const char *label = "3.1 (legacy direct-link)";
      switch (mkxp_getActiveRubyVersion()) {
      case MKXP_RUBY_18: label = "1.8 (mkxp18-merged.o)"; break;
      case MKXP_RUBY_19: label = "1.9 (mkxp19-merged.o)"; break;
      case MKXP_RUBY_30: label = "3.1 (3.0 detected, routed to 3.1+Legacy)"; break;
      case MKXP_RUBY_31: label = "3.1 (mkxp31-merged.o)"; break;
      case MKXP_RUBY_UNSET: default: label = "3.1 (legacy, UNSET fallback)"; break;
      }
      mkxp_debugLog("RUBY", "main.cpp", label);
  }
#endif

  /* Run game scripts.
   *
   * Dispatch through `getActiveScriptBinding()` (binding.h) instead
   * of the global `scriptBinding` directly so the host's
   * `mkxp_setActiveRubyVersion()` setting picks which Ruby
   * interpreter runs. The default keeps using the legacy 3.1
   * binding via the global pointer; other versions go through the
   * per-version `_mkxp_get_script_binding_NN()` entry points
   * exported by their merged .o files. */
  getActiveScriptBinding()->execute();

  rgssThreadShutdown(threadData);

  threadData->rqTermAck.set();
  threadData->ethread->requestTerminate();

  return 0;
}

int rgssThreadFun(void *userdata) {
  /* This thread runs both the game's logic and its rendering (RGSS
   * couples them), yet SDL spawns it with no QoS class. On
   * asymmetric A-series chips the scheduler is free to keep
   * unclassified threads on efficiency cores, and on A13-class
   * devices that alone holds heavy games well under their target
   * frame rate. Declare the thread user-interactive - it paces
   * every visible frame - so it qualifies for performance cores. */
  pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#ifdef MKXPZ_BUILD_XCODE
  mkxp_debugLog("INFO", "main.cpp",
                qos_class_self() == QOS_CLASS_USER_INTERACTIVE
                    ? "rgss thread QoS: user-interactive"
                    : "rgss thread QoS: elevation FAILED");
#endif
#if MKXPZ_MOBILE
  return mkxp_guardedRgssThreadMain(userdata, rgssThreadFunImpl);
#else
  return rgssThreadFunImpl(userdata);
#endif
}

static void printRgssVersion(int ver) {
  const char *const makers[] = {"", "XP", "VX", "VX Ace"};

  char buf[128];
  snprintf(buf, sizeof(buf), "RGSS version %d (RPG Maker %s)", ver,
           makers[ver]);

  Debug() << buf;
}

static void initSyntaxTransform(Config &conf) {
#ifdef MKXPZ_HAVE_SYNTAX_TRANSFORM_PATCHES
  extern unsigned int mkxp_syntax_transform_target_ruby_version_major, mkxp_syntax_transform_target_ruby_version_minor, mkxp_syntax_transform_target_ruby_version_teeny;
#endif // MKXPZ_HAVE_SYNTAX_TRANSFORM_PATCHES

  // Host-bridge override. The host app sets this per-game so
  // syntaxTransform doesn't have to live inside mkxp.json (which is
  // meant to mirror the developer's engine-config layer; mixing
  // host-managed keys in there bleeds into mkxp.original.json
  // snapshots and the per-game-defaults UI). When the bridge is
  // MKXP_SYNTAX_TRANSFORM_UNSET (the default), use whatever
  // Config::read produced from mkxp.json so desktop / test-harness
  // builds behave exactly as before.
  MKXPSyntaxTransformMode hostMode = mkxp_getSyntaxTransformMode();
  if (hostMode != MKXP_SYNTAX_TRANSFORM_UNSET)
    conf.syntaxTransform = hostMode;

  char buf[128];

  switch (conf.syntaxTransform) {
    case MKXP_SYNTAX_TRANSFORM_UNSET:        // unreachable post-override; treat as disabled
    case MKXP_SYNTAX_TRANSFORM_DISABLED:
    default:
      conf.syntaxTransformCustomVersionMajor = INT_MAX;
      conf.syntaxTransformCustomVersionMinor = INT_MAX;
      conf.syntaxTransformCustomVersionTeeny = INT_MAX;
      snprintf(buf, sizeof(buf), "Disabled");
      break;
    case MKXP_SYNTAX_TRANSFORM_CUSTOM:
      conf.syntaxTransformCustomVersionMajor = std::max(0, conf.syntaxTransformCustomVersionMajor);
      conf.syntaxTransformCustomVersionMinor = std::max(0, conf.syntaxTransformCustomVersionMinor);
      conf.syntaxTransformCustomVersionTeeny = std::max(0, conf.syntaxTransformCustomVersionTeeny);
      snprintf(buf, sizeof(buf), "Ruby %u.%u.%u", conf.syntaxTransformCustomVersionMajor, conf.syntaxTransformCustomVersionMinor, conf.syntaxTransformCustomVersionTeeny);
      break;
    case MKXP_SYNTAX_TRANSFORM_LEGACY:
      conf.syntaxTransformCustomVersionMajor = 1;
      conf.syntaxTransformCustomVersionMinor = conf.rgssVersion >= 3 ? 9 : 8;
      conf.syntaxTransformCustomVersionTeeny = conf.rgssVersion >= 3 ? 2 : 1;
      snprintf(buf, sizeof(buf), "Compatibility mode (Ruby %u.%u.%u)", conf.syntaxTransformCustomVersionMajor, conf.syntaxTransformCustomVersionMinor, conf.syntaxTransformCustomVersionTeeny);
      break;
  }

#ifdef MKXPZ_HAVE_SYNTAX_TRANSFORM_PATCHES
  mkxp_syntax_transform_target_ruby_version_major = conf.syntaxTransformCustomVersionMajor == INT_MAX ? -1 : conf.syntaxTransformCustomVersionMajor;
  mkxp_syntax_transform_target_ruby_version_minor = conf.syntaxTransformCustomVersionMinor == INT_MAX ? -1 : conf.syntaxTransformCustomVersionMinor;
  mkxp_syntax_transform_target_ruby_version_teeny = conf.syntaxTransformCustomVersionTeeny == INT_MAX ? -1 : conf.syntaxTransformCustomVersionTeeny;
  Debug() << "Syntax transform:" << buf;
#else
  // The user configured a syntax-transform mode but this build was
  // compiled against an unpatched Ruby runtime. The setting has no
  // effect; make it loud in the logs so the mismatch doesn't look
  // like a silent misconfiguration.
  if (conf.syntaxTransform != MKXP_SYNTAX_TRANSFORM_DISABLED)
    Debug() << "Syntax transform: requested" << buf << "but patches"
               " are not compiled in (MKXPZ_HAVE_SYNTAX_TRANSFORM_PATCHES"
               " undefined); setting will be ignored.";
#endif // MKXPZ_HAVE_SYNTAX_TRANSFORM_PATCHES
}

static void rgssThreadError(RGSSThreadData *rtData, const std::string &msg) {
  rtData->rgssErrorMsg = msg;
#if !MKXPZ_MOBILE
  mkxp_setErrorMessage(msg.c_str());
#endif
  rtData->ethread->requestTerminate();
  rtData->rqTermAck.set();
}

static void showInitError(const std::string &msg) {
  Debug() << msg;
#if MKXPZ_MOBILE
  mkxp_reportFatalError(msg.c_str());
#else
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "mkxp-z", msg.c_str(), 0);
#endif
}

// Initialize SDL and its subsidiary libs in the order they depend on
// each other: SDL core (video+controller+timer) -> user events ->
// SDL_image -> SDL_ttf -> SDL_sound. If anything fails, tear down
// the previous ones before returning false so the process leaves
// no resources dangling even on init failure.
//
// Returns true on full success. On failure the caller should return
// from main() - an error message box has already been shown via
// showInitError().
static bool initSDLLibs() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) < 0) {
    showInitError(std::string("Error initializing SDL: ") + SDL_GetError());
    return false;
  }

  if (!EventThread::allocUserEvents()) {
    showInitError("Error allocating SDL user events");
    SDL_Quit();
    return false;
  }

  const int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
  if (IMG_Init(imgFlags) != imgFlags) {
    showInitError(std::string("Error initializing SDL_image: ") + SDL_GetError());
    SDL_Quit();
    return false;
  }

  if (TTF_Init() < 0) {
    showInitError(std::string("Error initializing SDL_ttf: ") + SDL_GetError());
    IMG_Quit();
    SDL_Quit();
    return false;
  }

  if (Sound_Init() == 0) {
    showInitError(std::string("Error initializing SDL_sound: ") + Sound_GetError());
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return false;
  }

  return true;
}

// Reverse order of initSDLLibs. Safe to call even if only some libs
// were initialized (each *_Quit is idempotent-ish; SDL_sound's Quit
// is Sound_Quit).
static void shutdownSDLLibs() {
  Sound_Quit();
  TTF_Quit();
  IMG_Quit();
  SDL_Quit();
}

/* Create the persistent SDL window. No SDL_WINDOW_OPENGL flag
 * because ANGLE draws into the CAMetalLayer of SDL's Metal view.
 * Returns nullptr on failure after posting an error. */
static SDL_Window *createPersistentWindow(const Config &initConf) {
  Uint32 winFlags = SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_ALLOW_HIGHDPI;
#if !TARGET_OS_IPHONE
  winFlags |= SDL_WINDOW_RESIZABLE;
#endif

  /* Allow all orientations. Without this, SDL infers supported
   * orientations from the window w/h: a landscape-shaped game
   * (e.g. 640x480) would lock the window to landscape only,
   * preventing portrait gameplay. */
  SDL_SetHint(SDL_HINT_ORIENTATIONS,
              "Portrait LandscapeLeft LandscapeRight");

  SDL_Window *win = SDL_CreateWindow(
      initConf.windowTitle.c_str(), SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED, initConf.defScreenW,
      initConf.defScreenH, winFlags);

  if (!win)
    showInitError(std::string("Error creating window: ") + SDL_GetError());

  return win;
}

/* Initialize ANGLE and yield the EGL context pointer. Must be called
 * from the main thread; the RGSS thread claims the context at session
 * start. Returns nullptr on failure. */
static SDL_GLContext createPersistentGL(SDL_Window *win) {
  if (!initANGLE(win))
    return nullptr;

  /* Use the EGL context pointer as a non-null sentinel so
   * mkxpGL_MakeCurrent can distinguish bind from unbind. */
  SDL_GLContext ctx = (SDL_GLContext)s_eglContext;

  /* Release from main thread - the RGSS thread will claim it.
   * EGL contexts can only be current on one thread at a time. */
  mkxpGL_MakeCurrent(win, NULL);
  Debug() << "Using ANGLE (Metal)";
  return ctx;
}

#if TARGET_OS_IPHONE
/* iOS refuses audio to an app in the background (error -16980,
 * insufficient privileges), because the host has no audio background
 * mode, and alcOpenDevice then returns NULL. The app can go to the
 * background after the check, so an open that fails after a move to
 * the background waits for the foreground and tries again. */
static ALCdevice *openAudioDevice() {
  for (;;) {
    unsigned moves = mkxp_sys::backgroundMoveCount();
    mkxp_sys::waitUntilForeground();
    if (ALCdevice *dev = alcOpenDevice(0))
      return dev;
    if (mkxp_sys::backgroundMoveCount() == moves)
      return nullptr;
    Debug() << "The app went to the background while the audio device opened. Trying again.";
  }
}
#endif

/* Open the default OpenAL device and create a context on it. The
 * device pointer is written to *outDev; the context to *outCtx.
 * Returns false on failure. */
static bool createPersistentAudio(ALCdevice **outDev, ALCcontext **outCtx) {
#if TARGET_OS_IPHONE
  ALCdevice *dev = openAudioDevice();
#else
  ALCdevice *dev = alcOpenDevice(0);
#endif
  if (!dev) {
    showInitError("Could not detect an available audio device.");
    return false;
  }

  ALCcontext *ctx = alcCreateContext(dev, 0);
  if (ctx)
    alcMakeContextCurrent(ctx);

  *outDev = dev;
  *outCtx = ctx;
  return true;
}

/* Wait for rqTermAck with run-loop pumping so SwiftUI stays responsive.
 * Gives up after 10 seconds. Returns true if ack received. */
static bool waitForRGSSAck(RGSSThreadData &rtData) {
  for (int i = 0; i < 1000; ++i) {
    if (rtData.rqTermAck) {
      Debug() << "RGSS thread ack'd request after" << i * 10 << "ms";
      return true;
    }
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
  }
  return false;
}

/* EngineHost owns the process-wide engine resources (window, EGL
 * context, OpenAL device/context, RGSS thread). Lifecycle:
 * init() -> runSession() -> shutdown(). The three phases must be
 * called in that order; init() returning false means the host is
 * already torn down and runSession/shutdown must not be called.
 * Single-shot: once runSession returns, the engine is dead and the
 * user has to relaunch the host app to play another game. */
class EngineHost {
public:
  bool init(int argc, char *argv[]);
  void runSession(int argc, char *argv[]);
  void shutdown();

private:
  SDL_Window      *persistWin_    = nullptr;
  SDL_GLContext    persistGLCtx_  = nullptr;
  ALCdevice       *persistAlcDev_ = nullptr;
  ALCcontext      *persistAlcCtx_ = nullptr;
  SDL_Thread      *rgssThread_    = nullptr;
  SDL_DisplayMode  displayMode_{};
  char             dataDir_[512]{};
};

bool EngineHost::init(int argc, char *argv[]) {
#if MKXPZ_MOBILE
  mkxp_installFatalErrorHandlers();
#endif
  /* FIRST LAUNCH: wait for Library UI before SDL_Init. SDL_Init
   * creates a window that would cover the Library UI, so we wait
   * for the user to pick a game first. */
  const char *selectedPath = mkxp_waitForGamePath();
  if (selectedPath && selectedPath[0])
    snprintf(dataDir_, sizeof(dataDir_), "%s", selectedPath);

  SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
  SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
  SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
  SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

  if (!initSDLLibs())
    return false;

  /* Working directory for the first game (needed to read Config). */
  mkxp_fs::setCurrentDirectory(dataDir_);

  Config initConf;
  initConf.read(argc, argv);

  persistWin_ = createPersistentWindow(initConf);
  if (!persistWin_) {
    shutdownSDLLibs();
    return false;
  }
  mkxp_setGameSDLWindow(persistWin_);

  persistGLCtx_ = createPersistentGL(persistWin_);
  if (!persistGLCtx_) {
    teardownANGLE();
    mkxp_setGameSDLWindow(nullptr);
    SDL_DestroyWindow(persistWin_);
    persistWin_ = nullptr;
    shutdownSDLLibs();
    return false;
  }

  if (!createPersistentAudio(&persistAlcDev_, &persistAlcCtx_)) {
    teardownANGLE();
    mkxp_setGameSDLWindow(nullptr);
    SDL_DestroyWindow(persistWin_);
    persistWin_ = nullptr;
    persistGLCtx_ = nullptr;
    shutdownSDLLibs();
    return false;
  }

  SDL_GetDisplayMode(0, 0, &displayMode_);

  return true;
}

void EngineHost::runSession(int argc, char *argv[]) {
  Config conf;
  conf.read(argc, argv);

  if (conf.windowTitle.empty())
    conf.windowTitle = conf.game.title;

  assert(conf.rgssVersion >= 1 && conf.rgssVersion <= 3);
  printRgssVersion(conf.rgssVersion);

  initSyntaxTransform(conf);

  SDL_SetWindowTitle(persistWin_, conf.windowTitle.c_str());

  if (!displayMode_.refresh_rate)
    conf.syncToRefreshrate = false;

  EventThread eventThread;

  RGSSThreadData rtData(&eventThread, argv[0], persistWin_, persistAlcDev_,
                        persistAlcCtx_, displayMode_.refresh_rate,
                        mkxp_sys::getScalingFactor(), conf, persistGLCtx_);

  int winW, winH, drwW, drwH;
  SDL_GetWindowSize(persistWin_, &winW, &winH);
  rtData.windowSizeMsg.post(Vec2i(winW, winH));

  mkxpGL_GetDrawableSize(persistWin_, &drwW, &drwH);
  rtData.drawableSizeMsg.post(Vec2i(drwW, drwH));

  rtData.bindingUpdateMsg.post(loadBindings(conf));

  SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);

  /* 16 MB virtual stack. Ruby 1.8 / 1.9 use a conservative GC that
   * walks the entire machine stack on every collection and copies
   * the live stack into a fiber buffer on every Fiber switch
   * (cont_save_machine_stack memcpys machine_stack_start -
   * machine_stack_end). Deep RGSS3 games (MOG title plugins, Window
   * class hierarchy, fibers from Show Choices) regularly need 4-8 MB
   * of live stack; the previous 1 MB cap caused SP to drop into the
   * stack guard page mid-fiber-switch, surfacing as either an xzone
   * malloc trap or an ASan memcpy out-of-bounds depending on heap
   * layout. iOS commits stack pages lazily so the 16 MB only costs
   * virtual address space, not physical RAM. */
  rgssThread_ = SDL_CreateThreadWithStackSize(rgssThreadFun, "rgss",
                                               16 * 1024 * 1024, &rtData);

  /* Run event processing until the game ends. */
  eventThread.process(rtData);

  /* Ask the RGSS thread to stop, then wait for it. */
  rtData.rqTerm.set();
  const bool acked = waitForRGSSAck(rtData);

  if (!acked) {
    /* The RGSS thread is stuck (never called checkShutdown). Tell
     * the UI to ask the user to force-close the app. */
    mkxp_setEngineHung();
    mkxp_setErrorMessage(
        "The game stopped responding. Close the app from the app switcher.");
  }

  if (!rtData.rgssErrorMsg.empty()) {
    Debug() << rtData.rgssErrorMsg;
#if !MKXPZ_MOBILE
    mkxp_setErrorMessage(rtData.rgssErrorMsg.c_str());
#endif
  }

  if (rgssThread_) {
    SDL_WaitThread(rgssThread_, 0);
    rgssThread_ = nullptr;
  }

  eventThread.cleanup();

  Debug() << "Game session ended.";
  mkxp_setEngineTerminated();
}

void EngineHost::shutdown() {
  if (persistGLCtx_) {
    /* persistGLCtx_ is the ANGLE EGL context cast to SDL_GLContext
     * (see createPersistentGL), not an SDL-created context.
     * SDL_GL_DeleteContext would hand it to SDL's UIKit EAGL
     * backend, which dereferences it as an Objective-C view object
     * and crashes. Tear it down through EGL, like the init()
     * failure path does. */
    teardownANGLE();
    persistGLCtx_ = nullptr;
  }
  alcMakeContextCurrent(NULL);
  if (persistAlcCtx_) {
    alcDestroyContext(persistAlcCtx_);
    persistAlcCtx_ = nullptr;
  }
  if (persistAlcDev_) {
    alcCloseDevice(persistAlcDev_);
    persistAlcDev_ = nullptr;
  }
  if (persistWin_) {
    mkxp_setGameSDLWindow(nullptr);
    SDL_DestroyWindow(persistWin_);
    persistWin_ = nullptr;
  }
}

// The host enters through mkxp_run_app, which calls SDL_main. SDL_main.h
// renames main to SDL_main on iOS, but not on macOS.
#if MKXPZ_MOBILE && !defined(main)
#define main SDL_main
#endif

int main(int argc, char *argv[]) {
  try {
    EngineHost host;
    if (!host.init(argc, argv))
      return 0;
    host.runSession(argc, argv);
    host.shutdown();

    Debug() << "Shutting down.";
    shutdownSDLLibs();
    return 0;
  } catch (const Exception &exc) {
    Debug() << "FATAL uncaught Exception:" << exc.msg;
#if MKXPZ_MOBILE
    mkxp_reportFatalError(exc.msg.c_str());
#endif
    return 1;
  } catch (const std::exception &e) {
    Debug() << "FATAL uncaught std::exception:" << e.what();
#if MKXPZ_MOBILE
    mkxp_reportFatalError(e.what());
#endif
    return 1;
  } catch (...) {
    Debug() << "FATAL unknown exception caught";
#if MKXPZ_MOBILE
    mkxp_reportFatalError("An unexpected engine error occurred.");
#endif
    return 1;
  }
}

static bool initANGLE(SDL_Window *win) {
  s_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (s_eglDisplay == EGL_NO_DISPLAY) {
    showInitError("ANGLE: eglGetDisplay failed");
    return false;
  }

  EGLint major, minor;
  if (!eglInitialize(s_eglDisplay, &major, &minor)) {
    showInitError("ANGLE: eglInitialize failed");
    return false;
  }
  Debug() << "ANGLE: EGL" << major << "." << minor;

  EGLint configAttribs[] = {
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
    EGL_RED_SIZE,        8,
    EGL_GREEN_SIZE,      8,
    EGL_BLUE_SIZE,       8,
    EGL_ALPHA_SIZE,      8,
    EGL_DEPTH_SIZE,      0,
    EGL_STENCIL_SIZE,    0,
    EGL_NONE
  };
  EGLConfig eglConfig;
  EGLint numConfigs;
  if (!eglChooseConfig(s_eglDisplay, configAttribs, &eglConfig, 1, &numConfigs) || numConfigs == 0) {
    showInitError("ANGLE: eglChooseConfig failed");
    return false;
  }

  // ANGLE draws straight into a CAMetalLayer that it gets as the native
  // window. With any other CALayer, it adds its own sublayer and copies the
  // frame only once, so a view that UIKit places later leaves the picture
  // off screen.
  s_metalView = SDL_Metal_CreateView(win);
  if (!s_metalView) {
    showInitError(std::string("ANGLE: SDL_Metal_CreateView failed: ") + SDL_GetError());
    return false;
  }
  EGLNativeWindowType nativeWindow = (EGLNativeWindowType)SDL_Metal_GetLayer(s_metalView);

  s_eglSurface = eglCreateWindowSurface(s_eglDisplay, eglConfig, nativeWindow, NULL);
  if (s_eglSurface == EGL_NO_SURFACE) {
    showInitError("ANGLE: eglCreateWindowSurface failed");
    return false;
  }

  EGLint contextAttribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };
  s_eglContext = eglCreateContext(s_eglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttribs);
  if (s_eglContext == EGL_NO_CONTEXT) {
    showInitError("ANGLE: eglCreateContext failed");
    return false;
  }

  if (!eglMakeCurrent(s_eglDisplay, s_eglSurface, s_eglSurface, s_eglContext)) {
    showInitError("ANGLE: eglMakeCurrent failed");
    return false;
  }

  glGetProcAddressOverride = (GLGetProcAddressFunc)eglGetProcAddress;

  // Capture screen FBO (typically 0 under ANGLE/Metal).
  GLint fbo = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
  s_screenFBO = static_cast<GLuint>(fbo);

  try {
    initGLFunctions();
  } catch (const Exception &exc) {
    showInitError(exc.msg);
    return false;
  }

  gl.ClearColor(0, 0, 0, 1);
  gl.Clear(GL_COLOR_BUFFER_BIT);
  eglSwapBuffers(s_eglDisplay, s_eglSurface);

  printGLInfo();

  eglSwapInterval(s_eglDisplay, 1);

  return true;
}

static void teardownANGLE() {
  eglMakeCurrent(s_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  if (s_eglContext != EGL_NO_CONTEXT) {
    eglDestroyContext(s_eglDisplay, s_eglContext);
    s_eglContext = EGL_NO_CONTEXT;
  }
  if (s_eglSurface != EGL_NO_SURFACE) {
    eglDestroySurface(s_eglDisplay, s_eglSurface);
    s_eglSurface = EGL_NO_SURFACE;
  }
  if (s_eglDisplay != EGL_NO_DISPLAY) {
    eglTerminate(s_eglDisplay);
    s_eglDisplay = EGL_NO_DISPLAY;
  }
  if (s_metalView) {
    SDL_Metal_DestroyView(s_metalView);
    s_metalView = nullptr;
  }
  glGetProcAddressOverride = nullptr;
}
