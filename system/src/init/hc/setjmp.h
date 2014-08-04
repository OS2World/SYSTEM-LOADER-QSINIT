//
// QSINIT
// 32bit clib functions
//
#ifndef QSINIT_CLIB_SETJMP
#define QSINIT_CLIB_SETJMP

#include "qstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { u32t reserved[16]; } jmp_buf;

/** setjmp function.
    there is no FPU/MMX support! */
int   __stdcall _setjmp  (jmp_buf *env);

void  __stdcall _longjmp (jmp_buf *env, int return_value);

#define setjmp(x)     (_setjmp(&(x)))
#define longjmp(x,y)  _longjmp(&(x),y)

#ifdef __cplusplus
}
#endif

#endif // QSINIT_CLIB_SETJMP
