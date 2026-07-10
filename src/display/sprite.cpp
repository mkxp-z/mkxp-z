/*
 ** sprite.cpp
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

#include "sprite.h"

#include "sharedstate.h"
#include "bitmap.h"
#include "debugwriter.h"
#include "config.h"
#include "etc.h"
#include "etc-internal.h"
#include "util.h"

#include "gl-util.h"
#include "quad.h"
#include "transform.h"
#include "shader.h"
#include "glstate.h"
#include "quadarray.h"

// TODO: Replace M_PI with std::numbers::pi once we upgrade to C++20.
#include <math.h>
#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

#include <SDL_rect.h>

#include "sigslot/signal.hpp"

static float fwrap(float value, float range)
{
    float res = fmod(value, range);
    return res < 0 ? res + range : res;
}

struct SpritePrivate
{
    Bitmap *bitmap;
    Bitmap *realBitmap;
    
    sigslot::connection bitmapDispCon;
    
    int realOX;
    int realOY;
    float realZoomX;
    float realZoomY;
    
    Rect *realSrcRect;
    
    Quad quad;
    Transform trans;
    
    FloatRect srcRect;
    FloatRect adjustedSrcRect;
    sigslot::connection srcRectCon;
    
    bool mirrored;
    int bushDepth;
    float efBushDepth;
    float bushSlope;
    float bushIntercept;
    bool bushY;
    bool bushUnder;
    bool bushDirty;
    NormValue bushOpacity;
    NormValue opacity;
    BlendType blendType;
    
    Bitmap *pattern;
    BlendType patternBlendType;
    bool patternTile;
    NormValue patternOpacity;
    Vec2 patternScroll;
    Vec2 patternZoom;
    
    bool invert;
    
    Scene::Geometry sceneGeo;
    
    /* Would this sprite be visible on
     * the screen if drawn? */
    bool isVisible;
    bool *spriteVisible;
    Viewport *viewport;
    
    Color *color;
    Tone *tone;
    
    struct
    {
        int amp;
        int length;
        int speed;
        float phase;
        
        /* Wave effect is active (amp != 0) */
        bool active;
        /* qArray needs updating */
        bool dirty;
        SimpleQuadArray qArray;
    } wave;
    
    EtcTemps tmp;
    
    sigslot::connection prepareCon;
    
    SpritePrivate()
    : bitmap(0),
    realBitmap(0),
    realOX(0),
    realOY(0),
    realZoomX(1.0f),
    realZoomY(1.0f),
    realSrcRect(&tmp.rect),
    mirrored(false),
    bushDepth(0),
    bushSlope(0),
    bushIntercept(1.0f),
    bushY(true),
    bushUnder(true),
    bushDirty(true),
    bushOpacity(128),
    opacity(255),
    blendType(BlendNormal),
    pattern(0),
    patternTile(true),
    patternOpacity(255),
    invert(false),
    isVisible(false),
    color(&tmp.color),
    tone(&tmp.tone)
    
    {
        updateSrcRectCon();
        
        prepareCon = shState->prepareDraw.connect
        (&SpritePrivate::prepare, this);
        
        patternScroll = Vec2(0,0);
        patternZoom = Vec2(1, 1);
        
        wave.amp = 0;
        wave.length = 180;
        wave.speed = 360;
        wave.phase = 0.0f;
        wave.dirty = false;
    }
    
    ~SpritePrivate()
    {
        srcRectCon.disconnect();
        prepareCon.disconnect();
        
        bitmapDisposal();
    }
    
    void bitmapDisposal()
    {
        if (bitmap != realBitmap)
        {
            delete bitmap;
        }
        realBitmap = bitmap = 0;
        bitmapDispCon.disconnect();
    }

	void updateChild()
	{
		if (nullOrDisposed(bitmap))
			return;
		
		if (bitmap == realBitmap || !opacity)
		{
			return;
		}
		
		ChildPublic &shared = *bitmap->getChildInfo();
		
		shared.sceneRect = &sceneGeo.rect;
		shared.sceneOrig = &sceneGeo.orig;
		
		shared.x = trans.getPosition().x;
		shared.y = trans.getPosition().y;
		shared.realOffset = Vec2i(realOX, realOY);
		shared.realZoom = Vec2(std::max(realZoomX, 0.0f), std::max(realZoomY, 0.0f));
		shared.angle = fwrap(trans.getRotation(), 360);
		
		shared.mirrored = mirrored;
		
		shared.realSrcRect = realSrcRect->toIntRect();
		
		shared.waveAmp = wave.amp;
		
		//shared.width = sceneGeo.rect.w;
		//shared.height = sceneGeo.rect.h;
		
		bitmap->childUpdate();
		
		isVisible = shared.isVisible;
		
		if (!isVisible)
		{
			return;
		}
		
		if (trans.getOrigin().x != shared.offset.x || trans.getOrigin().y != shared.offset.y)
			trans.setOrigin(Vec2(shared.offset.x, shared.offset.y));
		if (trans.getScale().x != shared.zoom.x || trans.getScale().y != shared.zoom.y)
			trans.setScale(Vec2(shared.zoom.x, shared.zoom.y));
		if (srcRect.x != shared.srcRect.x || srcRect.y != shared.srcRect.y ||
		    srcRect.w != shared.srcRect.w || srcRect.h != shared.srcRect.h)
		{
			srcRect = shared.srcRect;
			onSrcRectChange();
		}
	}

    void recomputeBushDepth()
    {
        if (nullOrDisposed(bitmap))
            return;
        
        bushDirty = false;
        
        if (bushDepth <= 0)
        {
            bushSlope = 0;
            bushIntercept = 1.0f;
            bushY = true;
            bushUnder = true;
            return;
        }
        
        // Invert the angle if mirrored
        int mirror = mirrored ? -1 : 1;
        float angle = fwrap(mirror * trans.getRotation(), 360);
        
        // If it's not rotated, then we can skip all of those other calculations.
        if (angle == 0.0f)
        {
            bushSlope = 0;
            bushIntercept = (srcRect.y + srcRect.h - (bushDepth / trans.getScale().y)) / bitmap->height();
            bushY = true;
            bushUnder = true;
            return;
        }
        
        // Calculate the slope in segments of 45deg, so I don't have to deal with near-infinite slopes
        bushSlope = tan(abs(fwrap(angle - 45, 90) - 45) * M_PI / 180.0f);
        // Manually set negative slopes
        bushSlope *= fwrap(angle, 180) > 90 ? -1 : 1;
        
        
        // If the angle is within 45deg of 90deg or 270deg we use the x-axis instead
        // Additionally, since the shader's coordinates are percentage based, we need to make
        // the slope relative to the scaled bitmap's ratio
        float scaledW = bitmap->width() * trans.getScale().x;
        float scaledH = bitmap->height() * trans.getScale().y;
        if (fwrap(angle + 45, 180) < 90)
        {
            bushY = true;
            bushSlope = bushSlope * scaledW / scaledH;
        }
        else
        {
            bushY = false;
            bushSlope = bushSlope * scaledH / scaledW;
        }
        // Invert the check when we switch from the y-axis to the x-axis
        bushUnder = angle < 45 || angle >= 225;
        
        // Zoom and rotate the srcRect

        FloatRect src = srcRect;

        // Mirrored sprites whose src_rects extend beyond the bounds of the bitmap
        // need to swap the overflows
        if (mirrored)
        {
            float overflowX = std::max(src.x + src.w - bitmap->width(), 0.0f);
            
            if (src.x < 0)
            {
                src.x = 0;
            }
            src.x -= overflowX;
        }

        // A quick hack to get mirrored mega surfaces to work
        if (realBitmap != bitmap && mirrored && src.w > bitmap->width())
        {
            src.x *= -1;
            src.x -= srcRect.w - bitmap->width();
            if (realSrcRect->x < 0)
            {
                src.x += realSrcRect->x * (realZoomX / trans.getScale().x);
            }
        }

        src.x *= trans.getScale().x;
        src.y *= trans.getScale().y;
        src.w *= trans.getScale().x;
        src.h *= trans.getScale().y;
        
        // We use a "left-handed" coordinate system, with positive y values being below the x-axis,
        // so we need to use the negative of the angle to get the proper y values.
        float rotation  = -angle * M_PI / 180.0f;
        
        // p1 doesn't change, so we can skip rotating it
        Vec2 p1 = src.topLeft();
        Vec2 p2 = rotate_point(p1, rotation, src.topRight());
        Vec2 p3 = rotate_point(p1, rotation, src.bottomLeft());
        Vec2 p4 = rotate_point(p1, rotation, src.bottomRight());
        
        // Find the upper boundary of the bush effect and rotate it back.
        // The rotated slope is a horizontal line, so any x value will work.
        Vec2 point(0, std::max(std::max(p1.y, p2.y), std::max(p3.y, p4.y)) - bushDepth);
        point = rotate_point(p1, -rotation, point);
        
        // Unzoom the point and convert it into a percentage
        point.y = point.y / trans.getScale().y / bitmap->height();
        point.x = point.x / trans.getScale().x / bitmap->width();
        
        if (bushY)
            bushIntercept = (point.y - (bushSlope * point.x));
        else
            bushIntercept = (point.x - (bushSlope * point.y));
    }
    
    void onSrcRectChange()
    {
        if (bitmap == realBitmap)
            srcRect = realSrcRect->toFloatRect();
        
        adjustedSrcRect = srcRect;
        FloatRect &rect = adjustedSrcRect;
        Vec2i bmSize;
        Vec2i bmSizeHires;
        
        if (!nullOrDisposed(bitmap))
        {
            bmSize = Vec2i(bitmap->width(), bitmap->height());
            if (bitmap->hasHires())
            {
                bmSizeHires = Vec2i(bitmap->getHires()->width(), bitmap->getHires()->height());
            }
        }
        
        /* Clamp the rectangle so it doesn't reach outside
         * the bitmap bounds */
        if (rect.x < 0)
        {
            rect.w += rect.x;
            trans.setSrcRectOrigin(Vec2(rect.x, trans.getSrcRectOrigin().y));
        }
        else if(trans.getSrcRectOrigin().x != 0)
            trans.setSrcRectOrigin(Vec2(0, trans.getSrcRectOrigin().y));
        if (rect.y < 0)
        {
            rect.h += rect.y;
            trans.setSrcRectOrigin(Vec2(trans.getSrcRectOrigin().x, rect.y));
        }
        else if(trans.getSrcRectOrigin().y != 0)
            trans.setSrcRectOrigin(Vec2(trans.getSrcRectOrigin().x, 0));
        rect.x = clamp<float>(rect.x, 0, bmSize.x);
        rect.y = clamp<float>(rect.y, 0, bmSize.y);
        rect.w = clamp<float>(rect.w, 0, bmSize.x-rect.x);
        rect.h = clamp<float>(rect.h, 0, bmSize.y-rect.y);
        
        if (bmSizeHires.x && bmSizeHires.y && bmSize.x && bmSize.y)
        {
            FloatRect rectHires(rect.x * bmSizeHires.x / bmSize.x,
                                rect.y * bmSizeHires.y / bmSize.y,
                                rect.w * bmSizeHires.x / bmSize.x,
                                rect.h * bmSizeHires.y / bmSize.y);
            quad.setTexRect(mirrored ? rectHires.hFlipped() : rectHires);
        }
        else
        {
            quad.setTexRect(mirrored ? rect.hFlipped() : rect);
        }
        
        quad.setPosRect(FloatRect(0, 0, rect.w, rect.h));
        bushDirty = true;
        
        if (wave.active)
            wave.dirty = true;
    }
    
    void updateSrcRectCon()
    {
        /* Cut old connection */
        srcRectCon.disconnect();
        /* Create new one */
        if (realBitmap == bitmap)
        {
            srcRectCon = realSrcRect->valueChanged.connect
            (&SpritePrivate::onSrcRectChange, this);
        }
    }
    
    void updateVisibility()
    {
        /* Child bitmaps handle their own visibility checks */
        if (bitmap != realBitmap)
            return;
        
        isVisible = false;
        
        if (nullOrDisposed(bitmap))
            return;
        
        if (!opacity)
            return;
        
        /* Compare sprite bounding box against the scene */
        
        /* If sprite is zoomed/rotated, just opt out for now
         * for simplicity's sake */
        const Vec2 &scale = trans.getScale();
        if (!scale.x || !scale.y)
            return;
        
        if (scale.x != 1 || scale.y != 1 || trans.getRotation() != 0)
        {
            isVisible = true;
            return;
        }
        
        if (wave.active)
        {
            /* Don't do expensive wave bounding box
             * calculations */
            isVisible = wave.qArray.quadCount != 0;
            return;
        }
        
        IntRect self = adjustedSrcRect;
        self.setPos(trans.getPositionI() + trans.getGlobalOffset() - trans.getAdjustedOriginI());
        
        isVisible = SDL_HasIntersection(&self, &sceneGeo.rect);
    }
    
    void emitWaveChunk(SVertex *&vert, float phase, float width,
                       const Vec2 &zoom, int chunkY, int chunkLength, int offsetLength)
    {
        float wavePos = phase + ((offsetLength + chunkY) / (float) wave.length) * (float) (M_PI * 2);
        float chunkX = sin(wavePos) * wave.amp / zoom.x;
        
        FloatRect pos(chunkX, chunkY / zoom.y, adjustedSrcRect.w, chunkLength / zoom.y);
        
        /* For some bizarre reason, combining a positive wave.amp with
         * a non-zero angle (including multiples of 360) reduces the width.
         * That being said, RGSS applies the wave effect after rotation
         * and we're applying it before, so since we're deviating anyway
         * and this behavior is weird I'm choosing to not do it. */
        //if (p->trans.getRotation())
        //    pos.w *= pos.w / (pos.w + (2.0f * wave.amp));
        
        FloatRect tex = mirrored ? adjustedSrcRect.hFlipped() : adjustedSrcRect;
        
        tex.y += pos.y;
        tex.h = pos.h;
        if (bitmap->hasHires())
        {
            Vec2 bmSize = Vec2(bitmap->width(), bitmap->height());
            Vec2 bmSizeHires = Vec2(bitmap->getHires()->width(), bitmap->getHires()->height());
            if (bmSizeHires.x && bmSizeHires.y && bmSize.x && bmSize.y)
            {
                tex.x *= bmSizeHires.x / bmSize.x;
                tex.y *= bmSizeHires.y / bmSize.y;
                tex.w *= bmSizeHires.x / bmSize.x;
                tex.h *= bmSizeHires.y / bmSize.y;
            }
        }
        
        Quad::setTexPosRect(vert, tex, pos);
        vert += 4;
    }
    
    void updateWave()
    {
        wave.dirty = false;
        
        if (nullOrDisposed(bitmap))
            return;
        
        if (wave.amp == 0)
        {
            wave.active = false;
            return;
        }
        
        wave.active = true;
        
        float width = adjustedSrcRect.w;
        float height = adjustedSrcRect.h;
        const Vec2 &zoom = trans.getScale();
        
        /* The length of the sprite as it appears on screen */
        int visibleLength = height * zoom.y;
        
        if (!visibleLength || !width)
        {
            wave.qArray.resize(0);
            wave.qArray.commit();
            
            return;
        }
        
        /* RMVX does this, and I have no fucking clue why */
        if (wave.amp < 0)
        { 
            float amp = wave.amp;
            
            if (realBitmap != bitmap && realZoomX != trans.getScale().x)
            {
                amp *= realZoomX / trans.getScale().x;
            }
            
            float scaledAmp = amp / zoom.x;
            
            FloatRect tex = mirrored ? adjustedSrcRect.hFlipped() : adjustedSrcRect;
            FloatRect pos(0, 0, 0, adjustedSrcRect.h);
            float mult = (scaledAmp * 2) / srcRect.w;
            pos.x = -scaledAmp - (trans.getSrcRectOrigin().x * mult);
            pos.w = tex.w * (1 + mult);
            
            if ((pos.w * zoom.x) < 0.5f)
            {
                wave.qArray.resize(0);
                wave.qArray.commit();
                
                return;
            }
            
            if (bitmap->hasHires())
            {
                Vec2 bmSize = Vec2(bitmap->width(), bitmap->height());
                Vec2 bmSizeHires = Vec2(bitmap->getHires()->width(), bitmap->getHires()->height());
                if (bmSizeHires.x && bmSizeHires.y && bmSize.x && bmSize.y)
                {
                    tex.x *= bmSizeHires.x / bmSize.x;
                    tex.y *= bmSizeHires.y / bmSize.y;
                    tex.w *= bmSizeHires.x / bmSize.x;
                    tex.h *= bmSizeHires.y / bmSize.y;
                }
            }
            wave.qArray.resize(1);
            Quad::setTexPosRect(&wave.qArray.vertices[0], tex, pos);
            wave.qArray.commit();
            
            return;
        }
        
        /* A negative position in the srcRect affects the wave position. */
        int offsetLength = abs(trans.getSrcRectOrigin().y * zoom.y);
        
        /* First chunk length (aligned to 8 pixel boundary) */
        int posY = (int) trans.getPosition().y - (int) (trans.getOrigin().y * zoom.y) + trans.getGlobalOffset().y;
        int firstLength = 8 - ((posY + offsetLength) % 8);
        firstLength = std::min(firstLength % 8, visibleLength);
        
        /* If the position is negative, then the first chunk's alignment
         * needs a little more fiddling */
        int firstOffset;
        if (offsetLength && firstLength)
        {
            firstOffset = 8 - (posY % 8);
            firstOffset = std::min(firstOffset % 8, offsetLength);
            firstOffset += (offsetLength - firstOffset) & (~7);
        } else {
            firstOffset = 0;
        }
        
        /* Amount of full 8 pixel chunks in the middle */
        int chunks = (visibleLength - firstLength) / 8;
        
        /* Final chunk length */
        int lastLength = (visibleLength - firstLength) % 8;
        
        wave.qArray.resize(!!firstLength + chunks + !!lastLength);
        SVertex *vert = &wave.qArray.vertices[0];
        
        float phase = (wave.phase * (float) M_PI) / 180.0f;
        
        if (firstLength > 0)
            emitWaveChunk(vert, phase, width, zoom, 0, firstLength, firstOffset);
        
        for (int i = 0; i < chunks; ++i)
            emitWaveChunk(vert, phase, width, zoom, firstLength + i * 8, 8, offsetLength);
        
        if (lastLength > 0)
            emitWaveChunk(vert, phase, width, zoom, firstLength + chunks * 8, lastLength, offsetLength);
        
        wave.qArray.commit();
    }
    
    void prepare()
    {
        // Skip preparations and drawing if the bitmap is disposed or the sprite or viewport is invisible
        if (nullOrDisposed(realBitmap) || !(*spriteVisible) || (viewport && !viewport->getVisible()))
        {
            isVisible = false;
            return;
        }
        
        // Wave state influences updateVisibility, so we have to updateWave first.
        if (wave.dirty)
            updateWave();

        updateChild();
        
        updateVisibility();
        
        if (!isVisible)
            return;
        
        if (bushDirty)
            recomputeBushDepth();
    }
};

