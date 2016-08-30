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
#include "seldesc.h"
#include "efnlist.h"
#include "qsmodext.h"
#include "qcl/sys/qsmuxapi.h"

#define MUTEX_MAXLOCK       0xFFFF    ///< max. number of nested requests

/// setup DPMI & real mode interrupt handlers
u32t _std sys_tmirq32(u32t lapicaddr, u8t tmrint, u8t sprint);
/** query last # of APIC timer interrupts in real mode.
    @return total number of interrupts in high word and confirmed by ISR
            number of interrupts (with APIC EOI processed) in low word.
            Counters zeroed by this call.
            Always return 0 on EFI host. */
u32t _std sys_rmtstat(void);
/** set mt_yield function callback.
    mt_yield() called at least on ANY return from real mode/EFI calls.
    @param func    void callback function pointer
    @return previous callaback value */
void*_std sys_setyield(void *func);

void  init_process_data(void);

void* alloc_thread_memory(u32t pid, u32t tid, u32t size);

/// replace fiber regs by supplied one (both 32 & 64, depends on host)
void mt_setregs(mt_fibdata *fd, struct tss_s *regs);

/// set eax value in regs
void mt_seteax(mt_fibdata *fd, u32t eaxvalue);

void _std regs32to64(struct xcpt64_data *dst, struct tss_s *src);
void _std regs64to32(struct tss_s *dst, struct xcpt64_data *src);

/// note, that this func is ALWAYS reset mplock to 0!
void _std swapregs32(struct tss_s *saveregs, struct tss_s *setregs);

/** funny routine.
    Caller should set tiWaitReason if switch reason is SWITCH_WAIT.
    @param  thread    thread to switch to, can be 0 to schedule any available
    @param  reason    reason to do */
void _std switch_context(mt_thrdata *thread, u32t reason);


void _std switch_context_timer(void *regs, int is64);

/// common mt_yield callback
void yield(void);

/// @name mt_createthread() flags
//@{
#define SWITCH_TIMER      0x0000       ///< timer (interrupt/yield only!)
#define SWITCH_EXEC       0x0001       ///< mod_exec call (suspend current thread)
#define SWITCH_PROCEXIT   0x0002       ///< main thread exit
#define SWITCH_WAIT       0x0003       ///< current thread goes to wait state
#define SWITCH_EXIT       0x0004       ///< final exit, thread data should be freed
#define SWITCH_FIBER      0x0005       ///< switch to other fiber of active thread
//@}


/** allocate new fiber slot.
    @param  th        thread
    @param  type      fiber type
    @param  stacksize stack size to allocate or 0
    @param  eip       starting point
    @return fiber index or FFFF on error */
u32t        mt_allocfiber(mt_thrdata *th, u32t type, u32t stacksize, u32t eip);

/** free fiber data.
    Function just throws trap screen on any error ;)
    @param  th        thread
    @param  index     fiber index
    @param  fini      do not reinitialize it (final free) - 1/0 */
void        mt_freefiber(mt_thrdata *th, u32t index, int fini);

/** allocates new thread.
    Must be called inside MT lock only! */
mt_thrdata *mt_allocthread(mt_prcdata *pd, u32t eip, u32t stacksize);

/** free thread data.
    Must be called inside MT lock only!
    Function just throws trap screen on any error ;)
    @param  th        thread
    @param  fini      do not reinitialize it (final free) - 1/0 */
void        mt_freethread(mt_thrdata *th, int fini);

/// real mt_getthread(), chained into START to replace stub function
u32t _std   mt_gettid(void);

/// usleep() replacement to yield time to other threads/hlp function
void _std   mt_usleep(u32t usec);

/// must be called inside lock only!
mt_prcdata* get_by_pid(u32t pid);

mt_thrdata* get_by_tid(mt_prcdata *pd, u32t tid);

u32t        pid_by_module(module* handle);

void        update_wait_state(mt_prcdata *pd, u32t waitreason, u32t waitvalue);
/// just retn
void        pure_retn(void);

/** return mutex state.
    this is qs_sysmutex->isfree method implementation.
    @param        data       exit stuff, just 0 here
    @param        muxh       mutex
    @param  [out] owner_pid  owner pid, can be 0
    @param  [out] owner_tid  owner tid, can be 0
    @return -1 on error, 0 if mutex free, else current lock count */
