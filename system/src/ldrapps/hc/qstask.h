//
// QSINIT API
// multitasking support
// ----------------------------------------------------
//
// !WARNING! this API is unfinished now and some parts of QSINIT
//           code still thread-unsafe.
//
#ifndef QSINIT_TASKF
#define QSINIT_TASKF

#include "qstypes.h"
#include "time.h"

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
u32t     _std mt_initialize(void);

/** is multi-tasking active?
    @return boolean flag (1/0) */
u32t     _std mt_active(void);

/** yields CPU.
    This call not depends on MTLIB library presence and can be used everywere.
    It just will be ignored in single-task mode. */
void          mt_yield(void);

#ifdef __WATCOMC__
#pragma aux mt_yield "_*" modify exact [];
#endif

/// thread function type
typedef u32t _std (*mt_threadfunc)(void *arg);

/// thread enter/exit callback type
typedef u32t _std (*mt_threadcb  )(mt_tid tid);

/// thread creation data.
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
   @param  flags     Flags (MTCT_*)
   @param  optdata   Optional settings, can be 0. Note that all fields in this
                     struct must be valid (i.e. zeroed or filled with actual
                     value). For pre-allocated stack its size must be specified
                     too!
   @param  arg       thread argument
   @return thread id or 0 on error */
mt_tid   _std mt_createthread(mt_threadfunc thread, u32t flags, mt_ctdata *optdata, void *arg);

/// @name mt_createthread() flags
//@{
#define MTCT_SUSPENDED    0x0001       ///< create thread in suspended state
#define MTCT_NOFPU        0x0002       ///< thread will NEVER use FPU (MMX, SSE and so on)
#define MTCT_NOTRACE      0x0004       ///< TRACE output disabled for this thread
//@}

/** terminate thread.
   Thread can terminate itself (in this case onterm hook will be called,
   onexit hook called by mt_exitthread() only).

   @param  tid           Thread ID
   @retval E_MT_BADTID   Bad thread ID
   @retval E_MT_ACCESS   TID 1 or system thread cannot be terminated
   @retval E_MT_BUSY     Thread is waiting for child process
   @retval E_MT_GONE     Thread terminated already
   @retval E_OK          Thread terminated */
u32t     _std mt_termthread  (mt_tid tid, u32t exitcode);

void     _std mt_exitthread  (u32t exitcode);

/** resume suspended thread.
   @param  pid           Process ID, use 0 for current process.
   @param  tid           Thread ID
   @return 0 on success or error code */
qserr    _std mt_resumethread(u32t pid, mt_tid tid);

/** get current thread`s id.
    @return current thread id (1..x) */
u32t     _std mt_getthread   (void);

/** get current fiber id.
    @return current fiber index (0..x) in thread */
u32t     _std mt_getfiber    (void);

/** set executing fiber in thread.
   @param  tid           Thread ID (use 0 for current thread).
   @param  fiber         Fiber index (0..x)
   @retval E_OK          Success.
   @retval E_SYS_DONE    This fiber is active now
   @retval E_MT_BADTID   Bad thread ID
   @retval E_MT_BADFIB   Bad fiber index
   @retval E_MT_BUSY     Thread in waiting state
   @retval E_MT_GONE     Thread terminated already */
u32t     _std mt_setfiber    (mt_tid tid, u32t fiber);

/** set thread comment.
   Actually, comment is simple TLS var (16 bytes), which can be
   accessed directly. This comment string will be printed in
   thread dump and trap screen dump, at least.

   @param  str           Comment string */
qserr    _std mt_threadname  (const char *str);

/** dump process/thread list to log (in MT look).
    This function replaces mod_dumptree() call (Ctrl-Alt-F5) after
    successful mt_initialize(). */
void     _std mt_dumptree    (void);

/* TLS */

/** allocate thread local storage slot.
    Note, what all tls functions works in non-MT mode too and have no
    dependence on MTLIB library. */
u32t     _std mt_tlsalloc    (void);

/** free thread local storage slot.
   @param  index         TLS slot index
   @retval 0             on success
   @retval E_MT_TLSINDEX Invalid index value
   @retval E_SYS_ACCESS  Preallocated TLS slot cannot be released */
qserr    _std mt_tlsfree     (u32t index);

/** get TLS slot value.
   There is no way to query error in this function, it just returns 0
   for any unknown slot index. */
