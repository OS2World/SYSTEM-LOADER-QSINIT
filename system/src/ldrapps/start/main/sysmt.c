//
// QSINIT "start" module
// mutex (handle) processing
// -------------------------------------
//

#include "qsint.h"
#include "syslocal.h"
#include "cpudef.h"
#include "qstask.h"
#include "qserr.h"
#include "qssys.h"
#include "sysio.h"

#define TLS_GROW_SIZE   64    ///< TLS array one time grow size

static qs_mtlib               mtlib = 0;
u8t                       in_mtmode = 0;
qs_sysmutex                   mtmux = 0;
extern mod_addfunc* _std  mod_secondary;  // this thing is import from QSINIT module
extern module *     _std      mod_ilist;  // and this
extern mt_proc_cb   _std   mt_exechooks;  // and also this ;)

void   setup_loader_mt(void);
void   setup_pagman_mt(void);
void   setup_exi_mt   (void);

static qserr _std muxcvtfunc(qshandle mutex, mux_handle_int *res) {
   if (!res) return E_SYS_ZEROPTR; else {
      sft_entry *fe;
      qserr  rc = io_check_handle(mutex, IOHT_MUTEX, 0, &fe);
      *res = rc?0:fe->mux.mh;
      // dec MT lock, was incremented by success io_check_handle()
      if (!rc) mt_swunlock();
      return rc;
   }
}

static u32t _std memstat_thread(void *arg) {
   mtlib->threadname("memstat");
   // unzip LDI
   unzip_delaylist();
   // dump memstat once per 2 minutes
   if (env_istrue("MEMSTAT")==1)
      while (true) {
         mem_stat();
         usleep(CLOCKS_PER_SEC*120);
      }
   return 0;
}

/** callback from MT lib - MT mode is ready.
    Called at the end of mt_initialize() - before returning to user, so
    we have fully functional threading here, but still no threads except
    "system idle" :) */
void _std mt_startcb(qs_sysmutex mproc) {
   // and we need this pointer too ;)
   if (!mtlib) get_mtlib();
   mtmux     = mproc;
   mtmux->init(muxcvtfunc, qe_available_spec);
   in_mtmode = 1;
   // create mutexes for QSINIT binary (module loader & micro-FSD access)
   setup_loader_mt();
   // create mutexes for page allocation code
   setup_pagman_mt();
   // create mutexes for exi instances
   setup_exi_mt();
   // run funcs, waiting for this happens
   sys_notifyexec(SECB_MTMODE, 0);
   // memstat thread
   if (env_istrue("MEMSTAT")==1 || mod_delay) {
      mt_ctdata  tsd;
      memset(&tsd, 0, sizeof(mt_ctdata));
      tsd.size      = sizeof(mt_ctdata);
      tsd.stacksize = 65536;
      tsd.pid       = 1;
      mtlib->createthread(memstat_thread, 0, &tsd, 0);
   }
}

/** callback from MT lib - thread exit.
    This is the last call in thread context, next will be releasing mutexes
    and exit.
    In non-MT mode mod_exec calls it too, on exit, with td=0 arg */
