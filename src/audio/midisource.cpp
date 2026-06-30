/*
** midisource.cpp
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

#include "al-util.h"
#include "exception.h"
#include "sharedstate.h"
#include "sharedmidistate.h"
#include "util.h"
#include "debugwriter.h"
#include "fluid-fun.h"

#ifdef MKXPZ_RETRO
#  include "core.h"
#else
#  include <SDL_rwops.h>
#endif // MKXPZ_RETRO

#include <assert.h>
#include <math.h>
#include <vector>
#include <algorithm>

/* Vocabulary:
 *
 * Tick:
 *   Ticks are the smallest batch of samples that fluidsynth
 *   allows midi state changes to take effect in, ie. if two midi
 *   events are fired within less than a tick, it will look as
 *   if they fired at the same time. Midisource therefore always
 *   synthesizes sample blocks which are multiples of ticks.
 *   One tick is 64 samples, or 32 frames with two channels.
 *
 * Delta:
 *   Deltas are the abstract time unit in which the relative
 *   offsets between midi events are encoded.
 */

#define TICK_FRAMES 32
#define BUF_TICKS (STREAM_BUF_SIZE / TICK_FRAMES)
#define DEFAULT_BPM 120
#define MAX_CHANNELS 16

#define CC_CTRL_VOLUME       7
#define CC_CTRL_EXPRESSION  11
#define CC_CTRL_LOOP       111

#define CC_VAL_DEFAULT 127

enum MidiEventType
{
	NoteOff,
	NoteOn,
	ChanTouch,
	PitchBend,
	CC,
	PC,
	Tempo,

	Last
};

struct ChannelEvent
{
	uint8_t chan;
};

struct NoteEvent : ChannelEvent
{
	uint8_t key;
	uint8_t vel;
};

struct NoteTouchEvent : ChannelEvent
{
	uint8_t val;
};

struct PitchBendEvent : ChannelEvent
{
	uint16_t val;
};

struct CCEvent : ChannelEvent
{
	uint8_t ctrl;
	uint8_t val;
};

struct PCEvent : ChannelEvent
{
	uint8_t prog;
};

struct TempoEvent
{
	uint32_t bpm;
};

struct MidiEvent
{
	MidiEventType type;
	uint32_t delta;
	union
	{
		ChannelEvent chan;
		NoteEvent note;
		NoteTouchEvent chanTouch;
		PitchBendEvent pitchBend;
		CCEvent cc;
		PCEvent pc;
		TempoEvent tempo;
	} e;
};

struct MidiReadHandler
{
	virtual void onMidiHeader(std::string &error, uint16_t midiType, uint16_t trackCount, uint16_t division) = 0;
	virtual void onMidiTrackBegin() = 0;
	virtual void onMidiEvent(const MidiEvent &e, uint32_t absDelta) = 0;
};

static void
badMidiFormat(std::string &error)
{
	error = "Midi: Bad format";
}

/* File-like interface to a read-only memory buffer */
struct MemChunk
{
	const std::vector<uint8_t> &data;
	size_t i;

	MemChunk(const std::vector<uint8_t> &data)
	    : data(data),
	      i(0)
	{}

	uint8_t readByte(std::string &error)
	{
		if (i >= data.size())
		{
			endOfFile(error);
			return 0;
		}

		return data[i++];
	}

	void readData(std::string &error, void *buf, size_t n)
	{
		if (i + n > data.size())
		{
			endOfFile(error);
			return;
		}

		memcpy(buf, &data[i], n);
		i += n;
	}

	void skipData(std::string &error, size_t n)
	{
		if ((i += n) > data.size())
		{
			endOfFile(error);
			return;
		}
	}

	void endOfFile(std::string &error)
	{
		error = "Midi: EOF";
	}
};

static uint32_t
readVarNum(std::string &error, MemChunk &chunk)
{
	uint32_t result = 0;
	uint8_t byte;
	uint8_t len = 0;

	do
	{
		/* A variable length number can at most be made of 4 bytes */
		if (++len > 4)
		{
			badMidiFormat(error);
			return 0;
		}

		byte = chunk.readByte(error);
		if (!error.empty())
			return 0;
		result = (result << 0x7) | (byte & 0x7F);
	}
	while (byte & 0x80);

	return result;
}

