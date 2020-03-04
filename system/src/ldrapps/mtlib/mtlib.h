//
// QSINIT
// mtlib module internals
//
#ifndef MTLIB_INTERNAL
#define MTLIB_INTERNAL

#include "qsint.h"
#include "qsbase.h"
#include "cpudef.h"
#include "stdlib.h"
#include "errno.h"
#include "qspdata.h"
#include "qsvdata.h"
#include "qsdump.h"
#include "qscon.h"
#include "seldesc.h"
#include "efnlist.h"
#include "qsmodext.h"
#include "qcl/sys/qsmuxapi.h"

#define MUTEX_MAXLOCK       0xFFFF    ///< max. number of nested requests
#define ATTIMECB_ENTRIES         4    ///< number of supported "attime" callbacks

/// setup DPMI & real mode interrupt handlers
u32t _std    sys_tmirq32(u32t lapicaddr, u8t tmrint, u8t sprint, void *tm32);
/** set mt_yield function callback.
    mt_yield() called at least on ANY return from real mode/EFI calls.
    @param func    void callback function pointer
    @return previous callaback value */
void*_std    sys_setyield(void *func);

void         init_process_data(void);

/// replace fiber regs by supplied one (both 32 & 64, depends on host)
void         mt_setregs(mt_fibdata *fd, struct tss_s *regs);

/// set eax value in regs
void         mt_seteax(mt_fibdata *fd, u32t eaxvalue);

u32t _std    mt_exit   (process_context *pq);

void _std    regs32to64(struct xcpt64_data *dst, struct tss_s *src);
void _std    regs64to32(struct tss_s *dst, struct xcpt64_data *src);

/// note, that this func is ALWAYS reset mplock to 0!
void _std    swapregs32(struct tss_s *saveregs, struct tss_s *setregs);

/** funny routine.
    Caller should set tiWaitReason if switch reason is SWITCH_WAIT.
    @param  thread    thread to switch to, can be 0 to schedule any available
    @param  reason    reason to do */
void _std    switch_context(mt_thrdata *thread, u32t reason);


void _std    switch_context_timer(void *regs, int is64);

/// common mt_yield callback
void yield(void);

/// @name switch_context() reason values
//@{
#define SWITCH_TIMER      0x0000       ///< timer (interrupt/yield only!)
#define SWITCH_EXEC       0x0001       ///< mod_exec call (suspend current thread)
#define SWITCH_PROCEXIT   0x0002       ///< main thread exit
#define SWITCH_WAIT       0x0003       ///< current thread goes to wait state
#define SWITCH_EXIT       0x0004       ///< final exit, thread data should be freed
#define SWITCH_FIBER      0x0005       ///< switch to other fiber of active thread
#define SWITCH_FIBEXIT    0x0006       ///< same as SWITCH_FIBER but delete current
#define SWITCH_SUSPEND    0x0007       ///< current thread suspends self
#define SWITCH_MANUAL     0x0008       ///< just a manual context switch
//@}

/** set TLS variable for another thread.
    Should be called in lock, because mt_thrdata may gone at any time. */
qserr _std   mt_tlssetas   (mt_thrdata *th, u32t index, u64t value);

/** allocate new fiber slot.
    @param  th           thread
    @param  type         fiber type
    @param  stacksize    stack size to allocate or 0
    @param  eip          starting point
    @param  [out] fibid  fiber id on success
    @return error code or 0 */
qserr        mt_allocfiber (mt_thrdata *th, u32t type, u32t stacksize, u32t eip,
                            u32t *fibid);

/** free fiber data.
    Function just throws trap screen on any error ;)
    @param  th           thread
    @param  index        fiber index */
void         mt_freefiber  (mt_thrdata *th, u32t index);

/** set executing fiber in thread.
   @param  tid           Thread ID (use 0 for current thread).
   @param  fiber         Fiber index (0..x)
   @retval E_OK          Success.
   @retval E_SYS_DONE    This fiber is active now
   @retval E_MT_BADTID   Bad thread ID
   @retval E_MT_BADFIB   Bad fiber index
   @retval E_MT_BUSY     Thread in waiting state
   @retval E_MT_GONE     Thread terminated already */
