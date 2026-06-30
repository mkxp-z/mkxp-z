/*
** sharedstate.cpp
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

#include "sharedstate.h"

#include "scene.h"
#include "util.h"
#ifndef MKXPZ_RETRO
#include "filesystem.h"
#endif // MKXPZ_RETRO
#include "graphics.h"
#ifndef MKXPZ_RETRO
#include "input.h"
#include "audio.h"
#endif // MKXPZ_RETRO
#include "glstate.h"
#include "shader.h"
#include "texpool.h"
#include "font.h"
#include "eventthread.h"
#include "gl-util.h"
#include "global-ibo.h"
#include "quad.h"
#include "binding.h"
#include "exception.h"
#include "sharedmidistate.h"

#include <unistd.h>
#include <stdio.h>
#include <string>
#include <chrono>

SharedState *SharedState::instance = 0;
int SharedState::rgssVersion = 0;
static GlobalIBO *_globalIBO = 0;

static const char *gameArchExt()
{
	if (rgssVer == 1)
		return ".rgssad";
	else if (rgssVer == 2)
		return ".rgss2a";
	else if (rgssVer == 3)
		return ".rgss3a";

	assert(!"unreachable");
	return 0;
}

struct SharedStatePrivate
{
	void *bindingData;
#ifndef MKXPZ_RETRO
	SDL_Window *sdlWindow;
#endif // MKXPZ_RETRO
	Scene *screen;

#ifndef MKXPZ_RETRO
	FileSystem fileSystem;

	EventThread &eThread;
#endif // MKXPZ_RETRO
	RGSSThreadData &rtData;
	Config &config;
	SharedMidiState midiState;

	Graphics graphics;
#ifndef MKXPZ_RETRO
	Input input;
	Audio audio;
#endif // MKXPZ_RETRO

	GLState _glState;

	ShaderSet shaders;

	TexPool texPool;

	SharedFontState fontState;
	Font *defaultFont;

	TEX::ID globalTex;
	int globalTexW, globalTexH;
	bool globalTexDirty;

	TEXFBO gpTexFBO;

	TEXFBO atlasTex;

	Quad gpQuad;

	uint64_t stampCounter;
    
    std::chrono::time_point<std::chrono::steady_clock> startupTime;

	SharedStatePrivate(Exception &exception, RGSSThreadData *threadData)
	    : bindingData(0),
#ifndef MKXPZ_RETRO
	      sdlWindow(threadData->window),
	      fileSystem(threadData->argv0, threadData->config.allowSymlinks),
	      eThread(*threadData->ethread),
#endif // MKXPZ_RETRO
	      rtData(*threadData),
	      config(threadData->config),
	      midiState(threadData->config),
	      graphics(threadData),
#ifndef MKXPZ_RETRO
	      input(*threadData),
	      audio(*threadData),
#endif // MKXPZ_RETRO
	      _glState(threadData->config),
	      shaders(exception),
	      fontState(threadData->config),
	      stampCounter(0)
	{}
	
	void init(RGSSThreadData *threadData)
	{
        
        startupTime = std::chrono::steady_clock::now();
        
#ifndef MKXPZ_RETRO
		/* Shaders have been compiled in ShaderSet's constructor */
		if (gl.ReleaseShaderCompiler)
			gl.ReleaseShaderCompiler();
#endif // MKXPZ_RETRO

#ifdef MKXPZ_RETRO
		mkxp_retro::fs->initFontSets(fontState);
#else
		std::string archPath = config.execName + gameArchExt();

		for (size_t i = 0; i < config.patches.size(); ++i)
		{
			Exception e;
			fileSystem.addPath(e, config.patches[i].c_str());
			if (e.is_error())
				throw e;
		}

		/* Check if a game archive exists */
		FILE *tmp = fopen(archPath.c_str(), "rb");
		if (tmp)
		{
			Exception e;
			fileSystem.addPath(e, archPath.c_str());
			fclose(tmp);
			if (e.is_error())
				throw e;
		}

		{
			Exception e;
			fileSystem.addPath(e, ".");
			if (e.is_error())
				throw e;
		}

		for (size_t i = 0; i < config.rtps.size(); ++i)
		{
			Exception e;
			fileSystem.addPath(e, config.rtps[i].c_str());
			if (e.is_error())
				throw e;
		}

		if (config.pathCache)
			fileSystem.createPathCache();

		fileSystem.initFontSets(fontState);
