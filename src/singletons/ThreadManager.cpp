//
// Created by fcors on 10/21/2023.
//

#include "ThreadManager.h"
#include "DisplayManager.h"

#include <eventthread.h>

ThreadManager::ThreadManager() = default;

ThreadManager::~ThreadManager() = default;

ThreadManager &ThreadManager::getInstance() {
    static ThreadManager threadManager;
    return threadManager;
}

RGSSThreadData &ThreadManager::getThreadData() const {
    return *m_threadData;
}

AbstractEventThread &ThreadManager::getEventThread() const {
    return *m_eventThread;
}

void ThreadManager::checkShutdown() {
    if (!m_threadData->rqTerm)
        return;

    m_threadData->rqTermAck.set();
    TEX_POOL.disable();
    //scriptBinding->terminate();
}

void ThreadManager::checkReset() {
    if (!m_threadData->rqReset)
        return;

    m_threadData->rqReset.clear();
    //scriptBinding->reset();
}
