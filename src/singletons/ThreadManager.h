//
// Created by fcors on 10/21/2023.
//
#pragma once

#include "AbstractEventThread.h"

#include <memory>

class SDL_Instance;

struct ALCdevice;
struct ALCcontext;

class SharedState;

struct SDL_Thread;
struct SDL_Window;
using ALCboolean = char;

#if defined(__WIN32__)
struct WSAData;
#endif


class ThreadManager {
    ThreadManager();

    ~ThreadManager();

public:
    static ThreadManager &getInstance();

    static void killInstance();

    bool init();

    bool startRgssThread();

    bool startEventLoop();

    const std::shared_ptr<SharedState> &getSharedState() const;

private:
    friend std::unique_ptr<ThreadManager>::deleter_type;

    static std::unique_ptr<ThreadManager> s_instance;

    bool m_initialized = false;
    std::unique_ptr<SDL_Instance> m_sdl;
    std::shared_ptr<RGSSThreadData> m_threadData;
    std::shared_ptr<SharedState> m_sharedState;
    std::unique_ptr<SDL_Window, void (*)(SDL_Window *)> m_window;

    std::shared_ptr<AbstractEventThread> m_eventThread;
    std::unique_ptr<ALCcontext, void (*)(ALCcontext *)> alcCtx;
    std::unique_ptr<ALCdevice, ALCboolean(*)(ALCdevice *)> alcDev;
    SDL_Thread *m_eventLoop = nullptr;

#if defined(__WIN32__)
    std::unique_ptr<WSAData> wsadata;
#endif
};
