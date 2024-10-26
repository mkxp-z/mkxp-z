/*
** font.cpp
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

#include "font.h"

#include "sharedstate.h"
#include "filesystem.h"
#include "exception.h"
#include "boost-hash.h"
#include "util.h"
#include "config.h"

#include "debugwriter.h"

#include <string>
#include <utility>
#include <algorithm>
#include <cctype>
#include <array>

#ifdef MKXPZ_BUILD_XCODE
#include "filesystem/filesystem.h"
#endif

#include <SDL_ttf.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H

#ifndef MKXPZ_BUILD_XCODE
#ifndef MKXPZ_CJK_FONT
#include "liberation.ttf.xxd"
#else
#include "wqymicrohei.ttf.xxd"
#endif


#ifndef MKXPZ_CJK_FONT
#define BUNDLED_FONT liberation
#else
#define BUNDLED_FONT wqymicrohei
#endif

#define BUNDLED_FONT_DECL(FONT) \
	extern unsigned char ___assets_##FONT##_ttf[]; \
	extern unsigned int ___assets_##FONT##_ttf_len;

BUNDLED_FONT_DECL(liberation)

#define BUNDLED_FONT_D(f) ___assets_## f ##_ttf
#define BUNDLED_FONT_L(f) ___assets_## f ##_ttf_len

// Go fuck yourself CPP
#define BNDL_F_D(f) BUNDLED_FONT_D(f)
#define BNDL_F_L(f) BUNDLED_FONT_L(f)

#endif

static SDL_RWops *openBundledFont()
{
#ifndef MKXPZ_BUILD_XCODE
    return SDL_RWFromConstMem(BNDL_F_D(BUNDLED_FONT), BNDL_F_L(BUNDLED_FONT));
#else
    return SDL_RWFromFile(mkxp_fs::getPathForAsset("Fonts/liberation", "ttf").c_str(), "rb");
#endif
}


/* <name, size> */
typedef std::pair<std::string, int> FontSizeKey;
/* <name, ppem> */
typedef std::pair<std::string, int> FontPPEMKey;

struct FontSet
{
	/* 'Regular' style */
	std::string regular;

	/* Any other styles (used in case no 'Regular' exists) */
	std::string other;
};

struct SharedFontStatePrivate
{
	/* Maps: font family name, To: substituted family name,
	 * as specified via configuration file / arguments */
	BoostHash<std::string, std::string> subs;

	/* Maps: font family name, To: set of physical
	 * font filenames located in "Fonts/" */
	BoostHash<std::string, FontSet> sets;

	/* Pool of font size to ppem values */
	BoostHash<FontSizeKey, int> size_to_ppem;

	/* Pool of already opened fonts; once opened, they are reused
	 * and never closed until the termination of the program */
	BoostHash<FontPPEMKey, std::array<TTF_Font*, 2>> ppem_to_font;
    
    /* Internal default font family that is used anytime an
     * empty/invalid family is requested */
    std::string defaultFamily;

	int fontSizeMethod;
	float fontScale;
	bool fontKerning;
};

SharedFontState::SharedFontState(const Config &conf)
{
	p = new SharedFontStatePrivate;

	/* Parse font substitutions */
	for (size_t i = 0; i < conf.fontSubs.size(); ++i)
	{
		const std::string &raw = conf.fontSubs[i];
		size_t sepPos = raw.find_first_of('>');

		if (sepPos == std::string::npos)
			continue;

		std::string from = raw.substr(0, sepPos);
		std::string to   = raw.substr(sepPos+1);

		p->subs.insert(from, to);
	}
	
	p->fontSizeMethod = conf.fontSizeMethod;
	if (!p->fontSizeMethod)
	{
		if (rgssVer == 1)
			p->fontSizeMethod = 1;
		else
			p->fontSizeMethod = 2;
	}
	p->fontScale = conf.fontScale;
	if (p->fontScale < 0.1f)
	{
		if (p->fontSizeMethod == 1)
			p->fontScale = 0.9f;
		else
			p->fontScale = 1.0f;
	}
	p->fontKerning = conf.fontKerning;
}