Sprite::Sprite(Viewport *viewport)
: ViewportElement(viewport)
{
    p = new SpritePrivate;
    p->spriteVisible = &visible;
    p->viewport = viewport;
    onGeometryChange(scene->getGeometry());
}

Sprite::~Sprite()
{
    dispose();
}

DEF_ATTR_RD_SIMPLE(Sprite, Bitmap,     Bitmap*, p->realBitmap)
DEF_ATTR_RD_SIMPLE(Sprite, X,          int,     p->trans.getPosition().x)
DEF_ATTR_RD_SIMPLE(Sprite, Y,          int,     p->trans.getPosition().y)
DEF_ATTR_RD_SIMPLE(Sprite, OX,         int,     p->realOX)
DEF_ATTR_RD_SIMPLE(Sprite, OY,         int,     p->realOY)
DEF_ATTR_RD_SIMPLE(Sprite, ZoomX,      float,   p->realZoomX)
DEF_ATTR_RD_SIMPLE(Sprite, ZoomY,      float,   p->realZoomY)
DEF_ATTR_RD_SIMPLE(Sprite, Angle,      float,   p->trans.getRotation())
DEF_ATTR_RD_SIMPLE(Sprite, Mirror,     bool,    p->mirrored)
DEF_ATTR_RD_SIMPLE(Sprite, BushDepth,  int,     p->bushDepth)
DEF_ATTR_RD_SIMPLE(Sprite, BlendType,  int,     p->blendType)
DEF_ATTR_RD_SIMPLE(Sprite, Pattern,    Bitmap*, p->pattern)
DEF_ATTR_RD_SIMPLE(Sprite, PatternBlendType, int, p->patternBlendType)
DEF_ATTR_RD_SIMPLE(Sprite, Width,      int,     p->realSrcRect->width)
DEF_ATTR_RD_SIMPLE(Sprite, Height,     int,     p->realSrcRect->height)
DEF_ATTR_RD_SIMPLE(Sprite, WaveAmp,    int,     p->wave.amp)
DEF_ATTR_RD_SIMPLE(Sprite, WaveLength, int,     p->wave.length)
DEF_ATTR_RD_SIMPLE(Sprite, WaveSpeed,  int,     p->wave.speed)
DEF_ATTR_RD_SIMPLE(Sprite, WavePhase,  float,   p->wave.phase)

