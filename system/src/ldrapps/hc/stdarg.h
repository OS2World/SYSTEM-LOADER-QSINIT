//
// QSINIT API
// 32bit stdarg emu
//
#ifndef QSINIT_STDARG
#define QSINIT_STDARG

#ifdef __cplusplus
extern "C" {
#endif
#include "qstypes.h"

typedef char *va_list;

#define va_start(ap,pn) (ap=(char*)&pn+Round4(sizeof(pn)))

#ifdef __WATCOMC__
#define va_arg(ap,type) (*(type*)__va_arg_get(&ap,sizeof(type)))
void *__va_arg_get(char **cp, int size);
#pragma aux __va_arg_get = \
    "add     eax,3"        \
    "and     eax,not 3"    \
    "xadd    [ecx],eax"    \
    parm  [ecx][eax]       \
    value [eax];
#else
#define va_arg(ap,type) (ap+=Round4(sizeof(type)),*(type*)(ap-Round4(sizeof(type)))
#endif

#define va_end(ap)      (ap=0)

int __stdcall _vsnprintf(char *buf, u32t count, const char *format, va_list arg);

int __stdcall vprintf(const char *format, va_list arg);

/// vprint to log/serial port
int __stdcall log_vprintf(const char *fmt, va_list arg);

#define vsprintf(buf,fmt,arg) _vsnprintf(buf,4096,fmt,arg)
#define vsnprintf _vsnprintf

#ifdef __cplusplus
}
#endif

#endif // QSINIT_STDARG