SharedFontState::~SharedFontState()
{
	BoostHash<FontPPEMKey, std::array<TTF_Font*, 2>>::const_iterator iter;
	for (iter = p->ppem_to_font.cbegin(); iter != p->ppem_to_font.cend(); ++iter)
	{
		for (int i=0; i < iter->second.size(); i++)
			if (iter->second[i] != 0)
				TTF_CloseFont(iter->second[i]);
	}

	delete p;
}

void SharedFontState::initFontSetCB(SDL_RWops &ops,
                                    const std::string &filename)
{
	TTF_Font *font = TTF_OpenFontRW(&ops, 0, 0);

	if (!font)
		return;

	std::string family = TTF_FontFaceFamilyName(font);
	std::string style = TTF_FontFaceStyleName(font);

	std::transform(family.begin(), family.end(), family.begin(),
		[](unsigned char c){ return std::tolower(c); });

	TTF_CloseFont(font);

	FontSet &set = p->sets[family];

	if (style == "Regular" && set.regular.empty())
		set.regular = filename;
	else if (style != "Regular" && set.other.empty())
		set.other = filename;
}

/* The following code was taken from Wine to emulate
 * Windows's font size selection behavior. */
 
/* We're not currently using yMax and yMin for anything,
 * but it could be useful later. */
typedef struct {
	TTF_Font *font;
	int ppem;
	short yMax;
	short yMin;
} Font_Container;

#define BYTE uint8_t
#define WORD uint16_t
#define DWORD uint32_t
#define UINT unsigned int
#define SHORT short
#define USHORT unsigned short
#define GDI_ERROR ~0u

#define MS_MAKE_TAG(ch0, ch1, ch2, ch3)                                                 \
                    ((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) |       \
                    ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24))
#define MS_VDMX_TAG MS_MAKE_TAG('V', 'D', 'M', 'X')

/* Wine's code suggests the tables are stored in big endian format. */
#define RTLUSHORTBYTESWAP(x) (uint16_t)((x >> 8) | (x << 8))
#define RTLULONGBYTESWAP(x) (((uint32_t)RTLUSHORTBYTESWAP((uint16_t)x) << 16) | RTLUSHORTBYTESWAP((uint16_t)(x >> 16)))

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define GET_BE_WORD(x) (x)
#else
#define GET_BE_WORD(x) RTLUSHORTBYTESWAP(x)
#endif

static unsigned int freetype_get_font_data( Font_Container *font, uint32_t table,
                                            unsigned int offset, void *buf, unsigned int cbData)
{
	FT_Face ft_face = *(reinterpret_cast<FT_Face *>( font->font ));
	FT_ULong len;
	FT_Error err;

	if (!FT_IS_SFNT(ft_face)) return GDI_ERROR;

	if(!buf)
		len = 0;
	else
		len = cbData;

	/* MS tags differ in endianness from FT ones */
	table = RTLULONGBYTESWAP( table );

	/* make sure value of len is the value freetype says it needs */
	if (buf && len)
	{
		FT_ULong needed = 0;
		err = FT_Load_Sfnt_Table(ft_face, table, offset, NULL, &needed);
		if(!err && needed < len)
			len = needed;
	}
	err = FT_Load_Sfnt_Table(ft_face, table, offset, (FT_Byte*)buf, &len);
	if (err) /* Can't find table */
		return GDI_ERROR;
	return (int)len;
}

typedef struct {
	uint16_t version;
	uint16_t numRecs;
	uint16_t numRatios;
} VDMX_Header;

typedef struct {
	uint8_t bCharSet;
	uint8_t xRatio;
	uint8_t yStartRatio;
	uint8_t yEndRatio;
} Ratios;

typedef struct {
	uint16_t recs;
	uint8_t startsz;
	uint8_t endsz;
} VDMX_group;

typedef struct {
	uint16_t yPelHeight;
	uint16_t yMax;
	uint16_t yMin;
} VDMX_vTable;

