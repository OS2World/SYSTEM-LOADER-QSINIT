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
    provided by START module for mutex implementation module (MTLIB).
    Must be called inside MT lock only. */
typedef qserr _std (*qs_muxcvtfunc)(qshandle mutex, mux_handle_int *res);

typedef struct {
   int        state;
   u32t         pid;
   u32t         tid;
   u32t     waitcnt;
   u32t      sft_no;
   int        owner;   ///< caller is current owner (just to speed up mt_muxstate())
} qs_sysmutex_state;

/** Low level mutex api.
    This code is internal and should not be used on user level (even if 
    you can acquire such ptr in some way).
    This is the interface to mutex implemetation, used by system functions to
    handle mutexes as common system handles */
typedef struct qs_sysmutex_s {
   /// common init call.
   void      _exicc (*init   )(qs_muxcvtfunc sfunc);
   /// create mutex.
   qserr     _exicc (*create )(mux_handle_int *res, u32t sft_no);
   /// release mutex.
   qserr     _exicc (*release)(mux_handle_int muxh);
   /// delete mutex.
   qserr     _exicc (*free   )(mux_handle_int muxh, int force);
   /** return mutex state.
       info can be 0.
       @return -1 on error, 0 if mutex free, else current lock count */
   int       _exicc (*state  )(mux_handle_int muxh, qs_sysmutex_state *info);
} _qs_sysmutex, *qs_sysmutex;

#ifdef __cplusplus
}
#endif
#endif // QSINIT_MUTEX_CLASS
