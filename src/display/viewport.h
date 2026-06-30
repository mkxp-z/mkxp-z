/*
** viewport.h
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

#ifndef VIEWPORT_H
#define VIEWPORT_H

#include "scene.h"
#include "flashable.h"
#include "disposable.h"
#include "util.h"

#ifdef MKXPZ_RETRO
#  include "wasm-types.h"
#endif // MKXPZ_RETRO

struct ViewportPrivate;

class Viewport : public Scene, public SceneElement, public Flashable, public Disposable
{
public:
	Viewport(int x, int y, int width, int height);
	Viewport(Rect *rect);
	Viewport();
	~Viewport();

	void update(Exception &exception);

	DECL_ATTR( Rect,  Rect&  )
	DECL_ATTR( OX,    int    )
	DECL_ATTR( OY,    int    )
	DECL_ATTR( Color, Color& )
	DECL_ATTR( Tone,  Tone&  )

	void initDynAttribs();

#ifdef MKXPZ_RETRO
	const uint64_t id; // Globally unique nonzero ID for this viewport, for change detection during save state deserialization

	bool sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const;
	bool sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size);
	void sandbox_deserialize_begin();
	void sandbox_deserialize_end();
#endif // MXKPZ_RETRO

private:
	void initViewport(int x, int y, int width, int height);
	void geometryChanged();

	void composite(Exception &exception);
	void draw(Exception &exception);
	void onGeometryChange(const Geometry &);
	bool isEffectiveViewport(Rect *&, Color *&, Tone *&) const;

	void releaseResources();
	const char *klassName() const { return "viewport"; }

	ABOUT_TO_ACCESS_DISP

	ViewportPrivate *p;
	friend struct ViewportPrivate;

	IntruListLink<Scene> sceneLink;
};

class ViewportElement : public SceneElement
{
public:
	ViewportElement(void (*dispose)(void *), Viewport *viewport = 0, int z = 0, int spriteY = 0);
	~ViewportElement();

	DECL_ATTR_NOEXCEPT( Viewport,  Viewport* )

#ifdef MKXPZ_RETRO
	bool sandbox_serialize_viewport_element(void *&data, mkxp_sandbox::wasm_size_t &max_size) const;
	bool sandbox_deserialize_viewport_element(const void *&data, mkxp_sandbox::wasm_size_t &max_size);
	void sandbox_deserialize_begin_viewport_element();
	void sandbox_deserialize_end_viewport_element();
#endif // MXKPZ_RETRO

protected:
	virtual void onViewportChange() {}

private:
	void (*m_dispose)(void *);
	Viewport *m_viewport;
	sigslot::connection viewportDispCon;
#ifdef MKXPZ_RETRO
	uint64_t deserSavedViewportId;
#endif // MKXPZ_RETRO
	void viewportElementDisposal();
};

#endif // VIEWPORT_H
