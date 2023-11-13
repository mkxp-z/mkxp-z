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

#ifndef MKXPZ_BUILD_XCODE
#include "icon.png.xxd"
#endif

#include <alc.h>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_sound.h>
#include <SDL_ttf.h>

#include <assert.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <regex>
#include <string_view>
#include <array>

#ifdef MKXPZ_RUBY_GEM

#include "rgssthreadmanager.h"
#endif

#include "binding.h"
#include "rgssthreadmanager.h"
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
#include <winsock2.h>
#include "util/win-consoleutils.h"
#include "filesystem.hpp"

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
#if !defined(__MAC_10_15) || __MAC_OS_X_VERSION_MAX_ALLOWED < __MAC_10_15
#define MKXPZ_INIT_GL_LATER
#endif
#endif

#ifndef MKXPZ_INIT_GL_LATER
#define GLINIT_SHOWERROR(s) showInitError(s)
#else
#define GLINIT_SHOWERROR(s) rgssThreadError(threadData, s)
#endif

#ifdef MKXPZ_RUBY_GEM
RgssThreadManager RgssThreadManager::instance;
#endif

static void rgssThreadError(RGSSThreadData *rtData, std::string_view msg);

static void showInitError(const std::string &msg);

static inline const char *glGetStringInt(GLenum name) {
    return (const char *)gl.GetString(name);
}