static int load_VDMX(Font_Container *font, int height)
{
	VDMX_Header hdr;
	VDMX_group group;
	uint8_t devXRatio, devYRatio;
	unsigned short numRatios;
	unsigned int result, offset = -1;
	int i, ppem = 0;

	result = freetype_get_font_data(font, MS_VDMX_TAG, 0, &hdr, sizeof(hdr));

	if(result == GDI_ERROR) /* no vdmx table present, use linear scaling */
		return ppem;

	/* FIXME: need the real device aspect ratio */
	devXRatio = 1;
	devYRatio = 1;

	numRatios = GET_BE_WORD(hdr.numRatios);

	for(i = 0; i < numRatios; i++) {
		Ratios ratio;

		offset = sizeof(hdr) + (i * sizeof(Ratios));
		freetype_get_font_data(font, MS_VDMX_TAG, offset, &ratio, sizeof(Ratios));
		offset = -1;

		if (!ratio.bCharSet)
			continue;

		if((ratio.xRatio == 0 &&
			ratio.yStartRatio == 0 &&
			ratio.yEndRatio == 0) ||
		   (devXRatio == ratio.xRatio &&
			devYRatio >= ratio.yStartRatio &&
			devYRatio <= ratio.yEndRatio))
		{
			uint16_t group_offset;

			offset = sizeof(hdr) + numRatios * sizeof(ratio) + i * sizeof(group_offset);
			freetype_get_font_data(font, MS_VDMX_TAG, offset, &group_offset, sizeof(group_offset));
			offset = GET_BE_WORD(group_offset);
			break;
		}
	}

	if(offset == -1) return 0;

	if(freetype_get_font_data(font, MS_VDMX_TAG, offset, &group, sizeof(group)) != GDI_ERROR) {
		uint16_t recs;
		std::vector<VDMX_vTable> vTable;

		recs = GET_BE_WORD(group.recs);

		vTable.resize(recs);
		result = freetype_get_font_data(font, MS_VDMX_TAG, offset + sizeof(group), &vTable[0], recs * sizeof(VDMX_vTable));
		if(result == GDI_ERROR) /* Failed to retrieve vTable */
			return 0;

		for(i = 0; i < recs; i++) {
			VDMX_vTable &entry = vTable[i];
			short yMax = GET_BE_WORD(entry.yMax);
			short yMin = GET_BE_WORD(entry.yMin);
			ppem = GET_BE_WORD(entry.yPelHeight);

			if(yMax + -yMin == height) {
				font->yMax = yMax;
				font->yMin = yMin;
				break;
			}
			if(yMax + -yMin > height) {
				if(--i < 0) {
					ppem = 0;
					return 0; /* failed */
				}
				VDMX_vTable &entry = vTable[i];
				font->yMax = GET_BE_WORD(entry.yMax);
				font->yMin = GET_BE_WORD(entry.yMin);
				ppem = GET_BE_WORD(entry.yPelHeight);
				break;
			}
		}
		if(!font->yMax) /* ppem not found for height */
			ppem = 0;
	}

	return ppem;
}

/* Some fonts have large usWinDescent values, as a result of storing signed short
   in unsigned field. That's probably caused by sTypoDescent vs usWinDescent confusion in
   some font generation tools. */
static inline USHORT get_fixed_windescent(USHORT windescent)
{
    return abs((SHORT)windescent);
}

