//
// Created by fcors on 10/20/2023.
//
#pragma once

#include "sigslot/signal.hpp"

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

class ISharedState {
public:
    SDL_Window *sdlWindow() const;

    Scene *screen() const;
    void setScreen(Scene &screen);

    FileSystem &fileSystem() const;

    EventThread &eThread() const;
    RGSSThreadData &rtData() const;
    Config &config() const;

    Graphics &graphics() const;
    Input &input() const;
    Audio &audio() const;

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

    static SharedState *instance;
    static int rgssVersion;

    /* This function will throw an Exception instance
     * on initialization error */
    static void initInstance(RGSSThreadData *threadData);
    static void finiInstance();
};
