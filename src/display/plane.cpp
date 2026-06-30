/*
** plane.cpp
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

#include "plane.h"

#include <cmath>

#include "sharedstate.h"
#include "bitmap.h"
#include "etc.h"
#include "util.h"

#include "gl-util.h"
#include "quad.h"
#include "quadarray.h"
#include "transform.h"
#include "etc-internal.h"
#include "shader.h"
#include "glstate.h"

#include "sigslot/signal.hpp"

#ifdef MKXPZ_RETRO
#  include "sandbox-serial-util.h"
#endif // MKXPZ_RETRO

#define GUARD_V(value, expression) do { expression; if (exception.is_error()) return value; } while (0)
#define GUARD(expression) GUARD_V(, expression)

static float fwrap(float value, float range)
{
	float res = std::fmod(value, range);
	return res < 0 ? res + range : res;
}

struct PlanePrivate
{
	Plane *plane;

	Bitmap *bitmap;
	Bitmap *realBitmap;

	sigslot::connection bitmapDispCon;

	NormValue opacity;
	BlendType blendType;
	Color *color;
	Tone *tone;

	float ox, oy;
	int realOX, realOY;
	float zoomX, zoomY;
	float realZoomX, realZoomY;
	
	bool isVisible;

	Scene::Geometry sceneGeo;

	bool quadSourceDirty;

	SimpleQuadArray qArray;

	EtcTemps tmp;

	sigslot::connection prepareCon;

	PlanePrivate(Plane *plane)
	    : plane(plane),
	      bitmap(0),
	      realBitmap(0),
	      opacity(255),
	      blendType(BlendNormal),
	      color(&tmp.color),
	      tone(&tmp.tone),
	      ox(0), oy(0),
	      realOX(0), realOY(0),
	      realZoomX(1), realZoomY(1),
	      zoomX(1), zoomY(1),
	      isVisible(true),
	      quadSourceDirty(false)
	{
		prepareCon = shState->prepareDraw.connect
		        (&PlanePrivate::prepare, this);

		qArray.resize(1);
	}

	~PlanePrivate()
	{
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

	void updateQuadSource()
	{
		if (gl.npot_repeat)
		{
			FloatRect srcRect;
			srcRect.x = (sceneGeo.orig.x + ox) / zoomX;
			srcRect.y = (sceneGeo.orig.y + oy) / zoomY;
			srcRect.w = sceneGeo.rect.w / zoomX;
			srcRect.h = sceneGeo.rect.h / zoomY;

			Quad::setTexRect(&qArray.vertices[0], srcRect);
			qArray.commit();

			return;
		}

		if (nullOrDisposed(bitmap))
			return;

		/* Scaled (zoomed) bitmap dimensions */
		float sw = bitmap->width()  * zoomX;
		float sh = bitmap->height() * zoomY;

		/* Plane offset wrapped by scaled bitmap dims */
		float wox = fwrap(ox, sw);
		float woy = fwrap(oy, sh);

		/* Viewport dimensions */
		int vpw = sceneGeo.rect.w;
		int vph = sceneGeo.rect.h;

		/* Amount the scaled bitmap is tiled (repeated) */
		size_t tilesX = ceil((vpw - sw + wox) / sw) + 1;
		size_t tilesY = ceil((vph - sh + woy) / sh) + 1;

		FloatRect tex = bitmap->rect();

		qArray.resize(tilesX * tilesY);

		for (size_t y = 0; y < tilesY; ++y)
			for (size_t x = 0; x < tilesX; ++x)
			{
				SVertex *vert = &qArray.vertices[(y*tilesX + x) * 4];
				FloatRect pos(x*sw - wox, y*sh - woy, sw, sh);

				Quad::setTexPosRect(vert, tex, pos);
			}

		qArray.commit();
	}

	void updateChild(Exception &exception)
	{
		if (!opacity || !realZoomX || !realZoomY)
		{
			isVisible = false;
		}
		
		if (bitmap == realBitmap)
		{
			ox = realOX;
			oy = realOY;
			zoomX = realZoomX;
			zoomY = realZoomY;
			isVisible = true;
			return;
		}
		
		ChildPublic &shared = *bitmap->getChildInfo();
		
		shared.sceneElementType = ChildPublic::PLANE;
		shared.sceneElement = plane;
		
		// Unlike Sprites, ox/oy in Planes is unaffected by zoom. So we treat it as x/y like Sprites instead.
		shared.x = -realOX;
		shared.y = -realOY;
		shared.realZoom = Vec2(realZoomX, realZoomY);
		
		shared.width = sceneGeo.rect.w;
		shared.height = sceneGeo.rect.h;
		GUARD(bitmap->childUpdate(exception));
		
		isVisible = shared.isVisible;
		
		if (!isVisible)
		{
			return;
		}
		
		if (ox != shared.offset.x || oy != shared.offset.y ||
		    zoomX != shared.zoom.x || zoomY != shared.zoom.y)
			quadSourceDirty = true;
		
		// Leaving these as floats increases precision when zoomed
		ox = shared.offset.x;
		oy = shared.offset.y;
		
		zoomX = shared.zoom.x;
		zoomY = shared.zoom.y;
		
		
	}

	void prepare()
	{
		if (nullOrDisposed(bitmap))
			return;
		
		{
			// Ignore errors
			Exception e;
			updateChild(e);
		}
		
		if (!isVisible)
			return;
		
		if (quadSourceDirty)
		{
			updateQuadSource();
			quadSourceDirty = false;
		}
	}
};

