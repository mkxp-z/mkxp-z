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

#include "mkxp-polyfill.h" // snprintf

#include "util/boost-hash.h"
#include "util/debugwriter.h"
#include "util/exception.h"
#include "util/util.h"
#include "display/font.h"
#include "crypto/rgssad.h"

#include "eventthread.h"
#include "sharedstate.h"

#include <physfs.h>

#ifdef MKXPZ_RETRO
#  include <list>
#  include <memory>
#  include <sstream>
#  include "core.h"
#  include <boost/optional.hpp>
#endif // MKXPZ_RETRO

#include <algorithm>
#include <stack>
#include <string>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vector>

#ifdef __APPLE__
#include <iconv.h>
#endif

#ifdef __WIN32__
#include <direct.h>
#endif

#ifndef MKXPZ_RETRO
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
#endif // MKXPZ_RETRO

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

  for (len = strlen(filename); len > 0; --len) {
    if (filename[len] == '/')
      return 0;

    if (filename[len] == '.')
      return &filename[len + 1];
  }

  return 0;
}

#ifndef MKXPZ_RETRO
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
#endif // MKXPZ_RETRO

static void strTolower(std::string &str) {
  for (size_t i = 0; i < str.size(); ++i)
    str[i] = tolower(str[i]);
}

#ifndef MKXPZ_RETRO
const Uint32 SDL_RWOPS_PHYSFS = SDL_RWOPS_UNKNOWN + 10;
#endif // MKXPZ_RETRO

struct FileSystemPrivate {
  /* Maps: lower case full filepath,
   * To:   mixed case full filepath */
  BoostHash<std::string, std::string> pathCache;
  /* Maps: lower case directory path,
   * To:   list of lower case filenames */
  BoostHash<std::string, std::vector<std::string>> fileLists;

  /* This is for compatibility with games that take Windows'
   * case insensitivity for granted */
  bool havePathCache;
};

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

  MKXPZ_THROW(Exception(Exception::PHYSFSError, "%s: %s", desc, englishStr));
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

  p = new FileSystemPrivate;
  p->havePathCache = false;

  if (allowSymlinks)
    PHYSFS_permitSymbolicLinks(1);
}

FileSystem::~FileSystem() {
  delete p;

  if (PHYSFS_deinit() == 0)
    Debug() << "PhyFS failed to deinit.";
}

void FileSystem::addPath(Exception &exception, const char *path, const char *mountpoint, bool reload) {
  /* Try the normal mount first */
    int state = PHYSFS_mount(path, mountpoint, 1);

#ifndef MKXPZ_RETRO
  if (!state) {
    /* If it didn't work, try mounting via a wrapped
     * SDL_RWops */
    PHYSFS_Io *io = createSDLRWIo(path);

    if (io)
      state = PHYSFS_mountIo(io, path, 0, 1);
  }
#endif // MKXPZ_RETRO

    if (!state) {
        PHYSFS_ErrorCode err = PHYSFS_getLastErrorCode();
        exception = Exception(Exception::PHYSFSError, "Failed to mount %s (%s)", path, PHYSFS_getErrorByCode(err));
        return;
    }
    
    if (reload) reloadPathCache();
    exception = Exception(Exception::Ok, "");
}

void FileSystem::removePath(Exception &exception, const char *path, bool reload) {
    
    if (!PHYSFS_unmount(path)) {
        PHYSFS_ErrorCode err = PHYSFS_getLastErrorCode();
        exception = Exception(Exception::PHYSFSError, "Failed to unmount %s (%s)", path, PHYSFS_getErrorByCode(err));
    }
    
    if (reload) reloadPathCache();
    exception = Exception(Exception::Ok, "");
}

struct CacheEnumData {
  FileSystemPrivate *p;
  std::stack<std::vector<std::string> *> fileLists;

#ifdef __APPLE__
  iconv_t nfd2nfc;
  char buf[512];
#endif

  CacheEnumData(FileSystemPrivate *p) : p(p) {
#ifdef __APPLE__
    nfd2nfc = iconv_open("utf-8", "utf-8-mac");
#endif
  }

  ~CacheEnumData() {
#ifdef __APPLE__
    iconv_close(nfd2nfc);
#endif
  }

