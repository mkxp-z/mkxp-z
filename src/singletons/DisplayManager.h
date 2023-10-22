//
// Created by fcors on 10/21/2023.
//

#pragma once

#include "IGraphics.h"
#include "ITexPool.h"

#include <memory>
#include "signal.hpp"

/*
#define DISPLAY_MANAGER DisplayManager::getInstance()
#define GRAPHICS DisplayManager::getInstance().getGraphics()
#define shState->_glState() DisplayManager::getInstance().getGlState()
#define shState->texPool() DisplayManager::getInstance().getTexPool()
#define shState->shaders() DisplayManager::getInstance().getShaderSet()
#define shState->screen() DisplayManager::getInstance().getScreen()
 */

class IGraphics;

class ITexPool;

class GLState;

class ShaderSet;

class DisplayManagerPrivate;

struct GlobalIBO;
struct Quad;

class Scene;

struct SDL_Window;
struct Vec2i;
struct TEXFBO;

using SdlWindowPtr = std::unique_ptr<SDL_Window, void (*)(SDL_Window *)>;

class DisplayManager {
    DisplayManager();

    ~DisplayManager();

public:

    static DisplayManager &getInstance();

    bool init();

    IGraphics &getGraphics();

    GLState &getGlState() const;

    ITexPool &getTexPool() const;

    ShaderSet &getShaderSet() const;

    Scene &getScreen() const;

    void setScreen(std::unique_ptr<Scene> &&screen);

    SDL_Window *getSdlWindow() const;

    void setSdlWindow(SdlWindowPtr &&window);

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

    sigslot::signal<> prepareDraw;

private:
    bool m_initialized = false;
    std::unique_ptr<DisplayManagerPrivate> m_private;
    std::unique_ptr<GlobalIBO> m_globalIBO;

    std::unique_ptr<IGraphics> m_graphics;
    std::unique_ptr<GLState> m_glState;
    std::unique_ptr<ITexPool> m_texPool;
    std::unique_ptr<ShaderSet> m_shaderSet;
    std::unique_ptr<Scene> m_screen;

    SdlWindowPtr m_window;
};
