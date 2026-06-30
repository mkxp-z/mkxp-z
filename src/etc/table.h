/*
** table.h
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

#ifndef TABLE_H
#define TABLE_H

#include "serializable.h"

#include <stdint.h>
#include "sigslot/signal.hpp"
#include <vector>

#ifdef MKXPZ_RETRO
#  include "wasm-types.h"
#endif // MKXPZ_RETRO

class Table : public Serializable
{
public:
	Table(int x = 1, int y = 1, int z = 1);
	/* Clone constructor */
	Table(const Table &other);
	virtual ~Table() {}

	int xSize() const { return xs; }
	int ySize() const { return ys; }
	int zSize() const { return zs; }

	int16_t get(int x, int y = 0, int z = 0) const;
	void set(int16_t value, int x, int y = 0, int z = 0);

	void resize(int x, int y, int z);
	void resize(int x, int y);
	void resize(int x);

	int serialSize() const;
	void serialize(char *buffer) const;
	static Table *deserialize(Exception &exception, const char *data, int len);

	/* <internal */
	inline int16_t &at(int x, int y = 0, int z = 0)
	{
		return data[xs*ys*z + xs*y + x];
	}

	inline const int16_t &at(int x, int y = 0, int z = 0) const
	{
		return data[xs*ys*z + xs*y + x];
	}

    sigslot::signal<> modified;

#ifdef MKXPZ_RETRO
	const uint64_t id; // Globally unique nonzero ID for this table, for change detection during save state deserialization
	bool deserModified;

	bool sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const;
	bool sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size);
	void sandbox_deserialize_begin(bool is_new);
#endif // MKXPZ_RETRO

private:
	int xs, ys, zs;
	std::vector<int16_t> data;
};

#endif // TABLE_H