  /* Converts in-place */
  void toNFC(char *inout) {
#ifdef __APPLE__
    size_t srcSize = strlen(inout);
    size_t bufSize = sizeof(buf);
    char *bufPtr = buf;
    char *inoutPtr = inout;

    /* Reserve room for null terminator */
    --bufSize;

    iconv(nfd2nfc, &inoutPtr, &srcSize, &bufPtr, &bufSize);
    /* Null-terminate */
    *bufPtr = 0;
    strcpy(inout, buf);
#else
    (void)inout;
#endif
  }
};

static PHYSFS_EnumerateCallbackResult cacheEnumCB(void *d, const char *origdir,
                                                  const char *fname) {
  if (shState && shState->rtData().rqTerm)
    return PHYSFS_ENUM_ERROR;

#ifdef MKXPZ_RETRO
  // Don't cache the /Dist or /System directories because the game doesn't need to access them
  if (!*origdir && (!strcmp(fname, "Dist") || !strcmp(fname, "System")))
    return PHYSFS_ENUM_OK;
#endif // MKXPZ_RETRO

  CacheEnumData &data = *static_cast<CacheEnumData *>(d);
  char fullPath[4096];

  if (!*origdir)
#ifdef MKXPZ_RETRO
    snprintf(fullPath, sizeof(fullPath), "/%s", fname);
#else
    snprintf(fullPath, sizeof(fullPath), "%s", fname);
#endif // MKXPZ_RETRO
  else
    snprintf(fullPath, sizeof(fullPath), "%s/%s", origdir, fname);

  /* Deal with OSX' weird UTF-8 standards */
  data.toNFC(fullPath);

  std::string mixedCase(fullPath);
  std::string lowerCase = mixedCase;
  strTolower(lowerCase);

  PHYSFS_Stat stat;
  PHYSFS_stat(fullPath, &stat);

  if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
    /* Create a new list for this directory */
    std::vector<std::string> &list = data.p->fileLists[lowerCase];

    /* Iterate over its contents */
    data.fileLists.push(&list);
    PHYSFS_enumerate(fullPath, cacheEnumCB, d);
    data.fileLists.pop();
  } else {
    /* Get the file list for the directory we're currently
     * traversing and append this filename to it */
    std::vector<std::string> &list = *data.fileLists.top();

    std::string lowerFilename(fname);
    strTolower(lowerFilename);
    list.push_back(lowerFilename);

    /* Add the lower -> mixed mapping of the file's full path */
    data.p->pathCache.insert(lowerCase, mixedCase);
  }

  return PHYSFS_ENUM_OK;
}

void FileSystem::createPathCache() {
  Debug() << "Loading path cache...";

  CacheEnumData data(p);
  data.fileLists.push(&p->fileLists[""]);
  PHYSFS_enumerate("", cacheEnumCB, &data);

  p->havePathCache = true;

  Debug() << "Path cache completed.";
}

void FileSystem::reloadPathCache() {
    if (!p->havePathCache) return;
    
    p->fileLists.clear();
    p->pathCache.clear();
    createPathCache();
}

struct FontSetsCBData {
  FileSystemPrivate *p;
  SharedFontState *sfs;
};

static PHYSFS_EnumerateCallbackResult fontSetEnumCB(void *data, const char *dir,
                                                    const char *fname) {
  FontSetsCBData *d = static_cast<FontSetsCBData *>(data);

  /* Only consider filenames with font extensions */
  const char *ext = findExt(fname);

  if (!ext)
    return PHYSFS_ENUM_OK;

  char lowExt[8];
  size_t i;

  for (i = 0; i < sizeof(lowExt) - 1 && ext[i]; ++i)
    lowExt[i] = tolower(ext[i]);
  lowExt[i] = '\0';

  if (strcmp(lowExt, "ttf") && strcmp(lowExt, "otf"))
    return PHYSFS_ENUM_OK;

  char filename[4096];
  snprintf(filename, sizeof(filename), "%s/%s", dir, fname);

#ifdef MKXPZ_RETRO
  std::shared_ptr<FileSystem::File> handle(new FileSystem::File(*mkxp_retro::fs, filename));
#else
  PHYSFS_File *handle = PHYSFS_openRead(filename);
#endif // MKXPZ_RETRO

#ifdef MKXPZ_RETRO
  if (!handle->is_read_open())
#else
  if (!handle)
#endif // MKXPZ_RETRO
    return PHYSFS_ENUM_ERROR;

#ifdef MKXPZ_RETRO
  d->sfs->initFontSetCB(handle, filename);
#else
  SDL_RWops ops;
  initReadOps(handle, ops, false);

  d->sfs->initFontSetCB(ops, filename);

  SDL_RWclose(&ops);
#endif // MKXPZ_RETRO

  return PHYSFS_ENUM_OK;
}

