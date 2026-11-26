/*
 ** graphics.cpp
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

#include "graphics.h"

#include "binding.h"
#include "bitmap.h"
#include "config.h"
#include "debugwriter.h"
#include "disposable.h"
#include "etc.h"
#include "etc-internal.h"
#include "eventthread.h"
#include "filesystem.h"
#include "framedeadline.h"
#include "framerateaverage.h"
#include "frametimes.h"
#include "gl-fun.h"
#include "gl-util.h"
#include "glstate.h"
#include "intrulist.h"
#include "movie.h"
#include "quad.h"
#include "scene.h"
#include "shader.h"
#include "sharedstate.h"
#include "texpool.h"
#include "util.h"
#include "input.h"
#include "sprite.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_timer.h>
#include <SDL_video.h>
#include <SDL_mutex.h>
#include <SDL_thread.h>

#include <algorithm>
#include <vector>

#include <EGL/egl.h>
#include "app_bridge.h"
extern EGLDisplay s_eglDisplay;
extern EGLSurface s_eglSurface;
extern EGLContext s_eglContext;

// Local copies of mkxpGL_SwapWindow / mkxpGL_MakeCurrent from main.cpp.
// Duplicated because both TUs need access to the EGL globals.
static inline void graphicsGL_SwapWindow(SDL_Window * /*win*/) {
    eglSwapBuffers(s_eglDisplay, s_eglSurface);
}
static inline void graphicsGL_MakeCurrent(SDL_Window * /*win*/, SDL_GLContext ctx) {
    if (ctx) eglMakeCurrent(s_eglDisplay, s_eglSurface, s_eglSurface, s_eglContext);
    else eglMakeCurrent(s_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <cmath>

#define DEF_SCREEN_W (rgssVer == 1 ? 640 : 544)
#define DEF_SCREEN_H (rgssVer == 1 ? 480 : 416)

#define DEF_FRAMERATE (rgssVer == 1 ? 40 : 60)

struct PingPong {
    TEXFBO rt[2];
    uint8_t srcInd, dstInd;
    int screenW, screenH;
    
    PingPong(int screenW, int screenH)
    : srcInd(0), dstInd(1), screenW(screenW), screenH(screenH) {
        for (int i = 0; i < 2; ++i) {
            TEXFBO::init(rt[i]);
            TEXFBO::allocEmpty(rt[i], screenW, screenH);
            TEXFBO::linkFBO(rt[i]);
            gl.ClearColor(0, 0, 0, 1);
            FBO::clear();
        }
    }
    
    ~PingPong() {
        for (int i = 0; i < 2; ++i)
            TEXFBO::fini(rt[i]);
    }
    
    TEXFBO &backBuffer() { return rt[srcInd]; }
    
    TEXFBO &frontBuffer() { return rt[dstInd]; }
    
    /* Better not call this during render cycles */
    void resize(int width, int height) {
        screenW = width;
        screenH = height;
        
        for (int i = 0; i < 2; ++i)
            TEXFBO::allocEmpty(rt[i], width, height);
    }
    
    void startRender() { bind(); }
    
    void swapRender() {
        std::swap(srcInd, dstInd);
        
        bind();
    }
    
    void clearBuffers() {
        glState.clearColor.pushSet(Vec4(0, 0, 0, 1));
        
        for (int i = 0; i < 2; ++i) {
            FBO::bind(rt[i].fbo);
            FBO::clear();
        }
        
        glState.clearColor.pop();
    }
    
private:
    void bind() { FBO::bind(rt[dstInd].fbo); }
};

class ScreenScene : public Scene {
public:
    ScreenScene(int width, int height) : pp(width, height) {
        updateReso(width, height);
        
        brightEffect = false;
        brightnessQuad.setColor(Vec4());
    }
    
    void composite() {
        const int w = geometry.rect.w;
        const int h = geometry.rect.h;
        
        /* The slots of prepareDraw read bitmap textures */
        Bitmap::drawHeldText();
        shState->prepareDraw();
        
        pp.startRender();
        
        glState.viewport.set(IntRect(0, 0, w, h));
        
        FBO::clear();

        Scene::composite();
        
        if (brightEffect) {
            SimpleColorShader &shader = shState->shaders().simpleColor;
            shader.bind();
            shader.applyViewportProj();
            shader.setTranslation(Vec2i());
            
            brightnessQuad.draw();
        }
    }
    
    void requestViewportRender(const Vec4 &c, const Vec4 &f, const Vec4 &t) {
        const IntRect &viewpRect = glState.scissorBox.get();
        const IntRect &screenRect = geometry.rect;
        
        const bool toneRGBEffect = t.xyzNotNull();
        const bool toneGrayEffect = t.w != 0;
        const bool colorEffect = c.w > 0;
        const bool flashEffect = f.w > 0;
        
        if (toneGrayEffect) {
            pp.swapRender();
            
            if (!viewpRect.encloses(screenRect)) {
                /* Scissor test _does_ affect FBO blit operations,
                 * and since we're inside the draw cycle, it will
                 * be turned on, so turn it off temporarily */
                glState.scissorTest.pushSet(false);
                
                int scaleIsSpecial = GLMeta::blitScaleIsSpecial(pp.frontBuffer(), false, geometry.rect, pp.backBuffer(), geometry.rect);

                GLMeta::blitBegin(pp.frontBuffer(), false, scaleIsSpecial);
                GLMeta::blitSource(pp.backBuffer(), scaleIsSpecial);
                GLMeta::blitRectangle(geometry.rect, Vec2i());
                GLMeta::blitEnd();
                
                glState.scissorTest.pop();
            }
            
            GrayShader &shader = shState->shaders().gray;
            shader.bind();
            shader.setGray(t.w);
            shader.applyViewportProj();
            shader.setTexSize(screenRect.size());
            
            TEX::bind(pp.backBuffer().tex);
            
            glState.blend.pushSet(false);
            screenQuad.draw();
            glState.blend.pop();
        }
        
        if (!toneRGBEffect && !colorEffect && !flashEffect)
            return;
        
        FlatColorShader &shader = shState->shaders().flatColor;
        shader.bind();
        shader.applyViewportProj();
        
        if (toneRGBEffect) {
            /* First split up additive / substractive components */
            Vec4 add, sub;
            
            if (t.x > 0)
                add.x = t.x;
            if (t.y > 0)
                add.y = t.y;
            if (t.z > 0)
                add.z = t.z;
            
            if (t.x < 0)
                sub.x = -t.x;
            if (t.y < 0)
                sub.y = -t.y;
            if (t.z < 0)
                sub.z = -t.z;
            
            /* Then apply them using hardware blending */
            gl.BlendFuncSeparate(GL_ONE, GL_ONE, GL_ZERO, GL_ONE);
            
            if (add.xyzNotNull()) {
                gl.BlendEquation(GL_FUNC_ADD);
                shader.setColor(add);
                
                screenQuad.draw();
            }
            
            if (sub.xyzNotNull()) {
                gl.BlendEquation(GL_FUNC_REVERSE_SUBTRACT);
                shader.setColor(sub);
                
                screenQuad.draw();
            }
        }
        
        if (colorEffect || flashEffect) {
            gl.BlendEquation(GL_FUNC_ADD);
            gl.BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO,
                                 GL_ONE);
        }
        
        if (colorEffect) {
            shader.setColor(c);
            screenQuad.draw();
        }
        
        if (flashEffect) {
            shader.setColor(f);
            screenQuad.draw();
        }
        
        glState.blendMode.refresh();
    }
    
    void setBrightness(float norm) {
        brightnessQuad.setColor(Vec4(0, 0, 0, 1.0f - norm));
        
        brightEffect = norm < 1.0f;
    }
    
    void updateReso(int width, int height) {
        geometry.rect.w = width;
        geometry.rect.h = height;
        
        screenQuad.setTexPosRect(geometry.rect, geometry.rect);
        brightnessQuad.setTexPosRect(geometry.rect, geometry.rect);
        
        notifyGeometryChange();
    }
    
    void setResolution(int width, int height) {
        pp.resize(width, height);
        updateReso(width, height);
    }
    
    PingPong &getPP() { return pp; }
    
private:
    PingPong pp;
    Quad screenQuad;
    
    Quad brightnessQuad;
    bool brightEffect;
};

/* Nanoseconds per second */
#define NS_PER_S 1000000000

struct FPSLimiter {
    uint64_t lastTickCount;
    
    /* ticks per frame */
    int64_t tpf = 0;
    
    /* Ticks per second */
    const uint64_t tickFreq;
    
    /* Ticks per milisecond */
    const uint64_t tickFreqMS;
    
    /* Ticks per nanosecond */
    const double tickFreqNS;
    
    bool disabled;

    unsigned comebacks = 0;
    
    /* Data for frame timing adjustment */
    struct {
        /* Last tick count */
        uint64_t last;
        
        /* How far behind/in front we are for ideal frame timing */
        int64_t idealDiff;
        
        bool resetFlag;
    } adj;
    
    FPSLimiter(int desiredFPS)
    : lastTickCount(SDL_GetPerformanceCounter()),
    tickFreq(SDL_GetPerformanceFrequency()), tickFreqMS(tickFreq / 1000),
    tickFreqNS((double)tickFreq / NS_PER_S), disabled(false) {
        setDesiredFPS(desiredFPS);
        
        adj.last = SDL_GetPerformanceCounter();
        adj.idealDiff = 0;
        adj.resetFlag = false;
    }
    
    void setDesiredFPS(int value) {
        // Some games set Graphics.frame_rate on each frame.
        const int64_t newTpf = tickFreq / std::max(value, 1);
        if (newTpf == tpf)
            return;
        tpf = newTpf;
        if (mkxp_debugLogEnabled()) {
            char msg[80];
            std::snprintf(msg, sizeof(msg),
                          "FPSLimiter::setDesiredFPS(%d) -> tpf=%lld",
                          value, (long long)tpf);
            mkxp_debugLog("INFO", "fps-limiter", msg);
        }
    }
    
    void delay() {
        if (disabled)
            return;

        /* Start again from now after the engine was away, or the
         * limiter catches up on that time by skipping frames. */
        unsigned seen = mkxp_engineComebacks();
        if (seen != comebacks) {
            comebacks = seen;
            lastTickCount = adj.last = SDL_GetPerformanceCounter();
            adj.idealDiff = 0;
        }
        
        int64_t tickDelta = SDL_GetPerformanceCounter() - lastTickCount;
        // Runtime fast-forward (host bridge): scale target ticks-per-
        // frame down by the multiplier so the limiter releases the
        // thread sooner. Multiplier of 1 = no scaling. The frame-
        // adjust accumulator (idealDiff) re-baselines each frame, so
        // toggling fast-forward off mid-game settles back to normal
        // pacing within a few frames.
        int multiplier = mkxp_getFastForwardMultiplier();
        int64_t effectiveTpf = multiplier > 1 ? (tpf / multiplier) : tpf;
        int64_t toDelay = effectiveTpf - tickDelta;
        
        /* Compensate for the last delta
         * to the ideal timestep */
        toDelay -= adj.idealDiff;
        
        if (toDelay < 0)
            toDelay = 0;
        
        endFrameWork();
        delayTicks(toDelay);
        
        uint64_t now = lastTickCount = SDL_GetPerformanceCounter();
        int64_t diff = now - adj.last;
        adj.last = now;

        /* With fast-forward, the frame adjustment below still brings
         * the time between two calls to one full frame. */
        startFrameWork(tpf / tickFreqNS / 4);
        
        /* Recalculate our temporal position
         * relative to the ideal timestep */
        adj.idealDiff = diff - tpf + adj.idealDiff;
        
        if (adj.resetFlag) {
            adj.idealDiff = 0;
            adj.resetFlag = false;
        }
    }
    
    void resetFrameAdjust() { adj.resetFlag = true; }

    double frameSeconds() const { return (double)tpf / tickFreq; }
    
    /* If we're more than a full frame's worth
     * of ticks behind the ideal timestep,
     * there's no choice but to skip frame(s)
     * to catch up */
    bool frameSkipRequired() const {
        if (disabled)
            return false;
        
        return adj.idealDiff > tpf;
    }
    
private:
    void delayTicks(uint64_t ticks) {
#if defined(HAVE_NANOSLEEP)
        struct timespec req;
        uint64_t nsec = ticks / tickFreqNS;
        req.tv_sec = nsec / NS_PER_S;
        req.tv_nsec = nsec % NS_PER_S;
        errno = 0;
        
        while (nanosleep(&req, &req) == -1) {
            int err = errno;
            errno = 0;
            
            if (err == EINTR)
                continue;
            
            Debug() << "nanosleep failed. errno:" << err;
            SDL_Delay(ticks / tickFreqMS);
            break;
        }
#else
        SDL_Delay(ticks / tickFreqMS);
#endif
    }
};

struct GraphicsPrivate {
    /* Screen resolution, ie. the resolution at which
     * RGSS renders at (settable with Graphics.resize_screen).
     * Can only be changed from within RGSS */
    Vec2i scRes;
    Vec2i scResLores;
    
    /* Screen size, to which the rendered frames are scaled up.
     * This can be smaller than the window size when fixed aspect
     * ratio is enforced */
    Vec2i scSize;
    
    /* Actual physical size of the game window */
    Vec2i winSize;
    
    /* Offset in the game window at which the scaled game screen
     * is blitted inside the game window */
    Vec2i scOffset;
    
    // Scaling factor, used to display the screen properly
    // on Retina displays
    int scalingFactor;
    
    ScreenScene screen;
    RGSSThreadData *threadData;
    SDL_GLContext glCtx;
    
    int frameRate;
    int frameCount;
    int brightness;

    // Counts swapGLBuffer calls since the last fast-forward enable.
    // Used to gate which frames actually present (and thus pace via
    // vsync) when the runtime multiplier is > 1 - one swap every N
    // calls, so Ruby logic ticks Nx per visible frame.
    unsigned int presentSkipCounter = 0;
    
    double last_update;
    double last_delta;
    
    
    FPSLimiter fpsLimiter;
    
    // Can be set from Ruby. Takes priority over config setting.
    bool useFrameSkip;
    
    bool frozen;
    TEXFBO frozenScene;
    /* A copy of the last frame we put on the screen. `framePresented`
     * refreshes it after every swap. */
    TEXFBO lastPresentedFrame;
    bool hasLastPresentedFrame;
    Quad screenQuad;
    
    float backingScaleFactor;
    
    Vec2i integerScaleFactor;
    TEXFBO integerScaleBuffer;
    bool integerScaleActive;
    bool integerLastMileScaling;
    
    FrameRateAverage frameRateAverage;
    FrameTimes frameTimes;
    SDL_mutex *avgFPSLock;

    SDL_mutex *glResourceLock;
    Uint32 glResourceLockOwner;
    unsigned int glResourceLockDepth;
    bool multithreadedMode;
    
    /* Global list of all live Disposables
     * (disposed on reset) */
    IntruList<Disposable> dispList;
    
    GraphicsPrivate(RGSSThreadData *rtData)
    : scResLores(DEF_SCREEN_W, DEF_SCREEN_H),
    scRes(rtData->config.enableHires ? (int)lround(rtData->config.framebufferScalingFactor * DEF_SCREEN_W) : DEF_SCREEN_W,
        rtData->config.enableHires ? (int)lround(rtData->config.framebufferScalingFactor * DEF_SCREEN_H) : DEF_SCREEN_H),
    scSize(scRes),
    winSize(rtData->config.defScreenW, rtData->config.defScreenH),
    screen(scRes.x, scRes.y), threadData(rtData),
    glCtx((SDL_GLContext)s_eglContext),
    multithreadedMode(true),
    frameRate(DEF_FRAMERATE), frameCount(0), brightness(255),
    fpsLimiter(frameRate), useFrameSkip(rtData->config.frameSkip), frozen(false),
    last_update(0), last_delta(0), backingScaleFactor(1), integerScaleFactor(0, 0),
    glResourceLockOwner(0), glResourceLockDepth(0),
    integerScaleActive(rtData->config.integerScaling.active),
    integerLastMileScaling(rtData->config.integerScaling.lastMileScaling) {
        avgFPSLock = SDL_CreateMutex();
        glResourceLock = SDL_CreateMutex();
        
        /* Query the actual window and drawable sizes directly from SDL.
         * main.cpp also posts these to windowSizeMsg/drawableSizeMsg, but
         * the constructor shouldn't poll message queues for initialization.
         * The window is fully created before the RGSS thread starts, so
         * these reads are safe. */
        {
            int winW, winH;
            SDL_GetWindowSize(rtData->window, &winW, &winH);
            winSize = Vec2i(winW, winH);

            int drwW, drwH;
            // SDL_GL_GetDrawableSize returns logical points under ANGLE
            // (no SDL GL context). Query EGL surface size instead.
            {
                EGLint eglW = 0, eglH = 0;
                eglQuerySurface(s_eglDisplay, s_eglSurface, EGL_WIDTH, &eglW);
                eglQuerySurface(s_eglDisplay, s_eglSurface, EGL_HEIGHT, &eglH);
                drwW = eglW;
                drwH = eglH;
            }
            backingScaleFactor = (float)drwW / winW;
            winSize = Vec2i(drwW, drwH);
        }

        // Diagnostic: log the resolution stack so we can confirm
        // host-side `enableHires` / `framebufferScalingFactor`
        // edits actually reach the engine. iOS testers often see
        // "no visible difference" when bumping render scale; this
        // line tells us whether scRes scaled or stayed at the
        // RGSS default. Routes through mkxp_debugLog so it lands in
        // the per-session log file the host opens at game launch
        // (Debug()'s std::cerr goes nowhere on iOS).
        if (mkxp_debugLogEnabled()) {
            char msg[320];
            std::snprintf(msg, sizeof(msg),
                          "scRes=%dx%d scResLores=%dx%d winSize=%dx%d "
                          "backingScale=%.2f enableHires=%d "
                          "framebufferScalingFactor=%.2f smoothScaling=%d "
                          "fastForwardMultiplier=%d",
                          scRes.x, scRes.y,
                          scResLores.x, scResLores.y,
                          winSize.x, winSize.y,
                          backingScaleFactor,
                          rtData->config.enableHires ? 1 : 0,
                          rtData->config.framebufferScalingFactor,
                          rtData->config.smoothScaling,
                          mkxp_getFastForwardMultiplier());
            mkxp_debugLog("INFO", "graphics-init", msg);
        }

        if (integerScaleActive) {
            integerScaleFactor = Vec2i(0, 0);
            rebuildIntegerScaleBuffer();
        }
        
        recalculateScreenSize(rtData->config.fixedAspectRatio);
        updateScreenResoRatio(rtData);
        
        TEXFBO::init(frozenScene);
        TEXFBO::allocEmpty(frozenScene, scRes.x, scRes.y);
        TEXFBO::linkFBO(frozenScene);

        TEXFBO::init(lastPresentedFrame);
        TEXFBO::allocEmpty(lastPresentedFrame, scRes.x, scRes.y);
        TEXFBO::linkFBO(lastPresentedFrame);
        hasLastPresentedFrame = false;
        
        FloatRect screenRect(0, 0, scRes.x, scRes.y);
        screenQuad.setTexPosRect(screenRect, screenRect);
        
        fpsLimiter.resetFrameAdjust();
    }
    
    ~GraphicsPrivate() {
        TEXFBO::fini(frozenScene);
        TEXFBO::fini(lastPresentedFrame);
        TEXFBO::fini(integerScaleBuffer);
        SDL_DestroyMutex(avgFPSLock);
        SDL_DestroyMutex(glResourceLock);
    }
    
    void updateScreenResoRatio(RGSSThreadData *rtData) {
        /* Guard against zero scSize — can happen transiently during
         * rapid rotation when window size and safe area insets are
         * from different orientations. */
        if (scSize.x <= 0 || scSize.y <= 0) {
            char buf[128];
            snprintf(buf, sizeof(buf),
                     "REJECTED zero scSize=%dx%d in updateScreenResoRatio",
                     scSize.x, scSize.y);
            mkxp_debugLog("RESIZE", "graphics.cpp [C++]", buf);
            return;
        }

        Vec2 &ratio = rtData->sizeResoRatio;
        ratio.x = (float)scRes.x / scSize.x * backingScaleFactor;
        ratio.y = (float)scRes.y / scSize.y * backingScaleFactor;
        
        /* screenOffset feeds Input::mouseX/mouseY, whose mousePos is in
         * top-left-origin window points. scOffset.y is GL bottom-left,
         * so flip it (same conversion as the gameRect below); without
         * the flip every mouse Y lands offset by the letterbox delta
         * (negative near the top of the game on tall screens). */
        rtData->screenOffset.x = scOffset.x / backingScaleFactor;
        rtData->screenOffset.y =
            (winSize.y - scOffset.y - scSize.y) / backingScaleFactor;

        // Publish the game viewport rect in logical points for the touch overlay.
        // scOffset.y is in GL coordinates (origin bottom-left), convert to
        // screen coordinates (origin top-left) for UIKit.
        // Use UIKit screen scale (not SDL backingScaleFactor) for GL-to-point conversion.
        float uiSc = mkxp_getScreenScale();
        float screenY = (winSize.y - scOffset.y - scSize.y) / uiSc;
        mkxp_setGameRect(scOffset.x / uiSc, screenY,
                         scSize.x / uiSc, scSize.y / uiSc);
    }
    
    /* Places the picture inside a host-supplied window-fraction
     * region. Returns false when no region applies (none set,
     * orientation tag mismatch, or degenerate after clamping) -
     * the caller then runs the automatic path. The picture centers
     * in the region; the vertical-alignment preset and the safe
     * area apply only on the automatic path. */
    bool applyHostViewportRegion(bool fixedAspectRatio) {
        float hvX, hvY, hvW, hvH;
        bool hvPortrait;
        if (!mkxp_getHostViewportRegion(&hvX, &hvY, &hvW, &hvH, &hvPortrait))
            return false;

        /* Rotation safety: SDL's resize reaches this thread before
         * the host's per-orientation set/clear call lands. A region
         * tagged for the other orientation draws automatic placement
         * until the matching call arrives. */
        if (hvPortrait != (winSize.x < winSize.y))
            return false;

        /* winSize is already drawable pixels; fractions multiply it
         * directly, no scale factor. hvY is top-left origin. */
        int cx    = (int)(hvX * winSize.x + 0.5f);
        int cyTop = (int)(hvY * winSize.y + 0.5f);
        int cw    = (int)(hvW * winSize.x + 0.5f);
        int ch    = (int)(hvH * winSize.y + 0.5f);

        cx    = std::max(0, std::min(cx, winSize.x));
        cyTop = std::max(0, std::min(cyTop, winSize.y));
        cw    = std::min(cw, winSize.x - cx);
        ch    = std::min(ch, winSize.y - cyTop);

        if (cw < 1 || ch < 1) {
            char buf[128];
            snprintf(buf, sizeof(buf),
                     "REJECTED degenerate host region %dx%d in %dx%d",
                     cw, ch, winSize.x, winSize.y);
            mkxp_debugLog("RESIZE", "graphics.cpp [C++]", buf);
            return false;
        }

        if (fixedAspectRatio) {
            float resRatio = (float)scRes.x / scRes.y;
            scSize.x = cw;
            scSize.y = (int)(scSize.x / resRatio);
            if (scSize.y > ch) {
                scSize.y = ch;
                scSize.x = (int)(scSize.y * resRatio);
            }
        } else {
            scSize.x = cw;
            scSize.y = ch;
        }

        /* Center in the region. scOffset.y is GL bottom-left. */
        scOffset.x = cx + (cw - scSize.x) / 2;
        int cyGl = winSize.y - cyTop - ch;
        scOffset.y = cyGl + (ch - scSize.y) / 2;
        return true;
    }

    /* Enforces fixed aspect ratio, if desired */
    void recalculateScreenSize(bool fixedAspectRatio) {
        scSize = winSize;

        if (applyHostViewportRegion(fixedAspectRatio))
            return;

        {
            float saTop = 0, saBottom = 0, saLeft = 0, saRight = 0;
            mkxp_getSafeAreaInsets(&saTop, &saBottom, &saLeft, &saRight);

            float uiScale = mkxp_getScreenScale();
            int saTopPx   = (int)(saTop    * uiScale);
            int saBotPx   = (int)(saBottom * uiScale);
            int saLeftPx  = (int)(saLeft   * uiScale);
            int saRightPx = (int)(saRight  * uiScale);

            // Available area inside safe insets.
            // During rapid rotation the window size and safe area insets
            // may come from different orientations, producing negative
            // or zero available dimensions.  Clamp to 1 to prevent
            // invalid viewport calculations and GL state corruption.
            bool isPortrait = winSize.x < winSize.y;

            // In landscape, ignore top/bottom safe areas — only left/right
            // matter (notch). The home indicator overlaps the game but
            // auto-hides during gameplay.
            int effectiveSaTop = isPortrait ? saTopPx : 0;
            int effectiveSaBot = isPortrait ? saBotPx : 0;

            int rawAvailW = winSize.x - saLeftPx - saRightPx;
            int rawAvailH = winSize.y - effectiveSaTop - effectiveSaBot;
            int availW = std::max(1, rawAvailW);
            int availH = std::max(1, rawAvailH);

            if (rawAvailW <= 0 || rawAvailH <= 0) {
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "CLAMPED avail: raw=%dx%d clamped=%dx%d "
                         "winSize=%dx%d sa=T%.0f B%.0f L%.0f R%.0f (px: T%d B%d L%d R%d) uiScale=%.1f",
                         rawAvailW, rawAvailH, availW, availH,
                         winSize.x, winSize.y,
                         saTop, saBottom, saLeft, saRight,
                         saTopPx, saBotPx, saLeftPx, saRightPx, uiScale);
                mkxp_debugLog("RESIZE", "graphics.cpp [C++]", buf);
            }

            if (fixedAspectRatio) {
                // Fit game within the safe area while preserving aspect ratio
                float resRatio = (float)scRes.x / scRes.y;
                scSize.x = availW;
                scSize.y = (int)(scSize.x / resRatio);
                if (scSize.y > availH) {
                    scSize.y = availH;
                    scSize.x = (int)(scSize.y * resRatio);
                }
            } else {
                // Stretch to fill the entire safe area
                scSize.x = availW;
                scSize.y = availH;
            }

            if (winSize.x < winSize.y) {
                // Portrait: position game within safe area based on vertical alignment.
                // Controls go below the game viewport.
                scOffset.x = saLeftPx + (availW - scSize.x) / 2;

                MKXPVerticalAlignment vAlign = mkxp_getVerticalAlignment();
                if (vAlign == MKXP_VALIGN_TOP) {
                    // Top: game pressed against top safe edge
                    scOffset.y = winSize.y - saTopPx - scSize.y;
                } else if (vAlign == MKXP_VALIGN_CENTER) {
                    // Center: game centered within safe area
                    scOffset.y = saBotPx + (availH - scSize.y) / 2;
                } else {
                    // Top-center (default): midpoint between top and center
                    int topY = winSize.y - saTopPx - scSize.y;
                    int centerY = saBotPx + (availH - scSize.y) / 2;
                    scOffset.y = (topY + centerY) / 2;
                }
            } else {
                // Landscape: center within available area (no top/bottom safe area)
                scOffset.x = saLeftPx + (availW - scSize.x) / 2;
                scOffset.y = (availH - scSize.y) / 2;
            }
        }
    }
    
    static int findHighestFittingScale(int base, int target) {
        int scale = 1;
        
        while (base * scale <= target)
            scale++;
        
        return std::max(scale - 1, 1);
    }
    
    /* Returns whether a new scale was found */
    bool findHighestIntegerScale()
    {
        Vec2i newScale(findHighestFittingScale(scRes.x, winSize.x),
                       findHighestFittingScale(scRes.y, winSize.y));
        
        if (threadData->config.fixedAspectRatio)
        {
            /* Limit both factors to the smaller of the two */
            newScale.x = newScale.y = std::min(newScale.x, newScale.y);
        }
        
        if (newScale == integerScaleFactor)
            return false;
        
        integerScaleFactor = newScale;
        return true;
    }
    
    void rebuildIntegerScaleBuffer()
    {
        TEXFBO::fini(integerScaleBuffer);
        TEXFBO::init(integerScaleBuffer);
        TEXFBO::allocEmpty(integerScaleBuffer, scRes.x * integerScaleFactor.x,
                           scRes.y * integerScaleFactor.y);
        TEXFBO::linkFBO(integerScaleBuffer);
    }
    
    bool integerScaleStepApplicable() const
    {
        if (!integerScaleActive)
            return false;
        
        if (integerScaleFactor.x < 1 || integerScaleFactor.y < 1) // XXX should be < 2, this is for testing only
            return false;
        
        return true;
    }
    
    void checkResize(bool skipIntScaleBuffer = false) {
        Vec2i oldWinSize = winSize;
        bool sizeChanged = threadData->windowSizeMsg.poll(winSize);

        bool insetsChanged = mkxp_consumeSafeAreaInsetsChanged();
        if (insetsChanged && !sizeChanged) {
            // During rotation iOS can fire several size/inset events per
            // frame. Skip the snprintf formatting entirely when debug
            // logging is disabled - this was a measurable hitch before.
            if (mkxp_debugLogEnabled()) {
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "insetsChanged (no size change) winSize=%dx%d",
                         winSize.x, winSize.y);
                mkxp_debugLog("RESIZE", "graphics.cpp [C++]", buf);
            }

            recalculateScreenSize(threadData->config.fixedAspectRatio);
            updateScreenResoRatio(threadData);

            if (mkxp_debugLogEnabled()) {
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "after insets recalc: scSize=%dx%d scOffset=%d,%d bsf=%.3f",
                         scSize.x, scSize.y, scOffset.x, scOffset.y, backingScaleFactor);
                mkxp_debugLog("RESIZE", "graphics.cpp [C++]", buf);
            }

            SDL_Rect screen = {scOffset.x, scOffset.y, scSize.x, scSize.y};
            threadData->ethread->notifyGameScreenChange(screen);
        }

        if (sizeChanged) {
            /* Drain all pending async GL work (e.g. pixel processing
             * dispatched by the previous SwapWindow) before touching
             * any GL state.  Without this, rotating the device can
             * cause a SIGSEGV in libGLImage on iOS. */
            glFinish();

            /* Query the actual size in pixels, not units */
            Vec2i drawableSize(winSize);
            threadData->drawableSizeMsg.poll(drawableSize);

            if (mkxp_debugLogEnabled()) {
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "sizeChanged: winSize=%dx%d drawable=%dx%d (was %dx%d)",
                         winSize.x, winSize.y, drawableSize.x, drawableSize.y,
                         oldWinSize.x, oldWinSize.y);
                mkxp_debugLog("RESIZE", "graphics.cpp [C++]", buf);
            }

            /* Guard against zero dimensions during rotation transitions.
             * iOS can momentarily report 0-width or 0-height while the
             * window is being resized.  A zero winSize.x would cause
             * division-by-zero in backingScaleFactor, producing inf/NaN
             * that permanently corrupts all viewport calculations.
             * Restore winSize so future frames keep using the last good value. */
            if (winSize.x <= 0 || winSize.y <= 0 ||
                drawableSize.x <= 0 || drawableSize.y <= 0) {
                if (mkxp_debugLogEnabled()) {
                    char buf[256];
                    snprintf(buf, sizeof(buf),
                             "REJECTED zero dims: winSize=%dx%d drawable=%dx%d, restoring %dx%d",
                             winSize.x, winSize.y, drawableSize.x, drawableSize.y,
                             oldWinSize.x, oldWinSize.y);
                    mkxp_debugLog("RESIZE", "graphics.cpp [C++]", buf);
                }
                winSize = oldWinSize;
                return;
            }
            
            backingScaleFactor = (float)drawableSize.x / winSize.x;
            winSize = drawableSize;
            
            /* Make sure integer buffers are rebuilt before screen offsets are
             * calculated so we have the final allocated buffer size ready */
            if (integerScaleActive && findHighestIntegerScale() && !skipIntScaleBuffer)
                rebuildIntegerScaleBuffer();
            
            /* some GL drivers change the viewport on window resize */
            glState.viewport.refresh();
            recalculateScreenSize(threadData->config.fixedAspectRatio);
            updateScreenResoRatio(threadData);

            if (mkxp_debugLogEnabled()) {
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "after resize: scSize=%dx%d scOffset=%d,%d bsf=%.3f",
                         scSize.x, scSize.y, scOffset.x, scOffset.y, backingScaleFactor);
                mkxp_debugLog("RESIZE", "graphics.cpp [C++]", buf);
            }
            
            SDL_Rect screen = {scOffset.x, scOffset.y, scSize.x, scSize.y};
            threadData->ethread->notifyGameScreenChange(screen);
        }
    }
    
    void checkShutDownReset() {
        shState->checkShutdown();
        shState->checkReset();
    }
    
    void shutdown() {
        threadData->rqTermAck.set();
        shState->texPool().disable();
        
        getActiveScriptBinding()->terminate();
    }
    
    void markFrameTime(FrameTimes::Part part) {
        if (mkxp_debugLogEnabled())
            frameTimes.mark(part, mkxp_engineSeconds());
    }

    void frameTimeDone(bool drawn) {
        if (!mkxp_debugLogEnabled())
            return;

        std::string report =
            frameTimes.frameDone(fpsLimiter.frameSeconds(), drawn);
        if (!report.empty())
            mkxp_debugLog("INFO", "frame-times", report.c_str());
    }

    void delayFrame() {
        markFrameTime(FrameTimes::Draw);
        fpsLimiter.delay();
        markFrameTime(FrameTimes::Sleep);
    }

    void swapGLBuffer() {
        delayFrame();
        graphicsGL_SwapWindow(threadData->window);
        markFrameTime(FrameTimes::Swap);
        frameTimeDone(true);

        ++frameCount;

        threadData->ethread->notifyFrame();

        if (mkxp_isGLContextBroken()) {
            mkxp_setErrorMessage(
                "The graphics context crashed. Close the app from the app switcher and reopen it.");
            shutdown();
            return;
        }

        mkxp_signalFrameRendered();
    }
    
    void compositeToBuffer(TEXFBO &buffer) {
        compositeToBufferScaled(buffer, scRes.x, scRes.y);
    }

    void compositeToBufferScaled(TEXFBO &buffer, int destWidth, int destHeight) {
        screen.composite();
        
        int scaleIsSpecial = GLMeta::blitScaleIsSpecial(buffer, false, IntRect(0, 0, destWidth, destHeight), screen.getPP().frontBuffer(), IntRect(0, 0, scRes.x, scRes.y));

        GLMeta::blitBegin(buffer, false, scaleIsSpecial);
        GLMeta::blitSource(screen.getPP().frontBuffer(), scaleIsSpecial);
        GLMeta::blitRectangle(IntRect(0, 0, scRes.x, scRes.y), IntRect(0, 0, destWidth, destHeight));
        GLMeta::blitEnd();
    }
    
    void metaBlitBufferFlippedScaled(int scaleIsSpecial) {
        metaBlitBufferFlippedScaled(scRes, scaleIsSpecial);
        GLMeta::blitRectangle(
                              IntRect(0, 0, scRes.x, scRes.y),
                              IntRect(scOffset.x,
                                      (scSize.y + scOffset.y),
                                      scSize.x,
                                      -scSize.y),
                              GLMeta::smoothScalingMethod(scaleIsSpecial) == Bilinear);
    }
    
    void metaBlitBufferFlippedScaled(const Vec2i &sourceSize, int scaleIsSpecial, bool forceNearestNeighbor=false) {
        GLMeta::blitRectangle(IntRect(0, 0, sourceSize.x, sourceSize.y),
                              IntRect(scOffset.x, scSize.y+scOffset.y, scSize.x, -scSize.y),
                              !forceNearestNeighbor && GLMeta::smoothScalingMethod(scaleIsSpecial) == Bilinear);
    }
    
    void redrawScreen() {
        screen.composite();
        
        // maybe unspaghetti this later
        if (integerScaleStepApplicable() && !integerLastMileScaling)
        {
            int scaleIsSpecial = GLMeta::blitScaleIsSpecial(integerScaleBuffer, false, IntRect(0, 0, scSize.x, scSize.y), screen.getPP().frontBuffer(), IntRect(0, 0, scRes.x, scRes.y));

            GLMeta::blitBeginScreen(winSize, scaleIsSpecial);
            GLMeta::blitSource(screen.getPP().frontBuffer(), scaleIsSpecial);
            
            FBO::clear();
            metaBlitBufferFlippedScaled(scRes, scaleIsSpecial, true);
            GLMeta::blitEnd();
            
            swapGLBuffer();
            updateAvgFPS();
            return;
        }
        
        if (integerScaleStepApplicable())
        {
            int scaleIsSpecial = GLMeta::blitScaleIsSpecial(integerScaleBuffer, false, IntRect(0, 0, integerScaleBuffer.width, integerScaleBuffer.height), screen.getPP().frontBuffer(), IntRect(0, 0, scRes.x, scRes.y));

            assert(integerScaleBuffer.tex != TEX::ID(0));
            GLMeta::blitBegin(integerScaleBuffer, false, scaleIsSpecial);
            GLMeta::blitSource(screen.getPP().frontBuffer(), scaleIsSpecial);
            
            GLMeta::blitRectangle(IntRect(0, 0, scRes.x, scRes.y),
                                  IntRect(0, 0, integerScaleBuffer.width, integerScaleBuffer.height),
                                  false);
            
            GLMeta::blitEnd();
        }
        

        Vec2i sourceSize;

        if (integerScaleActive)
        {
            sourceSize = Vec2i(integerScaleBuffer.width, integerScaleBuffer.height);
        }
        else
        {
            sourceSize = scRes;
        }

        int scaleIsSpecial = GLMeta::blitScaleIsSpecial(integerScaleBuffer, false, IntRect(0, 0, scSize.x, scSize.y), integerScaleActive ? integerScaleBuffer : screen.getPP().frontBuffer(), IntRect(0, 0, sourceSize.x, sourceSize.y));

        GLMeta::blitBeginScreen(winSize, scaleIsSpecial);
        //GLMeta::blitSource(screen.getPP().frontBuffer(), scaleIsSpecial);

        if (integerScaleActive)
        {
            GLMeta::blitSource(integerScaleBuffer, scaleIsSpecial);
        }
        else
        {
            GLMeta::blitSource(screen.getPP().frontBuffer(), scaleIsSpecial);
        }

        if (mkxp_getShowViewportBounds()) {
            float r, g, b, a;
            mkxp_getViewportBoundsColor(&r, &g, &b, &a);
            glState.clearColor.pushSet(Vec4(r, g, b, a));
            FBO::clear();
            glState.clearColor.pop();
        } else {
            FBO::clear();
        }
        metaBlitBufferFlippedScaled(sourceSize, scaleIsSpecial);

        GLMeta::blitEnd();

        swapGLBuffer();

        updateAvgFPS();
    }
    
    void checkSyncLock() {
        if (!threadData->syncPoint.mainSyncLocked())
            return;
        
        graphicsGL_MakeCurrent(threadData->window, 0);
        threadData->syncPoint.waitMainSync();
        graphicsGL_MakeCurrent(threadData->window, glCtx);
        
        fpsLimiter.resetFrameAdjust();
    }
    
    double averageFPS() {
        SDL_LockMutex(avgFPSLock);
        double ret = frameRateAverage.average(frameRate);
        SDL_UnlockMutex(avgFPSLock);
        return ret;
    }

    /* Under avgFPSLock, because the host reads frameRate through
     * averageFPS() from its own thread. */
    void storeFrameRate(int value) {
        SDL_LockMutex(avgFPSLock);
        frameRate = value;
        SDL_UnlockMutex(avgFPSLock);
    }
    
    void setLock(bool force = false) {
        if (!(force || multithreadedMode)) return;

        Uint32 currentThread = SDL_ThreadID();
        if (glResourceLockOwner == currentThread) {
            ++glResourceLockDepth;
            return;
        }
        
        SDL_LockMutex(glResourceLock);
        glResourceLockOwner = currentThread;
        glResourceLockDepth = 1;
        graphicsGL_MakeCurrent(threadData->window, threadData->glContext);
    }

    void releaseLock(bool force = false) {
        if (!(force || multithreadedMode)) return;

        Uint32 currentThread = SDL_ThreadID();
        if (glResourceLockOwner != currentThread || glResourceLockDepth == 0)
            return;

        if (--glResourceLockDepth > 0)
            return;

        glResourceLockOwner = 0;
        
        SDL_UnlockMutex(glResourceLock);
    }

    // Not shState->runTime(): fast-forward speeds that up, and this rate
    // must count the frames that the screen shows.
    void updateAvgFPS() {
        SDL_LockMutex(avgFPSLock);
        frameRateAverage.frameShown(mkxp_engineSeconds());
        SDL_UnlockMutex(avgFPSLock);
    }

    /* Blit the frozen scene buffer to the screen and swap.
     * Used by fadeout/fadein when the scene is frozen. */
    void blitFrozenSceneToScreen() {
        int scaleIsSpecial = GLMeta::blitScaleIsSpecial(
            integerScaleBuffer, false,
            IntRect(0, 0, scSize.x, scSize.y),
            frozenScene, IntRect(0, 0, scRes.x, scRes.y));

        GLMeta::blitBeginScreen(scSize, scaleIsSpecial);
        GLMeta::blitSource(frozenScene, scaleIsSpecial);

        FBO::clear();
        metaBlitBufferFlippedScaled(scaleIsSpecial);

        GLMeta::blitEnd();

        swapGLBuffer();
        updateAvgFPS();
        framePresented(frozenScene);
    }

    /* Copy the frame we just swapped, so a later pause can still read
     * it after the next composite has overwritten the source buffer.
     * `presented` is the buffer the caller blitted to the screen. Each
     * caller presents a different one: the ping-pong front buffer, the
     * frozen scene, or the transition buffer.
     *
     * This costs one same-size blit per frame. With gl.BlitFramebuffer
     * present it is a GPU-side copy. Without it, GLMeta falls back to a
     * textured quad. */
    void keepPresentedFrame(TEXFBO &presented) {
        const int w = presented.width;
        const int h = presented.height;
        if (w <= 0 || h <= 0)
            return;

        if (lastPresentedFrame.width != w || lastPresentedFrame.height != h)
            TEXFBO::allocEmpty(lastPresentedFrame, w, h);

        const IntRect rect(0, 0, w, h);
        int scaleIsSpecial = GLMeta::blitScaleIsSpecial(lastPresentedFrame, false, rect, presented, rect);

        GLMeta::blitBegin(lastPresentedFrame, false, scaleIsSpecial);
        GLMeta::blitSource(presented, scaleIsSpecial);
        GLMeta::blitRectangle(rect, rect);
        GLMeta::blitEnd();

        hasLastPresentedFrame = true;
    }

    /* mkxp_checkPause() handles audio source pausing and blocks until
     * resumed. We only reset frame timing afterward, so the limiter
     * doesn't try to catch up for the time spent paused.
     *
     * Before blocking, we capture an RGBA snapshot. The SDL window is
     * always fullscreen behind the SwiftUI layer and can't participate
     * in SwiftUI view transitions. The snapshot is a still copy that
     * SwiftUI places at the game viewport's position (gameRect) during
     * the hero zoom animation, so the transition appears to zoom into
     * the live game. Once the animation finishes, the snapshot is
     * discarded and the real SDL rendering takes over.
     *
     * We snapshot `lastPresentedFrame`, not the frame we just swapped.
     * The request arrives on the main thread while the player looks at
     * the previous frame, and the swap that follows is the first point
     * where the engine can answer. The two frames are 16 ms apart in a
     * normal update loop, but a scene change runs Ruby with no Graphics
     * call for as long as the map takes to load, and the frame after
     * that gap belongs to the new scene. An empty new scene composites
     * to black, so reading the fresh frame put a black picture in the
     * pause transition.
     *
     * `fallback` covers the first frame of a session, before any frame
     * is kept.
     *
     * We never read FBO 0, because iOS gives undefined content for the
     * on-screen framebuffer after swapBuffers. */
    void checkPause(TEXFBO &fallback) {
        if (!mkxp_isPaused() && !mkxp_isPauseRequested())
            return;

        TEXFBO &shown = hasLastPresentedFrame ? lastPresentedFrame : fallback;

        /* The engine's 2D projection maps Y top-to-bottom, so
         * glReadPixels on these FBOs produces top-down pixel data.
         * No vertical flip needed. */
        int w = shown.width;
        int h = shown.height;
        if (w > 0 && h > 0) {
            FBO::ID prevFBO = FBO::boundFramebufferID;
            FBO::bind(shown.fbo);

            std::vector<uint8_t> pixels(w * h * 4);
            gl.ReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

            FBO::bind(prevFBO);
            mkxp_setSnapshot(pixels.data(), w, h);
        }

        mkxp_checkPause();
    }

    /* Call right after every swap. */
    void framePresented(TEXFBO &presented) {
        checkPause(presented);
        keepPresentedFrame(presented);
        markFrameTime(FrameTimes::Draw);
    }
};

