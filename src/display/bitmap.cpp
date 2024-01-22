/*
 ** bitmap.cpp
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

#include "bitmap.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_rect.h>
#include <SDL_surface.h>

#include <pixman.h>

#include "gl-util.h"
#include "gl-meta.h"
#include "quad.h"
#include "quadarray.h"
#include "transform.h"
#include "exception.h"

#include "sharedstate.h"
#include "glstate.h"
#include "texpool.h"
#include "shader.h"
#include "filesystem.h"
#include "font.h"
#include "eventthread.h"
#include "graphics.h"
#include "system.h"
#include "util/util.h"

#include "debugwriter.h"

#include "sigslot/signal.hpp"

#include <math.h>
#include <algorithm>

extern "C" {
#include "libnsgif/libnsgif.h"
}

#define GUARD_MEGA \
{ \
if (p->megaSurface) \
throw Exception(Exception::MKXPError, \
"Operation not supported for mega surfaces"); \
}

#define GUARD_ANIMATED \
{ \
if (p->animation.enabled) \
throw Exception(Exception::MKXPError, \
"Operation not supported for animated bitmaps"); \
}

#define GUARD_UNANIMATED \
{ \
if (!p->animation.enabled) \
throw Exception(Exception::MKXPError, \
"Operation not supported for static bitmaps"); \
}

#define OUTLINE_SIZE 1

#ifndef INT16_MAX
#define INT16_MAX 32767
#endif

/* Normalize (= ensure width and
 * height are positive) */
static IntRect normalizedRect(const IntRect &rect)
{
    IntRect norm = rect;
    
    if (norm.w < 0)
    {
        norm.w = -norm.w;
        norm.x -= norm.w;
    }
    
    if (norm.h < 0)
    {
        norm.h = -norm.h;
        norm.y -= norm.h;
    }
    
    return norm;
}


// libnsgif loading callbacks, taken pretty much straight from their tests

static void *gif_bitmap_create(int width, int height)
{
    /* ensure a stupidly large bitmap is not created */
    return calloc(width * height, 4);
}


static void gif_bitmap_set_opaque(void *bitmap, bool opaque)
{
    (void) opaque;  /* unused */
    (void) bitmap;  /* unused */
    assert(bitmap);
}


static bool gif_bitmap_test_opaque(void *bitmap)
{
    (void) bitmap;  /* unused */
    assert(bitmap);
    return false;
}


static unsigned char *gif_bitmap_get_buffer(void *bitmap)
{
    assert(bitmap);
    return (unsigned char *)bitmap;
}


static void gif_bitmap_destroy(void *bitmap)
{
    assert(bitmap);
    free(bitmap);
}


static void gif_bitmap_modified(void *bitmap)
{
    (void) bitmap;  /* unused */
    assert(bitmap);
    return;
}

// --------------------

struct BitmapPrivate
{
    Bitmap *self;
    
    struct {
        int width;
        int height;
        
        bool enabled;
        bool playing;
        bool needsReset;
        bool loop;
        std::vector<TEXFBO> frames;
        float fps;
        int lastFrame;
        double startTime, playTime;
        
        inline unsigned int currentFrameIRaw() {
            if (fps <= 0) return lastFrame;
            return floor(lastFrame + (playTime / (1 / fps)));
        }
        
        unsigned int currentFrameI() {
            if (!playing || needsReset) return lastFrame;
            int i = currentFrameIRaw();
            return (loop) ? fmod(i, frames.size()) : (i > (int)frames.size() - 1) ? (int)frames.size() - 1 : i;
        }
        
        inline TEXFBO &currentFrame() {
            int i = currentFrameI();
            return frames[i];
        }
        
        inline void play() {
            playing = true;
            needsReset = true;
        }
        
        inline void stop() {
            lastFrame = currentFrameI();
            playing = false;
        }
        
        inline void seek(int frame) {
            lastFrame = clamp(frame, 0, (int)frames.size());
        }
        
        void updateTimer() {
            if (needsReset) {
                lastFrame = currentFrameI();
                playTime = 0;
                startTime = shState->runTime();
                needsReset = false;
                return;
            }
            
            playTime = shState->runTime() - startTime;
            return;
        }
    } animation;
    
    sigslot::connection prepareCon;
    
    TEXFBO gl;
    
    Font *font;
    
    /* "Mega surfaces" are a hack to allow Tilesets to be used
     * whose Bitmaps don't fit into a regular texture. They're
     * kept in RAM and will throw an error if they're used in
     * any context other than as Tilesets */
    SDL_Surface *megaSurface;
    
    /* A cached version of the bitmap in client memory, for
     * getPixel calls. Is invalidated any time the bitmap
     * is modified */
    SDL_Surface *surface;
    SDL_PixelFormat *format;
    
    /* The 'tainted' area describes which parts of the
     * bitmap are not cleared, ie. don't have 0 opacity.
     * If we're blitting / drawing text to a cleared part
     * with full opacity, we can disregard any old contents
     * in the texture and blit to it directly, saving
     * ourselves the expensive blending calculation. */
     
    /* pixman_region16_t supports bitmaps whose largest
     * dimension is no more than 32767 pixels.
     * Be certain to set pixmanUseRegion32 in the
     * constructor for larger bitmaps. */
    pixman_region16_t tainted;
    pixman_region32_t tainted32;
    bool pixmanUseRegion32;

    // For high-resolution texture replacement.
    Bitmap *selfHires;
    Bitmap *selfLores;
    bool assumingRubyGC;
    
    // Child bitmaps are created by Planes, Sprites, and Windows for mega surfaces
    ChildPrivate *pChild;
    
    BitmapPrivate(Bitmap *self)
    : self(self),
    megaSurface(0),
    selfHires(0),
    selfLores(0),
    surface(0),
    pChild(0),
    assumingRubyGC(false),
    pixmanUseRegion32(false)
    {
        format = SDL_AllocFormat(SDL_PIXELFORMAT_ABGR8888);
        
        animation.width = 0;
        animation.height = 0;
        animation.enabled = false;
        animation.playing = false;
        animation.loop = true;
        animation.playTime = 0;
        animation.startTime = 0;
        animation.fps = 0;
        animation.lastFrame = 0;
        
        prepareCon = shState->prepareDraw.connect(&BitmapPrivate::prepare, this);
        
        font = &shState->defaultFont();
        pixman_region_init(&tainted);
    }
    
    ~BitmapPrivate()
    {
        prepareCon.disconnect();
        SDL_FreeFormat(format);
        if (pixmanUseRegion32)
            pixman_region32_fini(&tainted32);
        else
            pixman_region_fini(&tainted);
    }
    
    TEXFBO &getGLTypes() {
        return (animation.enabled) ? animation.currentFrame() : gl;
    }
    
    void prepare()
    {
        if (!animation.enabled || !animation.playing) return;
        
        animation.updateTimer();
    }
    
    void allocSurface()
    {
        surface = SDL_CreateRGBSurface(0, getGLTypes().width, getGLTypes().height, format->BitsPerPixel,
                                       format->Rmask, format->Gmask,
                                       format->Bmask, format->Amask);
    }
    
    void clearTaintedArea()
    {
        if( pixmanUseRegion32)
        {
            pixman_region32_fini(&tainted32);
            pixman_region32_init(&tainted32);
        }
        else
        {
            pixman_region_fini(&tainted);
            pixman_region_init(&tainted);
        }
    }
    
    void addTaintedArea(const IntRect &rect)
    {
        IntRect norm = normalizedRect(rect);
        if (pixmanUseRegion32)
        {
            pixman_region32_union_rect
            (&tainted32, &tainted32, norm.x, norm.y, norm.w, norm.h);
        }
        else
        {
            pixman_region_union_rect
            (&tainted, &tainted, norm.x, norm.y, norm.w, norm.h);
        }
    }
    
    void substractTaintedArea(const IntRect &rect)
    {
        if (!touchesTaintedArea(rect))
            return;
        
        if (pixmanUseRegion32)
        {
            pixman_region32_t m_reg;
            pixman_region32_init_rect(&m_reg, rect.x, rect.y, rect.w, rect.h);
            
            pixman_region32_subtract(&tainted32, &m_reg, &tainted32);
            
            pixman_region32_fini(&m_reg);
        }
        else
        {
            pixman_region16_t m_reg;
            pixman_region_init_rect(&m_reg, rect.x, rect.y, rect.w, rect.h);
            
            pixman_region_subtract(&tainted, &m_reg, &tainted);
            
            pixman_region_fini(&m_reg);
        }
    }
    
    bool touchesTaintedArea(const IntRect &rect)
    {
        pixman_region_overlap_t result;
        if (pixmanUseRegion32)
        {
            pixman_box32_t box;
            box.x1 = rect.x;
            box.y1 = rect.y;
            box.x2 = rect.x + rect.w;
            box.y2 = rect.y + rect.h;
            
            result = pixman_region32_contains_rectangle(&tainted32, &box);
        }
        else
        {
            pixman_box16_t box;
            box.x1 = rect.x;
            box.y1 = rect.y;
            box.x2 = rect.x + rect.w;
            box.y2 = rect.y + rect.h;
            
            result = pixman_region_contains_rectangle(&tainted, &box);
        }
        
        return result != PIXMAN_REGION_OUT;
    }
    
    void bindTexture(ShaderBase &shader, bool substituteLoresSize = true)
    {
        if (selfHires) {
            selfHires->bindTex(shader);
            return;
        }

        if (animation.enabled) {
            if (selfLores) {
                Debug() << "BUG: High-res BitmapPrivate bindTexture for animations not implemented";
            }

            TEXFBO cframe = animation.currentFrame();
            TEX::bind(cframe.tex);
            shader.setTexSize(Vec2i(cframe.width, cframe.height));
            return;
        }
        TEX::bind(gl.tex);
        if (selfLores && substituteLoresSize) {
            shader.setTexSize(Vec2i(selfLores->width(), selfLores->height()));
        }
        else {
            shader.setTexSize(Vec2i(gl.width, gl.height));
        }
    }
    
    void bindFBO()
    {
        FBO::bind((animation.enabled) ? animation.currentFrame().fbo : gl.fbo);
    }
    
    void pushSetViewport(ShaderBase &shader) const
    {
        glState.viewport.pushSet(IntRect(0, 0, gl.width, gl.height));
        shader.applyViewportProj();
    }
    
    void popViewport() const
    {
        glState.viewport.pop();
    }
    
    void blitQuad(Quad &quad)
    {
        glState.blend.pushSet(false);
        quad.draw();
        glState.blend.pop();
    }
    
    void fillRect(const IntRect &rect,
                  const Vec4 &color)
    {
        if (megaSurface)
        {
            uint8_t r, g, b, a;
            r = clamp<float>(color.x, 0, 1) * 255.0f;
            g = clamp<float>(color.y, 0, 1) * 255.0f;
            b = clamp<float>(color.z, 0, 1) * 255.0f;
            a = clamp<float>(color.w, 0, 1) * 255.0f;
            SDL_FillRect(megaSurface, &rect, SDL_MapRGBA(format, r, g, b, a));
        }
        else
        {
            bindFBO();
            
            glState.scissorTest.pushSet(true);
            glState.scissorBox.pushSet(normalizedRect(rect));
            glState.clearColor.pushSet(color);
            
            FBO::clear();
            
            glState.clearColor.pop();
            glState.scissorBox.pop();
            glState.scissorTest.pop();
        }
    }
    
    static void ensureFormat(SDL_Surface *&surf, Uint32 format)
    {
        if (surf->format->format == format)
            return;
        
        SDL_Surface *surfConv = SDL_ConvertSurfaceFormat(surf, format, 0);
        SDL_FreeSurface(surf);
        surf = surfConv;
    }
    
    void onModified(bool freeSurface = true)
    {
        if (surface && freeSurface)
        {
            SDL_FreeSurface(surface);
            surface = 0;
        }
        
        self->modified();
    }
};

struct BitmapOpenHandler : FileSystem::OpenHandler
{
    // Non-GIF
    SDL_Surface *surface;
    
    // GIF
    std::string error;
    gif_animation *gif;
    unsigned char *gif_data;
    size_t gif_data_size;
    
    
    BitmapOpenHandler()
    : surface(0), gif(0), gif_data(0), gif_data_size(0)
    {}
    
    bool tryRead(SDL_RWops &ops, const char *ext)
    {
        if (IMG_isGIF(&ops)) {
            // Use libnsgif to initialise the gif data
            gif = new gif_animation;
            
            gif_bitmap_callback_vt gif_bitmap_callbacks = {
                gif_bitmap_create,
                gif_bitmap_destroy,
                gif_bitmap_get_buffer,
                gif_bitmap_set_opaque,
                gif_bitmap_test_opaque,
                gif_bitmap_modified
            };
            
            gif_create(gif, &gif_bitmap_callbacks);
            
            gif_data_size = ops.size(&ops);
            
            gif_data = new unsigned char[gif_data_size];
            ops.seek(&ops, 0, RW_SEEK_SET);
            ops.read(&ops, gif_data, gif_data_size, 1);
            
            int status;
            do {
                status = gif_initialise(gif, gif_data_size, gif_data);
                if (status != GIF_OK && status != GIF_WORKING) {
                    gif_finalise(gif);
                    delete gif;
                    delete gif_data;
                    error = "Failed to initialize GIF (Error " + std::to_string(status) + ")";
                    return false;
                }
            } while (status != GIF_OK);
            
            // Decode the first frame
            status = gif_decode_frame(gif, 0);
            if (status != GIF_OK && status != GIF_WORKING) {
                error = "Failed to decode first GIF frame. (Error " + std::to_string(status) + ")";
                gif_finalise(gif);
                delete gif;
                delete gif_data;
                return false;
            }
        } else {
            surface = IMG_LoadTyped_RW(&ops, 1, ext);
        }
        return (surface || gif);
    }
};

