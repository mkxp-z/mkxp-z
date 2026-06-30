/*
** aldatasource.h
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

#ifndef ALDATASOURCE_H
#define ALDATASOURCE_H

#include "al-util.h"
#include <string>

struct ALDataSource
{
	enum Status
	{
		NoError,
		EndOfStream,
		WrapAround,
		Error
	};

	virtual ~ALDataSource() {}

	/* Read/process next chunk of data, and attach it
	 * to provided AL buffer */
	virtual Status fillBuffer(AL::Buffer::ID alBuffer) = 0;

	virtual int sampleRate() = 0;

	/* If the source doesn't support seeking, it will
	 * reset back to the beginning */
	virtual void seekToOffset(double seconds) = 0;

	/* The frame count right after wrap around */
	virtual uint64_t loopStartFrames() = 0;

	/* Returns false if not supported */
	virtual bool setPitch(float value) = 0;
};

ALDataSource *createSDLSource(
				std::string &error,
#ifdef MKXPZ_RETRO
				std::shared_ptr<struct FileSystem::File> ops,
#else
				SDL_RWops &ops,
#endif // MKXPZ_RETRO
                              const char *extension,
			                  uint32_t maxBufSize,
			                  bool looped);

ALDataSource *createSndfileSource(
				std::string &error,
#ifdef MKXPZ_RETRO
				std::shared_ptr<struct FileSystem::File> ops,
#else
				SDL_RWops &ops,
#endif // MKXPZ_RETRO
				bool looped);

ALDataSource *createVorbisSource(
				std::string &error,
#ifdef MKXPZ_RETRO
				std::shared_ptr<struct FileSystem::File> ops,
#else
				SDL_RWops &ops,
#endif // MKXPZ_RETRO
                                 bool looped);

ALDataSource *createMidiSource(
				std::string &error,
#ifdef MKXPZ_RETRO
				std::shared_ptr<struct FileSystem::File> ops,
#else
				SDL_RWops &ops,
#endif // MKXPZ_RETRO
                               bool looped);

#endif // ALDATASOURCE_H
