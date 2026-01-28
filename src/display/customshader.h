/*
** customshader.h
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

#ifndef CUSTOMSHADER_H
#define CUSTOMSHADER_H

#include "util/disposable.h"
#include "display/gl/shader.h"
#include <string>

struct CustomShaderPrivate;

class CustomShader : public Disposable
{
public:
	CustomShader(const char *filename);
	~CustomShader();

	const std::string &getFilename() const;
	ShaderBase *getShader() const;

private:
	void releaseResources();
	const char *klassName() const { return "shader"; }

	CustomShaderPrivate *p;
};

#endif // CUSTOMSHADER_H
