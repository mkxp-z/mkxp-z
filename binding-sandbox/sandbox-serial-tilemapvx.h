/*
** sandbox-serial-tilemapvx.h
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

#ifndef MKXPZ_SANDBOX_SERIAL_TILEMAPVX_H
#define MKXPZ_SANDBOX_SERIAL_TILEMAPVX_H
#include "tilemapvx.cpp"
#endif // MKXPZ_SANDBOX_SERIAL_TILEMAPVX_H

bool TilemapVX::BitmapArray::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
	if (!mkxp_sandbox::sandbox_serialize(tilemap, data, max_size)) return false;

	return true;
}

bool TilemapVX::BitmapArray::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	if (!mkxp_sandbox::sandbox_deserialize(tilemap, data, max_size)) return false;

	return true;
}

bool TilemapVX::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
	if (!mkxp_sandbox::sandbox_serialize(p->origin, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->sceneGeo, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->frameIdx, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->flashAlphaIdx, data, max_size)) return false;

	if (!p->above.sandbox_serialize_viewport_element(data, max_size)) return false;
	if (!p->sandbox_serialize_viewport_element(data, max_size)) return false;

	for (size_t i = 0; i < BM_COUNT; ++i)
		if (!mkxp_sandbox::sandbox_serialize(p->bitmaps[i], data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->mapData, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->flags, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->flashMap.getData(), data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_serialize(bmProxy, data, max_size)) return false;

	return true;
}

bool TilemapVX::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	{
		Vec2i old_origin = p->origin;
		if (!mkxp_sandbox::sandbox_deserialize(p->origin, data, max_size)) return false;
		if (p->origin != old_origin) {
			p->mapViewportDirty = true;
		}
	}
	{
		Scene::Geometry old_geo = p->sceneGeo;
		if (!mkxp_sandbox::sandbox_deserialize(p->sceneGeo, data, max_size)) return false;
		if (p->sceneGeo != old_geo) {
			p->buffersDirty = true;
			p->mapViewportDirty = true;
		}
	}
	if (!mkxp_sandbox::sandbox_deserialize(p->frameIdx, data, max_size)) return false;
	p->frameIdx %= 30*3*4;
	if (!mkxp_sandbox::sandbox_deserialize(p->flashAlphaIdx, data, max_size)) return false;
	p->flashAlphaIdx %= flashAlphaN;

	if (!p->above.sandbox_deserialize_viewport_element(data, max_size)) return false;
	if (!p->sandbox_deserialize_viewport_element(data, max_size)) return false;

	for (size_t i = 0; i < BM_COUNT; ++i)
		if (!mkxp_sandbox::sandbox_deserialize(p->bitmaps[i], data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->mapData, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->flags, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->flashMap.getData(), data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_deserialize(bmProxy, data, max_size)) return false;

	return true;
}

void TilemapVX::sandbox_deserialize_begin(bool is_new)
{
	if (isDisposed()) return;

	if (is_new) {
		delete bmProxy;
	}

	p->above.sandbox_deserialize_begin_viewport_element();

	p->sandbox_deserialize_begin_viewport_element();

	for (size_t i = 0; i < BM_COUNT; ++i) {
		p->bmDisposedCons[i].disconnect();
	}

	for (size_t i = 0; i < BM_COUNT; ++i) {
		p->bmChangedCons[i].disconnect();
		p->deserSavedBitmapIds[i] = p->bitmaps[i] == nullptr ? 0 : p->bitmaps[i]->id;
	}

	p->mapDataCon.disconnect();
	p->deserSavedMapDataId = p->mapData == nullptr ? 0 : p->mapData->id;

	p->flagsCon.disconnect();
	p->deserSavedFlagsId = p->flags == nullptr ? 0 : p->flags->id;

	p->deserSavedDataId = p->flashMap.getData() == nullptr ? 0 : p->flashMap.getData()->id;
}

void TilemapVX::sandbox_deserialize_end()
{
	if (isDisposed()) return;
	p->above.sandbox_deserialize_end_viewport_element();

	if (isDisposed()) return;
	p->sandbox_deserialize_end_viewport_element();

	for (size_t i = 0; i < BM_COUNT; ++i) {
		if (isDisposed()) return;
		if (p->bitmaps[i] != nullptr) {
			p->bmDisposedCons[i] = p->bitmaps[i]->wasDisposed.connect( [i, this] { p->atlasDisposal(i); } );
			if (p->bitmaps[i]->isDisposed()) {
				p->atlasDisposal(i);
			}
		}
	}

	for (size_t i = 0; i < BM_COUNT; ++i) {
		if (isDisposed()) return;
		if (p->bitmaps[i] != nullptr) {
			p->bmChangedCons[i] = p->bitmaps[i]->modified.connect(&TilemapVXPrivate::invalidateAtlas, p);
			if (p->bitmaps[i]->deserModified || p->bitmaps[i]->id != p->deserSavedBitmapIds[i]) {
				p->invalidateAtlas();
			}
		}
	}

	if (isDisposed()) return;
	if (p->mapData != nullptr) {
		p->mapDataCon = p->mapData->modified.connect(&TilemapVXPrivate::invalidateBuffers, p);
		if (p->mapData->deserModified || p->mapData->id != p->deserSavedMapDataId) {
			p->invalidateBuffers();
		}
	}

	if (isDisposed()) return;
	if (p->flags != nullptr) {
		p->flagsCon = p->flags->modified.connect(&TilemapVXPrivate::invalidateBuffers, p);
		if (p->flags->deserModified || p->flags->id != p->deserSavedFlagsId) {
			p->invalidateBuffers();
		}
	}

	if (isDisposed()) return;
	if (p->flashMap.getData() != nullptr && (p->flashMap.getData()->deserModified || p->flashMap.getData()->id != p->deserSavedDataId)) {
		p->flashMap.setDirty();
	}
}