u64t     _std mt_tlsget      (u32t index);

/** query TLS slot address.
   @attention Note, that this address is NOT equal in different threads!
   @param  index         TLS slot index
   @param  slotaddr      Ptr to address value. Returning address is constant
                         while thread is active. Two qwords can be stored in it.
   @return error value or 0 on success */
qserr    _std mt_tlsaddr     (u32t index, u64t **slotaddr);

/** set TLS slot value.
   Actually TLS slot has space for two qwords, but second accessible via
   mt_tlsaddr() only.
   @param  index         TLS slot index
   @param  value         Value to set
   @retval 0             on success
   @retval E_MT_TLSINDEX Invalid index value */
qserr    _std mt_tlsset      (u32t index, u64t value);

/// @name pre-allocated tls slot index values
//@{
#define QTLS_ERRNO        0x0000       ///< runtime errno value for this thread
#define QTLS_ASCTMBUF     0x0001       ///< asctime() buffer address
#define QTLS_COMMENT      0x0002       ///< thread name string (up to 16 chars)
#define QTLS_SFINT1       0x0003       ///< system internal
#define QTLS_DSKSTRBUF    0x0004       ///< dsk_disktostr() buffer
#define QTLS_DSKSZBUF     0x0005       ///< dsk_formatsize() buffer
#define QTLS_SHLINE       0x0006       ///< shell output: 1st line
#define QTLS_SHFLAGS      0x0007       ///< shell output: flags
#define QTLS_MAXSYS       0x0007       ///< maximum pre-allocated slot index
//@}
#define TLS_VARIABLE_SIZE     16       ///< size of TLS slot (two qwords)

/* Wait objects */

typedef struct {
   u32t       htype;    ///< handle type (QWHT_*)
   u32t       group;    ///< bit mask - including into 0 or more of 32 groups
   union {
      clock_t     tme;  ///< sys_clock() value to wait for
      mt_tid      tid;  ///< wait for thread termination, can be 0 for any tid
      mt_pid      pid;  ///< wait for process exit
      qshandle    mux;  ///< grab mutex semaphore
      qshandle    que;  ///< event available in queue
   };
   union {
      u32t   *resaddr;  ///< address of variable for process/thread exit code (or 0)
      u64t   reserved;  ///< field reserved for system use
   };
} mt_waitentry;

/* WaitForMultipleObjects implementation.
   A bit complex function, it waits for multiple objects. By default it
   exits if any one specified object is signaled. To customize rules -
   objects can be combined into groups and for every group AND or OR logic
   is possible (i.e. waiting for all in group or for the first one).

   There are 32 groups is possible, every object can belong to all of
   them (this defined by bit mask "group" in mt_waitentry structure).

   If any one defined group is signaled - function returns. For objects, not
   included into groups (mt_waitentry.group==0), default OR logic is applied
   (i.e. first signaled will terminate waiting).

   Timeout can be defined by those rules too (i.e. just add QWHT_CLOCK wait
   entry with mt_waitentry.group = 0 or as separate group).

   Note, what clib usleep() function in MT mode calls this for a long waiting
   periods (more, than half of timer tick).

   QWHT_KEY entry type waits for at least one keypress. It does not remove key
   from keyboard buffer and, even, does not guarantee key code availability
   (other thread can get it earlier).
   The same note applied to QWHT_QUEUE.

   For QWHT_TID/QWHT_PID unneeded resaddr parameter must be zero, else result
   code will written into random location.

   @param  list             Objects list
   @param  count            Objects list size
   @param  glogic           Bit mask of group combining logic for 32 possible
                            groups (QWCF_OR or QWCF_AND)
   @param  [out] signaled   Group number of first signaled group. This will
                            be 0 if no groups defined, or 1..32 - group index.
                            Can be 0.

   @return 0 on success, E_MT_DISABLED if MT lib still not initialized,
      E_SYS_INVPARM and some handle related errors */
qserr    _std mt_waitobject  (mt_waitentry *list, u32t count, u32t glogic,
                              u32t *signaled);

#define QWCF_OR           0x0000
#define QWCF_AND          0x0001
/// set bit for the group, group in range 1..32.
#define QWCF_GSET(group,value) (((value)&1)<<((group)-1&0x1F))

