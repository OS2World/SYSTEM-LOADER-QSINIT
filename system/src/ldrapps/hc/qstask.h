//
// QSINIT API
// multitasking support
// ----------------------------------------------------
//
// !WARNING! this API still unfinished now!
//
#ifndef QSINIT_TASKFUNC
#define QSINIT_TASKFUNC

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
    @retval E_MT_OLDCPU     no local APIC on CPU (P5 and below or MS VirtualPC)
    @retval E_SYS_NOMEM     memory allocation error
    @retval E_SYS_DONE      mode already on */
qserr    _std mt_initialize(void);

/** is multi-tasking active?
    @return boolean flag (1/0) */
u32t     _std mt_active(void);

/** yields CPU.
    This call does not depend on MTLIB presence and can be used anywhere.
    It just will be ignored in single-task mode. */
void          mt_yield(void);

#ifdef __WATCOMC__
#pragma aux mt_yield "_*" modify exact [];
#endif

/// thread function type
typedef u32t _std (*mt_threadfunc)(void *arg);

/// thread enter callback (called in thread context)
typedef void _std (*mt_threadcbi )(mt_threadfunc thread, void *arg);
/// thread exit callback (called in thread context)
typedef void _std (*mt_threadcbo )(void);

/// thread creation data.
typedef struct {
   u32t             size;    ///< structure size
   void           *stack;    ///< pre-allocated stack pointer (or 0; bottom!)
   u32t        stacksize;    ///< stack size (default is 64k)
   mt_threadcbi  onenter;    ///< thread start hook
   mt_threadcbo   onexit;    ///< thread exit hook
   mt_threadcbo   onterm;    ///< thread termination hook (not implemented)
   u32t              pid;    ///< pid of parent process (use 0 for current)
   qserr       errorcode;    ///< creation error code (on exit)
} mt_ctdata;

/* create thread.
   @param  thread    Thread function
   @param  flags     Flags (MTCT_*)
   @param  optdata   Optional settings, can be 0. Note that all fields (except
                     errorcode) must be valid (i.e. zeroed or set to actual
                     value). Size of pre-allocated stack MUST be specified too!
   @param  arg       thread argument
   @return thread id or 0 on error. optdata->errorcode will contain error code if
           optdata is present */
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
   Function able to terminate ANY thread except marked as a "system".

   @param  pid           Process ID, use 0 for current process.
   @param  tid           Thread ID
   @retval 0             Success
   @retval E_MT_BADTID   Bad thread ID
   @retval E_MT_ACCESS   system thread cannot be terminated
   @retval E_MT_GONE     Thread terminated already (or this is the main thread,
                         who waits for child ones) */
qserr    _std mt_termthread  (mt_pid pid, mt_tid tid, u32t exitcode);

void     _std mt_exitthread  (u32t exitcode);

/** resume suspended thread.
   @param  pid           Process ID, use 0 for current process.
   @param  tid           Thread ID
   @return 0 on success or error code */
qserr    _std mt_resumethread(mt_pid pid, mt_tid tid);

/** suspend thread.
   Note, that function will fail if thread is in waiting state.
   Thread can suspend self, with the same rules. If this case function will
   return after resume.

   @param  pid           Process ID, use 0 for current process.
   @param  tid           Thread ID
   @param  waitstate_ms  Time to wait thread clean state (i.e. moment,
                         when is does not own any mutex nor wait something).
                         (not implemented, must be zero now)
   @return 0 on success or error code */
qserr    _std mt_suspendthread(mt_pid pid, mt_tid tid, u32t waitstate_ms);

/** get current thread`s id.
    @return current thread id (1..x) */
mt_tid   _std mt_getthread   (void);

/** get current fiber id.
    @return current fiber index (0..x) in thread */
u32t     _std mt_getfiber    (void);

/** validate pid/tid.
   @param  pid           Process ID
   @param  tid           Thread ID or 0 for process check
   @retval 0             on success
   @retval E_MT_GONE     process/thread still exists, but gone already
   @retval E_MT_BADPID   unknown pid
   @retval E_MT_BADTID   unknown tid */
