//
// QSINIT
// multi-tasking implementation module
//
#include "qecall.h"
#include "qsmodext.h"
#include "qsinit_ord.h"
#include "start_ord.h"
#include "mtlib.h"

#define INT_TIMER       206
#define INT_SPURIOUS    207
#define INT_STACK32    8192      ///< stack for 32-bit timer interrupt

int                mt_on = 0;
int                sys64 = 0;
u32t             classid = 0; // class id
u32t               *apic = 0; // pointer to LAPIC
u32t             cmstate = 0; // current intel's clock modulation state
u32t            dump_lvl = 0;
u8t       apic_timer_int = 0;
u8t           *tmstack32 = 0;
u64t             rdtsc55 = 0;
u32t              tics55 = 0; // APIC timer/16 tics in 55 ms
u32t             tick_ms = 0; // in ms
u64t          tsc_100mks = 0;
u32t           tsc_shift = 0; // shr for TSC value to sched. counter (value <1ms)
u64t          tick_rdtsc = 0; // in rdtsc counters
u32t          tick_timer = 0; // in APIC timer ticks
volatile u64t next_rdtsc = 0;
u16t            main_tss = 0; // main tss (or 0 in 64-bit or 32-bit safe mode)
pf_memset        _memset = 0;
pf_memcpy        _memcpy = 0;
pf_usleep        _usleep = 0;
pf_mtstartcb  mt_startcb = 0;
pf_mtpexitcb  mt_pexitcb = 0;
u32t           mh_qsinit = 0,
                mh_start = 0;
u32t          *pxcpt_top = 0; // ptr to xcpt_top in START module (xcption stack)

/// 64-bit timer interrupt callback
void _std timer64cb  (struct xcpt64_data *user);
/// 32-bit interrupt handler
void timer32(void);
/// 32-bit spurious handler
void spurious32(void);

/// update timer values
void update_clocks(void) {
   cmstate    = hlp_cmgetstate();
   tick_timer = (u64t)tics55  * tick_ms * 1000 / 54932;
   rdtsc55    = hlp_tscin55ms();
   tsc_100mks = rdtsc55 * 100 / 54932;
   tick_rdtsc = rdtsc55 * tick_ms * 1000 / 54932;
   tsc_shift  = bsr64(tsc_100mks*10);

   log_it(2, "new clocks: %u timer=%08X rdtsc=%LX shift=%u tsc100=%LX\n",
      tick_ms, tick_timer, tick_rdtsc, tsc_shift, tsc_100mks);

   apic[APIC_TMRDIV]     = 3;
   apic[APIC_TMRINITCNT] = tick_timer;

   next_rdtsc = hlp_tscread() + tick_rdtsc;
}

static void _std notify_callback(sys_eventinfo *info) {
   if (info->eventtype&SECB_CMCHANGE) {
      // clock modulation changed, update values for safeness
      u32t cmnew = hlp_cmgetstate();
      if (cmstate!=cmnew) update_clocks();
   }
}

static u32t catch_functions(void) {
   if (!mod_apichain(mh_start, ORD_START_mod_dumptree, APICN_REPLACE, mt_dumptree) ||
      !mod_apichain(mh_start, ORD_START_mt_getthread, APICN_REPLACE, mt_gettid) ||
         !mod_apichain(mh_qsinit, ORD_QSINIT_usleep, APICN_REPLACE, mt_usleep))
   {
      log_it(2, "unable to replace api!\n");
      return E_MOD_NOORD;
   }
   if (!sys_notifyevent(SECB_GLOBAL|SECB_CMCHANGE, notify_callback)) {
      /* this is not critical, actually. Intel`s clock modulation has no effect on timer
         values, it just called for order */
      log_it(2, "sys_notifyevent() error!\n");
   }
   return 0;
}

