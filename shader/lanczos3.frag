// From https://raw.githubusercontent.com/Sentmoraap/doing-sdl-right/93a52a0db0ff2da5066cce12f5b9a2ac62e6f401/assets/lanczos3.frag
// Copyright 2020 Lilian Gimenez (Sentmoraap).
// mkxp-z modifications Copyright 2022-2023 Splendide Imaginarius.
// MIT license.

#ifdef GLSLES
	precision highp float;
#endif

uniform sampler2D texture;
uniform vec2 sourceSize;
uniform vec2 texSizeInv;
// Max diameter is 32, i.e. max radius is 16.
// You can change this by grepping for "32".
uniform int radius;
varying vec2 v_texCoord;

float lanczos(float x)
{
	x = max(abs(x), 0.00001);
	float val = x * 3.141592654;
	return sin(val) * sin(val / float(radius)) / (val * val);
}

void main()
{
	int diameter = 2 * radius;
	int radiusMinus = radius - 1;
	int radiusMinusNeg = -1 * radiusMinus;

	vec2 pixel = v_texCoord * sourceSize + 0.5;
	vec2 frac = fract(pixel);
	vec2 onePixel = texSizeInv;
	pixel = floor(pixel) * texSizeInv - onePixel / float(radiusMinus);

	float lanczosX[32];
	float sum = 0.0;
	for(int x = 0; x < diameter; x++)
	{
		lanczosX[x] = lanczos(float(x) - float(radiusMinus) - frac.x);
		sum += lanczosX[x];
	}
	for(int x = 0; x < diameter; x++) lanczosX[x] /= sum;
	sum = 0.0;
	float lanczosY[32];
	for(int y = 0; y < diameter; y++)
	{
		lanczosY[y] = lanczos(float(y) - float(radiusMinus) - frac.y);
		sum += lanczosY[y];
	}
	for(int y = 0; y < diameter; y++) lanczosY[y] /= sum;
	gl_FragColor = vec4(0);
	for(int y = radiusMinusNeg; y <= radius; y++)
	{
		vec4 colour = vec4(0);
		for(int x = radiusMinusNeg; x <= radius; x++)
			colour += texture2D(texture, pixel + vec2(float(x) * onePixel.x, float(y) * onePixel.y)).rgba * lanczosX[x + radiusMinus];
		gl_FragColor += colour * lanczosY[y + radiusMinus];
	}
}
