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

int                 mt_on = 0;
int                 sys64 = 0;
u32t              classid = 0; // class id
u32t                *apic = 0; // pointer to LAPIC
u32t              cmstate = 0; // current intel's clock modulation state
u32t             dump_lvl = 0;
u8t        apic_timer_int = 0;
u8t            *tmstack32 = 0;
u64t              rdtsc55 = 0;
u32t               tics55 = 0; // APIC timer/16 tics in 55 ms
u32t              tick_ms = 0; // in ms
u64t           tsc_100mks = 0;
u32t            tsc_shift = 0; // shr for TSC value to sched. counter (value <1ms)
u64t           tick_rdtsc = 0; // in rdtsc counters
u32t           tick_timer = 0; // in APIC timer ticks
volatile u64t  next_rdtsc = 0;
u16t             main_tss = 0; // main tss (or 0 in 64-bit or 32-bit safe mode)
pf_memset         _memset = 0;
pf_memcpy         _memcpy = 0;
pf_usleep         _usleep = 0;
pf_mtstartcb   mt_startcb = 0;
pf_mtpexitcb   mt_pexitcb = 0;
qs_fpustate   fpu_intdata = 0;
pf_selquery _sys_selquery = 0;
pf_sedataptr  _se_dataptr = 0;
qshandle            sys_q = 0;
u8t           sched_trace = 0;
u32t            mh_qsinit = 0,
                 mh_start = 0;
u32t           *pxcpt_top = 0; // ptr to xcpt_top in START module (xcption stack)

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
      /* this is not critical, actually. Intel`s clock modulation has no effect
         on timer values, it just called for order */
      log_it(2, "sys_notifyevent() error!\n");
   }
   return 0;
}

void timer_reenable(void) {
   apic[APIC_TMRDIV]     = 3;
   apic[APIC_LVT_TMR]    = 0x20000|apic_timer_int;
   apic[APIC_TMRINITCNT] = tick_timer;
   apic[APIC_SPURIOUS]   = APIC_SW_ENABLE|INT_SPURIOUS;
   log_it(2, "timer_reenable()\n");
}