qserr    _std mt_checkpidtid (mt_pid pid, mt_tid tid);

/// fiber creation data.
typedef struct {
   u32t             size;    ///< structure size
   void           *stack;    ///< pre-allocated stack pointer (bottom!)
   u32t        stacksize;    ///< stack size (default is 64k)
   qserr       errorcode;    ///< creation error code (on exit)
} mt_cfdata;

/* create a new fiber.
   Fiber is a separate execution stream in a thread (looks mostly like
   Windows API fibers).
   Fiber 0 is always allocated by the system - this is thread function code.
   More fibers can be created via this call.

   Switching between fibers is manual procedure. Exiting from the main (0)
   or any other fiber will terminate thread (unless MTCF_APC flag was used).

   Stack for every fiber allocated separately, as well as registers and
   FPU state.

   Using of MTCF_SWITCH flag will immediately switch execution from the
   current fiber to a new one. I.e. call will never return or return after
   switching to this fiber back. In this case Fiber ID, returned by this
   function can be invalid already (but still can be used to check success).

   @param  fiber     Fiber function
   @param  flags     Flags (MTCT_*)
   @param  optdata   Optional settings, can be 0. Note that all fields (except
                     errorcode) must be valid (i.e. zeroed or set to actual
                     value). Size of pre-allocated stack MUST be specified too!
   @param  arg       Fiber function argument
   @return fiber id (>0) or 0 on error. optdata->errorcode will contain error
           code if optdata is present  */
u32t     _std mt_createfiber (mt_threadfunc fiber, u32t flags, mt_cfdata *optdata, void *arg);

/// @name mt_createfiber() flags
//@{
#define MTCF_APC          0x0001       ///< switch to fiber 0 on exit instead of thread termination
#define MTCF_SWITCH       0x0002       ///< switch to new fiber on creation
//@}

/** switch to another fiber in the current thread.
   @param  fiber         Fiber index (0..x)
   @param  free          Flag to close and free current fiber after switch (1/0).
   @retval E_OK          Success.
   @retval E_SYS_DONE    This fiber is active now
   @retval E_MT_BADFIB   Bad fiber index or trying to free fiber 0 */
qserr    _std mt_switchfiber (u32t fiber, int free);

/** set thread comment.
   Actually, comment is simple TLS var (16 bytes), which can be
   accessed directly. This comment string will be printed in
   thread dump and trap screen dump, at least.

   @param  str           Comment string */
qserr    _std mt_threadname  (const char *str);

/** get thread comment.
   @param  pid           Process ID, use 0 for current process.
   @param  tid           Thread ID
   @param  buffer        Buffer for the name, at least 17 bytes length
   @return error code or 0 on success */
qserr    _std mt_getthname   (mt_pid pid, mt_tid tid, char *buffer);

/** dump process/thread list to log (in MT look).
    This function replaces mod_dumptree() call (Ctrl-Alt-F5) after
    successful mt_initialize(). */
void     _std mt_dumptree    (void);

/* TLS */