DEF_ATTR_SIMPLE(Sprite, BushOpacity, int,     p->bushOpacity)
DEF_ATTR_SIMPLE(Sprite, Opacity,     int,     p->opacity)
DEF_ATTR_SIMPLE(Sprite, SrcRect,     Rect&,  *p->realSrcRect)
DEF_ATTR_SIMPLE(Sprite, Color,       Color&, *p->color)
DEF_ATTR_SIMPLE(Sprite, Tone,        Tone&,  *p->tone)
DEF_ATTR_SIMPLE(Sprite, PatternTile, bool, p->patternTile)
DEF_ATTR_SIMPLE(Sprite, PatternOpacity, int, p->patternOpacity)
DEF_ATTR_SIMPLE(Sprite, PatternScrollX, int, p->patternScroll.x)
DEF_ATTR_SIMPLE(Sprite, PatternScrollY, int, p->patternScroll.y)
DEF_ATTR_SIMPLE(Sprite, PatternZoomX, float, p->patternZoom.x)
DEF_ATTR_SIMPLE(Sprite, PatternZoomY, float, p->patternZoom.y)
DEF_ATTR_SIMPLE(Sprite, Invert,      bool,    p->invert)

void Sprite::setBitmap(Bitmap *bitmap)
{
    guardDisposed();
    
    if (p->realBitmap == bitmap)
        return;
    
    if (p->bitmap != p->realBitmap)
        delete p->bitmap;
    
    p->bitmap = bitmap;
    p->realBitmap = bitmap;
    
    p->bitmapDispCon.disconnect();
    
    if (nullOrDisposed(bitmap))
    {
        p->realBitmap = p->bitmap = 0;
        return;
    }
    
    p->bitmapDispCon = bitmap->wasDisposed.connect(&SpritePrivate::bitmapDisposal, p);
    
    if (bitmap->isMega())
    {
        p->bitmap = bitmap->spawnChild();
        p->srcRect = p->bitmap->rect();
    }
    
    *p->realSrcRect = p->realBitmap->rect();
    p->onSrcRectChange();
    p->updateSrcRectCon();
}

