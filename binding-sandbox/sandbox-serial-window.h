/*
** sandbox-serial-window.h
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

#ifndef MKXPZ_SANDBOX_SERIAL_WINDOW_H
#define MKXPZ_SANDBOX_SERIAL_WINDOW_H
#include "window.cpp"
#endif // MKXPZ_SANDBOX_SERIAL_WINDOW_H

bool Window::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
	if (!mkxp_sandbox::sandbox_serialize(p->bgStretch, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->active, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->pause, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->sceneOffset, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->position, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->size, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->contentsOffset, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->realContentsOffset, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->opacity, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->backOpacity, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->contentsOpacity, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->contentsVisible, data, max_size)) return false;

	if (!p->controlsElement.sandbox_serialize_viewport_element(data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_serialize(p->cursorAniAlphaIdx, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->pauseAniAlphaIdx, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->pauseAniQuadIdx, data, max_size)) return false;

	if (!sandbox_serialize_viewport_element(data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_serialize(p->windowskin, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->contents, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->realContents, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->cursorRect == &p->tmp.rect ? nullptr : p->cursorRect, data, max_size)) return false;

	return true;
}

bool Window::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	{
		bool value = p->bgStretch;
		if (!mkxp_sandbox::sandbox_deserialize(p->bgStretch, data, max_size)) return false;
		if (p->bgStretch != value) {
			p->baseVertDirty = true;
		}
	}
	if (!mkxp_sandbox::sandbox_deserialize(p->active, data, max_size)) return false;
	{
		bool value = p->pause;
		if (!mkxp_sandbox::sandbox_deserialize(p->pause, data, max_size)) return false;
		if (p->pause != value) {
			p->controlsVertDirty = true;
		}
	}
	if (!mkxp_sandbox::sandbox_deserialize(p->sceneOffset, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->position, data, max_size)) return false;
	{
		Vec2i value = p->size;
		if (!mkxp_sandbox::sandbox_deserialize(p->size, data, max_size)) return false;
		if (p->size != value) {
			p->baseVertDirty = true;
		}
	}
	if (!mkxp_sandbox::sandbox_deserialize(p->contentsOffset, data, max_size)) return false;
	{
		Vec2i value = p->realContentsOffset;
		if (!mkxp_sandbox::sandbox_deserialize(p->realContentsOffset, data, max_size)) return false;
		if (p->realContentsOffset != value) {
			p->controlsVertDirty = true;
		}
	}
	{
		bool value = p->opacity;
		if (!mkxp_sandbox::sandbox_deserialize(p->opacity, data, max_size)) return false;
		if (p->opacity != value) {
			p->opacityDirty = true;
		}
	}
	{
		bool value = p->backOpacity;
		if (!mkxp_sandbox::sandbox_deserialize(p->backOpacity, data, max_size)) return false;
		if (p->backOpacity != value) {
			p->opacityDirty = true;
		}
	}
	{
		NormValue value = p->contentsOpacity;
		if (!mkxp_sandbox::sandbox_deserialize(p->contentsOpacity, data, max_size)) return false;
		if (value != p->contentsOpacity) {
			p->contentsQuad.setColor(Vec4(1, 1, 1, p->contentsOpacity.norm));
		}
	}
	if (!mkxp_sandbox::sandbox_deserialize(p->contentsVisible, data, max_size)) return false;

	if (!p->controlsElement.sandbox_deserialize_viewport_element(data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_deserialize(p->cursorAniAlphaIdx, data, max_size)) return false;
	p->cursorAniAlphaIdx %= cursorAniAlphaN;
	if (!mkxp_sandbox::sandbox_deserialize(p->pauseAniAlphaIdx, data, max_size)) return false;
	p->pauseAniAlphaIdx = std::min(p->pauseAniAlphaIdx, (uint8_t)(pauseAniAlphaN - 1));
	if (!mkxp_sandbox::sandbox_deserialize(p->pauseAniQuadIdx, data, max_size)) return false;
	p->pauseAniQuadIdx %= pauseAniQuadN;

	if (!sandbox_deserialize_viewport_element(data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_deserialize(p->windowskin, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->contents, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->realContents, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->cursorRect, data, max_size)) return false;
	if (p->cursorRect == nullptr) {
		p->cursorRect = &p->tmp.rect;
	}

	return true;
}

void Window::sandbox_deserialize_begin()
{
	sandbox_deserialize_begin_viewport_element();

	if (isDisposed()) return;

	p->controlsElement.sandbox_deserialize_begin_viewport_element();

	p->windowskinDispCon.disconnect();

	p->contentsDispCon.disconnect();

	p->deserSavedContentsId = p->contents == nullptr ? 0 : p->contents->id;

	p->cursorRectCon.disconnect();
	if (p->cursorRect != nullptr) {
		p->deserSavedCursorRect = *p->cursorRect;
	} else {
		p->deserSavedCursorRect.set(0, 0, 0, 0);
	}
}

void Window::sandbox_deserialize_end()
{
	if (isDisposed()) return;
	sandbox_deserialize_end_viewport_element();

	if (isDisposed()) return;
	p->controlsElement.sandbox_deserialize_end_viewport_element();

	if (isDisposed()) return;
	if (p->windowskin != nullptr) {
		p->windowskinDispCon = p->windowskin->wasDisposed.connect(&WindowPrivate::windowskinDisposal, p);
		if (p->windowskin->isDisposed()) {
			p->windowskinDisposal();
		}
	}

	if (isDisposed()) return;
	if (p->contents != nullptr) {
		p->contentsDispCon = p->contents->wasDisposed.connect(&WindowPrivate::contentsDisposal, p);
		if (p->contents->isDisposed()) {
			p->contentsDisposal();
		}
	}

	if (isDisposed()) return;
	if (p->contents != nullptr && (p->contents->deserSizeChanged || p->contents->id != p->deserSavedContentsId)) {
		p->contentsQuad.setTexPosRect(p->contents->rect(), p->contents->rect());
	}

	if (isDisposed()) return;
	if (p->cursorRect != nullptr) {
		p->cursorRectCon = p->cursorRect->valueChanged.connect(&WindowPrivate::markControlVertDirty, p);
		if (*p->cursorRect != p->deserSavedCursorRect) {
			p->markControlVertDirty();
		}
	}
}
