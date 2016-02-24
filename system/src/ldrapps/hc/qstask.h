//
// QSINIT API
// multitasking support
//
#ifndef QSINIT_TASKF
#define QSINIT_TASKF

#include "qstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef u32t    mt_tid;   ///< process id
typedef u32t    mt_pid;   ///< thread id
typedef void*   mt_fib;   ///< fiber id

/** Start multi-tasking.
    Function is simulate to qs_mtlib->start() call. See qcl\qsmt.h.
    @retval 0        on success
    @retval EINTR    library disabled by "set MTLIB = off"
    @retval ELIBACC  unable to load MTLIB module
    @retval EBUSY    unable to install timer handlers
    @retval ENODEV   no local APIC on CPU (P5 and below)
    @retval ENOMEM   memory allocation error
    @retval EEXIST   mode already on */
u32t   _std mt_initialize(void);

/** is multi-tasking active?
    @return boolean flag (1/0) */
u32t   _std mt_active(void);

/** yields CPU.
    This call not depends on MTLIB library presence and can be used everywere.
    It just will be ignored in single-task mode. */
void   _std mt_yield(void);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_TASKF