Bitmap::Bitmap(const char *filename)
{
    std::string hiresPrefix = "Hires/";
    std::string filenameStd = filename;
    Bitmap *hiresBitmap = nullptr;
    // TODO: once C++20 is required, switch to filenameStd.starts_with(hiresPrefix)
    if (shState->config().enableHires && filenameStd.compare(0, hiresPrefix.size(), hiresPrefix) != 0) {
        // Look for a high-res version of the file.
        std::string hiresFilename = hiresPrefix + filenameStd;
        try {
            hiresBitmap = new Bitmap(hiresFilename.c_str());
            hiresBitmap->setLores(this);
        }
        catch (const Exception &e)
        {
            Debug() << "No high-res Bitmap found at" << hiresFilename;
            hiresBitmap = nullptr;
        }
    }

    BitmapOpenHandler handler;
    shState->fileSystem().openRead(handler, filename);
    
    if (!handler.error.empty()) {
        // Not loaded with SDL, but I want it to be caught with the same exception type
        throw Exception(Exception::SDLError, "Error loading image '%s': %s", filename, handler.error.c_str());
    }
    else if (!handler.gif && !handler.surface) {
        throw Exception(Exception::SDLError, "Error loading image '%s': %s",
                        filename, SDL_GetError());
    }
    
    if (handler.gif) {
        p = new BitmapPrivate(this);
        if (handler.gif->width > INT16_MAX || handler.gif->height > INT16_MAX)
        {
            p->pixmanUseRegion32 = true;
            pixman_region_fini(&p->tainted);
            pixman_region32_init(&p->tainted32);
        }

        p->selfHires = hiresBitmap;
        
        if (handler.gif->width >= (uint32_t)glState.caps.maxTexSize || handler.gif->height > (uint32_t)glState.caps.maxTexSize)
        {
            throw new Exception(Exception::MKXPError, "Animation too large (%ix%i, max %ix%i)",
                                handler.gif->width, handler.gif->height, glState.caps.maxTexSize, glState.caps.maxTexSize);
        }
        
        if (handler.gif->frame_count == 1) {
            TEXFBO texfbo;
            try {
                texfbo = shState->texPool().request(handler.gif->width, handler.gif->height);
            }
            catch (const Exception &e)
            {
                gif_finalise(handler.gif);
                delete handler.gif;
                delete handler.gif_data;
                
                throw e;
            }
            
            TEX::bind(texfbo.tex);
            TEX::uploadImage(handler.gif->width, handler.gif->height, handler.gif->frame_image, GL_RGBA);
            gif_finalise(handler.gif);
            delete handler.gif;
            delete handler.gif_data;
            
            p->gl = texfbo;
            if (p->selfHires != nullptr) {
                p->gl.selfHires = &p->selfHires->getGLTypes();
            }
            p->addTaintedArea(rect());
            return;
        }
        
        p->animation.enabled = true;
        p->animation.width = handler.gif->width;
        p->animation.height = handler.gif->height;
        
        // Guess framerate based on the first frame's delay
        p->animation.fps = 1 / ((float)handler.gif->frames[handler.gif->decoded_frame].frame_delay / 100);
        if (p->animation.fps < 0) p->animation.fps = shState->graphics().getFrameRate();
        
        // Loop gif (Either it's looping or it's not, at the moment)
        p->animation.loop = handler.gif->loop_count >= 0;
        
        int fcount = handler.gif->frame_count;
        int fcount_partial = handler.gif->frame_count_partial;
        if (fcount > fcount_partial) {
            Debug() << "Non-fatal error reading" << filename << ": Only decoded" << fcount_partial << "out of" << fcount << "frames";
        }
        for (int i = 0; i < fcount_partial; i++) {
            if (i > 0) {
                int status = gif_decode_frame(handler.gif, i);
                if (status != GIF_OK && status != GIF_WORKING) {
                    for (TEXFBO &frame : p->animation.frames)
                        shState->texPool().release(frame);
                    
                    gif_finalise(handler.gif);
                    delete handler.gif;
                    delete handler.gif_data;
                    
                    throw Exception(Exception::MKXPError, "Failed to decode GIF frame %i out of %i (Status %i)",
                                    i + 1, fcount_partial, status);
                }
            }
            
            TEXFBO texfbo;
            try {
                texfbo = shState->texPool().request(p->animation.width, p->animation.height);
            }
            catch (const Exception &e)
            {
                for (TEXFBO &frame : p->animation.frames)
                    shState->texPool().release(frame);
                
                gif_finalise(handler.gif);
                delete handler.gif;
                delete handler.gif_data;
                
                throw e;
            }
            
            TEX::bind(texfbo.tex);
            TEX::uploadImage(p->animation.width, p->animation.height, handler.gif->frame_image, GL_RGBA);
            p->animation.frames.push_back(texfbo);
        }
        
        gif_finalise(handler.gif);
        delete handler.gif;
        delete handler.gif_data;
        p->addTaintedArea(rect());
        return;
    }

    SDL_Surface *imgSurf = handler.surface;

    initFromSurface(imgSurf, hiresBitmap, hiresBitmap && hiresBitmap->isMega());
}

Bitmap::Bitmap(int width, int height, bool isHires)
{
    if (width <= 0 || height <= 0)
        throw Exception(Exception::RGSSError, "failed to create bitmap");
    
    Bitmap *hiresBitmap = nullptr;

    if (shState->config().enableHires && !isHires) {
        // Create a high-res version as well.
        double scalingFactor = shState->config().textureScalingFactor;
        int hiresWidth = (int)lround(scalingFactor * width);
        int hiresHeight = (int)lround(scalingFactor * height);
        hiresBitmap = new Bitmap(hiresWidth, hiresHeight, true);
        hiresBitmap->setLores(this);
    }

    if (width > glState.caps.maxTexSize || height > glState.caps.maxTexSize || (hiresBitmap && hiresBitmap->isMega()))
    {
        p = new BitmapPrivate(this);
        SDL_Surface *surface = SDL_CreateRGBSurface(0, width, height, p->format->BitsPerPixel,
                                                    p->format->Rmask,
                                                    p->format->Gmask,
                                                    p->format->Bmask,
                                                    p->format->Amask);
        if (!surface)
            throw Exception(Exception::SDLError, "Error creating Bitmap: %s",
                            SDL_GetError());
        p->megaSurface = surface;
        SDL_SetSurfaceBlendMode(p->megaSurface, SDL_BLENDMODE_NONE);
    }
    else
    {
        TEXFBO tex = shState->texPool().request(width, height);
        
        p = new BitmapPrivate(this);
        p->gl = tex;
        p->selfHires = hiresBitmap;
        if (p->selfHires != nullptr) {
            p->gl.selfHires = &p->selfHires->getGLTypes();
        }
    }
    
    if (width > INT16_MAX || height > INT16_MAX)
    {
        p->pixmanUseRegion32 = true;
        pixman_region_fini(&p->tainted);
        pixman_region32_init(&p->tainted32);
    }
    clear();
}

Bitmap::Bitmap(void *pixeldata, int width, int height)
{
    SDL_Surface *surface = SDL_CreateRGBSurface(0, width, height, p->format->BitsPerPixel,
                                                p->format->Rmask,
                                                p->format->Gmask,
                                                p->format->Bmask,
                                                p->format->Amask);
    
    if (!surface)
        throw Exception(Exception::SDLError, "Error creating Bitmap: %s",
                        SDL_GetError());
    
    memcpy(surface->pixels, pixeldata, width*height*(p->format->BitsPerPixel/8));
    
    if (surface->w > glState.caps.maxTexSize || surface->h > glState.caps.maxTexSize)
    {
        p = new BitmapPrivate(this);
        p->megaSurface = surface;
        SDL_SetSurfaceBlendMode(p->megaSurface, SDL_BLENDMODE_NONE);
    }
    else
    {
        TEXFBO tex;
        
        try
        {
            tex = shState->texPool().request(surface->w, surface->h);
        }
        catch (const Exception &e)
        {
            SDL_FreeSurface(surface);
            throw e;
        }
        
        p = new BitmapPrivate(this);
        p->gl = tex;
        
        TEX::bind(p->gl.tex);
        TEX::uploadImage(p->gl.width, p->gl.height, surface->pixels, GL_RGBA);
        
        SDL_FreeSurface(surface);
    }
    
    if (width > INT16_MAX || height > INT16_MAX)
    {
        p->pixmanUseRegion32 = true;
        pixman_region_fini(&p->tainted);
        pixman_region32_init(&p->tainted32);
    }
    p->addTaintedArea(rect());
}

// frame is -2 for "any and all", -1 for "current", anything else for a specific frame
Bitmap::Bitmap(const Bitmap &other, int frame)
{
    other.guardDisposed();
    if (frame > -2) other.ensureAnimated();
    
    if (other.hasHires()) {
        Debug() << "BUG: High-res Bitmap from animation not implemented";
    }

    p = new BitmapPrivate(this);

    if (other.isMega())
    {
        p->megaSurface = SDL_ConvertSurfaceFormat(other.p->megaSurface, p->format->format, 0);
    }
    // TODO: Clean me up
    else if (!other.isAnimated() || frame >= -1) {
        p->gl = shState->texPool().request(other.width(), other.height());
        
        GLMeta::blitBegin(p->gl);
        // Blit just the current frame of the other animated bitmap
        if (!other.isAnimated() || frame == -1) {
            GLMeta::blitSource(other.getGLTypes());
        }
        else {
            auto &frames = other.getFrames();
            GLMeta::blitSource(frames[clamp(frame, 0, (int)frames.size() - 1)]);
        }
        GLMeta::blitRectangle(rect(), rect(), true);
        GLMeta::blitEnd();
    }
    else {
        p->animation.enabled = true;
        p->animation.fps = other.getAnimationFPS();
        p->animation.width = other.width();
        p->animation.height = other.height();
        p->animation.lastFrame = 0;
        p->animation.playTime = 0;
        p->animation.startTime = 0;
        p->animation.loop = other.getLooping();
        
        for (TEXFBO &sourceframe : other.getFrames()) {
            TEXFBO newframe;
            try {
                newframe = shState->texPool().request(p->animation.width, p->animation.height);
            } catch(const Exception &e) {
                for (TEXFBO &f : p->animation.frames)
                    shState->texPool().release(f);
                throw e;
            }
            
            GLMeta::blitBegin(newframe);
            GLMeta::blitSource(sourceframe);
            GLMeta::blitRectangle(rect(), rect(), true);
            GLMeta::blitEnd();
            
            p->animation.frames.push_back(newframe);
        }
    }
    
    if (width() > INT16_MAX || height() > INT16_MAX)
    {
        p->pixmanUseRegion32 = true;
        pixman_region_fini(&p->tainted);
        pixman_region32_init(&p->tainted32);
        pixman_region32_copy(&p->tainted32, &other.p->tainted32);
    }
    else
    {
        pixman_region_copy(&p->tainted, &other.p->tainted);
    }
}

Bitmap::Bitmap(TEXFBO &other)
{
    Bitmap *hiresBitmap = nullptr;

    if (other.selfHires != nullptr) {
        // Create a high-res version as well.
        hiresBitmap = new Bitmap(*other.selfHires);
        hiresBitmap->setLores(this);
    }

    p = new BitmapPrivate(this);

    p->gl = shState->texPool().request(other.width, other.height);

    p->selfHires = hiresBitmap;
    if (p->selfHires != nullptr) {
        p->gl.selfHires = &p->selfHires->getGLTypes();
    }

    // Skip blitting to lores texture, since only the hires one will be displayed.
    if (p->selfHires == nullptr) {
        GLMeta::blitBegin(p->gl);
        GLMeta::blitSource(other);
        GLMeta::blitRectangle(rect(), rect(), true);
        GLMeta::blitEnd();
    }

    if (width() > INT16_MAX || height() > INT16_MAX)
    {
        p->pixmanUseRegion32 = true;
        pixman_region_fini(&p->tainted);
        pixman_region32_init(&p->tainted32);
    }
    p->addTaintedArea(rect());
}

Bitmap::Bitmap(SDL_Surface *imgSurf, SDL_Surface *imgSurfHires, bool forceMega)
{
    Bitmap *hiresBitmap = nullptr;

    if (imgSurfHires != nullptr) {
        // Create a high-res version as well.
        hiresBitmap = new Bitmap(imgSurfHires, nullptr);
        hiresBitmap->setLores(this);
    }

    initFromSurface(imgSurf, hiresBitmap, forceMega);
}

Bitmap::~Bitmap()
{
    dispose();
}

