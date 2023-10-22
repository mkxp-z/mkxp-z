//
// Created by fcors on 10/21/2023.
//
#pragma once

#include "IAudio.h"
#include "ISyncPoint.h"
#include <memory>

#define AUDIO AudioManager::getInstance().getAudio()
#define SYNC_POINT AudioManager::getInstance().getSyncPoint()

class AudioManager {
    AudioManager();

    ~AudioManager();

public:
    static AudioManager &getInstance();

    IAudio &getAudio() const;

    ISyncPoint &getSyncPoint() const;

private:
    std::unique_ptr<IAudio> m_audio;
    std::unique_ptr<ISyncPoint> m_syncPoint;
};
