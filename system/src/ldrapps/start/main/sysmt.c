//
// QSINIT "start" module
// mutex (handle) processing
// -------------------------------------
//

#include "qsint.h"
#include "syslocal.h"
#include "qsinit_ord.h"
//#include "qsvdata.h"
#include "setjmp.h"
#include "qsxcpt.h"
#include "cpudef.h"
#include "qstask.h"
#include "qserr.h"
#include "qssys.h"
#include "sysio.h"
#include "dpmi.h"
#include "qcl/qsextdlg.h"

#define TLS_GROW_SIZE         64       ///< TLS array one time grow size

typedef void  _std (*pf_beep) (u16t freq, u32t ms);
static qs_mtlib            mtlib = 0;
static int              kbask_ms = 0,
                      memstat_on = 0;
u8t                    in_mtmode = 0;
qs_sysmutex                mtmux = 0;
qshandle                   sys_q = 0;
u64t              memstat_period = CLOCKS_PER_SEC*120;
static pf_beep             _beep = 0;  ///< original vio_beep call
u32t                     sys_con = 0;  ///< "system console" session
u32t                    ctrln_d0 = 0;

extern module *     _std      mod_ilist;  // and this
extern mt_proc_cb   _std   mt_exechooks;  // and also this ;)

void   setup_loader_mt(void);
void   setup_pagman_mt(void);
void   setup_exi_mt   (void);
void   trace_pid_done (void);

void _std v_beep(u16t freq, u32t ms) {
   if (freq>16384 || (int)ms<0) return;
   qe_postevent(sys_q, SYSQ_BEEP, freq, ms, 0);
}

static qserr _std muxcvtfunc(qshandle mutex, mux_handle_int *res) {
   if (!res) return E_SYS_ZEROPTR; else {
      sft_entry *fe;
      qserr  rc = io_check_handle(mutex, IOHT_MUTEX|IOHT_EVENT, 0, &fe);
      *res = rc?0:fe->muxev.mh;
      // dec MT lock, was incremented by success io_check_handle()
      if (!rc) mt_swunlock();
      return rc;
   }
}

/// free session module after exit
static void session_free(mt_prcdata *pd) {
   mod_secondary->exit_cb(pd->piContext, pd->piExitCode);
   mt_exechooks.mtcb_fini(pd->piContext);
   mt_safedand(&pd->piModule->flags, ~MOD_EXECPROC);
   mod_free((u32t)pd->piModule);
}


/** process system queue requests.
    Everything here should not use FPU (because no_fpu flag in the creation of
    this thread below). */
static void process_sysq(qe_event *ev) {
   switch (ev->code) {
      case SYSQ_SESSIONFREE: {
         mt_prcdata *pd = (mt_prcdata*)ev->a;
         u32t      rpid = pd->piMiscFlags&PFLM_CHAINEXIT?pd->piPID:0;
         session_free(pd);
         /* a bit ugly way to implement chain process - we suspend it until
            release of a predecessor and resume here - else session_free()
            will close handles of a new process too! */
         if (rpid) mtlib->thresume(rpid,1);
         break;
      }
      case SYSQ_BEEP:
         // beep is async, but all requests are just ordered here
         if (_beep) _beep(ev->a,ev->b);
         break;
      case SYSQ_MEMSTAT:
         mem_stat();
         qe_schedule(sys_q, sys_clock()+memstat_period, SYSQ_MEMSTAT, 0, 0, 0);
         break;
      case SYSQ_DCNOTIFY:
         // cache timer in MT mode
         sys_dccommit(DCN_TIMER);
         qe_schedule(sys_q, sys_clock()+CLOCKS_PER_SEC, SYSQ_DCNOTIFY, 0, 0, 0);
         break;
      case SYSQ_FREE: {
         void *block = (void*)ev->a;
         if (ev->b) {
            if (mem_getobjinfo(block,0,0)) free(block); else
               log_it(3,"invalid block in SYSQ_FREE: %X, sender: %X\n", block, ev->c);
         } else
            hlp_memfree(block);
         break;
      }
      case SYSQ_ALARM:
         mtlib->sendsignal(ev->a, ev->b, QMSV_ALARM, 0);
         break;
      case SYSQ_SCHEDAT:
         log_it(3,"AT task (%s)!\n", ev->a);
         if (ev->a) {
            cmd_shellcall(cmd_shellrmv("START", 0), (char*)ev->a, 0);
            free((char*)ev->a);
            ev->a = 0;
         }
         break;
      default:
         log_it(3,"unknown sys_q event %X %X %X %X!\n", ev->code, ev->a, ev->b, ev->c);
   }
}

extern void vh_input_read_loop(void);

