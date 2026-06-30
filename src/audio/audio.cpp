/*
** audio.cpp
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

#include "audio.h"

#include "audiostream.h"
#include "soundemitter.h"
#include "sharedstate.h"
#include "sharedmidistate.h"
#include "eventthread.h"
#include "forced-assert.h"

#include "mkxp-polyfill.h" // std::to_string

#include <string>
#include <utility>
#include <vector>

#ifdef MKXPZ_RETRO
#  include "graphics.h"
#  include "sandbox-serial-util.h"
#else
#  include "sdl-util.h"
#  include <SDL_mutex.h>
#  include <SDL_thread.h>
#  include <SDL_timer.h>
#endif // MKXPZ_RETRO

AudioMutex::AudioMutex()
{
#ifdef MKXPZ_RETRO
	MKXPZ_FORCED_ASSERT(!mkxp_mutex_init(&mutex, false));
#else
	MKXPZ_FORCED_ASSERT((mutex = SDL_CreateMutex()) != nullptr);
#endif // MKXPZ_RETRO
}

AudioMutex::~AudioMutex()
{
#ifdef MKXPZ_RETRO
	MKXPZ_FORCED_ASSERT(!mkxp_mutex_destroy(&mutex));
#else
	SDL_DestroyMutex(mutex);
#endif // MKXPZ_RETRO
}

void AudioMutex::lock()
{
#ifdef MKXPZ_RETRO
	MKXPZ_FORCED_ASSERT(!mkxp_mutex_lock(&mutex));
#else
	MKXPZ_FORCED_ASSERT(!SDL_LockMutex(mutex));
#endif // MKXPZ_RETRO
}

void AudioMutex::unlock()
{
#ifdef MKXPZ_RETRO
	MKXPZ_FORCED_ASSERT(!mkxp_mutex_unlock(&mutex));
#else
	MKXPZ_FORCED_ASSERT(!SDL_UnlockMutex(mutex));
#endif // MKXPZ_RETRO
}

AudioMutexGuard::AudioMutexGuard(AudioMutex &mutex) : mutex(&mutex)
{
	mutex.lock();
}

AudioMutexGuard::AudioMutexGuard(AudioMutexGuard &&guard) noexcept : mutex(std::exchange(guard.mutex, nullptr)) {}

AudioMutexGuard &AudioMutexGuard::operator=(AudioMutexGuard &&guard) noexcept
{
	mutex = std::exchange(guard.mutex, nullptr);
	return *this;
}

AudioMutexGuard::~AudioMutexGuard()
{
	if (mutex != nullptr) mutex->unlock();
}

struct BgmTracksGuard
{
	BgmTracksGuard(std::vector<AudioStream*> &bgmTracks) : bgmTracks(&bgmTracks)
	{
		for (auto track : bgmTracks)
			track->mutex.lock();
	}

	BgmTracksGuard(const BgmTracksGuard &guard) = delete;

	BgmTracksGuard(BgmTracksGuard &&guard) noexcept : bgmTracks(std::exchange(guard.bgmTracks, nullptr)) {}

	BgmTracksGuard &operator=(const BgmTracksGuard &guard) = delete;

	BgmTracksGuard &operator=(BgmTracksGuard &&guard) noexcept
	{
		bgmTracks = std::exchange(guard.bgmTracks, nullptr);
		return *this;
	}

	~BgmTracksGuard()
	{
		if (bgmTracks != nullptr)
			for (auto track : *bgmTracks)
				track->mutex.unlock();
	}

private:
	std::vector<AudioStream*> *bgmTracks;
};

struct AudioPrivate
{
    
    std::vector<AudioStream*> bgmTracks;
	AudioStream bgs;
	AudioStream me;

	SoundEmitter se;

#ifndef MKXPZ_RETRO
	SyncPoint &syncPoint;
#endif // MKXPZ_RETRO
    
    float volumeRatio;

	/* The 'MeWatch' is responsible for detecting
	 * a playing ME, quickly fading out the BGM and
	 * keeping it paused/stopped while the ME plays,
	 * and unpausing/fading the BGM back in again
	 * afterwards */
	enum MeWatchState
	{
		MeNotPlaying,
		BgmFadingOut,
		MePlaying,
		BgmFadingIn
	};

	struct
	{
#ifdef MKXPZ_RETRO
		AudioMutex mutex;
#else
		SDL_Thread *thread;
#endif // MKXPZ_RETRO
		AtomicFlag termReq;
		MeWatchState state;
	} meWatch;

	AudioPrivate(RGSSThreadData &rtData)
	    : bgs(ALStream::Looped, "bgs"),
	      me(ALStream::NotLooped, "me"),
	      se(rtData.config),
