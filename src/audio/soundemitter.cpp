/*
** soundemitter.cpp
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

#include "soundemitter.h"

#include "mkxp-polyfill.h" // snprintf

#include "sharedstate.h"
#include "filesystem.h"
#include "exception.h"
#include "config.h"
#include "util.h"
#include "debugwriter.h"

#ifdef MKXPZ_RETRO
#  include <sndfile.hh>
#  include "sandbox-serial-util.h"
#else
#  include <SDL_sound.h>
#endif // MKXPZ_RETRO

#define SE_CACHE_MEM (10*1024*1024) // 10 MB

struct SoundBuffer
{
	/* Uniquely identifies this or equal buffer */
	std::string key;

	AL::Buffer::ID alBuffer;

	/* Link into the buffer cache priority list */
	IntruListLink<SoundBuffer> link;

	/* Buffer byte count */
	uint32_t bytes;

	/* Reference count */
	uint8_t refCount;

	SoundBuffer()
	    : link(this),
	      refCount(1)

	{
		alBuffer = AL::Buffer::gen();
	}

	static SoundBuffer *ref(SoundBuffer *buffer)
	{
		++buffer->refCount;

		return buffer;
	}

	static void deref(SoundBuffer *buffer)
	{
		if (--buffer->refCount == 0)
			delete buffer;
	}

private:
	~SoundBuffer()
	{
		AL::Buffer::del(alBuffer);
	}
};

/* Before: [a][b][c][d], After (index=1): [a][c][d][b] */
static void
arrayPushBack(std::vector<size_t> &array, size_t size, size_t index)
{
	size_t v = array[index];

	for (size_t t = index; t < size-1; ++t)
		array[t] = array[t+1];

	array[size-1] = v;
}

SoundEmitter::SoundEmitter(const Config &conf)
    : bufferBytes(0),
      srcCount(conf.SE.sourceCount),
      alSrcs(srcCount),
      atchBufs(srcCount),
      filenames(srcCount),
      srcPrio(srcCount)
{
	for (size_t i = 0; i < srcCount; ++i)
	{
		alSrcs[i] = AL::Source::gen();
		atchBufs[i] = 0;
		srcPrio[i] = i;
	}
}

SoundEmitter::~SoundEmitter()
{
	for (size_t i = 0; i < srcCount; ++i)
	{
		AL::Source::stop(alSrcs[i]);
		AL::Source::del(alSrcs[i]);

		if (atchBufs[i])
			SoundBuffer::deref(atchBufs[i]);
	}

	BufferHash::const_iterator iter;
	for (iter = bufferHash.cbegin(); iter != bufferHash.cend(); ++iter)
		SoundBuffer::deref(iter->second);
}

void SoundEmitter::play(const std::string &filename,
                        int volume,
                        int pitch)
{
	float _volume = clamp<int>(volume, 0, 100) / 100.0f;
	float _pitch  = clamp<int>(pitch, 50, 150) / 100.0f;

	SoundBuffer *buffer = allocateBuffer(filename);

	if (!buffer)
		return;

	/* Try to find first free source */
	size_t i;
	for (i = 0; i < srcCount; ++i)
		if (AL::Source::getState(alSrcs[srcPrio[i]]) != AL_PLAYING)
			break;

	/* If we didn't find any, try to find the lowest priority source
	 * with the same buffer to overtake */
	if (i == srcCount)
		for (size_t j = 0; j < srcCount; ++j)
			if (atchBufs[srcPrio[j]] == buffer)
				i = j;

	/* If we didn't find any, overtake the one with lowest priority */
	if (i == srcCount)
		i = 0;

	size_t srcIndex = srcPrio[i];

	/* Only detach/reattach if it's actually a different buffer */
	bool switchBuffer = (atchBufs[srcIndex] != buffer);

	/* Push the used source to the back of the priority list */
	arrayPushBack(srcPrio, srcCount, i);

	AL::Source::ID src = alSrcs[srcIndex];
	AL::Source::stop(src);

	if (switchBuffer)
		AL::Source::detachBuffer(src);

	SoundBuffer *old = atchBufs[srcIndex];

	if (old)
		SoundBuffer::deref(old);

	atchBufs[srcIndex] = SoundBuffer::ref(buffer);

	if (switchBuffer)
		AL::Source::attachBuffer(src, buffer->alBuffer);

	AL::Source::setVolume(src, _volume);
	AL::Source::setPitch(src, _pitch);

	AL::Source::play(src);

	filenames[srcIndex] = filename;
}

void SoundEmitter::stop()
{
	for (size_t i = 0; i < srcCount; i++)
		AL::Source::stop(alSrcs[i]);
}

