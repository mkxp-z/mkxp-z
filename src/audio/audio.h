/*
** audio.h
**
** This file is part of mkxp.
**
** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
**
** mkxp is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** mkxp is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef AUDIO_H
#define AUDIO_H

#include "IAudio.h"
#include <memory>

/* Concerning the 'pos' parameter:
 *   RGSS3 actually doesn't specify a format for this,
 *   it's only implied that it is a numerical value
 *   (must be 0 on invalid cases), and it's not used for
 *   anything outside passing it back into bgm/bgs_play.
 *   We use this freedom to define pos to be a float,
 *   in seconds of playback. (RGSS3 seems to use large
 *   integers that _look_ like sample offsets but I can't
 *   quite make out their meaning yet) */

struct AudioPrivate;
struct RGSSThreadData;

class Audio : public IAudio {
public:
    void bgmPlay(const char *filename,
                 int volume,
                 int pitch,
                 float pos,
                 int track) override;

    void bgmStop(int track) override;

    void bgmFade(int time, int track) override;

    int bgmGetVolume(int track) override;

    void bgmSetVolume(int volume, int track) override;

    void bgsPlay(const char *filename,
                 int volume,
                 int pitch,
                 float pos) override;

    void bgsStop() override;

    void bgsFade(int time) override;

    void mePlay(const char *filename,
                int volume,
                int pitch) override;

    void meStop() override;

    void meFade(int time) override;

    void sePlay(const char *filename,
                int volume,
                int pitch) override;

    void seStop() override;

    void setupMidi() override;

    float bgmPos(int track) override;

    float bgsPos() override;

    void reset() override;

private:
    explicit Audio(RGSSThreadData &rtData);

    ~Audio() override;

    friend class AudioManager;

    friend struct SharedStatePrivate;
    friend std::unique_ptr<Audio>::deleter_type;

    std::unique_ptr<AudioPrivate> p;
};

#endif // AUDIO_H
