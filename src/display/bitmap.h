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

#include "disposable.h"
#include "etc-internal.h"
#include "etc.h"

#include "sigslot/signal.hpp"

class Font;
class ShaderBase;
struct TEXFBO;
struct SDL_Surface;

struct BitmapPrivate;
struct ChildPrivate;
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
    
    // sceneRect is the viewport, used for determining what's actually visible.
    // sceneOrig is the viewport's offset, and functions similarly to x/y.
    const IntRect *sceneRect;
    const Vec2i *sceneOrig;
    
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
    sceneRect(0),
    sceneOrig(0),
    wrap(false),
    mirrored(false),
    angle(0),
    waveAmp(0),
    isVisible(true)
    {
    	realZoom.x = realZoom.y = -1.0f;
    	zoom.x = zoom.y = -1.0f;
    }
};


// FIXME make this class use proper RGSS classes again
class Bitmap : public Disposable
{
public:
	Bitmap(const char *filename);
	Bitmap(int width, int height, bool isHires = false);
	Bitmap(void *pixeldata, int width, int height);
	Bitmap(TEXFBO &other);
	Bitmap(SDL_Surface *imgSurf, SDL_Surface *imgSurfHires, bool forceMega = false);

	/* Clone constructor */
    
    // frame is -2 for "any and all", -1 for "current", anything else for a specific frame
	Bitmap(const Bitmap &other, int frame = -2);
	~Bitmap();

	void initFromSurface(SDL_Surface *imgSurf, Bitmap *hiresBitmap, bool forceMega = false);

	Bitmap *spawnChild();
	ChildPublic *getChildInfo();
	void childUpdate();

	int width()  const;
	int height() const;
	bool hasHires() const;
	DECL_ATTR(Hires, Bitmap*)
	void setLores(Bitmap *lores);
	bool isMega() const;
    bool isAnimated() const;

	IntRect rect() const;

	void blt(int x, int y,
	         const Bitmap &source, const IntRect &rect,
	         int opacity = 255);

	void stretchBlt(IntRect destRect,
	                const Bitmap &source, IntRect sourceRect,
	                int opacity = 255);

	void fillRect(int x, int y,
	              int width, int height,
	              const Vec4 &color);
	void fillRect(const IntRect &rect, const Vec4 &color);

	void gradientFillRect(int x, int y,
	                      int width, int height,
	                      const Vec4 &color1, const Vec4 &color2,
	                      bool vertical = false);
	void gradientFillRect(const IntRect &rect,
	                      const Vec4 &color1, const Vec4 &color2,
	                      bool vertical = false);

	void clearRect(int x, int y,
	               int width, int height);
	void clearRect(const IntRect &rect);

	void blur();
	void radialBlur(int angle, int divisions);

	void clear();

	/* Creates a surface and assigns it to p->surface */
	void createSurface() const;

	Color getPixel(int x, int y) const;
	void setPixel(int x, int y, const Color &color);
    
    bool getRaw(void *output, int output_size);
    void replaceRaw(void *pixel_data, int size);
    void saveToFile(const char *filename);

	void hueChange(int hue);

	enum TextAlign
	{
		Left = 0,
		Center = 1,
		Right = 2
	};

	void drawText(int x, int y,
	              int width, int height,
	              const char *str, int align = Left);

	void drawText(const IntRect &rect,
	              const char *str, int align = Left);

	IntRect textSize(const char *str);

	DECL_ATTR(Font, Font&)

	/* Sets initial reference without copying by value,
	 * use at construction */
	void setInitFont(Font *value);

	/* <internal> */
	TEXFBO &getGLTypes() const;
    SDL_Surface *surface() const;
	SDL_Surface *megaSurface() const;
	void ensureNonMega() const;
    void ensureNonAnimated() const;
    void ensureAnimated() const;
    
    // Animation functions
    void stop();
    void play();
    bool isPlaying() const;
    void gotoAndStop(int frame);
    void gotoAndPlay(int frame);
    int numFrames() const;
    int currentFrameI() const;
    
    int addFrame(Bitmap &source, int position = -1);
    void removeFrame(int position = -1);
    
    void nextFrame();
    void previousFrame();
    std::vector<TEXFBO> &getFrames() const;
    
    void setAnimationFPS(float FPS);
    float getAnimationFPS() const;
    
    void setLooping(bool loop);
    bool getLooping() const;

    void ensureNotPlaying() const;
    // ----------
    
	/* Binds the backing texture and sets the correct
	 * texture size uniform in shader */
	void bindTex(ShaderBase &shader);

	/* Adds 'rect' to tainted area */
	void taintArea(const IntRect &rect);

	sigslot::signal<> modified;

	static int maxSize();

    void assumeRubyGC();

private:
	void releaseResources();
	const char *klassName() const { return "bitmap"; }

	BitmapPrivate *p;
};

#endif // BITMAP_H
