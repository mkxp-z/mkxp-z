/*
** sharedmidistate.h
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

#ifndef SHAREDMIDISTATE_H
#define SHAREDMIDISTATE_H

#include "config.h"
#include "debugwriter.h"
#include "fluid-fun.h"

#ifdef MKXPZ_RETRO
#  include "core.h"
#endif // MKXPZ_RETRO

#include <assert.h>
#include <vector>
#include <string>

#define SYNTH_INIT_COUNT 2
#ifdef MKXPZ_RETRO
#  define SYNTH_SAMPLERATE 48000
#else
#  define SYNTH_SAMPLERATE 44100
#endif

struct Synth
{
	fluid_synth_t *synth;
	bool inUse;
};

struct SharedMidiState
{
	bool inited;
	std::vector<Synth> synths;
	const std::string &soundFont;
	fluid_settings_t *flSettings;

	SharedMidiState(const Config &conf)
	    : inited(false),
	      soundFont(conf.midi.soundFont)
	{}

	~SharedMidiState()
	{
		/* We might have initialized, but if the consecutive libfluidsynth
		 * load failed, no resources will have been allocated */
		if (!inited || !HAVE_FLUID)
			return;

		for (size_t i = 0; i < synths.size(); ++i)
		{
			assert(!synths[i].inUse);
			fluid.delete_synth(synths[i].synth);
		}

		fluid.delete_settings(flSettings);
	}

#ifdef MKXPZ_RETRO
	void initIfNeeded(const Config &conf)
	{
		if (inited)
			return;

		inited = true;

		initFluidFunctions();

		if (!HAVE_FLUID)
			return;

		flSettings = fluid.new_settings();
		fluid.settings_setnum(flSettings, "synth.gain", 1.0f);
		fluid.settings_setnum(flSettings, "synth.sample-rate", mkxp_retro::sample_rate);
		fluid.settings_setint(flSettings, "synth.chorus.active", conf.midi.chorus);
		fluid.settings_setint(flSettings, "synth.reverb.active", conf.midi.reverb);

		for (size_t i = 0; i < SYNTH_INIT_COUNT; ++i)
			addSynth(false);
	}
#else
	void initIfNeeded(const Config &conf)
	{
		if (inited)
			return;

		inited = true;

		initFluidFunctions();

		if (!HAVE_FLUID)
			return;

		flSettings = fluid.new_settings();
		fluid.settings_setnum(flSettings, "synth.gain", 1.0f);
		fluid.settings_setnum(flSettings, "synth.sample-rate", SYNTH_SAMPLERATE);
		fluid.settings_setint(flSettings, "synth.chorus.active", conf.midi.chorus);
		fluid.settings_setint(flSettings, "synth.reverb.active", conf.midi.reverb);

		for (size_t i = 0; i < SYNTH_INIT_COUNT; ++i)
			addSynth(false);
	}
#endif // MKXPZ_RETRO

	fluid_synth_t *allocateSynth()
	{
		assert(HAVE_FLUID);
		assert(inited);

		size_t i;

		for (i = 0; i < synths.size(); ++i)
			if (!synths[i].inUse)
				break;

		if (i < synths.size())
		{
			fluid_synth_t *syn = synths[i].synth;
			fluid.synth_system_reset(syn);
			synths[i].inUse = true;

			return syn;
		}
		else
		{
			return addSynth(true);
		}
	}

	void releaseSynth(fluid_synth_t *synth)
	{
		size_t i;

		for (i = 0; i < synths.size(); ++i)
			if (synths[i].synth == synth)
				break;

		assert(i < synths.size());

		synths[i].inUse = false;
	}

