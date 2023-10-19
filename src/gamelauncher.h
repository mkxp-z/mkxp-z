//
// Created by fcors on 10/17/2023.
//

#pragma once

#include "config.h"
#include "SDL_Instance.h"
#include "eventthread.h"

#include <memory>
#include <functional>

#include <alc.h>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_sound.h>
#include <SDL_ttf.h>

#if defined(__WIN32__)
#include <winsock.h>
#endif

class GameLauncher {
private:
    GameLauncher();

public:
    ~GameLauncher();

    static GameLauncher &instance();

    std::shared_ptr<Config> getConfig() const;
    void setConfig(std::shared_ptr<Config> config);

    bool init(int argc, char *argv[]);
    bool showWindow(const std::string &windowName);
    void runThreads(const std::function<void()> &callback);

private:
    bool m_initialized = false;
    bool m_windowCreated = false;
    std::shared_ptr<Config> m_config;
    std::unique_ptr<SDL_Instance> m_sdl;
    std::unique_ptr<SDL_Window, void(*)(SDL_Window*)> m_window;
    std::unique_ptr<ALCdevice, ALCboolean(*)(ALCdevice*)> m_alcDev;
    std::unique_ptr<RGSSThreadData> rtData;
    std::unique_ptr<EventThread> eventThread;

#if defined(__WIN32__)
    std::unique_ptr<WSAData> wsadata;
#endif
};
