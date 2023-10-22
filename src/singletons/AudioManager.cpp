//
// Created by fcors on 10/21/2023.
//

#include "AudioManager.h"

#include "audio.h"
#include "IAudio.h"
#include "ThreadManager.h"

#include "eventthread.h"
#include "ConfigManager.h"
#include "sharedmidistate.h"

AudioManager::AudioManager() : m_alcDevice(nullptr, &alcCloseDevice) {

}

AudioManager::~AudioManager() = default;

AudioManager &AudioManager::getInstance() {
    static AudioManager audioManager;
    return audioManager;
}

bool AudioManager::init() {
    if (!shState->isInitialized())
        return false;

    m_syncPoint = shState->rtData()->syncPoint;
    m_midiState = std::make_unique<SharedMidiState>(*shState->config());
    m_audio = std::unique_ptr<Audio>(new Audio(*shState->rtData()));
    return true;
}

IAudio &AudioManager::getAudio() const {
    return *m_audio;
}

ISyncPoint &AudioManager::getSyncPoint() const {
    return *m_syncPoint;
}

ALCdevice &AudioManager::getAlcDevice() const {
    return *m_alcDevice;
}

void AudioManager::setAlcDevice(ALCdevicePtr &&alCdevice) {
    m_alcDevice = std::move(alCdevice);
}

SharedMidiState &AudioManager::getMidiState() {
    return *m_midiState;
}
