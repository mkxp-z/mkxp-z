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
#include <functional>
#include <stack>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <vector>

#ifdef __APPLE__
#include <iconv.h>
#endif

#ifdef __APPLE__
static void toNFC(char *inout, iconv_t nfd2nfc, char *buf, size_t bufBytes) {
  if (nfd2nfc == (iconv_t)-1 || bufBytes == 0)
    return;

  size_t srcSize = strlen(inout);
  size_t bufSize = bufBytes - 1;
  char *bufPtr = buf;
  char *inoutPtr = inout;

  iconv(nfd2nfc, NULL, NULL, NULL, NULL);
  if (iconv(nfd2nfc, &inoutPtr, &srcSize, &bufPtr, &bufSize) == (size_t)-1)
    return;

  *bufPtr = 0;
  strcpy(inout, buf);
}
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

  for (len = strlen(filename); len > 0; --len) {
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

static void strTolower(std::string &str) {
  for (size_t i = 0; i < str.size(); ++i)
    str[i] = tolower(static_cast<unsigned char>(str[i]));
}

const Uint32 SDL_RWOPS_PHYSFS = SDL_RWOPS_UNKNOWN + 10;

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

void FileSystem::addPath(const char *path, const char *mountpoint, bool reload) {
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
    
    if (reload) reloadPathCache();
}

void FileSystem::removePath(const char *path, bool reload) {
    
    if (!PHYSFS_unmount(path)) {
        PHYSFS_ErrorCode err = PHYSFS_getLastErrorCode();
        throw Exception(Exception::PHYSFSError, "Failed to unmount %s (%s)", path, PHYSFS_getErrorByCode(err));
    }
    
    if (reload) reloadPathCache();
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
    ::toNFC(inout, nfd2nfc, buf, sizeof(buf));
#else
    (void)inout;
#endif
  }
};

static PHYSFS_EnumerateCallbackResult cacheEnumCB(void *d, const char *origdir,
                                                  const char *fname) {
  if (shState && shState->rtData().rqTerm)
    throw Exception(Exception::MKXPError, "Game close requested. Aborting path cache enumeration.");

  CacheEnumData &data = *static_cast<CacheEnumData *>(d);
  char fullPath[512];
  char originalPath[512];

  if (!*origdir)
    snprintf(fullPath, sizeof(fullPath), "%s", fname);
  else
    snprintf(fullPath, sizeof(fullPath), "%s/%s", origdir, fname);

  strcpySafe(originalPath, fullPath, sizeof(originalPath), -1);

  /* Deal with OSX' weird UTF-8 standards */
  data.toNFC(fullPath);

  std::string mixedCase(fullPath);
  std::string lowerCase = mixedCase;
  strTolower(lowerCase);

  PHYSFS_Stat stat;
  PHYSFS_stat(originalPath, &stat);

  if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
    /* Create a new list for this directory */
    std::vector<std::string> &list = data.p->fileLists[lowerCase];

    /* Iterate over its contents */
    data.fileLists.push(&list);
    PHYSFS_enumerate(originalPath, cacheEnumCB, d);
    data.fileLists.pop();
  } else {
    /* Get the file list for the directory we're currently
     * traversing and append this filename to it */
    std::vector<std::string> &list = *data.fileLists.top();

    char filename[512];
    strcpySafe(filename, fname, sizeof(filename), -1);
    data.toNFC(filename);

    std::string lowerFilename(filename);
    strTolower(lowerFilename);
    list.push_back(lowerFilename);

    /* Add the lower -> mixed mapping of the file's full path */
    data.p->pathCache.insert(lowerCase, originalPath);
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

/* Rebuilds the cached listing of a single directory from a live
 * PhysFS enumeration. Used by openRead's stale-cache fallback: games
 * that write new files at runtime (e.g. Pokemon fangames downloading
 * battler spritesheets on first encounter) create entries the
 * boot-time path cache has never seen, so a cached lookup misses even
 * though the file exists. Non-recursive on purpose: a fileLists entry
 * only ever holds the files of its own directory. */
struct DirRefreshEnumData {
  FileSystemPrivate *p;
  std::vector<std::string> *list;
  /* Path-cache key of the directory being refreshed (NFC, lowercase).
   * Keys are rebuilt from this rather than from the on-disk dirpath so
   * they land exactly where openReadEnumCB's translation looks. */
  std::string lowerDir;

#ifdef __APPLE__
  iconv_t nfd2nfc;
  char buf[512];
#endif

  DirRefreshEnumData(FileSystemPrivate *p, std::vector<std::string> *list,
                     const char *lowerDir)
      : p(p), list(list), lowerDir(lowerDir) {
#ifdef __APPLE__
    nfd2nfc = iconv_open("utf-8", "utf-8-mac");
#endif
  }

  ~DirRefreshEnumData() {
#ifdef __APPLE__
    iconv_close(nfd2nfc);
#endif
  }

  /* Converts in-place */
  void toNFC(char *inout) {
#ifdef __APPLE__
    ::toNFC(inout, nfd2nfc, buf, sizeof(buf));
#else
    (void)inout;
#endif
  }
};

static PHYSFS_EnumerateCallbackResult
dirRefreshEnumCB(void *d, const char *origdir, const char *fname) {
  DirRefreshEnumData &data = *static_cast<DirRefreshEnumData *>(d);
  char originalPath[512];

  if (!*origdir)
    snprintf(originalPath, sizeof(originalPath), "%s", fname);
  else
    snprintf(originalPath, sizeof(originalPath), "%s/%s", origdir, fname);

  PHYSFS_Stat stat;
  PHYSFS_stat(originalPath, &stat);

  if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY)
    return PHYSFS_ENUM_OK;

  char filename[512];
  strcpySafe(filename, fname, sizeof(filename), -1);
  data.toNFC(filename);

  std::string lowerFilename(filename);
  strTolower(lowerFilename);

  std::string lowerFull = data.lowerDir.empty()
      ? lowerFilename
      : data.lowerDir + "/" + lowerFilename;

  data.list->push_back(lowerFilename);
  data.p->pathCache.insert(lowerFull, originalPath);

  return PHYSFS_ENUM_OK;
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

  char filename[512];
  snprintf(filename, sizeof(filename), "%s/%s", dir, fname);

  PHYSFS_File *handle = PHYSFS_openRead(filename);

  if (!handle)
    return PHYSFS_ENUM_ERROR;

  SDL_RWops ops;
  initReadOps(handle, ops, false);

  d->sfs->initFontSetCB(ops, filename);

  SDL_RWclose(&ops);

  return PHYSFS_ENUM_OK;
}

