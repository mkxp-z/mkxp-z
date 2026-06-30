/*
** sandbox-serial-etc.h
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

#ifndef MKXPZ_SANDBOX_SERIAL_ETC_H
#define MKXPZ_SANDBOX_SERIAL_ETC_H
#include "etc.cpp"
#endif // MKXPZ_SANDBOX_SERIAL_ETC_H

bool Color::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
	if (!mkxp_sandbox::sandbox_serialize(red, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(green, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(blue, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(alpha, data, max_size)) return false;

	return true;
}

bool Color::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size) {
	if (!mkxp_sandbox::sandbox_deserialize(red, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(green, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(blue, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(alpha, data, max_size)) return false;

	updateInternal();

	return true;
}

bool Tone::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
	if (!mkxp_sandbox::sandbox_serialize(red, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(green, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(blue, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(gray, data, max_size)) return false;

	return true;
}

bool Tone::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size) {
	if (!mkxp_sandbox::sandbox_deserialize(red, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(green, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(blue, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(gray, data, max_size)) return false;

	updateInternal();

	return true;
}

bool Rect::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
	if (!mkxp_sandbox::sandbox_serialize((int32_t)x, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize((int32_t)y, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize((int32_t)width, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize((int32_t)height, data, max_size)) return false;

	return true;
}

bool Rect::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size) {
	if (!mkxp_sandbox::sandbox_deserialize((int32_t &)x, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize((int32_t &)y, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize((int32_t &)width, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize((int32_t &)height, data, max_size)) return false;

	return true;
}