/** allocate thread local storage slot.
    Note, that all tls functions works in non-MT mode too and have no
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

/// @name pre-allocated tls slot index values (internal system variables)
//@{
#define QTLS_ERRNO        0x0000       ///< runtime errno value for this thread
#define QTLS_ASCTMBUF     0x0001       ///< asctime() buffer address
#define QTLS_COMMENT      0x0002       ///< thread name string (up to 16 chars)
#define QTLS_SFINT1       0x0003       ///< system internal
#define QTLS_DSKSTRBUF    0x0004       ///< dsk_disktostr() buffer
#define QTLS_DSKSZBUF     0x0005       ///< dsk_formatsize() buffer
#define QTLS_SHLINE       0x0006       ///< shell output: 1st line
#define QTLS_SHFLAGS      0x0007       ///< shell output: flags
#define QTLS_SIGINFO      0x0008       ///< signal processing data
#define QTLS_CHAININFO    0x0009       ///< module chaining support
#define QTLS_ALARMQH      0x000A       ///< alarm() support
#define QTLS_TPRINTF      0x000B       ///< tprintf() FILE* variable
#define QTLS_FORCEANSI    0x000C       ///< ANSI state override (1 - off, >=2 - on)
#define QTLS_MAXSYS       0x000C       ///< maximum pre-allocated slot index
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
      qshandle     wh;  ///< mutex/queue/event handle
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
   entry with mt_waitentry.group = 0 or as a separate group).

   Note, what clib usleep() function in MT mode calls this for a long waiting
   periods (more, than half of timer tick).

   QWHT_KEY entry type waits for at least one keypress. It does not remove key
   from keyboard buffer and, even, does not guarantee key code availability
   (other thread can get it earlier).
   The same note applied to QWHT_QUEUE.

   QWHT_SIGNAL cause exit from a wait cycle on any incoming signal. By default
   waiting process is unbreakable by signals. Only thread termination able to
   cancel it.

   For QWHT_TID/QWHT_PID unneeded resaddr parameter must be zero, else result
   code will written into a random location.

   Thread/process must exist for QWHT_TID/QWHT_PID entries. I.e., if thread
   has finished before this call - function will return error about wrong TID
   value.

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
#define QWHT_TID          0x0001       ///< wait for a thread exit (current process)
#define QWHT_PID          0x0002       ///< wait for process exit
#define QWHT_CLOCK        0x0003       ///< wait for sys_clock() value (mks)
#define QWHT_MUTEX        0x0004       ///< grab mutex semaphore
#define QWHT_KEY          0x0005       ///< wait for a key press
#define QWHT_QUEUE        0x0006       ///< wait for an event in queue
#define QWHT_SIGNAL       0x0007       ///< exit if any signal occurs
#define QWHT_EVENT        0x0008       ///< waiting for an event
#define QWHT_HANDLE       0x0009       ///< any of mutex/queue/event
//@}

/// wait for thread exit
#define       mt_waitthread(threadid) { u32t rc; \
   mt_waitentry we = { QWHT_TID, 0 }; we.tid = threadid; \
   mt_waitobject(&we, 1, 0, &rc); }


/* Event */

/** create an event.
   @param  signaled      Initial event state.
   @param  name          Global event name (optional, can be 0). Named events
                         can be opened via mt_eventopen().
   @param  [out] res     Event handle on success. Handle is common handle and
                         accepted by io_setstate(), io_getstate() and io_duphandle().
   @retval 0             on success
   @retval E_MT_DUPNAME  duplicate event name */
qserr    _std mt_eventcreate (int signaled, const char *name, qshandle *res);

/** open existing event handle.
   @param  name          Event name.
   @param  [out] res     Event handle on success.
   @retval 0             on success
   @retval E_SYS_NOPATH  no such mutex */
qserr    _std mt_eventopen   (const char *name, qshandle *res);

/// @name mt_eventact actions
//@{
#define QEVA_RESET        0x0000       ///< reset event
#define QEVA_SET          0x0001       ///< set event
#define QEVA_PULSE        0x0002       ///< pulse event for all
#define QEVA_PULSEONE     0x0003       ///< pulse event only for the first one who awaits
//@}

/** event set/reset/pulse.
   Note, that QEVA_PULSEONE will signal only for a thread, which would be
   really released by it. I.e. if thread awaits in mt_waitobject() with
   complex rules, it will be released only if event pulse is a single
   thing, needed to complete waiting.

   @param  handle        Event handle.
   @param  action        Event action (QEVA_* value).
   @return 0 on success else error code.*/
qserr    _std mt_eventact    (qshandle handle, u32t action);

#define       mt_eventreset(evh) mt_eventact((evh),QEVA_RESET)
#define       mt_eventset(evh)   mt_eventact((evh),QEVA_SET)
#define       mt_eventpulse(evh) mt_eventact((evh),QEVA_PULSE)