/* Basically just a case-insensitive search
 * for the folder "Fonts"... */
static PHYSFS_EnumerateCallbackResult
findFontsFolderCB(void *data, const char *, const char *fname) {
  size_t i = 0;
  char buffer[512];
  const char *s = fname;

  while (*s && i < sizeof(buffer))
    buffer[i++] = tolower(*s++);

  buffer[i] = '\0';

  if (strcmp(buffer, "fonts") == 0)
    PHYSFS_enumerate(fname, fontSetEnumCB, data);

  return PHYSFS_ENUM_OK;
}

void FileSystem::initFontSets(SharedFontState &sfs) {
  FontSetsCBData d = {p, &sfs};

  PHYSFS_enumerate("", findFontsFolderCB, &d);
}

struct OpenReadEnumData {
  FileSystem::OpenHandler &handler;
  SDL_RWops ops;

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
  if (strncasecmp(filename, data.filename, data.filenameN) != 0)
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

  /* Without the path cache, the listing has folders too. PhysFS opens a
   * folder on disk as a file, and the image loader then fails it with
   * SDLError, which a game's `rescue` does not catch. Pokemon Insurgence
   * loads "Graphics/Characters/" for an event with no sprite. */
  PHYSFS_Stat stat;
  if (PHYSFS_stat(fullPath, &stat) &&
      stat.filetype == PHYSFS_FILETYPE_DIRECTORY)
    return PHYSFS_ENUM_OK;

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

  const char *ext = findExt(filename);

  if (data.handler.tryRead(data.ops, ext))
    data.stopSearching = true;

  ++data.matchCount;
  return PHYSFS_ENUM_OK;
}

/* Windows lists a folder in name order, so a game that asks for "x"
 * always got "x.png" before "x.xcf". readdir order on iOS is arbitrary,
 * and SDL_image also decodes XCF (as an empty image, for Pokemon Silver
 * Chronicle's tileset), so try the candidates in name order too. */
