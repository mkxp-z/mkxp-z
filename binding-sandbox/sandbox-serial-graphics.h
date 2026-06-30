/*
** sandbox-serial-graphics.h
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

#ifndef MKXPZ_SANDBOX_SERIAL_GRAPHICS_H
#define MKXPZ_SANDBOX_SERIAL_GRAPHICS_H
#include "graphics.cpp"
#endif // MKXPZ_SANDBOX_SERIAL_GRAPHICS_H

bool Movie::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
    if (!mkxp_sandbox::sandbox_serialize(baseTicks != (uint64_t)-1, data, max_size)) return false;

    if (baseTicks != (uint64_t)-1) {
        if (!mkxp_sandbox::sandbox_serialize(baseTicks, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_serialize(currentTicks, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_serialize(srcOps->path(), data, max_size)) return false;
    }

    return true;
}

bool Graphics::sandbox_serialize_movie(const Movie *movie, void *&data, mkxp_sandbox::wasm_size_t &max_size) {
    if (movie == nullptr) {
        return false;
    }
    return movie->sandbox_serialize(data, max_size);
}