static int calc_ppem_for_height(Font_Container *font, int height)
{
	FT_Face ft_face = *(reinterpret_cast<FT_Face *>( font->font ));
	TT_OS2 *pOS2;
	TT_HoriHeader *pHori;

	int ppem;
	const int MAX_PPEM = (1 << 16) - 1;

	pOS2 = (TT_OS2 *)FT_Get_Sfnt_Table(ft_face, FT_SFNT_OS2);
	pHori = (TT_HoriHeader *)FT_Get_Sfnt_Table(ft_face, FT_SFNT_HHEA);

	if(height == 0)
		height = 16;

	/* Calc. height of EM square:
	 *
	 * For +ve lfHeight we have
	 * lfHeight = (winAscent + winDescent) * ppem / units_per_em
	 * Re-arranging gives:
	 * ppem = units_per_em * lfheight / (winAscent + winDescent)
	 *
	 * For -ve lfHeight we have
	 * |lfHeight| = ppem
	 * [i.e. |lfHeight| = (winAscent + winDescent - il) * ppem / units_per_em
	 * with il = winAscent + winDescent - units_per_em]
	 *
	 */

	if(height > 0) {
		USHORT windescent = get_fixed_windescent(pOS2->usWinDescent);
		int units;

		if(pOS2->usWinAscent + windescent == 0)
		{
			font->yMax = pHori->Ascender;
			font->yMin = pHori->Descender;
			units = pHori->Ascender - pHori->Descender;
		} else {
			font->yMax = pOS2->usWinAscent;
			font->yMin = -windescent;
			units = pOS2->usWinAscent + windescent;
		}
		ppem = (int)FT_MulDiv(ft_face->units_per_EM, height, units);

		/* If rounding ends up getting a font exceeding height, choose a smaller ppem */
		if(ppem > 1 && FT_MulDiv(units, ppem, ft_face->units_per_EM) > height)
			--ppem;

		if(ppem > MAX_PPEM) {
			//WARN("Ignoring too large height %d, ppem %d\n", height, ppem);
			ppem = 1;
		}
	}
	else if(height >= -MAX_PPEM)
		ppem = -height;
	else {
		//WARN("Ignoring too large height %d\n", height);
		ppem = 1;
	}

	return ppem;
}
/* /wine */

_TTF_Font *SharedFontState::getFont(std::string family,
                                    int size, float hiresMult, int outline_size)
{
	std::transform(family.begin(), family.end(), family.begin(),
		[](unsigned char c){ return std::tolower(c); });

	if (family.empty())
		family = p->defaultFamily;

	/* Check for substitutions */
	if (p->subs.contains(family))
		family = p->subs[family];

	/* Find out if the font asset exists */
	const FontSet &req = p->sets[family];

	if (req.regular.empty() && req.other.empty())
	{
		/* Doesn't exist; use built-in font */
		family = "";
	}

	FontSizeKey key(family, size);

	TTF_Font *font;
	int &ppem = p->size_to_ppem[key];
	int ppemMult;
	
	if (ppem != 0)
	{
		ppemMult = std::max<int>(ppem * hiresMult, 1);
		auto &group = p->ppem_to_font[FontPPEMKey(family, ppemMult)];
		if(outline_size == 0)
			font = group[0];
		else
			font = group[1];
		
		if (font)
		{
			if(outline_size && TTF_GetFontOutline(font) != outline_size)
				TTF_SetFontOutline(font, outline_size);
			return font;
		}
	}
	
	/* Not in pool; open new handle */
	SDL_RWops *ops;

	if (family.empty())
	{
		/* Built-in font */
		ops = openBundledFont();
	}
	else
	{
		/* Use 'other' path as alternative in case
		 * we have no 'regular' styled font asset */
		const char *path = !req.regular.empty()
		                 ? req.regular.c_str() : req.other.c_str();

		ops = SDL_AllocRW();
		try{
			shState->fileSystem().openReadRaw(*ops, path, true);
		} catch (const Exception &e) {
			SDL_FreeRW(ops);
			p->size_to_ppem.remove(key);
			throw e;
		}
	}

	// Pokemon Essentials games were made with the old font size method in mind,
	// so we default to it for all XP games.
	if(p->fontSizeMethod == 1)
	{
		if (ppem == 0)
		{
			ppem = std::max<int>(size * p->fontScale, 5);
			ppemMult = std::max<int>(ppem * hiresMult, 1);
		}
		font = TTF_OpenFontRW(ops, 1, ppemMult);
	} else {
		/* Try to compute the size the same way Windows does. */
		font = TTF_OpenFontRW(ops, 1, 0);

		if (font)
		{
			/* Dirty hack to get the FT_Face.
			 * SDL_ttf will probably never move it from the beginning of the struct. */
			FT_Face face= *(reinterpret_cast<FT_Face *>( font ));
			/* This is should always be true, but we may as well check... */
			if (FT_IS_SCALABLE( face ))
			{
				if (ppem == 0)
				{
					Font_Container c = { 0 };
					c.font = font;
					c.ppem = load_VDMX(&c, size);
					if (!c.ppem)
						c.ppem = calc_ppem_for_height( &c, size );
					
					ppem = std::max<int>(c.ppem * p->fontScale, 1);
					ppemMult = std::max<int>(ppem * hiresMult, 1);
				}
				if (TTF_SetFontSize(font, ppemMult))
				{
					TTF_CloseFont(font);
					font = 0;
				}
			} else {
				/* Someone must have renamed a non-scalable font file to ttf or otf.
				 * Wine has a scaling setup for these, but I'll just fall back to
				 * the mkxp method for now. */
				if (ppem == 0)
				{
					ppem = std::max<int>(size * p->fontScale, 5);
					ppemMult = std::max<int>(ppem * hiresMult, 1);
				}
				if (TTF_SetFontSize(font, ppemMult))
				{
					TTF_CloseFont(font);
					font = 0;
				}
			}
			if (font)
			{
				/* RGSS doesn't use font hinting */
				TTF_SetFontHinting(font, TTF_HINTING_NONE);
			}
		}
	}
	
	if (!font)
	{
		p->size_to_ppem.remove(key);
		throw Exception(Exception::SDLError, "%s", SDL_GetError());
	}
	
	auto &group = p->ppem_to_font[FontPPEMKey(family, std::max<int>(ppem * hiresMult, 1))];
	if(outline_size == 0)
	{
		group[0] = font;
	} else {
		if(TTF_GetFontOutline(font) != outline_size)
			TTF_SetFontOutline(font, outline_size);
		group[1] = font;
	}
	
	if (!p->fontKerning)
		TTF_SetFontKerning(font, 0);
	
	return font;
}

