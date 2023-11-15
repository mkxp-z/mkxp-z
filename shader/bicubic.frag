// From https://raw.githubusercontent.com/Sentmoraap/doing-sdl-right/f1a0183692abbd5d899fb432ab8dafe228a4929a/assets/bicubic.frag
// Copyright 2020 Lilian Gimenez (Sentmoraap).
// mkxp-z modifications Copyright 2022-2023 Splendide Imaginarius.
// MIT license.

#ifdef GLSLES
	precision highp float;
#endif

uniform sampler2D texture;
uniform vec2 sourceSize;
uniform vec2 texSizeInv;
varying vec2 v_texCoord;
uniform vec2 bc;

void main()
{
	vec2 pixel = v_texCoord * sourceSize + 0.5;
	vec2 frac = fract(pixel);
	vec2 frac2 = frac * frac;
	vec2 frac3 = frac * frac2;
	vec2 onePixel = texSizeInv;
	pixel = floor(pixel) * texSizeInv - onePixel / 2.0;
	vec4 colours[4];
	// 16 reads, unoptimized but forks for every Mitchell-Netravali filter
	for(int i = -1; i <= 2; i++)
	{
		vec4 p0 = texture2D(texture, pixel + vec2(   -onePixel.x, float(i) * onePixel.y)).rgba;
		vec4 p1 = texture2D(texture, pixel + vec2(             0, float(i) * onePixel.y)).rgba;
		vec4 p2 = texture2D(texture, pixel + vec2(    onePixel.x, float(i) * onePixel.y)).rgba;
		vec4 p3 = texture2D(texture, pixel + vec2(2.0 * onePixel.x, float(i) * onePixel.y)).rgba;
		colours[i + 1] = ((-bc.x / 6.0 - bc . y) * p0 + (- 1.5 * bc.x - bc.y + 2.0) * p1
				+ (1.5 * bc.x + bc.y - 2.0) * p2 + (bc.x / 6.0 + bc.y) * p3) * frac3.x
				+ ((0.5 * bc.x + 2.0 * bc.y) * p0 + (2.0 * bc.x + bc.y - 3.0) * p1
				+ (-2.5 * bc.x - 2.0 * bc.y + 3.0) * p2 - bc.y * p3) * frac2.x
				+ ((-0.5 * bc.x - bc.y) * p0 + (0.5 * bc.x + bc.y) * p2) * frac.x
				+ p0 * bc.x / 6.0 + (-bc.x / 3.0 + 1.0) * p1 + p2 * bc.x / 6.0;
	}
	gl_FragColor = ((-bc.x / 6.0 - bc . y) * colours[0] + (- 1.5 * bc.x - bc.y + 2.0) * colours[1]
			+ (1.5 * bc.x + bc.y - 2.0) * colours[2] + (bc.x / 6.0 + bc.y) * colours[3]) * frac3.y
			+ ((0.5 * bc.x + 2.0 * bc.y) * colours[0] + (2.0 * bc.x + bc.y - 3.0) * colours[1]
			+ (-2.5 * bc.x - 2.0 * bc.y + 3.0) * colours[2] - bc.y * colours[3]) * frac2.y
			+ ((-0.5 * bc.x - bc.y) * colours[0] + (0.5 * bc.x + bc.y) * colours[2]) * frac.y
			+ colours[0] * bc.x / 6.0 + (-bc.x / 3.0 + 1.0) * colours[1] + colours[2] * bc.x / 6.0;
}