qserr        mt_setfiber   (mt_tid tid, u32t fiber);

/** allocates new thread.
    Must be called inside MT lock only! */
qserr        mt_allocthread(mt_prcdata *pd, u32t eip, u32t stacksize, mt_thrdata **th);

u32t         mt_newfiber   (mt_thrdata *th, mt_threadfunc fiber, u32t flags,
                            mt_cfdata *optdata, void *arg);
/** free thread data.
    Must be called inside MT lock only!
    Function just throws trap screen on any error ;)
    @param  th        thread */
void         mt_freethread (mt_thrdata *th);

/** push signal into the thread`s signal queue.
    Must be called inside MT lock only!
    @param  th         thread
    @param  sig        signal number
    @return error code */
qserr        mt_sigpush    (mt_thrdata *th, u32t sig);

/** query number of signals in the thread signal queue.
    Must be called inside MT lock only!
    Function just throws trap screen on error in thread data ;)
    @param  th           thread
    @param  [out] bsize  size of tiSigQueue buffer (can be 0)
    @return # of available signals */
u32t         mt_sigavail   (mt_thrdata *th, u32t *bsize);

/// minimal checl of thread data
void         mt_checkth    (mt_thrdata *th);

/// real mt_getthread(), chained into START to replace stub function
u32t _std    mt_gettid(void);

/// usleep() replacement to yield time to other threads/hlp function
void _std    mt_usleep     (u32t usec);

/// must be called inside lock only!
mt_prcdata*  get_by_pid    (u32t pid);

mt_thrdata*  get_by_tid    (mt_prcdata *pd, u32t tid);

u32t         pid_by_module (module* handle, u32t *parent);
/// is key press available in session sno?
u32t         key_available (u32t sno);

void         update_wait_state(mt_prcdata *pd, u32t waitreason, u32t waitvalue);

void         enum_process  (qe_pdenumfunc cb, void *usrdata);

/** collect all child processes into the list.
    Function uses walk_start() / walk_next() to enum childs.

    @param  pd        process
    @param  self      include self into the list
    @return list or 0 if no one found. I.e. if returning value is non-zero,
            then list has at least ONE entry in it. */
dd_list      mt_gettree    (mt_prcdata *pd, int self);

/// return pid list in user`s heap block
u32t*  _std  mt_pidlist    (void);

/** release childs, who waits for mt_waitobject() from parent pid.
    Must be called inside lock only!
    @param        parent     launcher process pid, who should call mt_waitobject()
    @param        child      process, waiting for parent to be ready, can be 0
                             for all */
void         update_lwait  (u32t parent, u32t child);

/// just retn
void         pure_retn     (void);
/// exit from the secondary APC fiber
void         fiberexit_apc (void);

/** return mutex state.
    this is qs_sysmutex->isfree method implementation.
    @param        data       exit stuff, just 0 here
    @param        muxh       mutex
    @param  [out] info       mutex state data
    @return -1 on error, 0 if mutex free, else current lock count */
int   _exicc mutex_state   (EXI_DATA, mux_handle_int muxh, qs_sysmutex_state *info);

/** return event state.
    @retval -1    invalid handle
    @retval 0     event is reset
    @retval 1     event is set
    @retval 2     event action is pulse one (only inside of w_check_conditions()) */
int   _exicc event_state   (evt_handle_int evh);

qserr _exicc event_action  (EXI_DATA, evt_handle_int evh, u32t action);

/** capture mutex to specified thread.
    Must be called inside MT lock only! */
qserr        mutex_capture (mux_handle_int muxh, mt_thrdata *who);

/// release all mutexes, owned by thread
void         mutex_release_all(mt_thrdata *th);

/// inc/dec mutex usage counter by mt_waitiobject().
qserr        mutex_wcounter(mux_handle_int muxh, int inc);

