/*
** graphics.h
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

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <climits>
#include "util.h"

#ifdef MKXPZ_RETRO
#  include "wasm-types.h"
#endif // MKXPZ_RETRO

class Scene;
class Bitmap;
class Disposable;
struct RGSSThreadData;
struct GraphicsPrivate;
struct AtomicFlag;
struct THEORAPLAY_VideoFrame;
struct Movie;

class Graphics
{
public:
    double getDelta();
    double lastUpdate();
    
	bool update(Exception &exception, bool checkForShutdown = true);
	void freeze(Exception &exception);
	bool &frozen();
	void transition(Exception &exception,
	                int duration = 8,
	                Bitmap *transMap = 0,
	                int vague = 40,
			int start = 0,
			int stop = INT_MAX);
	void frameReset();

	DECL_ATTR_NOEXCEPT( FrameRate,  int )
	DECL_ATTR_NOEXCEPT( FrameCount, int )
	DECL_ATTR_NOEXCEPT( Brightness, int )

	void wait(Exception &exception, int duration, int start = 0, int stop = INT_MAX);
	void fadeout(Exception &exception, int duration, int start = 0, int stop = INT_MAX, int brightness = -1);
	void fadein(Exception &exception, int duration, int start = 0, int stop = INT_MAX, int brightness = -1);

	Bitmap *snapToBitmap(Exception &exception);

	int width() const;
	int height() const;
	int widthHires() const;
	int heightHires() const;
	bool isPingPongFramebufferActive() const;
    int displayContentWidth() const;
    int displayContentHeight() const;
    int displayWidth() const;
    int displayHeight() const;
	void resizeScreen(int width, int height, bool resizeWindow = true);
    void resizeWindow(int width, int height, bool center=false);
	void drawMovieFrame(const THEORAPLAY_VideoFrame* video, Bitmap *videoBitmap);
	bool updateMovieInput(Movie *movie);
	Movie *playMovie(Exception &exception, const char *filename, int volume, bool skippable);
	Movie *playMovie(Movie *movie);
	static void stopMovie(Movie *movie);
	static bool streamMovieAudioProc(Movie *movie);
#ifdef MKXPZ_RETRO
	static bool getMovieDupeFrame(Movie *movie);
	static bool sandbox_serialize_movie(const Movie *movie, void *&data, mkxp_sandbox::wasm_size_t &max_size);
	void sandbox_reinit();
#endif // MKXPZ_RETRO
	void screenshot(Exception &exception, const char *filename);

	void reset(Exception &exception);
    void center();

    /* Non-standard extension */
    DECL_ATTR_NOEXCEPT( Fullscreen, bool )
    DECL_ATTR_NOEXCEPT( ShowCursor, bool )
    DECL_ATTR_NOEXCEPT( Scale,    double )
    DECL_ATTR_NOEXCEPT( Frameskip, bool )
    DECL_ATTR_NOEXCEPT( FixedAspectRatio, bool )
    DECL_ATTR_NOEXCEPT( SmoothScaling, int )
    DECL_ATTR_NOEXCEPT( IntegerScaling, bool )
    DECL_ATTR_NOEXCEPT( LastMileScaling, bool )
    DECL_ATTR_NOEXCEPT( Threadsafe, bool )
    double averageFrameRate();

	/* <internal> */
	Scene *getScreen() const;
	void repaint(bool useBackBuffer = false);
	/* Repaint screen with static image until exitCond
	 * is set. Observes reset flag on top of shutdown
	 * if "checkReset" */
	void repaintWait(const AtomicFlag &exitCond,
	                 bool checkReset = true);
    
    void lock(bool force = false);
    void unlock(bool force = false);

#ifdef MKXPZ_RETRO
	std::vector<uint32_t> frozenPixels;
	void uploadFrozenPixels();
#endif // MKXPZ_RETRO

private:
	Graphics(RGSSThreadData *data);
	~Graphics();

	void addDisposable(Disposable *);
	void remDisposable(Disposable *);

	friend struct SharedStatePrivate;
	friend class Disposable;

	GraphicsPrivate *p;
};

#ifdef MKXPZ_RETRO
#  define GFX_LOCK
#  define GFX_UNLOCK
#else
#  define GFX_LOCK shState->graphics().lock()
#  define GFX_UNLOCK shState->graphics().unlock()
#endif // MKXPZ_RETRO

#endif // GRAPHICS_H
