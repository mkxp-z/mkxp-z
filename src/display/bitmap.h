/*
** bitmap.h
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

#ifndef BITMAP_H
#define BITMAP_H

#ifdef MKXPZ_RETRO
#  include <ft2build.h>
#  include FT_FREETYPE_H
#  include "wasm-types.h"
#endif // MKXPZ_RETRO

#include "disposable.h"
#include "etc-internal.h"
#include "etc.h"

#include "gl-util.h"

#include <string>
#include <vector>

#include "sigslot/signal.hpp"

class Font;
class ShaderBase;
struct SDL_Surface;

struct BitmapFrame
{
    TEXFBO gl;
#ifdef MKXPZ_RETRO
    std::vector<std::vector<uint32_t>> diff;
    std::string path;
    int originalFrameIndex;
#endif // MKXPZ_RETRO
};

struct BitmapPrivate;

struct ChildPublic
{
    // The real offset and zoom. Initialized to -1.0f, to determine if it's a Window.
    Vec2i realOffset;
    Vec2 realZoom;
    
    // The effective offset and zoom, after adjusting for the child's size, position, and shrinkage.
    Vec2 offset;
    Vec2 zoom;
    
    // The window's dimensions. Used by Windows.
    int width;
    int height;
    
    // Needed for Sprites, initialized to the parent's dimensions and used by everything.
    IntRect realSrcRect;
    IntRect srcRect;
    
    enum {
        NONE,
        PLANE,
        SPRITE,
        WINDOW,
        WINDOWVX,
    } sceneElementType;
    void *sceneElement;
    
    // The Sprite or Window's position, for modifying the offset and as the origin for rotations.
    // Also used for Planes instead of realOffset, due to how zooming interacts with it.
    // (Planes still output to offset, though)
    int x;
    int y;
    // Should the child wrap around. Only used by Planes.
    bool wrap;
    
    // Will the child be mirrored. Used by Sprites.
    bool mirrored;
    
    // Used by Sprites.
    float angle;
    int waveAmp;
    
    // If the child won't even be visible, then we can skip all drawing operations for it.
    bool isVisible;
    
    ChildPublic()
    :
    width(0),
    height(0),
    x(0),
    y(0),
    sceneElementType(NONE),
    sceneElement(nullptr),
    wrap(false),
    mirrored(false),
    angle(0),
    waveAmp(0),
    isVisible(true)
    {
    	realZoom.x = realZoom.y = -1.0f;
    	zoom.x = zoom.y = -1.0f;
    }

    // sceneRect is the viewport, used for determining what's actually visible.
    const IntRect *sceneRect() const noexcept;

    // sceneOrig is the viewport's offset, and functions similarly to x/y.
    const Vec2i *sceneOrig() const noexcept;
};

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
    Vec2 currentShrink;
    bool mirrored;
    IntRect oldVR;
    Vec2i oldOff;

    ChildPrivate();
    ~ChildPrivate();
    void init(Bitmap *self, Bitmap *parent);
    void childDirty();
    void parentDisposed();

#ifdef MKXPZ_RETRO
    bool sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const;
    bool sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size);
    void sandbox_deserialize_begin();
    void sandbox_deserialize_end();
#endif // MKXPZ_RETRO
};

// FIXME make this class use proper RGSS classes again
class Bitmap : public Disposable
{
	friend struct BitmapPrivate;
	friend class Plane;
	friend struct PlanePrivate;
	friend class Sprite;
	friend struct SpritePrivate;
	friend struct TilemapPrivate;
	friend struct TilemapVXPrivate;
	friend class Window;
	friend struct WindowPrivate;
	friend class WindowVX;
	friend struct WindowVXPrivate;
	friend struct ChildPrivate;

public:
	Bitmap(Exception &exception, const char *filename, bool useDiff = true);
	Bitmap(Exception &exception, int width = 1, int height = 1, bool isHires = false, bool useDiff = true);
	Bitmap(Exception &exception, void *pixeldata, int width, int height, bool useDiff = true);
	Bitmap(Exception &exception, TEXFBO &other, bool useDiff = true);
	Bitmap(Exception &exception, SDL_Surface *imgSurf, SDL_Surface *imgSurfHires, bool forceMega = false, bool useDiff = true);

	/* Clone constructor */
    
    // frame is -2 for "any and all", -1 for "current", anything else for a specific frame
	Bitmap(Exception &exception, const Bitmap &other, int frame = -2, bool useDiff = true);
	~Bitmap();

	void initFromFilename(Exception &exception, const char *filename, bool useDiff = true);
	void initFromDimensions(Exception &exception, int width = 1, int height = 1, bool isHires = false, bool useDiff = true);
	void initFromSurface(Exception &exception, SDL_Surface *imgSurf, Bitmap *hiresBitmap, bool forceMega = false, bool useDiff = true);

	Bitmap *spawnChild(Exception &exception);
	ChildPublic *getChildInfo();
	void childUpdate(Exception &exception);

	int getWidth(Exception &exception)  const;
	int getHeight(Exception &exception) const;
	bool getHasHires(Exception &exception) const;
	void setHiresRaw(Exception &exception, Bitmap *hires);
	DECL_ATTR(Hires, Bitmap*)
	void setLoresRaw(Exception &exception, Bitmap *lores);
	void setLores(Exception &exception, Bitmap *lores);
	bool getIsMega(Exception &exception) const;
	bool getIsAnimated(Exception &exception) const;
	IntRect getRect(Exception &exception) const;

	void blt(Exception &exception,
	         int x, int y,
	         const Bitmap &source, const IntRect &rect,
	         int opacity = 255);

	enum BitmapBltMode {
	    NORMAL,
	    KGL_SUBTRACT,
	};

	void stretchBlt(Exception &exception,
	                IntRect destRect,
	                const Bitmap &source, IntRect sourceRect,
	                int opacity = 255, bool smooth = false,
			enum BitmapBltMode mode = NORMAL);

	void fillRect(Exception &exception,
	              int x, int y,
	              int width, int height,
	              const Vec4 &color);
	void fillRect(Exception &exception, const IntRect &rect, const Vec4 &color);

	void gradientFillRect(Exception &exception,
	                      int x, int y,
	                      int width, int height,
	                      const Vec4 &color1, const Vec4 &color2,
	                      bool vertical = false);
	void gradientFillRect(Exception &exception,
	                      const IntRect &rect,
	                      const Vec4 &color1, const Vec4 &color2,
	                      bool vertical = false);

	void clearRect(Exception &exception,
	               int x, int y,
	               int width, int height);
	void clearRect(Exception &exception,
	               const IntRect &rect);

	void blur(Exception &exception);
	void radialBlur(Exception &exception, int angle, int divisions);

	void clear(Exception &exception);

	/* Creates a surface and assigns it to p->surface */
	void createSurface() const;

	Color getPixel(Exception &exception, int x, int y) const;
	void setPixel(Exception &exception, int x, int y, const Color &color);
    
    bool getRaw(Exception &exception, void *output, int output_size);
    void replaceRaw(Exception &exception, void *pixel_data, int size);
    void saveToFile(Exception &exception, const char *filename);

	void hueChange(Exception &exception, int hue);

	enum TextAlign
	{
		Left = 0,
		Center = 1,
		Right = 2
	};

	void drawText(Exception &exception,
	              int x, int y,
	              int width, int height,
	              const char *str, int align = Left);

	void drawText(Exception &exception,
	              const IntRect &rect,
	              const char *str, int align = Left);

	IntRect textSize(Exception &exception, const char *str);

	Font &getFont(Exception &exception) const;
	void setFont(Font &value);

	/* Sets initial reference without copying by value,
	 * use at construction */
	void setInitFont(Font *value);

	/* <internal> */
	TEXFBO &getGLTypes() const;
    SDL_Surface *surface() const;
	SDL_Surface *megaSurface() const;
	void ensureNonMega(Exception &exception) const;
    void ensureNonAnimated(Exception &exception) const;
    void ensureAnimated(Exception &exception) const;
    
    // Animation functions
    void stop(Exception &exception);
    void play(Exception &exception);
    bool isPlaying(Exception &exception) const;
    bool getPlaying(Exception &exception) const;
    void setPlaying(Exception &exception, bool playing);
    void gotoAndStop(Exception &exception, int frame);
    void gotoAndPlay(Exception &exception, int frame);
    int numFrames(Exception &exception) const;
    int currentFrameI(Exception &exception) const;
    
    int addFrame(Exception &exception, Bitmap &source, int position = -1);
    void removeFrame(Exception &exception, int position = -1);
    
    void nextFrame(Exception &exception);
    void previousFrame(Exception &exception);
    std::vector<BitmapFrame> &getFrames() const;
    
    void setAnimationFPS(Exception &exception, float FPS);
    float getAnimationFPS(Exception &exception) const;
    
    void setLooping(Exception &exception, bool loop);
    bool getLooping(Exception &exception) const;

    void ensureNotPlaying(Exception &exception) const;

    void kglInvert(Exception &exception);
    void kglCompressAlpha(Exception &exception);
    int kglShadowShaderH(Exception &exception, int x1, int x2, int y, bool soft);
    int kglShadowShaderV(Exception &exception, int y1, int y2, int x, bool wall, bool soft);

    // ----------
    
	/* Binds the backing texture and sets the correct
	 * texture size uniform in shader */
	void bindTex(ShaderBase &shader, bool substituteLoresSize = true);

	/* Adds 'rect' to tainted area */
	void taintArea(const IntRect &rect);

	sigslot::signal<> modified;
