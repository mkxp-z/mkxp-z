//
// Created by fcors on 10/21/2023.
//
#pragma once

#include "AbstractEventThread.h"

#include <memory>

/*
#define THREAD_MANAGER ThreadManager::getInstance()
#define *shState->rtData() ThreadManager::getInstance().getThreadData()
#define shState->eThread() ThreadManager::getInstance().getEventThread()
 */

class SDL_Instance;

#if defined(__WIN32__)
struct WSAData;
#endif


class ThreadManager {
    ThreadManager();

    ~ThreadManager();

public:
    static ThreadManager &getInstance();

    bool init();

    bool isInitialized();

private:

    bool m_initialized = false;
    std::shared_ptr<RGSSThreadData> m_threadData;
    std::shared_ptr<SDL_Instance> m_sdl;

    std::shared_ptr<AbstractEventThread> m_eventThread;

#if defined(__WIN32__)
    std::unique_ptr<WSAData> wsadata;
#endif
};