/** system service thread.
    1. keyboard poll & push keys to the current session.
    2. process async sys_q queue commands.
    3. unzip delayed LDI entries on start
*/
static u32t _std sys_servicethread(void *arg) {
   s64t     kbask_clk;
   mt_thrdata    *pth;
   qserr           rc;

   mt_getdata(0, &pth);
   // add TFLM_SYSTEM bit to thread flags
   mt_safedor(&pth->tiMiscFlags, TFLM_SYSTEM);

   mt_threadname("service");
   // unzip LDI
   unzip_delaylist();
   // dump memstat once per 2 minutes
   if (memstat_on) {
      if (memstat_on>1) memstat_period = CLOCKS_PER_SEC*memstat_on;
      qe_schedule(sys_q, sys_clock()+memstat_period, SYSQ_MEMSTAT, 0, 0, 0);
   }
   // keyboard poll period
   if (kbask_ms<4 || kbask_ms>128) kbask_ms = hlp_hosttype()==QSHT_EFI?8:18;
   kbask_clk = kbask_ms*1000;
   // log it
   log_it(3, "memstat %s, kb poll = %i ms!\n", memstat_on?"on":"off", kbask_ms);
   /* switch this thread into a separate invisible console.
      This console used, at least for memmgr error message during session
      exit. I.e. mod_exitcb() validation calls may produce error message
      screen. It showed in the current console for common modules, but for
      detached ones it requires a place to show. And here it is. */
   rc = se_newsession(se_devicemask(), 0, "System Console", VSF_NOLOOP|VSF_HIDDEN);
   if (rc) log_it(1, "unable to create session - %X!\n", rc); else
      sys_con = se_sesno();
   // post 1st cache commit event
   qe_postevent(sys_q, SYSQ_DCNOTIFY, 0, 0, 0);

   while (1) {
      u64t nextclk = sys_clock() + kbask_clk;
      qe_event *ev = qe_waitevent(sys_q, kbask_ms);

      // loop queue events until at least 3/4 of kbask_ms
      while (ev) {
         s64t tdiff;

         process_sysq(ev); free(ev); ev = 0;

         tdiff = nextclk - sys_clock();
         // signed check - <1/4 of kbask || <0
         if (tdiff>=kbask_clk>>2) ev = qe_waitevent(sys_q, tdiff/1000);
      }
      vh_input_read_loop();
   }
   return 0;
}

/// Alt-Esc & Ctrl-N processing callback (task switch on the device where it was pressed).
static void _std cb_taskswitch(sys_eventinfo *kd) {
   /* push Ctrl-N back to the active session queue if it was pressed on
      device 0 (screen) */
   if (!ctrln_d0 && (u16t)kd->info==0x310E && kd->info2==0) {
      se_keypush(se_foreground(), 0x310E, kd->info>>16);
      return;
   }
   se_selectnext(kd->info2);
}

/** Ctrl-Esc & Ctrl-B processing callback
    Function shows task list on the device where it was pressed. */
static void _std cb_tasklist(sys_eventinfo *kd) {
   // push Ctrl-B back on device 0 (screen)
   if (!ctrln_d0 && (u16t)kd->info==0x3002 && kd->info2==0) {
      se_keypush(se_foreground(), 0x3002, kd->info>>16);
      return;
   } else {
      qs_mtlib mt = get_mtlib();
      if (mt) {
         if (!in_mtmode) mt->initialize();
         if (in_mtmode) mt->tasklist(1<<kd->info2);
      }
   }
}

/** callback from MT lib - MT mode is ready.
    Called at the end of mt_initialize() - before returning to user, so
    we have fully functional threading here, but still no threads except
    "system idle" :) */
void _std mt_startcb(qs_sysmutex mproc) {
   mt_ctdata  ctd;
   qserr      err;
   // and we need this pointer too ;)
   if (!mtlib) get_mtlib();
   /* create & detach service queue.
      Detached state allows access from any thread, calls like
      qe_unschedule() and so on */
   if (qe_create(0,&sys_q)) log_it(0, "sys_q init error!\n"); else
      io_setstate(sys_q, IOFS_DETACHED, 1);
   // call back to the MT lib
   mtmux     = mproc;
   mtmux->init(muxcvtfunc, qe_available_spec, sys_q, qe_getschedlist);
   in_mtmode = 1;
   // create mutexes for QSINIT binary (module loader & micro-FSD access)
   setup_loader_mt();
   // create mutexes for page allocation code
   setup_pagman_mt();
   // create mutexes for exi instances
   setup_exi_mt();
   // catch vio_beep
   _beep         = (pf_beep)mod_apidirect(mh_qsinit, ORD_QSINIT_vio_beep, 0);
   mod_apichain(mh_qsinit, ORD_QSINIT_vio_beep, APICN_REPLACE, v_beep);
   // catch alt-esc and ctrl-n
   err = sys_sethotkey(0x0100, KEY_ALT, SECB_GLOBAL, cb_taskswitch);
   if (!err) err = sys_sethotkey(0x310E, KEY_CTRL, SECB_GLOBAL, cb_taskswitch);
   if (err) log_it(0, "hotkeys(n) error (%X)!\n", err);
   /* read vars for service thread (must be here because thread itself has
      pid 1 environment) */
   kbask_ms      = env_istrue("KBRES");
   memstat_on    = env_istrue("MEMSTAT");
   if (memstat_on<0) memstat_on = 0;
   // service thread
   memset(&ctd, 0, sizeof(mt_ctdata));
   ctd.size      = sizeof(mt_ctdata);
   ctd.stacksize = 32768;
   ctd.pid       = 1;
   mtlib->thcreate(sys_servicethread, MTCT_NOFPU|MTCT_NOTRACE, &ctd, 0);
   // run notifications about MT mode
   sys_notifyexec(SECB_MTMODE, 0);
}

