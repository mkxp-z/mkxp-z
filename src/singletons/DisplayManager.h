//
// Created by fcors on 10/21/2023.
//

#pragma once

#include "IGraphics.h"
#include "ITexPool.h"

#include <memory>
#include "signal.hpp"

#define DISPLAY_MANAGER DisplayManager::getInstance()
#define GRAPHICS DisplayManager::getInstance().getGraphics()
#define GL_STATE DisplayManager::getInstance().getGlState()
#define TEX_POOL DisplayManager::getInstance().getTexPool()
#define SHADERS DisplayManager::getInstance().getShaderSet()
#define SCREEN DisplayManager::getInstance().getScreen()

class IGraphics;

class ITexPool;

class GLState;

class ShaderSet;

class DisplayManagerPrivate;

struct GlobalIBO;
struct Quad;

class Scene;

struct Vec2i;
struct TEXFBO;

class DisplayManager {
    DisplayManager();

    ~DisplayManager();

public:
    static DisplayManager &getInstance();

    IGraphics &getGraphics();

    GLState &getGlState() const;

    ITexPool &getTexPool() const;

    ShaderSet &getShaderSet() const;

    Scene &getScreen() const;

    void setScreen(std::unique_ptr<Scene> &&screen);

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
    std::unique_ptr<DisplayManagerPrivate> m_private;
    std::unique_ptr<GlobalIBO> m_globalIBO;

    std::unique_ptr<IGraphics> m_graphics;
    std::unique_ptr<GLState> m_glState;
    std::unique_ptr<ITexPool> m_texPool;
    std::unique_ptr<ShaderSet> m_shaderSet;
    std::unique_ptr<Scene> m_screen;
};