template<typename T>
static T readBigEndian(std::string &error, MemChunk &chunk)
{
	T result = 0;

	for (size_t i = 0; i < sizeof(T) && error.empty(); ++i)
		result = (result << 0x8) | chunk.readByte(error);

	return result;
}

static void
readVoiceEvent(std::string &error, MidiEvent &e, MemChunk &chunk,
               uint8_t type, uint8_t data1, bool &handled)
{
	e.e.chan.chan = (type & 0x0F);

	uint8_t tmp;

	switch (type >> 4)
	{
	case 0x8 :
		e.type = NoteOff;
		e.e.note.key = data1;
		chunk.readByte(error); /* We don't care about velocity */
		break;

	case 0x9 :
		e.type = NoteOn;
		e.e.note.key = data1;
		e.e.note.vel = chunk.readByte(error);
		break;

	case 0xA :
		/* Note aftertouch unhandled */
		handled = false;
		break;

	case 0xB :
		e.type = CC;
		e.e.cc.ctrl = data1;
		e.e.cc.val = chunk.readByte(error);
		break;

	case 0xC :
		e.type = PC;
		e.e.pc.prog = data1;
		break;

	case 0xD :
		e.type = ChanTouch;
		e.e.chanTouch.val = data1;
		break;

	case 0xE :
		e.type = PitchBend;
		tmp = chunk.readByte(error);
		e.e.pitchBend.val = ((tmp & 0x7F) << 7) | (data1 & 0x7F);
		break;

	default :
		assert(!"unreachable");
		break;
	}
}

static void
readEvent(std::string &error, MidiReadHandler *handler, MemChunk &chunk,
          uint8_t &prevType, uint32_t &deltaBase, uint32_t &deltaCarry,
          bool &endOfTrack)
{
	MidiEvent e;
	bool handled = true;

	e.delta = readVarNum(error, chunk);
	if (!error.empty())
		return;

	uint8_t type = chunk.readByte(error);
	if (!error.empty())
		return;

	/* Check for running status */
	if (!(type & 0x80))
	{
		/* Running status for meta/system events is not allowed */
		if (prevType == 0)
		{
			badMidiFormat(error);
			return;
		}

		/* Running status voice event: 'type' becomes
		 * first data byte instead */
		readVoiceEvent(error, e, chunk, prevType, type, handled);
		if (!error.empty())
			return;
		goto event_read;
	}

	if ((type >> 4) != 0xF)
	{
		/* Normal voice event */
		uint8_t byte = chunk.readByte(error);
		if (!error.empty())
			return;
		readVoiceEvent(error, e, chunk, type, byte, handled);
		if (!error.empty())
			return;
		prevType = type;
	}
	else if (type == 0xFF)
	{
		/* Meta event */
		uint8_t metaType = chunk.readByte(error);
		if (!error.empty())
			return;
		uint32_t len = readVarNum(error, chunk);
		if (!error.empty())
			return;

		if (metaType == 0x51)
		{
			/* Tempo event */
			if (len != 3)
			{
				badMidiFormat(error);
				return;
			}

			uint8_t data[3];
			chunk.readData(error, data, 3);
			if (!error.empty())
				return;

			uint32_t mpqn = (data[0] << 0x10)
			              | (data[1] << 0x08)
			              | (data[2] << 0x00);

			e.type = Tempo;
			e.e.tempo.bpm = 60000000 / mpqn;
		}
		else if (metaType == 0x2F)
		{
			/* End-of-track event */
			if (len != 0)
			{
				badMidiFormat(error);
				return;
			}

			endOfTrack = true;
			handled = false;
		}
		else
		{
			handled = false;
		}

		if (!handled)
		{
			chunk.skipData(error, len);
			if (!error.empty())
				return;
		}

		prevType = 0;
	}
	else if (type == 0xF0 || type == 0xF7)
	{
		/* SysEx event */
		uint32_t len = readVarNum(error, chunk);
		if (!error.empty())
			return;
		handled = false;
		chunk.skipData(error, len);
		if (!error.empty())
			return;

		prevType = 0;
	}
	else
	{
		badMidiFormat(error);
		return;
	}

event_read:

	if (handled)
	{
		e.delta += deltaCarry;
		deltaCarry = 0;
		deltaBase += e.delta;

		handler->onMidiEvent(e, deltaBase);
	}
	else
	{
		deltaCarry += e.delta;
	}
}