/** waiting for a mutex or an event with timeout.
   This is just a sub-class of mt_waitobject() function.
   @param  handle        Mutex/event handle.
   @param  timeout_ms    Timeout, ms (0xFFFFFFFF to wait forever)
   @retval 0             success
   @retval E_SYS_TIMEOUT timeout reached */
qserr    _std mt_waithandle  (qshandle handle, u32t timeout_ms);


/* Mutex */

/** create mutex.
   @param  initialowner  Set this thread as initial owner
   @param  name          Global mutex name (optional, can be 0). Named mutexes
                         can be opened via mt_muxopen().
   @param  [out] res     Mutex handle on success. Note, that this handle is
                         is common handle and it accepted by io_setstate(),
                         io_getstate() and io_duphandle().
                         I.e., mutex handle can be detached from a process
                         as a file handle can.

   @retval 0             on success
   @retval E_MT_DUPNAME  duplicate mutex name */
qserr    _std mt_muxcreate   (int initialowner, const char *name, qshandle *res);

/** open existing mutex.
   @param  name          Mutex name.
   @param  [out] res     Mutex handle on success.
   @retval 0             on success
   @retval E_SYS_NOPATH  no such mutex */
qserr    _std mt_muxopen     (const char *name, qshandle *res);

/** waiting for a mutex available.
    As in most systems, usage count present. I.e. number of mt_muxrelease()
    calls must be equal to number of successful waitings and mt_muxcapture()
    calls. */
qserr    _std mt_muxcapture  (qshandle handle);

/** waiting for a mutex with timeout.
   @param  handle        Mutex handle.
   @retval 0             success
   @retval E_SYS_TIMEOUT timeout reached */
qserr    _std mt_muxwait     (qshandle handle, u32t timeout_ms);

/** query mutex state (by owner only).
   @param  handle        Mutex handle.
   @param  [out] lockcnt Lock count (can be 0)
   @retval 0             mutex owned by current thread, lockcnt is valid.
   @retval E_MT_NOTOWNER thread is not an owner
   @retval E_MT_SEMFREE  mutex is free */
qserr    _std mt_muxstate    (qshandle handle, u32t *lockcnt);

/** release mutex.
   @param  handle        Mutex handle.
   @retval 0             on success
   @retval E_MT_NOTOWNER   current thread does not own the semaphore
   @retval E_SYS_INVHTYPE  invalid handle (another handle specific errors
                           is possible here and in all mutex functions) */
qserr    _std mt_muxrelease  (qshandle handle);

/** close/delete mutex.
   @param  handle        Mutex handle.
   @retval 0             on success
   @retval E_MT_SEMBUSY  mutex still owned by someone */
qserr    _std mt_closehandle (qshandle handle);


/* Queue */

typedef u32t   qe_eid;  ///< event id

/// queue event.
typedef struct { 
   u32t          sign;  ///< signature (filled by system)
   u32t          code;  ///< event code
   clock_t     evtime;  ///< time stamp (filled by system)
   long         a,b,c;  ///< parameters
   qe_eid          id;  ///< id of scheduled event, else 0 (filled by system)
} qe_event;

#define QEID_POSTED  (FFFF)  ///< value returned if event time has passed

/** creates a new queue.
   Unlike mutexes, queues can be created (and used) in non-MT mode, but wait
   and schedule functionality will be denied.
   @param  name          Global queue name (optional, can be 0). Named queues
                         can be opened via qe_open().
   @param  [out] res     Queue handle on success.
   @retval 0             on success
   @retval E_MT_DUPNAME  duplicate queue name */
qserr    _std qe_create      (const char *name, qshandle *res);

/** open existing queue.
   @param  name          Queue name.
   @param  [out] res     Queue handle on success.
   @retval 0             on success
   @retval E_SYS_NOPATH  no such queue */
qserr    _std qe_open        (const char *name, qshandle *res);