#ifndef MKXPZ_RETRO
	      syncPoint(rtData.syncPoint),
#endif // MKXPZ_RETRO
          volumeRatio(1)
	{
        for (int i = 0; i < rtData.config.BGM.trackCount; i++) {
            std::string id = std::string("bgm" + std::to_string(i));
            bgmTracks.push_back(new AudioStream(ALStream::Looped, id.c_str()));
        }
        
		meWatch.state = MeNotPlaying;
#ifndef MKXPZ_RETRO
		meWatch.thread = createSDLThread
			<AudioPrivate, &AudioPrivate::meWatchThread>(this, "audio_mewatch");
#endif // MKXPZ_RETRO
	}

	~AudioPrivate()
	{
		meWatch.termReq.set();
#ifdef MKXPZ_RETRO
		{
			AudioMutexGuard guard(meWatch.mutex);
		}
#else
		SDL_WaitThread(meWatch.thread, 0);
#endif // MKXPZ_RETRO
        for (auto track : bgmTracks)
            delete track;
	}
    
    AudioStream *getTrackByIndex(Exception &exception, int index) {
        if (index < 0) index = 0;
        if (index > (int)(bgmTracks.size()) - 1) {
	    exception = Exception(Exception::MKXPError, "requested BGM track %d out of range (max: %d)", index, bgmTracks.size() - 1);
	    return nullptr;
        }
        return bgmTracks[index];
    }

	bool meWatchProc()
	{
#ifndef MKXPZ_RETRO
		syncPoint.passSecondarySync();
#endif // MKXPZ_RETRO

		if (meWatch.termReq)
			return false;

#ifdef MKXPZ_RETRO
		AudioMutexGuard guard(meWatch.mutex);
		if (meWatch.termReq)
			return false;
#endif // MKXPZ_RETRO

		float fadeOutStep;
		float fadeInStep;

#ifdef MKXPZ_RETRO
		if (mkxp_retro::using_threaded_audio())
#endif // MKXPZ_RETRO
		{
			fadeOutStep = .5f / AUDIO_SLEEP;
			fadeInStep  = .1f / AUDIO_SLEEP;
		}
#ifdef MKXPZ_RETRO
		else
		{
			double rate = mkxp_retro::get_refresh_rate();
			fadeOutStep = 5.f / rate;
			fadeInStep  = 1.f / rate;
		}
#endif // MKXPZ_RETRO

		switch (meWatch.state)
		{
			case MeNotPlaying:
			{
				AudioMutexGuard guard(me.mutex);

				if (me.stream.queryState() == ALStream::Playing)
				{
					/* ME playing detected. -> FadeOutBGM */
					for (auto track : bgmTracks)
					{
						AudioMutexGuard trackGuard(track->mutex);
						track->extPaused = true;
					}

					meWatch.state = BgmFadingOut;
				}

				break;
			}

			case BgmFadingOut :
			{
				AudioMutexGuard guard(me.mutex);

				if (me.stream.queryState() != ALStream::Playing)
				{
					/* ME has ended while fading OUT BGM. -> FadeInBGM */
					for (auto track : bgmTracks)
					{
						AudioMutexGuard trackGuard(track->mutex);
						track->extPaused = false;
					}
					meWatch.state = BgmFadingIn;

					break;
				}

				bool shouldBreak = false;

				{
					BgmTracksGuard tracksGuard(bgmTracks);

					for (auto track : bgmTracks) {
						float vol = track->getVolume(AudioStream::External);
						vol -= fadeOutStep;

						if (vol < 0 || track->stream.queryState() != ALStream::Playing) {
							/* Either BGM has fully faded out, or stopped midway. -> MePlaying */
							track->setVolume(AudioStream::External, 0);
							track->stream.pause();

							// check to see if there are any tracks still playing,
							// and if the last one was ended this round, this branch should exit
							std::vector<AudioStream*> playingTracks;
							for (auto t : bgmTracks)
								if (t->stream.queryState() == ALStream::Playing)
									playingTracks.push_back(t);


							if (playingTracks.size() <= 0 && !shouldBreak) shouldBreak = true;
							continue;
						}

						track->setVolume(AudioStream::External, vol);
					}
				}

				if (shouldBreak) {
					meWatch.state = MePlaying;
					break;
				}

				break;
			}

			case MePlaying :
			{
				AudioMutexGuard guard(me.mutex);

				if (me.stream.queryState() != ALStream::Playing)
				{
					/* ME has ended */
					for (auto track : bgmTracks) {
						AudioMutexGuard trackGuard(track->mutex);
						track->extPaused = false;

						ALStream::State sState = track->stream.queryState();

						if (sState == ALStream::Paused) {
							/* BGM is paused. -> FadeInBGM */
							track->stream.play();
							meWatch.state = BgmFadingIn;
						}
						else {
							/* BGM is stopped. -> MeNotPlaying */
							track->setVolume(AudioStream::External, 1.0f);

							if (!track->noResumeStop)
								track->stream.play();

							meWatch.state = MeNotPlaying;
						}
					}
				}

				break;
			}

			case BgmFadingIn :
			{
				BgmTracksGuard tracksGuard(bgmTracks);

				if (bgmTracks[0]->stream.queryState() == ALStream::Stopped)
				{
					/* BGM stopped midway fade in. -> MeNotPlaying */
					for (auto track : bgmTracks)
						track->setVolume(AudioStream::External, 1.0f);
					meWatch.state = MeNotPlaying;

					break;
				}

				AudioMutexGuard guard(me.mutex);

				if (me.stream.queryState() == ALStream::Playing)
				{
					/* ME started playing midway BGM fade in. -> FadeOutBGM */
					for (auto track : bgmTracks)
						track->extPaused = true;
					meWatch.state = BgmFadingOut;

					break;
				}

				float vol = bgmTracks[0]->getVolume(AudioStream::External);
				vol += fadeInStep;

				if (vol >= 1)
				{
					/* BGM fully faded in. -> MeNotPlaying */
					vol = 1.0f;
					meWatch.state = MeNotPlaying;
				}

				for (auto track : bgmTracks)
					track->setVolume(AudioStream::External, vol);

				break;
			}
		}

		return true;
	}

