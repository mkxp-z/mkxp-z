/*
** customshader.cpp
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

#include "customshader.h"
#include "sharedstate.h"
#include "filesystem/filesystem.h"
#include "util/exception.h"
#include "util/util.h"
#include "display/gl/shader.h"
#include <string>

struct CustomShaderPrivate
{
	std::string filename;
	ShaderBase *shader;

	CustomShaderPrivate(const char *filename)
	    : filename(filename),
	      shader(0)
	{
	}

	~CustomShaderPrivate()
	{
		delete shader;
	}
};

CustomShader::CustomShader(const char *filename)
{
	p = new CustomShaderPrivate(filename);

	if (!shState->fileSystem().exists(filename))
	{
		delete p;
		throw Exception(Exception::RGSSError,
		                "Shader file '%s' not found", filename);
	}

	// Read the fragment shader file
	std::string fragContents;
	if (!readFile(filename, fragContents))
	{
		delete p;
		throw Exception(Exception::RGSSError,
		                "Failed to read shader file '%s'", filename);
	}

	// Use a simple passthrough vertex shader (built-in)
	// The fragment shader will be the custom one from the file
	static const char *simpleVert = 
		"attribute vec2 position;\n"
		"attribute vec2 texCoord;\n"
		"varying vec2 v_texCoord;\n"
		"uniform mat4 projMat;\n"
		"void main() {\n"
		"    gl_Position = projMat * vec4(position, 0.0, 1.0);\n"
		"    v_texCoord = texCoord;\n"
		"}\n";

	p->shader = new ShaderBase();

	try
	{
		p->shader->init(
			(const unsigned char*)simpleVert, strlen(simpleVert),
			(const unsigned char*)fragContents.c_str(), fragContents.size(),
			"CustomShaderVert", filename, "CustomShader");
	}
	catch (const Exception &e)
	{
		delete p->shader;
		p->shader = 0;
		delete p;
		throw e;
	}
}

CustomShader::~CustomShader()
{
	dispose();
}

const std::string &CustomShader::getFilename() const
{
	guardDisposed();
	return p->filename;
}

ShaderBase *CustomShader::getShader() const
{
	guardDisposed();
	return p->shader;
}

void CustomShader::releaseResources()
{
	delete p;
}
