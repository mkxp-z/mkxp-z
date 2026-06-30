/*
** sandbox-serial-audio.h
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

#ifndef MKXPZ_SANDBOX_SERIAL_AUDIO_H
#define MKXPZ_SANDBOX_SERIAL_AUDIO_H
#include "audio.cpp"
#endif // MKXPZ_SANDBOX_SERIAL_AUDIO_H

bool Audio::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	AudioMutexGuard guard(p->meWatch.mutex);

	if (!mkxp_sandbox::sandbox_serialize(p->meWatch.state, data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_serialize((mkxp_sandbox::wasm_size_t)p->bgmTracks.size(), data, max_size)) return false;

	for (AudioStream *track : p->bgmTracks) {
		if (!track->sandbox_serialize(data, max_size)) return false;
	}

	if (!p->bgs.sandbox_serialize(data, max_size)) return false;

	if (!p->me.sandbox_serialize(data, max_size)) return false;

	if (!p->se.sandbox_serialize(data, max_size)) return false;

	return true;
}

bool Audio::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	AudioMutexGuard guard(p->meWatch.mutex);

	if (!mkxp_sandbox::sandbox_deserialize(p->meWatch.state, data, max_size)) return false;

	{
		mkxp_sandbox::wasm_size_t count;
		if (!mkxp_sandbox::sandbox_deserialize(count, data, max_size)) return false;
		if (count != p->bgmTracks.size()) return false;
	}

	for (AudioStream *track : p->bgmTracks) {
		if (!track->sandbox_deserialize(data, max_size)) return false;
	}

	if (!p->bgs.sandbox_deserialize(data, max_size)) return false;

	if (!p->me.sandbox_deserialize(data, max_size)) return false;

	if (!p->se.sandbox_deserialize(data, max_size)) return false;

	return true;
}