void Sprite::setX(int value)
{
    guardDisposed();
    
    if (p->trans.getPosition().x == value)
        return;
    
    p->trans.setPosition(Vec2(value, getY()));
}

void Sprite::setY(int value)
{
    guardDisposed();
    
    if (p->trans.getPosition().y == value)
        return;
    
    p->trans.setPosition(Vec2(getX(), value));
    
    if (p->wave.active)
        p->wave.dirty = true;
    
    if (rgssVer >= 2)
        setSpriteY(value);
}

void Sprite::setOX(int value)
{
    guardDisposed();
    
    if (p->realOX == value)
        return;
    
    p->realOX = value;
    p->trans.setOrigin(Vec2(value, getOY()));
}

void Sprite::setOY(int value)
{
    guardDisposed();
    
    if (p->realOY == value)
        return;
    
    p->realOY = value;
    p->trans.setOrigin(Vec2(getOX(), value));
    
    if (p->wave.active)
        p->wave.dirty = true;
}

void Sprite::setZoomX(float value)
{
    guardDisposed();
    
    if (p->realZoomX == value)
        return;
    
    // RGSS lets you set the zoom below 0, but it doesn't render it
    p->realZoomX = value;
    p->trans.setScale(Vec2(std::max(value, 0.0f), std::max(getZoomY(), 0.0f)));
    
    if (p->wave.active)
        p->wave.dirty = true;
}

