/*
** filesystem.cpp
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

#include "filesystem.h"

#include "util/boost-hash.h"
#include "util/debugwriter.h"
#include "util/exception.h"
#include "util/util.h"
#include "display/font.h"
#include "crypto/rgssad.h"

#include "eventthread.h"
#include "sharedstate.h"

#include <physfs.h>

#include <algorithm>
#include <stack>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vector>

#ifdef __WIN32__
#include <direct.h>
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

struct SDLRWIoContext {
  SDL_RWops *ops;
  std::string filename;

  SDLRWIoContext(const char *filename)
      : ops(SDL_RWFromFile(filename, "r")), filename(filename) {
    if (!ops)
      throw Exception(Exception::SDLError, "Failed to open file: %s",
                      SDL_GetError());
  }

  ~SDLRWIoContext() { SDL_RWclose(ops); }
};

static PHYSFS_Io *createSDLRWIo(const char *filename);

static SDL_RWops *getSDLRWops(PHYSFS_Io *io) {
  return static_cast<SDLRWIoContext *>(io->opaque)->ops;
}

static PHYSFS_sint64 SDLRWIoRead(struct PHYSFS_Io *io, void *buf,
                                 PHYSFS_uint64 len) {
  return SDL_RWread(getSDLRWops(io), buf, 1, len);
}

static int SDLRWIoSeek(struct PHYSFS_Io *io, PHYSFS_uint64 offset) {
  return (SDL_RWseek(getSDLRWops(io), offset, RW_SEEK_SET) != -1);
}

static PHYSFS_sint64 SDLRWIoTell(struct PHYSFS_Io *io) {
  return SDL_RWseek(getSDLRWops(io), 0, RW_SEEK_CUR);
}

static PHYSFS_sint64 SDLRWIoLength(struct PHYSFS_Io *io) {
  return SDL_RWsize(getSDLRWops(io));
}

static struct PHYSFS_Io *SDLRWIoDuplicate(struct PHYSFS_Io *io) {
  SDLRWIoContext *ctx = static_cast<SDLRWIoContext *>(io->opaque);
  int64_t offset = io->tell(io);
  PHYSFS_Io *dup = createSDLRWIo(ctx->filename.c_str());

  if (dup)
    SDLRWIoSeek(dup, offset);

  return dup;
}

static void SDLRWIoDestroy(struct PHYSFS_Io *io) {
  delete static_cast<SDLRWIoContext *>(io->opaque);
  delete io;
}

static PHYSFS_Io SDLRWIoTemplate = {0,
                                    0, /* version, opaque */
                                    SDLRWIoRead,
                                    0, /* write */
                                    SDLRWIoSeek,
                                    SDLRWIoTell,
                                    SDLRWIoLength,
                                    SDLRWIoDuplicate,
                                    0, /* flush */
                                    SDLRWIoDestroy};

static PHYSFS_Io *createSDLRWIo(const char *filename) {
  SDLRWIoContext *ctx;

  try {
    ctx = new SDLRWIoContext(filename);
  } catch (const Exception &e) {
    Debug() << "Failed mounting" << filename;
    return 0;
  }

  PHYSFS_Io *io = new PHYSFS_Io;
  *io = SDLRWIoTemplate;
  io->opaque = ctx;

  return io;
}

static inline PHYSFS_File *sdlPHYS(SDL_RWops *ops) {
  return static_cast<PHYSFS_File *>(ops->hidden.unknown.data1);
}

static Sint64 SDL_RWopsSize(SDL_RWops *ops) {
  PHYSFS_File *f = sdlPHYS(ops);

  if (!f)
    return -1;

  return PHYSFS_fileLength(f);
}

static Sint64 SDL_RWopsSeek(SDL_RWops *ops, int64_t offset, int whence) {
  PHYSFS_File *f = sdlPHYS(ops);

  if (!f)
    return -1;

  int64_t base;

  switch (whence) {
  default:
  case RW_SEEK_SET:
    base = 0;
    break;
  case RW_SEEK_CUR:
    base = PHYSFS_tell(f);
    break;
  case RW_SEEK_END:
    base = PHYSFS_fileLength(f);
    break;
  }

  int result = PHYSFS_seek(f, base + offset);

  return (result != 0) ? PHYSFS_tell(f) : -1;
}

