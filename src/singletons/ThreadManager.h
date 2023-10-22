//
// Created by fcors on 10/21/2023.
//
#pragma once

#include "AbstractEventThread.h"

#include <memory>

#define THREAD_MANAGER ThreadManager::getInstance()
#define RGSS_THREAD_DATA ThreadManager::getInstance().getThreadData()
#define EVENT_THREAD ThreadManager::getInstance().getEventThread()

class ThreadManager {
    ThreadManager();

    ~ThreadManager();

public:
    static ThreadManager &getInstance();

    RGSSThreadData &getThreadData() const;

    AbstractEventThread &getEventThread() const;

    /* Checks EventThread's shutdown request flag and if set,
	 * requests the binding to terminate. In this case, this
	 * function will most likely not return */
    void checkShutdown();

    void checkReset();

private:
    std::unique_ptr<RGSSThreadData> m_threadData;

    std::unique_ptr<AbstractEventThread> m_eventThread;
};
