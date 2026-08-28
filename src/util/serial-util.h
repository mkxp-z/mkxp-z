/*
** serial-util.h
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

#ifndef SERIALUTIL_H
#define SERIALUTIL_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <SDL_endian.h>

static inline uint16_t
byteSwap16(uint16_t value)
{
#ifdef _MSC_VER
	static_assert(sizeof(unsigned short) == sizeof(uint16_t), "unsigned short should be 16 bits");
	return _byteswap_ushort(value);
#else
	return __builtin_bswap16(value);
#endif
}

static inline uint16_t
byteSwap16IfBigEndian(uint16_t value)
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	return byteSwap16(value);
#else
	return value;
#endif
}

static inline uint32_t
byteSwap32(uint32_t value)
{
#ifdef _MSC_VER
	static_assert(sizeof(unsigned long) == sizeof(uint32_t), "unsigned long should be 32 bits");
	return _byteswap_ulong(value);
#else
	return __builtin_bswap32(value);
#endif
}

static inline uint32_t
byteSwap32IfBigEndian(uint32_t value)
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	return byteSwap32(value);
#else
	return value;
#endif
}

static inline uint64_t
byteSwap64(uint64_t value)
{
#ifdef _MSC_VER
	return _byteswap_uint64(value);
#else
	return __builtin_bswap64(value);
#endif
}

static inline uint64_t
byteSwap64IfBigEndian(uint64_t value)
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	return byteSwap64(value);
#else
	return value;
#endif
}

static inline int32_t
readInt32(const char **dataP)
{
	int32_t result;

	memcpy(&result, *dataP, 4);
	*dataP += 4;

	result = byteSwap32IfBigEndian(result);

	return result;
}

static inline double
readDouble(const char **dataP)
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	uint64_t result;

	memcpy(&result, *dataP, 8);
	*dataP += 8;

	result = byteSwap64(result);

	return *(double *)&result;
#else
	double result;

	memcpy(&result, *dataP, 8);
	*dataP += 8;

	return result;
#endif
}

static inline void
writeInt32(char **dataP, int32_t value)
{
	value = byteSwap32IfBigEndian(value);

	memcpy(*dataP, &value, 4);
	*dataP += 4;
}

static inline void
writeDouble(char **dataP, double value)
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	uint64_t valueUint = *(uint64_t *)&value;

	valueUint = byteSwap64(valueUint);

	memcpy(*dataP, &valueUint, 8);
#else
	memcpy(*dataP, &value, 8);
#endif
	*dataP += 8;
}

#endif // SERIALUTIL_H