/* Basically just a case-insensitive search
 * for the folder "Fonts"... */
static PHYSFS_EnumerateCallbackResult
findFontsFolderCB(void *data, const char *dir, const char *fname) {
  size_t i = 0;
  char buffer[4096];
  const char *s = fname;

  while (*s && i < sizeof(buffer))
    buffer[i++] = tolower(*s++);

  buffer[i] = '\0';

  if (strcmp(buffer, "fonts") == 0)
    PHYSFS_enumerate(*dir == 0 ? fname : (std::string(dir) + '/' + fname).c_str(), fontSetEnumCB, data);

  return PHYSFS_ENUM_OK;
}

void FileSystem::initFontSets(SharedFontState &sfs) {
  FontSetsCBData d = {p, &sfs};

  PHYSFS_enumerate(
#ifdef MKXPZ_RETRO
    "/Game",
#else
    "",
#endif // MKXPZ_RETRO
    findFontsFolderCB,
    &d
  );
}

struct OpenReadEnumData {
#ifdef MKXPZ_RETRO
  boost::optional<std::shared_ptr<struct FileSystem::File>> ops;
#else
  SDL_RWops ops;
#endif // MKXPZ_RETRO

  FileSystem::OpenHandler &handler;

  /* The filename (without directory) we're looking for */
  const char *filename;
  size_t filenameN;

  /* Optional hash to translate full filepaths
   * (used with path cache) */
  BoostHash<std::string, std::string> *pathTrans;

  /* Number of files we've attempted to read and parse */
  size_t matchCount;
  bool stopSearching;

  /* In case of a PhysFS error, save it here so it
   * doesn't get changed before we get back into our code */
  const char *physfsError;

  OpenReadEnumData(FileSystem::OpenHandler &handler, const char *filename,
                   size_t filenameN,
                   BoostHash<std::string, std::string> *pathTrans)
      : handler(handler), filename(filename), filenameN(filenameN),
        pathTrans(pathTrans), matchCount(0), stopSearching(false),
        physfsError(0) {}
};

static PHYSFS_EnumerateCallbackResult
openReadEnumCB(void *d, const char *dirpath, const char *filename) {
  OpenReadEnumData &data = *static_cast<OpenReadEnumData *>(d);
  char buffer[512];
  const char *fullPath;

  if (data.stopSearching)
    return PHYSFS_ENUM_STOP;

  /* If there's not even a partial match, continue searching */
  if (strncmp(filename, data.filename, data.filenameN) != 0)
    return PHYSFS_ENUM_OK;

  if (!*dirpath) {
    fullPath = filename;
  } else {
    snprintf(buffer, sizeof(buffer), "%s/%s", dirpath, filename);
    fullPath = buffer;
  }

  char last = filename[data.filenameN];
  /* If fname matches up to a following '.' (meaning the rest is part
   * of the extension), or up to a following '\0' (full match), we've
   * found our file */
  if (last != '.' && last != '\0')
    return PHYSFS_ENUM_OK;

  /* If the path cache is active, translate from lower case
   * to mixed case path */
  if (data.pathTrans)
    fullPath = (*data.pathTrans)[fullPath].c_str();

#ifdef MKXPZ_RETRO
  data.ops.emplace(new FileSystem::File(*mkxp_retro::fs, fullPath));
#else
  PHYSFS_File *phys = PHYSFS_openRead(fullPath);

  if (!phys) {
    /* Failing to open this file here means there must
     * be a deeper rooted problem somewhere within PhysFS.
     * Just abort alltogether. */
    data.stopSearching = true;
    data.physfsError = PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());

    return PHYSFS_ENUM_ERROR;
  }

  initReadOps(phys, data.ops, false);
#endif // MKXPZ_RETRO

  const char *ext = findExt(filename);

#ifdef MKXPZ_RETRO
  if (data.handler.tryRead(*data.ops, ext))
#else
  if (data.handler.tryRead(data.ops, ext))
#endif // MKXPZ_RETRO
    data.stopSearching = true;

  ++data.matchCount;
  return PHYSFS_ENUM_OK;
}