Graphics::Graphics(RGSSThreadData *data) {
    p = new GraphicsPrivate(data);
    if (data->config.fixedFramerate > 0) {
        // A pinned rate wins over every other source, because
        // setFrameRate ignores the game's own `Graphics.frame_rate`
        // while the pin is in place. Test this branch first: when
        // syncToRefreshrate came first, the pin never reached the
        // limiter and the game ran at the display refresh rate
        // instead - up to 3x too fast on a 120Hz phone.
        p->fpsLimiter.setDesiredFPS(data->config.fixedFramerate);
    } else if (data->config.fixedFramerate < 0) {
        p->fpsLimiter.disabled = true;
    } else if (data->config.syncToRefreshrate) {
        // syncToRefreshrate caps DRAW timing at the GL layer
        // (Metal/EGL waits for the display vsync before swap).
        // Upstream mkxp-z used to ALSO disable the software FPS
        // limiter here, on the theory that vsync alone provided
        // pacing.
        //
        // That doesn't work for RGSS games. RGSS couples logic
        // ticks to draw calls (each Graphics.update both advances
        // game state and presents a frame), so capping draws at
        // the display refresh also caps LOGIC ticks at that rate.
        // A 40-fps RGSS1 game running on a 60Hz iOS device ticks
        // logic 60 times/sec → game runs 1.5x too fast. On 120Hz
        // ProMotion → 3x. On the iOS simulator (which doesn't
        // enforce vsync at all) → unbounded.
        //
        // Keep the limiter active so it caps logic ticks at the
        // game's `Graphics.frame_rate`. The GL-layer vsync still
        // does its job (tear-free presentation) but no longer
        // dictates game speed. Initial target is the display
        // refresh - the game's first `Graphics.frame_rate = N`
        // call updates it to the right value via setFrameRate.
        p->storeFrameRate(data->refreshRate);
        p->fpsLimiter.setDesiredFPS(data->refreshRate);
    }
}