#ifndef MKXPZ_RETRO
	void meWatchThread()
	{
		while (meWatchProc())
			SDL_Delay(AUDIO_SLEEP);
	}
#endif // MKXPZ_RETRO
};

Audio::Audio(RGSSThreadData &rtData)
	: p(new AudioPrivate(rtData))
{}

#ifdef MKXPZ_RETRO
void Audio::render() {
	if (mkxp_retro::sandbox->get_movie_from_audio_thread() != nullptr) {
		AudioMutexGuard guard(mkxp_retro::sandbox->movie_mutex);
		/* We need to call `get_movie_from_audio_thread()` a second time to avoid race
		 * conditions where the movie gets destroyed after the first time we checked
		 * that the movie isn't null but before we lock the mutex */
		Graphics::streamMovieAudioProc(mkxp_retro::sandbox->get_movie_from_audio_thread());
	}
	p->meWatchProc();
	for (int i = 0; i < (int)p->bgmTracks.size(); i++) {
		p->bgmTracks[i]->render();
	}
	p->bgs.render();
	p->me.render();
}
#endif // MKXPZ_RETRO

void Audio::bgmPlay(Exception &exception,
	            const char *filename,
                    int volume,
                    int pitch,
                    double pos,
                    int track)
{
    if (track == -127) {
        for (int i = 0; i < (int)p->bgmTracks.size(); i++) {
            if (i == 0) {
                continue;
            }
            p->bgmTracks[i]->stop();
        }
        
        track = 0;
    }

	AudioStream *stream = p->getTrackByIndex(exception, track);
	if (stream != nullptr) {
		stream->play(exception, filename, volume, pitch, pos);
	}
}

