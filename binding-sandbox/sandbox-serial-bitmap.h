/*
** sandbox-serial-bitmap.h
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

#ifndef MKXPZ_SANDBOX_SERIAL_BITMAP_H
#define MKXPZ_SANDBOX_SERIAL_BITMAP_H
#include "bitmap.cpp"
#endif // MKXPZ_SANDBOX_SERIAL_BITMAP_H

// This is here just to make sure that MKXPZ_BINDING_SANDBOX_HASH will change if DIFF_TILE_SIZE is changed in bitmap.cpp
static_assert(DIFF_TILE_SIZE == 64, "please change this line to contain the correct value of DIFF_TILE_SIZE");

bool Bitmap::sandbox_serialize_without_hires(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
    if (!mkxp_sandbox::sandbox_serialize((int32_t)width(), data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize((int32_t)height(), data, max_size)) return false;

    if (!mkxp_sandbox::sandbox_serialize(p->animation.enabled, data, max_size)) return false;

    if (p->animation.enabled) {
        if (!mkxp_sandbox::sandbox_serialize((mkxp_sandbox::wasm_size_t)p->animation.frames.size(), data, max_size)) return false;
        for (const BitmapFrame &frame : p->animation.frames) {
            if (!mkxp_sandbox::sandbox_serialize(frame.path, data, max_size)) return false;
            if (!mkxp_sandbox::sandbox_serialize((mkxp_sandbox::wasm_size_t)frame.originalFrameIndex, data, max_size)) return false;
            if (!sandbox_serialize_pixels(data, max_size, frame.diff)) return false;
        }
    } else {
        if (!mkxp_sandbox::sandbox_serialize(p->path, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_serialize((mkxp_sandbox::wasm_size_t)p->originalFrameIndex, data, max_size)) return false;
        if (!sandbox_serialize_pixels(data, max_size, p->diff)) return false;
    }

    if (p->animation.enabled) {
        if (!mkxp_sandbox::sandbox_serialize(p->animation.playing, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_serialize(p->animation.fps, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_serialize(p->animation.loop, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_serialize((int32_t)p->animation.lastFrame, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_serialize(p->animation.startTime, data, max_size)) return false;
    }

    if (!mkxp_sandbox::sandbox_serialize(p->font == &shState->defaultFont() ? nullptr : p->font, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->pChild, data, max_size)) return false;

    return true;
}

bool Bitmap::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
    if (!sandbox_serialize_without_hires(data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->selfHires, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->selfLores, data, max_size)) return false;
    return true;
}

bool Bitmap::sandbox_serialize_pixels(void *&data, mkxp_sandbox::wasm_size_t &max_size, const std::vector<std::vector<uint32_t>> &diff) const
{
    if (!mkxp_sandbox::sandbox_serialize((mkxp_sandbox::wasm_size_t)diff.size(), data, max_size)) return false;
    mkxp_sandbox::wasm_size_t num_empty_tiles = 0;
    mkxp_sandbox::wasm_size_t tile_number = 0;
    for (const std::vector<uint32_t> &tile : diff) {
        if (tile.empty()) {
            ++num_empty_tiles;
        } else {
            if (num_empty_tiles > 0) {
                if (!mkxp_sandbox::sandbox_serialize(false, data, max_size)) return false;
                if (!mkxp_sandbox::sandbox_serialize(num_empty_tiles, data, max_size)) return false;
                num_empty_tiles = 0;
            }
            if (!mkxp_sandbox::sandbox_serialize(true, data, max_size)) return false;
            size_t tile_col = tile_number % CEIL_DIV_DIFF_TILE_SIZE(width());
            size_t tile_row = tile_number / CEIL_DIV_DIFF_TILE_SIZE(width());
            size_t tile_width = std::min(DIFF_TILE_SIZE, width() - DIFF_TILE_SIZE * tile_col);
            size_t tile_height = std::min(DIFF_TILE_SIZE, height() - DIFF_TILE_SIZE * tile_row);
            MKXPZ_FORCED_ASSERT(tile.size() == tile_width * tile_height);
            if (max_size < 4 * tile_width * tile_height) return false;
            std::memcpy(data, tile.data(), 4 * tile_width * tile_height);
            data = (uint8_t *)data + 4 * tile_width * tile_height;
            max_size -= 4 * tile_width * tile_height;
        }
        ++tile_number;
    }
    if (num_empty_tiles > 0) {
        if (!mkxp_sandbox::sandbox_serialize(false, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_serialize(num_empty_tiles, data, max_size)) return false;
        num_empty_tiles = 0;
    }

    return true;
}

bool Bitmap::sandbox_deserialize_without_hires(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
    int32_t old_width = width();
    int32_t old_height = height();
    int32_t new_width;
    int32_t new_height;
    if (!mkxp_sandbox::sandbox_deserialize(new_width, data, max_size)) return false;
    if (new_width != old_width) {
        deserModified = true;
        deserSizeChanged = true;
    }
    if (!mkxp_sandbox::sandbox_deserialize(new_height, data, max_size)) return false;
    if (new_height != old_height) {
        deserModified = true;
        deserSizeChanged = true;
    }

    bool old_animation_enabled = p->animation.enabled;
    if (!mkxp_sandbox::sandbox_deserialize(p->animation.enabled, data, max_size)) return false;
    if (old_animation_enabled != p->animation.enabled) {
        deserModified = true;
    }

    if (p->animation.enabled) {
        mkxp_sandbox::wasm_size_t num_frames;
        if (!mkxp_sandbox::sandbox_deserialize(num_frames, data, max_size)) return false;

        // Check if any animation frames have had their paths or frame indices changed, or need to be reloaded based on the diffs in the save state, and if so, reload the bitmap
        bool need_reload = num_frames != p->animation.frames.size();
        if (!need_reload) {
            const void *tmp_data = data;
            mkxp_sandbox::wasm_size_t tmp_max_size = max_size;
            for (const BitmapFrame &frame : p->animation.frames) {
                std::string path;
                if (!mkxp_sandbox::sandbox_deserialize(path, tmp_data, tmp_max_size)) return false;
                if (path != frame.path) {
                    need_reload = true;
                    break;
                }
                mkxp_sandbox::wasm_size_t index;
                if (!mkxp_sandbox::sandbox_deserialize(index, tmp_data, tmp_max_size)) return false;
                if (index != (mkxp_sandbox::wasm_size_t)frame.originalFrameIndex) {
                    need_reload = true;
                    break;
                }
                bool diff_need_reload;
                bool diff_need_reload_if_path_not_empty;
                if (!sandbox_deserialize_pixels_check_need_reload(tmp_data, max_size, frame.diff, diff_need_reload, diff_need_reload_if_path_not_empty, true)) return false;
                if (diff_need_reload || (diff_need_reload_if_path_not_empty && !path.empty())) {
                    need_reload = true;
                    break;
                }
            }
        }

        // Reload the bitmap if needed
        if (need_reload) {
            delete p;
            {
                Exception e;
                initFromDimensions(e, new_width, new_height, true);
                if (e.is_error() || isMega()) {
                    return false;
                }
                p->animation.enabled = true;
                p->animation.playing = false;
                p->animation.width = new_width;
                p->animation.height = new_height;
                p->animation.lastFrame = 0;
                p->diff.clear();
                for (BitmapFrame &frame : p->animation.frames) {
                    shState->texPool().release(frame.gl);
                }
                p->animation.frames.clear();
            }
            deserModified = true;

            std::unordered_map<std::string, Bitmap *> sources;

            for (mkxp_sandbox::wasm_size_t i = 0; i < num_frames; ++i) {
                std::string path;
                if (!mkxp_sandbox::sandbox_deserialize(path, data, max_size)) return false;

                Bitmap *source;
                {
                    const auto it = sources.find(path);
                    if (it == sources.end()) {
                        Exception e;
                        source = path.empty() ? new Bitmap(e, new_width, new_height, true, false) : new Bitmap(e, path.c_str(), false);
                        if (e.is_error()) {
                            delete source;
                            return false;
                        }
                        sources.insert({path, source});
                    } else {
                        source = it->second;
                    }
                }

                mkxp_sandbox::wasm_size_t index;
                if (!mkxp_sandbox::sandbox_deserialize(index, data, max_size)) return false;

                TEXFBO *src_texfbo;
                if (source->isAnimated()) {
                    if (index >= source->p->animation.frames.size()) {
                        delete source;
                        return false;
                    }
                    src_texfbo = &source->p->animation.frames[index].gl;
                } else {
                    if (index != 0) {
                        delete source;
                        return false;
                    }
                    src_texfbo = &source->p->gl;
                }

                TEXFBO new_texfbo = *src_texfbo;
                TEXFBO::clear(*src_texfbo);

                p->animation.frames.push_back({new_texfbo, std::vector<std::vector<uint32_t>>(CEIL_DIV_DIFF_TILE_SIZE(p->animation.width) * CEIL_DIV_DIFF_TILE_SIZE(p->animation.height)), path, (int)index});

                delete source;
            }
        } else {
            for (mkxp_sandbox::wasm_size_t i = 0; i < num_frames; ++i) {
                std::string path;
                if (!mkxp_sandbox::sandbox_deserialize(path, data, max_size)) return false;
                mkxp_sandbox::wasm_size_t index;
                if (!mkxp_sandbox::sandbox_deserialize(index, data, max_size)) return false;
            }
        }

        for (BitmapFrame &frame : p->animation.frames) {
            if (!sandbox_deserialize_pixels(data, max_size, frame.diff)) return false;
        }
    } else {
        std::string old_path = p->path;
        if (!mkxp_sandbox::sandbox_deserialize(p->path, data, max_size)) return false;
        mkxp_sandbox::wasm_size_t old_index = p->originalFrameIndex;
        {
            mkxp_sandbox::wasm_size_t index;
            if (!mkxp_sandbox::sandbox_deserialize(index, data, max_size)) return false;
            p->originalFrameIndex = index;
        }
        bool need_reload;
        bool need_reload_if_path_not_empty;
        if (!sandbox_deserialize_pixels_check_need_reload(data, max_size, p->diff, need_reload, need_reload_if_path_not_empty, false)) return false;

        // Reload bitmap if its path has changed, or its size has changed, or if it needs to be reloaded based on the diff in the save state
        if (deserSizeChanged || need_reload || (need_reload_if_path_not_empty && !p->path.empty()) || (mkxp_sandbox::wasm_size_t)p->originalFrameIndex != old_index || p->path != old_path) {
            if (p->path.empty()) {
                delete p;
                {
                    Exception e;
                    initFromDimensions(e, new_width, new_height, true);
                    if (e.is_error()) {
                        return false;
                    }
                }
            } else {
                bool new_animation_enabled = p->animation.enabled;
                std::string path(p->path);
                delete p;
                {
                    Exception e;
                    initFromFilename(e, path.c_str());
                    if (e.is_error() || p->path.empty() || (!p->animation.enabled && new_animation_enabled) || width() != new_width || height() != new_height) {
                        return false;
                    }
                }

                // If the newly reloaded bitmap is animated but the save state has a non-animated bitmap,
                // turn it into a non-animated one
                if (p->animation.enabled && !new_animation_enabled) {
                    if (p->originalFrameIndex < 0 || (size_t)p->originalFrameIndex >= p->animation.frames.size()) {
                        return false;
                    }
                    p->animation.enabled = false;
                    p->animation.playing = false;
                    p->animation.width = 0;
                    p->animation.height = 0;
                    p->animation.lastFrame = 0;
                    p->gl = p->animation.frames[p->originalFrameIndex].gl;
                    p->diff = p->animation.frames[p->originalFrameIndex].diff;
                    p->path = p->animation.frames[p->originalFrameIndex].path;
                    p->originalFrameIndex = p->animation.frames[p->originalFrameIndex].originalFrameIndex;
                    for (BitmapFrame &frame : p->animation.frames) {
                        shState->texPool().release(frame.gl);
                    }
                    p->animation.frames.clear();
                }
            }
            deserModified = true;
        }

        if (!sandbox_deserialize_pixels(data, max_size, p->diff)) return false;
    }

    if (p->animation.enabled) {
        p->animation.width = p->gl.width;
        p->animation.height = p->gl.height;
        {
            bool old_playing = p->animation.playing;
            bool new_playing;
            if (!mkxp_sandbox::sandbox_deserialize(new_playing, data, max_size)) return false;
            if (new_playing != old_playing) {
                if (new_playing) {
                    p->animation.play();
                } else {
                    p->animation.stop();
                }
            }
        }
        {
            float value = p->animation.fps;
            if (!mkxp_sandbox::sandbox_deserialize(p->animation.fps, data, max_size)) return false;
            if (p->animation.fps < 0) {
                p->animation.fps = 0;
            }
            if (p->animation.fps != value) {
                bool restart = p->animation.playing;
                p->animation.stop();
                if (restart) {
                    p->animation.play();
                }
            }
        }
        if (!mkxp_sandbox::sandbox_deserialize(p->animation.loop, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->animation.lastFrame, data, max_size)) return false;
        p->animation.lastFrame = clamp(p->animation.lastFrame, 0, (int)p->animation.frames.size());
        if (!mkxp_sandbox::sandbox_deserialize(p->animation.startTime, data, max_size)) return false;
    }

    if (!mkxp_sandbox::sandbox_deserialize(p->font, data, max_size)) return false;
    if (p->font == nullptr) {
        p->font = &shState->defaultFont();
    }
    if (!mkxp_sandbox::sandbox_deserialize(p->pChild, data, max_size)) return false;

    return true;
}

bool Bitmap::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
    if (!sandbox_deserialize_without_hires(data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(p->selfHires, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(p->selfLores, data, max_size)) return false;
    return true;
}

bool Bitmap::sandbox_deserialize_pixels_check_need_reload(const void *&data, mkxp_sandbox::wasm_size_t &max_size, const std::vector<std::vector<uint32_t>> &diff, bool &need_reload, bool &need_reload_if_path_not_empty, bool modify_data_and_max_size) const
{
    need_reload = false;
    need_reload_if_path_not_empty = false;

    const void *tmp_data = data;
    mkxp_sandbox::wasm_size_t tmp_max_size = max_size;

    mkxp_sandbox::wasm_size_t num_tiles;
    if (!mkxp_sandbox::sandbox_deserialize(num_tiles, tmp_data, tmp_max_size)) return false;
    if (num_tiles != diff.size()) {
        need_reload = true;
        need_reload_if_path_not_empty = true;
        if (!modify_data_and_max_size) {
            return true;
        }
    }

    mkxp_sandbox::wasm_size_t tile_number = 0;
    while (num_tiles > 0) {
        bool is_not_empty;
        if (!mkxp_sandbox::sandbox_deserialize(is_not_empty, tmp_data, tmp_max_size)) return false;

        if (!is_not_empty) {
            mkxp_sandbox::wasm_size_t num_empty_tiles;
            if (!mkxp_sandbox::sandbox_deserialize(num_empty_tiles, tmp_data, tmp_max_size)) return false;

            while (num_empty_tiles > 0) {
                // Check for tiles that are empty in the save state but not currently empty
                if (!need_reload && !diff[tile_number].empty()) {
                    need_reload_if_path_not_empty = true;
                    if (!modify_data_and_max_size) {
                        return true;
                    }
                }

                ++tile_number;
                --num_tiles;
                --num_empty_tiles;
            }
        } else {
            size_t tile_col = tile_number % CEIL_DIV_DIFF_TILE_SIZE(width());
            size_t tile_row = tile_number / CEIL_DIV_DIFF_TILE_SIZE(width());
            size_t tile_width = std::min(DIFF_TILE_SIZE, width() - DIFF_TILE_SIZE * tile_col);
            size_t tile_height = std::min(DIFF_TILE_SIZE, height() - DIFF_TILE_SIZE * tile_row);

            if (tmp_max_size < 4 * tile_width * tile_height) return false;
            tmp_data = (uint8_t *)tmp_data + 4 * tile_width * tile_height;
            tmp_max_size -= 4 * tile_width * tile_height;
            ++tile_number;
            --num_tiles;
        }
    }

    if (modify_data_and_max_size) {
        data = tmp_data;
        max_size = tmp_max_size;
    }
    return true;
}

bool Bitmap::sandbox_deserialize_pixels(const void *&data, mkxp_sandbox::wasm_size_t &max_size, std::vector<std::vector<uint32_t>> &diff, mkxp_sandbox::wasm_size_t frame_number)
{
    mkxp_sandbox::wasm_size_t num_tiles;
    if (!mkxp_sandbox::sandbox_deserialize(num_tiles, data, max_size)) return false;
    if (num_tiles != diff.size()) {
        return false;
    }

    mkxp_sandbox::wasm_size_t tile_number = 0;
    while (num_tiles > 0) {
        bool is_not_empty;
        if (!mkxp_sandbox::sandbox_deserialize(is_not_empty, data, max_size)) return false;

        if (!is_not_empty) {
            mkxp_sandbox::wasm_size_t num_empty_tiles;
            if (!mkxp_sandbox::sandbox_deserialize(num_empty_tiles, data, max_size)) return false;

            while (num_empty_tiles > 0) {
                std::vector<uint32_t> &tile = diff[tile_number];

                // Clear tiles that are empty in the save state but not currently empty
                if (!tile.empty()) {
                    tile.clear();

                    size_t tile_col = tile_number % CEIL_DIV_DIFF_TILE_SIZE(width());
                    size_t tile_row = tile_number / CEIL_DIV_DIFF_TILE_SIZE(width());
                    size_t tile_width = std::min(DIFF_TILE_SIZE, width() - DIFF_TILE_SIZE * tile_col);
                    size_t tile_height = std::min(DIFF_TILE_SIZE, height() - DIFF_TILE_SIZE * tile_row);
                    IntRect rect = IntRect(DIFF_TILE_SIZE * tile_col, DIFF_TILE_SIZE * tile_row, tile_width, tile_height);

                    FBO::bind(p->animation.enabled ? p->animation.frames[frame_number].gl.fbo : p->gl.fbo);

                    glState.scissorTest.pushSet(true);
                    glState.scissorBox.pushSet(rect);
                    glState.clearColor.pushSet(Vec4());

                    FBO::clear();

                    glState.clearColor.pop();
                    glState.scissorBox.pop();
                    glState.scissorTest.pop();

                    p->substractTaintedArea(rect);
                    deserModified = true;
                }

                ++tile_number;
                --num_tiles;
                --num_empty_tiles;
            }
        } else {
            size_t tile_col = tile_number % CEIL_DIV_DIFF_TILE_SIZE(width());
            size_t tile_row = tile_number / CEIL_DIV_DIFF_TILE_SIZE(width());
            size_t tile_width = std::min(DIFF_TILE_SIZE, width() - DIFF_TILE_SIZE * tile_col);
            size_t tile_height = std::min(DIFF_TILE_SIZE, height() - DIFF_TILE_SIZE * tile_row);
            IntRect rect = IntRect(DIFF_TILE_SIZE * tile_col, DIFF_TILE_SIZE * tile_row, tile_width, tile_height);

            if (max_size < 4 * tile_width * tile_height) return false;

            bool tile_modified = false;

            std::vector<uint32_t> &tile = diff[tile_number];

            if (tile.size() != tile_width * tile_height) {
                tile.clear();
                tile.resize(tile_width * tile_height);
                tile_modified = true;
            }

            if (!tile_modified && std::memcmp(tile.data(), data, 4 * tile_width * tile_height)) {
                tile_modified = true;
            }

            // Upload modified tiles to the bitmap
            if (tile_modified) {
                std::memcpy(tile.data(), data, 4 * tile_width * tile_height);

                if (isMega()) {
                    for (size_t y = 0; y < (size_t)rect.h; ++y) {
                        std::memcpy((uint32_t *)p->megaSurface + p->megaSurface->w * (rect.y + y) + rect.x, (const uint32_t *)data + rect.w * y, 4 * rect.w);
                    }
                } else {
                    TEX::bind(p->animation.enabled ? p->animation.frames[frame_number].gl.tex : p->gl.tex);
                    TEX::uploadSubImage(rect.x, rect.y, rect.w, rect.h, data, GL_RGBA);
                }

                p->addTaintedArea(rect);
                deserModified = true;
            }

            data = (uint8_t *)data + 4 * tile_width * tile_height;
            max_size -= 4 * tile_width * tile_height;
            ++tile_number;
            --num_tiles;
        }
    }

    return true;
}

void Bitmap::sandbox_deserialize_begin(bool is_new)
{
    loresDispCon.disconnect();

    deserModified = is_new;

    deserSizeChanged = is_new;
}

void Bitmap::sandbox_deserialize_end(bool is_sandbox_object)
{
    if (isDisposed()) return;
    if (p->selfLores != nullptr) {
        loresDispCon = p->selfLores->wasDisposed.connect(&Bitmap::loresDisposal, this);
        if (p->selfLores->isDisposed()) {
            loresDisposal();
        }
    }

    if (isDisposed()) return;
    if ((p->selfHires != nullptr && p->selfHires->deserModified) || (p->selfLores != nullptr && p->selfLores->deserModified)) {
        deserModified = true;
    }

    if (isDisposed()) return;
    assumeRubyGC(is_sandbox_object && p->selfHires != nullptr);
}