Graphics::~Graphics() { delete p; }

double Graphics::getDelta() {
    return p->last_delta * 1000000.0;
}

double Graphics::lastUpdate() {
    return p->last_update;
}

void Graphics::update(bool checkForShutdown) {
    if (mkxp_isGLContextBroken()) {
        mkxp_setErrorMessage(
            "The graphics context crashed. Close the app from the app switcher and reopen it.");
        shState->checkShutdown();
        return;
    }
    p->markFrameTime(FrameTimes::Scripts);
    p->threadData->rqWindowAdjust.wait();
    double now = shState->runTime();
    if (p->last_update > 0) {
        p->last_delta = now - p->last_update;
    } else if (p->frameRate > 0) {
        p->last_delta = 1.0 / p->frameRate;
    } else {
        p->last_delta = 0;
    }
    p->last_update = now;
    
    // update Input.repeat timing, rounding the framerate to the nearest 2
    {
        static const double mult = 2.0;
        // A slow step, such as a map load, stays in the average for 40
        // frames. The repeat delay then gets so short that one press
        // moves a menu cursor two steps. The limiter keeps the updates
        // at its own rate, so use that rate while it runs.
        double afr = p->fpsLimiter.disabled ? std::abs(averageFrameRate())
                                            : 1.0 / p->fpsLimiter.frameSeconds();
        afr += mult / 2;
        afr -= std::fmod(afr, mult);
        // Input.update runs once per Graphics.update, and fast-forward
        // shows only one update in n. The rate of updates is what turns
        // the repeat delay into a count of updates.
        shState->input().recalcRepeat(std::floor(afr), mkxp_getFastForwardMultiplier());
    }
    
    if (checkForShutdown)
        p->checkShutDownReset();
    
    p->checkSyncLock();

    if (p->fpsLimiter.frameSkipRequired()) {
        if (p->useFrameSkip) {
            /* Skip frame */
            p->delayFrame();
            p->frameTimeDone(false);
            ++p->frameCount;
            p->threadData->ethread->notifyFrame();
            
            return;
        } else {
            /* Just reset frame adjust counter */
            p->fpsLimiter.resetFrameAdjust();
        }
    }

    // Runtime fast-forward (host bridge): when the multiplier is > 1
    // we drop the composite + swap path for N-1 of every N
    // Graphics.update calls. The Ruby script's update tick still
    // runs each call (because update() returns after the early-out
    // below), so logic advances Nx per visible frame while the
    // screen refreshes at the display's native rate.
    //
    // Skipping redrawScreen here also skips the limiter and the wait
    // for the Metal drawable in eglSwapBuffers.
    {
        int multiplier = mkxp_getFastForwardMultiplier();
        if (multiplier > 1) {
            bool render = (p->presentSkipCounter % multiplier) == 0;
            p->presentSkipCounter++;
            if (!render) {
                p->markFrameTime(FrameTimes::Draw);
                p->frameTimeDone(false);
                ++p->frameCount;
                p->threadData->ethread->notifyFrame();
                /* This branch skips the draw and the swap, so there is
                 * no new frame to keep. */
                p->checkPause(p->screen.getPP().frontBuffer());
                return;
            }
        } else {
            p->presentSkipCounter = 0;
        }
    }

    if (p->frozen) {
        // RGSS scripts still call Graphics.update while the scene is frozen,
        // especially for Ruby-driven transitions that animate over a frozen
        // snapshot. Returning immediately here makes Graphics.delta_s collapse
        // toward zero and those timers never advance on iOS.
        p->blitFrozenSceneToScreen();
        return;
    }

    p->checkResize();
    p->redrawScreen();

    p->framePresented(p->screen.getPP().frontBuffer());
}