void setup_tasklist(void) {
   // catch ctrl-esc and ctrl-b
   qserr err = sys_sethotkey(0x011B, KEY_CTRL, SECB_GLOBAL, cb_tasklist);
   if (!err) err = sys_sethotkey(0x3002, KEY_CTRL, SECB_GLOBAL, cb_tasklist);
   if (err) log_it(0, "hotkeys(l) error (%X)!\n", err);
}

/** callback from MT lib - thread exit.
    stage==0: This is the last call in thread context, next will be releasing
              mutexes and exit.
              In non-MT mode mod_exec calls it too, on exit, with td=0 arg
    stage==1: Called when thread is gone.
              In non-MT mode mod_exec calls it too, with valid td arg */
void _std mt_pexitcb(mt_thrdata *td, int stage) {
   /* processing of exiting thread -- */
   if (!stage) {
      u32t pid = td?td->tiPID:mod_getpid();
      /* main thread exit / non-MT mode.
         This case must be the last for the process, because main thread
         waits for secondaries and only then goes here */
      if (!td || td->tiTID==1) {
         // non-MT - reset FPU owner!
         if (!td) fpu_strest();
         //log_it(0, "pid %u exit cb!\n", pid);
         /* call it here for the main thread, else notification code can be
            invoked in parent context after process exit */
         sys_notifyfree(pid);
         // as a company
         sys_hotkeyfree(pid);
         // as well
         se_sigfocus(0);
         // trace process exit hook
         trace_pid_done();
         /* here too - this will free all instances before the moment of imported
            DLL mod term function. I.e. if we will call it after DLL fini - DLL
            can deny unload if it provides a class for us */
         exi_free_as(pid);
      }
      // in MT mode only
      if (td) {
         // unschedule alarm() event delivery
         u32t alarm_ev = mt_tlsget(QTLS_ALARMQH);
         if (alarm_ev) {
            void *ev = qe_unschedule(alarm_ev);
            if (ev) free(ev);
         }
         /* if we owning mfs_mutex - hlp_fclose() must be called to reset
            mutex & flags in mfs i/o functions */
         if (!mt_muxstate(mod_secondary->mfs_mutex, 0)) hlp_fclose();
         /* if we owning ldr_mutex - then trap screen is too close to us now,
            but still tries to clear mod_ilist at least. */
         if (!mt_muxstate(mod_secondary->ldr_mutex, 0)) {
             if (mod_ilist) {
                log_it(0, "mod_ilist was lost!\n");
                mod_ilist = 0;
             }
         }
         // disable any vio output
         td->tiSession = SESN_DETACHED;
         // and check sessions for empty ones after that
         se_closeempty();
      }
   /* processing of finished thread -- */
   } else {
      /* note, that at this moment system may have ANOTHER ONE pid/tid pair
         equal to td arg (after mod_chain()). So, only td pointer value
         MUST be the key to process. */

      // release lost entries in exit chain stacks
      chain_thread_free(td);
      // release thread owned memory
      mem_freepool(QSMEMOWNER_COTHREAD, (u32t)td);
   }
}

void panic_no_memmgr(const char *msg) {
   u32t sesno = se_sesno();
   if (in_mtmode) se_switch(sesno, FFFF);

   vio_resetmode();
   vio_setcolor(VIO_COLOR_LRED);
   while (*msg)
      if (vio_charout(*msg++)) { vio_charout('\xDD'); vio_charout(' '); }
   vio_setcolor(VIO_COLOR_RESET);
   if (sesno!=sys_con) key_wait(60);
}

qs_mtlib get_mtlib(void) {
   if (!mtlib) {
      mtlib = NEW_G(qs_mtlib);
      // disable trace for this instance
      if (mtlib) exi_chainset(mtlib, 0);
   }
   // this can be NULL if no MTLIB.DLL in LDI archive
   return mtlib;
}

u32t _std mt_initialize(void) {
   qs_mtlib mt = get_mtlib();
   if (!mt) return E_SYS_NOFILE;
   // note, what we callbacked to mt_startcb() above during this call
   return mt->initialize();
}

u32t _std mt_active(void) {
   // MT mode start callback will always set this pointer
   if (!mtlib) return 0; else {
      qs_mtlib mt = get_mtlib();
      return mt ? mt->active() : 0;
   }
}

/// get tid stub, will be replaced by MTLIB on its load.
u32t _std START_EXPORT(mt_getthread)(void) {
   return 1;
}

/** get thread comment.
    note, what result is up to 16 chars & NOT terminated by zero if
    length is full 16 bytes.
    @return direct pointer to TLS variable with thread comment */
char *mt_getthreadname(void) {
   static char *tnb = "";
   if (in_mtmode) {
      char *tnstr;
      if (!mt_tlsaddr(QTLS_COMMENT, (u64t**)&tnstr)) return tnstr;
   }
   return tnb;
}

// -------------------------------------------------------------
//   M u t e x e s / E v e n t s
// -------------------------------------------------------------