void Sprite::setZoomY(float value)
{
    guardDisposed();
    
    if (p->realZoomY == value)
        return;
    
    // RGSS lets you set the zoom below 0, but it doesn't render it
    p->trans.setScale(Vec2(std::max(getZoomX(), 0.0f), std::max(value, 0.0f)));
    p->bushDirty = true;
        
    p->realZoomY = value;
    if (p->wave.active)
        p->wave.dirty = true;
}

void Sprite::setAngle(float value)
{
    guardDisposed();
    
    if (p->trans.getRotation() == value)
        return;
    
    p->trans.setRotation(value);
    
    p->bushDirty = true;
}

void Sprite::setMirror(bool mirrored)
{
    guardDisposed();
    
    if (p->mirrored == mirrored)
        return;
    
    p->mirrored = mirrored;
    p->onSrcRectChange();
    
    if (p->wave.active)
        p->wave.dirty = true;
}

void Sprite::setBushDepth(int value)
{
    guardDisposed();
    
    if (p->bushDepth == value)
        return;
    
    p->bushDepth = value;
    p->bushDirty = true;
}

void Sprite::setBlendType(int type)
{
    guardDisposed();
    
    switch (type)
    {
        default :
        case BlendNormal :
            p->blendType = BlendNormal;
            return;
        case BlendAddition :
            p->blendType = BlendAddition;
            return;
        case BlendSubstraction :
            p->blendType = BlendSubstraction;
            return;
    }
}