static qserr mt_start(void) {
   u64t    tscv;
   u32t     tmv, rc;
   char  *mtkey = getenv("MTLIB");
   u32t      ii;
   int   env_on = 1;

   /* parse env key:
      "MTLIB = ON [, TIMERES = 4][, DUMPLVL = 1" */
   if (mtkey) {
      str_list *lst = str_split(mtkey, ",");
      env_on = env_istrue("MTLIB");
      ii     = 1;
      while (ii<lst->count) {
         char *lp = lst->item[ii];
         // str_split() gracefully trims spaces around "=" for us
         if (strnicmp(lp,"TIMERES=",8)==0) tick_ms  = strtoul(lp+8, 0, 0); else
         if (strnicmp(lp,"DUMPLVL=",8)==0) dump_lvl = strtoul(lp+8, 0, 0); else
         if (strnicmp(lp,"SCHT",4)==0) sched_trace = 1;
         ii++;
      }
      free(lst);
   }
   if (!env_on) return E_MT_DISABLED;
   // adjust ticks to default
   sys64 = sys_is64mode();
   if (tick_ms<4 || tick_ms>128) tick_ms = sys64?4:16;

   apic  = (u32t *)sys_getlapic();
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

   if (sys64) {
      u64t rc;
      sys_tmirq64(timer64cb);
      // set 64-bit irq vectors (special code hosted in EFI binary)
      rc = call64(EFN_TIMERIRQON, 0, 3, (u32t)apic, INT_TIMER, INT_SPURIOUS);
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
   // launch system idle thread
   start_idle_thread();
   /* inform START about MT is happen ;)
      this call is very important, it creates mutexes for some API parts and
      calls registered notifications about MT mode (this, also, mean possible
      mt_createthread() in it) */
   if (mt_startcb) mt_startcb(mux_hlpinst);
   // we are done
   return 0;
}

qserr _exicc cmt_initialize(void *data) {
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

qserr _exicc cmt_threadname(EXI_DATA, const char *str) { return mt_threadname(str); }

qserr _exicc cmd_execse(EXI_DATA, u32t module, const char *env, const char *params,
                        u32t flags, u32t vdev, mt_pid *ppid, const char *title)
{ 
   return mod_execse(module, env, params, flags, vdev, ppid, title);
}

u32t  _exicc cmt_getmodpid(EXI_DATA, u32t mh) {
   if (!mh) return 0;
   return pid_by_module((module*)mh);
}

static void *methods_list[] = { cmt_initialize, cmt_state, cmt_getmodpid,
   cmt_waitobject, cmt_createthread, cmt_resumethread, cmt_threadname, cmd_execse };

qserr new_session(str_list *args, u32t flags, u32t devlist, const char *taskname) {
   qserr err = 0;
   if (strlen(args->item[0])>QS_MAXPATH) err = E_SYS_LONGNAME; else {
      u32t lptype;
      char     lp[QS_MAXPATH+1], *parms = 0, *cmd;
      strcpy(lp, args->item[0]);

      lptype = cmd_argtype(lp);
      if (lptype==ARGT_SHELLCMD || lptype==ARGT_BATCHFILE) {
         parms = sprintf_dyn(lptype==ARGT_BATCHFILE?" /c \"%s\"":" /c %s", lp);
         cmd   = env_getvar("COMSPEC");
         strncpy(lp, cmd?cmd:"CMD.EXE", QS_MAXPATH);
         if (cmd) free(cmd);
         lp[QS_MAXPATH] = 0;
      } else
      if (lptype!=ARGT_FILE) err = E_SYS_INVNAME;

      if (!err) {
         u32t modh;
         // zero first arg - it replaced by full module path or moved to parms
         args->item[0][0] = 0;
         cmd   = str_mergeargs(args);
         parms = strcat_dyn(parms, cmd);
         modh  = lp[1]==':'?mod_load(lp,0,&err,0):mod_searchload(lp,0,&err);
         free(cmd);

         if (modh)
            err = mod_execse(modh, 0, parms, flags, devlist, 0, taskname);
      }
   }
   return err;
}

u32t _std shl_detach(const char *cmd, str_list *args) {
   if (args->count==1 && strcmp(args->item[0],"/?")==0) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }
   if (args->count>0) {
      qserr  err = new_session(args, QEXS_DETACH, 0, 0);
      if (err) cmd_shellerr(EMSG_QS, err, 0);
      err = qserr2errno(err);
      env_setvar("ERRORLEVEL", err);
      return err;
   }
   cmd_shellerr(EMSG_CLIB, EINVAL, 0);
   return EINVAL;
}

u32t _std shl_start(const char *cmd, str_list *args) {
   if (args->count==1 && strcmp(args->item[0],"/?")==0) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }
   if (args->count>0) {
      u32t       flags = 0, ii = 0, devlist = 0;
      str_list *cmlist = 0;
      char   *taskname = 0;

      while (ii<args->count) {
         if (args->item[ii][0]=='/') {
            char ch = toupper(args->item[ii][1]);

            if (ch=='D' && args->item[ii][2]==':') {
               int  len = strlen(args->item[ii]+3);
               u64t val;
               if (!len || len>12) {
                  cmd_shellerr(EMSG_QS, len?E_SYS_TOOLARGE:E_SYS_INVPARM, 0);
                  return EINVAL;
               }
               val = strtoull(args->item[ii]+3, 0, 32);
               while (len--) {
                  devlist |= 1<<(val&0x1F);
                  val    >>= 5;
               }
            } else
            if (strlen(args->item[ii])==2) {
               switch (ch) {
                  case 'T': taskname = args->item[++ii]; break;
                  case 'I': flags|= QEXS_BOOTENV; break;
                  case 'B': flags|= QEXS_BACKGROUND; break;
                  default: ch = 0;
               }
            } else ch = 0;
            // invalid key ?
            if (!ch) break; else ii++;
         } else break;
      }
      if (ii>0) cmlist = str_copylines(args, ii, args->count-1);
      if ((cmlist?cmlist:args)->count) {
         qserr  err = new_session(cmlist?cmlist:args, flags, devlist, taskname);
         if (cmlist) free(cmlist);
         if (err) cmd_shellerr(EMSG_QS, err, 0);
         err = qserr2errno(err);
         env_setvar("ERRORLEVEL", err);
         return err;
      }
   }
   cmd_shellerr(EMSG_CLIB, EINVAL, 0);
   return EINVAL;
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
      /* import some functions directly, to free it from thunks and any
         possible chaining.
         Use of TRACE on these calls is really FATAL */
      _memset       = (pf_memset)   mod_apidirect(mh_qsinit, ORD_QSINIT_memset);
      _memcpy       = (pf_memcpy)   mod_apidirect(mh_qsinit, ORD_QSINIT_memcpy);
      _usleep       = (pf_usleep)   mod_apidirect(mh_qsinit, ORD_QSINIT_usleep);
      _sys_selquery = (pf_selquery) mod_apidirect(mh_qsinit, ORD_QSINIT_sys_selquery);
      _se_dataptr   = (pf_sedataptr)mod_apidirect(mh_start , ORD_START_se_dataptr);
      mt_startcb    = (pf_mtstartcb)mod_apidirect(mh_start , ORD_START_mt_startcb);
      mt_pexitcb    = (pf_mtpexitcb)mod_apidirect(mh_start , ORD_START_mt_pexitcb);
      fpu_intdata   = (qs_fpustate )mod_apidirect(mh_start , ORD_START_fpu_statedata);
      // check it!
      if (!_memset || !_memcpy || !_usleep || !_sys_selquery || !mt_startcb ||
         !mt_pexitcb || !fpu_intdata || !_se_dataptr)
      {
         log_printf("unable to import APIs!\n");
         return 0;
      }
      pxcpt_top   = mt_exechooks.mtcb_pxcpttop;
      // shared class for indirect import of MTLIB functions by side code
      classid     = exi_register("qs_mtlib", methods_list,
                                 sizeof(methods_list)/sizeof(void*),0,0,0,0,0);
      if (!classid) {
         log_printf("unable to register class!\n");
         return 0;
      }
      // set system flag on self, no any unload starting from this point
      ((module*)hmod)->flags |= MOD_SYSTEM;
      // some nice commands ;)
      cmd_shelladd("DETACH", shl_detach);
      cmd_shelladd("START", shl_start);
   }
   return 1;
}
