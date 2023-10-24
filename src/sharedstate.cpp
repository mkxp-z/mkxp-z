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

#include "util.h"
#include "filesystem.h"
#include "graphics.h"
#include "input.h"
#include "audio.h"
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
#include "scene.h"

#include <unistd.h>
#include <stdio.h>
#include <string>
#include <chrono>

int SharedState::rgssVersion = 0;
static std::unique_ptr<GlobalIBO> _globalIBO;

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

struct SharedStatePrivate {
    SDL_Window *sdlWindow;
    std::shared_ptr<Scene> screen;

    std::shared_ptr<FileSystem> filesystem;

    std::shared_ptr<AbstractEventThread> eThread;
    std::shared_ptr<RGSSThreadData> rtData;
    std::shared_ptr<Config> config;

    SharedMidiState midiState;

    std::unique_ptr<IGraphics> graphics;
    std::unique_ptr<IInput> input;
    std::unique_ptr<IAudio> audio;

    GLState _glState;

    ShaderSet shaders;

    TexPool texPool;

    SharedFontState fontState;
    std::unique_ptr<Font> defaultFont;

	TEX::ID globalTex;
	int globalTexW, globalTexH;
	bool globalTexDirty;

    TEXFBO gpTexFBO;

    TEXFBO atlasTex;

    Quad gpQuad;

    unsigned int stampCounter;

    std::chrono::time_point<std::chrono::steady_clock> startupTime;

    SharedStatePrivate(std::shared_ptr<RGSSThreadData> threadData)
            : sdlWindow(threadData->window),
              filesystem(threadData->filesystem),
              eThread(threadData->ethread),
              rtData(threadData),
              config(threadData->config),
              midiState(*threadData->config),
              graphics(new Graphics(threadData.get())),
              input(new Input(*threadData)),
              audio(new Audio(*threadData)),
              _glState(*threadData->config),
              fontState(*threadData->config),
              stampCounter(0) {

        startupTime = std::chrono::steady_clock::now();

		/* Shaders have been compiled in ShaderSet's constructor */
		if (gl.ReleaseShaderCompiler)
			gl.ReleaseShaderCompiler();

        std::string archPath = config->execName + gameArchExt();

        /* Check if a game archive exists */
        FILE *tmp = fopen(archPath.c_str(), "rb");
        if (tmp) {
            filesystem->addPath(archPath.c_str());
            fclose(tmp);
        }

        filesystem->addPath(".");

        for (size_t i = 0; i < config->rtps.size(); ++i)
            filesystem->addPath(config->rtps[i].c_str());

        if (config->pathCache)
            filesystem->createPathCache();

        filesystem->initFontSets(fontState);

		globalTexW = 128;
		globalTexH = 64;

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

		/* RGSS3 games will call setup_midi, so there's
		 * no need to do it on startup */
		if (rgssVer <= 2)
			midiState.initIfNeeded(*threadData->config);
	}

	~SharedStatePrivate()
	{
		TEX::del(globalTex);
		TEXFBO::fini(gpTexFBO);
		TEXFBO::fini(atlasTex);
	}
};

std::shared_ptr<SharedState> SharedState::initInstance(std::shared_ptr<RGSSThreadData> threadData) {
    /* This section is tricky because of dependencies:
     * SharedState depends on GlobalIBO existing,
     * Font depends on SharedState existing */

    rgssVersion = threadData->config->rgssVersion;

    _globalIBO = std::make_unique<GlobalIBO>();
    _globalIBO->ensureSize(1);

    std::shared_ptr<SharedState> ret;
    std::unique_ptr<Font> defaultFont;

    try {
        ret.reset(new SharedState(threadData));
        Font::initDefaults(ret->p->fontState);
        defaultFont = std::make_unique<Font>();
    }
    catch (const Exception &exc) {
        _globalIBO = nullptr;
        ret = nullptr;

        throw exc;
    }

    ret->p->defaultFont = std::move(defaultFont);
    return ret;
}

void SharedState::setScreen(std::shared_ptr<Scene> screen) {
    p->screen = screen;
}

#define GSATT(type, lower) \
    type SharedState :: lower() const \
    { \
        return p->lower; \
    }

#define GSREF(type, lower) \
    type &SharedState :: lower() const \
    { \
        return *p->lower;\
        }

GSATT(SDL_Window*, sdlWindow)

GSATT(std::shared_ptr<Scene>, screen)

GSATT(std::shared_ptr<FileSystem>, filesystem)

GSREF(AbstractEventThread, eThread)

GSATT(std::shared_ptr<RGSSThreadData>, rtData)

GSATT(std::shared_ptr<Config>, config)

GSREF(IGraphics, graphics)

GSREF(IInput, input)

GSREF(IAudio, audio)

GSATT(GLState &, _glState)

GSATT(ShaderSet &, shaders)

GSATT(TexPool &, texPool)

GSATT(Quad &, gpQuad)

GSATT(SharedFontState &, fontState)

GSATT(SharedMidiState &, midiState)

void SharedState::ensureQuadIBO(size_t minSize) {
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
    if (!p->rtData->rqTerm)
        return;

    p->rtData->rqTermAck.set();
    p->texPool.disable();
}

void SharedState::checkReset()
{
    if (!p->rtData->rqReset)
        return;

    p->rtData->rqReset.clear();
}

Font &SharedState::defaultFont() const
{
	return *p->defaultFont;
}

double SharedState::runTime() {
    if (!p) return 0;
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now - p->startupTime).count() / 1000.0 / 1000.0;
}

unsigned int SharedState::genTimeStamp()
{
	return p->stampCounter++;
}

SharedState::SharedState(std::shared_ptr<RGSSThreadData> threadData) : p(
        std::make_unique<SharedStatePrivate>(std::move(threadData)))
{
    p->screen = p->graphics->getScreen();
}

SharedState::~SharedState() {
    _globalIBO = nullptr;
}