void Graphics::freeze() {
    p->markFrameTime(FrameTimes::Scripts);
    p->frozen = true;
    
    p->checkShutDownReset();
    p->checkResize();
    
    /* Capture scene into frozen buffer */
    p->compositeToBuffer(p->frozenScene);
    p->markFrameTime(FrameTimes::Draw);
}

void Graphics::transition(int duration, const char *filename, int vague) {
    p->markFrameTime(FrameTimes::Scripts);
    p->checkSyncLock();
    
    if (!p->frozen)
        return;
    
    vague = clamp(vague, 1, 256);
    Bitmap *transMap = *filename ? new Bitmap(filename) : 0;
    
    setBrightness(255);
    
    /* Capture new scene */
    p->screen.composite();
    
    /* The PP frontbuffer will hold the current scene after the
     * composition step. Since the backbuffer is unused during
     * the transition, we can reuse it as the target buffer for
     * the final rendered image. */
    TEXFBO &currentScene = p->screen.getPP().frontBuffer();
    TEXFBO &transBuffer = p->screen.getPP().backBuffer();
    
    /* If no transition bitmap is provided,
     * we can use a simplified shader */
    TransShader &transShader = shState->shaders().trans;
    SimpleTransShader &simpleShader = shState->shaders().simpleTrans;
    
    // Handle high-res.
    Vec2i transSize(p->scResLores.x, p->scResLores.y);

    if (transMap) {
        TransShader &shader = transShader;
        shader.bind();
        shader.applyViewportProj();
        shader.setFrozenScene(p->frozenScene.tex);
        shader.setCurrentScene(currentScene.tex);
        if (transMap->hasHires()) {
            Debug() << "BUG: High-res Graphics transMap not implemented";
        }
        shader.setTransMap(transMap->getGLTypes().tex);
        shader.setVague(vague / 256.0f);
        shader.setTexSize(transSize);
    } else {
        SimpleTransShader &shader = simpleShader;
        shader.bind();
        shader.applyViewportProj();
        shader.setFrozenScene(p->frozenScene.tex);
        shader.setCurrentScene(currentScene.tex);
        shader.setTexSize(transSize);
    }
    
    glState.blend.pushSet(false);
    
    for (int i = 0; i < duration; ++i) {
        /* We need to clean up transMap properly before
         * a possible longjmp, so we manually test for
         * shutdown/reset here */
        if (p->threadData->rqTerm) {
            glState.blend.pop();
            delete transMap;
            p->shutdown();
            return;
        }
        
        if (p->threadData->rqReset) {
            glState.blend.pop();
            delete transMap;
            getActiveScriptBinding()->reset();
            return;
        }
        
        p->checkSyncLock();
        
        const float prog = i * (1.0f / duration);
        
        if (transMap) {
            transShader.bind();
            transShader.setProg(prog);
        } else {
            simpleShader.bind();
            simpleShader.setProg(prog);
        }
        
        /* Draw the composed frame to a buffer first
         * (we need this because we're skipping PingPong) */
        FBO::bind(transBuffer.fbo);
        FBO::clear();
        p->screenQuad.draw();
        
        p->checkResize();
        
        /* Then blit it flipped and scaled to the screen */
        FBO::unbind();
        FBO::clear();
        
        int scaleIsSpecial = GLMeta::blitScaleIsSpecial(p->integerScaleBuffer, false, IntRect(0, 0, p->scSize.x, p->scSize.y), transBuffer, IntRect(0, 0, p->scRes.x, p->scRes.y));

        GLMeta::blitBeginScreen(Vec2i(p->winSize), scaleIsSpecial);
        GLMeta::blitSource(transBuffer, scaleIsSpecial);
        p->metaBlitBufferFlippedScaled(scaleIsSpecial);
        GLMeta::blitEnd();
        
        p->swapGLBuffer();
        /* Call this manually, as redrawScreen() is not called during this loop. */
        p->updateAvgFPS();
        p->framePresented(transBuffer);
    }
    
    glState.blend.pop();
    
    delete transMap;
    
    p->frozen = false;
    p->markFrameTime(FrameTimes::Draw);
}