void FileSystem::openRead(OpenHandler &handler, const char *filename) {
  std::string filename_nm = normalize(
    filename,
    false,
#ifdef MKXPZ_RETRO
    true
#else
    false
#endif // MKXPZ_RETRO
  );
  char buffer[512];
  size_t len = strcpySafe(buffer, filename_nm.c_str(), sizeof(buffer), -1);
  char *delim;

  if (p->havePathCache)
    for (size_t i = 0; i < len; ++i)
      buffer[i] = tolower(buffer[i]);

  /* Find the deliminator separating directory and file name */
  for (delim = buffer + len; delim > buffer; --delim)
    if (*delim == '/')
      break;

  const bool root = (delim == buffer);

  const char *file = buffer;
  const char *dir = "";

  if (!root) {
    /* Cut the buffer in half so we can use it
     * for both filename and directory path */
    *delim = '\0';
    file = delim + 1;
    dir = buffer;
  }
  OpenReadEnumData data(handler, file, len + buffer - delim - !root,
                        p->havePathCache ? &p->pathCache : 0);

  if (p->havePathCache) {
    /* Get the list of files contained in this directory
     * and manually iterate over them */
    const std::vector<std::string> &fileList = p->fileLists[dir];

    for (size_t i = 0; i < fileList.size(); ++i)
      openReadEnumCB(&data, dir, fileList[i].c_str());
  } else {
    PHYSFS_enumerate(dir, openReadEnumCB, &data);
  }

  if (data.physfsError)
    handler.exception = Exception(Exception::PHYSFSError, "PhysFS: %s", data.physfsError);
  else if (data.matchCount == 0)
    handler.exception = Exception(Exception::NoFileError, "%s", filename);
}

#ifndef MKXPZ_RETRO
void FileSystem::openReadRaw(SDL_RWops &ops, const char *filename,
                             bool freeOnClose) {

  PHYSFS_File *handle = PHYSFS_openRead(normalize(filename, 0, 0).c_str());

  if (!handle)
    throw Exception(Exception::NoFileError, "%s", filename);

  initReadOps(handle, ops, freeOnClose);

  return;
}
#endif // MKXPZ_RETRO

#ifdef MKXPZ_RETRO
static std::string pop_last_path_element(const char *path) {
  std::string parent(path);
  size_t last_slash_index = parent.find_last_of('/');
  if (last_slash_index == std::string::npos) {
    last_slash_index = 0;
  }
  parent = parent.substr(0, last_slash_index);
  return parent;
}

FileSystem::File::File(FileSystem &fs, const char *read_path, const char *write_path_prefix, bool truncate, bool open_read, unsigned char exists) : read_handle(nullptr), write_handle(nullptr), read_error(PHYSFS_ERR_PERMISSION), write_error(PHYSFS_ERR_PERMISSION) {
  _path = fs.normalize(read_path, false, true);

  if (write_path_prefix != nullptr) {
    size_t prefix_length = strlen(write_path_prefix);

    if (_path.length() >= prefix_length && !strncmp(_path.c_str(), write_path_prefix, prefix_length)) {
      const char *suffix = _path.c_str() + prefix_length;

      if (exists > 1) {
        exists = PHYSFS_exists(read_path) ? 1 : 0;
      }

      // If the path doesn't exist but its parent does,
      // create the parent directory in the PhysFS write directory
      // since it might only exist in PhysFS's read-only search path
      if (!exists) {
        std::string suffix_parent = pop_last_path_element(suffix);
        if (suffix_parent.empty() || suffix_parent.front() != '/') {
          suffix_parent = '/' + suffix_parent;
        }
        if (suffix_parent != "/" && PHYSFS_exists((write_path_prefix + suffix_parent).c_str())) {
          PHYSFS_mkdir(suffix_parent.c_str());
        }
      }

      // If the path exists but not in the PhysFS write directory (mounted at "/Save"),
      // and the file is not opened in truncate mode,
      // copy the file into the PhysFS write directory first
      if (!truncate && exists && PHYSFS_exists((std::string("/Save/") + suffix).c_str()) == 0) {
        if ((read_handle = PHYSFS_openRead(_path.c_str())) != nullptr) {
          if ((write_handle = PHYSFS_openWrite(suffix)) != nullptr) {
            std::array<uint8_t, 4096> buffer;
            for (;;) {
              PHYSFS_sint64 n = PHYSFS_readBytes(read_handle, buffer.data(), buffer.size());
              PHYSFS_writeBytes(write_handle, buffer.data(), n);
              if (n < buffer.size()) {
                break;
              }
            }
          }
          PHYSFS_close(read_handle);
          read_handle = nullptr;
        }
      } else if (truncate) {
        write_handle = PHYSFS_openWrite(suffix);
      } else {
        write_handle = PHYSFS_openAppend(suffix);
      }

      if (write_handle == nullptr) {
        write_error = PHYSFS_getLastErrorCode();
      } else {
        write_error = PHYSFS_ERR_OK;
      }
    }
  }

  if (open_read) {
    if ((read_handle = PHYSFS_openRead(_path.c_str())) == nullptr) {
      read_error = PHYSFS_getLastErrorCode();
    } else {
      read_error = PHYSFS_ERR_OK;
    }
  }
}