static void disposePtr(void *ptr)
{
	((Plane *)ptr)->dispose();
}

Plane::Plane(Viewport *viewport)
    : ViewportElement(disposePtr, viewport)
{
	p = new PlanePrivate(this);

	onGeometryChange(scene->getGeometry());
}

DEF_ATTR_RD_SIMPLE(Plane, Bitmap,    Bitmap*, p->realBitmap)
DEF_ATTR_RD_SIMPLE(Plane, OX,        int,     p->realOX)
DEF_ATTR_RD_SIMPLE(Plane, OY,        int,     p->realOY)
DEF_ATTR_RD_SIMPLE(Plane, ZoomX,     float,   p->realZoomX)
DEF_ATTR_RD_SIMPLE(Plane, ZoomY,     float,   p->realZoomY)
DEF_ATTR_RD_SIMPLE(Plane, BlendType, int,     p->blendType)

DEF_ATTR_SIMPLE(Plane, Opacity,   int,     p->opacity)
DEF_ATTR_SIMPLE(Plane, Color,     Color&, *p->color)
DEF_ATTR_SIMPLE(Plane, Tone,      Tone&,  *p->tone)

Plane::~Plane()
{
	dispose();
}

void Plane::setBitmap(Exception &exception, Bitmap *value)
{
	GUARD(guardDisposed(exception));

	if (p->bitmap != p->realBitmap)
		delete p->bitmap;

	p->bitmap = value;
	p->realBitmap = value;

	p->bitmapDispCon.disconnect();

	if (nullOrDisposed(value))
	{
		p->realBitmap = p->bitmap = 0;
		return;
	}

	p->bitmapDispCon = value->wasDisposed.connect(&PlanePrivate::bitmapDisposal, p);

	if (value->isMega())
	{
		GUARD(p->bitmap = value->spawnChild(exception));
		p->bitmap->getChildInfo()->wrap = true;
	}
}

void Plane::setOX(Exception &exception, int value)
{
	GUARD(guardDisposed(exception));

	if (p->realOX == value)
	        return;

	p->realOX = value;
	p->quadSourceDirty = true;
}

void Plane::setOY(Exception &exception, int value)
{
	GUARD(guardDisposed(exception));

	if (p->realOY == value)
	        return;

	p->realOY = value;
	p->quadSourceDirty = true;
}

void Plane::setZoomX(Exception &exception, float value)
{
	GUARD(guardDisposed(exception));

	// RGSS hangs if you set this below 0
	value = std::max(value, 0.0f);

	if (p->realZoomX == value)
	        return;

	p->realZoomX = value;
	p->quadSourceDirty = true;
}

void Plane::setZoomY(Exception &exception, float value)
{
	GUARD(guardDisposed(exception));

	// RGSS hangs if you set this below 0
	value = std::max(value, 0.0f);

	if (p->realZoomY == value)
	        return;

	p->realZoomY = value;
	p->quadSourceDirty = true;
}

void Plane::setBlendType(Exception &exception, int value)
{
	GUARD(guardDisposed(exception));

	switch (value)
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

void Plane::initDynAttribs()
{
	p->color = new Color;
	p->tone = new Tone;
}

const IntRect *Plane::sceneRect() const noexcept
{
	return &p->sceneGeo.rect;
}

const Vec2i *Plane::sceneOrig() const noexcept
{
	return &p->sceneGeo.orig;
}

void Plane::draw(Exception &exception)
{
	if (nullOrDisposed(p->bitmap))
		return;

	if (!p->isVisible)
		return;

	ShaderBase *base;

	if (p->color->hasEffect() || p->tone->hasEffect() || p->opacity != 255)
	{
		PlaneShader &shader = shState->shaders().plane;

		shader.bind();
		shader.applyViewportProj();
		shader.setTone(p->tone->norm);
		shader.setColor(p->color->norm);
		shader.setFlash(Vec4());
		shader.setOpacity(p->opacity.norm);

		base = &shader;
	}
	else
	{
		SimpleShader &shader = shState->shaders().simple;

		shader.bind();
		shader.applyViewportProj();
		shader.setTranslation(Vec2i());

		base = &shader;
	}

	glState.blendMode.pushSet(p->blendType);

	p->bitmap->bindTex(*base);

	if (gl.npot_repeat)
		TEX::setRepeat(true);

	p->qArray.draw();

	if (gl.npot_repeat)
		TEX::setRepeat(false);

	glState.blendMode.pop();
}

void Plane::onGeometryChange(const Scene::Geometry &geo)
{
	if (gl.npot_repeat)
		Quad::setPosRect(&p->qArray.vertices[0], FloatRect(geo.rect));

	p->sceneGeo = geo;
	p->quadSourceDirty = true;
}

void Plane::releaseResources()
{
	unlink();

	delete p;
}

#ifdef MKXPZ_RETRO
void Plane::sandbox_reinit()
{
	if (isDisposed()) return;

	p->qArray.reinit();
	p->quadSourceDirty = true;
}

#ifndef MKXPZ_SANDBOX_SERIAL_PLANE_H
#define MKXPZ_SANDBOX_SERIAL_PLANE_H
#include "sandbox-serial-plane.h"
#endif // MKXPZ_SANDBOX_SERIAL_PLANE_H
#endif // MKXPZ_RETRO
