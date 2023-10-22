/*
** eventthread.h
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

#ifndef EVENTTHREAD_H
#define EVENTTHREAD_H

#include <string>

#include <stdint.h>

#include "config.h"
#include "etc-internal.h"
#include "sdl-util.h"
#include "keybindings.h"
#include "ISyncPoint.h"
#include "AbstractEventThread.h"
#include "SDL_Instance.h"
#include "filesystem.h"

struct RGSSThreadData;
typedef struct MKXPZ_ALCDEVICE ALCdevice;
struct SDL_Window;
union SDL_Event;

class EventThread : public AbstractEventThread {
public:
	void lockText(bool lock) override;


    static bool allocUserEvents();

    EventThread();

    ~EventThread() override;

    void process(RGSSThreadData &rtData) override;

    void cleanup() override;

    /* Called from RGSS thread */
    void requestFullscreenMode(bool mode) override;

    void requestWindowResize(int width, int height) override;

    void requestWindowReposition(int x, int y) override;

    void requestWindowCenter() override;

    void requestWindowRename(const char *title) override;

    void requestShowCursor(bool mode) override;

    void requestTextInputMode(bool mode) override;

    void requestSettingsMenu() override;

    void requestTerminate() override;

    bool getFullscreen() const override;

    bool getShowCursor() const override;

    bool getControllerConnected() const override;

    SDL_GameController *controller() const override;

    void showMessageBox(const char *body, int flags) override;

    /* RGSS thread calls this once per frame */
    void notifyFrame() override;

    /* Called on game screen (size / offset) changes */
    void notifyGameScreenChange(const SDL_Rect &screen) override;

private:
	static int eventFilter(void *, SDL_Event*);

	void resetInputStates();
	void setFullscreen(SDL_Window *, bool mode);
	void updateCursorState(bool inWindow,
	                       const SDL_Rect &screen);

	bool fullscreen;
	bool showCursor;
    
    SDL_GameController *ctrl;
    
	AtomicFlag msgBoxDone;
    
    SDL_mutex *textInputLock;

	struct
	{
		AtomicFlag sendUpdates;
	} fps;
};

/* Used to asynchronously inform the RGSS thread
 * about certain value changes */
template<typename T>
struct UnidirMessage
{
	UnidirMessage()
	    : mutex(SDL_CreateMutex()),
	      current(T())
	{}

	~UnidirMessage()
	{
		SDL_DestroyMutex(mutex);
	}

	/* Done from the sending side */
	void post(const T &value)
	{
		SDL_LockMutex(mutex);

		changed.set();
		current = value;

		SDL_UnlockMutex(mutex);
	}

	/* Done from the receiving side */
	bool poll(T &out) const
	{
		if (!changed)
			return false;

		SDL_LockMutex(mutex);

		out = current;
		changed.clear();

		SDL_UnlockMutex(mutex);

		return true;
	}

	/* Done from either */
	void get(T &out) const
	{
		SDL_LockMutex(mutex);
		out = current;
		SDL_UnlockMutex(mutex);
	}

private:
	SDL_mutex *mutex;
	mutable AtomicFlag changed;
	T current;
};

struct SyncPoint : public ISyncPoint {
    /* Used by eventFilter to control sleep/wakeup */
    void haltThreads() override;

    void resumeThreads() override;

    /* Used by RGSS thread */
    bool mainSyncLocked() override;

    void waitMainSync() override;

    /* Used by secondary (audio) threads */
    void passSecondarySync() override;

private:
    struct Util {
        Util();

        ~Util();

		void lock();
		void unlock(bool multi);
		void waitForUnlock();

		AtomicFlag locked;
		SDL_mutex *mut;
		SDL_cond *cond;
	};

	Util mainSync;
	Util reply;
	Util secondSync;
};

struct RGSSThreadData
{
	/* Main thread sets this to request RGSS thread to terminate */
	AtomicFlag rqTerm;
	/* In response, RGSS thread sets this to confirm
	 * that it received the request and isn't stuck */
	AtomicFlag rqTermAck;

	/* Set when F12 is pressed */
	AtomicFlag rqReset;

	/* Set when F12 is released */
	AtomicFlag rqResetFinish;

	// Set when window is being adjusted (resize, reposition)
	AtomicFlag rqWindowAdjust;

	std::shared_ptr<AbstractEventThread> ethread;
	UnidirMessage<Vec2i> windowSizeMsg;
	UnidirMessage<Vec2i> drawableSizeMsg;
	UnidirMessage<BDescVec> bindingUpdateMsg;
	std::shared_ptr<ISyncPoint> syncPoint;
	std::shared_ptr<SDL_Instance> sdlInstance;

	const char *argv0;

	std::shared_ptr<SDL_Window> window;
	std::shared_ptr<ALCdevice> alcDev;

	SDL_GLContext glContext;

	Vec2 sizeResoRatio;
	Vec2i screenOffset;
	int scale;
	const int refreshRate;

	std::shared_ptr<Config> config;
	std::shared_ptr<FileSystem> filesystem;

	std::string rgssErrorMsg;

	RGSSThreadData(std::shared_ptr<AbstractEventThread> ethread,
				   const char *argv0,
				   std::shared_ptr<SDL_Window> window,
				   std::shared_ptr<ALCdevice> alcDev,
				   int refreshRate,
				   int scalingFactor,
				   std::shared_ptr<Config> newconf,
				   std::shared_ptr<FileSystem> fs,
				   std::shared_ptr<SDL_Instance> sdl,
				   SDL_GLContext ctx)
			: ethread(ethread),
			  argv0(argv0),
			  window(window),
			  alcDev(alcDev),
			  syncPoint(std::make_shared<SyncPoint>()),
			  sdlInstance(sdl),
			  sizeResoRatio(1, 1),
			  refreshRate(refreshRate),
			  scale(scalingFactor),
			  config(newconf),
			  filesystem(fs),
			  glContext(ctx)
	{}
};

#endif // EVENTTHREAD_H