static void readMidiTrack(std::string &error, MidiReadHandler *handler, MemChunk &chunk)
{
	/* Track signature */
	char sig[5] = { 0 };
	chunk.readData(error, sig, 4);
	if (!error.empty())
		return;
	if (strcmp(sig, "MTrk"))
	{
		badMidiFormat(error);
		return;
	}

	handler->onMidiTrackBegin();

	uint32_t trackLen = readBigEndian<uint32_t>(error, chunk);
	if (!error.empty())
		return;

	/* The combined delta of all events on this track so far */
	uint32_t deltaBase = 0;

	/* If we don't care about an event, instead of inserting a
	 * useless event, we carry over its delta into the next one */
	uint32_t deltaCarry = 0;

	/* Holds the previous status byte for voice event, in case
	 * we encounter a running status */
	uint8_t prevType = 0;

	/* The 'EndOfTrack' meta event will toggle this flag on */
	bool endOfTrack = false;

	size_t savedPos = chunk.i;

	/* Read all events */
	while (!endOfTrack)
	{
		readEvent(error, handler, chunk, prevType, deltaBase, deltaCarry, endOfTrack);
		if (!error.empty())
			return;
	}

	/* Check that the track byte length from the header
	 * matches the amount we actually ended up reading */
	if ((chunk.i - savedPos) != trackLen)
	{
		badMidiFormat(error);
		return;
	}
}

static void readMidi(std::string &error, MidiReadHandler *handler, const std::vector<uint8_t> &data)
{
	MemChunk chunk(data);

	/* Midi signature */
	char sig[5] = { 0 };
	chunk.readData(error, sig, 4);
	if (!error.empty())
		return;
	if (strcmp(sig, "MThd"))
	{
		badMidiFormat(error);
		return;
	}

	/* Header length must always be 6 */
	uint32_t hdrLen = readBigEndian<uint32_t>(error, chunk);
	if (!error.empty())
		return;
	if (hdrLen != 6)
	{
		badMidiFormat(error);
		return;
	}

	/* Only types 0, 1, 2 exist */
	uint16_t type = readBigEndian<uint16_t>(error, chunk);
	if (!error.empty())
		return;
	if (type > 2)
	{
		badMidiFormat(error);
		return;
	}

	/* Type 0 only contains one track */
	uint16_t trackCount = readBigEndian<uint16_t>(error, chunk);
	if (!error.empty())
		return;
	if (trackCount == 0)
	{
		badMidiFormat(error);
		return;
	}
	if (type == 0 && trackCount > 1)
	{
		badMidiFormat(error);
		return;
	}

	uint16_t timeDiv = readBigEndian<uint16_t>(error, chunk);
	if (!error.empty())
		return;

	handler->onMidiHeader(error, type, trackCount, timeDiv);
	if (!error.empty())
		return;

	/* Read tracks */
	for (uint16_t i = 0; i < trackCount; ++i)
	{
		readMidiTrack(error, handler, chunk);
		if (!error.empty())
			return;
	}
}

struct Track
{
	std::vector<MidiEvent> events;

	/* Combined deltas of all events */
	uint64_t length;

	/* Event index that is resumed from after loop wraparound */
	int32_t loopI;

	/* Difference between last event's position and length of
	 * the longest overall track / song length */
	uint32_t loopOffsetEnd;

	/* Delta offset from the loop beginning to event[loopI] */
	uint32_t loopOffsetStart;

	bool valid;
	MidiEvent event;
	int32_t remDeltas;

	uint32_t index;
	bool wrapAroundFlag;
	bool atEnd;

	Track()
	    : length(0),
	      loopI(-1),
	      loopOffsetEnd(0),
	      loopOffsetStart(0),
	      valid(false),
	      index(0),
	      wrapAroundFlag(false),
	      atEnd(false)
	{}

	void appendEvent(const MidiEvent &e)
	{
		length += e.delta;
		events.push_back(e);
	}

