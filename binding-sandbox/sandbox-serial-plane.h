/*
** sandbox-serial-plane.h
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

#ifndef MKXPZ_SANDBOX_SERIAL_PLANE_H
#define MKXPZ_SANDBOX_SERIAL_PLANE_H
#include "plane.cpp"
#endif // MKXPZ_SANDBOX_SERIAL_PLANE_H

bool Plane::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
	if (!mkxp_sandbox::sandbox_serialize(p->opacity, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->blendType, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->ox, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->oy, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize((int32_t)p->realOX, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize((int32_t)p->realOY, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->realZoomX, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->realZoomY, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->zoomX, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->zoomY, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->isVisible, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->sceneGeo, data, max_size)) return false;

	if (!sandbox_serialize_viewport_element(data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_serialize(p->bitmap, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->realBitmap, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->color == &p->tmp.color ? nullptr : p->color, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->tone == &p->tmp.tone ? nullptr : p->tone, data, max_size)) return false;

	return true;
}

bool Plane::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	if (!mkxp_sandbox::sandbox_deserialize(p->opacity, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->blendType, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->ox, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->oy, data, max_size)) return false;
	{
		int32_t value = (int32_t)p->realOX;
		if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->realOX, data, max_size)) return false;
		if ((int32_t)p->realOX != value) {
			p->quadSourceDirty = true;
		}
	}
	{
		int32_t value = (int32_t)p->realOY;
		if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->realOY, data, max_size)) return false;
		if ((int32_t)p->realOY != value) {
			p->quadSourceDirty = true;
		}
	}
	{
		float value = p->realZoomX;
		if (!mkxp_sandbox::sandbox_deserialize(p->realZoomX, data, max_size)) return false;
		if (p->realZoomX != value) {
			p->quadSourceDirty = true;
		}
	}
	{
		float value = p->realZoomY;
		if (!mkxp_sandbox::sandbox_deserialize(p->realZoomY, data, max_size)) return false;
		if (p->realZoomY != value) {
			p->quadSourceDirty = true;
		}
	}
	if (!mkxp_sandbox::sandbox_deserialize(p->realZoomX, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->realZoomY, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->isVisible, data, max_size)) return false;
	{
		Scene::Geometry old_geo = p->sceneGeo;
		if (!mkxp_sandbox::sandbox_deserialize(p->sceneGeo, data, max_size)) return false;
		if (p->sceneGeo != old_geo) {
			p->quadSourceDirty = true;
		}
	}

	if (!sandbox_deserialize_viewport_element(data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_deserialize(p->bitmap, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->realBitmap, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->color, data, max_size)) return false;
	if (p->color == nullptr) {
		p->color = &p->tmp.color;
	}
	if (!mkxp_sandbox::sandbox_deserialize(p->tone, data, max_size)) return false;
	if (p->tone == nullptr) {
		p->tone = &p->tmp.tone;
	}

	return true;
}

void Plane::sandbox_deserialize_begin()
{
	sandbox_deserialize_begin_viewport_element();

	if (isDisposed()) return;

	p->bitmapDispCon.disconnect();
}

void Plane::sandbox_deserialize_end()
{
	if (isDisposed()) return;
	sandbox_deserialize_end_viewport_element();

	if (isDisposed()) return;
	if (p->bitmap != nullptr) {
		p->bitmapDispCon = p->bitmap->wasDisposed.connect(&PlanePrivate::bitmapDisposal, p);
		if (p->bitmap->isDisposed()) {
			p->bitmapDisposal();
		}
	}
}
