// NoanOS/musl shim: replaces i686-linux-gnu os_defines.h
// The system file uses __GLIBC_PREREQ() which is not defined under musl.
// We stub it to 0 so all glibc-specific code paths in libstdc++ are disabled.

#ifndef _GLIBCXX_OS_DEFINES
#define _GLIBCXX_OS_DEFINES 1

#ifndef __GLIBC_PREREQ
# define __GLIBC_PREREQ(maj, min) 0
#endif

#endif // _GLIBCXX_OS_DEFINES
