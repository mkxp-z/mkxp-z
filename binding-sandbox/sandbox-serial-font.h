/*
** sandbox-serial-font.h
**
** This file is part of mkxp.
**
** Copyright (C) 2026 The mkxp-z authors
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

#ifndef MKXPZ_SANDBOX_SERIAL_FONT_H
#define MKXPZ_SANDBOX_SERIAL_FONT_H
#include "font.cpp"
#endif // MKXPZ_SANDBOX_SERIAL_FONT_H

bool Font::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
	if (!mkxp_sandbox::sandbox_serialize(p->bold, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->italic, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->outline, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->shadow, data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_serialize(p->color == &p->colorTmp ? nullptr : p->color, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->outColor == &p->outColorTmp ? nullptr : p->outColor, data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_serialize((int32_t)p->size, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->name, data, max_size)) return false;

	return true;
}

bool Font::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	if (!mkxp_sandbox::sandbox_deserialize(p->bold, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->italic, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->outline, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->shadow, data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_deserialize(p->color, data, max_size)) return false;
	if (p->color == nullptr) {
		p->color = &p->colorTmp;
	}
	if (!mkxp_sandbox::sandbox_deserialize(p->outColor, data, max_size)) return false;
	if (p->outColor == nullptr) {
		p->outColor = &p->outColorTmp;
	}

	// Invalidate the inner font object if either the name or size of this font is different from before
	if (p->sdlFont != nullptr) {
		int32_t size = p->size;
		{
			int32_t new_size;
			if (!mkxp_sandbox::sandbox_deserialize(new_size, data, max_size)) return false;
			p->size = new_size;
		}
		std::string name(p->name);
		if (!mkxp_sandbox::sandbox_deserialize(p->name, data, max_size)) return false;
		if (p->size != size || p->name != name) {
			p->sdlFont = nullptr;
		}
	} else {
		if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->size, data, max_size)) return false;
		if (!mkxp_sandbox::sandbox_deserialize(p->name, data, max_size)) return false;
	}

	return true;
}

bool Font::sandbox_serialize_default(void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	if (!mkxp_sandbox::sandbox_serialize(FontPrivate::defaultBold, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(FontPrivate::defaultItalic, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(FontPrivate::defaultOutline, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(FontPrivate::defaultShadow, data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_serialize(FontPrivate::defaultColor == &FontPrivate::defaultColorTmp ? nullptr : FontPrivate::defaultColor, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(FontPrivate::defaultOutColor == &FontPrivate::defaultOutColorTmp ? nullptr : FontPrivate::defaultOutColor, data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_serialize((int32_t)FontPrivate::defaultSize, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(FontPrivate::defaultName, data, max_size)) return false;

	return true;
}

bool Font::sandbox_deserialize_default(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	if (!mkxp_sandbox::sandbox_deserialize(FontPrivate::defaultBold, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(FontPrivate::defaultItalic, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(FontPrivate::defaultOutline, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(FontPrivate::defaultShadow, data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_deserialize(FontPrivate::defaultColor, data, max_size)) return false;
	if (FontPrivate::defaultColor == nullptr) {
		FontPrivate::defaultColor = &FontPrivate::defaultColorTmp;
	}
	if (!mkxp_sandbox::sandbox_deserialize(FontPrivate::defaultOutColor, data, max_size)) return false;
	if (FontPrivate::defaultOutColor == nullptr) {
		FontPrivate::defaultOutColor = &FontPrivate::defaultOutColorTmp;
	}

	{
		int32_t new_size;
		if (!mkxp_sandbox::sandbox_deserialize(new_size, data, max_size)) return false;
		FontPrivate::defaultSize = new_size;
	}
	if (!mkxp_sandbox::sandbox_deserialize(FontPrivate::defaultName, data, max_size)) return false;

	return true;
}