void Bitmap::initFromSurface(SDL_Surface *imgSurf, Bitmap *hiresBitmap, bool forceMega)
{
    p->ensureFormat(imgSurf, SDL_PIXELFORMAT_ABGR8888);
    
    if (imgSurf->w > glState.caps.maxTexSize || imgSurf->h > glState.caps.maxTexSize || forceMega)
    {
        /* Mega surface */

        p = new BitmapPrivate(this);
        p->selfHires = hiresBitmap;
        p->megaSurface = imgSurf;
        SDL_SetSurfaceBlendMode(p->megaSurface, SDL_BLENDMODE_NONE);
    }
    else
    {
        /* Regular surface */
        TEXFBO tex;
        
        try
        {
            tex = shState->texPool().request(imgSurf->w, imgSurf->h);
        }
        catch (const Exception &e)
        {
            SDL_FreeSurface(imgSurf);
            throw e;
        }
        
        p = new BitmapPrivate(this);
        p->selfHires = hiresBitmap;
        p->gl = tex;
        if (p->selfHires != nullptr) {
            p->gl.selfHires = &p->selfHires->getGLTypes();
        }
        
        TEX::bind(p->gl.tex);
        TEX::uploadImage(p->gl.width, p->gl.height, imgSurf->pixels, GL_RGBA);
    }
    
    if (width() > INT16_MAX || height() > INT16_MAX)
    {
        p->pixmanUseRegion32 = true;
        pixman_region_fini(&p->tainted);
        pixman_region32_init(&p->tainted32);
    }
    p->addTaintedArea(rect());
}

/* "Child" bitmaps are a hack to support mega surfaces in Windows, Planes, and Sprites.
 * They determine which part of the parent will be visible, manually shrink it if necessary,
 * and send back new values for zoom and offsets. */

struct ChildPrivate
{
    Bitmap *self;
    Bitmap *parent;
    
    ChildPublic shared;
    
    sigslot::connection dirtyCon;
    sigslot::connection disposeCon;
    
    Vec2i parentPos;
    IntRect srcRect;
    IntRect oldSrcRect;
    bool dirty;
    Vec2 maxShrink;
    Vec2 currentZoom;
    Vec2 currentShrink;
    bool mirrored;
    int currentBushDepth;
    Transform *trans;
    IntRect oldVR;
    Vec2i oldOff;
    
    
    ChildPrivate(Bitmap *self, Bitmap *parent)
    : self(self),
    parent(parent),
    dirty(true),
    mirrored(false)
    {
        shared.width = parent->width();
        shared.height = parent->height();
        
        shared.realSrcRect.w = parent->width();
        shared.realSrcRect.h = parent->height();
        shared.srcRect.w = parent->width();
        shared.srcRect.h = parent->height();
        oldSrcRect = shared.realSrcRect;
        
        maxShrink.x = (float)self->width() / parent->width();
        maxShrink.y = (float)self->height() / parent->height();
        currentZoom.x = 1.0f;
        currentZoom.y = 1.0f;
        currentShrink.x = 1.0f;
        currentShrink.y = 1.0f;
        
        dirtyCon = parent->modified.connect(&ChildPrivate::childDirty, this);
        disposeCon = parent->wasDisposed.connect(&ChildPrivate::parentDisposed, this);
    }
    
    ~ChildPrivate()
    {
        dirtyCon.disconnect();
        disposeCon.disconnect();
    }
    
    void childDirty()
    {
        dirty = true;
    }
    
    void parentDisposed()
    {
        self->dispose();
    }
};

Bitmap *Bitmap::spawnChild()
{
    Bitmap *child;
    if(p->selfHires)
    {
        int childWidth = std::min(p->selfHires->width(), glState.caps.maxTexSize);
        int childHeight = std::min(p->selfHires->height(), glState.caps.maxTexSize);
        double scalingFactor = std::max(p->selfHires->width() / width(), p->selfHires->height() / height());
        double maxRatio = std::min((double)childWidth / shState->graphics().width(),
                                   (double)childHeight / shState->graphics().height());
        scalingFactor = std::min(maxRatio, scalingFactor);
        int loresWidth = (int)lround(scalingFactor * childWidth);
        int loresHeight = (int)lround(scalingFactor * childHeight);
        child = new Bitmap(loresWidth, loresHeight, true);
        Bitmap *hires = new Bitmap(childWidth, childHeight, true);
        hires->setLores(child);
        child->p->selfHires = hires;
    }
    else
    {
        int childWidth = std::min(width(), glState.caps.maxTexSize);
        int childHeight = std::min(height(), glState.caps.maxTexSize);
        child = new Bitmap(childWidth, childHeight, true);
    }
    
    
    child->p->pChild = new ChildPrivate(child, this);
    
    return child;
}

ChildPublic *Bitmap::getChildInfo()
{
    if (p->pChild)
        return &p->pChild->shared;
    return 0;
}

void Bitmap::childUpdate()
{
    if (!p->pChild)
        return;
    
    ChildPrivate *pChild = p->pChild;
    
    bool isWindow = pChild->shared.realZoom.x == -1.0f;
    bool isPlane = pChild->shared.wrap;
    bool isSprite = !isWindow && !isPlane;
    
    if (!pChild->shared.realZoom.x || !pChild->shared.realZoom.y)
    {
        pChild->shared.isVisible = false;
        return;
    }
    
    IntRect viewportRect(0, 0, shState->graphics().width(), shState->graphics().height());
    
    if (!SDL_IntersectRect(&viewportRect, pChild->shared.sceneRect, &viewportRect))
    {
        pChild->shared.zoom.x = pChild->shared.realZoom.x;
        pChild->shared.zoom.y = pChild->shared.realZoom.y;
        pChild->shared.isVisible = false;
        return;
    }
    
    if (isWindow)
    {
        viewportRect.x = pChild->shared.sceneRect->x;
        viewportRect.y = pChild->shared.sceneRect->y;
        IntRect window(pChild->shared.x + viewportRect.x - pChild->shared.sceneOrig->x,
                       pChild->shared.y + viewportRect.y - pChild->shared.sceneOrig->y,
                       pChild->shared.width, pChild->shared.height);
        if (!SDL_IntersectRect(&viewportRect, &window, &viewportRect))
        {
            pChild->shared.isVisible = false;
            return;
        }
        viewportRect.x = std::min(0, window.x);
        viewportRect.y = std::min(0, window.y);
    }
    
    bool updateNeeded = pChild->dirty;
    
    IntRect visibleRect = viewportRect;
    
    Vec2 realZoom(abs(pChild->shared.realZoom.x), abs(pChild->shared.realZoom.y));
    Vec2 shrink(1.0f, 1.0f);
    
    IntRect adjustedSrcRect = pChild->shared.realSrcRect;
    if (isSprite)
    {
        if (pChild->shared.realSrcRect.x < 0)
            adjustedSrcRect.w += pChild->shared.realSrcRect.x;
        if (pChild->shared.realSrcRect.y < 0)
            adjustedSrcRect.h += pChild->shared.realSrcRect.y;
        adjustedSrcRect.x = clamp(adjustedSrcRect.x, 0, pChild->parent->width());
        adjustedSrcRect.y = clamp(adjustedSrcRect.y, 0, pChild->parent->height());
        adjustedSrcRect.w = clamp(adjustedSrcRect.w, 0, pChild->shared.width - adjustedSrcRect.x);
        adjustedSrcRect.h = clamp(adjustedSrcRect.h, 0, pChild->shared.height - adjustedSrcRect.y);
        
        if (!adjustedSrcRect.w || !adjustedSrcRect.h)
        {
            pChild->shared.isVisible = false;
            return;
        }
    }
    else
        adjustedSrcRect = pChild->shared.realSrcRect;
    
    if (isPlane || isSprite)
    {
        visibleRect.x = pChild->shared.x - pChild->shared.sceneOrig->x + std::min(pChild->shared.sceneRect->x, 0);
        visibleRect.y = pChild->shared.y - pChild->shared.sceneOrig->y + std::min(pChild->shared.sceneRect->y, 0);
        
        if (pChild->shared.angle)
        {
            // rotate visibleRect clockwise around visibleRect.x and visibleRect.y
            FloatRect tmpRect = rotate_rect(visibleRect.pos(), -pChild->shared.angle,
                                       IntRect(Vec2i(),visibleRect.size()));
            tmpRect.x = floor(-tmpRect.x) + visibleRect.x;
            tmpRect.y = floor(-tmpRect.y) + visibleRect.y;
            visibleRect = tmpRect;
        }
        
        if (pChild->shared.waveAmp > 0)
        {
            /* At the moment the wave gets rotated too, which isn't what RGSS does.
               If that's ever fixed, then this needs to be moved to before the rotation. */
            
            /* The edge of the wave can still poke through sometimes for some reason,
               so we provide an extra 1 pixel buffer to ensure it can't happen. */
            visibleRect.x += pChild->shared.waveAmp + 1;
            visibleRect.w += pChild->shared.waveAmp * 2 + 2;
        }
        
        // maxShrink is the point at which the entire parent fits into the child
        Vec2 maxShrink;
        if (isSprite)
        {
            maxShrink.x = std::min((float)width() / adjustedSrcRect.w, 1.0f);
            maxShrink.y = std::min((float)height() / adjustedSrcRect.h, 1.0f);
        }
        else // Planes can just use the cached values
        {
            maxShrink = pChild->maxShrink;
        }
        shrink.x = clamp(std::min(width(), adjustedSrcRect.w) * realZoom.x / visibleRect.w, maxShrink.x, 1.0f);
        shrink.y = clamp(std::min(height(), adjustedSrcRect.h) * realZoom.y / visibleRect.h, maxShrink.y, 1.0f);
        
        // Uncomment to force max shrink for testing
        /*
        shrink.x = std::min(pChild->maxShrink.x, 1.0f);
        shrink.y = std::min(pChild->maxShrink.y, 1.0f);
        //*/
        
        pChild->shared.zoom.x = realZoom.x / shrink.x;
        pChild->shared.zoom.y = realZoom.y / shrink.y;
        if(!(shrink == pChild->currentShrink))
            updateNeeded = true;
        
        visibleRect.x = round(visibleRect.x / realZoom.x);
        visibleRect.y = round(visibleRect.y / realZoom.y);
        visibleRect.w = ceil(visibleRect.w / realZoom.x);
        visibleRect.h = ceil(visibleRect.h / realZoom.y);
        if (pChild->shared.wrap)
        {
            visibleRect.x = -wrapRange(-visibleRect.x, 0, adjustedSrcRect.w);
            visibleRect.y = -wrapRange(-visibleRect.y, 0, adjustedSrcRect.h);
        }
    }
    
    int realOX = pChild->shared.realOffset.x;
    int realOY = pChild->shared.realOffset.y;
    
    if (isSprite)
    {
        if (pChild->shared.realSrcRect.x < 0)
            realOX += pChild->shared.realSrcRect.x;
        if (pChild->shared.realSrcRect.y < 0)
            realOY += pChild->shared.realSrcRect.y;
    }
    
    
    // If none of this has changed, then we can just return now
    if (!updateNeeded && pChild->oldVR == visibleRect && pChild->oldOff == Vec2i(realOX, realOY) &&
        (pChild->shared.wrap ||
         (pChild->mirrored == pChild->shared.mirrored && pChild->shared.realSrcRect == pChild->oldSrcRect))
       )
    {
        return;
    }
    pChild->oldOff = Vec2i(realOX, realOY);
    pChild->oldVR = visibleRect;
    
    if (!isPlane)
    {
        // Double the visibleRect.pos, because I should be using a
        // zeroed out position for the visibleRect but doing this is easier
        IntRect tmpSourceRect(visibleRect.pos() * 2 - Vec2i(realOX, realOY),
                              adjustedSrcRect.size());
        if (!SDL_HasIntersection(&visibleRect, &tmpSourceRect))
        {
            pChild->shared.isVisible = false;
            return;
        }
        if (pChild->shared.angle)
        {
            // Rotating the viewport leaves triangles on all sides that are considered in bounds.
            // By also rotating the source rect and comparing it to the unrotated viewport, we can
            // be certain if the sprite is visible or not.
            tmpSourceRect.x = floor(-realOX * realZoom.x);
            tmpSourceRect.y = floor(-realOY * realZoom.y);
            tmpSourceRect.w = ceil(tmpSourceRect.w * realZoom.x);
            tmpSourceRect.h = ceil(tmpSourceRect.h * realZoom.x);
            FloatRect tmpRect = rotate_rect(Vec2i(), pChild->shared.angle, tmpSourceRect);
            Vec2i origin(pChild->shared.x - pChild->shared.sceneOrig->x + std::min(pChild->shared.sceneRect->x, 0),
                         pChild->shared.y - pChild->shared.sceneOrig->y + std::min(pChild->shared.sceneRect->y, 0));
            tmpRect.x = floor(tmpRect.x) + origin.x;
            tmpRect.y = floor(tmpRect.y) + origin.y;
            tmpSourceRect = tmpRect;
            
            if (!SDL_HasIntersection(&viewportRect, &tmpSourceRect))
            {
                pChild->shared.isVisible = false;
                return;
            }
        }
    }
    
    pChild->shared.isVisible = true;
    
    int selfWidth = round(width() / shrink.x);
    int selfHeight = round(height() / shrink.y);
    
    int overflowX = std::max(selfWidth - visibleRect.w, 0);
    int overflowY = std::max(selfHeight - visibleRect.h, 0);
    
    int minOX = pChild->parentPos.x;
    int minOY = pChild->parentPos.y;
    int maxOX = minOX + overflowX;
    int maxOY = minOY + overflowY;
    int maxOX2 = wrapRange(maxOX, 0, adjustedSrcRect.w);
    int maxOY2 = wrapRange(maxOY, 0, adjustedSrcRect.h);
    
    int adjustedrealOX = -visibleRect.x + realOX;
    int adjustedrealOY = -visibleRect.y + realOY;
    
    // The position in the srcRect that the child pulls from. Initialized to the previous run's result.
    Vec2i newParentPos = pChild->parentPos;
    
    if (pChild->shared.wrap)
    {
        adjustedrealOX = wrapRange(adjustedrealOX, 0, adjustedSrcRect.w);
        adjustedrealOY = wrapRange(adjustedrealOY, 0, adjustedSrcRect.h);
    }
    
    for (int i = 0; i < 2; i++)
    {
        if (updateNeeded || (adjustedrealOX < minOX && (!pChild->shared.wrap || maxOX2 == maxOX || adjustedrealOX > maxOX2)) || adjustedrealOX > maxOX)
        {
            if (selfWidth >= adjustedSrcRect.w)
                newParentPos.x = 0;
            else
                newParentPos.x = adjustedrealOX - overflowX / 2;
            if (!pChild->shared.wrap)
                newParentPos.x = clamp(newParentPos.x, 0,
                                       std::max(adjustedSrcRect.w - selfWidth,0));
        }
        if (updateNeeded || (adjustedrealOY < minOY && (!pChild->shared.wrap || maxOY2 == maxOY || adjustedrealOY > maxOY2)) || adjustedrealOY > maxOY)
        {
            if (selfHeight >= adjustedSrcRect.h)
                newParentPos.y = 0;
            else
                newParentPos.y = adjustedrealOY - overflowY / 2;
            if (!pChild->shared.wrap)
                newParentPos.y = clamp(newParentPos.y, 0,
                                       std::max(adjustedSrcRect.h - selfHeight,0));
        }
        if (updateNeeded)
        {
            pChild->parentPos = newParentPos;
        }
        // If either x or y was updated, run through it again to update the other one
        if (newParentPos != pChild->parentPos)
            updateNeeded = true;
        else
            break;
    }
    
    
    if (!isSprite)
    {
        pChild->shared.offset.x = realOX - newParentPos.x;
        pChild->shared.offset.y =  realOY - newParentPos.y;
    }
    
    if (isPlane)
    {
        pChild->shared.offset.x = wrapRange(pChild->shared.offset.x - visibleRect.x, 0,
                                            adjustedSrcRect.w);
        pChild->shared.offset.y = wrapRange(pChild->shared.offset.y - visibleRect.y, 0,
                                            adjustedSrcRect.h);
        
        // Leaving this as a float (and making plane.cpp store it as a float)
        // makes positioning almost perfect when zoomed
        pChild->shared.offset.x = pChild->shared.offset.x * realZoom.x;
        pChild->shared.offset.y = pChild->shared.offset.y * realZoom.y;
        
        pChild->shared.offset.x -= pChild->shared.sceneOrig->x;
        pChild->shared.offset.y -= pChild->shared.sceneOrig->y;
        
        pChild->shared.offset.x += std::min(pChild->shared.sceneRect->x, 0);
        pChild->shared.offset.y += std::min(pChild->shared.sceneRect->y, 0);
    }
    else if (isSprite)
    {
        if (!updateNeeded && pChild->oldSrcRect != pChild->shared.realSrcRect)
        {
            if (pChild->srcRect.encloses(adjustedSrcRect))
            {
                pChild->shared.srcRect = IntRect(pChild->shared.realSrcRect.pos() - pChild->srcRect.pos(),
                                                 pChild->shared.realSrcRect.size());
                
                pChild->shared.srcRect.x = floor(pChild->shared.srcRect.x * shrink.x);
                pChild->shared.srcRect.y = floor(pChild->shared.srcRect.y * shrink.y);
                pChild->shared.srcRect.w = round(pChild->shared.srcRect.w * shrink.x);
                pChild->shared.srcRect.h = round(pChild->shared.srcRect.h * shrink.y);
            }
            else
                updateNeeded = true;
        }
        pChild->oldSrcRect = pChild->shared.realSrcRect;
        // Sprite stores the offsets as floats, and they get jittery when shrunk if we try to use ints,
        // so we just leave it as a float and it works perfectly.
        // We also use the srcRect to position the subimage for sprites instead of modifying the offset.
        // It makes positioning the wave and bush effect a lot simpler.
        pChild->shared.offset.x = pChild->shared.realOffset.x * shrink.x;
        pChild->shared.offset.y = pChild->shared.realOffset.y * shrink.y;
        
        if (pChild->shared.mirrored)
        {
            newParentPos.x = std::max(adjustedSrcRect.w - selfWidth, 0) - newParentPos.x;
        }
        
        if (pChild->mirrored != pChild->shared.mirrored && selfWidth != adjustedSrcRect.w)
            updateNeeded = true;
        pChild->mirrored = pChild->shared.mirrored;
    }
    
    if (updateNeeded)
    {
        if (pChild->shared.wrap)
        {
            newParentPos.x = wrapRange(newParentPos.x, 0, adjustedSrcRect.w);
            newParentPos.y = wrapRange(newParentPos.y, 0, adjustedSrcRect.h);
        }
        
        std::vector<IntRect> subrects;
        long locNum = 1;
        IntRect baseRect(newParentPos.x + adjustedSrcRect.x,
                         newParentPos.y + adjustedSrcRect.y,
                         std::min(selfWidth, adjustedSrcRect.w - newParentPos.x),
                         std::min(selfHeight, adjustedSrcRect.h - newParentPos.y));
        
        if (isSprite)
        {
            int deltaW = selfWidth - baseRect.w;
            int deltaH = selfHeight - baseRect.h;
            
            if (deltaW)
            {
                baseRect.x = clamp(baseRect.x - (int)ceil(deltaW / 2.0f), 0, pChild->parent->width() - selfWidth);
                baseRect.w = selfWidth;
            }
            if (deltaH)
            {
                baseRect.y = clamp(baseRect.y - (int)ceil(deltaH / 2.0f), 0, pChild->parent->height() - selfHeight);
                baseRect.h = selfHeight;
            }
            
            if (adjustedSrcRect.w > baseRect.w && pChild->mirrored)
            {
                float x = (pChild->shared.realSrcRect.x + pChild->shared.realSrcRect.w) - (baseRect.x + baseRect.w);
                pChild->shared.srcRect.x = (std::min(pChild->shared.realSrcRect.x, 0) - x) * shrink.x;
            }
            else
            {
                pChild->shared.srcRect.x = (pChild->shared.realSrcRect.x - baseRect.x) * shrink.x;
            }
            pChild->shared.srcRect.w = pChild->shared.realSrcRect.w * shrink.x;
            pChild->shared.srcRect.y = (pChild->shared.realSrcRect.y - baseRect.y) * shrink.y;
            pChild->shared.srcRect.h = pChild->shared.realSrcRect.h * shrink.y;
            
            pChild->srcRect = baseRect;
        }
        
        subrects.push_back(baseRect);
        if (pChild->shared.wrap && baseRect.w < selfWidth)
        {
            locNum *= 2;
            subrects.push_back(IntRect(0, baseRect.y,
                                                selfWidth - baseRect.w,
                                                baseRect.h));
        }
        if (pChild->shared.wrap && baseRect.h < selfHeight)
        {
            locNum *= 2;
            subrects.push_back(IntRect(baseRect.x, 0,
                                                baseRect.w,
                                                selfHeight - baseRect.h));
        }
        if (locNum == 4)
        {
            subrects.push_back(IntRect(0, 0,
                                                selfWidth - baseRect.w,
                                                selfHeight - baseRect.h));
        }
        
        clear();
        
        int bufferX = 0;
        int bufferY = 0;
        for (long i = 0; i < locNum; i++)
        {
            IntRect sourceRect = subrects[i];
            IntRect destRect(sourceRect.x == baseRect.x ? 0 : bufferX,
                             sourceRect.y == baseRect.y ? 0 : bufferY,
                             sourceRect.x == baseRect.x ? round(sourceRect.w * shrink.x) : width() - bufferX,
                             sourceRect.y == baseRect.y ? round(sourceRect.h * shrink.y) : height() - bufferY);
            if (!bufferX)
            {
                bufferX = destRect.w;
                bufferY = destRect.h;
            }
            stretchBlt(destRect, *pChild->parent, sourceRect, 255);
        }
        
        pChild->dirty = false;
        pChild->currentShrink = shrink;
    }
}