FileSystem::File::~File() {
  if (read_handle != nullptr) {
    PHYSFS_close(read_handle);
  }

  if (write_handle != nullptr) {
    PHYSFS_close(write_handle);
  }
}

static std::string normalizePath(const char *path, bool absolute, const char *current_working_directory = nullptr) {
  // Replace backslashes with forward slashes
  std::string path_str(path);
  for (size_t i = 0; i < path_str.length(); ++i) {
    if (path_str[i] == '\\') {
      path_str[i] = '/';
    }
  }

  // If path doesn't start with a forward slash, prepend the current working directory before normalizing
  if (path_str.empty()) {
    path_str = current_working_directory != nullptr ? current_working_directory : mkxp_retro::sandbox.has_value() ? (const char *)mkxp_retro::sandbox->getcwd() : "/Game";
  } else if (path_str.front() != '/') {
    path_str = std::string(current_working_directory != nullptr ? current_working_directory : mkxp_retro::sandbox.has_value() ? (const char *)mkxp_retro::sandbox->getcwd() : "/Game") + '/' + path_str;
  }

  // Lexically normalize the path
  std::list<std::string> list;
  std::string component;
  std::istringstream stream(path_str);
  while (std::getline(stream, component, '/')) {
    list.push_front(component);
  }
  for (auto it = list.begin(); it != list.end();) {
    if (it->empty() || *it == ".") {
      list.erase(it++);
    } else {
      ++it;
    }
  }
  for (auto it = list.begin(); it != list.end();) {
    if (*it == "..") {
      while (std::next(it) != list.end() && *std::next(it) == "..") {
        ++it;
      }
      while (*it == "..") {
        if (std::next(it) != list.end()) {
          list.erase(std::next(it));
        }
        if (it == list.begin()) {
          list.erase(it);
          it = list.begin();
          break;
        } else {
          list.erase(it--);
        }
      }
    } else {
      ++it;
    }
  }

  // Convert the normalized path back into a string
  list.reverse();
  std::string normalized_path;
  if (absolute) {
    normalized_path.push_back('/');
  }
  for (auto it = list.begin(); it != list.end();) {
    normalized_path.append(*it);
    if (std::next(it) != list.end()) {
      normalized_path.push_back('/');
    }
    list.erase(it++);
  }

  return normalized_path;
}
#endif // MKXPZ_RETRO

std::string FileSystem::normalize(const char *pathname, bool preferred,
                            bool absolute, const char *current_working_directory) {
#ifdef MKXPZ_RETRO
  return normalizePath(pathname, absolute, current_working_directory);
#else
  return filesystemImpl::normalizePath(pathname, preferred, absolute);
#endif // MKXPZ_RETRO
}

bool FileSystem::exists(const char *filename) {
  return PHYSFS_exists(normalize(filename, false, false).c_str());
}

const char *FileSystem::desensitize(const char *filename) {
  std::string fn_lower(filename);
    
  std::transform(fn_lower.begin(), fn_lower.end(), fn_lower.begin(), [](unsigned char c){
      return std::tolower(c);
  });
  if (p->havePathCache && p->pathCache.contains(fn_lower))
    return p->pathCache[fn_lower].c_str();
  return filename;
}

bool FileSystem::enumerate(const char *path, PHYSFS_EnumerateCallback callback, void *data) {
  return PHYSFS_enumerate(normalize(path, false, false).c_str(), callback, data) != 0;
}
