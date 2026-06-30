/*
** sandbox-serial-soundemitter.h
**
** This file is part of mkxp.
**
** Copyright (C) 2026 The mkxp-z authors
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

#ifndef MKXPZ_SANDBOX_SERIAL_SOUNDEMITTER_H
#define MKXPZ_SANDBOX_SERIAL_SOUNDEMITTER_H
#include "soundemitter.cpp"
#endif // MKXPZ_SANDBOX_SERIAL_SOUNDEMITTER_H

bool SoundEmitter::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	if (!mkxp_sandbox::sandbox_serialize((mkxp_sandbox::wasm_size_t)srcCount, data, max_size)) return false;

	for (size_t i = 0; i < srcCount; ++i) {
		if (!mkxp_sandbox::sandbox_serialize(filenames[i], data, max_size)) return false;

		AL::Source::ID source = alSrcs[i];
		ALenum state = AL::Source::getState(source);
		if (!mkxp_sandbox::sandbox_serialize((int32_t)state, data, max_size)) return false;

		{
			ALfloat value;
			alGetSourcef(source.al, AL_SEC_OFFSET, &value);
			if (!mkxp_sandbox::sandbox_serialize(value, data, max_size)) return false;
		}
		{
			ALfloat value;
			alGetSourcef(source.al, AL_PITCH, &value);
			if (!mkxp_sandbox::sandbox_serialize(value, data, max_size)) return false;
		}
		{
			ALfloat value;
			alGetSourcef(source.al, AL_GAIN, &value);
			if (!mkxp_sandbox::sandbox_serialize(value, data, max_size)) return false;
		}
	}

	return true;
}

bool SoundEmitter::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	{
		mkxp_sandbox::wasm_size_t count;
		if (!mkxp_sandbox::sandbox_deserialize(count, data, max_size)) return false;
		if (count != srcCount) return false;
	}

	for (size_t i = 0; i < srcCount; ++i) {
		AL::Source::ID source = alSrcs[i];

		{
			std::string value = filenames[i];
			if (!mkxp_sandbox::sandbox_deserialize(filenames[i], data, max_size)) return false;
			SoundBuffer *old_buffer = atchBufs[i];
			if (filenames[i] != value) {
				if (filenames[i].empty()) {
					if (old_buffer != nullptr) {
						AL::Source::stop(source);
						AL::Source::detachBuffer(source);
						SoundBuffer::deref(old_buffer);
					}
					atchBufs[i] = nullptr;
				} else {
					SoundBuffer *new_buffer = allocateBuffer(filenames[i]);
					if (new_buffer == nullptr) return false;
					if (new_buffer != old_buffer) {
						AL::Source::stop(source);
						AL::Source::detachBuffer(source);
						if (old_buffer != nullptr) {
							SoundBuffer::deref(old_buffer);
						}
						atchBufs[i] = SoundBuffer::ref(new_buffer);
						AL::Source::attachBuffer(source, new_buffer->alBuffer);
					}
				}
			}
		}

		int32_t state;
		if (!mkxp_sandbox::sandbox_deserialize(state, data, max_size)) return false;
		if (state != AL::Source::getState(source)) {
			switch (state) {
				case AL_PLAYING:
					AL::Source::play(source);
					break;
				case AL_PAUSED:
					AL::Source::pause(source);
					break;
				case AL_STOPPED:
				default:
					AL::Source::stop(source);
					break;
			}
		}

		{
			ALfloat value;
			if (!mkxp_sandbox::sandbox_deserialize(value, data, max_size)) return false;
			alSourcef(source.al, AL_SEC_OFFSET, value);
		}
		{
			ALfloat value;
			if (!mkxp_sandbox::sandbox_deserialize(value, data, max_size)) return false;
			alSourcef(source.al, AL_PITCH, value);
		}
		{
			ALfloat value;
			if (!mkxp_sandbox::sandbox_deserialize(value, data, max_size)) return false;
			alSourcef(source.al, AL_GAIN, value);
		}
	}

	return true;
}