int Bitmap::width() const
{
    guardDisposed();
    
    if (p->megaSurface) {
        return p->megaSurface->w;
    }
    
    if (p->animation.enabled) {
        return p->animation.width;
    }
    
    return p->gl.width;
}

int Bitmap::height() const
{
    guardDisposed();
    
    if (p->megaSurface)
        return p->megaSurface->h;
    
    if (p->animation.enabled)
        return p->animation.height;
    
    return p->gl.height;
}

bool Bitmap::hasHires() const{
    guardDisposed();

    return p->selfHires;
}

DEF_ATTR_RD_SIMPLE(Bitmap, Hires, Bitmap*, p->selfHires)

void Bitmap::setHires(Bitmap *hires) {
    guardDisposed();

    hires->setLores(this);
    p->selfHires = hires;
}

void Bitmap::setLores(Bitmap *lores) {
    guardDisposed();

    p->selfLores = lores;
}

bool Bitmap::isMega() const{
    guardDisposed();
    
    return p->megaSurface;
}

bool Bitmap::isAnimated() const {
    guardDisposed();
    
    return p->animation.enabled;
}

IntRect Bitmap::rect() const
{
    guardDisposed();
    
    return IntRect(0, 0, width(), height());
}

void Bitmap::blt(int x, int y,
                 const Bitmap &source, const IntRect &rect,
                 int opacity)
{
    if (source.isDisposed())
        return;
    
    stretchBlt(IntRect(x, y, rect.w, rect.h),
               source, rect, opacity);
}

static bool shrinkRects(float &sourcePos, float &sourceLen, const int &sBitmapLen,
                         float &destPos, float &destLen, const int &dBitmapLen, bool normalize = false)
{
    float sStart = sourceLen > 0 ? sourcePos : sourceLen + sourcePos;
    float sEnd = sourceLen > 0 ? sourceLen + sourcePos : sourcePos;
    float sLength = sEnd - sStart;
    
    if (sStart >= 0 && sEnd < sBitmapLen)
        return false;
    
    if (sStart >= sBitmapLen || sEnd < 0)
        return true;
    
    float dStart = destLen > 0 ? destPos: destLen + destPos;
    float dEnd = destLen > 0 ? destLen + destPos : destPos;
    float dLength = dEnd - dStart;
    
    float delta = sEnd - sBitmapLen;
    float dDelta;
    if (delta > 0)
    {
        dDelta = (delta / sLength) * dLength;
        sLength -= delta;
        sEnd = sBitmapLen;
        dEnd -= dDelta;
        dLength -= dDelta;
    }
    if (sStart < 0)
    {
        dDelta = (sStart / sLength) * dLength;
        sLength += sStart;
        sStart = 0;
        dStart -= dDelta;
        dLength += dDelta;
    }
    
    if (!normalize)
    {
        sourcePos = sourceLen > 0 ? sStart : sEnd;
        sourceLen = sourceLen > 0 ? sLength : -sLength;
        destPos = destLen > 0  ? dStart : dEnd;
        destLen = destLen > 0 ? dLength : -dLength;
    }
    else
    {
        // Ensure the source rect has positive dimensions, for blitting from mega surfaces
        destPos = (destLen > 0 == sourceLen > 0) ? dStart : dEnd;
        destLen = (destLen > 0 == sourceLen > 0) ? dLength : -dLength;
        sourcePos = sStart;
        sourceLen = sLength;
    }
    
    return false;
}

static bool shrinkRects(int &sourcePos, int &sourceLen, const int &sBitmapLen,
                         int &destPos, int &destLen, const int &dBitmapLen)
{
    float fSourcePos = sourcePos;
    float fSourceLen = sourceLen;
    float fDestPos = destPos;
    float fDestLen = destLen;
    
    bool ret = shrinkRects(fSourcePos, fSourceLen, sBitmapLen, fDestPos, fDestLen, dBitmapLen, true);
    
    if (!ret)
        ret = shrinkRects(fDestPos, fDestLen, dBitmapLen, fSourcePos, fSourceLen, sBitmapLen);
    
    sourcePos = round(fSourcePos);
    sourceLen = round(fSourceLen);
    destPos = round(fDestPos);
    destLen = round(fDestLen);
    
    return ret || sourceLen == 0 || destLen == 0;
}

static uint32_t &getPixelAt(SDL_Surface *surf, SDL_PixelFormat *form, int x, int y)
{
    size_t offset = x*form->BytesPerPixel + y*surf->pitch;
    uint8_t *bytes = (uint8_t*) surf->pixels + offset;
    
    return *((uint32_t*) bytes);
}