void Sprite::setPattern(Bitmap *value)
{
    guardDisposed();
    
    if (p->pattern == value)
        return;
    
    p->pattern = value;
    
    if (!nullOrDisposed(value))
    {
        value->ensureNonMega();
    }
}

void Sprite::setPatternBlendType(int type)
{
    guardDisposed();
    
    switch (type)
    {
        default :
        case BlendNormal :
            p->patternBlendType = BlendNormal;
            return;
        case BlendAddition :
            p->patternBlendType = BlendAddition;
            return;
        case BlendSubstraction :
            p->patternBlendType = BlendSubstraction;
            return;
    }
}

#define DEF_WAVE_SETTER(Name, name, type) \
void Sprite::setWave##Name(type value) \
{ \
guardDisposed(); \
if (p->wave.name == value) \
return; \
p->wave.name = value; \
p->wave.dirty = true; \
}

DEF_WAVE_SETTER(Amp,    amp,    int)
DEF_WAVE_SETTER(Length, length, int)
DEF_WAVE_SETTER(Speed,  speed,  int)

#undef DEF_WAVE_SETTER

void Sprite::setWavePhase(float value)
{
	if (p->wave.phase == value)
		return;
	p->wave.phase = fwrap(value, 360.0f);
	p->wave.dirty = true;
}

