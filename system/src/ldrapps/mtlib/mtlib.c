//
// QSINIT
// multi-tasking implementation module
//
#include "qecall.h"
#include "qsmodext.h"
#include "qsinit_ord.h"
#include "start_ord.h"
#include "mtlib.h"
#include "qcl/qsmt.h"

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
u8t           sched_trace = 0,
              noapic_mode = 0;
u32t            mh_qsinit = 0,
                 mh_start = 0;
u32t           *pxcpt_top = 0; // ptr to xcpt_top in START module (xcption stack)
mtlib_timer      tmr_mode = TmAPIC;

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

   if (apic) {
      apic[APIC_TMRDIV]     = 3;
      apic[APIC_TMRINITCNT] = tick_timer;
   }
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
         !mod_apichain(mh_start, ORD_START_mod_pidlist, APICN_REPLACE, mt_pidlist) ||
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
   if (apic) {
      apic[APIC_TMRDIV]     = 3;
      apic[APIC_LVT_TMR]    = 0x20000|apic_timer_int;
      apic[APIC_TMRINITCNT] = tick_timer;
      apic[APIC_SPURIOUS]   = APIC_SW_ENABLE|INT_SPURIOUS;
   }
   log_it(2, "timer_reenable()\n");
}

static qserr mt_start(void) {
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
         if (stricmp(lp,"SCHT")==0) sched_trace = 1; else
         if (stricmp(lp,"NOAPIC")==0) tmr_mode = TmIrq0;
         ii++;
      }
      free(lst);
   }
   if (!env_on) return E_MT_DISABLED;
   // adjust ticks to default
   sys64 = sys_is64mode();
   if (tick_ms<4 || tick_ms>128) tick_ms = sys64?4:16;
   if (sys64) tmr_mode = TmAPIC;

   apic = 0;
   if (tmr_mode==TmAPIC) {
      apic = (u32t *)sys_getlapic();
      if (!apic) tmr_mode = TmIrq0;
   }
   if (tmr_mode==TmIrq0) {
      // how it can be? but exit, anyway
      if (sys64) {
         log_it(2, "No APIC?\n");
         return E_MT_OLDCPU;
      }
   }
   rdtsc55 = hlp_tscin55ms();
   if (!rdtsc55) {
      log_it(2, "rdtsc calc failed!?\n");
      return E_MT_OLDCPU;
   }
   if (apic) {
      u64t   tscv;
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

      apic_timer_int = INT_TIMER;
   }
   // catch some functions, critical for timer calculation
   rc = catch_functions();
   if (rc) return rc;

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
   } else 
   if (apic) {
      u64t int_tm = ((u64t)get_flatcs()<<32) + (u32t)&timer32,
           int_sp = ((u64t)get_flatcs()<<32) + (u32t)&spurious32;
      
      if (!sys_setint(INT_TIMER, &int_tm, SINT_INTGATE) ||
          !sys_setint(INT_SPURIOUS, &int_sp, SINT_INTGATE))
      {
         log_it(2, "Unable to install 32-bit int vectors!\n");
         return E_MT_TIMER;
      }
      if (!sys_tmirq32((u32t)apic, INT_TIMER, INT_SPURIOUS, 0))
         log_it(0, "sys_tmirq32(apic) failed!\n");
   }
   if (apic) apic[APIC_SPURIOUS] = APIC_SW_ENABLE|INT_SPURIOUS;
   // query active tss - any other will deny task switching in timer
   if (!sys64) main_tss = get_taskreg();
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
   if (apic) {
      apic[APIC_TMRDIV]     = 3;
      apic[APIC_LVT_TMR]    = 0x20000|apic_timer_int;
      // reload it with enabled periodic interrupt!
      apic[APIC_TMRINITCNT] = tick_timer;
   } else {
      // timer32 called at the end of irq0 interrupt
      if (!sys_tmirq32(0, 0, 0, timer32))
         log_it(0, "sys_tmirq32(tmr) failed!\n");
   }
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

/* MTLIB shared class */

static qserr _exicc cmt_initialize  (void *data) {
   if (mt_on) return E_SYS_DONE;
   return mt_start();
}

static u32t  _exicc cmt_state       (EXI_DATA) { return mt_on?1:0; }

