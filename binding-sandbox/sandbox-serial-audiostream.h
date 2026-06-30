/*
** sandbox-serial-audiostream.h
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

#ifndef MKXPZ_SANDBOX_SERIAL_AUDIOSTREAM_H
#define MKXPZ_SANDBOX_SERIAL_AUDIOSTREAM_H
#include "audiostream.cpp"
#endif // MKXPZ_SANDBOX_SERIAL_AUDIOSTREAM_H

bool AudioStream::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	AudioMutexGuard guard(mutex);

	ALStream::State state = stream.queryState();

	{
		bool enabled = fade.enabled;
		if (!mkxp_sandbox::sandbox_serialize(enabled, data, max_size)) return false;
		if (enabled) {
			if (!mkxp_sandbox::sandbox_serialize(fade.startTicks, data, max_size)) return false;
			if (!mkxp_sandbox::sandbox_serialize(fade.msStep, data, max_size)) return false;
		}
	}

	{
		bool enabled = fadeIn.enabled;
		if (!mkxp_sandbox::sandbox_serialize(enabled, data, max_size)) return false;
		if (enabled) {
			if (!mkxp_sandbox::sandbox_serialize(fadeIn.startTicks, data, max_size)) return false;
		}
	}

	if (!mkxp_sandbox::sandbox_serialize(current.filename, data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_serialize(state, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(playingOffset(), data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_serialize(current.pitch, data, max_size)) return false;

	for (int i = 0; i < AudioStream::VolumeType::VolumeTypeCount; ++i) {
		if (!mkxp_sandbox::sandbox_serialize(volumes[i], data, max_size)) return false;
	}

	return true;
}

bool AudioStream::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	{
		bool enabled;
		if (!mkxp_sandbox::sandbox_deserialize(enabled, data, max_size)) return false;
		if (enabled) {
			if (fade.enabled) {
				AudioMutexGuard guard(fade.mutex);
				if (!mkxp_sandbox::sandbox_deserialize(fade.startTicks, data, max_size)) return false;
				if (!mkxp_sandbox::sandbox_deserialize(fade.msStep, data, max_size)) return false;
			} else {
				if (!mkxp_sandbox::sandbox_deserialize(fade.startTicks, data, max_size)) return false;
				if (!mkxp_sandbox::sandbox_deserialize(fade.msStep, data, max_size)) return false;
				fade.active.set();
				fade.reqFini.clear();
				fade.reqTerm.clear();
				fade.enabled.set();
			}
		} else if (fade.enabled) {
			fade.reqTerm.set();
			{
				AudioMutexGuard guard(fade.mutex);
			}
			fade.reqTerm.clear();
			fade.reqFini.set();
			fadeOutProc();
			fade.enabled.clear();
		}
	}

	{
		bool enabled;
		if (!mkxp_sandbox::sandbox_deserialize(enabled, data, max_size)) return false;
		if (enabled) {
			if (fadeIn.enabled) {
				AudioMutexGuard guard(fadeIn.mutex);
				if (!mkxp_sandbox::sandbox_deserialize(fadeIn.startTicks, data, max_size)) return false;
			} else {
				if (!mkxp_sandbox::sandbox_deserialize(fadeIn.startTicks, data, max_size)) return false;
				fadeIn.rqFini.clear();
				fadeIn.rqTerm.clear();
				fadeIn.enabled.set();
			}
		} else if (fadeIn.enabled) {
			fadeIn.rqTerm.set();
			{
				AudioMutexGuard guard(fadeIn.mutex);
			}
			fadeIn.rqTerm.clear();
			fadeIn.rqFini.set();
			fadeOutProc();
			fadeIn.enabled.clear();
		}
	}

	AudioMutexGuard guard(mutex);

	{
		std::string value;
		if (!mkxp_sandbox::sandbox_deserialize(current.filename, data, max_size)) return false;
		if (current.filename != value) {
			Exception e;
			stream.open(e, current.filename);
			if (e.is_error()) return false;
		}
	}

	ALStream::State state;
	if (!mkxp_sandbox::sandbox_deserialize(state, data, max_size)) return false;
	double offset;
	if (!mkxp_sandbox::sandbox_deserialize(offset, data, max_size)) return false;
	if (state != stream.queryState()) {
		stream.stop();
		switch (state) {
			case ALStream::State::Playing:
				stream.needsRewind.set();
				stream.play(offset);
				break;
			case ALStream::State::Paused:
				stream.needsRewind.set();
				stream.play(offset);
				stream.pause();
				break;
			case ALStream::State::Stopped:
			default:
				break;
		}
	}

	if (!mkxp_sandbox::sandbox_deserialize(current.pitch, data, max_size)) return false;
	stream.setPitch(current.pitch);

	for (int i = 0; i < AudioStream::VolumeType::VolumeTypeCount; ++i) {
		if (!mkxp_sandbox::sandbox_deserialize(volumes[i], data, max_size)) return false;
	}
	updateVolume();

	return true;
}