bool SharedFontState::fontPresent(std::string family) const
{
	std::transform(family.begin(), family.end(), family.begin(),
		[](unsigned char c){ return std::tolower(c); });

	/* Check for substitutions */
	if (p->subs.contains(family))
		family = p->subs[family];

	const FontSet &set = p->sets[family];

	return !(set.regular.empty() && set.other.empty());
}

_TTF_Font *SharedFontState::openBundled(int size)
{
	SDL_RWops *ops = openBundledFont();

	return TTF_OpenFontRW(ops, 1, size);
}

void SharedFontState::setDefaultFontFamily(const std::string &family) {
    p->defaultFamily = family;
}

static bool pickExistingFontName(const std::vector<std::string> &names,
                          std::string &out,
                          const SharedFontState &sfs)
{
	/* Note: In RMXP, a names array with no existing entry
	 * results in no text being drawn at all (same for "" and []);
	 * we can't replicate this in mkxp due to the default substitute. */

	for (size_t i = 0; i < names.size(); ++i)
	{
		if (sfs.fontPresent(names[i]))
		{
			if (out == names[i])
				return false;
			out = names[i];
			return true;
		}
		else
		{
			if (i == 0)
			{
				Debug() << "Primary font not found:" << names[i];
			}
			else
			{
				Debug() << "Fallback font not found:" << names[i];
			}
		}
	}

	if (out[0] == '\0')
		return false;
	out = "";
	return true;
}


struct FontPrivate
{
	std::string name;
	int size;
	float hiresMult;
	bool bold;
	bool italic;
	bool outline;
	bool shadow;
	Color *color;
	Color *outColor;

	Color colorTmp;
	Color outColorTmp;

	static std::string defaultName;
	static int defaultSize;
	static bool defaultBold;
	static bool defaultItalic;
	static bool defaultOutline;
	static bool defaultShadow;
	static Color *defaultColor;
	static Color *defaultOutColor;

	static Color defaultColorTmp;
	static Color defaultOutColorTmp;

	static std::vector<std::string> initialDefaultNames;