#endif // MKXPZ_RETRO

		globalTexW = 128;
		globalTexH = 64;

		reinit();

		/* RGSS3 games will call setup_midi, so there's
		 * no need to do it on startup */
		if (rgssVer <= 2)
			midiState.initIfNeeded(threadData->config);
	}

	~SharedStatePrivate()
	{
		TEX::del(globalTex);
		TEXFBO::fini(gpTexFBO);
		TEXFBO::fini(atlasTex);
	}

	void reinit()
	{
		globalTex = TEX::gen();
		TEX::bind(globalTex);
		TEX::setRepeat(false);
		TEX::setSmooth(false);
		TEX::allocEmpty(globalTexW, globalTexH);
		globalTexDirty = false;

		TEXFBO::init(gpTexFBO);
		/* Reuse starting values */
		TEXFBO::allocEmpty(gpTexFBO, globalTexW, globalTexH);
		TEXFBO::linkFBO(gpTexFBO);
	}
};

void SharedState::initInstance(Exception &exception, RGSSThreadData *threadData)
{
	/* This section is tricky because of dependencies:
	 * SharedState depends on GlobalIBO existing,
	 * Font depends on SharedState existing */

	rgssVersion = threadData->config.rgssVersion;
    
	_globalIBO = new GlobalIBO();
	_globalIBO->ensureSize(1);

	Font *defaultFont = 0;

	SharedState::instance = new SharedState(exception, threadData);
	if (exception.is_error())
	{
		delete SharedState::instance;
		SharedState::instance = 0;
		delete _globalIBO;
		_globalIBO = 0;
		return;
	}

	MKXPZ_TRY
	{
		Font::initDefaults(instance->p->fontState);
		defaultFont = new Font();
	}
	MKXPZ_CATCH (const Exception &)
	{
		delete _globalIBO;
		_globalIBO = 0;
		delete SharedState::instance;
		SharedState::instance = 0;
		delete defaultFont;

		MKXPZ_RETHROW;
	}

	SharedState::instance->p->defaultFont = defaultFont;
}

void SharedState::finiInstance()
{
	delete SharedState::instance->p->defaultFont;
	SharedState::instance->p->defaultFont = 0;

	delete SharedState::instance;
	SharedState::instance = 0;

	delete _globalIBO;
	_globalIBO = 0;
}

void SharedState::setScreen(Scene &screen)
{
	p->screen = &screen;
}

#define GSATT(type, lower) \
	type SharedState :: lower() const \
	{ \
		return p->lower; \
	}

GSATT(void*, bindingData)
#ifndef MKXPZ_RETRO
GSATT(SDL_Window*, sdlWindow)
#endif // MKXPZ_RETRO
GSATT(Scene*, screen)
#ifndef MKXPZ_RETRO
GSATT(FileSystem&, fileSystem)
GSATT(EventThread&, eThread)
#endif // MKXPZ_RETRO
GSATT(RGSSThreadData&, rtData)
GSATT(Config&, config)
GSATT(Graphics&, graphics)
#ifndef MKXPZ_RETRO
GSATT(Input&, input)
GSATT(Audio&, audio)
#endif // MKXPZ_RETRO
GSATT(GLState&, _glState)
GSATT(ShaderSet&, shaders)
GSATT(TexPool&, texPool)
GSATT(Quad&, gpQuad)
GSATT(SharedFontState&, fontState)
GSATT(SharedMidiState&, midiState)

void SharedState::setBindingData(void *data)
{
	p->bindingData = data;
}

void SharedState::ensureQuadIBO(size_t minSize)
{
	_globalIBO->ensureSize(minSize);
}

GlobalIBO &SharedState::globalIBO()
{
	return *_globalIBO;
}

void SharedState::bindTex()
{
	TEX::bind(p->globalTex);

	if (p->globalTexDirty)
	{
		TEX::allocEmpty(p->globalTexW, p->globalTexH);
		p->globalTexDirty = false;
	}
}