void Bitmap::stretchBlt(IntRect destRect,
                        const Bitmap &source, IntRect sourceRect,
                        int opacity)
{
    guardDisposed();

    if (source.isDisposed())
        return;

    if (hasHires()) {
        int destX, destY, destWidth, destHeight;
        destX = destRect.x * p->selfHires->width() / width();
        destY = destRect.y * p->selfHires->height() / height();
        destWidth = destRect.w * p->selfHires->width() / width();
        destHeight = destRect.h * p->selfHires->height() / height();

        p->selfHires->stretchBlt(IntRect(destX, destY, destWidth, destHeight), source, sourceRect, opacity);
        return;
    }

    if (source.hasHires()) {
        int sourceX, sourceY, sourceWidth, sourceHeight;
        sourceX = sourceRect.x * source.getHires()->width() / source.width();
        sourceY = sourceRect.y * source.getHires()->height() / source.height();
        sourceWidth = sourceRect.w * source.getHires()->width() / source.width();
        sourceHeight = sourceRect.h * source.getHires()->height() / source.height();

        stretchBlt(destRect, *source.getHires(), IntRect(sourceX, sourceY, sourceWidth, sourceHeight), opacity);
        return;
    }

    opacity = clamp(opacity, 0, 255);
    
    if (opacity == 0)
        return;
    
    if(shrinkRects(sourceRect.x, sourceRect.w, source.width(), destRect.x, destRect.w, width()))
        return;
    if(shrinkRects(sourceRect.y, sourceRect.h, source.height(), destRect.y, destRect.h, height()))
        return;
    
    SDL_Surface *srcSurf = source.megaSurface();
    SDL_Surface *blitTemp = 0;
    bool touchesTaintedArea = p->touchesTaintedArea(destRect);
    
    if (p->megaSurface)
    {
        if (!srcSurf)
        {
            source.createSurface();
            srcSurf = source.p->surface;
        }
        
        if (destRect.w < 0 || destRect.h < 0)
        {
            // SDL can't handle negative dimensions when blitting, so we have to do it manually
            blitTemp = SDL_CreateRGBSurface(0, sourceRect.w, sourceRect.h, p->format->BitsPerPixel,
                                                        p->format->Rmask, p->format->Gmask,
                                                        p->format->Bmask, p->format->Amask);
            
            bool flipW = destRect.w < 0;
            bool flipH = destRect.y < 0;
            
            for(int dx = 0, sx = (flipW ? sourceRect.x + sourceRect.w - 1 : sourceRect.x);
                dx < sourceRect.w; dx++, (flipW ? sx-- : sx++))
            {
                for(int dy = 0, sy = (flipH ? sourceRect.y + sourceRect.h - 1 : sourceRect.y);
                    dy < sourceRect.h; dy++, (flipH ? sy-- : sy++))
                {
                    uint32_t &srcPixel = getPixelAt(srcSurf, p->format, sx, sy);
                    uint32_t &destPixel = getPixelAt(blitTemp, p->format, dx, dy);
                    destPixel = srcPixel;
                }
            }
            srcSurf = blitTemp;
            sourceRect.x = sourceRect.y = 0;
            destRect = normalizedRect(destRect);
        }
        
        if (touchesTaintedArea)
            SDL_SetSurfaceBlendMode(srcSurf, SDL_BLENDMODE_BLEND);
        else
            SDL_SetSurfaceBlendMode(srcSurf, SDL_BLENDMODE_NONE);
        
        Uint8 tempAlpha;
        SDL_GetSurfaceAlphaMod(srcSurf, &tempAlpha);
        SDL_SetSurfaceAlphaMod(srcSurf, opacity);
        
        if(sourceRect.w == destRect.w && sourceRect.h == destRect.h)
            SDL_BlitSurface(srcSurf, &sourceRect, p->megaSurface, &destRect);
        else
            SDL_BlitScaled(srcSurf, &sourceRect, p->megaSurface, &destRect);
        
        SDL_SetSurfaceBlendMode(srcSurf, SDL_BLENDMODE_NONE);
        SDL_SetSurfaceAlphaMod(srcSurf, tempAlpha);
        
        // Delete the source surface if the source is an animation
        if (source.p->animation.enabled && source.p->surface)
        {
            SDL_FreeSurface(source.p->surface);
            source.p->surface = 0;
        }
    }
    else if (!srcSurf && opacity == 255 && !touchesTaintedArea)
    {
        /* Fast blit */
        GLMeta::blitBegin(getGLTypes());
        GLMeta::blitSource(source.getGLTypes());
        GLMeta::blitRectangle(sourceRect, destRect, false);
        GLMeta::blitEnd();
    }
    else
    {
        if (srcSurf)
        {
            SDL_Rect srcRect = sourceRect;
            bool subImageFix = shState->config().subImageFix;
            bool srcRectTooBig = srcRect.w > glState.caps.maxTexSize ||
                                 srcRect.h > glState.caps.maxTexSize;
            bool srcSurfTooBig = srcSurf->w > glState.caps.maxTexSize || 
                                 srcSurf->h > glState.caps.maxTexSize;
            
            if (srcRectTooBig || srcSurfTooBig)
            {
                Uint8 tempAlpha;
                SDL_GetSurfaceAlphaMod(srcSurf, &tempAlpha);
                SDL_SetSurfaceAlphaMod(srcSurf, opacity);
                
                if (srcRectTooBig)
                {
                    /* We have to resize it here anyway, so use software resizing */
                    blitTemp =
                        SDL_CreateRGBSurface(0, abs(destRect.w), abs(destRect.h), p->format->BitsPerPixel,
                                             p->format->Rmask, p->format->Gmask,
                                             p->format->Bmask, p->format->Amask);
                    SDL_BlitScaled(srcSurf, &srcRect, blitTemp, 0);
                }
                else
                {
                    /* Just crop it, let the shader resize it later */
                    blitTemp =
                        SDL_CreateRGBSurface(0, sourceRect.w, sourceRect.h, p->format->BitsPerPixel,
                                             p->format->Rmask, p->format->Gmask,
                                             p->format->Bmask, p->format->Amask);
                    SDL_BlitSurface(srcSurf, &srcRect, blitTemp, 0);
                }
                
                SDL_SetSurfaceAlphaMod(srcSurf, tempAlpha);
                
                opacity = 255;
                srcSurf = blitTemp;
                
                sourceRect.w = srcSurf->w;
                sourceRect.h = srcSurf->h;
                sourceRect.x = 0;
                sourceRect.y = 0;
            }
            
            if (opacity == 255 && !touchesTaintedArea)
            {
                if (!subImageFix &&
                    srcSurf->w == destRect.w && srcSurf->h == destRect.h &&
                    srcSurf->w == sourceRect.w && srcSurf->h == sourceRect.h)
                {
                    /* No scaling needed */
                    TEX::bind(getGLTypes().tex);
                    TEX::uploadSubImage(destRect.x, destRect.y,
                                        destRect.w, destRect.h,
                                        srcSurf->pixels, GL_RGBA);
                }
                else
                {
                    /* Resizing or subImageFix involved: need to use intermediary TexFBO */
                    TEXFBO &gpTF = shState->gpTexFBO(srcSurf->w, srcSurf->h);
                    
                    TEX::bind(gpTF.tex);
                    TEX::uploadSubImage(0, 0, srcSurf->w, srcSurf->h, srcSurf->pixels, GL_RGBA);
                    
                    GLMeta::blitBegin(p->gl);
                    GLMeta::blitSource(gpTF);
                    /* True sets the scaler to use GL_LINEAR, 
                     * false leaves it alone (GL_NEAREST by default) */
                    GLMeta::blitRectangle(sourceRect, destRect, false);
                    GLMeta::blitEnd();
                }
            }
        }
        if (opacity < 255 || touchesTaintedArea)
        {
            /* We're touching a tainted area or still need to reduce opacity */
             
            /* Fragment pipeline */
            float normOpacity = (float) opacity / 255.0f;
            
            TEXFBO &gpTex = shState->gpTexFBO(destRect.w, destRect.h);
            Vec2i gpTexSize;
            
            GLMeta::blitBegin(gpTex);
            GLMeta::blitSource(getGLTypes());
            GLMeta::blitRectangle(destRect, Vec2i());
            GLMeta::blitEnd();
            
            int sourceWidth, sourceHeight;
            FloatRect bltSubRect;
            if (srcSurf)
            {
                shState->ensureTexSize(srcSurf->w, srcSurf->h, gpTexSize);
                sourceWidth = gpTexSize.x;
                sourceHeight = gpTexSize.y;
            }
            else
            {
                sourceWidth = source.width();
                sourceHeight = source.height();
            }
                bltSubRect = FloatRect((float) sourceRect.x / sourceWidth,
                                       (float) sourceRect.y / sourceHeight,
                                       ((float) sourceWidth / sourceRect.w) * ((float) destRect.w / gpTex.width),
                                       ((float) sourceHeight / sourceRect.h) * ((float) destRect.h / gpTex.height));
            
            
            BltShader &shader = shState->shaders().blt;
            shader.bind();
            if (srcSurf)
            {
                shader.setTexSize(gpTexSize);
                shader.setSource();
            }
            shader.setDestination(gpTex.tex);
            shader.setSubRect(bltSubRect);
            shader.setOpacity(normOpacity);
            
            if (srcSurf)
            {
                shState->bindTex();
                TEX::uploadSubImage(0, 0, srcSurf->w, srcSurf->h, srcSurf->pixels, GL_RGBA);
            }
            
            Quad &quad = shState->gpQuad();
            quad.setTexPosRect(sourceRect, destRect);
            quad.setColor(Vec4(1, 1, 1, normOpacity));
            
            if (!srcSurf)
                source.p->bindTexture(shader, false);
            p->bindFBO();
            p->pushSetViewport(shader);
            
            p->blitQuad(quad);
            
            p->popViewport();
        }
    }
    
    if (blitTemp)
        SDL_FreeSurface(blitTemp);
    
    p->addTaintedArea(destRect);
    p->onModified();
}

void Bitmap::fillRect(int x, int y,
                      int width, int height,
                      const Vec4 &color)
{
    fillRect(IntRect(x, y, width, height), color);
}

void Bitmap::fillRect(const IntRect &rect, const Vec4 &color)
{
    guardDisposed();
    
    GUARD_ANIMATED;
    
    if (hasHires()) {
        int destX, destY, destWidth, destHeight;
        destX = rect.x * p->selfHires->width() / width();
        destY = rect.y * p->selfHires->height() / height();
        destWidth = rect.w * p->selfHires->width() / width();
        destHeight = rect.h * p->selfHires->height() / height();

        p->selfHires->fillRect(IntRect(destX, destY, destWidth, destHeight), color);
    }

    p->fillRect(rect, color);
    
    if (color.w == 0)
    /* Clear op */
        p->substractTaintedArea(rect);
    else
    /* Fill op */
        p->addTaintedArea(rect);
    
    p->onModified();
}

void Bitmap::gradientFillRect(int x, int y,
                              int width, int height,
                              const Vec4 &color1, const Vec4 &color2,
                              bool vertical)
{
    gradientFillRect(IntRect(x, y, width, height), color1, color2, vertical);
}

void Bitmap::gradientFillRect(const IntRect &rect,
                              const Vec4 &color1, const Vec4 &color2,
                              bool vertical)
{
    guardDisposed();
    
    GUARD_ANIMATED;
    
    if (rect.w <= 0 || rect.h <= 0 || rect.x >= width() || rect.y >= height() ||
        rect.w < -rect.x || rect.h < -rect.y)
        return;
    
    if (hasHires()) {
        int destX, destY, destWidth, destHeight;
        destX = rect.x * p->selfHires->width() / width();
        destY = rect.y * p->selfHires->height() / height();
        destWidth = rect.w * p->selfHires->width() / width();
        destHeight = rect.h * p->selfHires->height() / height();

        p->selfHires->gradientFillRect(IntRect(destX, destY, destWidth, destHeight), color1, color2, vertical);
    }


    if (p->megaSurface)
    {
        float progress = 0.0f;
        float invProgress = 1.0f;
        Color c1 = color1;
        Color c2 = color2;
        int orig, end;
        uint8_t r, g, b, a;
        float max;
        SDL_Rect destRect = rect;
        int *current;
        if (vertical)
        {
            destRect.w = std::min(rect.w, width() - rect.x);
            destRect.h = 1;
            
            current = &destRect.y;
            orig = rect.y;
            max = rect.h - 1;
            end = std::min(rect.y + rect.h, height());
        }
        else
        {
            destRect.w = 1;
            destRect.h = std::min(rect.h, height() - rect.y);
            
            current = &destRect.x;
            orig = rect.x;
            max = rect.w - 1;
            end = std::min(rect.x + rect.w, width());
        }
        while (*current < end)
        {
            progress = (*current - orig) / max;
            invProgress = 1.0f - progress;
            r = round((c1.red * invProgress) + (c2.red * progress));
            g = round((c1.green * invProgress) + (c2.green * progress));
            b = round((c1.blue * invProgress) + (c2.blue * progress));
            a = round((c1.alpha * invProgress) + (c2.alpha * progress));
            Uint32 color = SDL_MapRGBA(p->format, r, g, b, a);
            
            SDL_FillRect(p->megaSurface, &destRect, color);
            
            (*current)++;
        }
    }
    else
    {
        SimpleColorShader &shader = shState->shaders().simpleColor;
        shader.bind();
        shader.setTranslation(Vec2i());
        
        Quad &quad = shState->gpQuad();
        
        if (vertical)
        {
            quad.vert[0].color = color1;
            quad.vert[1].color = color1;
            quad.vert[2].color = color2;
            quad.vert[3].color = color2;
        }
        else
        {
            quad.vert[0].color = color1;
            quad.vert[3].color = color1;
            quad.vert[1].color = color2;
            quad.vert[2].color = color2;
        }
        
        quad.setPosRect(rect);
        
        p->bindFBO();
        p->pushSetViewport(shader);
        
        p->blitQuad(quad);
        
        p->popViewport();
    }
    
    p->addTaintedArea(rect);
    
    p->onModified();
}

void Bitmap::clearRect(int x, int y, int width, int height)
{
    clearRect(IntRect(x, y, width, height));
}

