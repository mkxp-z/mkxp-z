//
// Created by fcors on 10/21/2023.
//

#include "ThreadManager.h"

#include <eventthread.h>

#include <SDL.h>
#include <SDL_sound.h>
#include <SDL_ttf.h>

#include "SDL_Instance.h"
#include "filesystem.h"
#include "ConfigManager.h"
#include "win-consoleutils.h"
#include "system.h"
#include "gl-fun.h"
#include "debugwriter.h"
#include "exception.h"
#include <alc.h>
#include <SDL_opengl.h>
#include <regex>

#ifdef __WIN32__

#include <winsock.h>

// Try to work around buggy GL drivers that tend to be in Optimus laptops
// by forcing MKXP to use the dedicated card instead of the integrated one
#include <windows.h>

#endif

#ifndef MKXPZ_BUILD_XCODE

#include "icon.png.xxd"
#include "sharedstate.h"


#endif

#ifdef MKXPZ_BUILD_XCODE
#include <Availability.h>
#include "TouchBar.h"
#if __MAC_OS_X_VERSION_MAX_ALLOWED < __MAC_10_15
#define MKXPZ_INIT_GL_LATER
#endif
#endif

#ifndef MKXPZ_INIT_GL_LATER
#define GLINIT_SHOWERROR(s) showInitError(s)
#else
#define GLINIT_SHOWERROR(s) rgssThreadError(threadData, s)
#endif

static void showInitError(const std::string &msg);

static void printRgssVersion(int ver);

static void setupWindowIcon(const Config &conf, SDL_Window *win);

static SDL_GLContext initGL(SDL_Window *win, Config &conf,
                            RGSSThreadData *threadData);

static void printGLInfo();

static void rgssThreadError(RGSSThreadData *rtData, const std::string &msg);

static inline const char *glGetStringInt(GLenum name) {
    return (const char *) gl.GetString(name);
}

static int eventThreadFun(void *userData);

static void destroyEventLoop(SDL_Thread *th);

ThreadManager::ThreadManager() : m_window(nullptr, &SDL_DestroyWindow), alcDev(nullptr, &alcCloseDevice),
                                 alcCtx(nullptr, &alcDestroyContext) {

}

ThreadManager::~ThreadManager() {
    SDL_Event e;
    e.type = SDL_QUIT;
    SDL_PushEvent(&e);
    SDL_WaitThread(m_eventLoop, nullptr);

#ifdef MKXPZ_STEAM
    STEAMSHIM_deinit();
#endif

    m_eventThread->cleanup();
}

std::unique_ptr<ThreadManager> ThreadManager::s_instance(new ThreadManager());

ThreadManager &ThreadManager::getInstance() {
    return *s_instance;
}

void ThreadManager::killInstance() {
    s_instance = nullptr;
}

