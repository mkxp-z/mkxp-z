//
// Created by fcors on 10/21/2023.
//

#pragma once

#include <memory>

#define TIME_MANAGER TimeManager::getInstance()

class TimeManagerPrivate;

class TimeManager {
    TimeManager();

    ~TimeManager();

public:
    static TimeManager &getInstance();

    void resetTime();

    unsigned int genTimeStamp();

    // Returns time since SharedState was constructed in microseconds
    double runTime();

private:
    std::unique_ptr<TimeManagerPrivate> m_private;
};
