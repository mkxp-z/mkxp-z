//
// Created by fcors on 11/12/2023.
//
#pragma once

#include <thread>
#include <mutex>

// We want the gem to always run from the current directory
#define WORKDIR_CURRENT

struct RGSSThreadData;

class RgssThreadManager {
private:
    RgssThreadManager() = default;

    ~RgssThreadManager() = default;

public:
    static inline RgssThreadManager &getInstance() {
        return instance;
    }

    inline RGSSThreadData *getThreadData() const {
        return threadData;
    }

    inline void setThreadData(RGSSThreadData *rtData) {
        threadData = rtData;
    }

    inline std::mutex &getThreadMutex() {
        return rgssThreadMutex;
    }

    inline void lockRgssThread() {
        rgssThreadLock = std::make_unique<std::scoped_lock<std::mutex>>(rgssThreadMutex);
    }

    inline void unlockRgssThread() {
        rgssThreadLock.reset();
    }

private:
    static RgssThreadManager instance;

    RGSSThreadData *threadData = nullptr;
    std::unique_ptr<std::scoped_lock<std::mutex>> rgssThreadLock;
    std::mutex rgssThreadMutex;
};