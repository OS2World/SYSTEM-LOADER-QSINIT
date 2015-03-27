//
// QSINIT API
// 32bit wchar emu
//
#ifndef QSINIT_WCHAR
#define QSINIT_WCHAR

#ifdef __cplusplus
extern "C" {
#endif
#include "qstypes.h"

/* Watcom C++ 1.9 (only C++, not C) have embedded wchar_t check and shows
   error on typedef, so use define here */
#ifdef wchar_t
#undef wchar_t
#endif
#define wchar_t u16t

u32t     _std wcslen(const wchar_t *str);

wchar_t *_std wcscpy(wchar_t *dst, const wchar_t *src);

wchar_t *_std wcsncpy(wchar_t *dst, const wchar_t *src, u32t n);

wchar_t  _std towlower(wchar_t chr);

wchar_t  _std towupper(wchar_t chr);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_WCHAR