void Bitmap::clearRect(const IntRect &rect)
{
    guardDisposed();
    
    GUARD_ANIMATED;
    
    if (hasHires()) {
        int destX, destY, destWidth, destHeight;
        destX = rect.x * p->selfHires->width() / width();
        destY = rect.y * p->selfHires->height() / height();
        destWidth = rect.w * p->selfHires->width() / width();
        destHeight = rect.h * p->selfHires->height() / height();

        p->selfHires->clearRect(IntRect(destX, destY, destWidth, destHeight));
    }

    p->fillRect(rect, Vec4());
    
    p->substractTaintedArea(rect);
    
    p->onModified();
}

void Bitmap::blur()
{
    guardDisposed();
    
    GUARD_ANIMATED;
    
    if (hasHires()) {
        p->selfHires->blur();
    }

    // TODO: Is there some kind of blur radius that we need to handle for high-res mode?

    if(p->megaSurface)
    {
        int buffer = 5;
        
        int widthMult = 1;
        int tmpWidth = width();
        int bufferX = 0;
        
        int heightMult = 1;
        int tmpHeight = height();
        int bufferY = 0;
        
        if(width() > glState.caps.maxTexSize)
        {
            widthMult = ceil((float) width() / (glState.caps.maxTexSize - (buffer * 2)));
            tmpWidth = ceil((float) width() / widthMult) + (buffer * 2);
            bufferX = buffer;
        }
        if(height() > glState.caps.maxTexSize)
        {
            heightMult = ceil((float) height() / (glState.caps.maxTexSize - (buffer * 2)));
            tmpHeight = ceil((float) height() / heightMult) + (buffer * 2);
            bufferY = buffer;
        }
        
        Bitmap *tmp = new Bitmap(tmpWidth + (bufferX * 2), tmpHeight + (bufferY * 2), true);
        IntRect sourceRect = tmp->rect();
        IntRect destRect = {};
        
        pixman_region16_t originalTainted;
        pixman_region32_t originalTainted32;
        if (p->pixmanUseRegion32)
        {
            pixman_region32_init(&originalTainted32);
            pixman_region32_copy(&originalTainted32, &p->tainted32);
        }
        else
        {
            pixman_region_init(&originalTainted);
            pixman_region_copy(&originalTainted, &p->tainted);
        }
        for (int i = 0; i < widthMult; i++)
        {
            int tmpX = i ? bufferX : 0;
            sourceRect.x = (tmpWidth - tmpX) * i;
            destRect.x = sourceRect.x + tmpX;
            destRect.w = sourceRect.w - (bufferX * (i ? 2 : 1));
            
            for (int j = 0; j < heightMult; j++)
            {
                int tmpY = j ? bufferY : 0;
                sourceRect.y = (tmpHeight - tmpY) * j;
                destRect.y = sourceRect.y + tmpY;
                destRect.h = sourceRect.h - (bufferY * (j ? 2 : 1));
                
                tmp->clear();
                p->clearTaintedArea();
                
                IntRect tmpRect = tmp->rect();
                tmpRect.x = tmpRect.w - std::min(sourceRect.w, width() - sourceRect.x);
                tmpRect.y = tmpRect.h - std::min(sourceRect.h, height() - sourceRect.y);
                tmpRect.w = sourceRect.w;
                tmpRect.h = sourceRect.h;
                
                
                tmp->stretchBlt(tmpRect, *this, sourceRect, 255);
                tmp->blur();
                
                stretchBlt(destRect, *tmp, IntRect(tmpRect.x + tmpX, tmpRect.y + tmpY, destRect.w, destRect.h), 255);
            }
        }
        delete tmp;
        p->clearTaintedArea();
        if (p->pixmanUseRegion32)
        {
            pixman_region32_copy(&p->tainted32, &originalTainted32);
            pixman_region32_fini(&originalTainted32);
        }
        else
        {
            pixman_region_copy(&p->tainted, &originalTainted);
            pixman_region_fini(&originalTainted);
        }
    }
    else
    {
        Quad &quad = shState->gpQuad();
        FloatRect rect(0, 0, width(), height());
        quad.setTexPosRect(rect, rect);
        
        TEXFBO auxTex = shState->texPool().request(width(), height());
        
        BlurShader &shader = shState->shaders().blur;
        BlurShader::HPass &pass1 = shader.pass1;
        BlurShader::VPass &pass2 = shader.pass2;
        
        glState.blend.pushSet(false);
        glState.viewport.pushSet(IntRect(0, 0, width(), height()));
        
        TEX::bind(p->gl.tex);
        FBO::bind(auxTex.fbo);
        
        pass1.bind();
        pass1.setTexSize(Vec2i(width(), height()));
        pass1.applyViewportProj();
        
        quad.draw();
        
        TEX::bind(auxTex.tex);
        p->bindFBO();
        
        pass2.bind();
        pass2.setTexSize(Vec2i(width(), height()));
        pass2.applyViewportProj();
        
        quad.draw();
        
        glState.viewport.pop();
        glState.blend.pop();
        
        shState->texPool().release(auxTex);
        
        p->onModified();
    }
}

void Bitmap::radialBlur(int angle, int divisions)
{
    guardDisposed();
    
    GUARD_MEGA;
    GUARD_ANIMATED;
    
    if (hasHires()) {
        p->selfHires->radialBlur(angle, divisions);
        return;
    }

    angle     = clamp<int>(angle, 0, 359);
    divisions = clamp<int>(divisions, 2, 100);
    
    const int _width = width();
    const int _height = height();
    
    float angleStep = (float) angle / (divisions-1);
    float opacity   = 1.0f / divisions;
    float baseAngle = -((float) angle / 2);
    
    ColorQuadArray qArray;
    
    int wNum = _width < _height ? ceil((ceil(_height / 2.0f) - (_width / 2)) / _width) : 1;
    int hNum = _height < _width ? ceil((ceil(_width / 2.0f) - (_height / 2)) / _height) : 1;
    
    qArray.resize(wNum * 2 + hNum * 2 + 1);
    
    std::vector<Vertex> &vert = qArray.vertices;
    
    int i = 0;
    
    /* Center */
    FloatRect texRect(0, 0, _width, _height);
    FloatRect posRect(0, 0, _width, _height);
    
    i += Quad::setTexPosRect(&vert[i*4], texRect, posRect);
    
    for (int j = 0; j < hNum; j++)
    {
        /* Upper */
        posRect = FloatRect(0, (int)ceil(j / 2.0f) * 2 * -_height, _width, (j % 2 ? 1 : -1) * _height);
        
        i += Quad::setTexPosRect(&vert[i*4], texRect, posRect);
        
        /* Lower */
        posRect = FloatRect(0, (int)ceil((j + 1) / 2.0f) * 2 * _height, _width, (j % 2 ? 1 : -1) * _height);
        
        i += Quad::setTexPosRect(&vert[i*4], texRect, posRect);
    }
    
    for (int j = 0; j < wNum; j++)
    {
        /* Left */
        posRect = FloatRect((int)ceil(j / 2.0f) * 2 * -_width, 0, (j % 2 ? 1 : -1) * _width, _height);
        
        i += Quad::setTexPosRect(&vert[i*4], texRect, posRect);
        
        /* Right */
        posRect = FloatRect((int)ceil((j + 1) / 2.0f) * 2 * _width, 0, (j % 2 ? 1 : -1) * _width, _height);
        
        i += Quad::setTexPosRect(&vert[i*4], texRect, posRect);
    }
    
    for (int i = 0; i < 4*qArray.count(); ++i)
        vert[i].color = Vec4(1, 1, 1, opacity);
    
    qArray.commit();
    
    TEXFBO newTex = shState->texPool().request(_width, _height);
    
    FBO::bind(newTex.fbo);
    
    glState.clearColor.pushSet(Vec4());
    FBO::clear();
    
    Transform trans;
    trans.setOrigin(Vec2(_width / 2.0f, _height / 2.0f));
    trans.setPosition(Vec2(_width / 2.0f, _height / 2.0f));
    
    glState.blendMode.pushSet(BlendAddition);
    
    SimpleMatrixShader &shader = shState->shaders().simpleMatrix;
    shader.bind();
    
    p->bindTexture(shader, false);
    TEX::setSmooth(true);
    
    p->pushSetViewport(shader);
    
    for (int i = 0; i < divisions; ++i)
    {
        trans.setRotation(baseAngle + i*angleStep);
        shader.setMatrix(trans.getMatrix());
        qArray.draw();
    }
    
    p->popViewport();
    
    TEX::setSmooth(false);
    
    glState.blendMode.pop();
    glState.clearColor.pop();
    
    shState->texPool().release(p->gl);
    p->gl = newTex;
    
    p->onModified();
}

void Bitmap::clear()
{
    guardDisposed();
    
    GUARD_ANIMATED;
    
    if (hasHires()) {
        p->selfHires->clear();
    }

    if (p->megaSurface)
    {
        SDL_Rect fRect = rect();
        SDL_FillRect(p->megaSurface, &fRect, 0);
    }
    else
    {
        p->bindFBO();
        
        glState.clearColor.pushSet(Vec4());
        
        FBO::clear();
        
        glState.clearColor.pop();
    }
    
    p->clearTaintedArea();
    
    p->onModified();
}

void Bitmap::createSurface() const
{
    if (p->surface)
        return;
    p->allocSurface();
    
    p->bindFBO();
    
    glState.viewport.pushSet(IntRect(0, 0, width(), height()));
    
    gl.ReadPixels(0, 0, width(), height(), GL_RGBA, GL_UNSIGNED_BYTE, p->surface->pixels);
    
    glState.viewport.pop();
}

Color Bitmap::getPixel(int x, int y) const
{
    guardDisposed();
    
    GUARD_ANIMATED;
    
    if (hasHires()) {
        Debug() << "GAME BUG: Game is calling getPixel on low-res Bitmap; you may want to patch the game to improve graphics quality.";

        int xHires = x * p->selfHires->width() / width();
        int yHires = y * p->selfHires->height() / height();

        // We take the average color from the high-res Bitmap.
        // RGB channels skip fully transparent pixels when averaging.
        int w = p->selfHires->width() / width();
        int h = p->selfHires->height() / height();

        if (w >= 1 && h >= 1) {
            double rSum = 0.;
            double gSum = 0.;
            double bSum = 0.;
            double aSum = 0.;

            long long rgbCount = 0;
            long long aCount = 0;

            for (int thisX = xHires; thisX < xHires+w && thisX < p->selfHires->width(); thisX++) {
                for (int thisY = yHires; thisY < yHires+h && thisY < p->selfHires->height(); thisY++) {
                    Color thisColor = p->selfHires->getPixel(thisX, thisY);
                    if (thisColor.getAlpha() >= 1.0) {
                        rSum += thisColor.getRed();
                        gSum += thisColor.getGreen();
                        bSum += thisColor.getBlue();
                        rgbCount++;
                    }
                    aSum += thisColor.getAlpha();
                    aCount++;
                }
            }

            double rAvg = rSum / (double)rgbCount;
            double gAvg = gSum / (double)rgbCount;
            double bAvg = bSum / (double)rgbCount;
            double aAvg = aSum / (double)aCount;

            return Color(rAvg, gAvg, bAvg, aAvg);
        }
    }

    if (x < 0 || y < 0 || x >= width() || y >= height())
        return Vec4();

    SDL_Surface *surf = nullptr;
    if (p->megaSurface)
        surf = p->megaSurface;
    else if (p->surface)
        surf = p->surface;
    else
    {
        createSurface();
        surf = p->surface;
    }
    
    uint32_t pixel = getPixelAt(surf, p->format, x, y);
    
    return Color((pixel >> p->format->Rshift) & 0xFF,
                 (pixel >> p->format->Gshift) & 0xFF,
                 (pixel >> p->format->Bshift) & 0xFF,
                 (pixel >> p->format->Ashift) & 0xFF);
}

void Bitmap::setPixel(int x, int y, const Color &color)
{
    guardDisposed();
    
    GUARD_ANIMATED;
    
    if (hasHires()) {
        Debug() << "GAME BUG: Game is calling setPixel on low-res Bitmap; you may want to patch the game to improve graphics quality.";

        int xHires = x * p->selfHires->width() / width();
        int yHires = y * p->selfHires->height() / height();

        int w = p->selfHires->width() / width();
        int h = p->selfHires->height() / height();

        if (w >= 1 && h >= 1) {
            for (int thisX = xHires; thisX < xHires+w && thisX < p->selfHires->width(); thisX++) {
                for (int thisY = yHires; thisY < yHires+h && thisY < p->selfHires->height(); thisY++) {
                    p->selfHires->setPixel(thisX, thisY, color);
                }
            }
        }
    }

    uint8_t pixel[] =
    {
        (uint8_t) clamp<double>(color.red,   0, 255),
        (uint8_t) clamp<double>(color.green, 0, 255),
        (uint8_t) clamp<double>(color.blue,  0, 255),
        (uint8_t) clamp<double>(color.alpha, 0, 255)
    };
    
    if (!p->megaSurface)
    {
        TEX::bind(p->gl.tex);
        TEX::uploadSubImage(x, y, 1, 1, &pixel, GL_RGBA);
    }
    
    p->addTaintedArea(IntRect(x, y, 1, 1));
    
    SDL_Surface *surf = nullptr;
    if (p->megaSurface)
        surf = p->megaSurface;
    else
    {
        /* Setting just a single pixel is no reason to throw away the
         * whole cached surface; we can just apply the same change */
        
        if (p->surface)
            surf = p->surface;
    }
    
    if (surf)
    {
        uint32_t &surfPixel = getPixelAt(surf, p->format, x, y);
        surfPixel = SDL_MapRGBA(p->format, pixel[0], pixel[1], pixel[2], pixel[3]);
    }
    
    p->onModified(false);
}

