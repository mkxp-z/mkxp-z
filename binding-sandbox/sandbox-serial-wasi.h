/*
** sandbox-serial-wasi.h
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

#ifndef MKXPZ_SANDBOX_SERIAL_WASI_H
#define MKXPZ_SANDBOX_SERIAL_WASI_H
#include "wasi.cpp"
#endif // MKXPZ_SANDBOX_SERIAL_WASI_H

bool wasi_instance::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const {
    for (size_t i = 0; i < 2; ++i) {
        if (!::sandbox_serialize(stdio_line_buffers[i], data, max_size)) return false;
    }

    if (!::sandbox_serialize(prng_state, data, max_size)) return false;
    if (max_size < 4) return false;
    std::memcpy(data, prng_buffer, 4);
    data = (uint8_t *)data + 4;
    max_size -= 4;
    if (!::sandbox_serialize((uint8_t)prng_buffer_size, data, max_size)) return false;

    if (!::sandbox_serialize((uint32_t)fdtable.size(), data, max_size)) return false;

    uint32_t num_free_handles = 0;

    for (const struct wasi_file_entry &entry : fdtable) {
        if (entry.type == wasi_fd_type::FSDIR) {
            if (num_free_handles > 0) {
                if (!::sandbox_serialize((uint8_t)0, data, max_size)) return false;
                if (!::sandbox_serialize(num_free_handles, data, max_size)) return false;
                num_free_handles = 0;
            }
            if (!::sandbox_serialize((uint8_t)1, data, max_size)) return false;
            if (!::sandbox_serialize(entry.dir_handle()->root, data, max_size)) return false;
            if (!::sandbox_serialize(entry.dir_handle()->path, data, max_size)) return false;
            if (!::sandbox_serialize(entry.dir_handle()->writable, data, max_size)) return false;
        } else if (entry.type == wasi_fd_type::FSFILE) {
            if (num_free_handles > 0) {
                if (!::sandbox_serialize((uint8_t)0, data, max_size)) return false;
                if (!::sandbox_serialize(num_free_handles, data, max_size)) return false;
                num_free_handles = 0;
            }
            if (!::sandbox_serialize((uint8_t)2, data, max_size)) return false;
            if (!::sandbox_serialize(entry.file_handle()->offset, data, max_size)) return false;
            if (!::sandbox_serialize(entry.file_handle()->root, data, max_size)) return false;
            if (!::sandbox_serialize(entry.file_handle()->file.path(), data, max_size)) return false;
            if (!::sandbox_serialize(entry.file_handle()->file.is_read_open(), data, max_size)) return false;
            if (!::sandbox_serialize(entry.file_handle()->file.is_write_open(), data, max_size)) return false;
        } else if (entry.type == wasi_fd_type::FSDIRSTREAM) {
            if (num_free_handles > 0) {
                if (!::sandbox_serialize((uint8_t)0, data, max_size)) return false;
                if (!::sandbox_serialize(num_free_handles, data, max_size)) return false;
                num_free_handles = 0;
            }
            if (!::sandbox_serialize((uint8_t)3, data, max_size)) return false;
            if (!::sandbox_serialize((wasm_size_t)entry.dir_stream()->entries.size(), data, max_size)) return false;
            for (const std::pair<std::string, enum PHYSFS_FileType> &pair : entry.dir_stream()->entries) {
                if (!::sandbox_serialize(pair.first, data, max_size)) return false;
                if (!::sandbox_serialize(pair.second, data, max_size)) return false;
            }
        } else if (entry.type == wasi_fd_type::FSFILESTREAM) {
            if (num_free_handles > 0) {
                if (!::sandbox_serialize((uint8_t)0, data, max_size)) return false;
                if (!::sandbox_serialize(num_free_handles, data, max_size)) return false;
                num_free_handles = 0;
            }
            if (!::sandbox_serialize((uint8_t)4, data, max_size)) return false;
            if (!::sandbox_serialize(entry.file_stream()->offset, data, max_size)) return false;
            if (!::sandbox_serialize(entry.file_stream()->root, data, max_size)) return false;
        } else if (entry.type == wasi_fd_type::AISTREAM) {
            if (num_free_handles > 0) {
                if (!::sandbox_serialize((uint8_t)0, data, max_size)) return false;
                if (!::sandbox_serialize(num_free_handles, data, max_size)) return false;
                num_free_handles = 0;
            }
            if (!::sandbox_serialize((uint8_t)5, data, max_size)) return false;
            if (!::sandbox_serialize((wasm_size_t)entry.ai_stream()->entries.size(), data, max_size)) return false;
            for (const struct ai_stream_entry &e : entry.ai_stream()->entries) {
                if (!::sandbox_serialize(e.is_ipv6, data, max_size)) return false;
                if (!e.is_ipv6) {
                    for (size_t i = 0; i < 4; ++i) {
                        if (!::sandbox_serialize(e.inner.ipv4[i], data, max_size)) return false;
                    }
                } else {
                    for (size_t i = 0; i < 8; ++i) {
                        if (!::sandbox_serialize(e.inner.ipv6[i], data, max_size)) return false;
                    }
                }
            }
        } else {
            ++num_free_handles;
        }
    }
    if (num_free_handles > 0) {
        if (!::sandbox_serialize((uint8_t)0, data, max_size)) return false;
        if (!::sandbox_serialize(num_free_handles, data, max_size)) return false;
        num_free_handles = 0;
    }

    return true;
}

bool wasi_instance::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size) {
    for (size_t i = 0; i < 2; ++i) {
        if (!::sandbox_deserialize(stdio_line_buffers[i], data, max_size)) return false;
    }

    if (!::sandbox_deserialize(prng_state, data, max_size)) return false;
    if (max_size < 4) return false;
    std::memcpy(prng_buffer, data, 4);
    data = (uint8_t *)data + 4;
    max_size -= 4;
    {
        uint8_t size;
        if (!::sandbox_deserialize(size, data, max_size)) return false;
        prng_buffer_size = size % 5;
    }

    uint32_t size;
    if (!::sandbox_deserialize(size, data, max_size)) return false;
    if (size < fdtable.size() && (fdtable[size].type == wasi_fd_type::FS || fdtable[size].type == wasi_fd_type::STDIN || fdtable[size].type == wasi_fd_type::STDOUT || fdtable[size].type == wasi_fd_type::STDERR)) return false;

    for (uint32_t i = fdtable.size(); i > size;) {
        deallocate_file_descriptor(--i);
    }
    vacant_fds.clear();
    fdtable.resize(size, {nullptr, wasi_fd_type::VACANT});

    uint32_t i = 0;
    while (i < size) {
        uint8_t type;
        if (!::sandbox_deserialize(type, data, max_size)) return false;

        if (type == 0) {
            uint32_t num_free_handles;
            if (!::sandbox_deserialize(num_free_handles, data, max_size)) return false;
            if (i + num_free_handles > size || i + num_free_handles < i) return false;
            for (uint32_t j = i; j < i + num_free_handles; ++j) {
                if (fdtable[i].type != wasi_fd_type::FSDIR && fdtable[i].type != wasi_fd_type::FSFILE) {
                    continue;
                }
                deallocate_file_descriptor(j);
                vacant_fds.clear();
            }
            i += num_free_handles;
        } else {
            if (fdtable[i].type != wasi_fd_type::VACANT && fdtable[i].type != wasi_fd_type::FSDIR && fdtable[i].type != wasi_fd_type::FSFILE) {
                return false;
            }
            if (type == 1) { // FSDIR
                uint32_t root;
                if (!::sandbox_deserialize(root, data, max_size)) return false;
                if (root >= fdtable.size() || fdtable[root].type != wasi_fd_type::FS) return false;
                std::string path;
                if (!::sandbox_deserialize(path, data, max_size)) return false;
                path = mkxp_retro::fs->normalize(path.c_str(), false, true);
                bool writable;
                if (!::sandbox_deserialize(writable, data, max_size)) return false;
                if (fdtable[i].type != wasi_fd_type::VACANT && fdtable[i].type != wasi_fd_type::FSDIR) {
                    deallocate_file_descriptor(i);
                    vacant_fds.clear();
                }
                if (fdtable[i].type == wasi_fd_type::FSDIR) {
                    *fdtable[i].dir_handle() = {path, root, writable && fdtable[root].dir_handle()->writable};
                } else {
                    fdtable[i] = {new fs_dir {path, root, writable && fdtable[root].dir_handle()->writable}, wasi_fd_type::FSDIR};
                }
            } else if (type == 2) { // FSFILE
                uint64_t offset;
                if (!::sandbox_deserialize(offset, data, max_size)) return false;
                uint32_t root;
                if (!::sandbox_deserialize(root, data, max_size)) return false;
                if (root >= fdtable.size() || fdtable[root].type != wasi_fd_type::FS) return false;
                std::string path;
                if (!::sandbox_deserialize(path, data, max_size)) return false;
                path = mkxp_retro::fs->normalize(path.c_str(), false, true);
                bool is_read_open;
                if (!::sandbox_deserialize(is_read_open, data, max_size)) return false;
                bool is_write_open;
                if (!::sandbox_deserialize(is_write_open, data, max_size)) return false;
                if (!fdtable[root].dir_handle()->writable) {
                    is_write_open = false;
                }
                if ((fdtable[i].type != wasi_fd_type::VACANT && fdtable[i].type != wasi_fd_type::FSFILE) || (fdtable[i].type == wasi_fd_type::FSFILE && std::strcmp(path.c_str(), fdtable[i].file_handle()->file.path()))) {
                    deallocate_file_descriptor(i);
                    vacant_fds.clear();
                }
                bool existing_handle = fdtable[i].type == wasi_fd_type::FSFILE;
                if (existing_handle) {
                    if ((is_read_open && !fdtable[i].file_handle()->file.is_read_open()) || (is_write_open && !fdtable[i].file_handle()->file.is_write_open())) {
                        deallocate_file_descriptor(i);
                        vacant_fds.clear();
                        existing_handle = false;
                    } else if (!is_read_open) {
                        fdtable[i].file_handle()->file.close_read();
                    } else if (!is_write_open) {
                        fdtable[i].file_handle()->file.close_write();
                    }
                }
                struct fs_file *handle;
                if (existing_handle) {
                    handle = fdtable[i].file_handle();
                    handle->streams.clear();
                } else {
                    handle = new fs_file {{*mkxp_retro::fs, path.c_str(), is_write_open ? fdtable[root].dir_handle()->path.c_str() : nullptr, false, is_read_open, true}, {}, offset, root};
                    if ((is_read_open && !handle->file.is_read_open()) || (is_write_open && !handle->file.is_write_open())) {
                        delete handle;
                        return false;
                    }
                    fdtable[i] = {handle, wasi_fd_type::FSFILE};
                }
            } else if (type == 3) { // FSDIRSTREAM
                wasm_size_t length;
                if (!::sandbox_deserialize(length, data, max_size)) return false;
                std::deque<std::pair<std::string, enum PHYSFS_FileType>> deque(length);
                for (std::pair<std::string, enum PHYSFS_FileType> &pair : deque) {
                    if (!::sandbox_deserialize(pair.first, data, max_size)) return false;
                    if (!::sandbox_deserialize(pair.second, data, max_size)) return false;
                }
                fdtable[i] = {new fs_dir_stream {deque}, wasi_fd_type::FSDIRSTREAM};
            } else if (type == 4)  { // FSFILESTREAM
                uint64_t offset;
                if (!::sandbox_deserialize(offset, data, max_size)) return false;
                uint32_t root;
                if (!::sandbox_deserialize(root, data, max_size)) return false;
                if (root >= fdtable.size() || fdtable[root].type != wasi_fd_type::FSFILE) return false;
                fdtable[root].file_handle()->streams.insert(i);
                fdtable[i] = {new fs_file_stream {offset, root}, wasi_fd_type::FSFILESTREAM};
            } else if (type == 5) { // AISTREAM
                wasm_size_t length;
                if (!::sandbox_deserialize(length, data, max_size)) return false;
                std::deque<struct ai_stream_entry> deque(length);
                for (struct ai_stream_entry &e : deque) {
                    if (!::sandbox_deserialize(e.is_ipv6, data, max_size)) return false;
                    if (!e.is_ipv6) {
                        for (size_t i = 0; i < 4; ++i) {
                            if (!::sandbox_deserialize(e.inner.ipv4[i], data, max_size)) return false;
                        }
                    } else {
                        for (size_t i = 0; i < 8; ++i) {
                            if (!::sandbox_deserialize(e.inner.ipv6[i], data, max_size)) return false;
                        }
                    }
                }
                fdtable[i] = {new ai_stream {deque}, wasi_fd_type::AISTREAM};
            } else {
                return false;
            }
            ++i;
        }
    }

    std::vector<uint32_t> new_vacant_fds;
    for (uint32_t j = 0; j < fdtable.size(); ++j) {
        if (fdtable[j].type == wasi_fd_type::VACANT) {
            new_vacant_fds.push_back(j);
        }
    }
    vacant_fds = boost::container::priority_deque<uint32_t>(std::less<uint32_t>(), std::move(new_vacant_fds));

    return true;
}
