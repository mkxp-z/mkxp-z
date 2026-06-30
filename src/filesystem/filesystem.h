/*
** filesystem.h
**
** This file is part of mkxp.
**
** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
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

#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#ifdef MKXPZ_RETRO
#  include <memory>
#else
#  include <SDL_rwops.h>
#endif // MKXPZ_RETRO
#include <physfs.h>
#include <string>

#ifndef MKXPZ_RETRO
#include "filesystemImpl.h"

namespace mkxp_fs = filesystemImpl;
#endif // MKXPZ_RETRO

#include "exception.h"

struct FileSystemPrivate;
class SharedFontState;

class FileSystem
{
public:
	struct File
	{
		friend class FileSystem;

	private:
		std::string _path;
		PHYSFS_File *read_handle;
		PHYSFS_File *write_handle;
		PHYSFS_ErrorCode read_error;
		PHYSFS_ErrorCode write_error;

	public:
		// Opens a file using PhysFS. If the file doesn't already exist and write_path_prefix is non-null, it will be created.
		// read_path: Path to open the read handle for the file from.
		// write_path_prefix: Null to skip opening a write handle for the file, otherwise the write handle will be opened from the path corresponding to read_path with write_path_prefix removed from the beginning.
		// truncate: Whether or not to delete the contents of the file after opening it. Does nothing unless write_path_prefix is non-null.
		// open_read: Whether or not to open a read handle on this file.
		// exists: If you already know whether or not the file you're opening exists, you can set this to 1 if it exists or 0 if it doesn't exist to reduce the amount of file system calls needed. Any values other than 1 or 0 mean you don't know.
		File(FileSystem &fs, const char *read_path, const char *write_path_prefix = nullptr, bool truncate = false, bool open_read = true, unsigned char exists = -1);
		File(const struct File &) = delete;
		File(struct File &&) noexcept = delete;
		struct File operator=(const struct File &) = delete;
		struct File operator=(struct File &&) noexcept = delete;
		~File();
		inline const char *path() const noexcept {
			return _path.c_str();
		}
		inline PHYSFS_File *get_read() const noexcept {
			return read_handle;
		}
		inline PHYSFS_File *get_write() const noexcept {
			return write_handle;
		}
		inline PHYSFS_File *operator->() const noexcept {
			return get_read();
		}
		inline PHYSFS_File &operator*() const noexcept {
			return *get_read();
		}
		inline bool is_read_open() const noexcept {
			return read_handle != nullptr;
		}
		inline bool is_write_open() const noexcept {
			return write_handle != nullptr;
		}
		inline PHYSFS_ErrorCode get_read_error() const noexcept {
			return read_error;
		}
		inline PHYSFS_ErrorCode get_write_error() const noexcept {
			return write_error;
		}
		inline void close_read() noexcept {
			if (read_handle != nullptr) {
				PHYSFS_close(read_handle);
				read_handle = nullptr;
			}
		}
		inline void close_write() noexcept {
			if (write_handle != nullptr) {
				PHYSFS_close(write_handle);
				write_handle = nullptr;
			}
		}
	};

	FileSystem(const char *argv0,
	           bool allowSymlinks);
	~FileSystem();

	void addPath(Exception &exception, const char *path, const char *mountpoint = 0, bool reload = false);
    void removePath(Exception &exception, const char *path, bool reload = false);

	/* Call these after the last 'addPath()' */
	void createPathCache();
    
    void reloadPathCache();

	/* Scans "Fonts/" and creates inventory of
	 * available font assets */
	void initFontSets(SharedFontState &sfs);

	struct OpenHandler
	{
		Exception exception;

		/* Try to read and interpret data provided from ops.
		 * If data cannot be parsed, return false, otherwise true.
		 * Can be called multiple times until a parseable file is found.
		 * It's the handler's responsibility to close every passed
		 * ops structure, even when data could not be parsed.
		 * After this function returns, ops becomes invalid, so don't take
		 * references to it. Instead, copy the structure without closing
		 * if you need to further read from it later. */
		virtual bool tryRead(
#ifdef MKXPZ_RETRO
			std::shared_ptr<struct FileSystem::File> ops,
#else
			SDL_RWops &ops,
#endif // MKXPZ_RETRO
			const char *ext
		) = 0;
	};

	void openRead(OpenHandler &handler,
	              const char *filename);

#ifndef MKXPZ_RETRO
	/* Circumvents extension supplementing */
	void openReadRaw(SDL_RWops &ops,
	                 const char *filename,
	                 bool freeOnClose = false);
#endif // MKXPZ_RETRO

	std::string normalize(const char *pathname, bool preferred, bool absolute, const char *current_working_directory = nullptr);

	/* Does not perform extension supplementing */
	bool exists(const char *filename);

	const char *desensitize(const char *filename);

	bool enumerate(const char *path, PHYSFS_EnumerateCallback callback, void *data);

private:
	FileSystemPrivate *p;
};

#ifndef MKXPZ_RETRO
extern const Uint32 SDL_RWOPS_PHYSFS;
#endif // MKXPZ_RETRO

#endif // FILESYSTEM_H