static size_t SDL_RWopsRead(SDL_RWops *ops, void *buffer, size_t size,
                            size_t maxnum) {
  PHYSFS_File *f = sdlPHYS(ops);

  if (!f)
    return 0;

  PHYSFS_sint64 result = PHYSFS_readBytes(f, buffer, size * maxnum);

  return (result != -1) ? (result / size) : 0;
}

static size_t SDL_RWopsWrite(SDL_RWops *ops, const void *buffer, size_t size,
                             size_t num) {
  PHYSFS_File *f = sdlPHYS(ops);

  if (!f)
    return 0;

  PHYSFS_sint64 result = PHYSFS_writeBytes(f, buffer, size * num);

  return (result != -1) ? (result / size) : 0;
}

static int SDL_RWopsClose(SDL_RWops *ops) {
  PHYSFS_File *f = sdlPHYS(ops);

  if (!f)
    return -1;

  int result = PHYSFS_close(f);
  ops->hidden.unknown.data1 = 0;

  return (result != 0) ? 0 : -1;
}

static int SDL_RWopsCloseFree(SDL_RWops *ops) {
  int result = SDL_RWopsClose(ops);

  SDL_FreeRW(ops);

  return result;
}

/* Copies the first srcN characters from src into dst,
 * or the full string if srcN == -1. Never writes more
 * than dstMax, and guarantees dst to be null terminated.
 * Returns copied bytes (minus terminating null) */
static size_t strcpySafe(char *dst, const char *src, size_t dstMax, int srcN) {
  if (srcN < 0)
    srcN = strlen(src);

  size_t cpyMax = std::min<size_t>(dstMax - 1, srcN);

  memcpy(dst, src, cpyMax);
  dst[cpyMax] = '\0';

  return cpyMax;
}

/* Attempt to locate an extension string in a filename.
 * Either a pointer into the input string pointing at the
 * extension, or null is returned */
static const char *findExt(const char *filename) {
  size_t len;

  for (len = strlen(filename) - 1; len >= 0; --len) {
    if (filename[len] == '/')
      return 0;

    if (filename[len] == '.')
      return &filename[len + 1];
  }

  return 0;
}

static void initReadOps(PHYSFS_File *handle, SDL_RWops &ops, bool freeOnClose) {
  ops.size = SDL_RWopsSize;
  ops.seek = SDL_RWopsSeek;
  ops.read = SDL_RWopsRead;
  ops.write = SDL_RWopsWrite;

  if (freeOnClose)
    ops.close = SDL_RWopsCloseFree;
  else
    ops.close = SDL_RWopsClose;

  ops.type = SDL_RWOPS_PHYSFS;
  ops.hidden.unknown.data1 = handle;
}

const Uint32 SDL_RWOPS_PHYSFS = SDL_RWOPS_UNKNOWN + 10;

static void throwPhysfsError(const char *desc) {
  PHYSFS_ErrorCode ec = PHYSFS_getLastErrorCode();
  const char *englishStr;
    if (ec == 0) {
        // Sometimes on Windows PHYSFS_init can return null
        // but the error code never changes
        englishStr = "unknown error";
    } else {
        englishStr = PHYSFS_getErrorByCode(ec);
    }

  throw Exception(Exception::PHYSFSError, "%s: %s", desc, englishStr);
}

FileSystem::FileSystem(const char *argv0, bool allowSymlinks) {
  if (PHYSFS_init(argv0) == 0)
    throwPhysfsError("Error initializing PhysFS");

  /* One error (=return 0) turns the whole product to 0 */

  int er = 1;

  er *= PHYSFS_registerArchiver(&RGSS1_Archiver);
  er *= PHYSFS_registerArchiver(&RGSS2_Archiver);
  er *= PHYSFS_registerArchiver(&RGSS3_Archiver);

  if (er == 0)
    throwPhysfsError("Error registering PhysFS RGSS archiver");

  if (allowSymlinks)
    PHYSFS_permitSymbolicLinks(1);
}

FileSystem::~FileSystem() {
  if (PHYSFS_deinit() == 0)
    Debug() << "PhyFS failed to deinit.";
}