/** close/delete queue.
   @param  queue         Queue handle.
   @return 0 on success else error code.*/
qserr    _std qe_close       (qshandle queue);

/** clear queue.
   This function frees all the events in the specified queue. 
   It does not free events that are scheduled for delivery and have yet to
   be sent.
   @param  queue         Queue handle.
   @return error code */
qserr    _std qe_clear       (qshandle queue);

/** gets the next event from the specified queue.
   @param  queue         Queue handle.
   @param  timeout_ms    Timeout period, in ms. Ignored in non-MT mode (i.e.
                         function will always return immediately).
   @return event or 0. Event must be released via free(). */
qe_event*_std qe_waitevent   (qshandle queue, u32t timeout_ms);

/** take an event from a queue.
   Taking an event from a queue reduces the number of events left in the
   queue and changes their ordinal positions.

   @param  queue         Queue handle.
   @param  index         The event number to take (0..x).
   @return event (in heap block, must be released by caller) or 0 in
           case of error). */
qe_event*_std qe_takeevent   (qshandle queue, u32t index);

/** returns a pointer to the specified event in a queue.
   @attention this function is dangerous, actually, because it returns
              pointer to an event, which is still available to take by
              any other thread.
   @param  queue         Queue handle.
   @param  index         The event number to take (0..x).
   @return event or 0 in case of error. */
qe_event*_std qe_peekevent   (const qshandle queue, u32t index);

/** query number of events in queue, available to receive.
   @param  queue         Queue handle.
   @return number of available events or -1 if handle is invalid */
int      _std qe_available   (qshandle queue);

/** place an event into queue. 
   @param  queue         Queue handle.
   @param  code          Event code value.
   @return 0 on success of error code. */
qserr    _std qe_postevent   (qshandle queue, u32t code, long a, long b, long c);

/** place an event into queue. 
   @param  queue         Queue handle.
   @param  event         Event data. Only "code" and a,b,c fields are used.
   @return 0 on success of error code. */
qserr    _std qe_sendevent   (qshandle queue, qe_event *event);

/** set up the future delivery of an event.
   Event identifier can be used to reschedule or unschedule this event
   later. If the delivery time has already passed, the event is delivered
   immediately.
   @param  queue         Queue handle.
   @param  attime        System clock (sys_clock()) when event should arrive.
   @param  code          Event code.
   @return unique identifier for the scheduled event, 0 on error or
           QEID_POSTED if event is just posted to queue because attime is a
           past. In non-MT mode function will always fail. */
qe_eid   _std qe_schedule    (qshandle queue, clock_t attime, u32t code, 
                              long a, long b, long c);

/** reschedules an event.
   @param  eventid       Event id.
   @param  attime        System clock (sys_clock()) when event should arrive.
   @return unique identifier for the scheduled event, 0 on error (invalid
           eventid or access error) or QEID_POSTED if event is
           just posted to queue because attime is a past. */
qe_eid   _std qe_reschedule  (qe_eid eventid, clock_t attime);

/** unschedule an event. 
   @param  eventid       Event id.
   @return event (in heap block, which must be released by caller), or
           0 on error (event has already been delivered or invalid event id
           or access error) */
qe_event*_std qe_unschedule  (qe_eid eventid);


/* Signals */

/// @name signal values 
//@{
#define QMSV_KILL              1       ///< terminate process (process)
#define QMSV_BREAK             2       ///< break process (Ctrl-C, process)
#define QMSV_ALARM             3       ///< clib alarm() signal
#define QMSV_CLIB           4080       ///< reserved for libc (4080..4095)
#define QMSV_USER           4096       ///< user signals (4096..0xFFFFFFFF)
//@}

/** signal processing function.
   Function called in a separate fiber.
   
   longjmp() call will be fatal here, siglongjmp() must be used instead.

   @param  signal        Signal to process.
   @return success flag (1/0). If signal is NOT processed (0) then default
           action will occur. */
typedef int _std (*mt_sigcb) (u32t signal);