	void scheduleEvent(bool looped)
	{
		if (valid)
			return;

		/* At end and not looped? */
		if (index == events.size() && ((!looped || loopI < 0) || length == 0))
		{
			valid = false;
			atEnd = true;
			return;
		}

		MidiEvent &e = events[index++];

		event = e;
		remDeltas = e.delta;
		valid = true;

		if (wrapAroundFlag)
		{
			remDeltas = loopOffsetEnd + loopOffsetStart;
			wrapAroundFlag = false;
		}

		if (looped && (index == events.size()) && loopI >= 0)
		{
			index = loopI;
			wrapAroundFlag = true;
		}
	}

	void reset()
	{
		valid = false;
		index = 0;
		wrapAroundFlag = false;
		atEnd = false;
	}
};

/* Some songs use CC events for effects like fade-out,
 * slowly decreasing a channel's volume to 0. The problem is that
 * for looped songs, events are continuously fed into the synth
 * without restoring those controls back to their default value.
 * We can't reset them at the very beginning of tracks because it
 * might cause audible interactions with notes which are still decaying
 * past the end of the song. Therefore, we insert a fake CC event right
 * after the first NoteOn event for each channel which resets the
 * control to its default state while avoiding audible glitches.
 * If there is already a CC event for this control before the first
 * NoteOn event, we don't clobber it by not inserting our fake event. */
template<uint8_t ctrl>
struct CCResetter
{
	bool chanHandled[MAX_CHANNELS];

	CCResetter()
	{
		memset(&chanHandled, 0, sizeof(chanHandled));
	}

	void handleEvent(const MidiEvent &e, Track &track)
	{
		if (e.type != NoteOn && e.type != CC)
			return;

		uint8_t chan = e.e.chan.chan;

		if (chanHandled[chan])
			return;

		if (e.type == CC && e.e.cc.ctrl == ctrl)
		{
			/* Don't clobber the existing CC value */
			chanHandled[chan] = true;
			return;
		}
		else if (e.type == NoteOn)
		{
			chanHandled[chan] = true;

			MidiEvent re;
			re.delta = 0;
			re.type = CC;
			re.e.cc.chan = chan;
			re.e.cc.ctrl = ctrl;
			re.e.cc.val = CC_VAL_DEFAULT;

			track.appendEvent(re);
		}
	}
};

struct MidiSource : ALDataSource, MidiReadHandler
{
	const uint32_t freq;
	fluid_synth_t *synth;

	int16_t synthBuf[BUF_TICKS*TICK_FRAMES*2];

	std::vector<Track> tracks;
	CCResetter<CC_CTRL_VOLUME>     volReset;
	CCResetter<CC_CTRL_EXPRESSION> expReset;

	/* Index of longest track */
	uint8_t longestI;

	bool looped;

	/* Absolute delta at which we received the LOOP_MARKER CC event */
	uint32_t loopDelta;

	/* Deltas per beat */
	uint16_t dpb;

	int8_t pitchShift;

	/* Deltas per tick */
	float playbackSpeed;

	float genDeltasCarry;

	/* MidiReadHandler (track that's currently being read) */
	int16_t curTrack;