void Sprite::initDynAttribs()
{
    p->realSrcRect = new Rect;
    p->color = new Color;
    p->tone = new Tone;
    
    p->updateSrcRectCon();
}

/* Flashable */
void Sprite::update()
{
    guardDisposed();
    
    Flashable::update();
    
    if (p->wave.speed != 0)
    {
        p->wave.phase += p->wave.speed / 180;
        p->wave.phase = fwrap(p->wave.phase, 360.0f);
        p->wave.dirty = true;
    }
}

/* SceneElement */
void Sprite::draw()
{
    if (!p->isVisible)
        return;
    
    if (emptyFlashFlag)
        return;
    
    ShaderBase *base;
    
    bool renderEffect = p->color->hasEffect() ||
    p->tone->hasEffect()  ||
    flashing              ||
    p->bushDepth != 0     ||
    p->invert             ||
    (p->pattern && !p->pattern->isDisposed());
    
    int scalingMethod = NearestNeighbor;

    int sourceWidthHires = p->bitmap->hasHires() ? p->bitmap->getHires()->width() : p->bitmap->width();
    int sourceHeightHires = p->bitmap->hasHires() ? p->bitmap->getHires()->height() : p->bitmap->height();

    double framebufferScalingFactor = shState->config().enableHires ? shState->config().framebufferScalingFactor : 1.0;

    int targetWidthHires = (int)lround(framebufferScalingFactor * p->bitmap->width() * p->trans.getScale().x);
    int targetHeightHires = (int)lround(framebufferScalingFactor * p->bitmap->height() * p->trans.getScale().y);

    int scaleIsSpecial = UpScale;

    if (targetWidthHires == sourceWidthHires && targetHeightHires == sourceHeightHires)
    {
        scaleIsSpecial = SameScale;
    }

    if (targetWidthHires < sourceWidthHires && targetHeightHires < sourceHeightHires)
    {
        scaleIsSpecial = DownScale;
    }

    switch (scaleIsSpecial)
    {
    case SameScale:
        scalingMethod = NearestNeighbor;
        break;
    case DownScale:
        scalingMethod = shState->config().bitmapSmoothScalingDown;
        break;
    default:
        scalingMethod = shState->config().bitmapSmoothScaling;
    }

    if (p->trans.getRotation() != 0.0)
    {
        scalingMethod = shState->config().bitmapSmoothScaling;
    }

    if (renderEffect)
    {
        if (scalingMethod != NearestNeighbor)
        {
            Debug() << "BUG: Smooth SpriteShader not implemented:" << scalingMethod;
            scalingMethod = NearestNeighbor;
        }

        SpriteShader &shader = shState->shaders().sprite;
        
        shader.bind();
        shader.applyViewportProj();
        shader.setSpriteMat(p->trans.getMatrix());
        
        shader.setTone(p->tone->norm);
        shader.setOpacity(p->opacity.norm);
        shader.setBushDepth(p->bushY, p->bushUnder, p->bushSlope, p->bushIntercept);
        shader.setBushOpacity(p->bushOpacity.norm);
        
        if (p->pattern && p->patternOpacity > 0) {
            if (p->pattern->hasHires()) {
                Debug() << "BUG: High-res Sprite pattern not implemented";
            }

            shader.setPattern(p->pattern->getGLTypes().tex, Vec2(p->pattern->width(), p->pattern->height()));
            shader.setPatternBlendType(p->patternBlendType);
            shader.setPatternTile(p->patternTile);
            shader.setPatternZoom(p->patternZoom);
            shader.setPatternOpacity(p->patternOpacity.norm);
            shader.setPatternScroll(p->patternScroll);
            shader.setShouldRenderPattern(true);
        }
        else {
            shader.setShouldRenderPattern(false);
        }
        
        shader.setInvert(p->invert);
        
        /* When both flashing and effective color are set,
         * the one with higher alpha will be blended */
        const Vec4 *blend = (flashing && flashColor.w > p->color->norm.w) ?
        &flashColor : &p->color->norm;
        
        shader.setColor(*blend);
        
        base = &shader;
    }
    else if (p->opacity != 255)
    {
        if (scalingMethod != NearestNeighbor)
        {
            Debug() << "BUG: Smooth AlphaSpriteShader not implemented:" << scalingMethod;
            scalingMethod = NearestNeighbor;
        }

        AlphaSpriteShader &shader = shState->shaders().alphaSprite;
        shader.bind();
        
        shader.setSpriteMat(p->trans.getMatrix());
        shader.setAlpha(p->opacity.norm);
        shader.applyViewportProj();
        base = &shader;
    }
    else
    {
        switch (scalingMethod)
        {
        case Bicubic:
        {
            BicubicSpriteShader &shader = shState->shaders().bicubicSprite;
            shader.bind();

            shader.setTexSize(Vec2i(sourceWidthHires, sourceHeightHires));
            shader.setSharpness(shState->config().bicubicSharpness);
            shader.setSpriteMat(p->trans.getMatrix());
            shader.applyViewportProj();
            base = &shader;
        }
            break;
        case Lanczos3:
        {
            Lanczos3SpriteShader &shader = shState->shaders().lanczos3Sprite;
            shader.bind();
            
            shader.setTexSize(Vec2i(sourceWidthHires, sourceHeightHires));
            shader.setSpriteMat(p->trans.getMatrix());
            shader.applyViewportProj();
            base = &shader;
        }
            break;
#ifdef MKXPZ_SSL
        case xBRZ:
        {
            XbrzSpriteShader &shader = shState->shaders().xbrzSprite;
            shader.bind();

            shader.setTexSize(Vec2i(sourceWidthHires, sourceHeightHires));
            shader.setTargetScale(Vec2((float)(shState->config().xbrzScalingFactor), (float)(shState->config().xbrzScalingFactor)));
            shader.setSpriteMat(p->trans.getMatrix());
            shader.applyViewportProj();
            base = &shader;
        }
            break;
#endif
        default:
        {
            SimpleSpriteShader &shader = shState->shaders().simpleSprite;
            shader.bind();

            shader.setSpriteMat(p->trans.getMatrix());
            shader.applyViewportProj();
            base = &shader;
        }
        }        
    }
    
    glState.blendMode.pushSet(p->blendType);
    
    p->bitmap->bindTex(*base, false);

#ifdef MKXPZ_SSL
    if (scalingMethod == xBRZ)
    {
        XbrzShader &shader = shState->shaders().xbrz;
        shader.setTargetScale(Vec2((float)(shState->config().xbrzScalingFactor), (float)(shState->config().xbrzScalingFactor)));
    }
#endif
    
    TEX::setSmooth(scalingMethod == Bilinear);

    if (p->wave.active)
        p->wave.qArray.draw();
    else
        p->quad.draw();
    
    TEX::setSmooth(false);

    glState.blendMode.pop();
}

void Sprite::onGeometryChange(const Scene::Geometry &geo)
{
    /* Offset at which the sprite will be drawn
     * relative to screen origin */
    const Vec2i &offset = geo.offset();
    if (p->wave.active && p->trans.getGlobalOffset().y != offset.y)
        p->wave.dirty = true;
    p->trans.setGlobalOffset(offset);
    
    p->sceneGeo = geo;
    p->viewport = getViewport();
}

void Sprite::releaseResources()
{
    unlink();
    
    delete p;
}
