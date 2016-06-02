//
// QSINIT API
// low level mutex implementation
//
#ifndef QSINIT_MUTEX_CLASS
#define QSINIT_MUTEX_CLASS

#include "qstypes.h"
#include "qsclass.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef u32t  mux_handle_int;

/** mutex handle conversion service function.
    Function converts public handle back to mux_handle_int. This service
    provided by START module for mutex implementation module (MTLIB). */
typedef qserr _std (*qs_muxcvtfunc)(qshandle mutex, mux_handle_int *rc);

/** Low level mutex api.
    This code is internal and should not be used on user level (even if 
    you can acquire such ptr in some way).
    This is interface to mutex implemetation, which is used by system
    functions to handle mutexes as common system handles */
typedef struct qs_sysmutex_s {
   /// common init call.
   void      _std (*init   )(qs_muxcvtfunc sfunc);
   /// create mutex.
   qserr     _std (*create )(mux_handle_int *res);
   /// capture mutex to current pid/tid.
   qserr     _std (*capture)(mux_handle_int res);
   /// release mutex.
   qserr     _std (*release)(mux_handle_int res);
   /// delete mutex.
   qserr     _std (*free   )(mux_handle_int res);
   /// is mutex free?
   int       _std (*isfree )(mux_handle_int res);
} _qs_sysmutex, *qs_sysmutex;

#ifdef __cplusplus
}
#endif
#endif // QSINIT_MUTEX_CLASS