static qserr _exicc cmt_waitobject  (EXI_DATA, mt_waitentry *list, u32t count,
                                     u32t glogic, u32t *signaled)
{  return mt_waitobject(list, count, glogic, signaled); }

static mt_tid _exicc cmt_thcreate   (EXI_DATA, mt_threadfunc thread, u32t flags,
                                     mt_ctdata *optdata, void *arg)
{  return mt_createthread(thread, flags, optdata, arg); }

static qserr _exicc cmt_thsuspend   (EXI_DATA, mt_pid pid, mt_tid tid, u32t ms)
{  return mt_suspendthread(pid, tid, ms); }

static qserr _exicc cmt_thresume    (EXI_DATA, u32t pid, u32t tid)
{  return mt_resumethread(pid, tid); }

static qserr _exicc cmt_thstop      (EXI_DATA, mt_pid pid, mt_tid tid, u32t rc)
{  return mt_termthread(pid, tid, rc); }

static void  _exicc cmt_thexit      (EXI_DATA, u32t rc)
{  mt_exitthread(rc); }

static qserr _exicc cmt_execse      (EXI_DATA, u32t module, const char *env,
                                     const char *params, u32t flags, u32t vdev,
                                     mt_pid *ppid, const char *title)
{  return mod_execse(module, env, params, flags, vdev, ppid, title); }

static u32t _exicc cmt_getmodpid    (EXI_DATA, u32t mh, u32t *parent) {
   if (!mh) return 0;
   return pid_by_module((module*)mh, parent);
}

static u32t _exicc cmt_createfiber  (EXI_DATA, mt_threadfunc fiber, u32t flags,
                                     mt_cfdata *optdata, void *arg)
{  return mt_createfiber(fiber, flags, optdata, arg); }

static qserr _exicc cmt_switchfiber (EXI_DATA, u32t fiber, int free)
{  return mt_switchfiber(fiber, free); }

static qserr _exicc cmt_sendsignal  (EXI_DATA, u32t pid, u32t tid, u32t signal,
                                     u32t flags) 
{  return mt_sendsignal (pid, tid, signal, flags); }

static mt_sigcb _exicc cmt_setsignal(EXI_DATA, mt_sigcb cb)
{  return mt_setsignal(cb); }

static qserr _exicc cmt_stop        (EXI_DATA, mt_pid pid, int tree, u32t result)
{  return mod_stop(pid, tree, result); }

static qserr _exicc cmt_checkpidtid (EXI_DATA, mt_pid pid, mt_tid tid, u32t *state)
{  return mt_checkpidtid(pid, tid, state); }

static qserr _exicc cmt_tasklist    (EXI_DATA, u32t devmask)
{  return se_tasklist(devmask); }

static qserr _exicc cmt_shedcmd     (EXI_DATA, clock_t clk, const char *cmd,
                                     qe_eid *eid)
{
   u32t len = cmd ? strlen(cmd) : 0;
   if (!len) return E_SYS_INVPARM;
   if (!mt_on) {
      qserr res = mt_initialize();
      if (res) return res;
   }
   if (mt_on) {
      /* command line is owned by this module, this means that it can be lost
         if thread killed just after this line ;) or user will not free
         event->a after unschedule the event */
      char  *cmc = malloc(len+1);
      qe_eid  id;
      memcpy(cmc, cmd, len+1);
      // sys queue is detached, so we can post into it here
      id = qe_schedule(sys_q, clk, SYSQ_SCHEDAT, (long)cmc, 0, 0);
      // should never occurs if mt_on is ON
      if (!id) {
         free(cmc);
         return E_SYS_SOFTFAULT;
      }
      if (eid) *eid = id;
      return 0;
   }
   return E_MT_DISABLED;
}

static void *methods_list[] = { cmt_initialize, cmt_state, cmt_getmodpid,
   cmt_waitobject, cmt_thcreate, cmt_thsuspend, cmt_thresume, cmt_thstop,
   cmt_thexit, cmt_execse, cmt_createfiber, cmt_switchfiber, cmt_sendsignal,
   cmt_setsignal, cmt_stop, cmt_checkpidtid, cmt_tasklist, cmt_shedcmd };

/* Shell commands */

