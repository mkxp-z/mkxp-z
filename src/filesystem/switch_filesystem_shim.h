#ifndef SWITCH_FILESYSTEM_SHIM_H
#define SWITCH_FILESYSTEM_SHIM_H
#ifdef __SWITCH__
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
/* ghc::filesystem has no Switch target; its Linux code works on newlib */
#define GHC_OS_LINUX
#define GHC_OS_DETECTED

#ifndef AT_FDCWD
#define AT_FDCWD (-100)
#endif
#ifndef AT_SYMLINK_NOFOLLOW
#define AT_SYMLINK_NOFOLLOW 0x100
#endif

/* Stub unsupported POSIX functions. newlib declares some of these in
 * gnu++ mode, so the stubs get their own names and these macros send calls
 * to them. */
#define symlink mkxpz_switch_symlink
#define readlink mkxpz_switch_readlink
#define utimensat mkxpz_switch_utimensat
#define truncate mkxpz_switch_truncate

static inline int symlink(const char *target, const char *linkpath) {
    (void)target; (void)linkpath;
    errno = ENOSYS;
    return -1;
}

static inline ssize_t readlink(const char *pathname, char *buf, size_t bufsiz) {
    (void)pathname; (void)buf; (void)bufsiz;
    errno = EINVAL;
    return -1;
}

static inline int utimensat(int dirfd, const char *pathname, 
                            const struct timespec times[2], int flags) {
    (void)dirfd; (void)pathname; (void)times; (void)flags;
    return 0;
}

static inline int truncate(const char *path, off_t length) {
    (void)path; (void)length;
    errno = ENOSYS;
    return -1;
}

#endif /* __SWITCH__ */
#endif /* SWITCH_FILESYSTEM_SHIM_H */
