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

#ifdef __cplusplus
}
#endif
#endif // QSINIT_STDDEF
