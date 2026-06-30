/*
** shader.h
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

#ifndef SHADER_H
#define SHADER_H

#include "etc-internal.h"
#include "gl-util.h"
#include "glstate.h"

class Shader
{
public:
	void bind();
	static void unbind();

	enum Attribute
	{
		Position = 0,
		TexCoord = 1,
		Color = 2
	};
    
    static std::string &commonHeader();

protected:
	Shader();
	Shader(const Shader &) = delete;
	Shader(Shader &&) = delete;
	Shader &operator=(const Shader &) = delete;
	Shader &operator=(Shader &&) = delete;
	~Shader();

	void init(Exception &exception,
	          const unsigned char *vert, int vertSize,
	          const unsigned char *frag, int fragSize,
	          const char *vertName, const char *fragName,
	          const char *programName);
	void initFromFile(Exception &exception,
	                  const char *vertFile, const char *fragFile,
	                  const char *programName);

	static void setVec4Uniform(GLint location, const Vec4 &vec);
    static void setVec2Uniform(GLint location, const Vec2 &vec);
	static void setTexUniform(GLint location, unsigned unitIndex, TEX::ID texture);

	GLuint vertShader, fragShader;
	GLuint program;
	bool initialized;

private:
#ifdef MKXPZ_BUILD_XCODE
    static std::string shaderCommon;
#endif
};

class ShaderBase : public Shader
{
public:

	struct GLProjMat : public GLProperty<Vec2i>
	{
	private:
		void apply(const Vec2i &value);
		GLint u_mat;

		friend class ShaderBase;
	};

	/* Stack is not used (only 'set()') */
	GLProjMat projMat;

	/* Retrieves the current glState.viewport size,
	 * calculates the corresponding ortho projection matrix
	 * and loads it into the shaders uniform */
	void applyViewportProj();

	void setTexSize(const Vec2i &value);
	void setTranslation(const Vec2i &value);

protected:
	void init();
	virtual bool framebufferScalingAllowed();

	GLint u_texSizeInv, u_translation;
};

class FlatColorShader : public ShaderBase
{
public:
	FlatColorShader(Exception &exception);

	void setColor(const Vec4 &value);

private:
	GLint u_color;
};

class SimpleShader : public ShaderBase
{
public:
	SimpleShader() = default;
	SimpleShader(Exception &exception);

	void setTexOffsetX(int value);

protected:
	GLint u_texOffsetX;
};

class SimpleColorShader : public ShaderBase
{
public:
	SimpleColorShader() = default;
	SimpleColorShader(Exception &exception);
};

class SimpleAlphaShader : public ShaderBase
{
public:
	SimpleAlphaShader() = default;
	SimpleAlphaShader(Exception &exception);
};

class SimpleSpriteShader : public ShaderBase
{
public:
	SimpleSpriteShader() = default;
	SimpleSpriteShader(Exception &exception);

	void setSpriteMat(const float value[16]);

protected:
	GLint u_spriteMat;
};

class AlphaSpriteShader : public ShaderBase
{
public:
	AlphaSpriteShader() = default;
	AlphaSpriteShader(Exception &exception);

	void setSpriteMat(const float value[16]);
	void setAlpha(float value);

private:
	GLint u_spriteMat, u_alpha;
};

class TransShader : public ShaderBase
{
public:
	TransShader() = default;
	TransShader(Exception &exception);

	void setCurrentScene(TEX::ID tex);
	void setFrozenScene(TEX::ID tex);
	void setTransMap(TEX::ID tex);
	void setProg(float value);
	void setVague(float value);

private:
	GLint u_currentScene, u_frozenScene, u_transMap, u_prog, u_vague;
};

class SimpleTransShader : public ShaderBase
{
public:
	SimpleTransShader() = default;
	SimpleTransShader(Exception &exception);

	void setCurrentScene(TEX::ID tex);
	void setFrozenScene(TEX::ID tex);
	void setProg(float value);

private:
	GLint u_currentScene, u_frozenScene, u_prog;
};