	/* The actual font is opened as late as possible
	 * (when it is queried by a Bitmap), prior it is
	 * set to null */
	TTF_Font *sdlFont;
	TTF_Font *sdlFontOutline;
    
    bool isSolid;

	FontPrivate(int size)
	    : size(size),
	      hiresMult(1.0f),
	      bold(defaultBold),
	      italic(defaultItalic),
	      outline(defaultOutline),
	      shadow(defaultShadow),
	      color(&colorTmp),
	      outColor(&outColorTmp),
	      colorTmp(*defaultColor),
	      outColorTmp(*defaultOutColor),
	      sdlFont(0),
	      sdlFontOutline(0),
          isSolid(false)
	{}

	FontPrivate(const FontPrivate &other)
	    : name(other.name),
	      size(other.size),
	      hiresMult(1.0f),
	      bold(other.bold),
	      italic(other.italic),
	      outline(other.outline),
	      shadow(other.shadow),
	      color(&colorTmp),
	      outColor(&outColorTmp),
	      colorTmp(*other.color),
	      outColorTmp(*other.outColor),
	      sdlFont(other.sdlFont),
	      sdlFontOutline(other.sdlFontOutline),
          isSolid(false)
	{}

	void operator=(const FontPrivate &o)
	{
		if (size != o.size || name != o.name)
		{
			sdlFont = 0;
			sdlFontOutline = 0;
		}
		if (hiresMult == o.hiresMult)
		{
			sdlFont = sdlFont == 0 ? o.sdlFont : sdlFont;
			sdlFontOutline = sdlFontOutline == 0 ? o.sdlFontOutline : sdlFontOutline;
		}

		 name     =  o.name;
		 size     =  o.size;
		 bold     =  o.bold;
		 italic   =  o.italic;
		 outline  =  o.outline;
		 shadow   =  o.shadow;
		*color    = *o.color;
		*outColor = *o.outColor;

        isSolid = o.isSolid;
	}
};

std::string FontPrivate::defaultName     = "Arial";
int         FontPrivate::defaultSize     = 22;
bool        FontPrivate::defaultBold     = false;
bool        FontPrivate::defaultItalic   = false;
bool        FontPrivate::defaultOutline  = false; /* Inited at runtime */
bool        FontPrivate::defaultShadow   = false; /* Inited at runtime */
Color      *FontPrivate::defaultColor    = &FontPrivate::defaultColorTmp;
Color      *FontPrivate::defaultOutColor = &FontPrivate::defaultOutColorTmp;

Color FontPrivate::defaultColorTmp(255, 255, 255, 255);
Color FontPrivate::defaultOutColorTmp(0, 0, 0, 128);

std::vector<std::string> FontPrivate::initialDefaultNames;

bool Font::isSolid() const {
    return p->isSolid;
}

bool Font::doesExist(const char *name)
{
	if (!name)
		return false;

	return shState->fontState().fontPresent(name);
}

Font::Font(const std::vector<std::string> *names,
           int size)
{
	p = new FontPrivate(size ? size : FontPrivate::defaultSize);

	if (names)
		setName(*names);
	else
		p->name = FontPrivate::defaultName;
}

Font::Font(const Font &other)
{
	p = new FontPrivate(*other.p);
}

Font::~Font()
{
	delete p;
}

const Font &Font::operator=(const Font &o)
{
	*p = *o.p;

	return o;
}

void Font::setName(const std::vector<std::string> &names)
{
	if (pickExistingFontName(names, p->name, shState->fontState()))
	{
		p->sdlFont = 0;
		p->sdlFontOutline = 0;
	}
	p->isSolid = strcmp(p->name.c_str(), "") && shState->config().fontIsSolid(p->name.c_str());
}

void Font::setSize(int value, bool checkIllegal)
{
	if (p->size == value)
		return;

	/* Catch illegal values (according to RMXP) */
	if (value < 6 || value > 96) {
		if (checkIllegal) {
			throw Exception(Exception::ArgumentError, "%s", "bad value for size");
		}
	}

	p->size = value;
	p->sdlFont = 0;
	p->sdlFontOutline = 0;
}