bool ThreadManager::init() {
    if (m_initialized)
        return true;

    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");

#ifdef GLES2_HEADER
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
#endif

    /* initialize SDL first */
    m_sdl = SDL_Instance::create(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
    if (m_sdl == nullptr) {
        showInitError(std::string("Error initializing SDL: ") + SDL_GetError());
        return false;
    }

    if (!EventThread::allocUserEvents()) {
        showInitError("Error allocating SDL user events");
        return false;
    }

#ifndef WORKDIR_CURRENT
    char dataDir[512]{};
#if defined(__linux__)
    char *tmp{};
    tmp = getenv("SRCDIR");
    if (tmp) {
      strncpy(dataDir, tmp, sizeof(dataDir));
    }
#endif
    if (!dataDir[0]) {
        strncpy(dataDir, mkxp_fs::getDefaultGameRoot().c_str(), sizeof(dataDir));
    }
    mkxp_fs::setCurrentDirectory(dataDir);
#endif

    auto config = ConfigManager::getInstance().getConfig();

#if defined(__WIN32__)
    // Create a debug console in debug mode
    if (config->winConsole) {
        if (setupWindowsConsole()) {
            reopenWindowsStreams();
        } else {
            char buf[200];
            snprintf(buf, sizeof(buf), "Error allocating console: %lu",
                     GetLastError());
            showInitError(std::string(buf));
        }
    }
#endif

    if (config->windowTitle.empty())
        config->windowTitle = config->game.title;

    assert(config->rgssVersion >= 1 && config->rgssVersion <= 3);
    printRgssVersion(config->rgssVersion);

    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!m_sdl->initIMG(imgFlags)) {
        showInitError(std::string("Error initializing SDL_image: ") +
                      SDL_GetError());
        m_sdl = nullptr;

#ifdef MKXPZ_STEAM
        STEAMSHIM_deinit();
#endif

        return false;
    }

    if (!m_sdl->initTTF()) {
        showInitError(std::string("Error initializing SDL_ttf: ") +
                      SDL_GetError());
        m_sdl = nullptr;

#ifdef MKXPZ_STEAM
        STEAMSHIM_deinit();
#endif

        return false;
    }

    if (!m_sdl->initSound()) {
        showInitError(std::string("Error initializing SDL_sound: ") +
                      Sound_GetError());
        m_sdl = nullptr;

#ifdef MKXPZ_STEAM
        STEAMSHIM_deinit();
#endif

        return false;
    }
#if defined(__WIN32__)
    wsadata = std::make_unique<WSAData>();
    if (WSAStartup(0x101, wsadata.get()) || wsadata->wVersion != 0x101) {
        char buf[200];
        snprintf(buf, sizeof(buf), "Error initializing winsock: %08X",
                 WSAGetLastError());
        showInitError(
                std::string(buf)); // Not an error worth ending the program over
    }
#endif

    Uint32 winFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_ALLOW_HIGHDPI;

    if (config->winResizable)
        winFlags |= SDL_WINDOW_RESIZABLE;
    if (config->fullscreen)
        winFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

#ifdef GLES2_HEADER
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    // LoadLibrary properly initializes EGL, it won't work otherwise.
    // Doesn't completely do it though, needs a small patch to SDL
#ifdef MKXPZ_BUILD_XCODE
    SDL_setenv("ANGLE_DEFAULT_PLATFORM", (conf.preferMetalRenderer) ? "metal" : "opengl", true);
    SDL_GL_LoadLibrary("@rpath/libEGL.dylib");
#endif
#endif

    m_window.reset(SDL_CreateWindow(config->windowTitle.c_str(), SDL_WINDOWPOS_UNDEFINED,
                                    SDL_WINDOWPOS_UNDEFINED, config->defScreenW,
                                    config->defScreenH, winFlags));

    if (!m_window) {
        showInitError(std::string("Error creating window: ") + SDL_GetError());
        m_sdl = nullptr;

#ifdef MKXPZ_STEAM
        STEAMSHIM_deinit();
#endif
        return false;
    }

#ifdef MKXPZ_BUILD_XCODE
    {
        std::string downloadsPath = "/Users/" + mkxp_sys::getUserName() + "/Downloads";

        if (mkxp_fs::getCurrentDirectory().find(downloadsPath) == 0) {
            showInitError(conf.game.title +
                          " cannot run from the Downloads directory.\n\n" +
                          "Please move the application to the Applications folder (or anywhere else) " +
                          "and try again.");
#ifdef MKXPZ_STEAM
            STEAMSHIM_deinit();
#endif
            return 0;
        }
    }
#endif

#if defined(MKXPZ_BUILD_XCODE)
#define DEBUG_FSELECT_MSG "Select the folder from which to load game files. This is the folder containing the game's INI."
#define DEBUG_FSELECT_PROMPT "Load Game"
    if (conf.manualFolderSelect) {
        std::string dataDirStr = mkxp_fs::selectPath(win, DEBUG_FSELECT_MSG, DEBUG_FSELECT_PROMPT);
        if (!dataDirStr.empty()) {
            conf.gameFolder = dataDirStr;
            mkxp_fs::setCurrentDirectory(dataDirStr.c_str());
            Debug() << "Current directory set to" << dataDirStr;
            conf.read(argc, argv);
            conf.readGameINI();
        }
    }
#endif

    /* OSX and Windows have their own native ways of
     * dealing with icons; don't interfere with them */
#ifdef __LINUX__
    setupWindowIcon(conf, win);
#else
    (void) setupWindowIcon;
#endif
    alcDev.reset(alcOpenDevice(nullptr));

    if (!alcDev) {
        showInitError("Could not detect an available audio device.");
        m_window = nullptr;
        m_sdl = nullptr;

#ifdef MKXPZ_STEAM
        STEAMSHIM_deinit();
#endif
        return false;
    }

    SDL_DisplayMode mode;
    SDL_GetDisplayMode(0, 0, &mode);

    /* Can't sync to display refresh rate if its value is unknown */
    if (!mode.refresh_rate)
        config->syncToRefreshrate = false;

    m_eventThread = std::make_shared<EventThread>();

#ifndef MKXPZ_INIT_GL_LATER
    SDL_GLContext glCtx = initGL(m_window.get(), *config, 0);
#else
    SDL_GLContext glCtx = NULL;
#endif

    m_threadData = std::make_shared<RGSSThreadData>(m_eventThread, config->windowTitle.c_str(), m_window.get(),
                                                    alcDev.get(),
                                                    mode.refresh_rate,
                                                    mkxp_sys::getScalingFactor(), config,
                                                    ConfigManager::getInstance().getfilesystem(), glCtx);

    int winW, winH, drwW, drwH;
    SDL_GetWindowSize(m_window.get(), &winW, &winH);
    m_threadData->windowSizeMsg.post(Vec2i(winW, winH));

    SDL_GL_GetDrawableSize(m_window.get(), &drwW, &drwH);
    m_threadData->drawableSizeMsg.post(Vec2i(drwW, drwH));

    /* Load and post key bindings */
    m_threadData->bindingUpdateMsg.post(loadBindings(*config));

#ifdef MKXPZ_BUILD_XCODE
    // Create Touch Bar
    initTouchBar(win, conf);
#endif

    // Start the event thread and have it run until the program terminates
    m_eventLoop = SDL_CreateThread(eventThreadFun, "eventLoop", &m_threadData);

    m_initialized = true;
    return true;
}

