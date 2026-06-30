/*
** sprite.h
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

#ifndef SPRITE_H
#define SPRITE_H

#include "scene.h"
#include "flashable.h"
#include "disposable.h"
#include "viewport.h"
#include "util.h"

#ifdef MKXPZ_RETRO
#  include "wasm-types.h"
#endif // MKXPZ_RETRO

class Bitmap;
struct Color;
struct Tone;
struct Rect;

struct SpritePrivate;

class Sprite : public ViewportElement, public Flashable, public Disposable
{
public:
	Sprite(Viewport *viewport = 0);
	~Sprite();

	int getWidth(Exception &exception)  const;
	int getHeight(Exception &exception) const;

	void update(Exception &exception);

	DECL_ATTR( Bitmap,      Bitmap* )
	DECL_ATTR( SrcRect,     Rect&   )
	DECL_ATTR( X,           int     )
	DECL_ATTR( Y,           int     )
	DECL_ATTR( OX,          int     )
	DECL_ATTR( OY,          int     )
	DECL_ATTR( ZoomX,       float   )
	DECL_ATTR( ZoomY,       float   )
	DECL_ATTR( Angle,       float   )
	DECL_ATTR( Mirror,      bool    )
	DECL_ATTR( BushDepth,   int     )
	DECL_ATTR( BushOpacity, int     )
	DECL_ATTR( Opacity,     int     )
	DECL_ATTR( BlendType,   int     )
	DECL_ATTR( Color,       Color&  )
	DECL_ATTR( Tone,        Tone&   )
    DECL_ATTR( Pattern,     Bitmap* )
    DECL_ATTR( PatternBlendType, int)
    DECL_ATTR( PatternTile, bool    )
    DECL_ATTR( PatternOpacity, int  )
    DECL_ATTR( PatternScrollX, int  )
    DECL_ATTR( PatternScrollY, int  )
    DECL_ATTR( PatternZoomX, float  )
    DECL_ATTR( PatternZoomY, float  )
    DECL_ATTR( Invert,      bool    )
	DECL_ATTR( WaveAmp,     int     )
	DECL_ATTR( WaveLength,  int     )
	DECL_ATTR( WaveSpeed,   int     )
	DECL_ATTR( WavePhase,   float   )

	void initDynAttribs();

	const IntRect *sceneRect() const noexcept;
	const Vec2i *sceneOrig() const noexcept;

#ifdef MKXPZ_RETRO
	bool sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const;
	bool sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size);
	void sandbox_deserialize_begin();
	void sandbox_deserialize_end();
	void sandbox_reinit();
#endif // MKXPZ_RETRO

private:
	SpritePrivate *p;

	void draw(Exception &exception);
	void onGeometryChange(const Scene::Geometry &);

	void releaseResources();
	const char *klassName() const { return "sprite"; }

	ABOUT_TO_ACCESS_DISP
};

#endif // SPRITE_H