void FileSystem::addPath(const char *path, const char *mountpoint) {
  /* Try the normal mount first */
    int state = PHYSFS_mount(path, mountpoint, 1);
  if (!state) {
    /* If it didn't work, try mounting via a wrapped
     * SDL_RWops */
    PHYSFS_Io *io = createSDLRWIo(path);

    if (io)
      state = PHYSFS_mountIo(io, path, 0, 1);
  }
    if (!state) {
        PHYSFS_ErrorCode err = PHYSFS_getLastErrorCode();
        throw Exception(Exception::PHYSFSError, "Failed to mount %s (%s)", path, PHYSFS_getErrorByCode(err));
    }
}

void FileSystem::removePath(const char *path) {
    
    if (!PHYSFS_unmount(path)) {
        PHYSFS_ErrorCode err = PHYSFS_getLastErrorCode();
        throw Exception(Exception::PHYSFSError, "Failed to unmount %s (%s)", path, PHYSFS_getErrorByCode(err));
    }
}

static PHYSFS_EnumerateCallbackResult fontSetEnumCB(void *data, const char *dir,
                                                    const char *fname) {
  SharedFontState *sfs = static_cast<SharedFontState *>(data);

  /* Only consider filenames with font extensions */
  const char *ext = findExt(fname);

  if (!ext)
    return PHYSFS_ENUM_OK;

  if (strcasecmp(ext, "ttf") && strcasecmp(ext, "otf"))
    return PHYSFS_ENUM_OK;

  std::string filename(dir);
  filename.reserve(filename.size() + 1 + strlen(fname));
  filename.push_back('/');
  filename.append(fname);

  PHYSFS_File *handle = PHYSFS_openRead(filename.c_str());

  if (!handle)
    return PHYSFS_ENUM_ERROR;

  SDL_RWops ops;
  initReadOps(handle, ops, false);

  sfs->initFontSetCB(ops, filename);

  SDL_RWclose(&ops);

  return PHYSFS_ENUM_OK;
}

struct PhysfsCaseCBData {
  PHYSFS_EnumerateCallback cb;
  void *data;
  const std::vector<const char*>& path;
  char dir[1024] = {0};
  int index = 0;
  PHYSFS_EnumerateCallbackResult result = PHYSFS_ENUM_OK;
};

static PHYSFS_EnumerateCallbackResult PHYSFS_case_cb(void *data, const char *dir, const char *fname) {
  PhysfsCaseCBData *cbd = static_cast<PhysfsCaseCBData *>(data);
  if (cbd->result != PHYSFS_ENUM_OK) {
    return cbd->result;
  }
  if (cbd->index == cbd->path.size()) {
    cbd->result = cbd->cb(cbd->data, dir, fname);
    return cbd->result;
  }
  {
    std::string name = FileSystem::normalize(fname, false, false);
    if (strcasecmp(cbd->path[cbd->index], name.c_str())) {
      return PHYSFS_ENUM_OK;
    }
  }
  size_t dlen = strlen(cbd->dir);
  if(dlen + strlen(fname) + 2 > sizeof(cbd->dir)) {
    throw Exception(Exception::PHYSFSError, "path too large: dir=%s, file=%s, %d > %d", cbd->dir, fname, dlen + strlen(fname) + 2, sizeof(cbd->dir));
  }
  cbd->dir[dlen] = '/';
  strcpy(&cbd->dir[dlen+1], fname);
  cbd->index++;
  PHYSFS_enumerate(cbd->dir, PHYSFS_case_cb, cbd);
  cbd->index--;
  cbd->dir[dlen] = 0;
  return cbd->result;
}

/* Case-insensitive enumerate. Dir string does not support leading or trailing slashes. */
static int PHYSFS_case_enumerate(const char *dir, PHYSFS_EnumerateCallback c, void *d) {
  if (!dir || !dir[0]) {
    return PHYSFS_enumerate(dir, c, d);
  }
  std::string dirString(dir);
  std::vector<const char*> path = {dirString.c_str()};
  for(char &c : dirString) {
    if (c == '/') {
      c = 0;
      path.push_back(&c + 1);
    }
  }
  PhysfsCaseCBData data{c, d, path};
  return PHYSFS_enumerate("", PHYSFS_case_cb, &data);
}