void SharedState::ensureTexSize(int minW, int minH, Vec2i &currentSizeOut)
{
	if (minW > p->globalTexW)
	{
		p->globalTexDirty = true;
		p->globalTexW = findNextPow2(minW);
	}

	if (minH > p->globalTexH)
	{
		p->globalTexDirty = true;
		p->globalTexH = findNextPow2(minH);
	}

	currentSizeOut = Vec2i(p->globalTexW, p->globalTexH);
}

TEXFBO &SharedState::gpTexFBO(int minW, int minH)
{
	bool needResize = false;

	if (minW > p->gpTexFBO.width)
	{
		p->gpTexFBO.width = findNextPow2(minW);
		needResize = true;
	}

	if (minH > p->gpTexFBO.height)
	{
		p->gpTexFBO.height = findNextPow2(minH);
		needResize = true;
	}

	if (needResize)
	{
		TEX::bind(p->gpTexFBO.tex);
		TEX::allocEmpty(p->gpTexFBO.width, p->gpTexFBO.height);
	}

	return p->gpTexFBO;
}

void SharedState::requestAtlasTex(int w, int h, TEXFBO &out)
{
	TEXFBO tex;

	if (w == p->atlasTex.width && h == p->atlasTex.height)
	{
		tex = p->atlasTex;
		p->atlasTex = TEXFBO();
	}
	else
	{
		TEXFBO::init(tex);
		TEXFBO::allocEmpty(tex, w, h);
		TEXFBO::linkFBO(tex);
	}

	out = tex;
}

void SharedState::releaseAtlasTex(TEXFBO &tex)
{
	/* No point in caching an invalid object */
	if (tex.tex == TEX::ID(0))
		return;

	TEXFBO::fini(p->atlasTex);

	p->atlasTex = tex;
}

void SharedState::checkShutdown()
{
#ifndef MKXPZ_RETRO
	if (!p->rtData.rqTerm)
		return;

	p->rtData.rqTermAck.set();
	p->texPool.disable();
	scriptBinding->terminate();
#endif // MKXPZ_RETRO
}

void SharedState::checkReset()
{
#ifndef MKXPZ_RETRO
	if (!p->rtData.rqReset)
		return;

	p->rtData.rqReset.clear();
	scriptBinding->reset();
#endif // MKXPZ_RETRO
}

Font &SharedState::defaultFont() const
{
	return *p->defaultFont;
}

double SharedState::runTime() {
	if (!p) return 0;
#ifdef MKXPZ_RETRO
	return mkxp_retro::get_ticks_us() / 1000000.0;
#else
	const auto now = std::chrono::steady_clock::now();
	return std::chrono::duration_cast<std::chrono::microseconds>(now - p->startupTime).count() / 1000.0 / 1000.0;
#endif // MKXPZ_RETRO
}

uint64_t SharedState::genTimeStamp()
{
	return p->stampCounter++;
}

SharedState::SharedState(Exception &exception, RGSSThreadData *threadData)
{
	p = new SharedStatePrivate(exception, threadData);
	if (exception.is_error())
	{
		delete p;
		p = nullptr;
		return;
	}
	SharedState::instance = this;
	MKXPZ_TRY
	{
		p->init(threadData);
		p->screen = p->graphics.getScreen();
	}
	MKXPZ_CATCH (const Exception &)
	{
#ifndef MKXPZ_RETRO
		// If the "error" was the user quitting the game before the path cache finished building,
		// then just return
		if (rtData().rqTerm)
			return;
#endif // MKXPZ_RETRO
		
		delete p;
		p = nullptr;
		SharedState::instance = 0;
		
		MKXPZ_RETHROW;
	}
}

SharedState::~SharedState()
{
	if (p != nullptr)
		delete p;
}

#ifdef MKXPZ_RETRO

void SharedState::sandbox_reinit()
{
	p->texPool.clear();
	TEXFBO::clear(p->atlasTex);

	_globalIBO->forget(); // The buffer doesn't exist anymore since the graphics context was reinitialized, so this makes sure we don't try to delete it when `delete _globalIBO` is called in the next line
	delete _globalIBO;
	_globalIBO = new GlobalIBO;
	_globalIBO->ensureSize(1);

	p->gpQuad.reinit();
	p->reinit();

	{
		// Ignore errors
		Exception e;
		p->shaders.reinit(e);
	}
}
#endif // MKXPZ_RETRO
