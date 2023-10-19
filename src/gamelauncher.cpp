//
// Created by fcors on 10/17/2023.
//
#include "gamelauncher.h"

#ifndef MKXPZ_BUILD_XCODE
#include "icon.png.xxd"
#endif

#include <alc.h>

#include <assert.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <regex>

#include "binding.h"
#include "sharedstate.h"
#include "eventthread.h"
#include "util/debugwriter.h"
#include "util/exception.h"
#include "display/gl/gl-debug.h"
#include "display/gl/gl-fun.h"

#include "filesystem/filesystem.h"

#include "system/system.h"

#if defined(__WIN32__)
#include "resource.h"
#include <Winsock2.h>
#include "util/win-consoleutils.h"

// Try to work around buggy GL drivers that tend to be in Optimus laptops
// by forcing MKXP to use the dedicated card instead of the integrated one
#include <windows.h>
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#ifdef MKXPZ_STEAM
#include "steamshim_child.h"
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

static void rgssThreadError(RGSSThreadData *rtData, const std::string &msg);
static void showInitError(const std::string &msg);

static inline const char *glGetStringInt(GLenum name) {
    return (const char *)gl.GetString(name);
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

static SDL_GLContext initGL(SDL_Window *win, Config &conf,
                            RGSSThreadData *threadData);

int rgssThreadFun(void *userdata) {
    RGSSThreadData *threadData = static_cast<RGSSThreadData *>(userdata);

#ifdef MKXPZ_INIT_GL_LATER
    threadData->glContext =
      initGL(threadData->window, threadData->config, threadData);
  if (!threadData->glContext)
    return 0;
#else
    SDL_GL_MakeCurrent(threadData->window, threadData->glContext);
#endif

    /* Setup AL context */
    ALCcontext *alcCtx = alcCreateContext(threadData->alcDev, 0);

    if (!alcCtx) {
        rgssThreadError(threadData, "Error creating OpenAL context");
        return 0;
    }

    alcMakeContextCurrent(alcCtx);

    try {
        SharedState::initInstance(threadData);
    } catch (const Exception &exc) {
        rgssThreadError(threadData, exc.msg);
        alcDestroyContext(alcCtx);

        return 0;
    }

    /* Start script execution */
    scriptBinding->execute();

    threadData->rqTermAck.set();
    threadData->ethread->requestTerminate();

    SharedState::finiInstance();

    alcDestroyContext(alcCtx);

    return 0;
}

static void printRgssVersion(int ver) {
    const char *const makers[] = {"", "XP", "VX", "VX Ace"};

    char buf[128];
    snprintf(buf, sizeof(buf), "RGSS version %d (RPG Maker %s)", ver,
             makers[ver]);

    Debug() << buf;
}

static void rgssThreadError(RGSSThreadData *rtData, const std::string &msg) {
    rtData->rgssErrorMsg = msg;
    rtData->ethread->requestTerminate();
    rtData->rqTermAck.set();
}

static void showInitError(const std::string &msg) {
    Debug() << msg;
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "mkxp-z", msg.c_str(), 0);
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
    SDL_GLContext glCtx{};

    /* Setup GL context. Must be done in main thread since macOS 10.15 */
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    if (conf.debugMode)
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

    glCtx = SDL_GL_CreateContext(win);

    if (!glCtx) {
        GLINIT_SHOWERROR(std::string("Could not create OpenGL context: ") + SDL_GetError());
        return 0;
    }

    try {
        initGLFunctions();
    } catch (const Exception &exc) {
        GLINIT_SHOWERROR(exc.msg);
        SDL_GL_DeleteContext(glCtx);

        return 0;
    }

// This breaks scaling for Retina screens.
// Using Metal should be rendering this irrelevant anyway, hopefully
#ifndef MKXPZ_BUILD_XCODE
    if (!conf.enableBlitting)
        gl.BlitFramebuffer = 0;
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


bool GameLauncher::init(int argc, char *argv[]) {
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

#if defined(__WIN32__)
    // Create a debug console in debug mode
    if (m_config->winConsole) {
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

#ifdef MKXPZ_STEAM
    if (!STEAMSHIM_init()) {
      showInitError("Failed to initialize Steamworks. The application cannot "
                    "continue launching.");
      SDL_Quit();
      return 0;
    }
#endif

    if (m_config->windowTitle.empty())
        m_config->windowTitle = m_config->game.title;

    assert(m_config->rgssVersion >= 1 && m_config->rgssVersion <= 3);
    printRgssVersion(m_config->rgssVersion);

    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (m_sdl->initIMG(imgFlags)) {
        showInitError(std::string("Error initializing SDL_image: ") +
                      SDL_GetError());
        m_sdl = nullptr;

#ifdef MKXPZ_STEAM
        STEAMSHIM_deinit();
#endif

        return false;
    }

    if (TTF_Init()) {
        showInitError(std::string("Error initializing SDL_ttf: ") +
                      SDL_GetError());
        m_sdl = nullptr;

#ifdef MKXPZ_STEAM
        STEAMSHIM_deinit();
#endif

        return false;
    }

    if (Sound_Init() == 0) {
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

    m_initialized = true;
    return true;
}

bool GameLauncher::showWindow(const std::string &windowName) {
    if (!m_initialized)
        return false;

    Uint32 winFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_ALLOW_HIGHDPI;

    if (m_config->winResizable)
        winFlags |= SDL_WINDOW_RESIZABLE;
    if (m_config->fullscreen)
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

    m_window = std::unique_ptr<SDL_Window, void(*)(SDL_Window*)>(SDL_CreateWindow(m_config->windowTitle.c_str(), SDL_WINDOWPOS_UNDEFINED,
                                                                                  SDL_WINDOWPOS_UNDEFINED, m_config->defScreenW,
                                                                                  m_config->defScreenH, winFlags), &SDL_DestroyWindow);

    if (!m_window) {
        showInitError(std::string("Error creating window: ") + SDL_GetError());

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
    (void)setupWindowIcon;
#endif
    m_alcDev = std::unique_ptr<ALCdevice, ALCboolean(*)(ALCdevice*)>(alcOpenDevice(0), &alcCloseDevice);

    if (!m_alcDev) {
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
        m_config->syncToRefreshrate = false;

    eventThread = std::make_unique<EventThread>();

#ifndef MKXPZ_INIT_GL_LATER
    SDL_GLContext glCtx = initGL(m_window.get(), *m_config, 0);
#else
    SDL_GLContext glCtx = NULL;
#endif

    rtData = std::make_unique<RGSSThreadData>(eventThread.get(), windowName.data(), m_window.get(), m_alcDev.get(), mode.refresh_rate,
                          mkxp_sys::getScalingFactor(), *m_config, glCtx);

    int winW, winH, drwW, drwH;
    SDL_GetWindowSize(m_window.get(), &winW, &winH);
    rtData->windowSizeMsg.post(Vec2i(winW, winH));

    SDL_GL_GetDrawableSize(m_window.get(), &drwW, &drwH);
    rtData->drawableSizeMsg.post(Vec2i(drwW, drwH));

    /* Load and post key bindings */
    rtData->bindingUpdateMsg.post(loadBindings(*m_config));

#ifdef MKXPZ_BUILD_XCODE
    // Create Touch Bar
    initTouchBar(win, conf);
#endif

    m_windowCreated = true;
    return true;
}

GameLauncher::~GameLauncher() {
    if (rtData != nullptr && rtData->glContext)
        SDL_GL_DeleteContext(rtData->glContext);

    /* Clean up any remainin events */
    if (eventThread != nullptr)
        eventThread->cleanup();

    Debug() << "Shutting down.";

    #if defined(__WIN32__)
        if (wsadata != nullptr && wsadata->wVersion)
            WSACleanup();
    #endif

    #ifdef MKXPZ_STEAM
        STEAMSHIM_deinit();
    #endif
}

void GameLauncher::runThreads(const std::function<void()> &callback) {
    if (!m_initialized || !m_windowCreated)
        return;

    /* Start RGSS thread */
    SDL_Thread *rgssThread = SDL_CreateThread(rgssThreadFun, "rgss", &rtData);

    /* Start event processing */
    eventThread->process(*rtData);

    /* Request RGSS thread to stop */
    rtData->rqTerm.set();

    /* Wait for RGSS thread response */
    for (int i = 0; i < 1000; ++i) {
        /* We can stop waiting when the request was ack'd */
        if (rtData->rqTermAck) {
            Debug() << "RGSS thread ack'd request after" << i * 10 << "ms";
            break;
        }

        /* Give RGSS thread some time to respond */
        SDL_Delay(10);
    }

    /* If RGSS thread ack'd request, wait for it to shutdown,
     * otherwise abandon hope and just end the process as is. */
    if (rtData->rqTermAck)
        SDL_WaitThread(rgssThread, 0);
    else
        SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_ERROR, m_config->game.title.c_str(),
                std::string("The RGSS script seems to be stuck. "+m_config->game.title+" will now force quit.").c_str(),
                m_window.get());

    if (!rtData->rgssErrorMsg.empty()) {
        Debug() << rtData->rgssErrorMsg;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, m_config->game.title.c_str(),
                                 rtData->rgssErrorMsg.c_str(), m_window.get());
    }
}

GameLauncher::GameLauncher() : m_window(nullptr, &SDL_DestroyWindow), m_alcDev(nullptr, &alcCloseDevice) {

}

GameLauncher &GameLauncher::instance() {
    static GameLauncher gameLauncher;
    return gameLauncher;
}

void GameLauncher::setConfig(std::shared_ptr<Config> config) {
    m_config = std::move(config);
}

std::shared_ptr<Config> GameLauncher::getConfig() const {
    return m_config;
}
