/*
** viewport.cpp
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

#include "viewport.h"

#include "sharedstate.h"
#include "etc.h"
#include "util.h"
#include "quad.h"
#include "glstate.h"
#include "graphics.h"

#include "sigslot/signal.hpp"

#ifdef MKXPZ_RETRO
#  include "sandbox-serial-util.h"
#endif // MKXPZ_RETRO

#define GUARD_V(value, expression) do { expression; if (exception.is_error()) return value; } while (0)
#define GUARD(expression) GUARD_V(, expression)

#ifdef MKXPZ_RETRO
static uint64_t next_id = 1;
#endif // MKXPZ_RETRO

struct ViewportPrivate
{
	/* Needed for geometry changes */
	Viewport *self;

	Rect *rect;
	sigslot::connection rectCon;
#ifdef MKXPZ_RETRO
	Rect deserSavedRect;
	bool deserGeometryChanged;
	bool deserScreenRectChanged;
#endif // MKXPZ_RETRO

	Color *color;
	Tone *tone;

	IntRect screenRect;
	bool isOnScreen;

	EtcTemps tmp;

	ViewportPrivate(int x, int y, int width, int height, Viewport *self)
	    : self(self),
	      rect(&tmp.rect),
	      color(&tmp.color),
	      tone(&tmp.tone),
	      isOnScreen(false)
	{
		rect->set(x, y, width, height);
		updateRectCon();
	}

	~ViewportPrivate()
	{
		rectCon.disconnect();
	}

	void onRectChange()
	{
		self->geometry.rect = rect->toIntRect();
		self->notifyGeometryChange();
		recomputeOnScreen();
	}

	void updateRectCon()
	{
		rectCon.disconnect();
		rectCon = rect->valueChanged.connect
		        (&ViewportPrivate::onRectChange, this);
	}

	void recomputeOnScreen()
	{
		isOnScreen = screenRect.x < rect->x + rect->width
			&& rect->x < screenRect.x + screenRect.w
			&& screenRect.y < rect->y + rect->height
			&& rect->y < screenRect.y + screenRect.h;
	}

	bool needsEffectRender(bool flashing)
	{
		bool rectEffective = !rect->isEmpty();
		bool colorToneEffective = color->hasEffect() || tone->hasEffect() || flashing;

		return (rectEffective && colorToneEffective && isOnScreen);
	}
};

Viewport::Viewport(int x, int y, int width, int height)
    : SceneElement(*shState->screen()),
#ifdef MKXPZ_RETRO
      id(next_id++),
#endif // MKXPZ_RETRO
      sceneLink(this)
{
	initViewport(x, y, width, height);
}

Viewport::Viewport(Rect *rect)
    : SceneElement(*shState->screen()),
#ifdef MKXPZ_RETRO
      id(next_id++),
#endif // MKXPZ_RETRO
      sceneLink(this)
{
	initViewport(rect->x, rect->y, rect->width, rect->height);
}

Viewport::Viewport()
    : SceneElement(*shState->screen()),
#ifdef MKXPZ_RETRO
      id(next_id++),
#endif // MKXPZ_RETRO
      sceneLink(this)
{
	const Graphics &graphics = shState->graphics();
	initViewport(0, 0, graphics.width(), graphics.height());
}

void Viewport::initViewport(int x, int y, int width, int height)
{
	p = new ViewportPrivate(x, y, width, height, this);

	/* Set our own geometry */
	geometry.rect = IntRect(x, y, width, height);

	/* Handle parent geometry */
	onGeometryChange(scene->getGeometry());
}

Viewport::~Viewport()
{
	dispose();
}

void Viewport::update(Exception &exception)
{
	GUARD(guardDisposed(exception));

	GUARD(Flashable::update(exception));
}

DEF_ATTR_RD_SIMPLE(Viewport, OX,   int,   geometry.orig.x)
DEF_ATTR_RD_SIMPLE(Viewport, OY,   int,   geometry.orig.y)

DEF_ATTR_SIMPLE(Viewport, Rect,  Rect&,  *p->rect)
DEF_ATTR_SIMPLE(Viewport, Color, Color&, *p->color)
DEF_ATTR_SIMPLE(Viewport, Tone,  Tone&,  *p->tone)

void Viewport::setOX(Exception &exception, int value)
{
	GUARD(guardDisposed(exception));

	if (geometry.orig.x == value)
		return;

	geometry.orig.x = value;
	notifyGeometryChange();
}

void Viewport::setOY(Exception &exception, int value)
{
	GUARD(guardDisposed(exception));

	if (geometry.orig.y == value)
		return;

	geometry.orig.y = value;
	notifyGeometryChange();
}

void Viewport::initDynAttribs()
{
	p->rect = new Rect(*p->rect);
	p->color = new Color;
	p->tone = new Tone;

	p->updateRectCon();
}

/* Scene */
void Viewport::composite(Exception &exception)
{
	if (emptyFlashFlag)
		return;

	bool renderEffect = p->needsEffectRender(flashing);

	if (elements.getSize() == 0 && !renderEffect)
		return;

	/* Setup scissor */
	glState.scissorTest.pushSet(true);
	glState.scissorBox.pushSet(p->rect->toIntRect());

	GUARD(Scene::composite(exception));

	/* If any effects are visible, request parent Scene to
	 * render them. */
	if (renderEffect)
		scene->requestViewportRender
		        (p->color->norm, flashColor, p->tone->norm);

	glState.scissorBox.pop();
	glState.scissorTest.pop();
}

/* SceneElement */
void Viewport::draw(Exception &exception)
{
	GUARD(composite(exception));
}

void Viewport::onGeometryChange(const Geometry &geo)
{
	p->screenRect = geo.rect;
	p->recomputeOnScreen();
}

void Viewport::releaseResources()
{
	unlink();

	delete p;
}


ViewportElement::ViewportElement(void (*dispose)(void *), Viewport *viewport, int z, int spriteY)
    : SceneElement(viewport ? *viewport : *shState->screen(), z, spriteY),
      m_dispose(dispose), m_viewport(viewport)
{
	if (rgssVer == 1 && viewport)
		viewportDispCon = viewport->wasDisposed.connect(&ViewportElement::viewportElementDisposal, this);
}

Viewport *ViewportElement::getViewport() const
{
	return m_viewport;
}

void ViewportElement::setViewport(Viewport *viewport)
{
	m_viewport = viewport;
	
	viewportDispCon.disconnect();
	if (rgssVer == 1 && viewport)
		viewportDispCon = viewport->wasDisposed.connect(&ViewportElement::viewportElementDisposal, this);
	
	setScene(viewport ? *viewport : *shState->screen());
	onViewportChange();
	onGeometryChange(scene->getGeometry());
}

void ViewportElement::viewportElementDisposal()
{
	viewportDispCon.disconnect();
	if(m_dispose != nullptr)
		m_dispose(this);
}

ViewportElement::~ViewportElement()
{
	viewportDispCon.disconnect();
}

#ifdef MKXPZ_RETRO
#ifndef MKXPZ_SANDBOX_SERIAL_VIEWPORT_H
#define MKXPZ_SANDBOX_SERIAL_VIEWPORT_H
#include "sandbox-serial-viewport.h"
#endif // MKXPZ_SANDBOX_SERIAL_VIEWPORT_H
#endif // MXKPZ_RETRO