/// @name mt_waitentry types
//@{
#define QWHT_TID          0x0001       ///< wait for thread exit
#define QWHT_PID          0x0002       ///< wait for process exit
#define QWHT_CLOCK        0x0003       ///< wait for sys_clock() value (mks)
#define QWHT_MUTEX        0x0004       ///< grab mutex semaphore
#define QWHT_KEY          0x0005       ///< wait for key press
#define QWHT_QUEUE        0x0006       ///< wait for an event in queue
//@}

/// wait for thread exit
#define       mt_waitthread(tid) { u32t rc; \
   mt_waitentry we = { QWHT_TID, 0 }; we.tid = tid; \
   mt_waitobject(&we, 1, 0, &rc); }


/* Mutex */

/** create mutex.
   @param  initialowner     Set this thread as initial owner
   @param  name             Global mutex name (optional, can be 0). Named
                            mutexes can be opened via mt_muxopen().
   @param  [out] res        Mutex handle on success. Note, that this handle is
                            is common handle and it accepted by io_setstate(),
                            io_getstate() and io_duphandle().
                            I.e., mutex handle can be detached from process as
                            file handle can.

   @retval 0                on success
   @retval E_MT_DUPNAME     duplicate mutex name */
qserr    _std mt_muxcreate   (int initialowner, const char *name, qshandle *res);

/** open existing mutex.
   @param  name             Mutex name.
   @param  [out] res        Mutex handle on success.
   @retval 0                on success
   @retval E_SYS_NOPATH     no such mutex */
qserr    _std mt_muxopen     (const char *name, qshandle *res);

/** waiting for mutex available.
    As in most systems, usage count is present. I.e. number of mt_muxrelease()
    calls should be equal to number of successful waitings and mt_muxcapture()
    calls. */
qserr    _std mt_muxcapture  (qshandle handle);

/** waiting for mutex with timeout.
   @param  handle           Mutex handle.
   @retval 0                success
   @retval E_SYS_TIMEOUT    timeout reached */
qserr    _std mt_muxwait     (qshandle handle, u32t timeout_ms);

/** query mutex state (by owner only).
   @param  handle           Mutex handle.
   @param  [out] lockcnt    Lock count (can be 0)
   @retval 0                mutex owned by current thread, lockcnt is valid.
   @retval E_MT_NOTOWNER    thread is not an owner
   @retval E_MT_SEMFREE     mutex is free */
qserr    _std mt_muxstate    (qshandle handle, u32t *lockcnt);

/** release mutex.
   @param  handle           Mutex handle.
   @retval 0                on success
   @retval E_MT_NOTOWNER    current thread does not own the semaphore
   @retval E_SYS_INVHTYPE   invalid handle (another handle specific errors
                            is possible here and in all mutex functions) */
qserr    _std mt_muxrelease  (qshandle mtx);

/** close/delete mutex.
   @param  handle           Mutex handle.
   @retval 0                on success
   @retval E_MT_SEMBUSY     mutex still owned by someone */
qserr    _std mt_closehandle (qshandle handle);


/* Queue */

/// queue event.
typedef struct { 
   u32t          sign;  ///< signature (filled by system)
   u32t          code;  ///< event code
   clock_t     evtime;  ///< time stamp (filled by system)
   long         a,b,c;  ///< parameters
   u32t      reserved;
} qe_event;

typedef u32t   qe_eid;  ///< event id

#define QEID_POSTED  (FFFF)  ///< value returned if event time has passed

/** creates a new queue.
   Unlike mutexes, queues can be created (and used) in non-MT mode, but wait
   and schedule functionality will be denied.
   @param  name             Global queue name (optional, can be 0). Named
                            queues can be opened via qe_open().
   @param  [out] res        Queue handle on success.
   @retval 0                on success
   @retval E_MT_DUPNAME     duplicate queue name */
qserr    _std qe_create      (const char *name, qshandle *res);

/** open existing queue.
   @param  name             Queue name.
   @param  [out] res        Queue handle on success.
   @retval 0                on success
   @retval E_SYS_NOPATH     no such queue */
qserr    _std qe_open        (const char *name, qshandle *res);

/** close/delete queue.
   @param  handle           Queue handle.
   @return 0 on success else error code.*/
qserr    _std qe_close       (qshandle queue);

