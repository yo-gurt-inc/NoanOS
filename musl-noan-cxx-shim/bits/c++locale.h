// NoanOS/musl shim: minimal c++locale.h for musl builds
// The system file typedefs __locale_t (a glibc type) as __c_locale.
// musl calls it locale_t. We provide the same interface without glibc symbols.

#ifndef _GLIBCXX_CXX_LOCALE_H
#define _GLIBCXX_CXX_LOCALE_H 1

#pragma GCC system_header

#include <clocale>

#define _GLIBCXX_C_LOCALE_GNU 0
#define _GLIBCXX_NUM_CATEGORIES 0

namespace std _GLIBCXX_VISIBILITY(default)
{
_GLIBCXX_BEGIN_NAMESPACE_VERSION

  // musl uses locale_t; glibc calls it __locale_t. Map to int for bare-metal.
  typedef int __c_locale;

  inline int
  __convert_from_v(const __c_locale& /*__cloc*/,
                   char* __out,
                   const int __size,
                   const char* __fmt, ...)
  {
    __builtin_va_list __args;
    __builtin_va_start(__args, __fmt);
    int __r = __builtin_vsnprintf(__out, static_cast<__SIZE_TYPE__>(__size),
                                  __fmt, __args);
    __builtin_va_end(__args);
    return __r;
  }

_GLIBCXX_END_NAMESPACE_VERSION
} // namespace std

#endif // _GLIBCXX_CXX_LOCALE_H