void Audio::bgmStop(Exception &exception, int track)
{
    if (track == -127) {
        for (auto track : p->bgmTracks)
            track->stop();
        
        return;
    }
    
    AudioStream *stream = p->getTrackByIndex(exception, track);
    if (stream != nullptr) {
        stream->stop();
    }
}

void Audio::bgmFade(Exception &exception, int time, int track)
{
    if (track == -127) {
        for (auto track : p->bgmTracks)
            track->fadeOut(time);
        
        return;
    }
    
    AudioStream *stream = p->getTrackByIndex(exception, track);
    if (stream != nullptr) {
        stream->fadeOut(time);
    }
}

int Audio::bgmGetVolume(Exception &exception, int track)
{
    if (track == -127)
        return p->bgmTracks[0]->getVolume(AudioStream::BaseRatio) * 100;
    
    AudioStream *stream = p->getTrackByIndex(exception, track);
    if (stream != nullptr) {
	return stream->getVolume(AudioStream::Base) * 100;
    } else {
        return 0;
    }
}

void Audio::bgmSetVolume(Exception &exception, int volume, int track)
{
    float vol = volume / 100.0;
    if (track == -127) {
        for (auto track : p->bgmTracks)
            track->setVolume(AudioStream::BaseRatio, vol);
        
        return;
    }

    AudioStream *stream = p->getTrackByIndex(exception, track);
    if (stream != nullptr) {
        stream->setVolume(AudioStream::Base, vol);
    }
}


void Audio::bgsPlay(Exception &exception,
	            const char *filename,
                    int volume,
                    int pitch,
                    double pos)
{
	p->bgs.play(exception, filename, volume, pitch, pos);
}

void Audio::bgsStop()
{
	p->bgs.stop();
}

void Audio::bgsFade(int time)
{
	p->bgs.fadeOut(time);
}


void Audio::mePlay(Exception &exception,
	           const char *filename,
                   int volume,
                   int pitch)
{
	p->me.play(exception, filename, volume, pitch);
}

void Audio::meStop()
{
	p->me.stop();
}

void Audio::meFade(int time)
{
	p->me.fadeOut(time);
}


void Audio::sePlay(const char *filename,
                   int volume,
                   int pitch)
{
	p->se.play(filename, volume, pitch);
}

void Audio::seStop()
{
	p->se.stop();
}

void Audio::setupMidi()
{
	shState->midiState().initIfNeeded(shState->config());
}

double Audio::bgmPos(Exception &exception, int track)
{
	AudioStream *stream = p->getTrackByIndex(exception, track);
	if (stream != nullptr) {
		return stream->playingOffset();
	} else {
		return 0.0;
	}
}

double Audio::bgsPos()
{
	return p->bgs.playingOffset();
}

void Audio::reset()
{
    for (auto track : p->bgmTracks) {
    	track->stop();
    }

	p->bgs.stop();
	p->me.stop();
	p->se.stop();
}

Audio::~Audio() { delete p; }

#ifdef MKXPZ_RETRO
#ifndef MKXPZ_SANDBOX_SERIAL_AUDIO_H
#define MKXPZ_SANDBOX_SERIAL_AUDIO_H
#include "sandbox-serial-audio.h"
#endif // MKXPZ_SANDBOX_SERIAL_AUDIO_H
#endif // MKXPZ_RETRO
