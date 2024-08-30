// From https://raw.githubusercontent.com/dolphin-emu/dolphin/39e266c5bf66a5626ead9cc8295e9638e305e7fc/Data/Sys/Shaders/default_pre_post_process.glsl
// mkxp-z modifications by Splendide Imaginarius.
// Public domain.

#ifdef GLSLES
	precision highp float;
#endif

uniform sampler2D texture;
uniform vec2 sourceSize;
uniform vec2 texSizeInv;
uniform vec2 targetSize;
uniform vec2 targetSizeInv;
varying vec2 v_texCoord;

/***** COLOR SAMPLING *****/

// Non filtered sample (nearest neighbor)
vec4 QuickSample(vec2 uv)
{
	return texture2D(texture, uv);
}
vec4 QuickSampleByPixel(vec2 xy)
{
	vec2 uv = vec2(xy * texSizeInv);
	return QuickSample(uv);
}

/***** Area Sampling *****/

// By Sam Belliveau and Filippo Tarpini. Public Domain license.
// Effectively a more accurate sharp bilinear filter when upscaling,
// that also works as a mathematically perfect downscale filter.
// https://entropymine.com/imageworsener/pixelmixing/
// https://github.com/obsproject/obs-studio/pull/1715
// https://legacy.imagemagick.org/Usage/filter/
vec4 AreaSampling(vec2 uv)
{
	// Compute the top-left and bottom-right corners of the target pixel box.
	vec2 t_beg = floor(uv * targetSize);
	vec2 t_end = t_beg + vec2(1.0, 1.0);

	// Convert the target pixel box to source pixel box.
	vec2 beg = t_beg * targetSizeInv * sourceSize;
	vec2 end = t_end * targetSizeInv * sourceSize;

	// Compute the top-left and bottom-right corners of the pixel box.
	vec2 f_beg = floor(beg);
	vec2 f_end = floor(end);

	// Compute how much of the start and end pixels are covered horizontally & vertically.
	float area_w = 1.0 - fract(beg.x);
	float area_n = 1.0 - fract(beg.y);
	float area_e = fract(end.x);
	float area_s = fract(end.y);

	// Compute the areas of the corner pixels in the pixel box.
	float area_nw = area_n * area_w;
	float area_ne = area_n * area_e;
	float area_sw = area_s * area_w;
	float area_se = area_s * area_e;

	// Initialize the color accumulator.
	vec4 avg_color = vec4(0.0, 0.0, 0.0, 0.0);

	// Prevents rounding errors due to the coordinates flooring above
	const vec2 offset = vec2(0.5, 0.5);

	// Accumulate corner pixels.
	avg_color += area_nw * QuickSampleByPixel(vec2(f_beg.x, f_beg.y) + offset);
	avg_color += area_ne * QuickSampleByPixel(vec2(f_end.x, f_beg.y) + offset);
	avg_color += area_sw * QuickSampleByPixel(vec2(f_beg.x, f_end.y) + offset);
	avg_color += area_se * QuickSampleByPixel(vec2(f_end.x, f_end.y) + offset);

	// Determine the size of the pixel box.
	int x_range = int(f_end.x - f_beg.x - 0.5);
	int y_range = int(f_end.y - f_beg.y - 0.5);

	// Workaround to compile the shader with DX11/12.
	// If this isn't done, it will complain that the loop could have too many iterations.
	// This number should be enough to guarantee downscaling from very high to very small resolutions.
	// Note that this number might be referenced in the UI.
	const int max_iterations = 16;

	// Fix up the average calculations in case we reached the upper limit
	x_range = min(x_range, max_iterations);
	y_range = min(y_range, max_iterations);

	// Accumulate top and bottom edge pixels.
	for (int ix = 0; ix < max_iterations; ++ix)
	{
		if (ix < x_range)
		{
			float x = f_beg.x + 1.0 + float(ix);
			avg_color += area_n * QuickSampleByPixel(vec2(x, f_beg.y) + offset);
			avg_color += area_s * QuickSampleByPixel(vec2(x, f_end.y) + offset);
		}
	}

	// Accumulate left and right edge pixels and all the pixels in between.
	for (int iy = 0; iy < max_iterations; ++iy)
	{
		if (iy < y_range)
		{
			float y = f_beg.y + 1.0 + float(iy);

			avg_color += area_w * QuickSampleByPixel(vec2(f_beg.x, y) + offset);
			avg_color += area_e * QuickSampleByPixel(vec2(f_end.x, y) + offset);

			for (int ix = 0; ix < max_iterations; ++ix)
			{
				if (ix < x_range)
				{
					float x = f_beg.x + 1.0 + float(ix);
					avg_color += QuickSampleByPixel(vec2(x, y) + offset);
				}
			}
		}
	}

	// Compute the area of the pixel box that was sampled.
	float area_corners = area_nw + area_ne + area_sw + area_se;
	float area_edges = float(x_range) * (area_n + area_s) + float(y_range) * (area_w + area_e);
	float area_center = float(x_range) * float(y_range);

	// Return the normalized average color.
	return avg_color / (area_corners + area_edges + area_center);
}

/***** Main Functions *****/

void main()
{
	vec4 color;

	color = AreaSampling(v_texCoord);

	gl_FragColor = color;
}
