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
       @retval 0        on success
       @retval EINTR    library disabled by "set MTLIB = off"
       @retval ELIBACC  unable to load MTLIB module
       @retval EBUSY    unable to install timer handlers
       @retval ENODEV   no local APIC on CPU (P5 and below)
       @retval ENOMEM   memory allocation error
       @retval EEXIST   mode already on */
   u32t   _std (*initialize)(void);
   /// return current mtlib state (on/off).
   u32t   _std (*active)(void);
} _qs_mtlib, *qs_mtlib;

#ifdef __cplusplus
}
#endif
#endif // QSINIT_MTLIBSHARED
