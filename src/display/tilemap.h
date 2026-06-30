/*
** tilemap.h
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

#ifndef TILEMAP_H
#define TILEMAP_H

#include "disposable.h"

#include "util.h"

#ifdef MKXPZ_RETRO
#  include "wasm-types.h"
#endif // MKXPZ_RETRO

class Viewport;
class Bitmap;
class Table;

struct Color;
struct Tone;

struct TilemapPrivate;

class Tilemap : public Disposable
{
public:
	class Autotiles
	{
	public:
		Autotiles() : tilemap(nullptr) {}
		Autotiles(Tilemap *tilemap) : tilemap(tilemap) {}
		~Autotiles() { if (tilemap) tilemap->atProxy = nullptr; }

		void set(int i, Bitmap *bitmap);
		Bitmap *get(int i) const;

#ifdef MKXPZ_RETRO
		bool sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const;
		bool sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size);
#endif // MKXPZ_RETRO

	private:
		Tilemap *tilemap;
		friend class Tilemap;
		friend struct TilemapPrivate;
	};

	Tilemap(Viewport *viewport = 0);
	~Tilemap();

	void update(Exception &exception);

	Autotiles *getAutotiles(Exception &exception);
	Viewport *getViewport(Exception &exception) const;

	DECL_ATTR( Tileset,    Bitmap*   )
	DECL_ATTR( MapData,    Table*    )
	DECL_ATTR( FlashData,  Table*    )
	DECL_ATTR( Priorities, Table*    )
	DECL_ATTR( Visible,    bool      )
	DECL_ATTR( OX,         int       )
	DECL_ATTR( OY,         int       )

	DECL_ATTR( Opacity,   int     )
	DECL_ATTR( BlendType, int     )
	DECL_ATTR( Color,     Color&  )
	DECL_ATTR( Tone,      Tone&   )

	void initDynAttribs();

#ifdef MKXPZ_RETRO
	bool sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const;
	bool sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size);
	void sandbox_deserialize_begin(bool is_new);
	void sandbox_deserialize_end();
	void sandbox_reinit();
#endif // MKXPZ_RETRO

private:
	TilemapPrivate *p;
	Autotiles *atProxy;

	void releaseResources();
	const char *klassName() const { return "tilemap"; }
};

#endif // TILEMAP_H