	MidiSource(
		std::string &error,
#ifdef MKXPZ_RETRO
		std::shared_ptr<struct FileSystem::File> ops,
#else
		SDL_RWops &ops,
#endif // MKXPZ_RETRO
	           bool looped)
	    :
#ifdef MKXPZ_RETRO
	      freq(mkxp_retro::sample_rate),
#else
	      freq(SYNTH_SAMPLERATE),
#endif // MKXPZ_RETRO
	      looped(looped),
	      dpb(480),
	      pitchShift(0),
	      genDeltasCarry(0),
	      curTrack(-1)
	{
#ifdef MKXPZ_RETRO
		size_t dataLen = PHYSFS_fileLength(ops->get_read());
#else
		size_t dataLen = SDL_RWsize(&ops);
#endif // MKXPZ_RETRO
		std::vector<uint8_t> data(dataLen);

		if (
#ifdef MKXPZ_RETRO
			PHYSFS_readBytes(ops->get_read(), &data[0], dataLen) < (PHYSFS_sint64)dataLen
#else
			SDL_RWread(&ops, &data[0], 1, dataLen) < dataLen
#endif // MKXPZ_RETRO
		) {
#ifndef MKXPZ_RETRO
			SDL_RWclose(&ops);
#endif // MKXPZ_RETRO
			error = "Reading midi data failed";
			return;
		}

#ifndef MKXPZ_RETRO
		SDL_RWclose(&ops);
#endif // MKXPZ_RETRO

		readMidi(error, this, data);
		if (!error.empty())
			return;

		synth = shState->midiState().allocateSynth();

		uint64_t longest = 0;

		for (size_t i = 0; i < tracks.size(); ++i)
			if (tracks[i].length > longest)
			{
				longest = tracks[i].length;
				longestI = i;
			}

		for (size_t i = 0; i < tracks.size(); ++i)
			tracks[i].loopOffsetEnd = longest - tracks[i].length;

		/* Enterbrain likes to be funny and put loop markers at
		 * the very end of ME tracks */
		if (loopDelta >= longest)
			loopDelta = 0;

		for (size_t i = 0; i < tracks.size(); ++i)
		{
			Track &track = tracks[i];

			int64_t base = 0;
			for (size_t j = 0; j < track.events.size(); ++j)
			{
				MidiEvent &e = track.events[j];
				base += e.delta;

				if (base < loopDelta)
					continue;

				/* Don't make the last event loop eternally */
				if (j < track.events.size())
				{
					track.loopI = j;
					track.loopOffsetStart = base - loopDelta;
				}

				break;
			}
		}

		updatePlaybackSpeed(DEFAULT_BPM);

		// FIXME: It would make the code in 'fillBuffer' a lot nicer if
		// we could combine all tracks into one giant one on construction,
		// instead of having to constantly iterate through all of them
	}

	~MidiSource()
	{
		shState->midiState().releaseSynth(synth);
	}


	void updatePlaybackSpeed(uint32_t bpm)
	{
		float deltaLength = 60.0f / (dpb * bpm);
		playbackSpeed = TICK_FRAMES / (deltaLength * freq);
	}

	void activateEvent(const MidiEvent &e)
	{
		int16_t key = e.e.note.key;

		/* Apply pitch shift if necessary */
		if ((e.type == NoteOn || e.type == NoteOff) && e.e.chan.chan != 9)
		{
			key += pitchShift;

			/* Drop events whose keys are out of bounds */
			if (key < 0 || key > 127)
				return;
		}

		switch (e.type)
		{
		case NoteOn:
			fluid.synth_noteon(synth, e.e.note.chan, key, e.e.note.vel);
			break;
		case NoteOff:
			fluid.synth_noteoff(synth, e.e.note.chan, key);
			break;
		case ChanTouch:
			fluid.synth_channel_pressure(synth, e.e.chanTouch.chan, e.e.chanTouch.val);
			break;
		case PitchBend:
			fluid.synth_pitch_bend(synth, e.e.pitchBend.chan, e.e.pitchBend.val);
			break;
		case CC:
			fluid.synth_cc(synth, e.e.cc.chan, e.e.cc.ctrl, e.e.cc.val);
			break;
		case PC:
			fluid.synth_program_change(synth, e.e.pc.chan, e.e.pc.prog);
			break;
		case Tempo:
			updatePlaybackSpeed(e.e.tempo.bpm);
			break;
		default:
			break;
		}
	}

	void renderTicks(size_t count, size_t offset)
	{
		size_t bufOffset = offset * TICK_FRAMES * 2;
		uint64_t len = count * TICK_FRAMES;
		void *buffer = &synthBuf[bufOffset];

		fluid.synth_write_s16(synth, len, buffer, 0, 2, buffer, 1, 2);
	}

	/* MidiReadHandler */
	void onMidiHeader(std::string &error, uint16_t midiType, uint16_t trackCount, uint16_t division)
	{
		if (midiType != 0 && midiType != 1)
		{
			error = "Midi: Type 2 not supported";
			return;
		}

		tracks.resize(trackCount);

		// SMTP unhandled
		if (division & 0x8000)
		{
			error = "Midi: SMTP parameters not supported";
			return;
		}
		else
			dpb = division;
	}

	void onMidiTrackBegin()
	{
		++curTrack;
	}

