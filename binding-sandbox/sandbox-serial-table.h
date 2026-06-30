/*
** sandbox-serial-table.h
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

#ifndef MKXPZ_SANDBOX_SERIAL_TABLE_H
#define MKXPZ_SANDBOX_SERIAL_TABLE_H
#include "table.cpp"
#endif // MKXPZ_SANDBOX_SERIAL_TABLE_H

bool Table::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
	if (!mkxp_sandbox::sandbox_serialize((int32_t)xs, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize((int32_t)ys, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize((int32_t)zs, data, max_size)) return false;

	MKXPZ_FORCED_ASSERT((uint32_t)xs * (uint32_t)ys * (uint32_t)zs == this->data.size());
	if (max_size < this->data.size() * sizeof(int16_t)) return false;

	if (this->data.size() > 0) {
		memcpy(data, this->data.data(), this->data.size() * sizeof(int16_t));
	}

	data = (uint8_t *)data + this->data.size() * sizeof(int16_t);
	max_size -= this->data.size() * sizeof(int16_t);
	return true;
}

bool Table::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	if (!mkxp_sandbox::sandbox_deserialize((int32_t &)xs, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize((int32_t &)ys, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize((int32_t &)zs, data, max_size)) return false;

	if (this->data.size() != (uint32_t)xs * (uint32_t)ys * (uint32_t)zs) {
		this->data.clear();
		this->data.resize((uint32_t)xs * (uint32_t)ys * (uint32_t)zs);
		deserModified = true;
	}
	if (max_size < this->data.size() * sizeof(int16_t)) return false;

	if (!deserModified) {
		if (mkxp_sandbox::deser_swap_bytes) {
			const int16_t *buf = this->data.data();
			for (size_t i = 0; i < this->data.size(); ++i) {
				if (buf[i] != (int16_t)__builtin_bswap16(((const uint16_t *)data)[i])) {
					deserModified = true;
					break;
				}
			}
		} else if (this->data.size() > 0 && memcmp(this->data.data(), data, this->data.size() * sizeof(int16_t))) {
			deserModified = true;
		}
	}

	if (deserModified) {
		if (mkxp_sandbox::deser_swap_bytes) {
			int16_t *buf = this->data.data();
			for (size_t i = 0; i < this->data.size(); ++i) {
				buf[i] = (int16_t)__builtin_bswap16(((const uint16_t *)data)[i]);
			}
		} else if (this->data.size() > 0) {
			memcpy(this->data.data(), data, this->data.size() * sizeof(int16_t));
		}
	}

	data = (uint8_t *)data + this->data.size() * sizeof(int16_t);
	max_size -= this->data.size() * sizeof(int16_t);
	return true;
}

void Table::sandbox_deserialize_begin(bool is_new)
{
	deserModified = is_new;
}