#ifdef MKXPZ_RETRO
	const uint64_t id; // Globally unique nonzero ID for this bitmap, for change detection during save state deserialization
	bool deserModified;
	bool deserSizeChanged;
#endif // MKXPZ_RETRO

	static int maxSize();

    void assumeRubyGC(bool value = true);

#ifdef MKXPZ_RETRO
	static void syncDiffs();
	bool sandbox_serialize_without_hires(void *&data, mkxp_sandbox::wasm_size_t &max_size) const;
	bool sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const;
	bool sandbox_deserialize_without_hires(const void *&data, mkxp_sandbox::wasm_size_t &max_size);
	bool sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size);
	void sandbox_deserialize_begin(bool is_new);
	void sandbox_deserialize_end(bool is_sandbox_object);
	void sandbox_reinit();
#endif // MKXPZ_RETRO

private:
	int width()  const;
	int height() const;
	bool hasHires() const;
	bool isMega() const;
	bool isAnimated() const;
	IntRect rect() const;
	float animationFPS() const;
	bool looping() const;

	void releaseResources();
	sigslot::connection loresDispCon;
	const char *klassName() const { return "bitmap"; }

	BitmapPrivate *p;

	void loresDisposal();

#ifdef MKXPZ_RETRO
	SDL_Surface *drawTextInner(FT_Face font, const char *str, SDL_Color &c, size_t outline);
	bool sandbox_serialize_pixels(void *&data, mkxp_sandbox::wasm_size_t &max_size, const std::vector<std::vector<uint32_t>> &diff) const;
	bool sandbox_deserialize_pixels_check_need_reload(const void *&data, mkxp_sandbox::wasm_size_t &max_size, const std::vector<std::vector<uint32_t>> &diff, bool &need_reload, bool &need_reload_if_path_not_empty, bool modify_data_and_max_size) const;
	bool sandbox_deserialize_pixels(const void *&data, mkxp_sandbox::wasm_size_t &max_size, std::vector<std::vector<uint32_t>> &diff, mkxp_sandbox::wasm_size_t frame_number = 0);
#endif // MKXPZ_RETRO
};

#endif // BITMAP_H
