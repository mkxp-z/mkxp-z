/*
** sandbox-serial-scene.h
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

#ifndef MKXPZ_SANDBOX_SERIAL_SCENE_H
#define MKXPZ_SANDBOX_SERIAL_SCENE_H
#include "scene.cpp"
#endif // MKXPZ_SANDBOX_SERIAL_SCENE_H

bool SceneElement::sandbox_serialize_scene_element(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
	if (!mkxp_sandbox::sandbox_serialize(creationStamp, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize((int32_t)z, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(visible, data, max_size)) return false;

	return true;
}

bool SceneElement::sandbox_deserialize_scene_element(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	{
		uint64_t value = creationStamp;
		if (!mkxp_sandbox::sandbox_deserialize(creationStamp, data, max_size)) return false;
		if (creationStamp != value) {
			unlink();
		}
	}
	{
		int32_t value = (int32_t)z;
		if (!mkxp_sandbox::sandbox_deserialize((int32_t &)z, data, max_size)) return false;
		if (z != value) {
			unlink();
		}
	}
	if (!mkxp_sandbox::sandbox_deserialize(visible, data, max_size)) return false;

	return true;
}

void SceneElement::sandbox_deserialize_begin_scene_element()
{
	deserSceneElementWasUnlinked = false;
}

void SceneElement::sandbox_deserialize_end_scene_element()
{
	if (deserSceneElementWasUnlinked && scene != nullptr) {
		scene->insert(*this);
		onGeometryChange(scene->getGeometry());
	}
}
