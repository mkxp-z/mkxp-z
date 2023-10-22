//
// Created by fcors on 10/21/2023.
//
#pragma once

#include "IAudio.h"
#include "ISyncPoint.h"

#include <alc.h>
#include <memory>

/*
#define AUDIO_MANAGER AudioManager::getInstance()
#define shState->audio() AudioManager::getInstance().getAudio()
#define SYNC_POINT AudioManager::getInstance().getSyncPoint()
#define shState->midiState() AudioManager::getInstance().getMidiState()
 */

using ALCdevicePtr = std::unique_ptr<ALCdevice, ALCboolean(*)(ALCdevice *)>;

class SharedMidiState;

class AudioManager {
    AudioManager();

    ~AudioManager();

public:
    static AudioManager &getInstance();

    bool init();

    IAudio &getAudio() const;

    ISyncPoint &getSyncPoint() const;

    ALCdevice &getAlcDevice() const;

    void setAlcDevice(ALCdevicePtr &&alCdevice);

    SharedMidiState &getMidiState();

private:
    std::unique_ptr<IAudio> m_audio;
    std::shared_ptr<ISyncPoint> m_syncPoint;
    std::unique_ptr<SharedMidiState> m_midiState;
    ALCdevicePtr m_alcDevice;
};