/** non-reenterable tree walking start.
    MT lock must be ON during whole enumeration process.
    @param        top        Top process, use 0 for pid 1 (QSINIT).
    @return top value and initialize enumeration for walk_next() */
mt_prcdata*  walk_start    (mt_prcdata *top);
/// non-reenterable tree walking get next
mt_prcdata*  walk_next     (mt_prcdata *cur);

/// thread`s exit point
void         mt_exitthreada(u32t rc);
#pragma aux mt_exitthreada "_*" parm [eax];
/// dump process tree to log
void   _std  mt_dumptree   (printf_function pfn);

void         register_mutex_class(void);
/// start system idle thread
void         start_idle_thread(void);

typedef void* _std (*pf_memset)   (void *dst, int c, u32t length);
typedef void* _std (*pf_memcpy)   (void *dst, const void *src, u32t length);
typedef void  _std (*pf_mtstartcb)(qs_sysmutex mproc);
typedef void  _std (*pf_usleep)   (u32t usec);
/** Exit thread processing in START module.
    Function must be called twice: stage 0 in thread context, just before exit
    and stage 1 when thread is gone. */
typedef void  _std (*pf_mtpexitcb)(mt_thrdata *td, int stage);
typedef int   _std (*pf_selquery) (u16t sel, void *desc);
typedef se_sysdata* _std (*pf_sedataptr)(u32t sesno);

int     _std sys_selquery(u16t sel, void *desc);

int     _std mem_threadblockex(void *block, mt_thrdata *th);

typedef enum { TmAPIC, TmIrq0 } mtlib_timer;

extern mtlib_timer tmr_mode;
extern u32t           *apic;      ///< APIC, really - in dword addressing mode
extern u8t   apic_timer_int;      ///< APIC timer interrupt number
extern
volatile u64t    next_rdtsc;
extern u64t      tick_rdtsc,
                 tsc_100mks;      ///< counters in 0.1 ms
extern u32t       tsc_shift;
extern u32t         tick_ms;      ///< timer tick in ms
extern u16t        main_tss;
extern int            mt_on;
extern int            sys64;      ///< 64-bit host mode
extern u8t      sched_trace;
extern u32t        dump_lvl;      /// dump level from env. key
extern u32t     pid_changes;
extern u32t       mh_qsinit,
                   mh_start;
extern pf_memset    _memset;      ///< direct memset call, without chunk
extern pf_memcpy    _memcpy;      ///< direct memcpy call, without chunk
extern pf_usleep    _usleep;      ///< direct original usleep call
extern
pf_sedataptr    _se_dataptr;      ///< direct se_dataptr call
extern
pf_selquery   _sys_selquery;      ///< direct sys_selquery call, without chunk
extern
pf_mtstartcb     mt_startcb;
extern
pf_mtpexitcb     mt_pexitcb;
extern
qs_fpustate     fpu_intdata;
extern
mt_prcdata       *pd_qsinit;      ///< top of process tree
extern
process_context  *pq_qsinit;      ///< process context of pid 1
extern volatile
mt_thrdata      *pt_current;      ///< current active thread (!!!)
extern
mt_thrdata      *pt_sysidle;      ///< "system idle" thread
extern
mt_proc_cb     mt_exechooks;      ///< common hooks & data (QSINIT`s variable)
extern
mod_addfunc  *mod_secondary;      ///< internal START exports for QSINIT
#pragma aux mt_exechooks "_*";
#pragma aux mod_secondary  "_*";

extern u32t      *pxcpt_top;      ///< ptr to xcpt_top in START module
/** bit shift for TSC value.
    This is shift right for TSC to get value (below 1ms), which used as
    scheduler tick */
extern u32t      tick_shift;
extern u16t        force_ss;      ///< force this SS sel for all new threads
extern
u32t              *pid_list;
/** system queue for special events.
    Created by START module in mt_startcb() callback */
