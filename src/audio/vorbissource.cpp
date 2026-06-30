/*
** vorbissource.cpp
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

#include "aldatasource.h"
#include "exception.h"

#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>
#include <vector>
#include <algorithm>
#include <cstring>

static size_t vfRead(void *ptr, size_t size, size_t nmemb, void *ops)
{
#ifdef MKXPZ_RETRO
	return PHYSFS_readBytes(static_cast<struct FileSystem::File*>(ops)->get_read(), ptr, size * nmemb) / size;
#else
	return SDL_RWread(static_cast<SDL_RWops*>(ops), ptr, size, nmemb);
#endif // MKXPZ_RETRO
}

static int vfSeek(void *ops, ogg_int64_t offset, int whence)
{
#ifdef MKXPZ_RETRO
	switch (whence) {
		case SEEK_CUR:
			{
				uint64_t pos = PHYSFS_tell(static_cast<struct FileSystem::File*>(ops)->get_read());
				if (pos != (uint64_t)-1) {
					offset = (uint64_t)offset + pos;
				}
			}
			break;
		case SEEK_END:
			{
				offset = (uint64_t)offset + (uint64_t)PHYSFS_fileLength(static_cast<struct FileSystem::File*>(ops)->get_read());
			}
			break;
	}
	PHYSFS_seek(static_cast<struct FileSystem::File*>(ops)->get_read(), offset);
	return offset;
#else
	return SDL_RWseek(static_cast<SDL_RWops*>(ops), offset, whence);
#endif // MKXPZ_RETRO
}

static long vfTell(void *ops)
{
#ifdef MKXPZ_RETRO
	return PHYSFS_tell(static_cast<struct FileSystem::File*>(ops)->get_read());
#else
	return SDL_RWtell(static_cast<SDL_RWops*>(ops));
#endif // MKXPZ_RETRO
}

static ov_callbacks OvCallbacks =
{
    vfRead,
    vfSeek,
    0,
    vfTell
};


struct VorbisSource : ALDataSource
{
#ifdef MKXPZ_RETRO
	std::shared_ptr<struct FileSystem::File> src;
#else
	SDL_RWops src;
#endif // MKXPZ_RETRO

	OggVorbis_File vf;

	uint32_t currentFrame;

	struct
	{
		uint64_t start;
		uint64_t length;
		uint64_t end;
		bool valid;
		bool requested;
	} loop;

	struct
	{
		int channels;
		int rate;
		int frameSize;
		ALenum alFormat;
	} info;

	std::vector<int16_t> sampleBuf;

	VorbisSource(
			std::string &error_,
#ifdef MKXPZ_RETRO
			std::shared_ptr<struct FileSystem::File> ops,
#else
			SDL_RWops &ops,
#endif // MKXPZ_RETRO
	             bool looped)
	    : src(ops),
	      currentFrame(0)
	{
#ifdef MKXPZ_RETRO
		int error = ov_open_callbacks(src.get(), &vf, 0, 0, OvCallbacks);
#else
		int error = ov_open_callbacks(&src, &vf, 0, 0, OvCallbacks);
#endif // MKXPZ_RETRO

		if (error)
		{
#ifndef MKXPZ_RETRO
			SDL_RWclose(&src);
#endif // MKXPZ_RETRO
			error_ = "Vorbisfile: Cannot read ogg file";
			return;
		}

		/* Extract bitstream info */
		info.channels = vf.vi->channels;
		info.rate = vf.vi->rate;

		if (info.channels > 2)
		{
			ov_clear(&vf);
#ifndef MKXPZ_RETRO
			SDL_RWclose(&src);
#endif // MKXPZ_RETRO
			
			error_ = "Cannot handle audio with more than 2 channels";
			return;
		}

		info.alFormat = chooseALFormat(sizeof(int16_t), info.channels);
		info.frameSize = sizeof(int16_t) * info.channels;

		sampleBuf.resize(STREAM_BUF_SIZE);

		loop.requested = looped;
		loop.valid = false;
		loop.start = loop.length = 0;

		if (!loop.requested)
			return;

		/* Try to extract loop info */
		for (int i = 0; i < vf.vc->comments; ++i)
		{
			char *comment = vf.vc->user_comments[i];
			char *sep = strstr(comment, "=");

			/* No '=' found */
			if (!sep)
				continue;

			/* Empty value */
			if (!*(sep+1))
				continue;

			*sep = '\0';

			if (!strcmp(comment, "LOOPSTART"))
				loop.start = strtol(sep+1, 0, 10);

			if (!strcmp(comment, "LOOPLENGTH"))
				loop.length = strtol(sep+1, 0, 10);

			*sep = '=';
		}

		loop.end = loop.start + loop.length;
		loop.valid = (loop.start && loop.length);
	}

	~VorbisSource()
	{
		ov_clear(&vf);
#ifndef MKXPZ_RETRO
		SDL_RWclose(&src);
#endif // MKXPZ_RETRO
	}

	int sampleRate()
	{
		return info.rate;
	}

	void seekToOffset(double seconds)
	{
		if (seconds <= 0)
		{
			ov_raw_seek(&vf, 0);
			currentFrame = 0;
		}

		currentFrame = lround(seconds * info.rate);

		if (loop.valid && currentFrame > loop.end)
			currentFrame = loop.start;

		/* If seeking fails, just seek back to start */
		if (ov_pcm_seek(&vf, currentFrame) != 0)
			ov_raw_seek(&vf, 0);
	}

	Status fillBuffer(AL::Buffer::ID alBuffer)
	{
		void *bufPtr = sampleBuf.data();
		uint64_t availBuf = sampleBuf.size();
		uint64_t bufUsed  = 0;

		uint64_t canRead = availBuf;

		Status retStatus = ALDataSource::NoError;

		bool readAgain = false;

		if (loop.valid)
		{
			uint64_t tilLoopEnd = loop.end * info.frameSize;

			canRead = std::min(availBuf, tilLoopEnd);
		}

		while (canRead > 16)
		{
			long res = ov_read(&vf, static_cast<char*>(bufPtr),
			                   canRead, 0, sizeof(int16_t), 1, 0);

			if (res < 0)
			{
				/* Read error */
				retStatus = ALDataSource::Error;

				break;
			}

			if (res == 0)
			{
				/* EOF */
				if (loop.requested)
				{
					retStatus = ALDataSource::WrapAround;
					seekToOffset(0);
				}
				else
				{
					retStatus = ALDataSource::EndOfStream;
				}

				/* If we sought right to the end of the file,
				 * we might be EOF without actually having read
				 * any data at all yet (which mustn't happen),
				 * so we try to continue reading some data. */
				if (bufUsed > 0)
					break;

				if (readAgain)
				{
					/* We're still not getting data though.
					 * Just error out to prevent an endless loop */
					retStatus = ALDataSource::Error;
					break;
				}

				readAgain = true;
			}

			bufUsed += (res / sizeof(int16_t));
			bufPtr = &sampleBuf[bufUsed];
			currentFrame += (res / info.frameSize);

			if (loop.valid && currentFrame >= loop.end)
			{
				/* Determine how many frames we're
				 * over the loop end */
				uint64_t discardFrames = currentFrame - loop.end;
				bufUsed -= discardFrames * info.channels;

				retStatus = ALDataSource::WrapAround;

				/* Seek to loop start */
				currentFrame = loop.start;
				if (ov_pcm_seek(&vf, currentFrame) != 0)
					retStatus = ALDataSource::Error;

				break;
			}

			canRead -= res;
		}

		if (retStatus != ALDataSource::Error)
			AL::Buffer::uploadData(alBuffer, info.alFormat, sampleBuf.data(),
			                       bufUsed*sizeof(int16_t), info.rate);

		return retStatus;
	}

	uint64_t loopStartFrames()
	{
		if (loop.valid)
			return loop.start;
		else
			return 0;
	}

	bool setPitch(float)
	{
		return false;
	}
};

ALDataSource *createVorbisSource(
				std::string &error,
#ifdef MKXPZ_RETRO
				std::shared_ptr<struct FileSystem::File> ops,
#else
				SDL_RWops &ops,
#endif // MKXPZ_RETRO
                                 bool looped)
{
	error.clear();
	VorbisSource *source = new VorbisSource(error, ops, looped);
	if (error.empty())
		return source;
	delete source;
	return nullptr;
}