static void openReadInOrder(OpenReadEnumData &data, const char *dir,
                            const std::vector<std::string> &fileList) {
  std::vector<const char *> names;
  for (size_t i = 0; i < fileList.size(); ++i)
    if (strncasecmp(fileList[i].c_str(), data.filename, data.filenameN) == 0)
      names.push_back(fileList[i].c_str());

  /* Without the path cache, "Twin.png" and "twin.png" can both match.
   * The name in the case the game asked for goes first. */
  const char *want = data.filename;
  const size_t wantN = data.filenameN;
  std::stable_sort(names.begin(), names.end(),
                   [want, wantN](const char *a, const char *b) {
                     int c = strcasecmp(a, b);
                     if (c != 0)
                       return c < 0;
                     bool aExact = strncmp(a, want, wantN) == 0;
                     bool bExact = strncmp(b, want, wantN) == 0;
                     if (aExact != bExact)
                       return aExact;
                     return strcmp(a, b) < 0;
                   });

  for (size_t i = 0; i < names.size() && !data.stopSearching; ++i)
    openReadEnumCB(&data, dir, names[i]);
}

static PHYSFS_EnumerateCallbackResult
listEnumCB(void *d, const char *, const char *filename) {
  static_cast<std::vector<std::string> *>(d)->push_back(filename);
  return PHYSFS_ENUM_OK;
}

/* PhysFS finds archive entries, and files on an iOS device, only by
 * exact case. Windows games often ask for another case: Pokemon
 * Insurgence's Game.rgssad holds "Data/map150.rxdata" and the game
 * loads "Data/Map150.rxdata". Without the path cache, match each part
 * of the path against a live listing of its folder. "Data" and "data"
 * can both exist, so each match is tried, the exact case first.
 * `visit` gets each full path that exists and stops the walk when it
 * returns true. */
static bool liveCaseWalk(const std::string &path, size_t start,
                         const std::string &prefix,
                         const std::function<bool(const std::string &)> &visit) {
  size_t slash = path.find('/', start);
  std::string part = path.substr(
      start, slash == std::string::npos ? std::string::npos : slash - start);

  std::vector<std::string> names;
  PHYSFS_enumerate(prefix.c_str(), listEnumCB, &names);

  std::vector<const std::string *> matches;
  for (size_t i = 0; i < names.size(); ++i)
    if (names[i] == part)
      matches.push_back(&names[i]);
  for (size_t i = 0; i < names.size(); ++i)
    if (names[i] != part && strcasecmp(names[i].c_str(), part.c_str()) == 0)
      matches.push_back(&names[i]);

  for (size_t i = 0; i < matches.size(); ++i) {
    std::string full = prefix.empty() ? *matches[i] : prefix + '/' + *matches[i];
    if (slash == std::string::npos ? visit(full)
                                   : liveCaseWalk(path, slash + 1, full, visit))
      return true;
  }
  return false;
}

