//
// Created by fcors on 10/21/2023.
//

#include "DisplayManager.h"

#include <SDL.h>

#include <glstate.h>
#include <graphics.h>
#include <shader.h>
#include <scene.h>

#include "quad.h"

struct DisplayManagerPrivate {
    TEX::ID globalTex;
    int globalTexW;
    int globalTexH;
    bool globalTexDirty;

    TEXFBO gpTexFBO;

    TEXFBO atlasTex;

    Quad gpQuad;
};

DisplayManager::DisplayManager() : m_sdlWindow(nullptr, &SDL_DestroyWindow) {
    // TODO: Initialize the definite instances of the classes
}

DisplayManager::~DisplayManager() = default;

DisplayManager &DisplayManager::getInstance() {
    static DisplayManager displayManager;
    return displayManager;
}

IGraphics &DisplayManager::getGraphics() {
    return *m_graphics;
}

GLState &DisplayManager::getGlState() const {
    return *m_glState;
}

ITexPool &DisplayManager::getTexPool() const {
    return *m_texPool;
}

ShaderSet &DisplayManager::getShaderSet() const {
    return *m_shaderSet;
}

SDL_Window *DisplayManager::getSdlWindow() const {
    return m_sdlWindow.get();
}


void DisplayManager::ensureQuadIBO(size_t minSize) {
    m_globalIBO->ensureSize(minSize);
}

GlobalIBO &DisplayManager::globalIBO() {
    return *m_globalIBO;
}

void DisplayManager::bindTex() {
    TEX::bind(m_private->globalTex);

    if (m_private->globalTexDirty) {
        TEX::allocEmpty(m_private->globalTexW, m_private->globalTexH);
        m_private->globalTexDirty = false;
    }
}

void DisplayManager::ensureTexSize(int minW, int minH, Vec2i &currentSizeOut) {
    if (minW > m_private->globalTexW) {
        m_private->globalTexDirty = true;
        m_private->globalTexW = findNextPow2(minW);
    }

    if (minH > m_private->globalTexH) {
        m_private->globalTexDirty = true;
        m_private->globalTexH = findNextPow2(minH);
    }

    currentSizeOut = Vec2i(m_private->globalTexW, m_private->globalTexH);
}

TEXFBO &DisplayManager::gpTexFBO(int minW, int minH) {
    bool needResize = false;

    if (minW > m_private->gpTexFBO.width) {
        m_private->gpTexFBO.width = findNextPow2(minW);
        needResize = true;
    }

    if (minH > m_private->gpTexFBO.height) {
        m_private->gpTexFBO.height = findNextPow2(minH);
        needResize = true;
    }

    if (needResize) {
        TEX::bind(m_private->gpTexFBO.tex);
        TEX::allocEmpty(m_private->gpTexFBO.width, m_private->gpTexFBO.height);
    }

    return m_private->gpTexFBO;
}

Quad &DisplayManager::gpQuad() const {
    return m_private->gpQuad;
}

void DisplayManager::requestAtlasTex(int w, int h, TEXFBO &out) {
    TEXFBO tex;

    if (w == m_private->atlasTex.width && h == m_private->atlasTex.height) {
        tex = m_private->atlasTex;
        m_private->atlasTex = TEXFBO();
    } else {
        TEXFBO::init(tex);
        TEXFBO::allocEmpty(tex, w, h);
        TEXFBO::linkFBO(tex);
    }

    out = tex;
}

void DisplayManager::releaseAtlasTex(TEXFBO &tex) {
    /* No point in caching an invalid object */
    if (tex.tex == TEX::ID(0))
        return;

    TEXFBO::fini(m_private->atlasTex);

    m_private->atlasTex = tex;
}

Scene &DisplayManager::getScreen() const {
    return *m_screen;
}

void DisplayManager::setScreen(std::unique_ptr<Scene> &&screen) {
    m_screen = std::move(screen);
}