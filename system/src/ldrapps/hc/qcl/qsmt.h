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
   u32t   _exicc (*initialize)  (void);
   /// return current mtlib state (on/off).
   u32t   _exicc (*active)      (void);
   /** get pid of EXEcuting module.
       mod_getmodpid() calls this internally, if MTLIB is active */
   u32t   _exicc (*getmodpid)   (u32t module);
   /// wait objects
   qserr  _exicc (*waitobject)  (mt_waitentry *list, u32t count, u32t glogic,
                                 u32t *signaled);
   /// create thread
   mt_tid _exicc (*createthread)(mt_threadfunc thread, u32t flags,
                                 mt_ctdata *optdata, void *arg);
   /// resume suspended thread
   qserr  _exicc (*resumethread)(u32t pid, u32t tid);
   /// set thread comment
   qserr  _exicc (*threadname)  (const char *str);
} _qs_mtlib, *qs_mtlib;

#ifdef __cplusplus
}
#endif
#endif // QSINIT_MTLIBSHARED