static void printGLInfo() {
    const std::string renderer(glGetStringInt(GL_RENDERER));
    const std::string version(glGetStringInt(GL_VERSION));
    std::regex rgx("ANGLE \\((.+), ANGLE Metal Renderer: (.+), Version (.+)\\)");

    if (std::smatch matches; std::regex_search(renderer, matches, rgx)) {

        Debug() << "Backend           :" << "Metal";
        Debug() << "Metal Device      :" << matches[2] << "(" + matches[1].str() + ")";
        Debug() << "Renderer Version  :" << matches[3].str();

        if (std::smatch vmatches; std::regex_search(version, vmatches, std::regex("\\(ANGLE (.+) git hash: .+\\)"))) {
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

static SDL_GLContext initGL(SDL_Window *win, const Config &conf,
                            RGSSThreadData *threadData);

#ifdef MKXPZ_RUBY_GEM
ALCcontext *startRgssThread(RGSSThreadData *threadData) {
    RgssThreadManager::getInstance().lockRgssThread();
#else
    int rgssThreadFun(void *userdata) {
        RGSSThreadData *threadData = static_cast<RGSSThreadData *>(userdata);
#endif

#ifdef MKXPZ_INIT_GL_LATER
    threadData->glContext =
            initGL(threadData->window, threadData->config, threadData);
    if (!threadData->glContext)
#ifdef MKXPZ_RUBY_GEM
        throw std::system_error();
#else
    return 0;
#endif
#else
    SDL_GL_MakeCurrent(threadData->window, threadData->glContext);
#endif

    /* Setup AL context */
    ALCcontext *alcCtx = alcCreateContext(threadData->alcDev, nullptr);

    if (!alcCtx) {
        rgssThreadError(threadData, "Error creating OpenAL context");
#ifdef MKXPZ_RUBY_GEM
        RgssThreadManager::getInstance().unlockRgssThread();
        throw std::system_error(std::error_code(), "RGSS failed to initialize!");
#else
        return 0;
#endif
    }

    alcMakeContextCurrent(alcCtx);

    try {
        SharedState::initInstance(threadData);
    } catch (const Exception &exc) {
        rgssThreadError(threadData, exc.msg);
        alcDestroyContext(alcCtx);


#ifdef MKXPZ_RUBY_GEM
        RgssThreadManager::getInstance().unlockRgssThread();
        throw std::system_error(std::error_code(), "RGSS failed to initialize!");
#else
        return 0;
#endif
    }

    /* Start script execution */
    scriptBinding->execute();

#ifdef MKXPZ_RUBY_GEM
    return alcCtx;
}

int killRgssThread(RGSSThreadData *threadData, ALCcontext *alcCtx) {
#endif
    threadData->rqTermAck.set();
    threadData->ethread->requestTerminate();

    SharedState::finiInstance();

    if (alcCtx != nullptr)
        alcDestroyContext(alcCtx);

#ifdef MKXPZ_RUBY_GEM
    RgssThreadManager::getInstance().unlockRgssThread();
#endif
    return 0;
}

static void printRgssVersion(int ver) {
    const std::array<std::string_view, 4> makers = {"", "XP", "VX", "VX Ace"};

    std::vector<char> buf(128);
    snprintf(buf.data(), buf.size(), "RGSS version %d (RPG Maker %s)", ver,
             makers[ver].data());

    Debug() << buf.data();
}

static void rgssThreadError(RGSSThreadData *rtData, std::string_view msg) {
    rtData->rgssErrorMsg = msg;
    rtData->ethread->requestTerminate();
    rtData->rqTermAck.set();
}

static void showInitError(const std::string &msg) {
    Debug() << msg;
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "mkxp-z", msg.c_str(), nullptr);
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

#ifdef MKXPZ_RUBY_GEM
int startGameWindow(int argc, char *argv[], bool showWindow) {
#else
int main(int argc, char *argv[]) {
#endif
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");

#ifdef GLES2_HEADER
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
#endif

    /* initialize SDL first */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
        showInitError(std::string("Error initializing SDL: ") + SDL_GetError());
        return 0;
    }

    if (!EventThread::allocUserEvents()) {
        showInitError("Error allocating SDL user events");
        return 0;
    }

#ifndef WORKDIR_CURRENT
    std::array<char, 512> dataDir;
#if defined(__linux__)
    char *tmp{};
    tmp = getenv("SRCDIR");
    if (tmp) {
      strncpy(dataDir, tmp, sizeof(dataDir));
    }
#endif
    if (!dataDir[0]) {
        strncpy(dataDir.data(), mkxp_fs::getDefaultGameRoot().c_str(), dataDir.size());
    }
    mkxp_fs::setCurrentDirectory(dataDir.data());
#endif

    /* now we load the config */
    Config conf;
    conf.read(argc, argv);

#if defined(__WIN32__)
    // Create a debug console in debug mode
    if (conf.winConsole) {
        if (setupWindowsConsole()) {
            reopenWindowsStreams();
        } else {
            std::array<char, 200> buf;
            snprintf(buf.data(), buf.size(), "Error allocating console: %lu",
                     GetLastError());
            showInitError(std::string(buf.data()));
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

    if (conf.windowTitle.empty())
        conf.windowTitle = conf.game.title;

    assert(conf.rgssVersion >= 1 && conf.rgssVersion <= 3);
    printRgssVersion(conf.rgssVersion);

    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (IMG_Init(imgFlags) != imgFlags) {
        showInitError(std::string("Error initializing SDL_image: ") +
                      SDL_GetError());
        SDL_Quit();

#ifdef MKXPZ_STEAM
        STEAMSHIM_deinit();
#endif

        return 0;
    }

    if (TTF_Init() < 0) {
        showInitError(std::string("Error initializing SDL_ttf: ") +
                      SDL_GetError());
        IMG_Quit();
        SDL_Quit();

#ifdef MKXPZ_STEAM
        STEAMSHIM_deinit();
#endif

        return 0;
    }

    if (Sound_Init() == 0) {
        showInitError(std::string("Error initializing SDL_sound: ") +
                      Sound_GetError());
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();

#ifdef MKXPZ_STEAM
        STEAMSHIM_deinit();
#endif

        return 0;
    }
#if defined(__WIN32__)
    WSAData wsadata = {0};
    if (WSAStartup(0x101, &wsadata) || wsadata.wVersion != 0x101) {
        std::array<char, 200> buf;
        snprintf(buf.data(), buf.size(), "Error initializing winsock: %08X",
                 WSAGetLastError());
        showInitError(
                std::string(buf.data())); // Not an error worth ending the program over
    }
#endif

    SDL_Window *win;
    Uint32 winFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_ALLOW_HIGHDPI;

    if (conf.winResizable)
        winFlags |= SDL_WINDOW_RESIZABLE;
    if (conf.fullscreen)
        winFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#ifdef MKXPZ_RUBY_GEM
    if (!showWindow)
        winFlags |= SDL_WINDOW_HIDDEN;
#endif

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

    win = SDL_CreateWindow(conf.windowTitle.c_str(), SDL_WINDOWPOS_UNDEFINED,
                           SDL_WINDOWPOS_UNDEFINED, conf.defScreenW,
                           conf.defScreenH, winFlags);

    if (!win) {
        showInitError(std::string("Error creating window: ") + SDL_GetError());

#ifdef MKXPZ_STEAM
        STEAMSHIM_deinit();
#endif
        return 0;
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

    ALCdevice *alcDev = alcOpenDevice(nullptr);

    if (!alcDev) {
        showInitError("Could not detect an available audio device.");
        SDL_DestroyWindow(win);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();

#ifdef MKXPZ_STEAM
        STEAMSHIM_deinit();
#endif
        return 0;
    }

    SDL_DisplayMode mode;
    SDL_GetDisplayMode(0, 0, &mode);

    /* Can't sync to display refresh rate if its value is unknown */
    if (!mode.refresh_rate)
        conf.syncToRefreshrate = false;

    EventThread eventThread;

#ifndef MKXPZ_INIT_GL_LATER
    SDL_GLContext glCtx = initGL(win, conf, 0);
#else
    SDL_GLContext glCtx = nullptr;
#endif

    RGSSThreadData rtData(&eventThread, argv[0], win, alcDev, mode.refresh_rate,
                          mkxp_sys::getScalingFactor(), conf, glCtx);

    int drwW;
    int drwH;
    int winW;
    int winH;
    SDL_GetWindowSize(win, &winW, &winH);
    rtData.windowSizeMsg.post(Vec2i(winW, winH));

    SDL_GL_GetDrawableSize(win, &drwW, &drwH);
    rtData.drawableSizeMsg.post(Vec2i(drwW, drwH));

    /* Load and post key bindings */
    rtData.bindingUpdateMsg.post(loadBindings(conf));

#ifdef MKXPZ_BUILD_XCODE
    // Create Touch Bar
    initTouchBar(win, conf);
#endif

#ifdef MKXPZ_RUBY_GEM
    /* Yield the thread so the interpreter thread can start the RGSS stuff */
    RgssThreadManager::getInstance().setThreadData(&rtData);
    std::this_thread::yield();
#else
    /* Start RGSS thread */
    SDL_Thread *rgssThread = SDL_CreateThread(rgssThreadFun, "rgss", &rtData);
#endif

    /* Start event processing */
    eventThread.process(rtData);

    /* Request RGSS thread to stop */
    rtData.rqTerm.set();

    /* Wait for RGSS thread response */
    for (int i = 0; i < 1000; ++i) {
        /* We can stop waiting when the request was ack'd */
        if (rtData.rqTermAck) {
            Debug() << "RGSS thread ack'd request after" << i * 10 << "ms";
            break;
        }

        /* Give RGSS thread some time to respond */
        SDL_Delay(10);
    }

#ifdef MKXPZ_RUBY_GEM
    std::scoped_lock lockRgssThread(RgssThreadManager::getInstance().getThreadMutex());
#else
    /* If RGSS thread ack'd request, wait for it to shutdown,
     * otherwise abandon hope and just end the process as is. */
    if (rtData.rqTermAck)
      SDL_WaitThread(rgssThread, nullptr);
    else
        SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_ERROR, conf.game.title.c_str(),
                std::string("The RGSS script seems to be stuck. "+conf.game.title+" will now force quit.").c_str(),
                win);
#endif

    if (!rtData.rgssErrorMsg.empty()) {
        Debug() << rtData.rgssErrorMsg;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, conf.game.title.c_str(),
                                 rtData.rgssErrorMsg.c_str(), win);
    }

    if (rtData.glContext)
        SDL_GL_DeleteContext(rtData.glContext);

    /* Clean up any remainin events */
    eventThread.cleanup();

    Debug() << "Shutting down.";

    alcCloseDevice(alcDev);
    SDL_DestroyWindow(win);

#if defined(__WIN32__)
    if (wsadata.wVersion)
        WSACleanup();
#endif

#ifdef MKXPZ_STEAM
    STEAMSHIM_deinit();
#endif
    Sound_Quit();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

#ifdef MKXPZ_RUBY_GEM
    RgssThreadManager::getInstance().setThreadData(nullptr);
#endif
    return 0;
}

static SDL_GLContext initGL(SDL_Window *win, const Config &conf,
                            RGSSThreadData *threadData) {
    SDL_GLContext glCtx{};

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

    return glCtx;
}