void Graphics::frameReset() {p->fpsLimiter.resetFrameAdjust();}

static void guardDisposed() {}

DEF_ATTR_RD_SIMPLE(Graphics, FrameRate, int, p->frameRate)

DEF_ATTR_SIMPLE(Graphics, FrameCount, int, p->frameCount)

void Graphics::setFrameRate(int value) {
    p->storeFrameRate(std::max(value, 1));

    // fixedFramerate>0 means the user / mkxp.json explicitly
    // pinned a target FPS; respect that and ignore the game's
    // own request.
    if (p->threadData->config.fixedFramerate > 0)
        return;

    // Always update the software FPS limiter to match the game's
    // requested rate, even when syncToRefreshrate is on (the
    // limiter caps LOGIC ticks; vsync still handles tear-free
    // DRAW presentation at the GL layer). See the rationale in
    // the Graphics ctor.
    p->fpsLimiter.setDesiredFPS(p->frameRate);
    //shState->input().recalcRepeat((unsigned int)p->frameRate);
}

double Graphics::averageFrameRate() {
    return p->averageFPS();
}

void Graphics::wait(int duration) {
    p->markFrameTime(FrameTimes::Scripts);
    for (int i = 0; i < duration; ++i) {
        p->checkShutDownReset();
        p->redrawScreen();
        p->framePresented(p->screen.getPP().frontBuffer());
    }
}

