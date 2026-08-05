/*
 * libnoan_cxx_stubs.c — thin stubs/aliases so a glibc-built libstdc++.a
 * links cleanly against musl libc on NoanOS (i686, static).
 *
 * Categories:
 *   1. LFS (large-file) 64-bit aliases: glibc exposes fopen64 etc. as weak
 *      aliases for the standard functions; musl just uses one set.
 *   2. Glibc internals referenced by libstdc++:
 *      __libc_single_threaded, __dso_handle, __isoc23_strtoul,
 *      _dl_find_object, arc4random
 *   3. pthread stubs (no real threads on NoanOS).
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* -------------------------------------------------------------------------
 * 1.  LFS aliases
 *     musl implements everything as 64-bit-clean already; the *64 names
 *     are simply additional entry points for the same functions.
 * ---------------------------------------------------------------------- */

FILE *fopen64(const char *path, const char *mode) { return fopen(path, mode); }

int fseeko64(FILE *f, off_t off, int whence) { return fseeko(f, off, whence); }

off_t ftello64(FILE *f) { return ftello(f); }

off_t lseek64(int fd, off_t off, int whence) { return lseek(fd, off, whence); }

int fstat64(int fd, struct stat *st) { return fstat(fd, st); }

/* -------------------------------------------------------------------------
 * 2.  Glibc internals
 * ---------------------------------------------------------------------- */

/* libstdc++ checks this to skip locks in single-threaded mode.
 * We always run single-threaded, so set it to 1. */
char __libc_single_threaded = 1;

/* Used as a unique sentinel by cxa_atexit / dl_iterate_phdr machinery.
 * A single static object is sufficient for a static binary. */
void *__dso_handle = &__dso_handle;

/* C23 strtoul — musl's strtoul already handles the same semantics. */
unsigned long __isoc23_strtoul(const char *s, char **endptr, int base)
{
    return strtoul(s, endptr, base);
}

/* Dynamic linker object lookup — not available in a static binary. */
int _dl_find_object(void *addr, void *result)
{
    (void)addr; (void)result;
    return -1; /* ENOENT — no dlopen in a static binary */
}

/* arc4random — libstdc++ uses this for hash seed randomisation.
 * A simple LCG is good enough; we don't need cryptographic quality. */
static uint32_t _lcg_state = 0xdeadbeefUL;

uint32_t arc4random(void)
{
    /* Xorshift32 — simple, statistically decent, no overflow warnings */
    _lcg_state ^= _lcg_state << 13;
    _lcg_state ^= _lcg_state >> 17;
    _lcg_state ^= _lcg_state << 5;
    return _lcg_state;
}

/* -------------------------------------------------------------------------
 * 3.  pthread stubs — NoanOS has no kernel thread support.
 *     All operations are no-ops or return success so that libstdc++'s
 *     internal locking machinery compiles and links without crashing
 *     at runtime (the __libc_single_threaded flag makes libstdc++ skip
 *     most of the locking paths anyway).
 * ---------------------------------------------------------------------- */

/* pthread_cond_t / pthread_mutex_t are treated as opaque ints here. */
typedef int pthread_cond_t_stub;
typedef int pthread_mutex_t_stub;
typedef void *pthread_condattr_t_stub;

int pthread_cond_broadcast(pthread_cond_t_stub *c) { (void)c; return 0; }
int pthread_cond_wait(pthread_cond_t_stub *c, pthread_mutex_t_stub *m)
{
    (void)c; (void)m; return 0;
}
int pthread_once(int *once, void (*init)(void))
{
    if (once && !*once) { *once = 1; init(); }
    return 0;
}