private:
	fluid_synth_t *addSynth(bool usedNow)
	{
		fluid_synth_t *syn = fluid.new_synth(flSettings);

#ifdef MKXPZ_RETRO
		extern const uint8_t mkxp_gmgsx_sf2[];
		extern const size_t mkxp_gmgsx_sf2_len;

		struct data {
			union {
				PHYSFS_File *file;
				fluid_long_long_t builtin_offset;
			} inner;
			const bool is_builtin;

			data() noexcept : is_builtin(true) {
				inner.builtin_offset = 0;
			}

			data(PHYSFS_File *file) noexcept : is_builtin(false) {
				inner.file = file;
			}

			~data() {
				if (!is_builtin && inner.file != nullptr)
					PHYSFS_close(inner.file);
			}
		};

		fluid_sfloader_t *loader = new_fluid_defsfloader(flSettings);
		fluid_sfloader_set_callbacks(
			loader,
			[](const char *filename) {
				if (std::strcmp(filename, "/GMGSx.sf2") == 0)
					return (void *)new data;
				PHYSFS_File *file = PHYSFS_openRead(mkxp_retro::fs->normalize(filename, false, true, "/Game").c_str());
				return file == nullptr ? nullptr : (void *)new data(file);
			},
			[](void *buf, fluid_long_long_t count, void *handle) {
				struct data *data = (struct data *)handle;
				if (data->is_builtin)
				{
					assert((size_t)(data->inner.builtin_offset + count) < mkxp_gmgsx_sf2_len);
					std::memcpy(buf, mkxp_gmgsx_sf2 + data->inner.builtin_offset, count);
					data->inner.builtin_offset = (uint64_t)data->inner.builtin_offset + (uint64_t)count;
					return (int)FLUID_OK;
				}
				else
					return PHYSFS_readBytes(data->inner.file, buf, count) == -1 ? (int)FLUID_FAILED : (int)FLUID_OK;
			},
			[](void *handle, fluid_long_long_t offset, int origin) {
				struct data *data = (struct data *)handle;
				if (data->is_builtin)
				{
					switch (origin) {
						case SEEK_CUR:
							data->inner.builtin_offset = (uint64_t)data->inner.builtin_offset + (uint64_t)offset;
							break;
						case SEEK_END:
							data->inner.builtin_offset = (uint64_t)mkxp_gmgsx_sf2_len + (uint64_t)offset;
							break;
						default:
							data->inner.builtin_offset = offset;
							break;
					}
					return (int)FLUID_OK;
				}
				else
				{
					switch (origin) {
						case SEEK_CUR:
							{
								uint64_t pos = PHYSFS_tell(data->inner.file);
								if (pos != (uint64_t)-1) {
									offset = (uint64_t)offset + pos;
								}
							}
							break;
						case SEEK_END:
							{
								offset = (uint64_t)offset + (uint64_t)PHYSFS_fileLength(data->inner.file);
							}
							break;
					}
					return PHYSFS_seek(data->inner.file, offset) ? (int)FLUID_OK : (int)FLUID_FAILED;
				}
			},
			[](void *handle) {
				struct data *data = (struct data *)handle;
				if (data->is_builtin)
					return data->inner.builtin_offset;
				else
				{
					PHYSFS_sint64 pos = PHYSFS_tell(data->inner.file);
					return pos == -1 ? (fluid_long_long_t)FLUID_FAILED : pos;
				}
			},
			[](void *handle) {
				delete (struct data *)handle;
				return (int)FLUID_OK;
			}
		);

		fluid_synth_add_sfloader(syn, loader);
		if (soundFont.empty())
			fluid.synth_sfload(syn, "/GMGSx.sf2", 1);
		else
			fluid.synth_sfload(syn, soundFont.c_str(), 1);
#else
		if (!soundFont.empty())
			fluid.synth_sfload(syn, soundFont.c_str(), 1);
		else
			Debug() << "Warning: No soundfont specified, sound might be mute";
#endif // MKXPZ_RETRO

		Synth synth;
		synth.inUse = usedNow;
		synth.synth = syn;
		synths.push_back(synth);

		return syn;
	}
};

#endif // SHAREDMIDISTATE_H