void Graphics::fadeout(int duration) {
    p->markFrameTime(FrameTimes::Scripts);
    FBO::unbind();
    
    float curr = p->brightness;
    float diff = 255.0f - curr;
    
    for (int i = duration - 1; i > -1; --i) {
        setBrightness(diff + (curr / duration) * i);
        
        if (p->frozen) {
            p->blitFrozenSceneToScreen();
        } else {
            update();
        }
    }
}

void Graphics::fadein(int duration) {
    p->markFrameTime(FrameTimes::Scripts);
    FBO::unbind();
    
    float curr = p->brightness;
    float diff = 255.0f - curr;
    
    for (int i = 1; i <= duration; ++i) {
        setBrightness(curr + (diff / duration) * i);
        
        if (p->frozen) {
            p->blitFrozenSceneToScreen();
        } else {
            update();
        }
    }
}

Bitmap *Graphics::snapToBitmap() {
    p->screen.composite();

    if (shState->config().enableHires) {
        // TODO: Maybe don't reconstruct this struct every time?
        TEXFBO tf;
        tf.width = width();
        tf.height = height();
        tf.selfHires = &p->screen.getPP().frontBuffer();

        return new Bitmap(tf);
    }

    return new Bitmap(p->screen.getPP().frontBuffer());
}

