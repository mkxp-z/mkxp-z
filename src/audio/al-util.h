/*
** al-util.h
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

#ifndef ALUTIL_H
#define ALUTIL_H

#include <al.h>
#include <alext.h>

#ifdef MKXPZ_RETRO
#  include <memory>
#  include "filesystem.h"
#else
#  include <SDL_audio.h>
#endif // MKXPZ_RETRO
#include <assert.h>
#include <cmath>

#define GLOBAL_VOLUME 1.0f

namespace AL
{

#define DEF_AL_ID \
struct ID \
{ \
	ALuint al; \
	explicit ID(ALuint al = 0)  \
	    : al(al)  \
	{}  \
	ID &operator=(const ID &o)  \
	{  \
		al = o.al;  \
		return *this; \
	}  \
	bool operator==(const ID &o) const  \
	{  \
		return al == o.al;  \
	}  \
};

namespace Buffer
{
	DEF_AL_ID

	inline Buffer::ID gen()
	{
		Buffer::ID id;
		alGenBuffers(1, &id.al);

		return id;
	}

	inline void del(Buffer::ID id)
	{
		alDeleteBuffers(1, &id.al);
	}

	inline void uploadData(Buffer::ID id, ALenum format, const ALvoid *data, ALsizei size, ALsizei freq)
	{
		alBufferData(id.al, format, data, size, freq);
	}

	inline ALint getInteger(Buffer::ID id, ALenum prop)
	{
		ALint value;
		alGetBufferi(id.al, prop, &value);

		return value;
	}

	inline ALint getSize(Buffer::ID id)
	{
		return getInteger(id, AL_SIZE);
	}

	inline ALint getBits(Buffer::ID id)
	{
		return getInteger(id, AL_BITS);
	}

	inline ALint getChannels(Buffer::ID id)
	{
		return getInteger(id, AL_CHANNELS);
	}
}

namespace Source
{
	DEF_AL_ID

	inline Source::ID gen()
	{
		Source::ID id;
		alGenSources(1, &id.al);

		return id;
	}

	inline void del(Source::ID id)
	{
		alDeleteSources(1, &id.al);
	}

	inline void attachBuffer(Source::ID id, Buffer::ID buffer)
	{
		alSourcei(id.al, AL_BUFFER, buffer.al);
	}

	inline void detachBuffer(Source::ID id)
	{
		attachBuffer(id, Buffer::ID(0));
	}

	inline void queueBuffer(Source::ID id, Buffer::ID buffer)
	{
		alSourceQueueBuffers(id.al, 1, &buffer.al);
	}

	inline Buffer::ID unqueueBuffer(Source::ID id)
	{
		Buffer::ID buffer;
		alSourceUnqueueBuffers(id.al, 1, &buffer.al);

		return buffer;
	}

	inline void clearQueue(Source::ID id)
	{
		attachBuffer(id, Buffer::ID(0));
	}

	inline ALint getInteger(Source::ID id, ALenum prop)
	{
		ALint value;
		alGetSourcei(id.al, prop, &value);

		return value;
	}

	inline ALint getProcBufferCount(Source::ID id)
	{
		return getInteger(id, AL_BUFFERS_PROCESSED);
	}

	inline ALenum getState(Source::ID id)
	{
		return getInteger(id, AL_SOURCE_STATE);
	}

	inline ALfloat getSecOffset(Source::ID id)
	{
		ALfloat value;
		alGetSourcef(id.al, AL_SEC_OFFSET, &value);

		return value;
	}

	inline void setVolume(Source::ID id, float value)
	{
		/*
		 * RPG Maker uses a -35 decibel scale for volume. 100% volume is 0 dB, 99%
		 * volume is -0.35 dB, 98% volume is -0.7 dB, 97% volume is -1.05 dB and so
		 * on. 0% volume is an exception - the scale is hardcoded to be silent at 0%
		 * volume.
		 *
		 * This was deduced by running an RPG Maker XP game in Wine and attaching
		 * winedbg to the game, with a breakpoint set in the
		 * `IDirectSoundBuffer8::SetVolume` function in Wine's implementation of
		 * dsound.dll. Chances are your Wine installation's dsound.dll will be
		 * stripped, but you can easily find that function in Ghidra or whatever by
		 * searching for strings, with the help of the source code of the function as
		 * a reference:
		 *
		 * https://github.com/wine-mirror/wine/blob/1941a915368b8898da21d0dd4157fd68a4f4c9dd/dlls/dsound/buffer.c#L203
		 *
		 * Once you have a breakpoint at `IDirectSoundBuffer8::SetVolume`, all you
		 * need to do is look at the stack in winedbg whenever the breakpoint is hit,
		 * where the arguments to the function will be. Keep in mind that RPG Maker
		 * calls `IDirectSoundBuffer8::SetVolume` once per frame per sound source, so
		 * it'll be much easier for you if you modify your game's scripts to just play
		 * one BGM sound at a known volume, and no other sounds.
		 */
		if (value > FLT_EPSILON) {
			value = std::pow(10.0f, -(35.0f / 20.0f) * (1.0f - value));
		}
		alSourcef(id.al, AL_GAIN, value * GLOBAL_VOLUME);
	}

	inline void setPitch(Source::ID id, float value)
	{
		alSourcef(id.al, AL_PITCH, value);
	}

	inline void play(Source::ID id)
	{
		alSourcePlay(id.al);
	}

	inline void stop(Source::ID id)
	{
		alSourceStop(id.al);
	}

	inline void pause(Source::ID id)
	{
		alSourcePause(id.al);
	}
}

}

#ifndef MKXPZ_RETRO
inline uint8_t formatSampleSize(int sdlFormat)
{
	switch (sdlFormat)
	{
	case AUDIO_U8 :
	case AUDIO_S8 :
		return 1;

	case AUDIO_U16LSB :
	case AUDIO_U16MSB :
	case AUDIO_S16LSB :
	case AUDIO_S16MSB :
		return 2;

    case AUDIO_S32LSB :
    case AUDIO_S32MSB :
    case AUDIO_F32LSB :
    case AUDIO_F32MSB :
        return 4;

	default :
            assert(!"Unhandled sample format");
	}

	return 0;
}
#endif // MKXPZ_RETRO

inline ALenum chooseALFormat(int sampleSize, int channelCount)
{
	switch (sampleSize)
	{
	case 1 :
		switch (channelCount)
		{
		case 1 : return AL_FORMAT_MONO8;
		case 2 : return AL_FORMAT_STEREO8;
		}
	case 2 :
		switch (channelCount)
		{
		case 1 : return AL_FORMAT_MONO16;
		case 2 : return AL_FORMAT_STEREO16;
		}
    case 4 :
        switch (channelCount)
        {
        case 1 : return AL_FORMAT_MONO_FLOAT32;
        case 2 : return AL_FORMAT_STEREO_FLOAT32;
        }
	default :
		assert(!"Unhandled sample size / channel count");
	}

	return 0;
}

#define AUDIO_SLEEP 10
#define STREAM_BUF_SIZE 32768

#endif // ALUTIL_H
