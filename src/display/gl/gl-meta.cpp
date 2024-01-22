/*
** gl-meta.cpp
**
** This file is part of mkxp.
**
** Copyright (C) 2014 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
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

#include "gl-meta.h"
#include "gl-fun.h"
#include "sharedstate.h"
#include "glstate.h"
#include "quad.h"
#include "config.h"
#include "etc.h"

namespace FBO
{
	ID boundFramebufferID;
}

namespace GLMeta
{

void subRectImageUpload(GLint srcW, GLint srcX, GLint srcY,
                        GLint dstX, GLint dstY, GLsizei dstW, GLsizei dstH,
                        SDL_Surface *src, GLenum format)
{
	if (gl.unpack_subimage)
	{
		gl.PixelStorei(GL_UNPACK_ROW_LENGTH, srcW);
		gl.PixelStorei(GL_UNPACK_SKIP_PIXELS, srcX);
		gl.PixelStorei(GL_UNPACK_SKIP_ROWS, srcY);

		TEX::uploadSubImage(dstX, dstY, dstW, dstH, src->pixels, format);
	}
	else
	{
		SDL_PixelFormat *form = src->format;
		SDL_Surface *tmp = SDL_CreateRGBSurface(0, dstW, dstH, form->BitsPerPixel,
		                                        form->Rmask, form->Gmask, form->Bmask, form->Amask);
		SDL_Rect srcRect = { srcX, srcY, dstW, dstH };

		SDL_BlitSurface(src, &srcRect, tmp, 0);

		TEX::uploadSubImage(dstX, dstY, dstW, dstH, tmp->pixels, format);

		SDL_FreeSurface(tmp);
	}
}

void subRectImageEnd()
{
	if (gl.unpack_subimage)
	{
		gl.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		gl.PixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
		gl.PixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	}
}

#define HAVE_NATIVE_VAO gl.GenVertexArrays

static void vaoBindRes(VAO &vao)
{
	VBO::bind(vao.vbo);
	IBO::bind(vao.ibo);

	for (size_t i = 0; i < vao.attrCount; ++i)
	{
		const VertexAttribute &va = vao.attr[i];

		gl.EnableVertexAttribArray(va.index);
		gl.VertexAttribPointer(va.index, va.size, va.type, GL_FALSE, vao.vertSize, va.offset);
	}
}

void vaoInit(VAO &vao, bool keepBound)
{
	if (HAVE_NATIVE_VAO)
	{
		gl.GenVertexArrays(1, &vao.nativeVAO);
		gl.BindVertexArray(vao.nativeVAO);
		vaoBindRes(vao);
		if (!keepBound)
			gl.BindVertexArray(0);
	}
	else
	{
		if (keepBound)
		{
			VBO::bind(vao.vbo);
			IBO::bind(vao.ibo);
		}
	}
}

void vaoFini(VAO &vao)
{
	if (HAVE_NATIVE_VAO)
		gl.DeleteVertexArrays(1, &vao.nativeVAO);
}

void vaoBind(VAO &vao)
{
	if (HAVE_NATIVE_VAO)
		gl.BindVertexArray(vao.nativeVAO);
	else
		vaoBindRes(vao);
}

void vaoUnbind(VAO &vao)
{
	if (HAVE_NATIVE_VAO)
	{
		gl.BindVertexArray(0);
	}
	else
	{
		for (size_t i = 0; i < vao.attrCount; ++i)
			gl.DisableVertexAttribArray(vao.attr[i].index);

		VBO::unbind();
		IBO::unbind();
	}
}

#define HAVE_NATIVE_BLIT gl.BlitFramebuffer

static void _blitBegin(FBO::ID fbo, const Vec2i &size)
{
	if (HAVE_NATIVE_BLIT)
	{
		FBO::boundFramebufferID = fbo;
		gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo.gl);
	}
	else
	{
		FBO::bind(fbo);
		glState.viewport.pushSet(IntRect(0, 0, size.x, size.y));

		switch (shState->config().smoothScaling)
		{
		case Bicubic:
		{
			BicubicShader &shader = shState->shaders().bicubic;
			shader.bind();
			shader.applyViewportProj();
			shader.setTranslation(Vec2i());
			shader.setTexSize(Vec2i(size.x, size.y));
			shader.setSharpness(shState->config().bicubicSharpness);
		}

			break;
		case Lanczos3:
		{
			Lanczos3Shader &shader = shState->shaders().lanczos3;
			shader.bind();
			shader.applyViewportProj();
			shader.setTranslation(Vec2i());
			shader.setTexSize(Vec2i(size.x, size.y));
		}

			break;
#ifdef MKXPZ_SSL
		case xBRZ:
		{
			XbrzShader &shader = shState->shaders().xbrz;
			shader.bind();
			shader.applyViewportProj();
			shader.setTranslation(Vec2i());
			shader.setTexSize(Vec2i(size.x, size.y));
			shader.setTargetScale(Vec2(1., 1.));
		}

			break;
#endif
		default:
		{
			SimpleShader &shader = shState->shaders().simple;
			shader.bind();
			shader.applyViewportProj();
			shader.setTranslation(Vec2i());
			shader.setTexSize(Vec2i(size.x, size.y));
		}
		}
	}
}

int blitDstWidthLores = 1;
int blitDstWidthHires = 1;
int blitDstHeightLores = 1;
int blitDstHeightHires = 1;

int blitSrcWidthLores = 1;
int blitSrcWidthHires = 1;
int blitSrcHeightLores = 1;
int blitSrcHeightHires = 1;

void blitBegin(TEXFBO &target, bool preferHires)
{
	blitDstWidthLores = target.width;
	blitDstHeightLores = target.height;

	if (preferHires && target.selfHires != nullptr) {
		blitDstWidthHires = target.selfHires->width;
		blitDstHeightHires = target.selfHires->height;
		_blitBegin(target.selfHires->fbo, Vec2i(target.selfHires->width, target.selfHires->height));
	}
	else {
		blitDstWidthHires = blitDstWidthLores;
		blitDstHeightHires = blitDstHeightLores;
		_blitBegin(target.fbo, Vec2i(target.width, target.height));
	}
}

void blitBeginScreen(const Vec2i &size)
{
	blitDstWidthLores = 1;
	blitDstWidthHires = 1;
	blitDstHeightLores = 1;
	blitDstHeightHires = 1;

	_blitBegin(FBO::ID(0), size);
}

void blitSource(TEXFBO &source)
{
	blitSrcWidthLores = source.width;
	blitSrcHeightLores = source.height;
	if (source.selfHires != nullptr) {
		blitSrcWidthHires = source.selfHires->width;
		blitSrcHeightHires = source.selfHires->height;
	}
	else {
		blitSrcWidthHires = blitSrcWidthLores;
		blitSrcHeightHires = blitSrcHeightLores;
	}

	if (HAVE_NATIVE_BLIT)
	{
		gl.BindFramebuffer(GL_READ_FRAMEBUFFER, source.fbo.gl);
	}
	else
	{
		switch (shState->config().smoothScaling)
		{
		case Bicubic:
		{
			BicubicShader &shader = shState->shaders().bicubic;
			shader.bind();
			shader.setTexSize(Vec2i(blitSrcWidthHires, blitSrcHeightHires));
		}

			break;
		case Lanczos3:
		{
			Lanczos3Shader &shader = shState->shaders().lanczos3;
			shader.bind();
			shader.setTexSize(Vec2i(blitSrcWidthHires, blitSrcHeightHires));
		}

			break;
#ifdef MKXPZ_SSL
		case xBRZ:
		{
			XbrzShader &shader = shState->shaders().xbrz;
			shader.bind();
			shader.setTexSize(Vec2i(blitSrcWidthHires, blitSrcHeightHires));
		}

			break;
#endif
		default:
		{
			SimpleShader &shader = shState->shaders().simple;
			shader.bind();
			shader.setTexSize(Vec2i(blitSrcWidthHires, blitSrcHeightHires));
		}
		}
		if (source.selfHires != nullptr) {
			TEX::bind(source.selfHires->tex);
		}
		else {
			TEX::bind(source.tex);
		}
	}
}

void blitRectangle(const IntRect &src, const Vec2i &dstPos)
{
	blitRectangle(src, IntRect(dstPos.x, dstPos.y, src.w, src.h), false);
}

void blitRectangle(const IntRect &src, const IntRect &dst, bool smooth)
{
	// Handle high-res dest
	int scaledDstX = dst.x * blitDstWidthHires / blitDstWidthLores;
	int scaledDstY = dst.y * blitDstHeightHires / blitDstHeightLores;
	int scaledDstWidth = dst.w * blitDstWidthHires / blitDstWidthLores;
	int scaledDstHeight = dst.h * blitDstHeightHires / blitDstHeightLores;
	IntRect dstScaled(scaledDstX, scaledDstY, scaledDstWidth, scaledDstHeight);

	// Handle high-res source
	int scaledSrcX = src.x * blitSrcWidthHires / blitSrcWidthLores;
	int scaledSrcY = src.y * blitSrcHeightHires / blitSrcHeightLores;
	int scaledSrcWidth = src.w * blitSrcWidthHires / blitSrcWidthLores;
	int scaledSrcHeight = src.h * blitSrcHeightHires / blitSrcHeightLores;
	IntRect srcScaled(scaledSrcX, scaledSrcY, scaledSrcWidth, scaledSrcHeight);

	if (HAVE_NATIVE_BLIT)
	{
		gl.BlitFramebuffer(srcScaled.x, srcScaled.y, srcScaled.x+srcScaled.w, srcScaled.y+srcScaled.h,
		                   dstScaled.x, dstScaled.y, dstScaled.x+dstScaled.w, dstScaled.y+dstScaled.h,
		                   GL_COLOR_BUFFER_BIT, smooth ? GL_LINEAR : GL_NEAREST);
	}
	else
	{
#ifdef MKXPZ_SSL
		if (shState->config().smoothScaling == xBRZ)
		{
			XbrzShader &shader = shState->shaders().xbrz;
			shader.setTargetScale(Vec2((float)(shState->config().xbrzScalingFactor), (float)(shState->config().xbrzScalingFactor)));
		}
#endif
		if (smooth)
			TEX::setSmooth(true);

		glState.blend.pushSet(false);
		Quad &quad = shState->gpQuad();
		quad.setTexPosRect(srcScaled, dstScaled);
		quad.draw();
		glState.blend.pop();

		if (smooth)
			TEX::setSmooth(false);
	}
}

void blitEnd()
{
	blitDstWidthLores = 1;
	blitDstWidthHires = 1;
	blitDstHeightLores = 1;
	blitDstHeightHires = 1;

	blitSrcWidthLores = 1;
	blitSrcWidthHires = 1;
	blitSrcHeightLores = 1;
	blitSrcHeightHires = 1;

	if (!HAVE_NATIVE_BLIT) {
		glState.viewport.pop();
	}
}

}