extern qshandle       sys_q;
extern
mt_prcdata       **pid_ptrs;
extern
u32t              pid_alloc,      ///< number of allocated entires in pid_list
               tls_prealloc;      ///< number of preallocated TLS entires
extern
qe_availfunc     hlp_qavail;      ///< queue check helper from START
extern
qs_muxcvtfunc    hlp_cvtmux;      ///< mutex check helper from START
extern
qs_sysmutex     mux_hlpinst;      ///< mutex helper instance for START module

typedef struct _we_list_entry {
   mt_thrdata       *caller;
   int             signaled;      ///< result, -1 while waiting
   qserr             reterr;      ///< error returned instead of result
   u32t                ecnt;      ///< number of entries in we
   u32t              clogic;      ///< combine logic
   struct
   _we_list_entry     *prev,
                      *next;
   u8t                *sigf;      ///< "entry signaled" array (bool value)
   mt_waitentry       we[1];      ///< array, actually
} we_list_entry;

#define MUTEX_SIGN       0x4458554D   ///< mutex_data signature
#define EVENT_SIGN       0x44545645   ///< event_data signature
#define SEDAT_SIGN       0x52445345   ///< se_start_data signature

typedef struct _mutex_data {
   u32t                sign;
   u32t              sft_no;      ///< just for dump printing, but real sft #
   mt_thrdata        *owner;
   struct _mutex_data *prev,      ///< entries for listing in owner thread
                      *next;
   u32t             lockcnt;      ///< lock count (by current owner)
   u32t             waitcnt;      ///< # of active wait entries (to block delete)
} mutex_data;

typedef struct _event_data {
   u32t                sign;
   u32t              sft_no;      ///< just for dump printing, but real sft #
   u32t             waitcnt;      ///< # of active wait entries (to block delete)
   u8t                state;      ///< 2/1/0
} event_data;

///< session start data (mtdata parameter for mod_exec)
typedef struct {
   u32t                sign;
   u32t               flags;      ///< mod_execse() flags parameter
   u32t                 pid;      ///< process pid (on success)
   u32t                 sno;      ///< process ses number (on success)
   u32t                vdev;      ///< device list for this session (or 0)
   char              *title;      ///< title in this module`s owned heap block or 0
} se_start_data;

///< session free list entry
typedef struct _se_free_data {
   mt_prcdata           *pd;
   struct
   _se_free_data      *prev,
                      *next;
} se_free_data;

///< attime callback entry
typedef struct {
   u64t           start_tsc,
                   diff_tsc;
   qs_croncbfunc        cbf;
   void            *usrdata;
} attime_entry;

extern
attime_entry          attcb[ATTIMECB_ENTRIES];

/** inform about process/thread exit and check wait lists for action.
    Must be called inside MT lock only!
    When called from the timer interrupt, both pid/tid must be zero.

    @param pid         if tid==0 - pid exit, else tid exit (pid/tid pair)
                       or just 0 to skip.
    @param tid         this tid exiting (or 0).
    @param special     return 1 if this special wait list has success and can
                       continue executing. Can be 0 if no special check.
    @return 0 or 1 if special arg check was success */
int  w_check_conditions(u32t pid, u32t tid, we_list_entry *special);
/// must be called inside MT lock only!
void w_add(we_list_entry *entry);
/// must be called inside MT lock only!
void w_del(we_list_entry *entry);
/** remove thread from waiting list.
    must be called inside MT lock only!
    @param newstate    new thread state */
void w_term(mt_thrdata *wth, u32t newstate);

#define memset         _memset
#define memcpy         _memcpy
#define sys_selquery   _sys_selquery
#define se_dataptr     _se_dataptr

#define THROW_ERR_PQ(pq) \
   sys_exfunc4(xcpt_prcerr, pq->self->name, pq->pid)

#define THROW_ERR_PD(pd) \
   sys_exfunc4(xcpt_prcerr, pd->piModule->name, pd->piPID)

#endif // MTLIB_INTERNAL