class SpriteShader : public ShaderBase
{
public:
	SpriteShader() = default;
	SpriteShader(Exception &exception);

	void setSpriteMat(const float value[16]);
	void setTone(const Vec4 &value);
	void setColor(const Vec4 &value);
	void setOpacity(float value);
	void setBushDepth(bool bushY, bool bushUnder, float bushSlope, float bushIntercept);
	void setBushOpacity(float value);
    void setPattern(const TEX::ID pattern, const Vec2 &dimensions);
    void setPatternBlendType(int blendType);
    void setPatternTile(bool value);
    void setShouldRenderPattern(bool value);
    void setPatternOpacity(float value);
    void setPatternScroll(const Vec2 &scroll);
    void setPatternZoom(const Vec2 &zoom);
    void setInvert(bool value);

private:
	GLint u_spriteMat, u_tone, u_opacity, u_color,
    u_bushY, u_bushUnder, u_bushSlope, u_bushIntercept, u_bushOpacity, u_pattern, u_renderPattern,
    u_patternBlendType, u_patternSizeInv, u_patternTile, u_patternOpacity, u_patternScroll, u_patternZoom, u_invert;
};

class PlaneShader : public ShaderBase
{
public:
	PlaneShader() = default;
	PlaneShader(Exception &exception);

	void setTone(const Vec4 &value);
	void setColor(const Vec4 &value);
	void setFlash(const Vec4 &value);
	void setOpacity(float value);

private:
	GLint u_tone, u_color, u_flash, u_opacity;
};

class GrayShader : public ShaderBase
{
public:
	GrayShader() = default;
	GrayShader(Exception &exception);

	void setGray(float value);

protected:
	virtual bool framebufferScalingAllowed();

private:
	GLint u_gray;
};

class TilemapShader : public ShaderBase
{
public:
	TilemapShader() = default;
	TilemapShader(Exception &exception);

	void setAniIndex(int value);

	void setTone(const Vec4 &value);
	void setColor(const Vec4 &value);
	void setOpacity(float value);

	void setATFrames(int values[7]);

private:
	GLint u_aniIndex, u_tone, u_color, u_opacity, u_atFrames;
};

class FlashMapShader : public ShaderBase
{
public:
	FlashMapShader() = default;
	FlashMapShader(Exception &exception);

	void setAlpha(float value);

private:
	GLint u_alpha;
};

class HueShader : public ShaderBase
{
public:
	HueShader() = default;
	HueShader(Exception &exception);

	void setHueAdjust(float value);

private:
	GLint u_hueAdjust;
};

class SimpleMatrixShader : public ShaderBase
{
public:
	SimpleMatrixShader() = default;
	SimpleMatrixShader(Exception &exception);

	void setMatrix(const float value[16]);

private:
	GLint u_matrix;
};

/* Gaussian blur */
struct BlurShader
{
	class HPass : public ShaderBase
	{
	public:
		HPass() = default;
		HPass(Exception &exception);
	};

	class VPass : public ShaderBase
	{
	public:
		VPass() = default;
		VPass(Exception &exception);
	};

	BlurShader() = default;
	BlurShader(Exception &exception);

	HPass pass1;
	VPass pass2;
};

class TilemapVXShader : public ShaderBase
{
public:
	TilemapVXShader() = default;
	TilemapVXShader(Exception &exception);

	void setAniOffset(const Vec2 &value);

private:
	GLint u_aniOffset;
};

/* Bitmap blit */
class BltShader : public ShaderBase
{
public:
	BltShader() = default;
	BltShader(Exception &exception);

	void init();

	void setSource();
	void setDestination(const TEX::ID value);
	void setDestCoorF(const Vec2 &value);
	void setSubRect(const FloatRect &value);
	void setOpacity(float value);

private:
	GLint u_source, u_destination, u_subRect, u_opacity;
};

class KglInvertShader : public ShaderBase
{
public:
	KglInvertShader() = default;
	KglInvertShader(Exception &exception);
};

