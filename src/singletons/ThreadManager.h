//
// Created by fcors on 10/21/2023.
//
#pragma once

#include "AbstractEventThread.h"

#include <memory>

class SDL_Instance;
struct ALCcontext;
class SharedState;

#if defined(__WIN32__)
struct WSAData;
#endif


class ThreadManager {
    ThreadManager();

    ~ThreadManager();

public:
    static ThreadManager &getInstance();
    static void killThreadManager();

    bool init();

    bool isInitialized();

    bool startRgssThread();

    SharedState *getSharedState() const;

private:
    friend std::unique_ptr<ThreadManager>::deleter_type;

    bool m_initialized = false;
    std::shared_ptr<SDL_Instance> m_sdl;
    std::shared_ptr<RGSSThreadData> m_threadData;
    std::unique_ptr<SharedState> m_sharedState;

    std::shared_ptr<AbstractEventThread> m_eventThread;
    std::unique_ptr<ALCcontext, void(*)(ALCcontext*)> alcCtx;

#if defined(__WIN32__)
    std::unique_ptr<WSAData> wsadata;
#endif
};