bool ThreadManager::startRgssThread() {
#ifdef MKXPZ_INIT_GL_LATER
    m_threadData->glContext =
      initGL(m_threadData->window.get(), *m_threadData->config, m_threadData.get());
  if (!m_threadData->glContext)
    return false;
#else
    SDL_GL_MakeCurrent(m_threadData->window, m_threadData->glContext);
#endif

    /* Setup AL context */
    alcCtx = std::unique_ptr<ALCcontext, void (*)(ALCcontext *)>(alcCreateContext(m_threadData->alcDev, 0),
                                                                 &alcDestroyContext);

    if (!alcCtx) {
        rgssThreadError(m_threadData.get(), "Error creating OpenAL context");
        return false;
    }

    alcMakeContextCurrent(alcCtx.get());

    try {
        m_sharedState = SharedState::initInstance(m_threadData);
    } catch (const Exception &exc) {
        rgssThreadError(m_threadData.get(), exc.msg);
        alcCtx = nullptr;

        return false;
    }

    return true;
}

const std::shared_ptr<SharedState> & ThreadManager::getSharedState() const {
    return m_sharedState;
}

static void showInitError(const std::string &msg) {
    Debug() << msg;
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "mkxp-z", msg.c_str(), 0);
}

static void printRgssVersion(int ver) {
    const char *const makers[] = {"", "XP", "VX", "VX Ace"};

    char buf[128];
    snprintf(buf, sizeof(buf), "RGSS version %d (RPG Maker %s)", ver,
             makers[ver]);

    Debug() << buf;
}

static void setupWindowIcon(const Config &conf, SDL_Window *win) {
    SDL_RWops *iconSrc;

    if (conf.iconPath.empty())
#ifndef MKXPZ_BUILD_XCODE
        iconSrc = SDL_RWFromConstMem(___assets_icon_png, ___assets_icon_png_len);
#else
        iconSrc = SDL_RWFromFile(mkxp_fs::getPathForAsset("icon", "png").c_str(), "rb");
#endif
    else
        iconSrc = SDL_RWFromFile(conf.iconPath.c_str(), "rb");

    SDL_Surface *iconImg = IMG_Load_RW(iconSrc, SDL_TRUE);

    if (iconImg) {
        SDL_SetWindowIcon(win, iconImg);
        SDL_FreeSurface(iconImg);
    }
}

static SDL_GLContext initGL(SDL_Window *win, Config &conf,
                            RGSSThreadData *threadData) {
    SDL_GLContext glCtx = nullptr;

    /* Setup GL context. Must be done in main thread since macOS 10.15 */
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    if (conf.debugMode)
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

    glCtx = SDL_GL_CreateContext(win);

    if (!glCtx) {
        GLINIT_SHOWERROR(std::string("Could not create OpenGL context: ") + SDL_GetError());
        return nullptr;
    }

    try {
        initGLFunctions();
    } catch (const Exception &exc) {
        GLINIT_SHOWERROR(exc.msg);
        SDL_GL_DeleteContext(glCtx);

        return nullptr;
    }

// This breaks scaling for Retina screens.
// Using Metal should be rendering this irrelevant anyway, hopefully
#ifndef MKXPZ_BUILD_XCODE
    if (!conf.enableBlitting)
        gl.BlitFramebuffer = nullptr;
#endif

    gl.ClearColor(0, 0, 0, 1);
    gl.Clear(GL_COLOR_BUFFER_BIT);
    SDL_GL_SwapWindow(win);

    printGLInfo();

    bool vsync = conf.vsync || conf.syncToRefreshrate;
    SDL_GL_SetSwapInterval(vsync ? 1 : 0);

    // GLDebugLogger dLogger;
    return glCtx;
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

        std::smatch vmatches;
        if (std::regex_search(version, vmatches, std::regex("\\(ANGLE (.+) git hash: .+\\)"))) {
            Debug() << "ANGLE Version     :" << vmatches[1].str();
        }
        return;
    }

    Debug() << "Backend      :" << "OpenGL";
    Debug() << "GL Vendor    :" << glGetStringInt(GL_VENDOR);
    Debug() << "GL Renderer  :" << renderer;
    Debug() << "GL Version   :" << version;
    Debug() << "GLSL Version :" << glGetStringInt(GL_SHADING_LANGUAGE_VERSION);
}

static void rgssThreadError(RGSSThreadData *rtData, const std::string &msg) {
    rtData->rgssErrorMsg = msg;
    rtData->ethread->requestTerminate();
    rtData->rqTermAck.set();
}

static int eventThreadFun(void *userData) {
    auto threadData = *static_cast<std::shared_ptr<RGSSThreadData> *>(userData);
    threadData->ethread->process(*threadData);
    threadData->rqTerm.set();
    return 0;
}

static void destroyEventLoop(SDL_Thread *th) {
    SDL_WaitThread(th, nullptr);
}