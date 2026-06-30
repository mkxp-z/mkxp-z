/*
** audiostream.cpp
**
** This file is part of mkxp.
**
** Copyright (C) 2014 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
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

#include "audiostream.h"

#include "util.h"
#include "exception.h"

#ifdef MKXPZ_RETRO
#  include "core.h"
#  include "sandbox-serial-util.h"
#else
#  include <SDL_mutex.h>
#  include <SDL_thread.h>
#  include <SDL_timer.h>
#endif // MKXPZ_RETRO

AudioStream::AudioStream(ALStream::LoopMode loopMode,
                         const std::string &threadId)
	: extPaused(false),
	  noResumeStop(false),
	  stream(loopMode, threadId)
{
	current.volume = 1.0f;
	current.pitch = 1.0f;

	for (size_t i = 0; i < VolumeTypeCount; ++i)
		volumes[i] = 1.0f;

#ifdef MKXPZ_RETRO
	fade.enabled.clear();
	fadeIn.enabled.clear();
#else
	fade.thread = 0;
	fade.threadName = std::string("audio_fadeout (") + threadId + ")";

	fadeIn.thread = 0;
	fadeIn.threadName = std::string("audio_fadein (") + threadId + ")";
#endif // MKXPZ_RETRO
}

AudioStream::~AudioStream()
{
#ifdef MKXPZ_RETRO
	if (fade.enabled)
	{
		fade.reqTerm.set();
		AudioMutexGuard guard(fade.mutex);
	}

	if (fadeIn.enabled)
	{
		fadeIn.rqTerm.set();
		AudioMutexGuard guard(fadeIn.mutex);
	}
#else
	if (fade.thread)
	{
		fade.reqTerm.set();
		SDL_WaitThread(fade.thread, 0);
	}

	if (fadeIn.thread)
	{
		fadeIn.rqTerm.set();
		SDL_WaitThread(fadeIn.thread, 0);
	}
#endif // MKXPZ_RETRO

	AudioMutexGuard guard(mutex);

	stream.stop();
	stream.close();
}

void AudioStream::play(Exception &exception,
	               const std::string &filename,
                       int volume,
                       int pitch,
                       double offset)
{
	finiFadeOutInt();

	AudioMutexGuard guard(mutex);

	float _volume = clamp<int>(volume, 0, 100) / 100.0f;
	float _pitch  = clamp<int>(pitch, 50, 150) / 100.0f;

	ALStream::State sState = stream.queryState();

	/* If all parameters match the current ones and we're
	 * still playing, there's nothing to do */
	if (filename == current.filename
	&&  _volume  == current.volume
	&&  _pitch   == current.pitch
	&&  (sState == ALStream::Playing || sState == ALStream::Paused))
	{
		return;
	}

	/* If all parameters except volume match the current ones,
	 * we update the volume and continue streaming */
	if (filename == current.filename
	&&  _pitch   == current.pitch
	&&  (sState == ALStream::Playing || sState == ALStream::Paused))
	{
		setVolume(Base, _volume);
		current.volume = _volume;
		return;
	}

	/* Requested audio file is different from current one */
	bool diffFile = (filename != current.filename);

	if (diffFile || sState == ALStream::Closed)
	{
		stream.open(exception, filename);
		if (exception.is_error())
			return;
	} else {
		switch (sState)
		{
			case ALStream::Paused :
			case ALStream::Playing :
				stream.stop();
		}
	}

	setVolume(Base, _volume);
	stream.setPitch(_pitch);

	if (offset > 0)
	{
		setVolume(FadeIn, 0);
		startFadeIn();
	}

	current.filename = filename;
	current.volume = _volume;
	current.pitch = _pitch;

	if (!extPaused)
		stream.play(offset);
	else
		noResumeStop = false;
}

void AudioStream::stop()
{
	finiFadeOutInt();

	AudioMutexGuard guard(mutex);

	noResumeStop = true;

	stream.stop();
}

void AudioStream::fadeOut(int duration)
{
	AudioMutexGuard guard(mutex);

	ALStream::State sState = stream.queryState();
	noResumeStop = true;

	if (fade.active)
	{
		return;
	}

	if (sState == ALStream::Paused)
	{
		stream.stop();
		return;
	}

	if (sState != ALStream::Playing)
	{
		return;
	}

#ifdef MKXPZ_RETRO
	if (fade.enabled)
#else
	if (fade.thread)
#endif // MKXPZ_RETRO
	{
#ifdef MKXPZ_RETRO
		fade.reqTerm.set();
		{
			AudioMutexGuard guard(fade.mutex);
		}
		fade.reqTerm.clear();
		fade.reqFini.set();
		fadeOutProc();
		fade.enabled.clear();
#else
		fade.reqFini.set();
		SDL_WaitThread(fade.thread, 0);
		fade.thread = 0;
#endif // MKXPZ_RETRO
	}

	fade.active.set();
	fade.msStep = 1.0f / duration;
	fade.reqFini.clear();
	fade.reqTerm.clear();
#ifdef MKXPZ_RETRO
	fade.startTicks = mkxp_retro::get_ticks_ms();
#else
	fade.startTicks = SDL_GetTicks64();
#endif // MKXPZ_RETRO

#ifdef MKXPZ_RETRO
	fade.enabled.set();
#else
	fade.thread = createSDLThread
		<AudioStream, &AudioStream::fadeOutThread>(this, fade.threadName);
#endif // MKXPZ_RETRO
}

void AudioStream::seek(double offset)
{
	AudioMutexGuard guard(mutex);
	stream.play(offset);
}

