//
// QSINIT
// mtlib module internals
//
#ifndef MTLIB_INTERNAL
#define MTLIB_INTERNAL

#include "stdlib.h"
#include "qsint.h"
#include "qsutil.h"
#include "errno.h"
#include "qstask.h"
#include "qspdata.h"
#include "seldesc.h"
#include "efnlist.h"
#include "qserr.h"

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
u32t mt_allocfiber(mt_thrdata *th, u32t type, u32t stacksize, u32t eip);

/** free fiber data.
    Function just throws trap screen on any error ;)
    @param  th        thread
    @param  index     fiber index
    @param  fini      do not reinitialize it (final free) - 1/0 */
void mt_freefiber(mt_thrdata *th, u32t index, int fini);

/// allocate new thread
mt_thrdata *mt_allocthread(mt_prcdata *pd, u32t eip, u32t stacksize);

/** free thread data.
    Function just throws trap screen on any error ;)
    @param  th        thread
    @param  fini      do not reinitialize it (final free) - 1/0 */
void mt_freethread(mt_thrdata *th, int fini);

/// real mt_getthread(), chained into START to replace stub function
u32t _std mt_gettid(void);

/// must be called inside lock only!
mt_prcdata* get_by_pid(u32t pid);

mt_thrdata* get_by_tid(mt_prcdata *pd, u32t tid);

u32t pid_by_module(module* handle);

void update_wait_state(mt_prcdata *pd, u32t waitreason, u32t waitvalue);
/// just retn
void pure_retn(void);

/// non-reenterable tree walking start (whole process should be locked)
mt_prcdata *walk_start(void);
/// non-reenterable tree walking get next
mt_prcdata *walk_next(mt_prcdata *cur);

/// thread`s exit point
void mt_exitthreada(u32t rc);
#pragma aux mt_exitthreada "_*" parm [eax];


typedef void* _std (*pf_memset)(void *dst, int c, u32t length);
typedef void* _std (*pf_memcpy)(void *dst, const void *src, u32t length);
typedef void  _std (*pf_mtstartcb)(void);

extern u32t           *apic;      ///< APIC, really - in dword addressing mode
extern u8t   apic_timer_int;      ///< APIC timer interrupt number
extern
volatile u64t    next_rdtsc;
extern u64t      tick_rdtsc,
                 tsc_100mks;      ///< counters in 0.1 ms
extern u32t       tsc_shift;
extern u16t        main_tss;
extern int            mt_on;
extern int           regs64;      ///< 64-bit register in fiber data
extern u32t       mh_qsinit,
                   mh_start;
extern pf_memset    _memset;      ///< direct memset call, without chunk
extern pf_memcpy    _memcpy;      ///< direct memcpy call, without chunk
extern
pf_mtstartcb     mt_startcb;
extern
mt_prcdata       *pd_qsinit;      ///< top of process tree
extern volatile
mt_thrdata      *pt_current;      ///< current active thread (!!!)
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

#define memset      _memset
#define memcpy      _memcpy

#define THROW_ERR_PQ(pq) \
   sys_exfunc4(xcpt_prcerr, pq->self->name, pq->pid)

#define THROW_ERR_PD(pd) \
   sys_exfunc4(xcpt_prcerr, pd->piModule->name, pd->piPID)


#endif // MTLIB_INTERNAL