class KglCompressAlphaShader : public ShaderBase
{
public:
	KglCompressAlphaShader() = default;
	KglCompressAlphaShader(Exception &exception);
};

class KglSubtractShader : public BltShader
{
public:
	KglSubtractShader() = default;
	KglSubtractShader(Exception &exception);
};

class KglShadowShaderH : public ShaderBase
{
public:
	KglShadowShaderH() = default;
	KglShadowShaderH(Exception &exception);

	void setParams(int x1, int x2, int y, bool soft, int w, int h, int x_center, int y_center, double slope1, double slope2);

private:
	GLint u_x1, u_x2, u_y, u_soft, u_w, u_h, u_x_center, u_y_center, u_slope1, u_slope2;
};

class KglShadowShaderV : public ShaderBase
{
public:
	KglShadowShaderV() = default;
	KglShadowShaderV(Exception &exception);

	void setParams(int y1, int y2, int x, bool wall, bool soft, int w, int h, int x_center, int y_center, double slope1, double slope2);

private:
	GLint u_y1, u_y2, u_x, u_wall, u_soft, u_w, u_h, u_x_center, u_y_center, u_slope1, u_slope2;
};

class Lanczos3Shader : public SimpleShader
{
public:
	Lanczos3Shader() = default;
	Lanczos3Shader(Exception &exception);

	void setTexSize(const Vec2i &value);

protected:
	GLint u_sourceSize;
};

class BicubicShader : public Lanczos3Shader
{
public:
	BicubicShader() = default;
	BicubicShader(Exception &exception);

	void setSharpness(int sharpness);

protected:
	GLint u_bc;
};

#ifdef MKXPZ_SSL
class XbrzShader : public Lanczos3Shader
{
public:
	XbrzShader() = default;
	XbrzShader(Exception &exception);

	void setTargetScale(const Vec2 &value);

protected:
	GLint u_targetScale;
};
#endif

class Lanczos3SpriteShader : public SimpleSpriteShader
{
public:
	Lanczos3SpriteShader() = default;
	Lanczos3SpriteShader(Exception &exception);

	void setTexSize(const Vec2i &value);

protected:
	GLint u_sourceSize;
};

class BicubicSpriteShader : public Lanczos3SpriteShader
{
public:
	BicubicSpriteShader() = default;
	BicubicSpriteShader(Exception &exception);

	void setSharpness(int sharpness);

protected:
	GLint u_bc;
};

class XbrzSpriteShader : public Lanczos3SpriteShader
{
public:
	XbrzSpriteShader() = default;
	XbrzSpriteShader(Exception &exception);

	void setTargetScale(const Vec2 &value);

protected:
	GLint u_targetScale;
};

/* Global object containing all available shaders */
struct ShaderSet
{
	FlatColorShader flatColor;
	SimpleShader simple;
	SimpleColorShader simpleColor;
	SimpleAlphaShader simpleAlpha;
	SimpleSpriteShader simpleSprite;
	AlphaSpriteShader alphaSprite;
	SpriteShader sprite;
	PlaneShader plane;
	GrayShader gray;
	TilemapShader tilemap;
	FlashMapShader flashMap;
	TransShader trans;
	SimpleTransShader simpleTrans;
	HueShader hue;
	BltShader blt;
	SimpleMatrixShader simpleMatrix;
	BlurShader blur;
	TilemapVXShader tilemapVX;
	KglInvertShader kglInvert;
	KglCompressAlphaShader kglCompressAlpha;
	KglSubtractShader kglSubtract;
	KglShadowShaderH kglShadowH;
	KglShadowShaderV kglShadowV;
	BicubicShader bicubic;
	Lanczos3Shader lanczos3;
#ifdef MKXPZ_SSL
	XbrzShader xbrz;
#endif
	Lanczos3SpriteShader lanczos3Sprite;
	BicubicSpriteShader bicubicSprite;
#ifdef MKXPZ_SSL
	XbrzSpriteShader xbrzSprite;
#endif

	ShaderSet(Exception &exception);

	void reinit(Exception &exception);
};

#endif // SHADER_H
