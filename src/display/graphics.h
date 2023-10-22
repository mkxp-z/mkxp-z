/*
** graphics.h
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

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "IGraphics.h"
#include "DisplayManager.h"

#include <memory>

class Graphics : public IGraphics {
public:
    double getDelta() override;

    double lastUpdate() override;

    void update(bool checkForShutdown = true) override;

    void freeze() override;

    void transition(int duration = 8,
                    const char *filename = "",
                    int vague = 40) override;

    void frameReset() override;

    DECL_ATTR_OVRD(FrameRate, int)

    DECL_ATTR_OVRD(FrameCount, int)

    DECL_ATTR_OVRD(Brightness, int)

    void wait(int duration) override;

    void fadeout(int duration) override;

    void fadein(int duration) override;

    Bitmap *snapToBitmap() override;

    int width() const override;

    int height() const override;

    int displayWidth() const override;

    int displayHeight() const override;

    void resizeScreen(int width, int height) override;

    void resizeWindow(int width, int height, bool center = false) override;

    void drawMovieFrame(const THEORAPLAY_VideoFrame *video, Bitmap *videoBitmap) override;

    bool updateMovieInput(Movie *movie) override;

    void playMovie(const char *filename, int volume, bool skippable) override;

    void screenshot(const char *filename) override;

    void reset() override;

    void center() override;

    /* Non-standard extension */
    DECL_ATTR_OVRD(Fullscreen, bool)

    DECL_ATTR_OVRD(ShowCursor, bool)

    DECL_ATTR_OVRD(Scale, double)

    DECL_ATTR_OVRD(Frameskip, bool)

    DECL_ATTR_OVRD(FixedAspectRatio, bool)

    DECL_ATTR_OVRD(SmoothScaling, bool)

    DECL_ATTR_OVRD(IntegerScaling, bool)

    DECL_ATTR_OVRD(LastMileScaling, bool)

    DECL_ATTR_OVRD(Threadsafe, bool)

    double averageFrameRate() override;

    /* <internal> */
    std::shared_ptr<Scene> getScreen() const override;

    /* Repaint screen with static image until exitCond
     * is set. Observes reset flag on top of shutdown
     * if "checkReset" */
    void repaintWait(const AtomicFlag &exitCond,
                     bool checkReset = true) override;

    void lock(bool force = false) override;

    void unlock(bool force = false) override;

private:
    explicit Graphics(RGSSThreadData *data);

    ~Graphics() override;

protected:
    void addDisposable(Disposable *) override;

    void remDisposable(Disposable *) override;

private:
    friend struct SharedStatePrivate;

    friend class Disposable;

    friend class DisplayManager;

    friend std::unique_ptr<Graphics>::deleter_type;

    std::unique_ptr<GraphicsPrivate> p;
};

#define GFX_LOCK shState->graphics().lock()
#define GFX_UNLOCK shState->graphics().unlock()

#endif // GRAPHICS_H