static qserr mt_commoncreate(int initialstate, const char *name, qshandle *res,
                             u32t namespc, u32t type, u32t createarg)
{
   qserr  rc = 0;
   u32t  fno;
   if (!res) return E_SYS_ZEROPTR;
   if (name && !*name) name = 0;
   *res = 0;
   // need to start MT first!
   if (!in_mtmode) return E_MT_DISABLED;

   mt_swlock();
   fno  = sft_find(name, namespc);

   if (fno) rc = E_MT_DUPNAME; else {
      mux_handle_int mhi = 0;
      u32t           pid = mod_getpid();
      u32t          ifno = ioh_alloc(),
                     fno = sft_alloc();
      sft_entry      *fe = 0;

      if (!ifno || !fno) rc = E_SYS_NOMEM; else
         rc = mtmux->create(&mhi, fno, createarg, initialstate);
      if (!rc) {
         fe = (sft_entry*)malloc(sizeof(sft_entry));
         fe->sign       = SFTe_SIGN;
         fe->type       = type;
         fe->fpath      = name?strdup(name):0;
         fe->name_space = namespc;
         fe->pub_fno    = SFT_PUBFNO+fno;
         fe->muxev.mh   = mhi;
         fe->ioh_first  = 0;
         fe->broken     = 0;
         // add new entries
         sftable[fno]   = fe;
         sft_setblockowner(fe, fno);
      }

      if (!rc) {
         ioh_setuphandle(ifno, fno, pid);
         if (!rc) *res = IOH_HBIT+ifno;
      }
   }
   mt_swunlock();
   return rc;
}

static qserr mt_commonopen(const char *name, qshandle *res, u32t namespc, u32t type) {
   qserr  rc = 0;
   u32t  fno;
   if (!res) return E_SYS_ZEROPTR;
   *res = 0;
   if (name && !*name) name = 0;
   if (!name) return E_SYS_INVPARM;
   // no MT - no mutexes!
   if (!in_mtmode) return E_SYS_NOPATH;

   mt_swlock();
   fno  = sft_find(name, namespc);

   if (!fno) rc = E_SYS_NOPATH; else {
      mux_handle_int mhi = 0;
      u32t           pid = mod_getpid();
      u32t          ifno = ioh_alloc();
      sft_entry      *fe = sftable[fno];

      if (!ifno) rc = E_SYS_NOMEM; else
         if (fe->type!=type) rc = E_SYS_INVHTYPE;

      if (!rc) {
         ioh_setuphandle(ifno, fno, pid);
         if (!rc) *res = IOH_HBIT+ifno;
      }
   }
   mt_swunlock();
   return rc;
}

qserr _std mt_muxcreate(int initialowner, const char *name, qshandle *res) {
   return mt_commoncreate(initialowner, name, res, NAMESPC_MUTEX, IOHT_MUTEX, 0);
}

qserr _std mt_muxopen(const char *name, qshandle *res) {
   return mt_commonopen(name, res, NAMESPC_MUTEX, IOHT_MUTEX);
}

qserr _std mt_muxrelease(qshandle mtx) {
   sft_entry   *fe;
   qserr       err;
   if (!in_mtmode) return E_SYS_INVPARM;
   // this call LOCKs us if err==0
   err = io_check_handle(mtx, IOHT_MUTEX, 0, &fe);
   if (!err) {
      err = mtmux->release(fe->muxev.mh);
      mt_swunlock();
   }
   return err;
}

qserr _std mt_muxstate(qshandle handle, u32t *lockcnt) {
   sft_entry   *fe;
   qserr       err;
   if (!in_mtmode) return E_SYS_INVPARM;
   // this call LOCKs us if err==0
   err = io_check_handle(handle, IOHT_MUTEX, 0, &fe);
   if (!err) {
      qs_sysmutex_state mi;
      int  lkcnt = mtmux->state(fe->muxev.mh, &mi);

      if (lkcnt<0) err = E_SYS_INVPARM; else
      if (lkcnt==0) err = E_MT_SEMFREE; else
      if (!mi.owner) err = E_MT_NOTOWNER; else
      if (lockcnt) *lockcnt = lkcnt;
      mt_swunlock();
   }
   return err;
}

qserr _std mt_closehandle_int(qshandle handle, int force) {
   io_handle_data *fh;
   sft_entry      *fe;
   qserr          err;
   if (!in_mtmode) return E_SYS_INVPARM;
   // this call LOCKs us if err==0
   err = io_check_handle(handle, IOHT_MUTEX|IOHT_EVENT, &fh, &fe);
   if (err) return err;
   /* single object? then try close it first and return error
      (E_MT_SEMBUSY commonly). Both event & mutex should be accepted
      here. */
   if (ioh_singleobj(fe)) err = mtmux->free(fe->muxev.mh, force);
   if (!err) {
      handle ^= IOH_HBIT;
      // unlink it from sft entry & ioh array
      ioh_unlink(fe, handle);
      fh->sign = 0;
      // we are reach real close here!
      if (fe->ioh_first==0) {
         sftable[fh->sft_no] = 0;
         if (fe->fpath) free(fe->fpath);
         fe->sign  = 0;
         fe->fpath = 0;
         free(fe);
      }
      free(fh);
   }
   mt_swunlock();
   return err;
}

qserr _std mt_closehandle(qshandle handle) {
   return mt_closehandle_int(handle,0);
}

qserr _std mt_muxcapture(qshandle handle) {
   if (!in_mtmode) return E_SYS_INVPARM; else {
      mt_waitentry we = { QWHT_MUTEX, 0 };
      we.wh = handle;
      return mtlib->waitobject(&we, 1, 0, 0);
   }
}

qserr _std mt_eventcreate(int signaled, const char *name, qshandle *res) {
   return mt_commoncreate(signaled, name, res, NAMESPC_EVENT, IOHT_EVENT, 1);
}

