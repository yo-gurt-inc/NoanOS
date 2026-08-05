/*
 * libnoan_cxx_lfs_stubs.c — LFS (large-file) aliases for glibc-built
 * libstdc++.a linking against musl libc on NoanOS (i686, static).
 *
 * glibc exposes fopen64, fseeko64, ftello64, lseek64, fstat64 as aliases
 * of their standard counterparts.  musl implements everything as 64-bit-
 * clean but does not expose the *64 names; we provide them here.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

FILE *fopen64(const char *path, const char *mode) { return fopen(path, mode); }
int   fseeko64(FILE *f, off_t off, int whence)    { return fseeko(f, off, whence); }
off_t ftello64(FILE *f)                            { return ftello(f); }
off_t lseek64(int fd, off_t off, int whence)       { return lseek(fd, off, whence); }
int   fstat64(int fd, struct stat *st)             { return fstat(fd, st); }