struct SoundOpenHandler : FileSystem::OpenHandler
{
#ifdef MKXPZ_RETRO
	int errnum;
#endif // MKXPZ_RETRO
	SoundBuffer *buffer;

	SoundOpenHandler()
	    : buffer(0)
	{}

	bool tryRead(
#ifdef MKXPZ_RETRO
		std::shared_ptr<struct FileSystem::File> ops,
#else
		SDL_RWops &ops,
#endif // MKXPZ_RETRO
		const char *ext
	) {
#ifdef MKXPZ_RETRO
		extern SF_VIRTUAL_IO sfvirtual;
		SndfileHandle handle(sfvirtual, ops.get());
#else
		Sound_Sample *sample = Sound_NewSample(&ops, ext, 0, STREAM_BUF_SIZE);
#endif // MKXPZ_RETRO

#ifdef MKXPZ_RETRO
		if ((errnum = handle.error()))
#else
		if (!sample)
#endif // MKXPZ_RETRO
		{
#ifndef MKXPZ_RETRO
			SDL_RWclose(&ops);
#endif // MKXPZ_RETRO
			return false;
		}

		/* Do all of the decoding in the handler so we don't have
		 * to keep the source ops around */
#ifdef MKXPZ_RETRO
		uint8_t sampleSize = 2;
		uint32_t sampleCount = (uint32_t)handle.frames() * (uint32_t)handle.channels();
#else
		uint32_t decBytes = Sound_DecodeAll(sample);
		uint8_t sampleSize = formatSampleSize(sample->actual.format);
		uint32_t sampleCount = decBytes / sampleSize;
#endif // MKXPZ_RETRO

		buffer = new SoundBuffer;
		buffer->bytes = sampleSize * sampleCount;

		ALenum alFormat = chooseALFormat(
			sampleSize,
#ifdef MKXPZ_RETRO
			handle.channels()
#else
			sample->actual.channels
#endif // MKXPZ_RETRO
		);

#ifdef MKXPZ_RETRO
		int16_t *buf = (int16_t *)std::malloc(buffer->bytes);
		if (buf == NULL) {
			return false;
		}
		handle.read(buf, sampleCount);
		AL::Buffer::uploadData(buffer->alBuffer, alFormat, buf,
							   buffer->bytes, handle.samplerate());
		std::free(buf);
#else
		AL::Buffer::uploadData(buffer->alBuffer, alFormat, sample->buffer,
							   buffer->bytes, sample->actual.rate);
		Sound_FreeSample(sample);
#endif // MKXPZ_RETRO

		return true;
	}
};

SoundBuffer *SoundEmitter::allocateBuffer(const std::string &filename)
{
	SoundBuffer *buffer = bufferHash.value(filename, 0);

	if (buffer)
	{
		/* Buffer still in cashe.
		 * Move to front of priority list */
		buffers.remove(buffer->link);
		buffers.append(buffer->link);

		return buffer;
	}
	else
	{
		/* Buffer not in cache, needs to be loaded */
		SoundOpenHandler handler;
#ifdef MKXPZ_RETRO
		mkxp_retro::fs->openRead(handler, filename.c_str()); // TODO: move into shState
#else
		shState->fileSystem().openRead(handler, filename.c_str());
#endif // MKXPZ_RETRO
		buffer = handler.buffer;

		if (!buffer)
		{
			char buf[512];
			snprintf(
				buf,
				sizeof(buf),
				"Unable to decode sound: %s: %s",
				filename.c_str(),
#ifdef MKXPZ_RETRO
				sf_error_number(handler.errnum)
#else
				Sound_GetError()
#endif // MKXPZ_RETRO
			);
			Debug() << buf;
			return 0;
		}

		buffer->key = filename;
		uint32_t wouldBeBytes = bufferBytes + buffer->bytes;

		/* If memory limit is reached, delete lowest priority buffer
		 * until there is room or no buffers left */
		while (wouldBeBytes > SE_CACHE_MEM && !buffers.isEmpty())
		{
			SoundBuffer *last = buffers.tail();
			bufferHash.remove(last->key);
			buffers.remove(last->link);

			wouldBeBytes -= last->bytes;

			SoundBuffer::deref(last);
		}

		bufferHash.insert(filename, buffer);
		buffers.prepend(buffer->link);

		bufferBytes = wouldBeBytes;

		return buffer;
	}
}

#ifdef MKXPZ_RETRO
#ifndef MKXPZ_SANDBOX_SERIAL_SOUNDEMITTER_H
#define MKXPZ_SANDBOX_SERIAL_SOUNDEMITTER_H
#include "sandbox-serial-soundemitter.h"
#endif // MKXPZ_SANDBOX_SERIAL_SOUNDEMITTER_H
#endif // MKXPZ_RETRO
