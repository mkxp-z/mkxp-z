/*
** font.h
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

#ifndef FONT_H
#define FONT_H

#include "etc.h"
#include "util.h"
#include "filesystem.h"

#ifdef MKXPZ_RETRO
#  include <ft2build.h>
#  include FT_FREETYPE_H
#  include "wasm-types.h"
#endif // MKXPZ_RETRO

#include <vector>
#include <string>

struct SDL_RWops;
struct _TTF_Font;
struct Config;

struct SharedFontStatePrivate;

class SharedFontState
{
public:
	SharedFontState(const Config &conf);
	~SharedFontState();

	/* Called from FileSystem during font cache initialization
	 * (when "Fonts/" is scanned for available assets).
	 * 'ops' is an opened handle to a possible font file,
	 * 'filename' is the corresponding path */
	void initFontSetCB(
#ifdef MKXPZ_RETRO
	                   std::shared_ptr<struct FileSystem::File> ops,
#else
	                   SDL_RWops &ops,
#endif // MKXPZ_RETRO
	                   const std::string &filename);

#ifdef MKXPZ_RETRO
	FT_Face
#else
	_TTF_Font *
#endif // MKXPZ_RETRO
	getFont(Exception &exception, std::string family,
	                   int size, float hiresMult, int outline_size = 0);

	bool fontPresent(std::string family) const;

#ifndef MKXPZ_RETRO
	static _TTF_Font *openBundled(int size);
#endif // MKXPZ_RETRO
    void setDefaultFontFamily(const std::string &family);

#ifdef MKXPZ_RETRO
	FT_Library getLibrary() const noexcept;
#endif // MKXPZ_RETRO

private:
	SharedFontStatePrivate *p;
};

struct FontPrivate;

class Font
{
public:
	static bool doesExist(const char *name);

	Font(const std::vector<std::string> *names = 0,
	     int size = 0);

	/* Clone constructor */
	Font(const Font &other);

	~Font();

	const Font &operator=(const Font &o);

	int getSize() const;
	void setSizeNoCheck(int value);
	void setSize(Exception &exception, int value, bool checkIllegal=true);
	void setHiresMult(float value);

	DECL_ATTR_NOEXCEPT( Bold,     bool   )
	DECL_ATTR_NOEXCEPT( Italic,   bool   )
	DECL_ATTR_NOEXCEPT( Color,    Color& )
	DECL_ATTR_NOEXCEPT( Shadow,   bool   )
	DECL_ATTR_NOEXCEPT( Outline,  bool   )
	DECL_ATTR_NOEXCEPT( OutColor, Color& )

	DECL_ATTR_NOEXCEPT_STATIC( DefaultSize,     int    )
	DECL_ATTR_NOEXCEPT_STATIC( DefaultBold,     bool   )
	DECL_ATTR_NOEXCEPT_STATIC( DefaultItalic,   bool   )
	DECL_ATTR_NOEXCEPT_STATIC( DefaultColor,    Color& )
	DECL_ATTR_NOEXCEPT_STATIC( DefaultShadow,   bool   )
	DECL_ATTR_NOEXCEPT_STATIC( DefaultOutline,  bool   )
	DECL_ATTR_NOEXCEPT_STATIC( DefaultOutColor, Color& )

	/* There is no point in providing getters for these,
	 * as the bindings will always return the stored native
	 * string/array object anyway. It's impossible to mirror
	 * in the C++ core.
	 * The core object picks the first existing name from the
	 * passed array and stores it internally (same for default). */
	void setName(const std::vector<std::string> &names);
	static void setDefaultName(const std::vector<std::string> &names,
	                           const SharedFontState &sfs);

	static const std::vector<std::string> &getInitialDefaultNames();
    bool isSolid() const;

	/* Assigns heap allocated objects to object properties;
	 * using this in pure C++ will cause memory leaks
	 * (ie. only to be used in GCed language bindings) */
	void initDynAttribs();
	static void initDefaultDynAttribs();

	static void initDefaults(const SharedFontState &sfs);

	/* internal */
#ifdef MKXPZ_RETRO
	FT_Face
#else
	_TTF_Font *
#endif // MKXPZ_RETRO
	getSdlFont(Exception &exception, int outline_size);

#ifdef MKXPZ_RETRO
	bool sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const;
	bool sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size);
	static bool sandbox_serialize_default(void *&data, mkxp_sandbox::wasm_size_t &max_size);
	static bool sandbox_deserialize_default(const void *&data, mkxp_sandbox::wasm_size_t &max_size);
#endif // MKXPZ_RETRO

private:
	FontPrivate *p;
};

#endif // FONT_H
