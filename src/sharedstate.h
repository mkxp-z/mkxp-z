/*
** sharedstate.h
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

#ifndef SHAREDSTATE_H
#define SHAREDSTATE_H

#include "sigslot/signal.hpp"
#include "IGraphics.h"
#include "IInput.h"
#include "IAudio.h"
#include "AbstractEventThread.h"
#include "ThreadManager.h"

#define shState ThreadManager::getInstance().getSharedState()
#define glState shState->_glState()
#define rgssVer SharedState::rgssVersion

struct SharedStatePrivate;
struct RGSSThreadData;
struct GlobalIBO;
struct SDL_Window;
struct TEXFBO;
struct Quad;
struct ShaderSet;

class Scene;

class FileSystem;

class EventThread;

class Graphics;

class Input;

class Audio;

class GLState;

class TexPool;

class Font;

class SharedFontState;

struct GlobalIBO;
struct Config;
struct Vec2i;
struct SharedMidiState;

struct SharedState {
    void *bindingData() const;

    void setBindingData(void *data);

    SDL_Window *sdlWindow() const;

    std::shared_ptr<Scene> screen() const;

    void setScreen(std::shared_ptr<Scene> screen);

    std::shared_ptr<FileSystem> filesystem() const;

    AbstractEventThread &eThread() const;

    std::shared_ptr<RGSSThreadData> rtData() const;

    std::shared_ptr<Config> config() const;

    IGraphics &graphics() const;

    IInput &input() const;

    IAudio &audio() const;

    GLState &_glState() const;

    ShaderSet &shaders() const;

    TexPool &texPool() const;

    SharedFontState &fontState() const;

    Font &defaultFont() const;

    SharedMidiState &midiState() const;

	sigslot::signal<> prepareDraw;

	unsigned int genTimeStamp();
    
    // Returns time since SharedState was constructed in microseconds
    double runTime();

	/* Returns global quad IBO, and ensures it has indices
	 * for at least minSize quads */
	void ensureQuadIBO(size_t minSize);
	GlobalIBO &globalIBO();

	/* Global general purpose texture */
	void bindTex();
	void ensureTexSize(int minW, int minH, Vec2i &currentSizeOut);

	TEXFBO &gpTexFBO(int minW, int minH);

	Quad &gpQuad() const;

	/* Basically just a simple "TexPool"
	 * replacement for Tilemap atlas use */
	void requestAtlasTex(int w, int h, TEXFBO &out);
	void releaseAtlasTex(TEXFBO &tex);

	/* Checks EventThread's shutdown request flag and if set,
	 * requests the binding to terminate. In this case, this
	 * function will most likely not return */
	void checkShutdown();

	void checkReset();

    static int rgssVersion;

    /* This function will throw an Exception instance
     * on initialization error */
    static std::shared_ptr<SharedState> initInstance(std::shared_ptr<RGSSThreadData> threadData);

private:
    SharedState(std::shared_ptr<RGSSThreadData> threadData);

public:
    ~SharedState();

private:
    friend std::unique_ptr<SharedState>::deleter_type;
    friend std::default_delete<SharedState>;

    std::unique_ptr<SharedStatePrivate> p;
};

#endif // SHAREDSTATE_H
