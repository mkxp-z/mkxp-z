/*
** tilequad.h
**
** This file is part of mkxp. It is based on parts of "SFML 2.0" (license further below)
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

////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2012 Laurent Gomila (laurent.gom@gmail.com)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////


#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "etc-internal.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

class Transform
{
public:
	Transform()
	    : scale(1, 1),
	      rotation(0),
	      dirty(true)
	{
		memset(matrix, 0, sizeof(matrix));

		matrix[10] = 1;
		matrix[15] = 1;
	}

	Vec2 &getPosition() { return position; }
	Vec2 &getOrigin()   { return origin;   }
	Vec2 &getSrcRectOrigin()   { return srcRectOrigin;   }
	Vec2 &getScale()    { return scale;    }
	float getRotation() { return rotation; }

	Vec2i getPositionI() const
	{
		return Vec2i(position.x, position.y);
	}

	Vec2i getAdjustedOriginI() const
	{
		return Vec2i(adjustedOrigin.x, adjustedOrigin.y);
	}

	void setPosition(const Vec2 &value)
	{
		position = value;
		dirty = true;
	}

	void setOrigin(const Vec2 &value)
	{
		origin = value;
		adjustedOrigin.x = origin.x + srcRectOrigin.x;
		adjustedOrigin.y = origin.y + srcRectOrigin.y;
		dirty = true;
	}

	void setSrcRectOrigin(const Vec2 &value)
	{
		srcRectOrigin = value;
		adjustedOrigin.x = origin.x + srcRectOrigin.x;
		adjustedOrigin.y = origin.y + srcRectOrigin.y;
		dirty = true;
	}

	void setScale(const Vec2 &value)
	{
		scale = value;
		dirty = true;
	}

	void setRotation(float value)
	{
		rotation = value;
		dirty = true;
	}

	void setGlobalOffset(const Vec2i &value)
	{
		offset = value;
		dirty = true;
	}

	const float *getMatrix()
	{
		if (dirty)
		{
			updateMatrix();
			dirty = false;
		}

		return matrix;
	}

private:
	void updateMatrix()
	{
		if (rotation >= 360 || rotation < -360)
			rotation = (float) fmod(rotation, 360);

        // RGSS allows negative angles
		//if (rotation < 0)
		//	rotation += 360;

		float angle  = rotation * 3.141592654f / 180.0f;
		float cosine = (float) cos(angle);
		float sine   = (float) sin(angle);
		float sxc    = scale.x * cosine;
		float syc    = scale.y * cosine;
		float sxs    = scale.x * sine;
		float sys    = scale.y * sine;
		float tx     = -adjustedOrigin.x * sxc - adjustedOrigin.y * sys + position.x + offset.x;
		float ty     =  adjustedOrigin.x * sxs - adjustedOrigin.y * syc + position.y + offset.y;

		matrix[0]  =  sxc;
		matrix[1]  = -sxs;
		matrix[4]  =  sys;
		matrix[5]  =  syc;
		matrix[12] =  tx;
		matrix[13] =  ty;
	}

	Vec2 position;
	Vec2 origin;
	Vec2 srcRectOrigin;
	Vec2 adjustedOrigin;
	Vec2 scale;
	float rotation;

	/* Silently added to position */
	Vec2i offset;

	float matrix[16];

	bool dirty;
};

// Rotates a point around an origin point, counter-clockwise
// https://stackoverflow.com/a/2259502
static Vec2 rotate_point(const Vec2 &origin, const float &angle, Vec2 point)
{
    float s = sin(angle);
    float c = cos(angle);
    // translate point back to origin:
    point.x -= origin.x;
    point.y -= origin.y;
    // rotate point
    float xnew = point.x * c - point.y * s;
    float ynew = point.x * s + point.y * c;
    // translate point back:
    point.x = xnew + origin.x;
    point.y = ynew + origin.y;
    return point;
}

// Returns the rect that contains the source rect after rotating it around a point
static FloatRect rotate_rect(const Vec2i &origin, float rotation, const IntRect &sourceRect)
{
	// We use a "left-handed" coordinate system, with positive y values being below the x-axis,
	// so we need to use the negative of the angle to get the proper y values.
	float angle  = -rotation * M_PI / 180.0f;
	
	Vec2i pos = sourceRect.pos();
	Vec2i size = sourceRect.size();
	Vec2 p1 = rotate_point(origin, angle, pos);
	Vec2 p2 = rotate_point(origin, angle, Vec2i(pos.x + size.x, pos.y));
	Vec2 p3 = rotate_point(origin, angle, Vec2i(pos.x, pos.y + size.y));
	Vec2 p4 = rotate_point(origin, angle, pos + size);
	
	FloatRect result;
	
	float x = std::min(std::min(p1.x, p2.x), std::min(p3.x, p4.x));
	float y = std::min(std::min(p1.y, p2.y), std::min(p3.y, p4.y));
	result.x = x;
	result.y = y;
	result.w = ceil(std::max(std::max(p1.x, p2.x), std::max(p3.x, p4.x))) - floor(x);
	result.h = ceil(std::max(std::max(p1.y, p2.y), std::max(p3.y, p4.y))) - floor(y);
	
	return result;
}

#endif // TRANSFORM_H
