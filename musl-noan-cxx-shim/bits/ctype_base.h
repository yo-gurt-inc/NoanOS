// NoanOS/musl shim: replaces i686-linux-gnu/bits/ctype_base.h
//
// The system file initialises the ctype_base::mask constants using the
// glibc _IS* enum (defined in glibc's <ctype.h>).  musl does not define
// those symbols, so the build fails.
//
// We reproduce the same numeric values that glibc uses on little-endian
// i686 (_ISbit macro with network-byte-order swap):
//
//   _ISbit(b) = (b < 8) ? ((1 << b) << 8) : ((1 << b) >> 8)
//
//   bit 0  _ISupper  = 0x0100
//   bit 1  _ISlower  = 0x0200
//   bit 2  _ISalpha  = 0x0400
//   bit 3  _ISdigit  = 0x0800
//   bit 4  _ISxdigit = 0x1000
//   bit 5  _ISspace  = 0x2000
//   bit 6  _ISprint  = 0x4000
//   bit 8  _ISblank  = 0x0001
//   bit 9  _IScntrl  = 0x0002
//   bit 10 _ISpunct  = 0x0004
//
// These must match what the pre-built libstdc++.a was compiled with so
// that the runtime ctype tables are interpreted correctly.

#ifndef _GLIBCXX_CTYPE_BASE_H
#define _GLIBCXX_CTYPE_BASE_H 1

#pragma GCC system_header

namespace std _GLIBCXX_VISIBILITY(default)
{
_GLIBCXX_BEGIN_NAMESPACE_VERSION

  struct ctype_base
  {
    typedef const int*   __to_type;
    typedef unsigned short mask;

    static const mask upper  = 0x0100;
    static const mask lower  = 0x0200;
    static const mask alpha  = 0x0400;
    static const mask digit  = 0x0800;
    static const mask xdigit = 0x1000;
    static const mask space  = 0x2000;
    static const mask print  = 0x4000;
    static const mask graph  = alpha | digit | 0x0004; // alpha|digit|punct
    static const mask cntrl  = 0x0002;
    static const mask punct  = 0x0004;
    static const mask alnum  = alpha | digit;
    static const mask blank  = 0x0001;
  };

_GLIBCXX_END_NAMESPACE_VERSION
} // namespace std

#endif // _GLIBCXX_CTYPE_BASE_H