/** clear queue.
   This function frees all the events in the specified queue. 
   It does not free events that are scheduled for delivery and have yet to
   be sent.
   @param  queue            Queue handle.
   @return error code */
qserr    _std qe_clear       (qshandle queue);

/** gets the next event from the specified queue.
   @param  queue            Queue handle.
   @param  timeout_ms       Timeout period, in ms. Ignored in non-MT
                            mode (i.e. function will always return
                            immediately).
   @return event or 0. Event must be released via free(). */
qe_event*_std qe_waitevent   (qshandle queue, u32t timeout_ms);

/** take an event from a queue.
   Taking an event from a queue reduces the number of events left in the
   queue and changes their ordinal positions.

   @param  queue            Queue handle.
   @param  index            The event number to take (0..x).
   @return event (in heap block, must be released by caller) or 0 in
           case of error). */
qe_event*_std qe_takeevent   (qshandle queue, u32t index);

/** returns a pointer to the specified event in a queue.
   @attention this function is dangerous, actually, because it returns
              pointer to an event, which is still available to take by
              any other thread.
   @param  queue            Queue handle.
   @param  index            The event number to take (0..x).
   @return event or 0 in case of error. */
qe_event*_std qe_peekevent   (const qshandle queue, u32t index);

/** query number of events in queue, available to receive.
   @param  queue            Queue handle.
   @return number of available events or -1 if handle is invalid */
int      _std qe_available   (qshandle queue);

/** place an event into queue. 
   @param  queue            Queue handle.
   @param  code             Event code value.
   @return 0 on success of error code. */
qserr    _std qe_postevent   (qshandle queue, u32t code, long a, long b, long c);

/** place an event into queue. 
   @param  queue            Queue handle.
   @param  event            Event data. Only "code" and a,b,c fields are used.
   @return 0 on success of error code. */
qserr    _std qe_sendevent   (qshandle queue, qe_event *event);

/** set up the future delivery of an event.
   Event identifier can be used to reschedule or unschedule this event
   later. If the delivery time has already passed, the event is delivered
   immediately.
   @param  queue            Queue handle.
   @param  attime           System clock time (sys_clock()) when event should
                            arrive.
   @param  code             Event code.
   @return unique identifier for the scheduled event, 0 on error or
           QEID_POSTED if event is just posted to queue because attime is a
           past. In non-MT mode function will always fail. */
qe_eid   _std qe_schedule    (qshandle queue, clock_t attime, u32t code, 
                              long a, long b, long c);

/** reschedules an event.
   @param  eventid          Event id.
   @param  attime           System clock time (sys_clock()) when event should
                            arrive.
   @return unique identifier for the scheduled event, 0 on error (invalid
           eventid or access error) or QEID_POSTED if event is
           just posted to queue because attime is a past. */
qe_eid   _std qe_reschedule  (qe_eid eventid, clock_t attime);

/** unschedule an event. 
   @param  eventid          Event id.
   @return event (in heap block, which must be released by caller), or
           0 on error (event has already been delivered or invalid event id
           or access error) */
qe_event*_std qe_unschedule  (qe_eid eventid);


/* Session management */

/// @name mod_execse bit flags
//@{
#define QEXS_DETACH       0x0001       ///< launch detached process
#define QEXS_WAITFOR      0x0002       ///< delay execution until 1st mt_waitobject() for this pid
#define QEXS_NOTACHILD    0x0004       ///< launch as independent process, not a caller`s child
//@}

/** start new session.
    Function starts a new session. Caller can query launched process exit
    code only by mt_waitobject() call. To guarantee it success QEXS_WAITFOR
    flag should be used - it suspends new process until 1st mt_waitobject()
    call from launcher process with child pid in wait entries.

    @param       module  Module handle.
    @param       env     Environment data.
    @param       params  Arguments string.
    @param       flags   Execution flags (QEXS_*).
    @param [out] ppid    Pid of new process (on success).
    @return module load/start error code. Further activity is not depends on
            caller */
qserr    _std mod_execse     (u32t module, const char *env, const char *params,
                              u32t flags, mt_pid *ppid);

#define SESN_DETACHED          0       ///< detached session common number

/// query current session number
u32t          se_sesno       (void);

#ifdef __WATCOMC__
#pragma aux se_sesno "_*" value [eax] modify exact [eax];
#endif

#ifdef __cplusplus
}
#endif
#endif // QSINIT_TASKF
