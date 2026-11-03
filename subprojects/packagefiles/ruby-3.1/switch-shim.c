/* Nintendo Switch (libnx): POSIX functions Ruby links against that newlib
 * and libnx don't provide. Processes, users and dynamic loading don't exist
 * there, so most of these fail. See also switch_shim.h from
 * ruby-3.1-switch.patch. */

#include <errno.h>
#include <pthread.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <switch/kernel/thread.h>

uid_t getuid(void) { return 0; }
uid_t geteuid(void) { return 0; }
gid_t getgid(void) { return 0; }
gid_t getegid(void) { return 0; }

int pipe(int pipefd[2]) { (void)pipefd; return -1; }

int pthread_kill(pthread_t thread, int sig) { (void)thread; (void)sig; return 0; }

/* Bounds of the calling thread's stack, for get_stack() in thread_pthread.c */
int rb_switch_get_stack(void **addr, size_t *size) {
    Thread *t = threadGetSelf();
    if (!t || !t->stack_mirror) return -1;
    *addr = t->stack_mirror; /* libnx threads run on the mirror mapping */
    *size = t->stack_sz;
    return 0;
}

void *memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen) {
    if (!haystack || !needle || needlelen > haystacklen) return NULL;
    if (needlelen == 0) return (void *)haystack;
    const char *h = (const char *)haystack;
    for (size_t i = 0; i <= haystacklen - needlelen; i++) {
        if (memcmp(h + i, needle, needlelen) == 0) return (void *)(h + i);
    }
    return NULL;
}

mode_t umask(mode_t mask) { (void)mask; return 0; }

int execv(const char *path, char *const argv[]) { (void)path; (void)argv; return -1; }
int execl(const char *path, const char *arg, ...) { (void)path; (void)arg; return -1; }
int execle(const char *path, const char *arg, ...) { (void)path; (void)arg; return -1; }
pid_t waitpid(pid_t pid, int *status, int options) { (void)pid; (void)status; (void)options; return -1; }
pid_t getppid(void) { return 0; }

struct passwd *getpwnam(const char *name) { (void)name; return NULL; }
struct passwd *getpwuid(uid_t uid) { (void)uid; return NULL; }
void endpwent(void) {}

FILE *popen(const char *command, const char *type) { (void)command; (void)type; return NULL; }
int pclose(FILE *stream) { (void)stream; return -1; }

int chown(const char *pathname, uid_t owner, gid_t group) { (void)pathname; (void)owner; (void)group; return 0; }

/* Ruby sizes fiber stacks from _SC_PAGESIZE in Init_Cont; returning -1 there
 * overflows the fiber pool size, raises FiberError, and ruby_init() exits. */
long sysconf(int name) {
    switch (name) {
      case _SC_PAGESIZE: return 0x1000; /* Horizon uses 4 KiB pages */
      default: errno = EINVAL; return -1;
    }
}

int getrusage(int who, struct rusage *usage) { (void)who; (void)usage; return -1; }

/* malloc-backed stand-ins, for code that doesn't see switch_shim.h's inline
 * versions */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    (void)addr; (void)prot; (void)flags; (void)fd; (void)offset;
    return malloc(length);
}
int munmap(void *addr, size_t length) { (void)length; free(addr); return 0; }
int mprotect(void *addr, size_t len, int prot) { (void)addr; (void)len; (void)prot; return 0; }

void *dlopen(const char *filename, int flag) { (void)filename; (void)flag; return NULL; }
int dlclose(void *handle) { (void)handle; return 0; }
void *dlsym(void *handle, const char *symbol) { (void)handle; (void)symbol; return NULL; }
char *dlerror(void) { return NULL; }
