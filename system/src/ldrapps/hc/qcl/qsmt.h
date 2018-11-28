//
// QSINIT API
// multi-tasking support
//
#ifndef QSINIT_MTLIBSHARED
#define QSINIT_MTLIBSHARED

#include "qstypes.h"
#include "qsclass.h"
#include "qstask.h"

#ifdef __cplusplus
extern "C" {
#endif

/** mtlib information shared class.
    This is interface for checking of MTLIB presence/state.
    It used by START module, at least, to prevent static linking of MTLIB.DLL.
    Most of functions here have mt_* pair with the same arguments.

    Usage:
    @code
       qs_mtlib mt = NEW(qs_mtlib);
       if (mt) {
          u32r err = mt->initialize();
          DELETE(mt);
          if (err) printf("mtlib start errno %u\n", err);
       }
    @endcode */
typedef struct qs_mtlib_s {
   /** start multi-tasking.
       Function is simulate to mt_initialize().

       @retval E_OK            on success	
       @retval E_MT_DISABLED   library disabled by "set MTLIB = off"
       @retval E_SYS_NOFILE    unable to load MTLIB module
       @retval E_MT_TIMER      unable to install timer handlers
       @retval E_MT_OLDCPU     no local APIC on CPU (P5 and below)
       @retval E_SYS_NOMEM     memory allocation error
       @retval E_SYS_DONE      mode already on */
   qserr    _exicc (*initialize)  (void);
   /// return current mtlib state (on/off).
   u32t     _exicc (*active)      (void);
   /** get pid of EXEcuting module.
       mod_getmodpid() calls this internally, if MTLIB is active */
   u32t     _exicc (*getmodpid)   (u32t module, u32t *parent);
   /// wait objects
   qserr    _exicc (*waitobject)  (mt_waitentry *list, u32t count, u32t glogic,
                                   u32t *signaled);
   /// create thread
   mt_tid   _exicc (*thcreate)    (mt_threadfunc thread, u32t flags,
                                   mt_ctdata *optdata, void *arg);
   /// suspend thread
   qserr    _exicc (*thsuspend)   (mt_pid pid, mt_tid tid, u32t waitstate_ms);
   /// resume suspended thread
   qserr    _exicc (*thresume)    (u32t pid, u32t tid);
   /// stop (kill) the thread
   qserr    _exicc (*thstop)      (mt_pid pid, mt_tid tid, u32t exitcode);
   /// exit thread
   void     _exicc (*thexit)      (u32t exitcode);
   /// start session
   qserr    _exicc (*execse)      (u32t module, const char *env, const char *args,
                                   u32t flags, u32t vdev, mt_pid *ppid,
                                   const char *title);
   /// create fiber
   u32t     _exicc (*createfiber) (mt_threadfunc fiber, u32t flags,
                                   mt_cfdata *optdata, void *arg);
   /// switch to another fiber in the current thread
   qserr    _exicc (*switchfiber) (u32t fiber, int free);
   /// send signal
   qserr    _exicc (*sendsignal)  (u32t pid, u32t tid, u32t signal, u32t flags);
   /// set thread signal handler
   mt_sigcb _exicc (*setsignal)   (mt_sigcb cb);
   /// process stop
   qserr    _exicc (*stop)        (mt_pid pid, int tree, u32t result);
   /** validate pid/tid.
       @param  pid             pid value
       @param  tid             tid value or 0 for process check
       @retval 0               on success
       @retval E_MT_GONE       thread still exists, but gone already
       @retval E_MT_BADPID     unknown pid
       @retval E_MT_BADTID     unknown tid */
   qserr    _exicc (*checkpidtid) (mt_pid pid, mt_tid tid);
   /** opens a task list on specified device(s).
       @param  devmask         devices to show task list (bit mask, set
                               bit 0 for screen).
       @return error value or 0. */
   qserr    _exicc (*tasklist)    (u32t devmask);
} _qs_mtlib, *qs_mtlib;

#ifdef __cplusplus
}
#endif
#endif // QSINIT_MTLIBSHARED
