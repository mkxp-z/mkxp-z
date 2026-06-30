/*
** window.h
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

#ifndef WINDOW_H
#define WINDOW_H

#include "viewport.h"
#include "disposable.h"

#include "util.h"

#ifdef MKXPZ_RETRO
#  include "wasm-types.h"
#endif // MKXPZ_RETRO

class Bitmap;
struct Rect;

struct WindowPrivate;

class Window : public ViewportElement, public Disposable
{
public:
	Window(Viewport *viewport = 0);
	~Window();

	void update(Exception &exception);

	DECL_ATTR( Windowskin,      Bitmap* )
	DECL_ATTR( Contents,        Bitmap* )
	DECL_ATTR( Stretch,         bool    )
	DECL_ATTR( CursorRect,      Rect&   )
	DECL_ATTR( Active,          bool    )
	DECL_ATTR( Pause,           bool    )
	DECL_ATTR( X,               int     )
	DECL_ATTR( Y,               int     )
	DECL_ATTR( Width,           int     )
	DECL_ATTR( Height,          int     )
	DECL_ATTR( OX,              int     )
	DECL_ATTR( OY,              int     )
	DECL_ATTR( Opacity,         int     )
	DECL_ATTR( BackOpacity,     int     )
	DECL_ATTR( ContentsOpacity, int     )

	void initDynAttribs();

	const IntRect *sceneRect() const noexcept;
	const Vec2i *sceneOrig() const noexcept;

	void setZ(Exception &exception, int value);
	void setVisible(Exception &exception, bool value);

#ifdef MKXPZ_RETRO
	bool sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const;
	bool sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size);
	void sandbox_deserialize_begin();
	void sandbox_deserialize_end();
	void sandbox_reinit();
#endif // MKXPZ_RETRO

private:
	WindowPrivate *p;

	void draw(Exception &exception);
	void onGeometryChange(const Scene::Geometry &);

	void onViewportChange();

	void releaseResources();
	const char *klassName() const { return "window"; }

	ABOUT_TO_ACCESS_DISP
};

#endif // WINDOW_H