int Graphics::width() const { return p->scResLores.x; }

int Graphics::height() const { return p->scResLores.y; }

int Graphics::widthHires() const { return p->scRes.x; }

int Graphics::heightHires() const { return p->scRes.y; }

bool Graphics::isPingPongFramebufferActive() const {
    return p->screen.getPP().frontBuffer().fbo == FBO::boundFramebufferID || p->screen.getPP().backBuffer().fbo == FBO::boundFramebufferID;
}

int Graphics::displayContentWidth() const {
    return p->scSize.x;
}

int Graphics::displayContentHeight() const {
    return p->scSize.y;
}

int Graphics::displayWidth() const {
    SDL_DisplayMode dm{};
    SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(shState->sdlWindow()), &dm);
    return dm.w / p->backingScaleFactor;
}

int Graphics::displayHeight() const {
    SDL_DisplayMode dm{};
    SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(shState->sdlWindow()), &dm);
    return dm.h / p->backingScaleFactor;
}

void Graphics::resizeScreen(int width, int height) {
    p->threadData->rqWindowAdjust.wait();
    p->checkResize(true);
    
    Vec2i sizeLores(width, height);

    if (shState->config().enableHires) {
        double framebufferScalingFactor = shState->config().framebufferScalingFactor;
        width = (int)lround(framebufferScalingFactor * width);
        height = (int)lround(framebufferScalingFactor * height);
    }

    Vec2i size(width, height);
    
    if (p->scRes == size && p->scResLores == sizeLores)
        return;
    
    p->scRes = size;
    p->scResLores = sizeLores;
    
    p->screen.setResolution(width, height);
    
    if (p->integerScaleActive)
        p->rebuildIntegerScaleBuffer();
    
    TEXFBO::allocEmpty(p->frozenScene, width, height);
    
    FloatRect screenRect(0, 0, width, height);
    p->screenQuad.setTexPosRect(screenRect, screenRect);
    
    glState.scissorBox.set(IntRect(0, 0, p->scRes.x, p->scRes.y));
    
    /* Trigger a size recalculation so the viewport is updated. */
    p->recalculateScreenSize(shState->config().fixedAspectRatio);
    p->updateScreenResoRatio(p->threadData);
    SDL_Rect screen = {p->scOffset.x, p->scOffset.y, p->scSize.x, p->scSize.y};
    p->threadData->ethread->notifyGameScreenChange(screen);
}

void Graphics::resizeWindow(int width, int height, bool center) {
    /* On iOS the window is always fullscreen — resizing is meaningless. */
    (void)width; (void)height; (void)center;
    return;
}

bool Graphics::updateMovieInput(Movie *movie) {
    return  p->threadData->rqTerm || p->threadData->rqReset;
}

