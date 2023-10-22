//
// Created by fcors on 10/21/2023.
//

#pragma once


class IAudio {
public:
    virtual ~IAudio() = default;

    virtual void bgmPlay(const char *filename,
                         int volume = 100,
                         int pitch = 100,
                         float pos = 0,
                         int track = -127) = 0;

    virtual void bgmStop(int track = -127) = 0;

    virtual void bgmFade(int time, int track = -127) = 0;

    virtual int bgmGetVolume(int track = -127) = 0;

    virtual void bgmSetVolume(int volume = 100, int track = -127) = 0;

    virtual void bgsPlay(const char *filename,
                         int volume = 100,
                         int pitch = 100,
                         float pos = 0) = 0;

    virtual void bgsStop() = 0;

    virtual void bgsFade(int time) = 0;

    virtual void mePlay(const char *filename,
                        int volume = 100,
                        int pitch = 100) = 0;

    virtual void meStop() = 0;

    virtual void meFade(int time) = 0;

    virtual void sePlay(const char *filename,
                        int volume = 100,
                        int pitch = 100) = 0;

    virtual void seStop() = 0;

    virtual void setupMidi() = 0;

    virtual float bgmPos(int track = 0) = 0;

    virtual float bgsPos() = 0;

    virtual void reset() = 0;
};
