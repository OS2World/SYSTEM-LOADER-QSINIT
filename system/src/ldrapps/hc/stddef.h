//
// QSINIT API
// stddef header
//
#ifndef QSINIT_STDDEF
#define QSINIT_STDDEF

#ifdef __cplusplus
extern "C" {
#endif

#include "qstypes.h"

typedef unsigned  size_t;
typedef signed   ssize_t;
typedef long       off_t;

#define offsetof(type,field) FIELDOFFSET(type,field)
// printf compatible function type
typedef int __cdecl (*printf_function)(const char *fmt, ...);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_STDDEF
