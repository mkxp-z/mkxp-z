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
#include "encoding.h"
#include "sharedmidistate.h"

#include <unistd.h>
#include <stdio.h>
#include <string>
#include <chrono>

#include <SDL_endian.h>
#include <SDL_filesystem.h>

#ifdef MKXPZ_EXP_FS
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include "ghc/filesystem.hpp"
namespace fs = ghc::filesystem;
#endif

#ifdef _WIN32
#  include <winreg.h>
#endif // _WIN32

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#  define MKXPZ_ENCODING_UTF16_NATIVE "UTF-16BE"
#else
#  define MKXPZ_ENCODING_UTF16_NATIVE "UTF-16LE"
#endif

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

static std::string resolveRtpPath(
	const std::string &rtpRaw,
	bool allowCurrentDir,
	const fs::path &rtpDir,
#ifdef _WIN32
	HKEY hkey
#else
	const void *_unused
#endif // _WIN32
)
{
	std::string rtp(rtpRaw);
	for (char &c : rtp) {
		if (c == '\\') {
			c = '/';
		}
	}

	if (fs::path(rtp).is_relative()) {
		if (allowCurrentDir && fs::exists(rtp)) {
			return rtp;
		}

		/* Search for the RTP in the pref path */
		if (!rtpDir.empty() && fs::exists(rtpDir / rtp)) {
			return rtpDir / rtp;
		}

#ifdef _WIN32
		/* Check in the Windows registry */
		if (hkey != nullptr) {
			std::string rtpRawUtf16(Encoding::convertString(rtpRaw, "UTF-8", MKXPZ_ENCODING_UTF16_NATIVE));
			rtpRawUtf16.push_back(0);
			std::vector<char> buffer;
			DWORD size;
			LSTATUS error = RegGetValueW(
				hkey,
				nullptr,
				(LPCWSTR)rtpRawUtf16.data(),
				RRF_RT_REG_SZ,
				nullptr,
				nullptr,
				&size
			);
			if (error != ERROR_SUCCESS) {
				size = 0;
			} else {
				do {
					buffer.resize(size);
					error = RegGetValueW(
						hkey,
						nullptr,
						(LPCWSTR)rtpRawUtf16.data(),
						RRF_RT_REG_SZ,
						nullptr,
						buffer.data(),
						&size
					);
				} while (error == ERROR_MORE_DATA);
				if (error != ERROR_SUCCESS) {
					size = 0;
				}
			}
			if (size != 0) {
				buffer.resize(size);
				buffer.pop_back();
				buffer.pop_back();
				return Encoding::convertString(std::string(std::make_move_iterator(buffer.begin()), std::make_move_iterator(buffer.end())), MKXPZ_ENCODING_UTF16_NATIVE);
			}
		}
#endif // _WIN32

		if (!allowCurrentDir) {
			throw Exception(Exception::PHYSFSError, "Failed to resolve %s RTP", rtp.c_str());
		}
	}

	return rtp;
}

struct SharedStatePrivate
{
	void *bindingData;
	SDL_Window *sdlWindow;
	Scene *screen;

	FileSystem fileSystem;

	EventThread &eThread;
	RGSSThreadData &rtData;
	Config &config;

	SharedMidiState midiState;

	Graphics graphics;
	Input input;
	Audio audio;

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

	unsigned int stampCounter;
    
    std::chrono::time_point<std::chrono::steady_clock> startupTime;

	SharedStatePrivate(RGSSThreadData *threadData)
	    : bindingData(0),
	      sdlWindow(threadData->window),
	      fileSystem(threadData->argv0, threadData->config.allowSymlinks),
	      eThread(*threadData->ethread),
	      rtData(*threadData),
	      config(threadData->config),
	      midiState(threadData->config),
	      graphics(threadData),
	      input(*threadData),
	      audio(*threadData),
	      _glState(threadData->config),
	      fontState(threadData->config),
	      stampCounter(0)
	{}
	