	void onMidiEvent(const MidiEvent &e, uint32_t absDelta)
	{
		assert(curTrack >= 0 && curTrack < (int16_t) tracks.size());

		Track &track = tracks[curTrack];

		track.appendEvent(e);
		volReset.handleEvent(e, track);
		expReset.handleEvent(e, track);

		if (e.type == CC && e.e.cc.ctrl == CC_CTRL_LOOP)
			loopDelta = absDelta;
	}

	/* ALDataSource */
	Status fillBuffer(AL::Buffer::ID buf)
	{
		/* In case there is no currently scheduled one */
		for (size_t i = 0; i < tracks.size(); ++i)
			tracks[i].scheduleEvent(looped);

		size_t remTicks = BUF_TICKS;

		/* Iterate until all ticks that fit into the buffer
		 * have been rendered */
		while (remTicks > 0)
		{
			/* Check for events that have to be activated now, activate them,
			 * and schedule new ones if the queue isn't empty */
			for (size_t i = 0; i < tracks.size(); ++i)
			{
				Track &track = tracks[i];

				/* We have to loop and ensure that the final scheduled
				 * event lies in the future, as multiple events might
				 * have to be activated at once */
				while (true)
				{
					if (!track.valid || track.remDeltas > 0)
						break;

					int32_t prevOffset = track.remDeltas;

					activateEvent(track.event);

					track.valid = false;
					track.scheduleEvent(looped);

					/* Negative deltas from the previous event have to
					 * be carried over into the next to stay in sync */
					if (prevOffset < 0)
						track.remDeltas += prevOffset;

					/* Ensure it lies in the future */
					if (track.remDeltas > 0)
						break;
				}
			}

			size_t nextEvent = (size_t) -1;
			bool allInvalid = true;

			/* Search all tracks for the temporally nearest event */
			for (size_t i = 0; i < tracks.size(); ++i)
			{
				Track &track = tracks[i];

				if (!track.valid)
					continue;

				allInvalid = false;

				uint32_t remDelta = track.remDeltas / playbackSpeed;

				/* We need to render at least one tick regardless to
				 * avoid an endless loop of waiting for the next event
				 * to become current */
				if (remDelta < nextEvent)
					nextEvent = std::max<uint32_t>(remDelta, 1);
			}

			/* Calculate amount of ticks we'll render next */
			size_t genTicks = allInvalid ? remTicks : std::min(remTicks, nextEvent);

			if (genTicks == 0)
				continue;

			renderTicks(genTicks, BUF_TICKS - remTicks);
			remTicks -= genTicks;

			float genDeltas = (genTicks * playbackSpeed) + genDeltasCarry;

			float intDeltas;
			genDeltasCarry = modff(genDeltas, &intDeltas);

			/* Substract integer part of consumed deltas while carrying
			 * over the fractional amount into the next iteration */
			for (size_t i = 0; i < tracks.size(); ++i)
				if (tracks[i].valid)
					tracks[i].remDeltas -= intDeltas;
		}

		/* Fill AL buffer */
		AL::Buffer::uploadData(buf, AL_FORMAT_STEREO16, synthBuf, sizeof(synthBuf), freq);

		if (tracks[longestI].atEnd)
			return EndOfStream;

		return NoError;
	}

	int sampleRate()
	{
		return freq;
	}

	/* Midi sources cannot seek, and so always reset to beginning */
	void seekToOffset(double)
	{
		/* Reset synth */
		fluid.synth_system_reset(synth);

		/* Reset runtime variables */
		genDeltasCarry = 0;
		updatePlaybackSpeed(DEFAULT_BPM);

		/* Reset tracks */
		for (size_t i = 0; i < tracks.size(); ++i)
			tracks[i].reset();
	}

	uint64_t loopStartFrames() { return 0; }

	bool setPitch(float value)
	{
		// not completely correct, but close
		pitchShift = round((value > 1.0f ? 14 : 24) * (value - 1.0f));

		return true;
	}
};

ALDataSource *createMidiSource(
			std::string &error,
#ifdef MKXPZ_RETRO
			std::shared_ptr<struct FileSystem::File> ops,
#else
			SDL_RWops &ops,
#endif // MKXPZ_RETRO
                               bool looped)
{
	error.clear();
	MidiSource *source = new MidiSource(error, ops, looped);
	if (error.empty())
		return source;
	delete source;
	return nullptr;
}