qserr _std mt_eventopen(const char *name, qshandle *res) {
   return mt_commonopen(name, res, NAMESPC_EVENT, IOHT_EVENT);
}

qserr _std mt_eventact(qshandle evt, u32t action) {
   sft_entry   *fe;
   qserr       err;
   if (!in_mtmode) return E_SYS_INVPARM;
   // this call LOCKs us if err==0
   err = io_check_handle(evt, IOHT_EVENT, 0, &fe);
   if (!err) {
      err = mtmux->event(fe->muxev.mh, action);
      mt_swunlock();
   }
   return err;
}

qserr _std mt_waitsingle(mt_waitentry *uwe, u32t timeout_ms) {
   if (!in_mtmode) return E_MT_DISABLED; else {
      // user`s object must be the first - to be catched on zero timeout
      mt_waitentry we[3];
      u32t       ecnt = 3, sig;
      qserr       res;
      we[0] = *uwe;
      we[0].group = 1;
      we[1].htype = QWHT_SIGNAL;
      we[1].group = 2;
      // no timeout if arg is 0xFFFFFFFF or user`s uwe is QWHT_CLOCK
      if (we[0].htype==QWHT_CLOCK || timeout_ms==FFFF) ecnt--; else {
         we[2].htype = QWHT_CLOCK;
         we[2].tme   = sys_clock() + (timeout_ms)*1000;
         we[2].group = 0;
      }
      // loop until timeout while QWHT_SIGNAL breaks us
      while (1) {
         sig = 0;
         res = mtlib->waitobject(&we, ecnt, 0, &sig);
         if (res) return res;
         if (sig!=2) break;
      }
      return sig==1 ? 0 : E_SYS_TIMEOUT;
   }
}

qserr _std mt_waithandle(qshandle handle, u32t timeout_ms) {
   if (!in_mtmode) return E_SYS_INVPARM; else {
      mt_waitentry we;
      we.htype = QWHT_HANDLE;
      we.wh    = handle;
      return mt_waitsingle(&we, timeout_ms);
   }
}

/* -------------------------------------------------------------
   T L S   v a r s
   -------------------------------------------------------------
   TLS implementation is independent from MTLIB module to allow
   vars usage in non-MT mode.

   In detail, every TLS storage is not allocated until first write into it
   in the running thread. This saves both time & memory. */

/** get process/thread data pointers.
    Must be called in MT lock! */
void _std mt_getdata(mt_prcdata **ppd, mt_thrdata **pth) {
   mt_thrdata *th = mt_exechooks.mtcb_cth;

   if ((u32t)th<0x1000 || th->tiSign!=THREADINFO_SIGN || !th->tiParent) {
      log_printf("tcb: %08X\n", th);
      if ((u32t)th>0x1000) log_printf("tcb: %8lb\n", th);
      mod_dumptree(0);
      _throwmsg_("Current thread state damaged");
   }
   if (ppd) *ppd = th->tiParent;
   if (pth) *pth = th;
}

mt_thrdata* _std mt_gettcb(void) {
   mt_thrdata *rc;
   mt_swlock();
   mt_getdata(0, &rc);
   mt_swunlock();
   return rc;
}

static mt_thrdata* thdata(mt_prcdata *pd, u32t tid) {
   if (!pd || !tid || tid>pd->piListAlloc) return 0;
   return pd->piList[tid-1];
}

/** commit TLS array/slot.
    Must be called in MT lock! */
void mt_tlscommit(mt_thrdata *th, u32t index) {
   mt_prcdata *pd = th->tiParent;
   // TLS used first time in this thread?
   if (!th->tiTLSArray) {
      u32t pasize = sys_queryinfo(QSQI_TLSPREALLOC,0);
      // we are the first in process, select piTLSSize.
      if (!pd->piTLSSize) pd->piTLSSize = pasize?Round64(pasize):64;
      // array for this thread
      th->tiTLSArray = (u64t**)calloc(pd->piTLSSize,sizeof(u64t*));
      // allocated, but uninited has TLS_FAKE_PTR value, unallocated - zero!
      if (pasize) memsetd((u32t*)th->tiTLSArray, TLS_FAKE_PTR, pasize);
   }
   if (index<pd->piTLSSize)
      if (th->tiTLSArray[index]==(u64t*)TLS_FAKE_PTR) {
         void *pv = (u64t*)calloc(TLS_VARIABLE_SIZE,1);
         // attach block to thread
         mem_threadblockex(pv, th);
         th->tiTLSArray[index] = (u64t*)pv;
      }
}

u32t _std mt_tlsalloc(void) {
   mt_thrdata  *th;
   mt_prcdata  *pd;
   u32t    ii, idx;
   u64t      **pos;

   mt_swlock();
   mt_getdata(&pd, 0);
   // use main thread for a reference, not current
   th = pd->piList[0];
   // commit TLS array for this thread
   if (!th->tiTLSArray) mt_tlscommit(th, FFFF);

   pos = (u64t**)memchrd((u32t*)th->tiTLSArray, 0, pd->piTLSSize);
   if (!pos) {
      u32t     oldsz = pd->piTLSSize;
      pd->piTLSSize += TLS_GROW_SIZE;
      // grow exiting arrays
      for (ii=0; ii<pd->piListAlloc; ii++) {
         mt_thrdata *tl = pd->piList[ii];
         if (tl && tl->tiTLSArray) {
            tl->tiTLSArray = (u64t**)realloc(tl->tiTLSArray, sizeof(u64t*)*pd->piTLSSize);
            memset(tl->tiTLSArray+oldsz, 0, TLS_GROW_SIZE*sizeof(u64t*));
         }
      }
      pos = th->tiTLSArray + oldsz;
   }
   idx = pos - th->tiTLSArray;
   /// update exiting arrays
   for (ii=0; ii<pd->piListAlloc; ii++) {
      mt_thrdata *tl = pd->piList[ii];
      if (tl && tl->tiTLSArray) tl->tiTLSArray[idx] = (u64t*)TLS_FAKE_PTR;
   }
   mt_swunlock();
   return idx;
}