	void init(RGSSThreadData *threadData)
	{
        
        startupTime = std::chrono::steady_clock::now();
        
		/* Shaders have been compiled in ShaderSet's constructor */
		if (gl.ReleaseShaderCompiler)
			gl.ReleaseShaderCompiler();

		std::string archPath = config.execName + gameArchExt();

		for (size_t i = 0; i < config.patches.size(); ++i)
			fileSystem.addPath(config.patches[i].c_str());

		/* Check if a game archive exists */
		FILE *tmp = fopen(archPath.c_str(), "rb");
		if (tmp)
		{
			fileSystem.addPath(archPath.c_str());
			fclose(tmp);
		}

		fileSystem.addPath(".");

		{
			char *prefDir = SDL_GetPrefPath(nullptr, "mkxp-z");
			fs::path rtpDir;
			if (prefDir != nullptr) {
				rtpDir = std::string(prefDir) + "RTP";
				fs::create_directory(rtpDir);
			}
#ifdef _WIN32
			HKEY hkey = nullptr;
			const char *rtpKey;
			switch (rgssVer) {
				case 1:
					rtpKey = "Software\\Enterbrain\\RGSS\\RTP";
					break;
				case 2:
					rtpKey = "Software\\Enterbrain\\RGSS2\\RTP";
					break;
				case 3:
					rtpKey = "Software\\Enterbrain\\RGSS3\\RTP";
					break;
				default:
					assert(!"unreachable");
			}
			RegOpenKeyExA(
				HKEY_LOCAL_MACHINE,
				rtpKey,
				0,
				KEY_QUERY_VALUE | KEY_WOW64_32KEY,
				&hkey
			);
#else
			const void *hkey = nullptr;
#endif // _WIN32
			for (const std::string &rtpRaw : config.rtps) {
				fileSystem.addPath(resolveRtpPath(rtpRaw, true, rtpDir, hkey).c_str());
			}
			for (const std::string &rtpRaw : config.iniRtps) {
				try {
					fileSystem.addPath(resolveRtpPath(rtpRaw, false, rtpDir, hkey).c_str());
				} catch (const Exception &e) {
					Debug() << "Error mounting RTP" << rtpRaw << ":" << e.msg;
				}
			}
#ifdef _WIN32
			if (hkey != nullptr) {
				RegCloseKey(hkey);
			}
#endif // _WIN32
			if (prefDir != nullptr) {
				SDL_free(prefDir);
			}
		}

		if (config.pathCache)
			fileSystem.createPathCache();

		fileSystem.initFontSets(fontState);

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
			midiState.initIfNeeded(threadData->config);
	}

	~SharedStatePrivate()
	{
		TEX::del(globalTex);
		TEXFBO::fini(gpTexFBO);
		TEXFBO::fini(atlasTex);
	}
};

void SharedState::initInstance(RGSSThreadData *threadData)
{
	/* This section is tricky because of dependencies:
	 * SharedState depends on GlobalIBO existing,
	 * Font depends on SharedState existing */

	rgssVersion = threadData->config.rgssVersion;
    
	_globalIBO = new GlobalIBO();
	_globalIBO->ensureSize(1);

	SharedState::instance = 0;
	Font *defaultFont = 0;

	try
	{
		SharedState::instance = new SharedState(threadData);
		Font::initDefaults(instance->p->fontState);
		defaultFont = new Font();
	}
	catch (const Exception &exc)
	{
		delete _globalIBO;
		delete SharedState::instance;
		delete defaultFont;

		throw exc;
	}

	SharedState::instance->p->defaultFont = defaultFont;
}

void SharedState::finiInstance()
{
	delete SharedState::instance->p->defaultFont;

	delete SharedState::instance;

	delete _globalIBO;
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
GSATT(SDL_Window*, sdlWindow)
GSATT(Scene*, screen)
GSATT(FileSystem&, fileSystem)
GSATT(EventThread&, eThread)
GSATT(RGSSThreadData&, rtData)
GSATT(Config&, config)
GSATT(Graphics&, graphics)
GSATT(Input&, input)
GSATT(Audio&, audio)
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
	if (!p->rtData.rqTerm)
		return;

	p->rtData.rqTermAck.set();
	p->texPool.disable();
	scriptBinding->terminate();
}

void SharedState::checkReset()
{
	if (!p->rtData.rqReset)
		return;

	p->rtData.rqReset.clear();
	scriptBinding->reset();
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

SharedState::SharedState(RGSSThreadData *threadData)
{
	p = new SharedStatePrivate(threadData);
	SharedState::instance = this;
	try
	{
		p->init(threadData);
		p->screen = p->graphics.getScreen();
	}
	catch (const Exception &exc)
	{
		// If the "error" was the user quitting the game before the path cache finished building,
		// then just return
		if (rtData().rqTerm)
			return;
		
		delete p;
		SharedState::instance = 0;
		
		throw exc;
	}
}

SharedState::~SharedState()
{
	delete p;
}
