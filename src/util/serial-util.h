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

static inline int32_t
readInt32(const char **dataP)
{
	int32_t result;

	memcpy(&result, *dataP, 4);
	*dataP += 4;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#  ifdef _MSC_VER
	static_assert(sizeof(unsigned long) == sizeof(int32_t), "unsigned long should be 32 bits");
	result = (int32_t)_byteswap_ulong((unsigned long)result);
#  else
	result = (int32_t)__builtin_bswap32((uint32_t)result);
#  endif
#endif

	return result;
}

static inline double
readDouble(const char **dataP)
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	uint64_t result;

	memcpy(&result, *dataP, 8);
	*dataP += 8;

#  ifdef _MSC_VER
	result = (uint64_t)_byteswap_uint64((unsigned __int64)result);
#  else
	result = __builtin_bswap64(result);
#  endif

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
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#  ifdef _MSC_VER
	static_assert(sizeof(unsigned long) == sizeof(int32_t), "unsigned long should be 32 bits");
	value = (int32_t)_byteswap_ulong((unsigned long)value);
#  else
	value = (int32_t)__builtin_bswap32((uint32_t)value);
#  endif
#endif

	memcpy(*dataP, &value, 4);
	*dataP += 4;
}

static inline void
writeDouble(char **dataP, double value)
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	uint64_t valueUint = *(uint64_t *)&value;

#  ifdef _MSC_VER
	valueUint = (uint64_t)_byteswap_uint64((unsigned __int64)valueUint);
#  else
	valueUint = __builtin_bswap64(valueUint);
#  endif

	memcpy(*dataP, &valueUint, 8);
#else
	memcpy(*dataP, &value, 8);
#endif
	*dataP += 8;
}

#endif // SERIALUTIL_H
