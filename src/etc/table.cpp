/*
** table.cpp
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

#include "table.h"

#include <string.h>
#include <algorithm>

#include "serial-util.h"
#include "exception.h"
#include "forced-assert.h"
#include "util.h"

#ifdef MKXPZ_RETRO
#  include "sandbox-serial-util.h"

static uint64_t next_id = 1;
#endif // MKXPZ_RETRO

/* Init normally */
Table::Table(int x, int y /*= 1*/, int z /*= 1*/)
    :
#ifdef MKXPZ_RETRO
      id(next_id++),
#endif // MKXPZ_RETRO
      xs(x), ys(y), zs(z),
      data(x*y*z)
{}

Table::Table(const Table &other)
    :
#ifdef MKXPZ_RETRO
      id(next_id++),
#endif // MKXPZ_RETRO
      xs(other.xs), ys(other.ys), zs(other.zs),
      data(other.data)
{}

int16_t Table::get(int x, int y, int z) const
{
	return data[xs*ys*z + xs*y + x];
}

void Table::set(int16_t value, int x, int y, int z)
{
	if (x < 0 || x >= xs
	||  y < 0 || y >= ys
	||  z < 0 || z >= zs)
	{
		return;
	}

	data[xs*ys*z + xs*y + x] = value;

	modified();
}

void Table::resize(int x, int y, int z)
{
	if (x == xs && y == ys && z == zs)
		return;

	std::vector<int16_t> newData(x*y*z);

	for (int k = 0; k < std::min(z, zs); ++k)
		for (int j = 0; j < std::min(y, ys); ++j)
			for (int i = 0; i < std::min(x, xs); ++i)
				newData[x*y*k + x*j + i] = at(i, j, k);

	data.swap(newData);

	xs = x;
	ys = y;
	zs = z;

	return;
}

void Table::resize(int x, int y)
{
	resize(x, y, zs);
}

void Table::resize(int x)
{
	resize(x, ys, zs);
}

/* Serializable */
int Table::serialSize() const
{
	/* header + data */
	return 20 + (xs * ys * zs) * 2;
}

void Table::serialize(char *buffer) const
{
	/* Table dimensions: we don't care
	 * about them but RMXP needs them */
	int dim = 1;
	int size = xs * ys * zs;

	if (ys > 1)
		dim = 2;

	if (zs > 1)
		dim = 3;

	writeInt32(&buffer, dim);
	writeInt32(&buffer, xs);
	writeInt32(&buffer, ys);
	writeInt32(&buffer, zs);
	writeInt32(&buffer, size);

	if (size > 0)
		memcpy(buffer, dataPtr(data), sizeof(int16_t)*size);
}


Table *Table::deserialize(Exception &exception, const char *data, int len)
{
	if (len < 20)
	{
		exception = Exception(Exception::RGSSError, "Marshal: Table: bad file format");
		return nullptr;
	}

	readInt32(&data);
	int x = readInt32(&data);
	int y = readInt32(&data);
	int z = readInt32(&data);
	int size = readInt32(&data);

	if (size != x*y*z)
	{
		exception = Exception(Exception::RGSSError, "Marshal: Table: bad file format");
		return nullptr;
	}

	if (len != 20 + x*y*z*2)
	{
		exception = Exception(Exception::RGSSError, "Marshal: Table: bad file format");
		return nullptr;
	}

	Table *t = new Table(x, y, z);

	if (size > 0)
		memcpy(dataPtr(t->data), data, sizeof(int16_t)*size);

	return t;
}

#ifdef MKXPZ_RETRO
#ifndef MKXPZ_SANDBOX_SERIAL_TABLE_H
#define MKXPZ_SANDBOX_SERIAL_TABLE_H
#include "sandbox-serial-table.h"
#endif // MKXPZ_SANDBOX_SERIAL_TABLE_H
#endif // MKXPZ_RETRO
