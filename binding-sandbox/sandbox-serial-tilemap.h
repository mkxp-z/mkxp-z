/*
** sandbox-serial-tilemap.h
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

#ifndef MKXPZ_SANDBOX_SERIAL_TILEMAP_H
#define MKXPZ_SANDBOX_SERIAL_TILEMAP_H
#include "tilemap.cpp"
#endif // MKXPZ_SANDBOX_SERIAL_TILEMAP_H

bool Tilemap::Autotiles::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
	if (!mkxp_sandbox::sandbox_serialize(tilemap, data, max_size)) return false;

	return true;
}

bool Tilemap::Autotiles::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	if (!mkxp_sandbox::sandbox_deserialize(tilemap, data, max_size)) return false;

	return true;
}

bool Tilemap::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
	if (!mkxp_sandbox::sandbox_serialize(p->visible, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->origin, data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_serialize(p->tiles.aniIdx, data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_serialize(p->flashAlphaIdx, data, max_size)) return false;

	if (!p->elem.ground->sandbox_serialize_viewport_element(data, max_size)) return false;

	for (size_t i = 0; i < zlayersMax; ++i)
		if (!p->elem.zlayers[i]->sandbox_serialize_viewport_element(data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_serialize(p->elem.sceneGeo, data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_serialize(p->opacity, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->blendType, data, max_size)) return false;

	for (size_t i = 0; i < autotileCount; ++i)
		if (!mkxp_sandbox::sandbox_serialize(p->autotiles[i], data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->tileset, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->mapData, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->priorities, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->flashMap.getData(), data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->color == &p->tmp.color ? nullptr : p->color, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->tone == &p->tmp.tone ? nullptr : p->tone, data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_serialize(atProxy, data, max_size)) return false;

	return true;
}

bool Tilemap::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	{
		bool value = p->visible;
		if (!mkxp_sandbox::sandbox_deserialize(p->visible, data, max_size)) return false;
		if (p->visible != value && p->tilemapReady) {
			{
				Exception e;
				p->elem.ground->setVisible(e, p->visible);
			}
			for (size_t i = 0; i < p->elem.activeLayers; ++i) {
				Exception e;
				p->elem.zlayers[i]->setVisible(e, p->visible);
			}
		}
	}
	{
		Vec2i old_origin = p->origin;
		if (!mkxp_sandbox::sandbox_deserialize(p->origin, data, max_size)) return false;
		if (p->origin != old_origin) {
			p->mapViewportDirty = true;
		}
		if (p->origin.y != old_origin.y) {
			p->zOrderDirty = true;
		}
	}

	if (!mkxp_sandbox::sandbox_deserialize(p->tiles.aniIdx, data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_deserialize(p->flashAlphaIdx, data, max_size)) return false;
	p->flashAlphaIdx %= flashAlphaN;

	if (!p->elem.ground->sandbox_deserialize_viewport_element(data, max_size)) return false;

	for (size_t i = 0; i < zlayersMax; ++i)
		if (!p->elem.zlayers[i]->sandbox_deserialize_viewport_element(data, max_size)) return false;

	{
		Scene::Geometry old_geo = p->elem.sceneGeo;
		if (!mkxp_sandbox::sandbox_deserialize(p->elem.sceneGeo, data, max_size)) return false;
		if (p->elem.sceneGeo != old_geo) {
			p->mapViewportDirty = true;
		}
	}

	if (!mkxp_sandbox::sandbox_deserialize(p->opacity, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->blendType, data, max_size)) return false;

	for (size_t i = 0; i < autotileCount; ++i)
		if (!mkxp_sandbox::sandbox_deserialize(p->autotiles[i], data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->tileset, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->mapData, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->priorities, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->flashMap.getData(), data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->color, data, max_size)) return false;
	if (p->color == nullptr) {
		p->color = &p->tmp.color;
	}
	if (!mkxp_sandbox::sandbox_deserialize(p->tone, data, max_size)) return false;
	if (p->tone == nullptr) {
		p->tone = &p->tmp.tone;
	}

	if (!mkxp_sandbox::sandbox_deserialize(atProxy, data, max_size)) return false;

	return true;
}

void Tilemap::sandbox_deserialize_begin(bool is_new)
{
	if (isDisposed()) return;

	if (is_new) {
		delete atProxy;
	}

	p->elem.ground->sandbox_deserialize_begin_viewport_element();

	for (size_t i = 0; i < zlayersMax; ++i) {
		p->elem.zlayers[i]->sandbox_deserialize_begin_viewport_element();
	}

	for (size_t i = 0; i < autotileCount; ++i) {
		p->autotilesDispCon[i].disconnect();
	}

	p->tilesetDispCon.disconnect();

	p->tilesetCon.disconnect();
	p->deserSavedTilesetId = p->tileset == nullptr ? 0 : p->tileset->id;

	for (size_t i = 0; i < autotileCount; ++i) {
		p->autotilesCon[i].disconnect();
		p->deserSavedAutotileIds[i] = p->autotiles[i] == nullptr ? 0 : p->autotiles[i]->id;
	}

	p->mapDataCon.disconnect();
	p->deserSavedMapDataId = p->mapData == nullptr ? 0 : p->mapData->id;

	p->prioritiesCon.disconnect();
	p->deserSavedPrioritiesId = p->priorities == nullptr ? 0 : p->priorities->id;

	p->deserSavedDataId = p->flashMap.getData() == nullptr ? 0 : p->flashMap.getData()->id;
}

void Tilemap::sandbox_deserialize_end()
{
	if (isDisposed()) return;
	p->elem.ground->sandbox_deserialize_end_viewport_element();

	for (size_t i = 0; i < zlayersMax; ++i) {
		if (isDisposed()) return;
		p->elem.zlayers[i]->sandbox_deserialize_end_viewport_element();
	}

	for (size_t i = 0; i < autotileCount; ++i) {
		if (isDisposed()) return;
		if (p->autotiles[i] != nullptr) {
			p->autotilesDispCon[i] = p->autotiles[i]->wasDisposed.connect( [i, this] { p->atlasContentsDisposal(i); } );
			if (p->autotiles[i]->isDisposed()) {
				p->atlasContentsDisposal(i);
			}
		}
	}

	if (isDisposed()) return;
	if (p->tileset != nullptr) {
		p->tilesetDispCon = p->tileset->wasDisposed.connect(&TilemapPrivate::tilesetDisposal, p);
		if (p->tileset->isDisposed()) {
			p->tilesetDisposal();
		}
	}

	if (isDisposed()) return;
	if (p->tileset != nullptr) {
		p->tilesetCon = p->tileset->modified.connect(&TilemapPrivate::invalidateAtlasSize, p);
		if (p->tileset->deserModified || p->tileset->id != p->deserSavedTilesetId) {
			p->invalidateAtlasSize();
			Exception e;
			p->updateAtlasInfo(e);
		}
	}

	for (size_t i = 0; i < autotileCount; ++i) {
		if (isDisposed()) return;
		if (p->autotiles[i] != nullptr) {
			p->autotilesCon[i] = p->autotiles[i]->modified.connect(&TilemapPrivate::invalidateAtlasContents, p);
			if (p->autotiles[i]->deserModified || p->autotiles[i]->id != p->deserSavedAutotileIds[i]) {
				p->invalidateAtlasContents();
				p->updateAutotileInfo();
			}
		} else if (p->deserSavedAutotileIds[i] != 0) {
			p->invalidateAtlasContents();
		}
	}

	if (isDisposed()) return;
	if (p->mapData != nullptr) {
		p->mapDataCon = p->mapData->modified.connect(&TilemapPrivate::invalidateBuffers, p);
		if (p->mapData->deserModified || p->mapData->id != p->deserSavedMapDataId) {
			p->invalidateBuffers();
		}
	}

	if (isDisposed()) return;
	if (p->priorities != nullptr) {
		p->prioritiesCon = p->priorities->modified.connect(&TilemapPrivate::invalidateBuffers, p);
		if (p->priorities->deserModified || p->priorities->id != p->deserSavedPrioritiesId) {
			p->invalidateBuffers();
		}
	}

	if (isDisposed()) return;
	if (p->flashMap.getData() != nullptr && (p->flashMap.getData()->deserModified || p->flashMap.getData()->id != p->deserSavedDataId)) {
		p->flashMap.setDirty();
	}
}