qserr _std mt_tlsfree(u32t index) {
   mt_thrdata  *th;
   mt_prcdata  *pd;
   u32t     ii, rc = 0;

   mt_swlock();
   mt_getdata(&pd, 0);
   // use main thread as reference, not current
   th = pd->piList[0];
   if (!pd->piTLSSize) mt_tlscommit(th, FFFF);

   if (index>=pd->piTLSSize) rc = E_MT_TLSINDEX; else
   if (index<sys_queryinfo(QSQI_TLSPREALLOC,0)) rc = E_SYS_ACCESS; else
   // array in main thread is not commited (i.e. was no any mt_tlsalloc() calls)
   if (!th->tiTLSArray) rc = E_MT_TLSINDEX; else
   if (th->tiTLSArray[index]==0) rc = E_MT_TLSINDEX; else {
      for (ii=0; ii<pd->piListAlloc; ii++) {
         mt_thrdata *tl = pd->piList[ii];
         if (tl && tl->tiTLSArray) {
            void *pv = tl->tiTLSArray[index];
            tl->tiTLSArray[index] = 0;
            // is it really allocated?
            if (pv!=(void*)TLS_FAKE_PTR) free(pv);
         }
      }
   }
   mt_swunlock();
   return rc;
}

u64t _std mt_tlsget(u32t index) {
   mt_thrdata *th;
   mt_prcdata *pd;

   mt_swlock();
   mt_getdata(&pd,&th);
   if (index<pd->piTLSSize && th->tiTLSArray) {
      u64t *pv = th->tiTLSArray[index];
      // slot written at least once?
      if (pv && pv!=(u64t*)TLS_FAKE_PTR) {
         u64t rc = *pv;
         mt_swunlock();
         return rc;
      }
   }
   mt_swunlock();
   return 0;
}

static qserr tlsset_common(u32t index, u64t **slotaddr, u64t setv, mt_thrdata *oth) {
   mt_thrdata *th;
   mt_prcdata *pd;
   u32t        rc = 0;

   mt_swlock();
   if (!oth) mt_getdata(&pd,&th); else
      if (oth->tiSign!=THREADINFO_SIGN) rc = E_MT_BADTID;
         else pd = (th = oth)->tiParent;
   if (!rc) {
      // first save in this process?
      if (!pd->piTLSSize) mt_tlscommit(th, FFFF);
#if 0
      log_printf("index = %u, %X, %LX pd=%X, th=%X tid=%u pid=%u\n", index, slotaddr,
         setv, pd, th, th->tiPID, th->tiTID);
#endif
      if (index>=pd->piTLSSize) rc = E_MT_TLSINDEX; else {
         u64t *pv;
         // there is no array, reconstructing one
         if (!th->tiTLSArray) {
            mt_thrdata *th0 = pd->piList[0];
            /* if main thread array exists, then use it as a reference, else
               was no mt_tlsalloc() before and common commit will help */
            if (th0->tiTLSArray) {
               u32t ii;
               th->tiTLSArray = (u64t**)mem_dup(th0->tiTLSArray);
               for (ii=0; ii<pd->piTLSSize; ii++)
                  if (th->tiTLSArray[ii]) th->tiTLSArray[ii] = (u64t*)TLS_FAKE_PTR;
            }
         }
         mt_tlscommit(th, index);
         pv = th->tiTLSArray[index];

         if (!pv) rc = E_MT_TLSINDEX; else
            if (slotaddr) *slotaddr = pv; else *pv = setv;
      }
   }
   mt_swunlock();
   return rc;
}

qserr _std mt_tlsaddr(u32t index, u64t **slotaddr) {
   mt_thrdata *th;
   mt_prcdata *pd;
   u32t        rc = 0;

   if (!slotaddr) return E_SYS_ZEROPTR;
   *slotaddr = 0;
   return tlsset_common(index, slotaddr, 0, 0);
}

// unpublished in API because it unsafe - exported just for MTLIB needs
qserr _std mt_tlssetas(mt_thrdata *th, u32t index, u64t value) {
   return tlsset_common(index, 0, value, th);
}

qserr _std mt_tlsset(u32t index, u64t value) {
   return tlsset_common(index, 0, value, 0);
}

qserr _std mt_threadname(const char *str) {
   char *va;
   qserr rc = mt_tlsaddr(QTLS_COMMENT, (u64t**)&va);
   if (!rc) {
      // no lock here, because partial name is safe ;)
      memset(va, 0, TLS_VARIABLE_SIZE);
      if (str && *str) strncpy(va, str, TLS_VARIABLE_SIZE);
   }
   return rc;
}