static u32t mt_start(void) {
   u64t    tscv;
   u32t     tmv, rc;
   char  *mtkey = getenv("MTLIB");
   u32t      ii;
   int   env_on = 1;

   /* parse env key:
      "MTLIB = ON [, TIMERES = 4][, DUMPLEVEL = 1]" */
   if (mtkey) {
      str_list *lst = str_split(mtkey, ",");
      env_on = env_istrue("MTLIB");
      ii     = 1;
      while (ii<lst->count) {
         char *lp = lst->item[ii];
         // str_split() gracefully trims spaces around "=" for us
         if (strnicmp(lp,"TIMERES=",8)==0) tick_ms = strtoul(lp+8, 0, 0); else
         if (strnicmp(lp,"DUMPLEVEL=",10)==0) dump_lvl = strtoul(lp+10, 0, 0);
         ii++;
      }
      free(lst);
   }
   if (!env_on) return E_MT_DISABLED;
   // adjust ticks to default
   if (tick_ms<4 || tick_ms>128) tick_ms = 16;

   apic = (u32t *)sys_getlapic();
   if (!apic) {
      log_it(2, "There is no Local APIC in CPU or software failure occured!\n");
      return E_MT_OLDCPU;
   }
   rdtsc55 = hlp_tscin55ms();
   if (!rdtsc55) {
      log_it(2, "rdtsc calc failed!?\n");
      return E_MT_OLDCPU;
   }
   // div to 16
   apic[APIC_TMRDIV]     = 3;
   apic[APIC_TMRINITCNT] = FFFF;
   // one shot, masked int
   apic[APIC_LVT_TMR]    = APIC_DISABLE;

   tscv   = hlp_tscread();
   // wait 55 ms (approx)
   while (hlp_tscread()-tscv < rdtsc55) usleep(1);

   tmv    = FFFF - apic[APIC_TMRCURRCNT];
   tics55 = tmv;
   tscv   = (u64t)tmv * 16;
   tscv   = tscv * 1000000 / 54932;

   tmv    = tscv/100000;
   tmv    = tmv/10 + (tmv%10>=5?1:0);
   log_it(2, "approx bus freq %u MHz (%u, %X)\n", tmv, tics55, rdtsc55);
   // catch some functions, critical for timer calculation
   rc = catch_functions();
   if (rc) return rc;

   apic_timer_int = INT_TIMER;
   // stack for timer32(). used in 64-bit mode too (mt_yield callback)
   tmstack32 = (u8t*)malloc_shared(INT_STACK32);
   if (!tmstack32) return E_SYS_NOMEM;
   log_it(2, "tm32 stack: %08X, esp=%08X!\n", tmstack32, tmstack32+INT_STACK32);
   mem_zero(tmstack32);
   // initial esp value
   tmstack32+= INT_STACK32;
   sys64     = sys_is64mode();

   if (sys64) {
      u64t rc;
      sys_tmirq64(timer64cb);
      // set 64-bit irq vectors (special code hosted in EFI binary)
      rc = call64(EFN_TIMERIRQON, 0, 2, INT_TIMER, INT_SPURIOUS);
      if (!rc || rc==-1) {
         log_it(2, "Unable to install 64-bit int vectors!\n");
         sys_tmirq64(0);
         return E_MT_TIMER;
      }
   } else {
      u64t int_tm = ((u64t)get_flatcs()<<32) + (u32t)&timer32,
           int_sp = ((u64t)get_flatcs()<<32) + (u32t)&spurious32;
      // query active tss - any other will deny task switching in timer
      main_tss = get_taskreg();

      if (!sys_setint(INT_TIMER, &int_tm, SINT_INTGATE) ||
          !sys_setint(INT_SPURIOUS, &int_sp, SINT_INTGATE))
      {
         log_it(2, "Unable to install 32-bit int vectors!\n");
         return E_MT_TIMER;
      }
      if (!sys_tmirq32((u32t)apic, INT_TIMER, INT_SPURIOUS))
         log_it(0, "sys_tmirq32() call error!\n");
   }
   apic[APIC_SPURIOUS] = APIC_SW_ENABLE|INT_SPURIOUS;
   // calc timer vars
   update_clocks();
   // number of preallocated TLS entries
   tls_prealloc = sys_queryinfo(QSQI_TLSPREALLOC, 0);
   // init process/thread data
   init_process_data();
   // register mutex class provider
   register_mutex_class();
   // !!!
   mt_on = 1;
   // start timer, at last!
   apic[APIC_TMRDIV]     = 3;
   apic[APIC_LVT_TMR]    = 0x20000|apic_timer_int;
   // reload it with enabled periodic interrupt!
   apic[APIC_TMRINITCNT] = tick_timer;
   // launch "system idle" thread
   start_idle_thread();
   /* inform START about MT is happen ;)
      this call is very important, it creates mutexes for some API parts and
      calls registered notifications about MT ready (this, also, mean possible
      mt_createthread() calls from it) */
   if (mt_startcb) mt_startcb(mux_hlpinst);
   // we are done
   return 0;
}

