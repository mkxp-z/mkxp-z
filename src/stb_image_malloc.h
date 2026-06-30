/*
** stb_image_malloc.h
**
** This file is part of mkxp.
**
** Copyright (C) 2025 - 2026 The mkxp-z authors
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

#ifndef MKXPZ_STB_IMAGE_MALLOC_H
#define MKXPZ_STB_IMAGE_MALLOC_H

#include <stdlib.h>

#define STBI_MALLOC malloc
#define STBI_REALLOC realloc
#define STBI_FREE free

#endif /* MKXPZ_STB_IMAGE_MALLOC_H */