void FileSystem::openRead(OpenHandler &handler, const char *filename) {
  std::string filename_nm = normalize(filename, false, false);
  char buffer[512];
  size_t len = strcpySafe(buffer, filename_nm.c_str(), sizeof(buffer), -1);
  char *delim;

  if (p->havePathCache) {
#ifdef __APPLE__
    iconv_t nfd2nfc = iconv_open("utf-8", "utf-8-mac");
    char nfcBuf[512];
    toNFC(buffer, nfd2nfc, nfcBuf, sizeof(nfcBuf));
    if (nfd2nfc != (iconv_t)-1)
      iconv_close(nfd2nfc);
    len = strlen(buffer);
#endif
    for (size_t i = 0; i < len; ++i)
      buffer[i] = tolower(static_cast<unsigned char>(buffer[i]));
  }

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
    openReadInOrder(data, dir, p->fileLists[dir]);
  } else {
    std::vector<std::string> fileList;
    PHYSFS_enumerate(dir, listEnumCB, &fileList);
    openReadInOrder(data, dir, fileList);

    if (*dir && !data.stopSearching)
      liveCaseWalk(dir, 0, "", [&](const std::string &liveDir) {
        if (liveDir == dir)
          return false;
        std::vector<std::string> liveList;
        PHYSFS_enumerate(liveDir.c_str(), listEnumCB, &liveList);
        openReadInOrder(data, liveDir.c_str(), liveList);
        return data.stopSearching;
      });
  }

  if (data.physfsError)
    throw Exception(Exception::PHYSFSError, "PhysFS: %s", data.physfsError);

  /* Stale-path-cache fallback: the cache is built once at boot, so
   * files written afterwards (runtime-downloaded sprites etc.) miss
   * even though PhysFS can see them. Refresh just this directory's
   * cached listing from a live enumeration and retry, so the caller
   * pays one readdir on a miss instead of a full-tree reload. The
   * live enumeration uses the caller's original path case, which is
   * correct for the files-written-at-runtime scenario (the game opens
   * what it just wrote). A cached file with the same name and another
   * extension can match and fail to read, so a miss is any search in
   * which no handler took a file. */
  if (p->havePathCache && !data.stopSearching) {
    char obuffer[512];
    size_t olen = strcpySafe(obuffer, filename_nm.c_str(), sizeof(obuffer), -1);

#ifdef __APPLE__
    iconv_t nfd2nfc = iconv_open("utf-8", "utf-8-mac");
    char nfcBuf[512];
    toNFC(obuffer, nfd2nfc, nfcBuf, sizeof(nfcBuf));
    if (nfd2nfc != (iconv_t)-1)
      iconv_close(nfd2nfc);
    olen = strlen(obuffer);
#endif

    char *odelim;
    for (odelim = obuffer + olen; odelim > obuffer; --odelim)
      if (*odelim == '/')
        break;

    const char *odir = "";
    if (odelim != obuffer) {
      *odelim = '\0';
      odir = obuffer;
    }

    std::vector<std::string> fresh;
    DirRefreshEnumData refresh(p, &fresh, dir);

    if (PHYSFS_enumerate(odir, dirRefreshEnumCB, &refresh)) {
      p->fileLists[dir] = fresh;

      openReadInOrder(data, dir, fresh);

      if (data.physfsError)
        throw Exception(Exception::PHYSFSError, "PhysFS: %s", data.physfsError);
    }
  }

  if (data.matchCount == 0)
    throw Exception(Exception::NoFileError, "%s", filename);
}

void FileSystem::openReadRaw(SDL_RWops &ops, const char *filename,
                             bool freeOnClose) {

  /* Try the exact-case query first. This preserves the previous
   * behaviour for the overwhelming majority of calls (where the
   * caller's filename case matches the archive / filesystem entry
   * exactly) and also preserves disambiguation for the rare case
   * of two archive entries differing only in case: the
   * exact-match handle is what `PHYSFS_openRead` returns directly
   * from the archive's entry hash, so a caller that wrote
   * `Data/Readme.txt` still gets `Data/Readme.txt` even if
   * `Data/README.txt` also exists. */
  std::string normalized = normalize(filename, 0, 0);
  PHYSFS_File *handle = PHYSFS_openRead(normalized.c_str());

  /* If the exact case missed, fall back to a case-insensitive
   * lookup. `desensitize` returns the entry in its stored case, from
   * the path cache or, with the cache off, from live folder listings;
   * otherwise it returns its input unchanged, in
   * which case the second PHYSFS_openRead simply repeats the
   * first miss and we throw the same ENOENT the caller would have
   * seen without this fix. No behavioural regression for games
   * whose files resolve on the exact case.
   *
   * Concrete motivation: Pokemon Insurgence's `Game.rgssad`
   * contains `Data/map003.rxdata` (lowercase m, 8, 10, ...) while
   * the game's Ruby scripts request `Data/Map003.rxdata`. On
   * Windows the call would hit because NTFS is case-insensitive.
   * On iOS / case-sensitive hosts and through PhysFS's exact
   * hashing, the exact match fails and we take this fallback. */
  if (!handle) {
    const char *resolved = desensitize(normalized.c_str());
    if (resolved != normalized.c_str())
      handle = PHYSFS_openRead(resolved);
  }

  if (!handle)
    throw Exception(Exception::NoFileError, "%s", filename);

  initReadOps(handle, ops, freeOnClose);
    return;
}

namespace {

bool isAbsolutePath(const char *path) {
  if (!path || !path[0])
    return false;
  if (path[0] == '/')
    return true;
  return path[1] == ':';
}

} // namespace