void Graphics::playMovie(const char *filename, int volume_, bool skippable) {
    if (shState->config().enableHires) {
        Debug() << "BUG: High-res Graphics playMovie not implemented";
    }

    // Fast-reject formats TheoraPlay cannot decode (AVI, MP4, etc.).
    // RMXP games commonly ship .avi intros that rely on a Windows-only
    // plugin DLL (rubyscreen.dll). Without an extension gate the
    // decoder thread spins forever on an unparseable file, which then
    // causes the Ruby thread to hang in preparePlayback()'s init loop
    // and the user gets a black screen they cannot escape.
    {
        const char *dot = filename ? strrchr(filename, '.') : nullptr;
        bool supported = false;
        if (dot) {
            // Accept anything Ogg-based. Matches TheoraPlay's capabilities.
            static const char *ok[] = {".ogv", ".ogg", ".ogm", nullptr};
            for (int i = 0; ok[i]; ++i) {
                if (strcasecmp(dot, ok[i]) == 0) { supported = true; break; }
            }
        }
        if (!supported) {
            char buf[600];
            snprintf(buf, sizeof(buf), "skipping unsupported format: %s",
                     filename ? filename : "(null)");
            mkxp_debugLog("MOVIE", "graphics.cpp [C++]", buf);
            return;
        }
    }

    Movie *movie = new Movie(skippable);
    MovieOpenHandler handler(movie->srcOps);
    shState->fileSystem().openRead(handler, filename);
    float volume = volume_ * 0.01f;
    
    if (movie->preparePlayback()) {        
        Sprite movieSprite;
        
        // Currently this stretches to fit the screen. VX Ace behavior is to center it and let the edges run off
        movieSprite.setBitmap(movie->videoBitmap);
        double ratio = std::min((double)width() / movie->video->width, (double)height() / movie->video->height);
        movieSprite.setZoomX(ratio);
        movieSprite.setZoomY(ratio);
        movieSprite.setX((width() / 2) - (movie->video->width * ratio / 2));
        movieSprite.setY((height() / 2) - (movie->video->height * ratio / 2));
        
        Sprite letterboxSprite;
        Bitmap letterbox(width(), height());
        letterbox.fillRect(0, 0, width(), height(), Vec4(0,0,0,255));
        letterboxSprite.setBitmap(&letterbox);
        
        letterboxSprite.setZ(4999);
        movieSprite.setZ(5001);
        
        movie->play(volume);
    }
    
    delete movie;
}

void Graphics::screenshot(const char *filename) {
    p->threadData->rqWindowAdjust.wait();
    Bitmap *ss = snapToBitmap();
    ss->saveToFile(filename);
    ss->dispose();
    delete ss;
}

DEF_ATTR_RD_SIMPLE(Graphics, Brightness, int, p->brightness)

void Graphics::setBrightness(int value) {
    value = clamp(value, 0, 255);
    
    if (p->brightness == value)
        return;
    
    p->brightness = value;
    p->screen.setBrightness(value / 255.0);
}

void Graphics::reset() {
    /* Dispose all live Disposables and mark them detached.
     * On iOS, Ruby GC may free these objects in a later session —
     * ~Disposable must skip remDisposable for detached objects. */
    IntruListLink<Disposable> *iter;
    
    for (iter = p->dispList.begin(); iter != p->dispList.end(); ) {
        IntruListLink<Disposable> *next = iter->next;
        iter->data->dispose();
        iter->data->detached = true;
        iter->prev = 0;
        iter->next = 0;
        iter = next;
    }
    
    p->dispList.clear();
    
    /* Reset attributes (frame count not included) */
    p->fpsLimiter.resetFrameAdjust();
    p->frozen = false;
    p->screen.getPP().clearBuffers();
    
    setFrameRate(DEF_FRAMERATE);
    setBrightness(255);
    
    // Always update at least once to clear the screen
    if (p->threadData->rqResetFinish)
        update();
    else
        repaintWait(p->threadData->rqResetFinish, false);
    p->threadData->rqReset.clear();
}

void Graphics::center() {
    p->threadData->rqWindowAdjust.wait();
    if (getFullscreen())
        return;
    
    p->threadData->ethread->requestWindowCenter();
}

bool Graphics::getFullscreen() const {
    return p->threadData->ethread->getFullscreen();
}

void Graphics::setFullscreen(bool value) {
    p->threadData->ethread->requestFullscreenMode(value);
}

bool Graphics::getShowCursor() const {
    return p->threadData->ethread->getShowCursor();
}

void Graphics::setShowCursor(bool value) {
    p->threadData->ethread->requestShowCursor(value);
}

bool Graphics::getFixedAspectRatio() const
{
    // It's a bit hacky to expose config values as a Graphics
    // attribute, but there's really no point in state duplication
    return shState->config().fixedAspectRatio;
}

void Graphics::setFixedAspectRatio(bool value)
{
    shState->config().fixedAspectRatio = value;
    p->recalculateScreenSize(p->threadData->config.fixedAspectRatio);
    p->findHighestIntegerScale();
    p->recalculateScreenSize(p->threadData->config.fixedAspectRatio);
    p->updateScreenResoRatio(p->threadData);
}

int Graphics::getSmoothScaling() const
{
    // Same deal as with fixed aspect ratio
    return shState->config().smoothScaling;
}

void Graphics::setSmoothScaling(int value)
{
    shState->config().smoothScaling = value;
}

bool Graphics::getIntegerScaling() const
{
    return p->integerScaleActive;
}

void Graphics::setIntegerScaling(bool value)
{
    p->integerScaleActive = value;
    p->findHighestIntegerScale();
    p->rebuildIntegerScaleBuffer();
    
    p->recalculateScreenSize(p->threadData->config.fixedAspectRatio);
    p->updateScreenResoRatio(p->threadData);
}

bool Graphics::getLastMileScaling() const
{
    return p->integerLastMileScaling;
}

void Graphics::setLastMileScaling(bool value)
{
    p->integerLastMileScaling = value;
    p->recalculateScreenSize(p->threadData->config.fixedAspectRatio);
    p->updateScreenResoRatio(p->threadData);
}

bool Graphics::getThreadsafe() const
{
    return p->multithreadedMode;
}

void Graphics::setThreadsafe(bool value)
{
    p->multithreadedMode = value;
}

double Graphics::getScale() const {
    p->checkResize();
    return (double)(p->winSize.y / p->backingScaleFactor) / p->scRes.y;
    
}

void Graphics::setScale(double factor) {
    p->threadData->rqWindowAdjust.wait();
    factor = clamp(factor, 0.5, 4.0);

    if (factor == getScale())
        return;

    int widthpx = p->scRes.x * factor;
    int heightpx = p->scRes.y * factor;

    shState->eThread().requestWindowResize(widthpx, heightpx);
}

bool Graphics::getFrameskip() const { return p->useFrameSkip; }

void Graphics::setFrameskip(bool value) { p->useFrameSkip = value; }

Scene *Graphics::getScreen() const { return &p->screen; }

void Graphics::repaintWait(const AtomicFlag &exitCond, bool checkReset) {
    if (exitCond)
        return;
    
    /* Repaint the screen with the last good frame we drew */
    TEXFBO &lastFrame = p->screen.getPP().frontBuffer();

    int scaleIsSpecial = GLMeta::blitScaleIsSpecial(p->integerScaleBuffer, false, IntRect(0, 0, p->scSize.x, p->scSize.y), lastFrame, IntRect(0, 0, p->scRes.x, p->scRes.y));

    GLMeta::blitBeginScreen(p->winSize, scaleIsSpecial);
    GLMeta::blitSource(lastFrame, scaleIsSpecial);
    
    while (!exitCond) {
        shState->checkShutdown();
        
        if (checkReset)
            shState->checkReset();
        
        FBO::clear();
        p->metaBlitBufferFlippedScaled(scaleIsSpecial);
        graphicsGL_SwapWindow(p->threadData->window);
        p->fpsLimiter.delay();
        
        p->threadData->ethread->notifyFrame();

        /* Only the pause check here. The blit scope this loop opened
         * stays open across every iteration, and keepPresentedFrame
         * ends a blit scope of its own, which would reset the
         * dimensions this loop still needs. Nothing to keep anyway:
         * the loop repaints the frame lastPresentedFrame already
         * holds. */
        p->checkPause(lastFrame);
    }
    
    GLMeta::blitEnd();
}

void Graphics::lock(bool force) {
    p->setLock(force);
}

void Graphics::unlock(bool force) {
    p->releaseLock(force);
}

void Graphics::addDisposable(Disposable *d) { p->dispList.append(d->link); }

void Graphics::remDisposable(Disposable *d) { p->dispList.remove(d->link); }

void Graphics::detachAllDisposables() {
    // Dispose GL resources while the current TexPool/GL context is still
    // valid. Without this, Ruby GC may later destruct a session-1 Bitmap
    // under session 2's GL context, releasing stale GL IDs into the new
    // TexPool and corrupting subsequent texture allocations.
    IntruListLink<Disposable> *iter = p->dispList.begin();
    IntruListLink<Disposable> *end = p->dispList.end();
    while (iter != end) {
        IntruListLink<Disposable> *next = iter->next;
        iter->data->dispose();
        iter->data->detached = true;
        iter->prev = 0;
        iter->next = 0;
        iter = next;
    }
    p->dispList.clear();
}

#undef GRAPHICS_THREAD_LOCK