u32t _exicc cmt_initialize(void *data) {
   if (mt_on) return E_SYS_DONE;
   return mt_start();
}

u32t  _exicc cmt_state(EXI_DATA) { return mt_on?1:0; }

qserr _exicc cmt_waitobject(EXI_DATA, mt_waitentry *list, u32t count, u32t glogic, 
                          u32t *signaled)
{
   return mt_waitobject(list, count, glogic, signaled);
}

mt_tid _exicc cmt_createthread(EXI_DATA, mt_threadfunc thread, u32t flags,
                          mt_ctdata *optdata, void *arg)
{
   return mt_createthread(thread, flags, optdata, arg);
}

qserr _exicc cmt_resumethread(EXI_DATA, u32t pid, u32t tid) { return mt_resumethread(pid, tid); }

qserr _exicc cmt_threadname(const char *str) { return mt_threadname(str); }

u32t  _exicc cmt_getmodpid(EXI_DATA, u32t mh) {
   if (!mh) return 0;
   return pid_by_module((module*)mh);
}

static void *methods_list[] = { cmt_initialize, cmt_state, cmt_getmodpid,
   cmt_waitobject, cmt_createthread, cmt_resumethread, cmt_threadname };

// data is not used and no any signature checks in this class
typedef struct {
   u32t     reserved;
} mt_data;

static void _std mt_init(void *instance, void *data) {
   mt_data *cpd  = (mt_data*)data;
   cpd->reserved = 0xCC;
}

static void _std mt_done(void *instance, void *data) {
   mt_data *cpd  = (mt_data*)data;
   cpd->reserved = 0;
}

unsigned __cdecl LibMain(unsigned hmod, unsigned termination) {
   if (!termination) {
      if (mod_query(mod_getname(hmod,0), MODQ_LOADED|MODQ_NOINCR)) {
         log_printf("%s already loaded!\n", mod_getname(hmod,0));
         return 0;
      }
      mh_qsinit = mod_query(MODNAME_QSINIT, MODQ_NOINCR),
      mh_start  = mod_query(MODNAME_START, MODQ_NOINCR);
      if (!mh_qsinit || !mh_start) {
         log_printf("unable to query system modules?!\n");
         return 0;
      }
      /* import memset() and memcpy() directly, to free it from thunk and
         any possible chaining */
      _memset    = (pf_memset)mod_apidirect(mh_qsinit, ORD_QSINIT_memset);
      _memcpy    = (pf_memcpy)mod_apidirect(mh_qsinit, ORD_QSINIT_memcpy);
      _usleep    = (pf_usleep)mod_apidirect(mh_qsinit, ORD_QSINIT_usleep);
      // by direct pointer too - this is critical START module adjustments,
      mt_startcb = (pf_mtstartcb)mod_apidirect(mh_start, ORD_START_mt_startcb);
      mt_pexitcb = (pf_mtpexitcb)mod_apidirect(mh_start, ORD_START_mt_pexitcb);

      pxcpt_top  = mt_exechooks.mtcb_pxcpttop;

      classid = exi_register("qs_mtlib", methods_list,
         sizeof(methods_list)/sizeof(void*), sizeof(mt_data), 0,
            mt_init, mt_done, 0);
      if (!classid) {
         log_printf("unable to register class!\n");
         return 0;
      }
      // set system flag on self, no any unload from this point
      ((module*)hmod)->flags |= MOD_SYSTEM;
   }
   return 1;
}