namespace filesystemImpl {

std::string collapseRelativePath(const char *path) {
  if (!path || !path[0])
    return std::string();

  std::string normalized;
  normalized.reserve(strlen(path));
  for (const char *p = path; *p; ++p)
    normalized += (*p == '\\') ? '/' : *p;

  std::vector<std::string> stack;
  size_t i = 0;
  const size_t n = normalized.size();
  while (i < n) {
    while (i < n && normalized[i] == '/')
      ++i;
    if (i >= n)
      break;

    size_t j = i;
    while (j < n && normalized[j] != '/')
      ++j;

    const std::string part = normalized.substr(i, j - i);
    i = j;

    if (part == ".")
      continue;

    if (part == "..") {
      if (!stack.empty() && stack.back() != "..")
        stack.pop_back();
      else
        stack.push_back(part);
      continue;
    }

    stack.push_back(part);
  }

  if (stack.empty())
    return ".";

  std::string result = stack.front();
  for (size_t k = 1; k < stack.size(); ++k) {
    result += '/';
    result += stack[k];
  }
  return result;
}

} // namespace filesystemImpl

std::string FileSystem::normalize(const char *pathname, bool preferred,
                            bool absolute) {
  if (!pathname || !pathname[0])
    return std::string();

  if (!absolute && !isAbsolutePath(pathname))
    return filesystemImpl::collapseRelativePath(pathname);

  return filesystemImpl::normalizePath(pathname, preferred, absolute);
}

// PhysFS reads an empty path as the root of the game folder.
bool FileSystem::exists(const char *filename) {
  std::string normalized = normalize(filename, false, false);
  if (normalized.empty())
    return false;

  if (PHYSFS_exists(normalized.c_str()))
    return true;

  const char *resolved = desensitize(normalized.c_str());
  if (resolved != normalized.c_str())
    return PHYSFS_exists(resolved) != 0;

  return false;
}

bool FileSystem::directoryExists(const char *filename) {
  std::string normalized = normalize(filename, false, false);
  if (normalized.empty())
    return false;

  PHYSFS_Stat stat;

  if (PHYSFS_stat(normalized.c_str(), &stat) &&
      stat.filetype == PHYSFS_FILETYPE_DIRECTORY)
    return true;

  const char *resolved = desensitize(normalized.c_str());
  if (resolved != normalized.c_str() && PHYSFS_stat(resolved, &stat) &&
      stat.filetype == PHYSFS_FILETYPE_DIRECTORY)
    return true;

  return false;
}

std::string FileSystem::resolvePath(const char *filename) {
  std::string normalized = normalize(filename, false, false);

  if (PHYSFS_exists(normalized.c_str()))
    return normalized;

  const char *resolved = desensitize(normalized.c_str());
  if (resolved != normalized.c_str() && PHYSFS_exists(resolved))
    return resolved;

  return std::string();
}

std::string FileSystem::resolvePathOrParent(const char *filename) {
  std::string resolved = resolvePath(filename);
  if (!resolved.empty())
    return resolved;

  std::string normalized = normalize(filename, false, false);
  std::string::size_type slash = normalized.find_last_of('/');
  if (slash == std::string::npos)
    return std::string();

  std::string parent = normalized.substr(0, slash);
  if (parent.empty() || parent == ".")
    return std::string();

  std::string resolvedParent = resolvePath(parent.c_str());
  if (resolvedParent.empty())
    return std::string();

  std::string base = normalized.substr(slash + 1);
  if (base.empty())
    return resolvedParent;

  if (!resolvedParent.empty() && resolvedParent.back() != '/')
    resolvedParent += '/';
  resolvedParent += base;
  return resolvedParent;
}

std::string FileSystem::resolveFeaturePath(const char *filename) {
  std::string feature = normalize(filename, false, false);
  if (feature.empty())
    return std::string();

  std::string resolved = resolvePath(feature.c_str());
  if (!resolved.empty())
    return resolved;

  if (findExt(feature.c_str()))
    return std::string();

  static const char *suffixes[] = {".rb", ".so"};
  for (size_t i = 0; i < ARRAY_SIZE(suffixes); ++i) {
    std::string candidate = feature + suffixes[i];
    resolved = resolvePath(candidate.c_str());
    if (!resolved.empty())
      return resolved;
  }

  return std::string();
}

const char *FileSystem::desensitize(const char *filename) {
  std::string fn_lower(filename);
    
  std::transform(fn_lower.begin(), fn_lower.end(), fn_lower.begin(), [](unsigned char c){
      return std::tolower(c);
  });
  if (p->havePathCache) {
    if (p->pathCache.contains(fn_lower))
      return p->pathCache[fn_lower].c_str();
    return filename;
  }

  if (PHYSFS_exists(filename))
    return filename;

  static thread_local std::string live;
  if (liveCaseWalk(filename, 0, "", [](const std::string &found) {
        live = found;
        return true;
      }))
    return live.c_str();
  return filename;
}
