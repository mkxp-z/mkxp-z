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

#include <vector>

class Font;
class ShaderBase;
struct TEXFBO;
struct SDL_Surface;

struct BitmapPrivate;
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
	                int opacity = 255, bool smooth = false);

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

	Color getPixel(int x, int y) const;
	void setPixel(int x, int y, const Color &color);
    
    bool getRaw(void *output, int output_size);
    void replaceRaw(void *pixel_data, int size);

    /* Sub-rect CPU->GPU upload. Uploads the rectangle `[x, y, w, h)`
     * from the bitmap's cached CPU shadow surface (the one returned
     * by `surface()`) into the matching region of the GPU texture,
     * without re-allocating the texture and without discarding the
     * shadow. This is the fast path for callers that paint directly
     * into the shadow surface every frame (e.g. the H-Mode7 software
     * rasterizer) -- using it avoids the full-texture realloc and the
     * next-frame `glReadPixels` round-trip that `replaceRaw` forces
     * via `onModified(true)`. No-op for mega surfaces (they have no
     * GPU texture) and for bitmaps without a CPU shadow yet. */
    void uploadCPURect(int x, int y, int w, int h);

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
	void bindTex(ShaderBase &shader, bool substituteLoresSize = true);

	/* For mega surface bitmaps: extracts the given rect from
	 * the CPU-side surface, uploads to a temp GL texture, binds
	 * it, and sets the shader's texSize to the rect dimensions.
	 * Returns false if this bitmap is not a mega surface. */
	bool bindTexMega(ShaderBase &shader, const IntRect &srcRect);

	/* Adds 'rect' to tainted area */
	void taintArea(const IntRect &rect);

	sigslot::signal<> modified;

	static int maxSize();
	/* The limit the GPU truly accepts. maxSize() can be raised above
	 * this by the maxTextureSize config option, which exists so that
	 * oversized work buffers can be allocated; a texture built at that
	 * raised size draws as black. */
	static int realMaxSize();

    void assumeRubyGC();

	static void drawHeldText();

private:
	void stretchBlt(IntRect destRect,
	                const Bitmap &source, IntRect sourceRect,
	                int opacity, bool smooth,
	                const std::vector<IntRect> *parts);

	void releaseResources();
	sigslot::connection loresDispCon;
	const char *klassName() const { return "bitmap"; }

	BitmapPrivate *p;

	void loresDisposal();
};

#endif // BITMAP_H