/** set signal processing callback for the current thread.
   Note, that clib signal use this function for signal processing. I.e.,
   replacing of callback will broke signal() settings.

   @param  cb            New callback function, 0 to return default.
   @return previous installed callback function or 0 if no one. */
mt_sigcb _std mt_setsignal   (mt_sigcb cb);

/// @name mt_signal() flags
//@{
#define QMSF_TREE         0x0001       ///< target is process tree (including child processes)
#define QMSF_TREEWOPARENT 0x0002       ///< target is process tree without parent
//@}

/** send a signal to a thread.
   Use mod_stop() to terminate process immediately, without signal processing.
   Note, that this function starts MT mode as well as all its users like
   alarm(), raise() and so on.

   Also note, that system unable to break waiting state of a thread to
   deliver signal to it. It just collect it into the signal queue for this
   thread.

   This mean, that thread, which waits for a child will deny QMSV_BREAK signal
   and receive any other only after child`s exit. 
   
   For mt_waitobject() waitings QWHT_SIGNAL entry can be added to terminate
   call when signal occurs.

   Default system processing for QMSV_KILL and QMSV_BREAK signals terminates
   process in the main thread and thread in any other (TID>1).
   
   QMSV_BREAK cause _exit(EXIT_FAILURE) call in the main thread and QMSV_KILL
   just call mod_stop() for the current process.

   Also, common thread cannot send a signal to a system thread (call returns
   E_SYS_ACCESS error).

   And another one limitation - signals delayed if current thread fiber is
   not 0 (main thread execution stream).

   @param  pid           Process id, 0 for current.
   @param  tid           Thread id for thread signal or 0 (TID 1). QMSF_TREE
                         and QMSF_TREEWOPARENT accepted only for TID 1.
   @param  signal        Signal value.
   @param  flags         QMSF_* flags.
   @return error code or 0 if signal was sent at least to a one process. */
qserr    _std mt_sendsignal  (u32t pid, u32t tid, u32t signal, u32t flags);

/* Session management */

/// @name mod_execse bit flags
//@{
#define QEXS_DETACH       0x0001       ///< launch detached process
#define QEXS_WAITFOR      0x0002       ///< delay execution until 1st mt_waitobject() for this pid
#define QEXS_BOOTENV      0x0004       ///< use init process environment instead of current
#define QEXS_BACKGROUND   0x0008       ///< start new session in the background
//@}

/** Function starts a new session.
    
    Caller can query launched process exit code only by mt_waitobject() call.
    To guarantee success QEXS_WAITFOR flag should be added - it suspends new
    process until 1st mt_waitobject() call (in parent process!) with launched
    pid in wait entries.

    Note, that start looks identically with common exec - mod_load() and then
    mod_execse().

    @param       module  Module handle.
    @param       env     Environment data. QEXS_BOOTENV flag will determine
                         the source of environment data if this argument is 0.
    @param       params  Arguments string, can be 0.
    @param       flags   Execution flags (QEXS_*).
    @param       vdev    List of video devices to use (use 0 for default).
    @param [out] ppid    Pid of new process (on success).
    @param       title   Session title (ignored for QEXS_DETACH), can be 0.
    @return module load/start error code. Further activity is not depends on
            caller */
qserr    _std mod_execse     (u32t module, const char *env, const char *params,
                              u32t flags, u32t vdev, mt_pid *ppid,
                              const char *title);

/** Stop process (hard kill) function.
    If caller is a subject to stop too (as member of a tree) - it will be
    stopped last.
    Note, that function starts MT mode as well as mod_execse().

    With non-zero "tree" parameter function stops all processes, which it
    can stop. In this case it may return E_MT_ACCESS if one of processes,
    selected to delete - was a system.

    @param       pid     Process ID, can be 0 for current.
    @param       tree    Kill process tree flag (1/0).
    @param       result  Process exit code (applied to every process in tree).
    @return error code or 0. */
qserr    _std mod_stop       (mt_pid pid, int tree, u32t result);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_TASKFUNC