bool Bitmap::getRaw(void *output, int output_size)
{
    if (output_size != width()*height()*4) return false;
    
    guardDisposed();
    
    if (hasHires()) {
        Debug() << "GAME BUG: Game is calling getRaw on low-res Bitmap; you may want to patch the game to improve graphics quality.";
    }

    if (!p->animation.enabled && (p->surface || p->megaSurface)) {
        void *src = (p->megaSurface) ? p->megaSurface->pixels : p->surface->pixels;
        memcpy(output, src, output_size);
    }
    else {
        FBO::bind(getGLTypes().fbo);
        gl.ReadPixels(0,0,width(),height(),GL_RGBA,GL_UNSIGNED_BYTE,output);
    }
    return true;
}

void Bitmap::replaceRaw(void *pixel_data, int size)
{
    guardDisposed();
    
    if (hasHires()) {
        Debug() << "GAME BUG: Game is calling replaceRaw on low-res Bitmap; you may want to patch the game to improve graphics quality.";
    }

    int w = width();
    int h = height();
    int requiredsize = w*h*4;
    
    if (size != w*h*4)
        throw Exception(Exception::MKXPError, "Replacement bitmap data is not large enough (given %i bytes, need %i)", size, requiredsize);
    
    if (p->megaSurface)
    {
        // This should always be true
        if (p->megaSurface->format->BitsPerPixel == 32)
            memcpy(p->megaSurface->pixels, pixel_data, w*h*4);
    }
    else
    {
        TEX::bind(getGLTypes().tex);
        TEX::uploadImage(w, h, pixel_data, GL_RGBA);
    }
    
    taintArea(IntRect(0,0,w,h));
    p->onModified();
}

void Bitmap::saveToFile(const char *filename)
{
    guardDisposed();
    
    if (hasHires()) {
        Debug() << "GAME BUG: Game is calling saveToFile on low-res Bitmap; you may want to patch the game to improve graphics quality.";
    }

    SDL_Surface *surf;
    
    if (p->surface || p->megaSurface) {
        surf = (p->surface) ? p->surface : p->megaSurface;
    }
    else {
        surf = SDL_CreateRGBSurface(0, width(), height(),p->format->BitsPerPixel, p->format->Rmask,p->format->Gmask,p->format->Bmask,p->format->Amask);
        
        if (!surf)
            throw Exception(Exception::SDLError, "Failed to prepare bitmap for saving: %s", SDL_GetError());
        
        getRaw(surf->pixels, surf->w * surf->h * 4);
    }
    
    // Try and determine the intended image format from the filename extension
    const char *period = strrchr(filename, '.');
    int filetype = 0;
    if (period) {
        period++;
        std::string ext;
        for (int i = 0; i < (int)strlen(period); i++) {
            ext += tolower(period[i]);
        }
        
        if (!ext.compare("png")) {
            filetype = 1;
        }
        else if (!ext.compare("jpg") || !ext.compare("jpeg")) {
            filetype = 2;
        }
    }
    
    std::string fn_normalized = shState->fileSystem().normalize(filename, 1, 1);
    int rc;
    switch (filetype) {
        case 2:
            rc = IMG_SaveJPG(surf, fn_normalized.c_str(), 90);
            break;
        case 1:
            rc = IMG_SavePNG(surf, fn_normalized.c_str());
            break;
        case 0: default:
            rc = SDL_SaveBMP(surf, fn_normalized.c_str());
            break;
    }
    
    if (!p->surface && !p->megaSurface)
        SDL_FreeSurface(surf);
    
    if (rc) throw Exception(Exception::SDLError, "%s", SDL_GetError());
}

void Bitmap::hueChange(int hue)
{
    guardDisposed();
    
    GUARD_ANIMATED;
    
    if (hasHires()) {
        p->selfHires->hueChange(hue);
        return;
    }

    if ((hue % 360) == 0)
        return;
    
    if (p->megaSurface)
    {
        int widthMult = ceil((float) width() / glState.caps.maxTexSize);
        int tmpWidth = ceil((float) width() / widthMult);
        int heightMult = ceil((float) height() / glState.caps.maxTexSize);
        int tmpHeight = ceil((float) height() / heightMult);
        
        Bitmap *tmp = new Bitmap(tmpWidth, tmpHeight, true);
        IntRect sourceRect = {0, 0, tmpWidth, tmpHeight};
        
        pixman_region16_t originalTainted;
        pixman_region32_t originalTainted32;
        if (p->pixmanUseRegion32)
        {
            pixman_region32_init(&originalTainted32);
            pixman_region32_copy(&originalTainted32, &p->tainted32);
        }
        else
        {
            pixman_region_init(&originalTainted);
            pixman_region_copy(&originalTainted, &p->tainted);
        }
        for (int i = 0; i < widthMult; i++)
        {
            for (int j = 0; j < heightMult; j++)
            {
                tmp->clear();
                p->clearTaintedArea();
                sourceRect.x = tmpWidth * i;
                sourceRect.y = tmpHeight * j;
                tmp->stretchBlt(tmp->rect(), *this, sourceRect, 255);
                tmp->hueChange(hue);
                stretchBlt(sourceRect, *tmp, tmp->rect(), 255);
            }
        }
        delete tmp;
        p->clearTaintedArea();
        if (p->pixmanUseRegion32)
        {
            pixman_region32_copy(&p->tainted32, &originalTainted32);
            pixman_region32_fini(&originalTainted32);
        }
        else
        {
            pixman_region_copy(&p->tainted, &originalTainted);
            pixman_region_fini(&originalTainted);
        }
    }
    else
    {
        TEXFBO newTex = shState->texPool().request(width(), height());
        
        FloatRect texRect(rect());
        
        Quad &quad = shState->gpQuad();
        quad.setTexPosRect(texRect, texRect);
        quad.setColor(Vec4(1, 1, 1, 1));
        
        HueShader &shader = shState->shaders().hue;
        shader.bind();
        /* Shader expects normalized value */
        shader.setHueAdjust(wrapRange(hue, 0, 360) / 360.0f);
        
        FBO::bind(newTex.fbo);
        p->pushSetViewport(shader);
        p->bindTexture(shader, false);
        
        p->blitQuad(quad);
        
        p->popViewport();
        
        TEX::unbind();
        
        shState->texPool().release(p->gl);
        p->gl = newTex;
    }
    
    p->onModified();
}

void Bitmap::drawText(int x, int y,
                      int width, int height,
                      const char *str, int align)
{
    drawText(IntRect(x, y, width, height), str, align);
}

static std::string fixupString(const char *str)
{
    std::string s(str);
    
    /* RMXP actually draws LF as a "missing gylph" box,
     * but since we might have accidentally converted CRs
     * to LFs when editing scripts on a Unix OS, treat them
     * as white space too */
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i] == '\r' || s[i] == '\n')
            s[i] = ' ';
    
    return s;
}

static void applyShadow(SDL_Surface *&in, const SDL_PixelFormat &fm, const SDL_Color &c)
{
    SDL_Surface *out = SDL_CreateRGBSurface
    (0, in->w+1, in->h+1, fm.BitsPerPixel, fm.Rmask, fm.Gmask, fm.Bmask, fm.Amask);
    
    float fr = c.r / 255.0f;
    float fg = c.g / 255.0f;
    float fb = c.b / 255.0f;
    
    /* We allocate an output surface one pixel wider and higher than the input,
     * (implicitly) blit a copy of the input with RGB values set to black into
     * it with x/y offset by 1, then blend the input surface over it at origin
     * (0,0) using the bitmap blit equation (see shader/bitmapBlit.frag) */
    
    for (int y = 0; y < in->h+1; ++y)
        for (int x = 0; x < in->w+1; ++x)
        {
            /* src: input pixel, shd: shadow pixel */
            uint32_t src = 0, shd = 0;
            
            /* Output pixel location */
            uint32_t *outP = ((uint32_t*) ((uint8_t*) out->pixels + y*out->pitch)) + x;
            
            if (y < in->h && x < in->w)
                src = ((uint32_t*) ((uint8_t*) in->pixels + y*in->pitch))[x];
            
            if (y > 0 && x > 0)
                shd = ((uint32_t*) ((uint8_t*) in->pixels + (y-1)*in->pitch))[x-1];
            
            /* Set shadow pixel RGB values to 0 (black) */
            shd &= fm.Amask;
            
            if (x == 0 || y == 0)
            {
                *outP = src;
                continue;
            }
            
            if (x == in->w || y == in->h)
            {
                *outP = shd;
                continue;
            }
            
            /* Input and shadow alpha values */
            uint8_t srcA, shdA;
            srcA = (src & fm.Amask) >> fm.Ashift;
            shdA = (shd & fm.Amask) >> fm.Ashift;
            
            if (srcA == 255 || shdA == 0)
            {
                *outP = src;
                continue;
            }
            
            if (srcA == 0 && shdA == 0)
            {
                *outP = 0;
                continue;
            }
            
            float fSrcA = srcA / 255.0f;
            float fShdA = shdA / 255.0f;
            
            /* Because opacity == 1, co1 == fSrcA */
            float co2 = fShdA * (1.0f - fSrcA);
            /* Result alpha */
            float fa = fSrcA + co2;
            /* Temp value to simplify arithmetic below */
            float co3 = fSrcA / fa;
            
            /* Result colors */
            uint8_t r, g, b, a;
            
            r = clamp<float>(fr * co3, 0, 1) * 255.0f;
            g = clamp<float>(fg * co3, 0, 1) * 255.0f;
            b = clamp<float>(fb * co3, 0, 1) * 255.0f;
            a = clamp<float>(fa, 0, 1) * 255.0f;
            
            *outP = SDL_MapRGBA(&fm, r, g, b, a);
        }
    
    /* Store new surface in the input pointer */
    SDL_FreeSurface(in);
    in = out;
}

void Bitmap::drawText(const IntRect &rect, const char *str, int align)
{
    guardDisposed();
    
    GUARD_ANIMATED;
    
    // RGSS doesn't let you draw text backwards
    if (rect.w <= 0 || rect.h <= 0 || rect.x >= width() || rect.y >= height() ||
        rect.w < -rect.x || rect.h < -rect.y)
        return;
    
    if (hasHires()) {
        Font &loresFont = getFont();
        Font &hiresFont = p->selfHires->getFont();
        // Disable the illegal font size check when creating a high-res font.
        hiresFont.setSize(loresFont.getSize() * p->selfHires->width() / width(), false);
        hiresFont.setBold(loresFont.getBold());
        hiresFont.setColor(loresFont.getColor());
        hiresFont.setItalic(loresFont.getItalic());
        hiresFont.setShadow(loresFont.getShadow());
        hiresFont.setOutline(loresFont.getOutline());
        hiresFont.setOutColor(loresFont.getOutColor());

        int rectX = rect.x * p->selfHires->width() / width();
        int rectY = rect.y * p->selfHires->height() / height();
        int rectWidth = rect.w * p->selfHires->width() / width();
        int rectHeight = rect.h * p->selfHires->height() / height();

        p->selfHires->drawText(IntRect(rectX, rectY, rectWidth, rectHeight), str, align);
    }

    std::string fixed = fixupString(str);
    str = fixed.c_str();
    
    if (*str == '\0')
        return;
    
    if (str[0] == ' ' && str[1] == '\0')
        return;
    
    TTF_Font *font = p->font->getSdlFont();
    const Color &fontColor = p->font->getColor();
    const Color &outColor = p->font->getOutColor();
    
    // RGSS crops the the text slightly if there's an outline
    int scaledOutlineSize = 0;
    if (p->font->getOutline()) {
        // Handle high-res for outline.
        if (p->selfLores) {
            scaledOutlineSize = OUTLINE_SIZE * width() / p->selfLores->width();
        } else {
            scaledOutlineSize = OUTLINE_SIZE;
        }
    }
    int doubleOutlineSize = scaledOutlineSize * 2;
    
    SDL_Color c = fontColor.toSDLColor();
    int txtAlpha;
    if(scaledOutlineSize)
    {
        c.a = 255;
        txtAlpha = fontColor.alpha;
    }
    else
    {
        txtAlpha = 255;
    }
    
    // Trim the text to only fill double the rect width
    int charLimit = 0;
    float squeezeLimit = 0.5f;
    if (TTF_MeasureUTF8(font, str, std::min(width() - rect.x, rect.w) / squeezeLimit, &charLimit, nullptr) == 0)
    {
        fixed = fixed.substr(0, charLimit + 1);
        str = fixed.c_str();
    }
    
    SDL_Surface *txtSurf;
    
    if (p->font->isSolid())
        txtSurf = TTF_RenderUTF8_Solid(font, str, c);
    else
        txtSurf = TTF_RenderUTF8_Blended(font, str, c);
    
    p->ensureFormat(txtSurf, SDL_PIXELFORMAT_ABGR8888);
    
    if (p->font->getShadow())
        applyShadow(txtSurf, *p->format, c);
    
    int alignX = rect.x;
    
    switch (align)
    {
        default:
        case Left :
            break;
            
        case Center :
            alignX += ceil((rect.w - (txtSurf->w + doubleOutlineSize)) / 2.0f);
            break;
            
        case Right :
            alignX += rect.w - txtSurf->w;
            break;
    }
    
    if (alignX < rect.x)
        alignX = rect.x;
    
    int alignY = rect.y + (rect.h - txtSurf->h) / 2;
    
    alignY = std::max(alignY, rect.y);
    
    /* FIXME: RGSS begins squeezing the text before it fills the rect.
     * While this is extremely undesirable, a number of games will understandably
     * have made the rects bigger to compensate, so we should probably match it */
    float squeeze = (float) rect.w / txtSurf->w;
    
    squeeze = clamp(squeeze, squeezeLimit, 1.0f);
    
    /* outline using TTF_Outline and blending it together with SDL_BlitSurface
     * FIXME: RGSS's "outline" includes a complete set of text behind the regular text
     * which loses opacity at a different rate than you'd expect.
     * I gave up on trying to figure out the algorithm, so our transparent text will
     * generally be paler than it should be. */
    if (scaledOutlineSize)
    {
        SDL_Color co = outColor.toSDLColor();
        SDL_Surface *outline;
        /* set the next font render to render the outline */
        TTF_SetFontOutline(font, scaledOutlineSize);
        if (p->font->isSolid())
            outline = TTF_RenderUTF8_Solid(font, str, co);
        else
            outline = TTF_RenderUTF8_Blended(font, str, co);
        
        p->ensureFormat(outline, SDL_PIXELFORMAT_ABGR8888);
        SDL_Rect inRect = {scaledOutlineSize, scaledOutlineSize,
                           (int)(rect.w / squeeze) - doubleOutlineSize, rect.h - doubleOutlineSize};
        SDL_Rect outRect = {doubleOutlineSize, doubleOutlineSize, txtSurf->w, txtSurf->h};
        
        SDL_SetSurfaceBlendMode(txtSurf, SDL_BLENDMODE_BLEND);
        SDL_BlitSurface(txtSurf, &inRect, outline, &outRect);
        SDL_FreeSurface(txtSurf);
        txtSurf = outline;
        /* reset outline to 0 */
        TTF_SetFontOutline(font, 0);
    }
    
    IntRect destRect(alignX, alignY,
                    std::min(rect.w, (int)(txtSurf->w * squeeze)),
                    std::min(rect.h, txtSurf->h));
    
    destRect.w = std::min(destRect.w, width() - destRect.x);
    destRect.h = std::min(destRect.h, height() - destRect.y);
    
    IntRect sourceRect(scaledOutlineSize, scaledOutlineSize, destRect.w / squeeze, destRect.h);
    
    Bitmap *txtBitmap = new Bitmap(txtSurf, nullptr, true);
    TEX::setSmooth(true);
    stretchBlt(destRect, *txtBitmap, sourceRect, txtAlpha);
    TEX::setSmooth(false);
    delete txtBitmap;
}

