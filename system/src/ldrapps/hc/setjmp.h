//
// QSINIT
// setjmp header
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
int   __stdcall _setjmp    (jmp_buf *env);

void  __stdcall _longjmp   (jmp_buf *env, int return_value);

/** longjmp call for signal processing callback.
    Function assumes fiber 0 (main fiber in the thread) as a target.
    I.e. function will switch thread back to the fiber 0 and setup registers
    to jmp_buf contents. Caller fiber will be deleted.

    If non-MT mode or if current fiber is already "main" (0) - function
    works as usual _longjmp(). */
void  __stdcall _siglongjmp(jmp_buf *env, int return_value);

#define setjmp(x)       (_setjmp(&(x)))
#define longjmp(x,y)    _longjmp(&(x),y)
#define siglongjmp(x,y) _siglongjmp(&(x),y)

#ifdef __cplusplus
}
#endif

#endif // QSINIT_CLIB_SETJMP