void AudioStream::setVolume(VolumeType type, float value)
{
	volumes[type] = value;
	updateVolume();
}

float AudioStream::getVolume(VolumeType type)
{
	return volumes[type];
}

double AudioStream::playingOffset()
{
	return stream.queryOffset();
}

void AudioStream::updateVolume()
{
	float vol = 1.0f;

	for (size_t i = 0; i < VolumeTypeCount; ++i)
		vol *= volumes[i];

	stream.setVolume(vol);
}

void AudioStream::finiFadeOutInt()
{
#ifdef MKXPZ_RETRO
	if (fade.enabled)
#else
	if (fade.thread)
#endif // MKXPZ_RETRO
	{
#ifdef MKXPZ_RETRO
		fade.reqTerm.set();
		{
			AudioMutexGuard guard(fade.mutex);
		}
		fade.reqTerm.clear();
		fade.reqFini.set();
		fadeOutProc();
		fade.enabled.clear();
#else
		fade.reqFini.set();
		SDL_WaitThread(fade.thread, 0);
		fade.thread = 0;
#endif // MKXPZ_RETRO
	}

#ifdef MKXPZ_RETRO
	if (fadeIn.enabled)
#else
	if (fadeIn.thread)
#endif // MKXPZ_RETRO
	{
#ifdef MKXPZ_RETRO
		fadeIn.rqTerm.set();
		{
			AudioMutexGuard guard(fadeIn.mutex);
		}
		fadeIn.rqTerm.clear();
		fadeIn.rqFini.set();
		fadeInProc();
		fadeIn.enabled.clear();
#else
		fadeIn.rqFini.set();
		SDL_WaitThread(fadeIn.thread, 0);
		fadeIn.thread = 0;
#endif // MKXPZ_RETRO
	}
}

void AudioStream::startFadeIn()
{
	/* Previous fadein should always be terminated in play() */
#ifdef MKXPZ_RETRO
	assert(!fadeIn.enabled);
#else
	assert(!fadeIn.thread);
#endif // MKXPZ_RETRO

	fadeIn.rqFini.clear();
	fadeIn.rqTerm.clear();
#ifdef MKXPZ_RETRO
	fadeIn.startTicks = mkxp_retro::get_ticks_ms();
#else
	fadeIn.startTicks = SDL_GetTicks64();
#endif // MKXPZ_RETRO

#ifdef MKXPZ_RETRO
	fadeIn.enabled.set();
#else
	fadeIn.thread = createSDLThread
		<AudioStream, &AudioStream::fadeInThread>(this, fadeIn.threadName);
#endif // MKXPZ_RETRO
}

bool AudioStream::fadeOutProc()
{
	/* Just immediately terminate on request */
	if (fade.reqTerm)
	{
		fade.active.clear();
		return false;
	}

#ifdef MKXPZ_RETRO
	AudioMutexGuard fadeGuard(fade.mutex);
	if (fade.reqTerm)
	{
		fade.active.clear();
		return false;
	}
#endif // MKXPZ_RETRO

	AudioMutexGuard guard(mutex);

#ifdef MKXPZ_RETRO
	uint64_t curDur = mkxp_retro::get_ticks_ms() - fade.startTicks;
#else
	uint64_t curDur = SDL_GetTicks64() - fade.startTicks;
#endif // MKXPZ_RETRO
	float resVol = 1.0f - (curDur*fade.msStep);

	ALStream::State state = stream.queryState();

	if (state != ALStream::Playing
	|| resVol < 0
	|| fade.reqFini)
	{
		if (state != ALStream::Paused)
			stream.stop();

		setVolume(FadeOut, 1.0f);

		fade.active.clear();
		return false;
	}

	setVolume(FadeOut, resVol);

	return true;
}

bool AudioStream::fadeInProc()
{
	if (fadeIn.rqTerm)
		return false;

#ifdef MKXPZ_RETRO
	AudioMutexGuard fadeGuard(fadeIn.mutex);
	if (fadeIn.rqTerm)
		return false;
#endif // MKXPZ_RETRO

	AudioMutexGuard guard(mutex);

	/* Fade in duration is always 1 second */
#ifdef MKXPZ_RETRO
	uint64_t cur = mkxp_retro::get_ticks_ms() - fadeIn.startTicks;
#else
	uint64_t cur = SDL_GetTicks64() - fadeIn.startTicks;
#endif // MKXPZ_RETRO
	float prog = cur / 1000.0f;

	ALStream::State state = stream.queryState();

	if (state != ALStream::Playing
	||  prog >= 1.0f
	||  fadeIn.rqFini)
	{
		setVolume(FadeIn, 1.0f);

		return false;
	}

	setVolume(FadeIn, prog);

	return true;
}

#ifdef MKXPZ_RETRO
void AudioStream::render()
{
	if (fade.enabled && !fadeOutProc())
		fade.enabled.clear();

	if (fadeIn.enabled && !fadeInProc())
		fadeIn.enabled.clear();

	stream.render();
}

#ifndef MKXPZ_SANDBOX_SERIAL_AUDIOSTREAM_H
#define MKXPZ_SANDBOX_SERIAL_AUDIOSTREAM_H
#include "sandbox-serial-audiostream.h"
#endif // MKXPZ_SANDBOX_SERIAL_AUDIOSTREAM_H
#else
void AudioStream::fadeOutThread()
{
	while (fadeOutProc())
		SDL_Delay(AUDIO_SLEEP);
}

void AudioStream::fadeInThread()
{
	while (fadeInProc())
		SDL_Delay(AUDIO_SLEEP);
}
#endif // MKXPZ_RETRO
