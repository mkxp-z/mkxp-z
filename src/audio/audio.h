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

/* Concerning the 'pos' parameter:
 *   RGSS3 actually doesn't specify a format for this,
 *   it's only implied that it is a numerical value
 *   (must be 0 on invalid cases), and it's not used for
 *   anything outside passing it back into bgm/bgs_play.
 *   We use this freedom to define pos to be a float,
 *   in seconds of playback. (RGSS3 seems to use large
 *   integers that _look_ like sample offsets but I can't
 *   quite make out their meaning yet) */

#ifdef MKXPZ_RETRO
#  include "mkxp-polyfill.h"
#  include "wasm-types.h"
#else
#  include <SDL_mutex.h>
#endif // MKXPZ_RETRO

#include "exception.h"

struct AudioPrivate;
struct RGSSThreadData;

#if defined(MKXPZ_RETRO) && !defined(MKXPZ_NO_THREADED_AUDIO) && (!defined(MKXPZ_NO_PTHREAD_H) || !defined(MKXPZ_NO_STD_MUTEX))
#  define MKXPZ_HAVE_THREADED_AUDIO
#endif

class AudioMutex
{
public:
	AudioMutex();
	~AudioMutex();
	void lock();
	void unlock();

private:
#ifdef MKXPZ_RETRO
	mkxp_mutex_t mutex;
#else
	SDL_mutex *mutex;
#endif // MKXPZ_RETRO
};

class AudioMutexGuard
{
public:
	AudioMutexGuard(AudioMutex &mutex);
	AudioMutexGuard(const AudioMutexGuard &guard) = delete;
	AudioMutexGuard(AudioMutexGuard &&guard) noexcept;
	AudioMutexGuard &operator=(const AudioMutexGuard &guard) = delete;
	AudioMutexGuard &operator=(AudioMutexGuard &&guard) noexcept;
	~AudioMutexGuard();

private:
	AudioMutex *mutex;
};

class Audio
{
public:
#ifdef MKXPZ_RETRO
	// Render one video frame's worth of audio to OpenAL. This function is only available in libretro builds.
	void render();
#endif // MKXPZ_RETRO

	void bgmPlay(Exception &exception,
	             const char *filename,
	             int volume = 100,
	             int pitch = 100,
	             double pos = 0,
                 int track = -127);
	void bgmStop(Exception &exception, int track = -127);
	void bgmFade(Exception &exception, int time, int track = -127);
    int bgmGetVolume(Exception &exception, int track = -127);
    void bgmSetVolume(Exception &exception, int volume = 100, int track = -127);

	void bgsPlay(Exception &exception,
	             const char *filename,
	             int volume = 100,
	             int pitch = 100,
	             double pos = 0);
	void bgsStop();
	void bgsFade(int time);

	void mePlay(Exception &exception,
	            const char *filename,
	            int volume = 100,
	            int pitch = 100);
	void meStop();
	void meFade(int time);

	void sePlay(const char *filename,
	            int volume = 100,
	            int pitch = 100);
	void seStop();

	void setupMidi();
	double bgmPos(Exception &exception, int track = 0);
	double bgsPos();

	void reset();

	std::string getLastError();

#ifdef MKXPZ_RETRO
	bool sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size);
	bool sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size);
#endif // MKXPZ_RETRO

#ifndef MKXPZ_RETRO
private:
#endif // MKXPZ_RETRO
	Audio(RGSSThreadData &rtData);
	~Audio();
#ifdef MKXPZ_RETRO
private:
#endif // MKXPZ_RETRO

	friend struct SharedStatePrivate;

	AudioPrivate *p;
};

#endif // AUDIO_H
