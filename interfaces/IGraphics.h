//
// Created by fcors on 10/21/2023.
//

#pragma once

#include "util.h"

#include <memory>

class Scene;

class Bitmap;

class Disposable;

struct RGSSThreadData;
struct GraphicsPrivate;
struct AtomicFlag;
struct THEORAPLAY_VideoFrame;
struct Movie;

class IGraphics {

public:
    virtual ~IGraphics() = default;

    virtual double getDelta() = 0;

    virtual double lastUpdate() = 0;

    virtual void update(bool checkForShutdown = true) = 0;

    virtual void freeze() = 0;

    virtual void transition(int duration = 8,
                            const char *filename = "",
                            int vague = 40) = 0;

    virtual void frameReset() = 0;

    DECL_ATTR_PURE(FrameRate, int)

    DECL_ATTR_PURE(FrameCount, int)

    DECL_ATTR_PURE(Brightness, int)

    virtual void wait(int duration) = 0;

    virtual void fadeout(int duration) = 0;

    virtual void fadein(int duration) = 0;

    virtual Bitmap *snapToBitmap() = 0;

    virtual int width() const = 0;

    virtual int height() const = 0;

    virtual int displayWidth() const = 0;

    virtual int displayHeight() const = 0;

    virtual void resizeScreen(int width, int height) = 0;

    virtual void resizeWindow(int width, int height, bool center = false) = 0;

    virtual void drawMovieFrame(const THEORAPLAY_VideoFrame *video, Bitmap *videoBitmap) = 0;

    virtual bool updateMovieInput(Movie *movie) = 0;

    virtual void playMovie(const char *filename, int volume, bool skippable) = 0;

    virtual void screenshot(const char *filename) = 0;

    virtual void reset() = 0;

    virtual void center() = 0;

    /* Non-standard extension */
    DECL_ATTR_PURE(Fullscreen, bool)

    DECL_ATTR_PURE(ShowCursor, bool)

    DECL_ATTR_PURE(Scale, double)

    DECL_ATTR_PURE(Frameskip, bool)

    DECL_ATTR_PURE(FixedAspectRatio, bool)

    DECL_ATTR_PURE(SmoothScaling, bool)

    DECL_ATTR_PURE(IntegerScaling, bool)

    DECL_ATTR_PURE(LastMileScaling, bool)

    DECL_ATTR_PURE(Threadsafe, bool)

    virtual double averageFrameRate() = 0;

    /* <internal> */
    virtual std::shared_ptr<Scene> getScreen() const = 0;

    /* Repaint screen with static image until exitCond
     * is set. Observes reset flag on top of shutdown
     * if "checkReset" */
    virtual void repaintWait(const AtomicFlag &exitCond,
                             bool checkReset = true) = 0;

    virtual void lock(bool force = false) = 0;

    virtual void unlock(bool force = false) = 0;

protected:
    virtual void addDisposable(Disposable *) = 0;

    virtual void remDisposable(Disposable *) = 0;

    friend class Disposable;
};
