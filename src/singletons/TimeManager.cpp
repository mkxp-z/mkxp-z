//
// Created by fcors on 10/21/2023.
//

#include "TimeManager.h"

#include <chrono>

struct TimeManagerPrivate {
    unsigned int stampCounter = 0;
    std::chrono::time_point<std::chrono::steady_clock> startupTime;

    TimeManagerPrivate() {
        startupTime = std::chrono::steady_clock::now();
    }
};

TimeManager::TimeManager() : m_private(std::make_unique<TimeManagerPrivate>()) {

}

TimeManager::~TimeManager() = default;

TimeManager &TimeManager::getInstance() {
    static TimeManager timeManager;
    return timeManager;
}

void TimeManager::resetTime() {
    m_private->startupTime = std::chrono::steady_clock::now();
}

unsigned int TimeManager::genTimeStamp() {
    return m_private->stampCounter++;
}

double TimeManager::runTime() {
    if (!m_private) return 0;
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now - m_private->startupTime).count() / 1000.0 /
           1000.0;
}