qserr new_session(str_list *args, u32t flags, u32t devlist, const char *taskname) {
   qserr err = 0;
   if (strlen(args->item[0])>QS_MAXPATH) err = E_SYS_LONGNAME; else {
      u32t lptype;
      char     lp[QS_MAXPATH+1], *parms = 0, *cmd;
      strcpy(lp, args->item[0]);

      lptype = cmd_argtype(lp);
      if (lptype==ARGT_SHELLCMD || lptype==ARGT_BATCHFILE) {
         parms = sprintf_dyn(lptype==ARGT_BATCHFILE?" /c \"%s\"":" /c %s", lp);
         cmd   = env_getvar("COMSPEC", 0);
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

u32t _std shl_stop(const char *cmd, str_list *args) {
   int rc=-1, tree=1, quiet=0;
   if (args->count>0) {
      static char *argstr   = "/s|/q";
      static short argval[] = {  0, 1};
      // help overrides any other keys
      if (str_findkey(args, "/?", 0)) {
         cmd_shellhelp(cmd,CLR_HELP);
         return 0;
      }
      args = str_parseargs(args, 0, SPA_RETLIST, argstr, argval, &tree, &quiet);

      if (args->count>0) {
         u32t ii;
         for (ii=0; ii<args->count; ii++) {
            u32t pid = str2ulong(args->item[ii]);
            if (!pid) {
               if (!quiet) break;
            } else {
               rc = mod_stop(pid, tree, EXIT_FAILURE);
               if (!quiet)
                  if (rc) break; else
                     printf("pid %u stopped\n", pid);
            }
         }
         if (quiet) rc = 0;
      }
      // free args clone, returned by str_parseargs() above
      free(args);
      if (rc>=0) {
         if (rc) cmd_shellerr(EMSG_QS, rc, 0);
         return 0;
      }
   }
   cmd_shellerr(EMSG_CLIB, EINVAL, 0);
   return EINVAL;
}

u32t _std shl_at(const char *cmd, str_list *args) {
   int quiet=0;
   if (args->count>0) {
      int rc=-1, mode=0, pos=0, pause=1;
      char *varname = 0;
      // help overrides any other keys
      if (str_findkey(args, "/?", 0)) {
         cmd_shellhelp(cmd,CLR_HELP);
         return 0;
      }
      /* str_parseargs() cannot be used here since it can find the same key in
         the command to launch */
      while (pos<args->count) {
         char *str = args->item[pos++];
         if (*str!='/' && *str!='-') { pos--; break; } else
            if (stricmp(++str,"q")==0) quiet = 0; else
               if (stricmp(str,"d")==0) mode = 1; else
                  if (stricmp(str,"del")==0) mode = 1; else
                     if (stricmp(str,"list")==0) mode = 2; else
                        if (stricmp(str,"np")==0) pause = 0; else
                           if (strnicmp(str,"v:",2)==0) varname = str+2; else
                              { pos--; break; }
      }
      // invalid arg combination
      if (mode && varname) ; else
      // no error
      if (pos<args->count) {
         if (mode==0) {
            u64t cldiff = 0;
            if (args->item[pos][0]=='+') {
               u32t  diff;
               char    dt; 
               if (sscanf(args->item[pos]+1, "%u%c", &diff, &dt)==2) {
                  cldiff = diff;
                  switch (toupper(dt)) {
                     case 'D': cldiff *= 24;
                     case 'H': cldiff *= 60;
                     case 'M': cldiff *= 60;
                     case 'S': cldiff *= CLOCKS_PER_SEC;
                               break;
                     default : pos = -1;
                  }
                  // error?
                  if (pos<0) cldiff = 0; else pos++;
               }
            } else {
               time_t    now = time(0);
               struct tm tme;
               localtime_r(&now, &tme);
               // check for time string
               if (isdigit(args->item[pos][0])) {
                  if (strccnt(args->item[pos],'.')==2) {
                     if (sscanf(args->item[pos], "%u.%u.%4u", &tme.tm_mday,
                        &tme.tm_mon, &tme.tm_year)==3 && tme.tm_mday<=31 &&
                           tme.tm_mon<13 && tme.tm_year>1900)
                     {
                        tme.tm_year -= 1900;
                        if (tme.tm_mon) tme.tm_mon--;
                        pos++;
                     } else {
                        if (!quiet) printf("Date format is invalid\n");
                        pos=-1; quiet=1;
                     }
                  }
                  if (pos>=0) {
                     int ddot = strccnt(args->item[pos],':'), acnt;
                     if (ddot==1 || ddot==2) {
                        tme.tm_sec = 0;
                        acnt = sscanf(args->item[pos], "%2u:%2u:%2u", &tme.tm_hour,
                                      &tme.tm_min, &tme.tm_sec);
                        if ((acnt==2 || acnt==3) && tme.tm_hour<24 && tme.tm_min<60
                           && tme.tm_sec<60) pos++; else {
                              if (!quiet) printf("Time format is invalid\n");
                              pos=-1; quiet=1;
                           }
                     }
                  }
               }
               if (pos>=0) {
                  time_t dst = mktime(&tme);
                  cldiff = dst<=now ? 1LL : (u64t)(dst-now)*CLOCKS_PER_SEC;
               }
            }
            // time is valid
            if (cldiff)
               if (pos<args->count) {
                  qe_eid    eid;
                  str_list *lst = str_copylines(args, pos, args->count);
                  char    *lcmd = str_mergeargs(lst);
                  free(lst);
                  rc = cmt_shedcmd(0,0, sys_clock()+cldiff, lcmd, &eid);
                  free(lcmd);
                  
                  if (!rc && !quiet)
                     if (eid!=QEID_POSTED) printf("Command scheduled with id %u\n", eid);
                        else printf("Command launched since time is passed\n");
                  // set shell variable
                  if (varname)
                     if (rc || eid==QEID_POSTED) unsetenv(varname); else {
                        char id[24];
                        ultoa(eid, id, 10);
                        setenv (varname, id, 1);
                     }
               } else
               if (!quiet) {
                  printf("Command line is empty!\n");
                  quiet=1;
               }
         } else
         if (mode==1) {
            u32t id = str2ulong(args->item[pos]);
            if (id) {
               // this is memory addr, something like at /d 10 will just hang us
               qe_event *ev = id>_16MB ? qe_unschedule(id) : 0;
               if (ev) {
                  // check that ev->a is a real heap block
                  if (ev->a && mem_getobjinfo((void*)ev->a,0,0)) free((void*)ev->a);
                  free(ev);
               } else
               if (!quiet) printf("There is no command with such ID!\n");
               rc = 0;
            }
         }
      } else
      if (mode==2) {
         // list all tasks
         ptr_list   tl = 0;
         u32t       ii;
         if (mt_on) {
            qe_eid  *list;
            // lock it to get solid data
            mt_swlock();
            list = hlp_qslist(sys_q);
            if (list) {
               ii = 0;
               while (list[ii]) {
                  qe_event *ev = qe_getschedule(list[ii]);
                  if (ev && ev->code==SYSQ_SCHEDAT) {
                     char tstr[32];
                     int  flen;
                     qsclock now = sys_clock();
            
                     if (ev->evtime<=now) strcpy(tstr, "passed"); else {
                        u32t diff = (ev->evtime - now) / CLOCKS_PER_SEC;
                        if (diff/10==0) strcpy(tstr, "< 10 sec"); else
                        if (diff<120) sprintf(tstr, "%u sec", diff); else
                        if (diff/60<120) sprintf(tstr, "%u min", diff/60); else {
                           struct tm ttm, nowtm;
                           time_t   tnow = time(0),
                                      tt = tnow + diff;
                           int   sameday;
            
                           localtime_r(&tt, &ttm);
                           localtime_r(&tnow, &nowtm);
                           sameday = nowtm.tm_year==ttm.tm_year && nowtm.tm_mon==
                                     ttm.tm_mon && nowtm.tm_mday==ttm.tm_mday;
                           strftime(tstr, 32, sameday?"%T":"%d.%m.%Y %T", &ttm);
                        }
                     }
                     // align time to center
                     flen = 20 - strlen(tstr) >> 1;
                     while (flen--) strcat(tstr, " ");
            
                     if (!tl) tl = NEW(ptr_list);
                     tl->add(sprintf_dyn("%10u � %20s � %s", list[ii], tstr, ev->a));
                  }
                  ii++;
               }
            }
            mt_swunlock();
            if (list) free(list);
         }

         if (tl) {
            cmd_printseq(0, pause?PRNSEQ_INIT:PRNSEQ_INITNOPAUSE, 0);
            cmd_printf(//" 컴컴컴컴컴쩡컴컴컴컴컴컴컴컴컴컴컫컴컴컴컴컴컴컴컴컴컴컴컴� \n"
                       "     id    �       at time        � command \n"
                       " 컴컴컴컴컴탠컴컴컴컴컴컴컴컴컴컴컵컴컴컴컴컴컴컴컴컴컴컴컴� \n");
            for (ii=0; ii<tl->count(); ii++) cmd_printf("%s\n", tl->value(ii));

            tl->freeitems(0,FFFF);
            DELETE(tl);
         } else
            printf("There are no scheduled commands\n");
         rc = 0;
      } else
      if (mode==1) {
         // enum system queue for SYSQ_SCHEDAT and delete all entries
         u32t  cnt = 0;
         if (mt_on) {
            qe_eid *list;
            mt_swlock();
            list = hlp_qslist(sys_q);
            if (list) {
               u32t ii = 0;
               while (list[ii]) {
                  qe_event *ev = qe_getschedule(list[ii]);
                  if (ev && ev->code==SYSQ_SCHEDAT) {
                     if ((ev = qe_unschedule(list[ii]))) {
                        if (ev->a) free((void*)ev->a);
                        free(ev);
                        cnt++;
                     }
                  }
                  ii++;
               }
            }
            mt_swunlock();
            if (list) free(list);
         }
         if (!quiet)
            if (cnt) printf("%u schedule%s deleted\n", cnt, cnt==1?"":"s");
               else printf("There are no scheduled commands\n");
         rc = 0;
      }

      if (rc>=0) {
         if (rc && !quiet) cmd_shellerr(EMSG_QS, rc, 0);
         return 0;
      }
   }
   if (!quiet) cmd_shellerr(EMSG_CLIB, EINVAL, 0);
   return EINVAL;
}

unsigned __cdecl LibMain(unsigned hmod, unsigned termination) {
   if (!termination) {
      if (mod_query(mod_getname(hmod,0), MODQ_LOADED|MODQ_NOINCR)) {
         log_printf("%s already loaded!\n", mod_getname(hmod,0));
         return 0;
      }
      if (sizeof(_qs_mtlib)!=sizeof(methods_list))
         _throwmsg_("mtlib class: size mismatch");

      mh_qsinit = mod_query(MODNAME_QSINIT, MODQ_NOINCR),
      mh_start  = mod_query(MODNAME_START, MODQ_NOINCR);
      if (!mh_qsinit || !mh_start) {
         log_printf("unable to query system modules?!\n");
         return 0;
      }
      /* import some functions directly, to free it from thunks and any
         possible chaining.
         Use of TRACE on these calls is really FATAL */
      _memset       = (pf_memset)   mod_apidirect(mh_qsinit, ORD_QSINIT_memset, 0);
      _memcpy       = (pf_memcpy)   mod_apidirect(mh_qsinit, ORD_QSINIT_memcpy, 0);
      _usleep       = (pf_usleep)   mod_apidirect(mh_qsinit, ORD_QSINIT_usleep, 0);
      _sys_selquery = (pf_selquery) mod_apidirect(mh_qsinit, ORD_QSINIT_sys_selquery, 0);
      _se_dataptr   = (pf_sedataptr)mod_apidirect(mh_start , ORD_START_se_dataptr, 0);
      mt_startcb    = (pf_mtstartcb)mod_apidirect(mh_start , ORD_START_mt_startcb, 0);
      mt_pexitcb    = (pf_mtpexitcb)mod_apidirect(mh_start , ORD_START_mt_pexitcb, 0);
      fpu_intdata   = (qs_fpustate )mod_apidirect(mh_start , ORD_START_fpu_statedata, 0);
      // check it!
      if (!_memset || !_memcpy || !_usleep || !_sys_selquery || !mt_startcb ||
         !mt_pexitcb || !fpu_intdata || !_se_dataptr)
      {
         log_printf("unable to import direct API!\n");
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
      cmd_shelladd("STOP", shl_stop);
      cmd_shelladd("AT", shl_at);
   }
   return 1;
}