void Font::setHiresMult(float value)
{
	if (p->hiresMult == value)
		return;

	p->hiresMult = value;
	p->sdlFont = 0;
	p->sdlFontOutline = 0;
}

static void guardDisposed() {}

DEF_ATTR_RD_SIMPLE(Font, Size, int, p->size)

DEF_ATTR_SIMPLE(Font, Bold,     bool,    p->bold)
DEF_ATTR_SIMPLE(Font, Italic,   bool,    p->italic)
DEF_ATTR_SIMPLE(Font, Shadow,   bool,    p->shadow)
DEF_ATTR_SIMPLE(Font, Outline,  bool,    p->outline)
DEF_ATTR_SIMPLE(Font, Color,    Color&, *p->color)
DEF_ATTR_SIMPLE(Font, OutColor, Color&, *p->outColor)

DEF_ATTR_SIMPLE_STATIC(Font, DefaultSize,     int,     FontPrivate::defaultSize)
DEF_ATTR_SIMPLE_STATIC(Font, DefaultBold,     bool,    FontPrivate::defaultBold)
DEF_ATTR_SIMPLE_STATIC(Font, DefaultItalic,   bool,    FontPrivate::defaultItalic)
DEF_ATTR_SIMPLE_STATIC(Font, DefaultShadow,   bool,    FontPrivate::defaultShadow)
DEF_ATTR_SIMPLE_STATIC(Font, DefaultOutline,  bool,    FontPrivate::defaultOutline)
DEF_ATTR_SIMPLE_STATIC(Font, DefaultColor,    Color&, *FontPrivate::defaultColor)
DEF_ATTR_SIMPLE_STATIC(Font, DefaultOutColor, Color&, *FontPrivate::defaultOutColor)

void Font::setDefaultName(const std::vector<std::string> &names,
                          const SharedFontState &sfs)
{
	pickExistingFontName(names, FontPrivate::defaultName, sfs);
}

const std::vector<std::string> &Font::getInitialDefaultNames()
{
	return FontPrivate::initialDefaultNames;
}

void Font::initDynAttribs()
{
	p->color = new Color(p->colorTmp);

	if (rgssVer >= 3)
		p->outColor = new Color(p->outColorTmp);;
}

void Font::initDefaultDynAttribs()
{
	FontPrivate::defaultColor = new Color(FontPrivate::defaultColorTmp);

	if (rgssVer >= 3)
		FontPrivate::defaultOutColor = new Color(FontPrivate::defaultOutColorTmp);
}

void Font::initDefaults(const SharedFontState &sfs)
{
	std::vector<std::string> &names = FontPrivate::initialDefaultNames;

	switch (rgssVer)
	{
	case 1 :
		// FIXME: Japanese version has "MS PGothic" instead
		names.push_back("Arial");
		break;

	case 2 :
		names.push_back("UmePlus Gothic");
		names.push_back("MS Gothic");
		names.push_back("Courier New");
		break;

	default:
	case 3 :
		names.push_back("VL Gothic");
		FontPrivate::defaultSize = 24;
	}

	setDefaultName(names, sfs);

	FontPrivate::defaultOutline = (rgssVer >= 3 ? true : false);
	FontPrivate::defaultShadow  = (rgssVer == 2 ? true : false);
}

_TTF_Font *Font::getSdlFont(int outline_size)
{
	_TTF_Font **font;
	if (outline_size == 0)
		font = &p->sdlFont;
	else
		font = &p->sdlFontOutline;

	if (!*font)
		*font = shState->fontState().getFont(p->name.c_str(),
		                                     p->size, p->hiresMult, outline_size);

	if(outline_size && TTF_GetFontOutline(*font) != outline_size)
		TTF_SetFontOutline(*font, outline_size);

	int style = TTF_STYLE_NORMAL;

	if (p->bold)
		style |= TTF_STYLE_BOLD;

	if (p->italic)
		style |= TTF_STYLE_ITALIC;

	TTF_SetFontStyle(*font, style);

	return *font;
}