qserr _std mt_getthname(mt_pid pid, mt_tid tid, char *buffer) {
   if (!buffer) return E_SYS_ZEROPTR; else
   if (!tid) return E_MT_BADTID; else {
      process_context *pq;
      qserr            rc = 0;

      mt_swlock();
      pq = mod_pidctx(pid);
      if (!pq) rc = E_MT_BADPID; else {
         mt_thrdata *th = thdata((mt_prcdata*)pq->rtbuf[RTBUF_PROCDAT], tid);
         if (!th) rc = E_MT_BADTID; else {
            // thread comment string
            char *th_np = th->tiTLSArray?(char*)th->tiTLSArray[QTLS_COMMENT]:0;
            if (th_np && th_np!=(char*)TLS_FAKE_PTR && th_np[0]) {
               memcpy(buffer, th->tiTLSArray[QTLS_COMMENT], 16); buffer[16] = 0;
            } else
               buffer[0] = 0;
         }
      }
      mt_swunlock();
      return rc;
   }
}

/* -------------------------------------------------------------
   S i g n a l s
   ------------------------------------------------------------- */

void sigljmp_thunk(void);

void __stdcall _siglongjmp(jmp_buf *env, int return_value) {
   if (in_mtmode) {
      mt_thrdata *th;
      mt_swlock();
      mt_getdata(0, &th);
      // force fiber 0 to longjmp address
      if (th->tiFiberIndex) {
         struct tss_s     rd;
         rmcallregs_t *ljmpd = (rmcallregs_t*)env;
         /* warning - using setjmp`s data stack here. Not so good, but
            returning from another task during exception does the same */
         memset(&rd, 0, sizeof(rd));
         rd.tss_eip      = (u32t)&sigljmp_thunk;
         rd.tss_cs       = get_flatcs();
         rd.tss_ds       = rd.tss_cs + 8;
         rd.tss_es       = rd.tss_ds;
         rd.tss_ss       = ljmpd->r_ss;
         rd.tss_esp      = ljmpd->r_eax;
         rd.tss_ecx      = (u32t)env;
         rd.tss_eax      = return_value;
         /* start with _disabled interrupts, _longjmp will popf-ix it */
         rd.tss_eflags   = CPU_EFLAGS_IOPLMASK;
         // set registers for fiber 0
         mtmux->setregs(th->tiList + 0, &rd);
         // delete this fiber
         mtlib->switchfiber(0,1);
      }
      mt_swunlock();
   }
   _longjmp(env, return_value);
}

/* -------------------------------------------------------------
   F P U   l o g i c
   -------------------------------------------------------------
   FPU context switching logics is different in BIOS & EFI hosts.

   In EFI TS bit always OFF. This mean FPU context save/rest at every
   thread context switching.
   On BIOS host TS bit is used as designed. */

/// function must be called inside lock or cli
static qserr _std fpu_allocstate(mt_thrdata *owner, int fiber) {
   if (!owner) return E_SYS_ZEROPTR; else
   if (owner->tiSign!=THREADINFO_SIGN) return E_SYS_INVOBJECT; else
   if (fiber>0 && fiber>=owner->tiFibers) return E_MT_BADFIB; else {
      u32t    fibidx = fiber<0?owner->tiFiberIndex:fiber;
      mt_fibdata *fb = owner->tiList + fibidx;
      // check it too
      if (fibidx>=owner->tiFibers) return E_SYS_INVOBJECT;
      // switch to fiber 0
      if (fb->fiFPUMode==FIBF_FIB0 && fibidx) fb = owner->tiList;

      if (!fb->fiFPURegs) {
         // prevent re-scheduling in malloc functions mt_swunlock()
         mt_swlock();
         fb->fiFPUMode = FIBF_EMPTY;
         fb->fiFPURegs = malloc(fpu_statesize()+48);
         mem_zero(fb->fiFPURegs);
         mem_threadblockex(fb->fiFPURegs, owner);
         /* allocate 48 bytes more to align it on 64 bytes (heap blocks
            are always rounded up to 16 bytes).
            We never free this pointer directly, this mean changing is safe. */
         if ((u32t)fb->fiFPURegs&0x3F)
            fb->fiFPURegs = (void*)Round64((u32t)fb->fiFPURegs);
         mt_swunlock();
      }
#if 0
      log_it(1, "fpu_allocstate: pid %u tid %u %X\n", owner->tiPID, owner->tiTID, fb->fiFPURegs);
#endif
      return 0;
   }
}

/// called from exception handler
static void _std fpu_stsave_int(mt_thrdata *owner) {
   if ((owner->tiMiscFlags&TFLM_NOFPU)==0) {
      mt_fibdata *fb = owner->tiList + owner->tiFiberIndex;
      // go to fiber 0
      if (fb->fiFPUMode==FIBF_FIB0) fb = owner->tiList;

      if (!fb->fiFPURegs) {
         qserr rc = fpu_allocstate(owner,-1);
         if (rc) log_it(1, "fpu_allocstate: pid %u tid %u error %X\n",
            owner->tiPID, owner->tiTID, rc);
      }
      fpu_statesave(fb->fiFPURegs);
      fb->fiFPUMode = FIBF_OK;
   }
}