/* http://www.lemoda.net/c/utf8-to-ucs2/index.html */
static uint16_t utf8_to_ucs2(const char *_input,
                             const char **end_ptr)
{
    const unsigned char *input =
    reinterpret_cast<const unsigned char*>(_input);
    *end_ptr = _input;
    
    if (input[0] == 0)
        return -1;
    
    if (input[0] < 0x80)
    {
        *end_ptr = _input + 1;
        
        return input[0];
    }
    
    if ((input[0] & 0xE0) == 0xE0)
    {
        if (input[1] == 0 || input[2] == 0)
            return -1;
        
        *end_ptr = _input + 3;
        
        return (input[0] & 0x0F)<<12 |
        (input[1] & 0x3F)<<6  |
        (input[2] & 0x3F);
    }
    
    if ((input[0] & 0xC0) == 0xC0)
    {
        if (input[1] == 0)
            return -1;
        
        *end_ptr = _input + 2;
        
        return (input[0] & 0x1F)<<6  |
        (input[1] & 0x3F);
    }
    
    return -1;
}

IntRect Bitmap::textSize(const char *str)
{
    guardDisposed();
    
    GUARD_ANIMATED;
    
    // TODO: High-res Bitmap textSize not implemented, but I think it's the same as low-res?
    // Need to double-check this.

    TTF_Font *font = p->font->getSdlFont();
    
    std::string fixed = fixupString(str);
    str = fixed.c_str();
    
    int w, h;
    TTF_SizeUTF8(font, str, &w, &h);
    
    /* If str is one character long, *endPtr == 0 */
    const char *endPtr;
    uint16_t ucs2 = utf8_to_ucs2(str, &endPtr);
    
    /* For cursive characters, returning the advance
     * as width yields better results */
    if (p->font->getItalic() && *endPtr == '\0')
        TTF_GlyphMetrics(font, ucs2, 0, 0, 0, 0, &w);
    
    return IntRect(0, 0, w, h);
}

DEF_ATTR_RD_SIMPLE(Bitmap, Font, Font&, *p->font)

void Bitmap::setFont(Font &value)
{
    // High-res support handled in drawText, not here.
    *p->font = value;
}

void Bitmap::setInitFont(Font *value)
{
    if (hasHires()) {
        Font *hiresFont = new Font(*value);
        // Disable the illegal font size check when creating a high-res font.
        hiresFont->setSize(hiresFont->getSize() * p->selfHires->width() / width(), false);
        p->selfHires->setInitFont(hiresFont);
    }

    p->font = value;
}

TEXFBO &Bitmap::getGLTypes() const
{
    return p->getGLTypes();
}

SDL_Surface *Bitmap::surface() const
{
    return p->surface;
}

SDL_Surface *Bitmap::megaSurface() const
{
    return p->megaSurface;
}

void Bitmap::ensureNonMega() const
{
    if (isDisposed())
        return;
    
    GUARD_MEGA;
}

void Bitmap::ensureNonAnimated() const
{
    if (isDisposed())
        return;
    
    GUARD_ANIMATED;
}

void Bitmap::ensureAnimated() const
{
    if (isDisposed())
        return;
    
    GUARD_UNANIMATED;
}

void Bitmap::stop()
{
    guardDisposed();
    
    GUARD_UNANIMATED;
    if (!p->animation.playing) return;
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap stop not implemented";
    }

    p->animation.stop();
}

void Bitmap::play()
{
    guardDisposed();
    
    GUARD_UNANIMATED;
    if (p->animation.playing) return;

    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap play not implemented";
    }

    p->animation.play();
}

bool Bitmap::isPlaying() const
{
    guardDisposed();
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap isPlaying not implemented";
    }

    if (!p->animation.playing)
        return false;
    
    if (p->animation.loop)
        return true;
    
    return p->animation.currentFrameIRaw() < p->animation.frames.size();
}

void Bitmap::gotoAndStop(int frame)
{
    guardDisposed();
    
    GUARD_UNANIMATED;
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap gotoAndStop not implemented";
    }

    p->animation.stop();
    p->animation.seek(frame);
}
void Bitmap::gotoAndPlay(int frame)
{
    guardDisposed();
    
    GUARD_UNANIMATED;
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap gotoAndPlay not implemented";
    }

    p->animation.stop();
    p->animation.seek(frame);
    p->animation.play();
}

int Bitmap::numFrames() const
{
    guardDisposed();
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap numFrames not implemented";
    }

    if (!p->animation.enabled) return 1;
    return (int)p->animation.frames.size();
}

int Bitmap::currentFrameI() const
{
    guardDisposed();
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap currentFrameI not implemented";
    }

    if (!p->animation.enabled) return 0;
    return p->animation.currentFrameI();
}

int Bitmap::addFrame(Bitmap &source, int position)
{
    guardDisposed();
    
    GUARD_MEGA;
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap addFrame dest not implemented";
    }

    if (source.hasHires()) {
        Debug() << "BUG: High-res Bitmap addFrame source not implemented";
    }

    if (source.height() != height() || source.width() != width())
        throw Exception(Exception::MKXPError, "Animations with varying dimensions are not supported (%ix%i vs %ix%i)",
                        source.width(), source.height(), width(), height());
    
    TEXFBO newframe = shState->texPool().request(source.width(), source.height());
    
    // Convert the bitmap into an animated bitmap if it isn't already one
    if (!p->animation.enabled) {
        p->animation.width = p->gl.width;
        p->animation.height = p->gl.height;
        p->animation.enabled = true;
        p->animation.lastFrame = 0;
        p->animation.playTime = 0;
        p->animation.startTime = 0;
        
        if (p->animation.fps <= 0)
            p->animation.fps = shState->graphics().getFrameRate();
        
        p->animation.frames.push_back(p->gl);
        
        if (p->surface)
        {
            SDL_FreeSurface(p->surface);
            p->surface = 0;
        }
        p->gl = TEXFBO();
    }
    
    if (source.surface()) {
        TEX::bind(newframe.tex);
        TEX::uploadImage(source.width(), source.height(), source.surface()->pixels, GL_RGBA);
        SDL_FreeSurface(p->surface);
        p->surface = 0;
    }
    else {
        GLMeta::blitBegin(newframe);
        GLMeta::blitSource(source.getGLTypes());
        GLMeta::blitRectangle(rect(), rect(), true);
        GLMeta::blitEnd();
    }
    
    int ret;
    
    if (position < 0) {
        p->animation.frames.push_back(newframe);
        ret = (int)p->animation.frames.size();
    }
    else {
        p->animation.frames.insert(p->animation.frames.begin() + clamp(position, 0, (int)p->animation.frames.size()), newframe);
        ret = position;
    }
    
    return ret;
}

void Bitmap::removeFrame(int position) {
    guardDisposed();
    
    GUARD_UNANIMATED;
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap removeFrame not implemented";
    }

    int pos = (position < 0) ? (int)p->animation.frames.size() - 1 : clamp(position, 0, (int)(p->animation.frames.size() - 1));
    shState->texPool().release(p->animation.frames[pos]);
    p->animation.frames.erase(p->animation.frames.begin() + pos);
    
    // Change the animated bitmap back to a normal one if there's only one frame left
    if (p->animation.frames.size() == 1) {
        
        p->animation.enabled = false;
        p->animation.playing = false;
        p->animation.width = 0;
        p->animation.height = 0;
        p->animation.lastFrame = 0;
        
        p->gl = p->animation.frames[0];
        p->animation.frames.erase(p->animation.frames.begin());
        
        FBO::bind(p->gl.fbo);
        taintArea(rect());
    }
}

void Bitmap::nextFrame()
{
    guardDisposed();
    
    GUARD_UNANIMATED;
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap nextFrame not implemented";
    }

    stop();
    if ((uint32_t)p->animation.lastFrame >= p->animation.frames.size() - 1)  {
        if (!p->animation.loop) return;
        p->animation.lastFrame = 0;
        return;
    }
    
    p->animation.lastFrame++;
}

void Bitmap::previousFrame()
{
    guardDisposed();
    
    GUARD_UNANIMATED;
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap previousFrame not implemented";
    }

    stop();
    if (p->animation.lastFrame <= 0) {
        if (!p->animation.loop) {
            p->animation.lastFrame = 0;
            return;
        }
        p->animation.lastFrame = (int)p->animation.frames.size() - 1;
        return;
    }
    
    p->animation.lastFrame--;
}

void Bitmap::setAnimationFPS(float FPS)
{
    guardDisposed();
    
    GUARD_MEGA;
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap setAnimationFPS not implemented";
    }

    bool restart = p->animation.playing;
    p->animation.stop();
    p->animation.fps = (FPS < 0) ? 0 : FPS;
    if (restart) p->animation.play();
}

std::vector<TEXFBO> &Bitmap::getFrames() const
{
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap getFrames not implemented";
    }

    return p->animation.frames;
}

float Bitmap::getAnimationFPS() const
{
    guardDisposed();
    
    GUARD_MEGA;
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap getAnimationFPS not implemented";
    }

    return p->animation.fps;
}

void Bitmap::setLooping(bool loop)
{
    guardDisposed();
    
    GUARD_MEGA;
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap setLooping not implemented";
    }

    p->animation.loop = loop;
}

bool Bitmap::getLooping() const
{
    guardDisposed();
    
    GUARD_MEGA;
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap getLooping not implemented";
    }

    return p->animation.loop;
}

void Bitmap::bindTex(ShaderBase &shader)
{
    // Hires mode is handled by p->bindTexture.

    p->bindTexture(shader);
}

void Bitmap::taintArea(const IntRect &rect)
{
    if (hasHires()) {
        int destX, destY, destWidth, destHeight;
        destX = rect.x * p->selfHires->width() / width();
        destY = rect.y * p->selfHires->height() / height();
        destWidth = rect.w * p->selfHires->width() / width();
        destHeight = rect.h * p->selfHires->height() / height();

        p->selfHires->taintArea(IntRect(destX, destY, destWidth, destHeight));
    }

    p->addTaintedArea(rect);
}

int Bitmap::maxSize(){
    return glState.caps.maxTexSize;
}

void Bitmap::assumeRubyGC()
{
    p->assumingRubyGC = true;
}

void Bitmap::releaseResources()
{
    if (p->selfHires && !p->assumingRubyGC) {
        delete p->selfHires;
    }

    if (p->megaSurface)
        SDL_FreeSurface(p->megaSurface);
    if (p->surface)
        SDL_FreeSurface(p->surface);
    else if (p->animation.enabled) {
        p->animation.enabled = false;
        p->animation.playing = false;
        for (TEXFBO &tex : p->animation.frames)
            shState->texPool().release(tex);
    }
    else
        shState->texPool().release(p->gl);
    
    if (p->pChild)
    {
        delete p->pChild;
    }
    
    delete p;
}
