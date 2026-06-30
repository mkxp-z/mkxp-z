/*
** sandbox-serial-sprite.h
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

#ifndef MKXPZ_SANDBOX_SERIAL_SPRITE_H
#define MKXPZ_SANDBOX_SERIAL_SPRITE_H
#include "sprite.cpp"
#endif // MKXPZ_SANDBOX_SERIAL_SPRITE_H

bool Sprite::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
    if (!mkxp_sandbox::sandbox_serialize(p->wave.active, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize((int32_t)p->wave.amp, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize((int32_t)p->wave.length, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize((int32_t)p->wave.speed, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->wave.phase, data, max_size)) return false;

    if (!mkxp_sandbox::sandbox_serialize(p->trans, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->mirrored, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize((int32_t)p->bushDepth, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->bushOpacity, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->opacity, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->blendType, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->patternBlendType, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->patternTile, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->patternOpacity, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->patternScroll, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->patternZoom, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->invert, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->sceneGeo, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->isVisible, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize((int32_t)p->realOX, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize((int32_t)p->realOY, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->realZoomX, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->realZoomY, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->srcRect, data, max_size)) return false;

    if (!sandbox_serialize_viewport_element(data, max_size)) return false;

    if (!mkxp_sandbox::sandbox_serialize(p->bitmap, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->realBitmap, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->pattern, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->realSrcRect == &p->tmp.rect ? nullptr : p->realSrcRect, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->color == &p->tmp.color ? nullptr : p->color, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->tone == &p->tmp.tone ? nullptr : p->tone, data, max_size)) return false;

    return true;
}

bool Sprite::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
    {
        if (!mkxp_sandbox::sandbox_deserialize(p->wave.active, data, max_size)) return false;
        if (!p->wave.active) {
            p->wave.dirty = false;
        }
    }
    {
        int32_t value = (int32_t)p->wave.amp;
        if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->wave.amp, data, max_size)) return false;
        if ((int32_t)p->wave.amp != value) {
            if (p->wave.active) {
                p->wave.dirty = true;
            }
        }
    }
    {
        int32_t value = (int32_t)p->wave.length;
        if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->wave.length, data, max_size)) return false;
        if ((int32_t)p->wave.length != value) {
            if (p->wave.active) {
                p->wave.dirty = true;
            }
        }
    }
    {
        int32_t value = (int32_t)p->wave.speed;
        if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->wave.speed, data, max_size)) return false;
        if ((int32_t)p->wave.speed != value) {
            if (p->wave.active) {
                p->wave.dirty = true;
            }
        }
    }
    {
        float value = p->wave.phase;
        if (!mkxp_sandbox::sandbox_deserialize(p->wave.phase, data, max_size)) return false;
        if (p->wave.phase != value) {
            if (p->wave.active) {
                p->wave.dirty = true;
            }
        }
    }

    {
        float old_y = p->trans.getPosition().y;
        if (!mkxp_sandbox::sandbox_deserialize(p->trans, data, max_size)) return false;
        if (p->trans.getPosition().y != old_y) {
            p->deserYChanged = true;
            if (p->wave.active) {
                p->wave.dirty = true;
            }
        }
    }
    {
        bool value = p->mirrored;
        if (!mkxp_sandbox::sandbox_deserialize(p->mirrored, data, max_size)) return false;
        if (p->mirrored != value) {
            p->deserMirrorChanged = true;
            if (p->wave.active) {
                p->wave.dirty = true;
            }
        }
    }
    {
        int32_t value = (int32_t)p->bushDepth;
        if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->bushDepth, data, max_size)) return false;
        if ((int32_t)p->bushDepth != value) {
            p->deserBushDepthChanged = true;
        }
    }
    if (!mkxp_sandbox::sandbox_deserialize(p->bushOpacity, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(p->opacity, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(p->blendType, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(p->patternBlendType, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(p->patternTile, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(p->patternOpacity, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(p->patternScroll, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(p->patternZoom, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(p->invert, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(p->sceneGeo, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(p->isVisible, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->realOX, data, max_size)) return false;
    {
        int32_t value = p->realOY;
        if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->realOY, data, max_size)) return false;
        if ((int32_t)p->realOY != value) {
            if (p->wave.active) {
                p->wave.dirty = true;
            }
        }
    }
    {
        float value = p->realZoomX;
        if (!mkxp_sandbox::sandbox_deserialize(p->realZoomX, data, max_size)) return false;
        if (p->realZoomX != value) {
            if (p->wave.active) {
                p->wave.dirty = true;
            }
        }
    }
    {
        float value = p->realZoomY;
        if (!mkxp_sandbox::sandbox_deserialize(p->realZoomY, data, max_size)) return false;
        if (p->realZoomY != value) {
            p->deserBushDepthChanged = true;
            if (p->wave.active) {
                p->wave.dirty = true;
            }
        }
    }
    if (!mkxp_sandbox::sandbox_deserialize(p->srcRect, data, max_size)) return false;

    if (!sandbox_deserialize_viewport_element(data, max_size)) return false;

    if (!mkxp_sandbox::sandbox_deserialize(p->bitmap, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(p->realBitmap, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(p->pattern, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(p->realSrcRect, data, max_size)) return false;
    if (p->realSrcRect == nullptr) {
        p->realSrcRect = &p->tmp.rect;
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

void Sprite::sandbox_deserialize_begin()
{
    sandbox_deserialize_begin_viewport_element();

    if (isDisposed()) return;

    p->bitmapDispCon.disconnect();

    p->srcRectCon.disconnect();
    if (p->realSrcRect != nullptr) {
        p->deserSavedSrcRect = *p->realSrcRect;
    } else {
        p->deserSavedSrcRect.set(0, 0, 0, 0);
    }

    p->deserMirrorChanged = false;

    p->deserYChanged = false;

    p->deserBushDepthChanged = false;

    p->deserSavedBitmapId = p->bitmap != nullptr ? p->bitmap->id : -1;
}

void Sprite::sandbox_deserialize_end()
{
    if (isDisposed()) return;
    sandbox_deserialize_end_viewport_element();

    if (isDisposed()) return;
    if (p->bitmap != nullptr) {
        p->bitmapDispCon = p->bitmap->wasDisposed.connect(&SpritePrivate::bitmapDisposal, p);
        if (p->bitmap->isDisposed()) {
            p->bitmapDisposal();
        }
        if (p->wave.active && (p->bitmap != nullptr ? p->bitmap->id : -1) != p->deserSavedBitmapId) {
            p->wave.dirty = true;
        }
    }

    if (isDisposed()) return;
    if (p->realSrcRect != nullptr && p->realBitmap == p->bitmap) {
        p->srcRectCon = p->realSrcRect->valueChanged.connect(&SpritePrivate::onSrcRectChange, p);
        if (*p->realSrcRect != p->deserSavedSrcRect) {
            p->onSrcRectChange();
        }
    }

    if (isDisposed()) return;
    if (p->deserMirrorChanged) {
        p->onSrcRectChange();
    }

    if (isDisposed()) return;
    if (p->deserYChanged && rgssVer >= 2) {
        setSpriteY(p->trans.getPositionI().y);
    }

    if (isDisposed()) return;
    if (p->deserBushDepthChanged) {
        p->recomputeBushDepth();
    }
}