int _exicc  mutex_state(EXI_DATA, mux_handle_int muxh, qs_sysmutex_state *info);

/** capture mutex to specified thread.
    Must be called inside MT lock only! */
qserr       mutex_capture(mux_handle_int muxh, mt_thrdata *who);

/// release all mutexes, owned by thread
void        mutex_release_all(mt_thrdata *th);

/// inc/dec mutex usage counter by mt_waitiobject().
qserr       mutex_wcounter(mux_handle_int muxh, int inc);

/// non-reenterable tree walking start (whole process should be locked)
mt_prcdata *walk_start(void);
/// non-reenterable tree walking get next
mt_prcdata *walk_next(mt_prcdata *cur);

/// thread`s exit point
void        mt_exitthreada(u32t rc);
#pragma aux mt_exitthreada "_*" parm [eax];

void        register_mutex_class(void);
/// start system idle thread
void        start_idle_thread(void);

typedef void* _std (*pf_memset)   (void *dst, int c, u32t length);
typedef void* _std (*pf_memcpy)   (void *dst, const void *src, u32t length);
typedef void  _std (*pf_mtstartcb)(qs_sysmutex mproc);
typedef void  _std (*pf_usleep)   (u32t usec);
typedef void  _std (*pf_mtpexitcb)(mt_thrdata *td);

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
extern u32t        dump_lvl;      /// dump level from env. key
extern u32t       mh_qsinit,
                   mh_start;
extern pf_memset    _memset;      ///< direct memset call, without chunk
extern pf_memcpy    _memcpy;      ///< direct memcpy call, without chunk
extern pf_usleep    _usleep;      ///< direct original usleep call
extern
pf_mtstartcb     mt_startcb;
extern
pf_mtpexitcb     mt_pexitcb;
extern
mt_prcdata       *pd_qsinit;      ///< top of process tree
extern volatile
mt_thrdata      *pt_current;      ///< current active thread (!!!)
extern
mt_thrdata      *pt_sysidle;      ///< "system idle" thread
extern
mt_proc_cb     mt_exechooks;      ///< this thing is import from QSINIT module
#pragma aux mt_exechooks "_*";
extern u32t      *pxcpt_top;      ///< ptr to xcpt_top in START module
/** bit shift for TSC value.
    This is value to shift right TSC to get value below 1ms, which
    is used as scheduler tick */
extern u32t      tick_shift;
extern u16t        force_ss;      ///< force this SS sel for all new threads
extern
u32t              *pid_list;
extern
mt_prcdata       **pid_ptrs;
extern
u32t              pid_alloc,      ///< number of allocated entires in pid_list
               tls_prealloc;      ///< number of preallocated TLS entires
extern
qs_muxcvtfunc    hlp_cvtmux;
extern
qs_sysmutex     mux_hlpinst;      ///< mutex helper instance for START module

typedef struct _we_list_entry {
   mt_thrdata   *caller;
   int         signaled;          ///< result, -1 while waiting
   qserr         reterr;          ///< error returned instead of result
   u32t            ecnt;          ///< number of entries in we
   u32t          clogic;          ///< combine logic
   struct
   _we_list_entry *prev,
                  *next;
   u8t            *sigf;          ///< "entry signaled" array (bool value)
   mt_waitentry      we[1];       ///< array, actually
} we_list_entry;

typedef struct _mutex_data {
   u32t                sign;
   u32t              sft_no;      ///< just for dump printing, but real sft #
   mt_thrdata        *owner;
   struct _mutex_data *prev,      ///< entries for listing in owner thread
                      *next;
   u32t             lockcnt;      ///< lock count (by current owner)
   u32t             waitcnt;      ///< # of active wait entries (to block delete)
} mutex_data;

/** inform about process/thread exit and check wait lists for action.
    Must be called inside MT lock only!
    When called from timer interrupt, both pid/tid must be zero.

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

#define memset      _memset
#define memcpy      _memcpy

#define THROW_ERR_PQ(pq) \
   sys_exfunc4(xcpt_prcerr, pq->self->name, pq->pid)

#define THROW_ERR_PD(pd) \
   sys_exfunc4(xcpt_prcerr, pd->piModule->name, pd->piPID)


#endif // MTLIB_INTERNAL
