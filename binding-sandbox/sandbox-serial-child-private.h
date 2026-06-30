/*
** sandbox-serial-child-private.h
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

#ifndef MKXPZ_SANDBOX_SERIAL_CHILD_PRIVATE_H
#define MKXPZ_SANDBOX_SERIAL_CHILD_PRIVATE_H
#include "bitmap.cpp"
#endif // MKXPZ_SANDBOX_SERIAL_CHILD_PRIVATE_H

bool ChildPrivate::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
    if (!mkxp_sandbox::sandbox_serialize(shared.realOffset, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(shared.realZoom, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(shared.offset, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(shared.zoom, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize((int32_t)shared.width, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize((int32_t)shared.height, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(shared.realSrcRect, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(shared.srcRect, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(shared.sceneElementType, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize((int32_t)shared.x, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize((int32_t)shared.y, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(shared.wrap, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(shared.mirrored, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(shared.angle, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize((int32_t)shared.waveAmp, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(shared.isVisible, data, max_size)) return false;

    if (!mkxp_sandbox::sandbox_serialize(parentPos, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(srcRect, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(oldSrcRect, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(dirty, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(maxShrink, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(currentShrink, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(mirrored, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(oldVR, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(oldOff, data, max_size)) return false;

    switch (shared.sceneElementType) {
        case ChildPublic::NONE:
            break;
        case ChildPublic::PLANE:
            if (!mkxp_sandbox::sandbox_serialize((Plane *)shared.sceneElement, data, max_size)) return false;
            break;
        case ChildPublic::SPRITE:
            if (!mkxp_sandbox::sandbox_serialize((Sprite *)shared.sceneElement, data, max_size)) return false;
            break;
        case ChildPublic::WINDOW:
            if (!mkxp_sandbox::sandbox_serialize((Window *)shared.sceneElement, data, max_size)) return false;
            break;
        case ChildPublic::WINDOWVX:
            if (!mkxp_sandbox::sandbox_serialize((WindowVX *)shared.sceneElement, data, max_size)) return false;
            break;
    }

    if (!mkxp_sandbox::sandbox_serialize(self, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(parent, data, max_size)) return false;

    return true;
}

bool ChildPrivate::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
    if (!mkxp_sandbox::sandbox_deserialize(shared.realOffset, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(shared.realZoom, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(shared.offset, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(shared.zoom, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize((int32_t &)shared.width, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize((int32_t &)shared.height, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(shared.realSrcRect, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(shared.srcRect, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(shared.sceneElementType, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize((int32_t &)shared.x, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize((int32_t &)shared.y, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(shared.wrap, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(shared.mirrored, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(shared.angle, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize((int32_t &)shared.waveAmp, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(shared.isVisible, data, max_size)) return false;

    if (!mkxp_sandbox::sandbox_deserialize(parentPos, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(srcRect, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(oldSrcRect, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(dirty, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(maxShrink, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(currentShrink, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(mirrored, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(oldVR, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(oldOff, data, max_size)) return false;

    switch (shared.sceneElementType) {
        case ChildPublic::NONE:
            break;
        case ChildPublic::PLANE:
            if (!mkxp_sandbox::sandbox_deserialize((Plane *&)shared.sceneElement, data, max_size)) return false;
            break;
        case ChildPublic::SPRITE:
            if (!mkxp_sandbox::sandbox_deserialize((Sprite *&)shared.sceneElement, data, max_size)) return false;
            break;
        case ChildPublic::WINDOW:
            if (!mkxp_sandbox::sandbox_deserialize((Window *&)shared.sceneElement, data, max_size)) return false;
            break;
        case ChildPublic::WINDOWVX:
            if (!mkxp_sandbox::sandbox_deserialize((WindowVX *&)shared.sceneElement, data, max_size)) return false;
            break;
        default:
            assert(!"unreachable");
    }

    if (!mkxp_sandbox::sandbox_deserialize(self, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(parent, data, max_size)) return false;

    return true;
}

void ChildPrivate::sandbox_deserialize_begin()
{
    dirtyCon.disconnect();
    disposeCon.disconnect();
}

void ChildPrivate::sandbox_deserialize_end()
{
    if (parent != nullptr) {
        dirtyCon = parent->modified.connect(&ChildPrivate::childDirty, this);
        disposeCon = parent->wasDisposed.connect(&ChildPrivate::parentDisposed, this);
        if (!parent->isDisposed() && parent->deserModified) {
            childDirty();
        }
        if (parent->isDisposed()) {
            parentDisposed();
        }
    }
}
