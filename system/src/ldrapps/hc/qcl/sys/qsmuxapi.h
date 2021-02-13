//
// QSINIT API
// low level MTLIB services
//
#ifndef QSINIT_MUTEX_CLASS
#define QSINIT_MUTEX_CLASS

/* note, that headers not typed here because of too many system & converted
   ones to use. This is internal file, so no reason to worry */

#ifdef __cplusplus
extern "C" {
#endif

#include "qstask.h"

typedef u32t  mux_handle_int;
typedef u32t  evt_handle_int;

/** mutex handle conversion service function.
    Function converts public handle back to mux_handle_int. This service
    provided by START module for mutex implementation module (MTLIB).
    Must be called inside MT lock only. */
typedef qserr   _std (*qs_muxcvtfunc)(qshandle mutex, mux_handle_int *res);

/** low level on-timer callback.
    @param  pusrdata    pointer (!) to user data parameter (this allows to
                        modify it for next call)
    @return 0 or diff between signaled and next clock value to reinstall
            callback. */
typedef qsclock _std (*qs_croncbfunc)(void **pusrdata);

/** is event available in queue?
    Low level function for wait object implementation, can be called
    from timer interrupt. Have no locks inside, must be called in MT
    locked state only.
    @param        queue   queue handle
    @param  [out] ppid    pid of this handle`s owner, ptr can be 0,
                          result too (in case of detached handle)
    @param  [out] sft_no  SFT #, can be 0
    @return number of events in queue or -1 if handle is invalid */
typedef int     _std (*qe_availfunc) (qshandle queue, u32t *ppid, u32t *sft_no);

/** process data enumeration callback.
    _qs_sysmutex.enumpd calls this function for every existing process in
    system. MT lock active during call.
    @param  pd            process data.
    @param  usrdata       user data arg from enumpd call.
    @return 1 to continue enumeration, 0 to cancel */
typedef int     _std (*qe_pdenumfunc)(mt_prcdata *pd, void *usrdata);

/** return zero term list of scheduled events for this queue.
    Call this in MT lock to guarantee, that all eid will remain valid until
    you process it.
    @param        queue   queue handle
    @return zero if no scheduled events, else thread(!) owned heap block */
typedef qe_eid* _std (*qe_getschfunc)(qshandle queue);

typedef struct {
   int        state;
   u32t         pid;
   u32t         tid;
   u32t     waitcnt;
   u32t      sft_no;
   int        owner;   ///< caller is current owner (just to speed up mt_muxstate())
} qs_sysmutex_state;

/** Low level MT lib services.
    This code is internal and should not be used on user level (even if
    you can acquire such ptr in some way).
    Basically, this is the interface to mutex implemetation, used by system
    functions to handle mutexes as common system handles */
typedef struct qs_sysmutex_s {
   /// common init call.
   void      _exicc (*init   )(qs_muxcvtfunc sf1, qe_availfunc sf2,
                               qshandle sys_q, qe_getschfunc sf3);
   /// create mutex/event.
   qserr     _exicc (*create )(mux_handle_int *res, u32t sft_no, int event,
                               int signaled);
   /// release mutex.
   qserr     _exicc (*release)(mux_handle_int muxh);
   /// event action
   qserr     _exicc (*event  )(evt_handle_int evh, u32t action);
   /// delete mutex/event.
   qserr     _exicc (*free   )(mux_handle_int muxh, int force);
   /** return mutex state.
       info can be 0.
       @return -1 on error, 0 if mutex free, else current lock count */
   int       _exicc (*state  )(mux_handle_int muxh, qs_sysmutex_state *info);
   /** install callback at specified time.
       Callback invoked during context switching (from timer interrupt too!),
       in random context, with unknown stack size and cannot use any system
       calls!

       Only one callback per function address is possible, next call with the
       same address will replace target time and "usrdata".

       Up to 4 (four) callbacks in the system can be installed.

       @param  at           Time to fire (use 0 to deinstall this callback)
       @param  cb           Callback function address
       @param  usrdata      User data arg for callback
       @return 0 on success or error value */
   qserr     _exicc (*callat )(qsclock at, qs_croncbfunc cb, void *usrdata);
   /** enum processes.
       Process enumeration function, callback called for every process, with
       address mt_prcdata in parameter. MT lock is on during entire call.
       @param  cb           Callback function address */
   void      _exicc (*enumpd )(qe_pdenumfunc cb, void *usrdata);
   /// replace fiber regs by supplied one (both 32 & 64, depends on host)
   void      _exicc (*setregs)(mt_fibdata *fd, struct tss_s *regs);
   /** get process data by pid value.
       @attention Function turns on MT lock and LEAVE it ON after exit.
       
       @param  pid          Process ID
       @return process data or 0. MT lock is ON in both cases! */
   mt_prcdata* _exicc (*getpd)(u32t pid);
} _qs_sysmutex, *qs_sysmutex;

/// direct export for MTLIB
typedef struct qs_fpustate_s {
   u32t               useTS;
   void      _std   (*updatets)(void);
   /// #NM exception handler
   void      _std   (*xcpt_nm) (void);
   /// context switch callback (called from interrupt too!)
   void      _std   (*switchto)(mt_thrdata *to);
   /// allocate state (use -1 for current fiber)
   qserr     _std   (*allocstate)(mt_thrdata *owner, int fiber);
} _qs_fpustate, *qs_fpustate;

/// @name sys_q events
#define SYSQ_SESSIONFREE    0x00001  ///< session free (a=mt_prcdata*)
#define SYSQ_BEEP           0x00002  ///< speaker beep (freq in a, dur in b)
#define SYSQ_MEMSTAT        0x00003  ///< periodic memory stat (scheduled)
#define SYSQ_DCNOTIFY       0x00004  ///< data cache timer notification
#define SYSQ_FREE           0x00005  ///< free memory block (a=ptr, b=heap(1/0), c=sender)
#define SYSQ_ALARM          0x00006  ///< alarm signal (a=pid, b=tid, scheduled)
#define SYSQ_SCHEDAT        0x00007  ///< scheduled command (a=cmdline)
//@}

#ifdef __cplusplus
}
#endif
#endif // QSINIT_MUTEX_CLASS
