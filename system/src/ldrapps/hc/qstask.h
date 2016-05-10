//
// QSINIT API
// multitasking support
// ----------------------------------------------------
//
// !WARNING! this API is unfinished now and a huge parts of QSINIT
//           code is thread-unsafe.
//
#ifndef QSINIT_TASKF
#define QSINIT_TASKF

#include "qstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef u32t    mt_pid;   ///< process id
typedef u32t    mt_tid;   ///< thread id
typedef void*   mt_fib;   ///< fiber id

/** Start multi-tasking.
    Function is simulate to qs_mtlib->start() call. See qcl\qsmt.h.
    @retval E_OK            on success
    @retval E_MT_DISABLED   library disabled by "set MTLIB = off"
    @retval E_SYS_NOFILE    unable to load MTLIB module
    @retval E_MT_TIMER      unable to install timer handlers
    @retval E_MT_OLDCPU     no local APIC on CPU (P5 and below)
    @retval E_SYS_NOMEM     memory allocation error
    @retval E_SYS_DONE      mode already on */
u32t   _std mt_initialize(void);

/** is multi-tasking active?
    @return boolean flag (1/0) */
u32t   _std mt_active(void);

/** yields CPU.
    This call not depends on MTLIB library presence and can be used everywere.
    It just will be ignored in single-task mode. */
void        mt_yield(void);

/// thread function type
typedef u32t _std (*mt_threadfunc)(void *arg);

/// thread enter/exit callback type
typedef u32t _std (*mt_threadcb  )(mt_tid tid);

/** thread creation data.
    Note, that thread termination hook called in thread context, but with
    custom 4k stack. This hook can be used to deny thread termination, as
    advance. It can switch back to fiber 0 and then kill this fiber. As a
    result - thread will continue execution, but it stops mt_termthread()
    caller until this thread`s real exit. */
typedef struct {
   u32t             size;    ///< structure size
   void           *stack;    ///< pre-allocated stack pointer
   u32t        stacksize;    ///< stack size (default is 64k)
   mt_threadcb   onenter;    ///< thread start hook
   mt_threadcb    onexit;    ///< thread exit hook
   mt_threadcb    onterm;    ///< thread termination hook
   u32t              pid;    ///< pid of parent process (use 0 to current)
} mt_ctdata;

/* create thread.
   @param  thread    Thread function
   @param  flags     Flags
   @param  optdata   Optional settings, can be 0. Note that all fields in this
                     struct must be valid (i.e. zeroed or filled with actual
                     value). Note, what for pre-allocated stack its size also
                     MUST be specified.
   @param  arg       thread argument
   @return thread id or 0 on error */
mt_tid _std mt_createthread(mt_threadfunc thread, u32t flags, mt_ctdata *optdata, void *arg);

/// @name mt_createthread() flags
//@{
#define MTCT_SUSPENDED    0x0001       ///< create thread in suspended state
//@}

/* terminate thread.
   Thread can terminate itself (in this case onterm hook will be called,
   onexit hook called by mt_exitthread() only).

   @param  tid           Thread ID
   @retval E_MT_BADTID   Bad thread ID
   @retval E_MT_ACCESS   TID 1 or system thread cannot be terminated
   @retval E_MT_BUSY     Thread in waiting state
   @retval E_MT_GONE     Thread terminated already
   @retval E_OK          Thread terminated */
u32t   _std mt_termthread  (mt_tid tid, u32t exitcode);

void   _std mt_exitthread  (u32t exitcode);

/** get current thread`s id.
    @return current thread id (1..x) */
u32t   _std mt_getthread   (void);

/** get current fiber id.
    @return current fiber index (0..x) in thread */
u32t   _std mt_getfiber    (void);

/** set executing fiber in thread.
   @param  tid           Thread ID (use 0 for current thread).
   @param  fiber         Fiber index (0..x)
   @retval E_OK          Success.
   @retval E_SYS_DONE    This fiber is active now
   @retval E_MT_BADTID   Bad thread ID
   @retval E_MT_BADFIB   Bad fiber index
   @retval E_MT_BUSY     Thread in waiting state
   @retval E_MT_GONE     Thread terminated already */
u32t   _std mt_setfiber    (mt_tid tid, u32t fiber);

/** dump process/thread list to log (in MT look).
    This function replaces mod_dumptree() call (Ctrl-F5) after
    success mt_initialize(). */
void   _std mt_dumptree    (void);

/// allocate thread local storage slot.
u32t   _std mt_tlsalloc    (void);

/** free thread local storage slot.
   @param  index         TLS slot index
   @retval 0             on success
   @retval E_MT_TLSINDEX Invalid index value
   @retval E_SYS_ACCESS  Preallocated TLS slot cannot be released */
qserr  _std mt_tlsfree     (u32t index);

/** get TLS slot value.
   There is no way to query error in this function, it just returns 0
   for any unknown slot index. */
u64t   _std mt_tlsget      (u32t index);

/** query TLS slot address.
   @attention Note, what this address is NOT equal in different threads!
   @param  index         TLS slot index
   @param  slotaddr      Ptr to address value. Returning address is constant
                         while thread is active. Two qwords can be stored at
                         this address.
   @return error value or 0 on success */
qserr  _std mt_tlsaddr     (u32t index, u64t **slotaddr);

/** set TLS slot value.
   Actually TLS slot has space for two qwords, but second accessible via
   mt_tlsaddr() only.
   @param  index         TLS slot index
   @param  value         Value to set
   @retval 0             on success
   @retval E_MT_TLSINDEX Invalid index value
   @retval E_MT_DISABLED MT lib is not initialized */
qserr  _std mt_tlsset      (u32t index, u64t value);

/// @name pre-allocated tls slot index values
//@{
#define QTLS_ERRNO        0x0000       ///< runtime errno value for this thread
#define QTLS_ASCTMBUF     0x0001       ///< asctime() buffer address
#define QTLS_MAXSYS       0x0001       ///< maximum pre-allocated slot index
//@}
#define TLS_VARIABLE_SIZE     16       ///< size of TLS slot (two qwords)

#ifdef __WATCOMC__
#pragma aux mt_yield "_*" modify exact [];
#endif

#ifdef __cplusplus
}
#endif
#endif // QSINIT_TASKF
