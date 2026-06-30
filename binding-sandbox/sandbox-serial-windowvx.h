/*
** sandbox-serial-audio.h
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

#ifndef MKXPZ_SANDBOX_SERIAL_WINDOWVX_H
#define MKXPZ_SANDBOX_SERIAL_WINDOWVX_H
#include "windowvx.cpp"
#endif // MKXPZ_SANDBOX_SERIAL_WINDOWVX_H

bool WindowVX::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
	if (!mkxp_sandbox::sandbox_serialize(p->active, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->arrowsVisible, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->pause, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->geo, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->contentsOff, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->realContentsOff, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize((int32_t)p->padding, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize((int32_t)p->paddingBottom, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->opacity, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->backOpacity, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->contentsOpacity, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->openness, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->pauseAlphaIdx, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->pauseQuadIdx, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->cursorAlphaIdx, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->sceneOffset, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->contentsVisible, data, max_size)) return false;

	if (!sandbox_serialize_viewport_element(data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_serialize(p->windowskin, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->contents, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->realContents, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->cursorRect == &p->tmp.rect ? nullptr : p->cursorRect, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->tone == &p->tmp.tone ? nullptr : p->tone, data, max_size)) return false;

	return true;
}

bool WindowVX::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	if (!mkxp_sandbox::sandbox_deserialize(p->active, data, max_size)) return false;
	{
		bool value = p->arrowsVisible;
		if (!mkxp_sandbox::sandbox_deserialize(p->arrowsVisible, data, max_size)) return false;
		if (p->arrowsVisible != value) {
			p->ctrlVertDirty = true;
		}
	}
	{
		bool value = p->pause;
		if (!mkxp_sandbox::sandbox_deserialize(p->pause, data, max_size)) return false;
		if (p->pause != value) {
			p->ctrlVertDirty = true;
		}
	}
	{
		Vec2i value = p->geo.size();
		if (!mkxp_sandbox::sandbox_deserialize(p->geo, data, max_size)) return false;
		if (p->geo.size() != value) {
			p->base.vertDirty = true;
			p->base.texSizeDirty = true;
			p->clipRectDirty = true;
			p->ctrlVertDirty = true;
			p->updateBaseQuad();
		}
		p->width = p->geo.w;
		p->height = p->geo.h;
	}
	if (!mkxp_sandbox::sandbox_deserialize(p->contentsOff, data, max_size)) return false;
	{
		Vec2i value = p->realContentsOff;
		if (!mkxp_sandbox::sandbox_deserialize(p->realContentsOff, data, max_size)) return false;
		if (p->realContentsOff != value) {
			p->ctrlVertDirty = true;
		}
	}
	{
		int32_t value = (int32_t)p->padding;
		if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->padding, data, max_size)) return false;
		if ((int32_t)p->padding != value) {
			p->clipRectDirty = true;
		}
	}
	{
		int32_t value = (int32_t)p->paddingBottom;
		if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->paddingBottom, data, max_size)) return false;
		if ((int32_t)p->paddingBottom != value) {
			p->clipRectDirty = true;
		}
	}
	{
		NormValue value = p->opacity;
		if (!mkxp_sandbox::sandbox_deserialize(p->opacity, data, max_size)) return false;
		if (p->opacity != value) {
			p->base.quad.setColor(Vec4(1, 1, 1, p->opacity.norm));
		}
	}
	{
		NormValue value = p->backOpacity;
		if (!mkxp_sandbox::sandbox_deserialize(p->backOpacity, data, max_size)) return false;
		if (p->backOpacity != value) {
			p->base.texDirty = true;
		}
	}
	{
		NormValue value = p->contentsOpacity;
		if (!mkxp_sandbox::sandbox_deserialize(p->contentsOpacity, data, max_size)) return false;
		if (p->contentsOpacity != value) {
			p->contentsQuad.setColor(Vec4(1, 1, 1, p->contentsOpacity.norm));
		}
	}
	{
		NormValue value = p->openness;
		if (!mkxp_sandbox::sandbox_deserialize(p->openness, data, max_size)) return false;
		if (p->openness != value) {
			p->updateBaseQuad();
		}
	}
	if (!mkxp_sandbox::sandbox_deserialize(p->pauseAlphaIdx, data, max_size)) return false;
	p->pauseAlphaIdx = std::min(p->pauseAlphaIdx, (uint8_t)(pauseAlphaN - 1));
	if (!mkxp_sandbox::sandbox_deserialize(p->pauseQuadIdx, data, max_size)) return false;
	p->pauseQuadIdx %= pauseQuadN;
	if (!mkxp_sandbox::sandbox_deserialize(p->cursorAlphaIdx, data, max_size)) return false;
	p->cursorAlphaIdx %= cursorAlphaN;
	if (!mkxp_sandbox::sandbox_deserialize(p->sceneOffset, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->contentsVisible, data, max_size)) return false;

	if (!sandbox_deserialize_viewport_element(data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_deserialize(p->windowskin, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->contents, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->realContents, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->cursorRect, data, max_size)) return false;
	if (p->cursorRect == nullptr) {
		p->cursorRect = &p->tmp.rect;
	}
	if (!mkxp_sandbox::sandbox_deserialize(p->tone, data, max_size)) return false;
	if (p->tone == nullptr) {
		p->tone = &p->tmp.tone;
	}

	return true;
}

void WindowVX::sandbox_deserialize_begin()
{
	sandbox_deserialize_begin_viewport_element();

	if (isDisposed()) return;

	p->windowskinDispCon.disconnect();

	p->contentsDispCon.disconnect();

	p->deserSavedContentsId = p->contents == nullptr ? 0 : p->contents->id;

	p->deserSavedWindowskinId = p->windowskin == nullptr ? 0 : p->windowskin->id;

	p->cursorRectCon.disconnect();
	if (p->cursorRect != nullptr) {
		p->deserSavedCursorRect = *p->cursorRect;
	} else {
		p->deserSavedCursorRect.set(0, 0, 0, 0);
	}

	p->toneCon.disconnect();
	if (p->tone != nullptr) {
		p->deserSavedTone = *p->tone;
	} else {
		p->deserSavedTone.set(0, 0, 0, 0);
	}
}

void WindowVX::sandbox_deserialize_end()
{
	if (isDisposed()) return;
	sandbox_deserialize_end_viewport_element();

	if (isDisposed()) return;
	if (p->windowskin != nullptr) {
		p->windowskinDispCon = p->windowskin->wasDisposed.connect(&WindowVXPrivate::windowskinDisposal, p);
		if (p->windowskin->isDisposed()) {
			p->windowskinDisposal();
		}
	}

	if (isDisposed()) return;
	if (p->contents != nullptr) {
		p->contentsDispCon = p->contents->wasDisposed.connect(&WindowVXPrivate::contentsDisposal, p);
		if (p->contents->isDisposed()) {
			p->contentsDisposal();
		}
	}

	if (isDisposed()) return;
	if (p->windowskin != nullptr && (p->windowskin->deserModified || p->windowskin->id != p->deserSavedWindowskinId)) {
		p->invalidateBaseTex();
	}

	if (isDisposed()) return;
	if (p->contents != nullptr && (p->contents->deserModified || p->contents->id != p->deserSavedContentsId)) {
		p->contentsQuad.setTexPosRect(p->contents->rect(), p->contents->rect());
		p->ctrlVertDirty = true;
	}

	if (isDisposed()) return;
	if (p->cursorRect != nullptr) {
		p->cursorRectCon = p->cursorRect->valueChanged.connect(&WindowVXPrivate::invalidateCursorVert, p);
		if (*p->cursorRect != p->deserSavedCursorRect) {
			p->invalidateCursorVert();
		}
	}

	if (isDisposed()) return;
	if (p->tone != nullptr) {
		p->toneCon = p->tone->valueChanged.connect(&WindowVXPrivate::invalidateBaseTex, p);
		if (*p->tone != p->deserSavedTone) {
			p->invalidateBaseTex();
		}
	}
}