/// must be called under lock or cli
static void _std fpu_strest_int(mt_thrdata *nowner) {
   if ((nowner->tiMiscFlags&TFLM_NOFPU)==0) {
      mt_fibdata *fb = nowner->tiList + nowner->tiFiberIndex;
      // clts
      sys_clrtsflag();
      // go to fiber 0
      if (fb->fiFPUMode==FIBF_FIB0) fb = nowner->tiList;
#if 0
      log_it(1, "fpu_strest %u:%u %X %u\n", nowner->tiPID, nowner->tiTID,
         fb->fiFPURegs, fb->fiFPUMode);
#endif
      if (fb->fiFPURegs && fb->fiFPUMode==FIBF_OK) fpu_staterest(fb->fiFPURegs);
         else fpu_reinit();
      // new owner
      mt_exechooks.mtcb_cfpt = nowner;
      mt_exechooks.mtcb_cfpf = nowner->tiFiberIndex;
   } else {
      sys_clrtsflag();
      mt_exechooks.mtcb_cfpt = 0;
#if 0
      log_it(1, "pid %u (%s) tid %u is NOFPU, but use FPU!\n", nowner->tiPID,
         nowner->tiParent->piContext->self->name, nowner->tiTID);
#endif
   }
}

/** this is #NM exception handler.
    Called from various places, so make another one cli on enter */
void _std fpu_xcpt_nm(void) {
   mt_thrdata *owner, *cth;
   int         state = sys_intstate(0);
   // lock it, because nobody did it above
   mt_swlock();

   mt_getdata(0, &cth);
   owner = mt_exechooks.mtcb_cfpt;

   if (owner==cth && mt_exechooks.mtcb_cfpf==cth->tiFiberIndex) {
      log_it(2, "someone set TS flag!\n");
   } else {
      sys_clrtsflag();
      // save state
      if (owner) fpu_stsave_int(owner);
      // rest state
      fpu_strest_int(cth);
   }
   sys_intstate(state);
   mt_swunlock();
}

/** update TS bit to proper value.
    Called from exception handler, in MT lock */
void _std fpu_updatets(void) {
   if (hlp_hosttype()==QSHT_EFI) sys_clrtsflag(); else {
      mt_thrdata *owner = mt_exechooks.mtcb_cfpt, *cth;
      int           off;
      mt_getdata(0, &cth);
      off = owner==cth && mt_exechooks.mtcb_cfpf==cth->tiFiberIndex;

      if (Xor(getcr0()&CPU_CR0_TS,off))
         if (off) sys_clrtsflag(); else sys_settsflag();
   }
}

/* callback from switching code (to BIOS/EFI) when TS bit is set
   and FPU owner is non-zero.
   Callback MUST reset TS flag to zero on exit. MT lock is ON here. */
void _std fpu_rmcallcb(void) {
   int state = sys_intstate(0);

   mt_thrdata *owner = mt_exechooks.mtcb_cfpt, *cth;
   mt_getdata(0, &cth);
#if 0
   log_it(2, "fpu_rmcallcb!!! (owner %u:%u, current %u:%u)\n",
      owner?owner->tiPID:0, owner?owner->tiTID:0, cth->tiPID,
         cth->tiTID);
#endif
   if (getcr0()&CPU_CR0_TS) fpu_xcpt_nm();
   sys_intstate(state);
}

/* process start cb in non-MT mode.
   NEVER use TS flag on EFI host! */
void _std fpu_stsave(process_context *newpq) {
   if (hlp_hosttype()==QSHT_EFI) {
      int         state = sys_intstate(0);
      mt_thrdata *owner = mt_exechooks.mtcb_cfpt;
      // save current thread context on exec
      if (!owner) mt_getdata(0, &owner);
      fpu_stsave_int(owner);
      // reset owner & clts - else EFI will annoy us with tonns of #NM
      mt_exechooks.mtcb_cfpt = 0;
      sys_clrtsflag();

      sys_intstate(state);
   } else
      sys_settsflag();
}

/* process exit cb in non-MT mode */
void _std fpu_strest(void) {
   mt_thrdata *cth;
   int       state = sys_intstate(0);

   mt_getdata(0, &cth);
   if (cth==mt_exechooks.mtcb_cfpt) mt_exechooks.mtcb_cfpt = 0;

   if (hlp_hosttype()==QSHT_EFI) {
      process_context *pq = mt_exechooks.mtcb_ctxmem->pctx;
      mt_prcdata      *pd = (mt_prcdata*)pq->rtbuf[RTBUF_PROCDAT];
      mt_thrdata     *pth = pd->piList[cth->tiParent->piParentTID - 1];

      fpu_strest_int(pth);
   } else
      sys_settsflag();

   sys_intstate(state);
}

/* this is direct callback from thread switching code of MTLIB.
   it can switch FPU context or do whatever he wants with FPU */
static void _std fpu_switchto(mt_thrdata *to) {
   if (hlp_hosttype()==QSHT_EFI) {
      mt_thrdata *owner = mt_exechooks.mtcb_cfpt;
      // always switch on EFI, because TS bit is unusable
      if (owner) fpu_xcpt_nm(); else sys_clrtsflag();
   } else {
      if (to==mt_exechooks.mtcb_cfpt && to->tiFiberIndex==
         mt_exechooks.mtcb_cfpf) sys_clrtsflag(); else sys_settsflag();
   }
}

_qs_fpustate _std fpu_statedata = {
   0, fpu_updatets, fpu_xcpt_nm, fpu_switchto, fpu_allocstate };

