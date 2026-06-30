/*
** sandbox-serial-viewport.h
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

#ifndef MKXPZ_SANDBOX_SERIAL_VIEWPORT_H
#define MKXPZ_SANDBOX_SERIAL_VIEWPORT_H
#include "viewport.cpp"
#endif // MKXPZ_SANDBOX_SERIAL_VIEWPORT_H

bool Viewport::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
	if (!mkxp_sandbox::sandbox_serialize(geometry.orig, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->screenRect, data, max_size)) return false;

	if (!sandbox_serialize_scene_element(data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_serialize(p->rect == &p->tmp.rect ? nullptr : p->rect, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->color == &p->tmp.color ? nullptr : p->color, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->tone == &p->tmp.tone ? nullptr : p->tone, data, max_size)) return false;

	return true;
}

bool Viewport::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	{
		Vec2i value = geometry.orig;
		if (!mkxp_sandbox::sandbox_deserialize(geometry.orig, data, max_size)) return false;
		if (geometry.orig != value) {
			p->deserGeometryChanged = true;
		}
	}

	{
		IntRect value = p->screenRect;
		if (!mkxp_sandbox::sandbox_deserialize(p->screenRect, data, max_size)) return false;
		if (p->screenRect != value) {
			p->deserScreenRectChanged = true;
		}
	}

	if (!sandbox_deserialize_scene_element(data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_deserialize(p->rect, data, max_size)) return false;
	if (p->rect == nullptr) {
		p->rect = &p->tmp.rect;
	}
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

void Viewport::sandbox_deserialize_begin()
{
	sandbox_deserialize_begin_scene_element();

	if (isDisposed()) return;

	p->rectCon.disconnect();
	if (p->rect != nullptr) {
		p->deserSavedRect = *p->rect;
	} else {
		p->deserSavedRect.set(0, 0, 0, 0);
	}

	p->deserGeometryChanged = false;

	p->deserScreenRectChanged = false;
}

void Viewport::sandbox_deserialize_end()
{
	if (isDisposed()) return;
	sandbox_deserialize_end_scene_element();

	if (isDisposed()) return;
	if (p->rect != nullptr) {
		p->rectCon = p->rect->valueChanged.connect(&ViewportPrivate::onRectChange, p);
		if (*p->rect != p->deserSavedRect) {
			geometry.rect = p->rect->toIntRect();
			p->deserGeometryChanged = true;
			p->deserScreenRectChanged = true;
		}
	}

	if (isDisposed()) return;
	if (p->deserGeometryChanged) {
		notifyGeometryChange();
	}

	if (isDisposed()) return;
	if (p->deserScreenRectChanged) {
		p->recomputeOnScreen();
	}
}

bool ViewportElement::sandbox_serialize_viewport_element(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
	if (!sandbox_serialize_scene_element(data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(m_viewport, data, max_size)) return false;

	return true;
}

bool ViewportElement::sandbox_deserialize_viewport_element(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	if (!sandbox_deserialize_scene_element(data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(m_viewport, data, max_size)) return false;

	return true;
}

void ViewportElement::sandbox_deserialize_begin_viewport_element()
{
	sandbox_deserialize_begin_scene_element();

	viewportDispCon.disconnect();

	deserSavedViewportId = m_viewport == nullptr ? 0 : m_viewport->id;
}

void ViewportElement::sandbox_deserialize_end_viewport_element()
{
	if (m_viewport != nullptr) {
		if (rgssVer == 1) {
			viewportDispCon = m_viewport->wasDisposed.connect(&ViewportElement::viewportElementDisposal, this);
			if (m_viewport->isDisposed()) {
				viewportElementDisposal();
			}
		}
	}

	if ((m_viewport != nullptr && m_viewport->id != deserSavedViewportId) || (m_viewport == nullptr && deserSavedViewportId != 0)) {
		if (!deserSceneElementWasUnlinked) {
			unlink();
		}
		scene = m_viewport == nullptr ? shState->screen() : m_viewport;
		onViewportChange();
	}

	sandbox_deserialize_end_scene_element();
}