void FileSystem::initFontSets(SharedFontState &sfs) {
  PHYSFS_case_enumerate("fonts", fontSetEnumCB, &sfs);
}

struct OpenReadEnumData {
  FileSystem::OpenHandler &handler;
  SDL_RWops ops{};

  std::map<std::string, std::vector<std::string>>& files;

  bool readFile(const char *filename, bool ignorePhysfsError) {
    std::string fname(filename);
    for(char &c : fname) c = tolower(c);
    for(const std::string &file : files[fname]) {
      PHYSFS_File *phys = PHYSFS_openRead(file.c_str());
      if (!phys) {
        /* Failing to open this file here means there must
         * be a deeper rooted problem somewhere within PhysFS.
         * Just abort all together. */
        if (!ignorePhysfsError)
          throw Exception(Exception::PHYSFSError, "PhysFS: %s, file=%s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()), file.c_str());
        continue;
      }
      initReadOps(phys, ops, false);

      const char *ext = findExt(file.c_str());
      if (handler.tryRead(ops, ext ? ext : ""))
        return true;
    }
    return false;
  }

  OpenReadEnumData(FileSystem::OpenHandler &handler, std::map<std::string, std::vector<std::string>>& files)
      : handler(handler), files(files) {}
};

static PHYSFS_EnumerateCallbackResult
openReadEnumCB(void *d, const char *dirpath, const char *filename) {
  OpenReadEnumData &data = *static_cast<OpenReadEnumData *>(d);

  std::string fullPath;
  if (dirpath) {
    fullPath.reserve(strlen(dirpath) + 1 + strlen(filename));
    fullPath.append(dirpath);
    fullPath.push_back('/');
  }
  fullPath.append(filename);

  std::string name = FileSystem::normalize(filename, false, false);
  for (char &c : name) c = tolower(c);

  data.files[name].push_back(fullPath);

  const char *ext = findExt(name.c_str());
  if (ext) {
    data.files[std::string(name.c_str(), ext-1)].push_back(fullPath);
  }

  return PHYSFS_ENUM_OK;
}

void FileSystem::openRead(OpenHandler &handler, const char *filename) {
  std::string filename_nm = normalize(filename, false, false);

  char *delim;

  /* Find the deliminator separating directory and file name */
  for (delim = &filename_nm.back(); delim > filename_nm.c_str(); --delim)
    if (*delim == '/')
      break;

  const bool root = (delim == filename_nm.c_str());

  const char *file = filename_nm.c_str();
  const char *dir = "";

  if (!root) {
    /* Cut the buffer in half so we can use it
     * for both filename and directory path */
    *delim = '\0';
    file = delim + 1;
    dir = filename_nm.c_str();
  }

  std::string lowerDir = dir;
  for (char &c : lowerDir) c = tolower(c);
  OpenReadEnumData data(handler, pathCache[lowerDir]);

  /* first check if the path cache contains the file, ignoring any physfs errors */
  if (data.readFile(file, true))
    return;

  /* next update the cache via case-sensitive search & try again */
  data.files.clear();
  PHYSFS_enumerate(dir, openReadEnumCB, &data);
  if (data.readFile(file, false))
    return;

  /* finally try case-insensitive search if the case-sensitive one fails */
  if (strlen(dir)) {
    data.files.clear();
    PHYSFS_case_enumerate(dir, openReadEnumCB, &data);
    if (data.readFile(file, false))
      return;
  }

  throw Exception(Exception::NoFileError, "%s", filename);
}

void FileSystem::openReadRaw(SDL_RWops &ops, const char *filename,
                             bool freeOnClose) {

  PHYSFS_File *handle = PHYSFS_openRead(filename);

  if (!handle)
    handle = PHYSFS_openRead(normalize(filename, 0, 0).c_str());

  if (!handle)
    throw Exception(Exception::NoFileError, "%s", filename);

  initReadOps(handle, ops, freeOnClose);
    return;
}

std::string FileSystem::normalize(const char *pathname, bool preferred, bool absolute) {
  return filesystemImpl::normalizePath(pathname, preferred, absolute);
}

bool FileSystem::exists(const char *filename) {
  return PHYSFS_exists(normalize(filename, false, false).c_str());
}