void _std mt_pexitcb(mt_thrdata *td) {
   u32t pid = td?td->tiPID:mod_getpid();
   // main thread exit / non-MT mode
   if (!td || td->tiTID==1) {
      // non-MT - reset FPU owner!
      if (!td) fpu_strest();
      //log_it(0, "pid %u exit cb!\n", pid);
      /* call it here for the main thread, else notification code can be
         invoked in parent context after process exit */
      sys_notifyfree(pid);
      /* here too - this will free all instances before the moment of imported
         DLL mod term function. I.e. if we will call it after DLL fini - DLL
         can deny unload if it provides class for us */
      exi_free_as(pid);
   }
   // in MT mode only
   if (td) {
      /* if we owning mfs_mutex - hlp_fclose() must be called to reset
         mutex & flags in mfs i/o function */
      if (!mt_muxstate(mod_secondary->mfs_mutex, 0)) hlp_fclose();
      /* if we owning ldr_mutex - then trap screen is too close to us now,
         but still tries to clear mod_ilist at least. */
      if (!mt_muxstate(mod_secondary->ldr_mutex, 0)) {
          if (mod_ilist) {
             log_it(0, "mod_ilist was lost!\n");
             mod_ilist = 0;
          }
      }
   }
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
    @return actually direct pointer to TLS variable with thread comment */
char *mt_getthreadname(void) {
   static char *tnb = "";
   if (in_mtmode) {
      char *tnstr;
      if (!mt_tlsaddr(QTLS_COMMENT, (u64t**)&tnstr)) return tnstr;
   }
   return tnb;
}

// -------------------------------------------------------------
// M u t e x e s
// -------------------------------------------------------------

qserr _std mt_muxcreate(int initialowner, const char *name, qshandle *res) {
   qserr  rc = 0;
   u32t  fno;
   if (!res) return E_SYS_ZEROPTR;
   if (name && !*name) name = 0;
   *res = 0;
   // need to start MT first!
   if (!in_mtmode) return E_MT_DISABLED;

   mt_swlock();
   fno  = sft_find(name, NAMESPC_MUTEX);

   if (fno) rc = E_MT_DUPNAME; else {
      mux_handle_int mhi = 0;
      u32t           pid = mod_getpid();
      u32t          ifno = ioh_alloc(),
                     fno = sft_alloc();
      sft_entry      *fe = 0;

      if (!ifno || !fno) rc = E_SYS_NOMEM; else
         rc = mtmux->create(&mhi, fno);
      if (!rc) {
         fe = (sft_entry*)malloc(sizeof(sft_entry));
         fe->sign       = SFTe_SIGN;
         fe->type       = IOHT_MUTEX;
         fe->fpath      = name?strdup(name):0;
         fe->name_space = NAMESPC_MUTEX;
         fe->pub_fno    = SFT_PUBFNO+fno;
         fe->mux.mh     = mhi;
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

qserr _std mt_muxopen(const char *name, qshandle *res) {
   qserr  rc = 0;
   u32t  fno;
   if (!res) return E_SYS_ZEROPTR;
   *res = 0;
   if (name && !*name) name = 0;
   if (!name) return E_SYS_INVPARM;
   // no MT - no mutexes!
   if (!in_mtmode) return E_SYS_NOPATH;

   mt_swlock();
   fno  = sft_find(name, NAMESPC_MUTEX);

   if (!fno) rc = E_SYS_NOPATH; else {
      mux_handle_int mhi = 0;
      u32t           pid = mod_getpid();
      u32t          ifno = ioh_alloc();
      sft_entry      *fe = sftable[fno];

      if (!ifno) rc = E_SYS_NOMEM; else
      if (fe->type!=IOHT_MUTEX) rc = E_SYS_INVHTYPE;

      if (!rc) {
         ioh_setuphandle(ifno, fno, pid);
         if (!rc) *res = IOH_HBIT+ifno;
      }
   }
   mt_swunlock();
   return rc;
}

qserr _std mt_muxrelease(qshandle mtx) {
   sft_entry   *fe;
   qserr       err;
   if (!in_mtmode) return E_SYS_INVPARM;
   // this call LOCKs us if err==0
   err = io_check_handle(mtx, IOHT_MUTEX, 0, &fe);
   if (!err) {
      err = mtmux->release(fe->mux.mh);
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
      int  lkcnt = mtmux->state(fe->mux.mh, &mi);

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
   err = io_check_handle(handle, IOHT_MUTEX, &fh, &fe);
   if (err) return err;
   /* single object? then try close it first and return error (E_MT_SEMBUSY
      commonly */
   if (ioh_singleobj(fe)) err = mtmux->free(fe->mux.mh, force);
   if (!err) {
      handle ^= IOH_HBIT;
      // unlink it from sft entry & ioh array
      ioh_unlink(fe, handle);
      fh->sign = 0;
      // we reach real close here!
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
      we.mux = handle;
      return mtlib->waitobject(&we, 1, 0, 0);
   }
}

qserr _std mt_muxwait(qshandle handle, u32t timeout_ms) {
   if (!in_mtmode) return E_SYS_INVPARM; else {
      // mutex must be first to be catched on zero timeout
      mt_waitentry we[2] = {{QWHT_MUTEX,1}, {QWHT_CLOCK,2}};
      qserr       res;
      u32t        sig;
      we[0].mux = handle;
      we[1].tme = sys_clock() + (timeout_ms)*1000;

      res = mtlib->waitobject(&we, 2, 0, &sig);

      if (res) return res;
      return sig==1 ? 0 : E_SYS_TIMEOUT;
   }
}

/* -------------------------------------------------------------
   T L S   v a r s
   -------------------------------------------------------------
   TLS implementation is independent from MTLIB module to allow
   vars usage in non-MT mode.

   In details, every TLS storage is not allocated until the first
   write into it in running thread. This saves both time & memory. */

/** get process/thread data pointers.
    Must be called in MT lock! */
void _std mt_getdata(mt_prcdata **ppd, mt_thrdata **pth) {
   process_context *pq = mt_exechooks.mtcb_ctxmem;
   mt_prcdata      *pd = (mt_prcdata*)pq->rtbuf[RTBUF_PROCDAT];
   if (ppd) *ppd = pd;
   if (pth) {
      u32t tid = mt_exechooks.mtcb_ctid - 1;
      *pth = pd->piListAlloc<=tid ? 0 : pd->piList[tid];
   }
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
      // preallocated, but uninited
      if (pasize) memsetd((u32t*)th->tiTLSArray, TLS_FAKE_PTR, pasize);
   }
   if (index<pd->piTLSSize)
      if (th->tiTLSArray[index]==(u64t*)TLS_FAKE_PTR) {
         void *pv = (u64t*)calloc(TLS_VARIABLE_SIZE,1);
         // attach block to thread
         mem_threadblockex(pv, th->tiPID, th->tiTID);
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
   // use main thread as reference, not current
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
            tl->tiTLSArray = (u64t**)realloc(tl->tiTLSArray, sizeof(u64t**)*pd->piTLSSize);
            memset(tl->tiTLSArray+oldsz, 0, TLS_GROW_SIZE*sizeof(u64t**));
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
      if (pv!=(u64t*)TLS_FAKE_PTR) {
         u64t rc = *pv;
         mt_swunlock();
         return rc;
      }
   }
   mt_swunlock();
   return 0;
}

static qserr tlsset_common(u32t index, u64t **slotaddr, u64t setv) {
   mt_thrdata *th;
   mt_prcdata *pd;
   u32t        rc = 0;

   mt_swlock();
   mt_getdata(&pd,&th);
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
         /* if we have main thread array, then use it as reference, else
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
   mt_swunlock();
   return rc;
}

qserr _std mt_tlsaddr(u32t index, u64t **slotaddr) {
   mt_thrdata *th;
   mt_prcdata *pd;
   u32t        rc = 0;

   if (!slotaddr) return E_SYS_ZEROPTR;
   *slotaddr = 0;
   return tlsset_common(index, slotaddr, 0);
}

qserr _std mt_tlsset(u32t index, u64t value) {
   return tlsset_common(index, 0, value);
}

/* -------------------------------------------------------------
   F P U   l o g i c
   -------------------------------------------------------------
   FPU context switching logics is different in BIOS & EFI hosts.

   In EFI TS bit always OFF. This mean FPU context switching at every
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
         mem_threadblockex(fb->fiFPURegs, owner->tiPID, owner->tiTID);
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
