//
// Created by fcors on 10/21/2023.
//

#include "AudioManager.h"

#include "audio.h"
#include "IAudio.h"

AudioManager::AudioManager() = default;

AudioManager::~AudioManager() = default;

AudioManager &AudioManager::getInstance() {
    static AudioManager audioManager;
    return audioManager;
}

IAudio &AudioManager::getAudio() const {
    return *m_audio;
}

ISyncPoint &AudioManager::getSyncPoint() const {
    return *m_syncPoint;
}
