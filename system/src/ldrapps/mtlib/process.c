//
// QSINIT
// process/thread handling support
// ------------------------------------------------------------
// most of code here should not use any API.
// exceptions are:
//  * memcpy/memset (imported by direct pointer)
//  * 64-bit mul/div (exported by direct pointer in QSINIT)
//
#include "mtlib.h"

process_context *pq_qsinit = 0;
mt_prcdata      *pd_qsinit = 0;
pmt_new          org_mtnew = 0;
pmt_exit          org_exit = 0;

u32t             *pid_list = 0;
mt_prcdata      **pid_ptrs = 0;
mt_thrdata     *pt_sysidle = 0;
u32t             pid_alloc = 0;    ///< number of allocated entires in pid_list
u16t              force_ss = 0;
u32t          tls_prealloc = 0;

/// process exec function (args in [eax ebx ecx edx])
void mt_launch(void);

/// stack, as it filled by launch32() code in QSINIT.
typedef struct {
   void      *retaddr;
   module     *module;
   u32t      reserved;
   char       *envptr;
   char      *cmdline;
   u32t     startaddr;
   u32t    parent_esp;
} launch32_stack;

/// replace fiber regs by supplied one.
void mt_setregs(mt_fibdata *fd, struct tss_s *regs) {
   struct tss_s *rd = &fd->fiRegs;
   memcpy(rd, regs, sizeof(struct tss_s));
}

void mt_seteax(mt_fibdata *fd, u32t eaxvalue) {
   fd->fiRegs.tss_eax = eaxvalue;
}

mt_thrdata* get_by_tid(mt_prcdata *pd, u32t tid) {
   if (!tid || tid>pd->piListAlloc) return 0; else {
      mt_thrdata *rc = pd->piList[tid-1];
      if (rc)
         if (rc->tiTID!=tid) {
            log_it(0, "tid mismatch for pid %u tid %u\n", pd->piPID, tid);
            mt_dumptree();
            THROW_ERR_PD(pd);
         }
      return rc;
   }
}

// must be called inside lock only!
mt_prcdata* get_by_pid(u32t pid) {
   u32t *pos = memchrd(pid_list, pid, pid_alloc);
   if (!pos) return 0;
   return pid_ptrs[pos-pid_list];
}

/* must be called only inside lock!
   list is used in TIMER handler too, but in scheduling only, so lock
   protects us */
static void pidlist_add(u32t pid, mt_prcdata *pd) {
   while (1) {
      u32t oldsize = pid_alloc;
      if (pid_list) {
         u32t *pos = memchrd(pid_list, 0, pid_alloc);
         if (pos) {
            pid_ptrs[pos-pid_list] = pd;
            *pos = pid;
            return;
         }
      }
      pid_alloc = (pid_alloc+10)*2;
      pid_list  = (u32t*)       realloc_shared(pid_list, pid_alloc*sizeof(u32t));
      pid_ptrs  = (mt_prcdata**)realloc_shared(pid_ptrs, pid_alloc*sizeof(mt_prcdata*));
      memset(pid_list+oldsize, 0, (pid_alloc-oldsize)*sizeof(u32t));
      memset(pid_ptrs+oldsize, 0, (pid_alloc-oldsize)*sizeof(mt_prcdata*));
   }
}

// must be called inside lock only!
static void pidlist_del(u32t pid) {
   u32t *pos = memchrd(pid_list, pid, pid_alloc);
   if (!pos) return;
   *pos = 0;
   pid_ptrs[pos-pid_list] = 0;
}

// callback, invoked by pag_enable()
static void _std pageson(sys_eventinfo *info) {
   /* actually, here must be SS replacement for all fibers, but
      let it be as it is :)
      just save new ss value for all new threads, else it can be
      cloned from the older one. */
   mt_swlock();
   force_ss = get_flatss();
   mt_swunlock();
   log_it(2, "mtlib: pae on, new ss: %hX\n", force_ss);
}

/* searches pid by exe module handle.
   actually used in "lm list" only! */
u32t pid_by_module(module* handle) {
   u32t ii = 0, rc = 0;
   mt_swlock();
   while (ii<pid_alloc)
      if (pid_list[ii] && pid_ptrs[ii]->piModule==handle)
         { rc = pid_list[ii]; break; } else ii++;
   mt_swunlock();
   return rc;
}

void* alloc_thread_memory(u32t pid, u32t tid, u32t size) {
   return mem_allocz(QSMEMOWNER_COTHREAD+tid-1, pid, size);
}

/** function just _throw_ trap screen on error.
    This is version for pre-MTLIB structs, it checks, what structures,
    was allocated by QSINIT`s process.c still is in the same totalitary
    state (single thread, single fiber, ... ;) */
static void check_prcdata_early(process_context *pq, mt_prcdata *pd) {
   mt_thrdata *td = pd && pd->piList?pd->piList[0]:0;
   mt_fibdata *fd = td?td->tiList:0;
   u32t        ii;

   if (!pd || !td || !fd || pd->piSign!=PROCINFO_SIGN || pd->piPID!=pq->pid ||
      pd->piContext!=pq || pq->rtbuf[RTBUF_PROCDAT]!=(u32t)pd || pd->piThreads!=1 ||
         (pd->piMiscFlags&PFLM_EMBLIST)==0 || pd->piListAlloc!=PREALLOC_THLIST ||
            td->tiTID!=1 || td->tiSign!=THREADINFO_SIGN || td->tiPID!=pd->piPID ||
               td->tiFibers!=1 || td->tiFirstMutex || td->tiFiberIndex ||
                  td->tiMiscFlags!=(TFLM_MAIN|TFLM_EMBLIST) || fd->fiStack ||
                     fd->fiSign!=FIBERSTATE_SIGN || fd->fiStackSize ||
                        fd->fiType!=FIBT_MAIN || fd->fiFPUMode!=FIBF_EMPTY &&
                           fd->fiFPUMode!=FIBF_OK) THROW_ERR_PQ(pq);
}

/** complex check of process data.
    @return 1 if data is good, else 0 or trap screen (depends on throwerr arg) */
static int check_prcdata(process_context *pq, mt_prcdata *pd, int throwerr) {
   mt_thrdata *td = pd->piList?pd->piList[0]:0; // main thread
   u32t       err = 0, ii, thcnt;

   if (!pd || !td || pd->piSign!=PROCINFO_SIGN || pd->piPID!=pq->pid ||
      pd->piContext!=pq || pq->rtbuf[RTBUF_PROCDAT]!=(u32t)pd ||
         pd->piListAlloc<pd->piThreads || !pd->piParent && pd!=pd_qsinit ||
            !pd->piThreads) err = 1;

   if (!err) {
      if ((pd->piMiscFlags&PFLM_EMBLIST)==0) {
         if (!mem_getobjinfo(td,0,0)) err = 2;  // is it from heap?
      } else
      if ((void*)td!=pd+1) err = 3;             // list must be really embedded
   }
   // check parent links
   if (!err && pd->piParent) {
      mt_prcdata *ppd = pd->piParent, *pl;
      pl = ppd->piFirstChild;
      while (pl)
         if (pl==pd) break; else pl = pl->piNext;
      if (!pl) err = 4;                         // is it in parent list?
         else
      if (ppd->piPID!=pd->piParentPID) err = 5;
   }
   // check childs
   if (!err && pd->piFirstChild) {
      mt_prcdata *pl = pd->piFirstChild;
      while (pl) {
         if (pl->piParent!=pd || pl->piParentPID!=pd->piPID) err = 6;
         pl = pl->piNext;
      }
   }
   // check every thread
   for (ii=0, thcnt=0; !err && ii<pd->piListAlloc; ii++) {
      mt_thrdata *tl = pd->piList[ii];
      if (tl) {
         u32t    fii;
         // check sign & fields
         if (tl->tiSign!=THREADINFO_SIGN || tl->tiPID!=pd->piPID || !tl->tiList ||
            tl->tiState>THRD_STATEMAX || !tl->tiFibers || tl->tiFibers>tl->tiListAlloc ||
               tl->tiFiberIndex>=tl->tiListAlloc) {
                  log_it(0, "la %d, l %X, st %d, #fb %d, idx %d\n", tl->tiListAlloc,
                     tl->tiList, tl->tiState, tl->tiFibers, tl->tiFiberIndex);
                  err = 7;
               }
         else
         if (tl->tiList && (tl->tiMiscFlags&TFLM_EMBLIST)==0) // is it from heap?
            if (!mem_getobjinfo(tl->tiList,0,0)) err = 8;
         // check fibers
         for (fii=0; !err && fii<tl->tiListAlloc; fii++) {
            mt_fibdata *fd = tl->tiList + fii;
            if (fd->fiSign!=FIBERSTATE_SIGN) err = 9;
            // FPU state buffer should be aligned to 64
            if (fd->fiFPURegs)
               if ((ptrdiff_t)fd->fiFPURegs&0x3F) err = 11;
         }
         thcnt++;
      }
   }
   // check number of threads match
   if (thcnt!=pd->piThreads) {
      log_it(0, "%u<>%u\n", thcnt, pd->piThreads);
      err = 10;
   }

   if (err) {
      log_it(0, "prcdata err %u (%08X,%u,%s)\n", err, pd, pq->pid, pq->self->name);
      if (throwerr) THROW_ERR_PQ(pq);
      return 0;
   }
   return 1;
}

mt_prcdata* _std mt_new(process_context *pq, void *mtdata) {
   struct tss_s   rd;
   mt_prcdata    *pd = org_mtnew(pq,0), *cpd,
                *ppd = pt_current->tiParent;
   mt_thrdata    *td = pd->piList[0];
   mt_fibdata    *fd = td->tiList;
   se_start_data *sd = (se_start_data*)mtdata;

   pd->piNext        = 0;
   pd->piFirstChild  = 0;
   pd->piParent      = ppd;

   if (sd && sd->sign!=SEDAT_SIGN) sd = 0;
   if (sd) {
      pd->piParentTID = 0;
      pd->piMiscFlags|= PFLM_NOPWAIT;
      if (sd->flags&QEXS_DETACH) td->tiSession = SESN_DETACHED; else {
         // !!
         td->tiSession = pt_current->tiSession;
      }
      sd->pid = pd->piPID;
      sd->sno = td->tiSession;
      // change parent data to QSINIT
      if (sd->flags&QEXS_NOTACHILD) {
         pd->piParentPID = 1;
         pd->piParent    = pd_qsinit;
         pq->parent      = pq_qsinit->self;
         pq->pctx        = pq_qsinit;
         ppd             = pd_qsinit;
      }
      /* process must be suspended until launcher`s
         mt_waitobject() for it */
      if (sd->flags&QEXS_WAITFOR) pd->piMiscFlags|=PFLM_LWAIT;
   } else {
      pd->piParentTID  = pt_current->tiTID;
      td->tiSession    = pt_current->tiSession;
   }
   td->tiState      = THRD_SUSPENDED;
   td->tiTime       = 0;
   td->tiLastTime   = 0;
   // first thread registers
   memset(&rd, 0, sizeof(rd));
   rd.tss_eip = (u32t)&mt_launch;
   rd.tss_cs  = get_flatcs();
   rd.tss_ds  = rd.tss_cs + 8;
   rd.tss_es  = rd.tss_ds;
   rd.tss_esp = pq->self->stack_ptr;
   rd.tss_ss  = force_ss?force_ss:get_flatss();
   rd.tss_eax = (u32t)pq->self;
   rd.tss_ebx = pq->self->start_ptr;
   rd.tss_ecx = (u32t)pq->envptr;
   rd.tss_edx = (u32t)pq->cmdline;
   rd.tss_eflags = CPU_EFLAGS_IOPLMASK|CPU_EFLAGS_IF;
   mt_setregs(fd, &rd);
   // insert process into tree
   cpd = ppd->piFirstChild;
   if (cpd) {
      while (cpd->piNext) cpd = cpd->piNext;
      cpd->piNext = pd;
   } else
      ppd->piFirstChild = pd;
   // and into index for search
   pidlist_add(pq->pid, pd);

   return pd;
}

u32t _std mt_exec(process_context *pq) {
   volatile mt_prcdata *pd = (mt_prcdata*)pq->rtbuf[RTBUF_PROCDAT];
   /* it should have one thread, one fiber, valid signs and so on, else
      we just _throw_ immediately */
   check_prcdata_early(pq, (mt_prcdata*)pd);

   if (pd->piMiscFlags&PFLM_NOPWAIT) {
      mt_thrdata *th0 = pd->piList[0];

      if (pd->piMiscFlags&PFLM_LWAIT) {
         th0->tiState      = THRD_WAITING;
         th0->tiWaitReason = THWR_WAITLNCH;
         th0->tiWaitHandle = mod_getpid();
      } else
         th0->tiState      = THRD_RUNNING; 
      return 0;
   } else {
      /* exec module (this thread will be stopped until exit, new thread
         context data was filled by mt_new() above */
      switch_context(pd->piList[0], SWITCH_EXEC);
      // return exit code
      return pd->piExitCode;
   }
}

u32t _std mt_exit(process_context *pq) {
   mt_prcdata *pd = (mt_prcdata*)pq->rtbuf[RTBUF_PROCDAT], *ppd, *cpd;
   u32t        ii;
   // lock it!
   mt_swlock();
   // just panic
   if (!pd) THROW_ERR_PQ(pq);
   // check and shows trap screen on error
   check_prcdata(pq, pd, 1);
   // return on system
   if (pd->piMiscFlags&MOD_SYSTEM) {
      mt_swunlock();
      return 0;
   }
   // remove process from tree & index
   ppd = pd->piParent;
   cpd = ppd->piFirstChild;

   if (cpd==pd) {
      ppd->piFirstChild = pd->piNext;
   } else {
      while (cpd->piNext!=pd) cpd = cpd->piNext;
      cpd->piNext = pd->piNext;
   }
   pidlist_del(pq->pid);
   // all threads except main should be free here!
   for (ii=0; ii<pd->piListAlloc; ii++) {
      mt_thrdata *th = pd->piList[ii];
      if (th)
         if (!ii) mt_freethread(th,1); else {
            log_it(0, "exit pid %u, but tid %u = %X\n", pd->piPID, ii+1, th);
            THROW_ERR_PD(pd);
         }
   }
   // tree is valid, no critical funcs below, so we can allow switching back
   mt_swunlock();

   if ((pd->piMiscFlags&PFLM_EMBLIST)==0) {
      mem_zero(pd->piList);
      free(pd->piList);
   }
   return org_exit(pq);
}

void init_process_data(void) {
   mt_prcdata *current;
   // top module context
   process_context *pq = mod_context();
   // check current process data
   current    = (mt_prcdata*)pq->rtbuf[RTBUF_PROCDAT];
   check_prcdata_early(pq, current);
   pidlist_add(pq->pid, current);
   // current active thread
   pt_current = current->piList[0];
   /* patch all launch32() calls (except START module) to split
      processes really */
   while (pq->parent) {
      module          *mod = pq->self;
      launch32_stack  *pst = (launch32_stack*)mod->stack_ptr - 1;
      process_context *ppq = pq->pctx;
      mt_prcdata       *pd = (mt_prcdata*)pq->rtbuf[RTBUF_PROCDAT],
                      *ppd = (mt_prcdata*)ppq->rtbuf[RTBUF_PROCDAT];
      mt_thrdata       *td = ppd->piList[0];
      mt_fibdata       *fd = td->tiList;
      u16t         flat_cs = get_flatcs();
      struct tss_s      rd;

      check_prcdata_early(ppq, ppd);
      // build tree
      pd->piNext        = 0;
      pd->piParent      = ppd;
      ppd->piFirstChild = pd;
      // and index
      pidlist_add(ppd->piPID, ppd);

      td->tiState       = THRD_WAITING;
      td->tiWaitReason  = THWR_CHILDEXEC;
      td->tiWaitHandle  = pd->piPID;
      td->tiTime        = 0;
      td->tiLastTime    = 0;
      // exit thread (tid 1) will cause exit process
      pst->retaddr      = mt_exitthreada;
      /* parent process main thread state after return.
         First dword in parent_esp points to address of @@l32exit in launch32(),
         so we should call a single ret to resume parent process. */
      memset(&rd, 0, sizeof(rd));
      rd.tss_eip = (u32t)pure_retn;
      rd.tss_cs  = flat_cs;
      rd.tss_ds  = flat_cs + 8;
      rd.tss_es  = rd.tss_ds;
      rd.tss_esp = pst->parent_esp;
      rd.tss_ss  = get_flatss();
      // these flags are void, popfd is second cpu command to execute after retn
      rd.tss_eflags = CPU_EFLAGS_IOPLMASK|CPU_EFLAGS_IF|CPU_EFLAGS_CPUID;
      // set parent process thread registers
      mt_setregs(fd, &rd);
      // dig into
      pq = ppq;
   }
   pq_qsinit = pq;
   /// check - is it really QSINIT?
   if (pq->self!=(module*)mh_qsinit) THROW_ERR_PQ(pq);
   // here we have a Tree ;)
   pd_qsinit = (mt_prcdata*)pq->rtbuf[RTBUF_PROCDAT];
   pd_qsinit->piNext = 0;

   // update exec hooks
   org_mtnew = mt_exechooks.mtcb_init;
   org_exit  = mt_exechooks.mtcb_fini;
   mt_exechooks.mtcb_init   = mt_new;
   mt_exechooks.mtcb_exec   = mt_exec;
   mt_exechooks.mtcb_fini   = mt_exit;
   mt_exechooks.mtcb_yield  = &yield;
   mt_exechooks.mtcb_glock  = 0;
   // install cb for page mode switching on
   sys_notifyevent(SECB_PAE|SECB_GLOBAL, pageson);

   // mt_dumptree();
}

void update_wait_state(mt_prcdata *pd, u32t waitreason, u32t waitvalue) {
   u32t ii;
   // enum threads
   for (ii=0; ii<pd->piListAlloc; ii++) {
      mt_thrdata *tl = pd->piList[ii];
      if (tl)
         if (tl->tiState==THRD_WAITING && tl->tiWaitReason==waitreason &&
            tl->tiWaitHandle==waitvalue) tl->tiState = THRD_RUNNING;
   }
}

static void lwait_off(mt_prcdata *pd, u32t parent) {
   if (pd->piThreads==1 && (pd->piMiscFlags&PFLM_LWAIT)) {
      mt_thrdata *tm = pd->piList[0];
      if (tm->tiState==THRD_WAITING && tm->tiWaitReason==THWR_WAITLNCH &&
         tm->tiWaitHandle==parent) tm->tiState = THRD_RUNNING;
   }
}

/** Release childs, who waits for mt_waitobject() from specified pid.
    Must be called inside lock only! */
void update_lwait(u32t parent, u32t child) {
   if (child) {
      mt_prcdata *pd = get_by_pid(child);
      if (pd) lwait_off(pd, parent);
   } else {
      u32t ii;
      for (ii=0; ii<pid_alloc; ii++)
         if (pid_list[ii]) lwait_off(pid_ptrs[ii], parent);
   }
}

mt_tid _std mt_createthread(mt_threadfunc thread, u32t flags, mt_ctdata *optdata, void *arg) {
   mt_prcdata  *where = 0;
   u32t     stacksize = _64KB;
   u32t        rstack = 0, rc;
   mt_thrdata     *th;
   // ...
   if (!mt_on || !thread) return 0;
   // the only valid flags value now
   if (flags&~(MTCT_NOFPU|MTCT_NOTRACE|MTCT_SUSPENDED)) return 0;

   if (optdata) {
      u32t maxsize;
      hlp_memavail(&maxsize,0);
      if (optdata->size!=sizeof(mt_ctdata)) return 0;
      if (optdata->stacksize>maxsize) return 0;
      if (optdata->stacksize) stacksize = optdata->stacksize;
      // we must add stacksize to stack to make esp value, so check it!
      if (optdata->stack)
         if (!optdata->stacksize) return 0; else rstack = (u32t)optdata->stack;
      mt_swlock();
      if (optdata->pid)
         if ((where = get_by_pid(optdata->pid))==0) { mt_swunlock(); return 0; }
   } else
      mt_swlock();
   if (!where) where = pt_current->tiParent;

   th = mt_allocthread(where, (u32t)thread, rstack?0:stacksize);
   if (!th) rc = 0; else {
      u32t *stackptr;
      // ready stack esp setup
      if (rstack) th->tiList[0].fiRegs.tss_esp = rstack + stacksize;
      // sub space for argument & ret addr
      th->tiList[0].fiRegs.tss_esp -= 8;
      // create stack
      stackptr = (u32t*)th->tiList[0].fiRegs.tss_esp;
      stackptr[0] = (u32t)&mt_exitthreada;
      stackptr[1] = (u32t)arg;

      if (optdata) {
         th->tiCbStart = optdata->onenter;
         th->tiCbExit  = optdata->onexit;
         th->tiCbTerm  = optdata->onterm;
      }
      if (flags&MTCT_NOFPU) th->tiMiscFlags |= TFLM_NOFPU;
      if (flags&MTCT_NOTRACE) th->tiMiscFlags |= TFLM_NOTRACE;
      if ((flags&MTCT_SUSPENDED)==0) th->tiState = THRD_RUNNING;
      rc = th->tiTID;
   }
   mt_swunlock();
   return rc;
}

qserr _std mt_resumethread(u32t pid, mt_tid tid) {
   mt_thrdata  *th;
   mt_prcdata  *pd, *rpd;
   u32t         rc = 0;

   mt_swlock();
   th  = (mt_thrdata*)pt_current;
   pd  = th->tiParent;
   rpd = !pid ? pd : get_by_pid(pid);

   if (!rpd) rc = E_MT_BADPID; else {
      mt_thrdata *rth = get_by_tid(rpd, tid);
      // no tid
      if (!rth) rc = E_MT_BADTID; else
      // thread state mismatch
      if (rth->tiState!=THRD_SUSPENDED) rc = E_MT_NOTSUSPENDED; else
         rth->tiState = THRD_RUNNING;
   }
   mt_swunlock();
   return rc;
}

u32t _std mt_termthread(mt_tid tid, u32t result) {
   mt_thrdata  *th;
   mt_prcdata  *pd;
   mt_thrdata *dth;
   u32t   fibIndex, rc = 0;

   mt_swlock();
   th  = (mt_thrdata*)pt_current;
   pd  = th->tiParent;
   dth = get_by_tid(pd, tid);
   // no tid
   if (!dth) rc = E_MT_BADTID; else {
      u32t st = dth->tiState,
           wr = dth->tiWaitReason;
      // thread is gone or this is main, waiting for secondaries
      if (st==THRD_FINISHED || st==THRD_WAITING && wr==THWR_TIDMAIN) rc = E_MT_GONE;
         else
      // main or system?
      if ((dth->tiMiscFlags&(TFLM_SYSTEM|TFLM_MAIN))) rc = E_MT_ACCESS;
         else
      /* waiting for something? still able to stop mt_waitobject, another
         one mt_termthread (THWR_TIDEXIT state) & THWR_WAITLNCH. */
      if (st==THRD_WAITING && wr!=THWR_TIDEXIT && wr!=THWR_WAITOBJ &&
         wr!=THWR_WAITLNCH) rc = E_MT_BUSY;
   }

   if (rc || th->tiTID==tid) {
      mt_swunlock();
      if (rc) return rc;
      // this can`t be waiting!
      mt_exitthread(result);
   }
   // create fiber & then launch it
   fibIndex = mt_allocfiber(dth, FIBT_MAIN, PAGESIZE, (u32t)&mt_exitthreada);
   if (fibIndex==FFFF) {
      rc = E_MT_ACCESS;
      mt_swunlock();
   } else {
      // cleanup mt_waitobject()
      if (dth->tiState==THRD_WAITING && dth->tiWaitReason==THWR_WAITOBJ)
         w_term(dth, THRD_SUSPENDED);

      mt_seteax(dth->tiList+fibIndex, result);
      dth->tiFiberIndex = fibIndex;
      dth->tiState      = THRD_RUNNING;

      th->tiWaitReason  = THWR_TIDEXIT;
      th->tiWaitHandle  = tid;
      // lock will be reset here
      switch_context(dth, SWITCH_WAIT);
      // here we are when thread has finished
   }
   return rc;
}

/// this removes register saving and so on ...
#pragma aux mt_exitthread_int aborts;

/** exit thread call. */
void _std mt_exitthread_int(u32t result) {
   mt_thrdata  *th;
   mt_prcdata  *pd;

   // this lock will be cleared in any of switch_context() below
   mt_swlock();
   th = (mt_thrdata*)pt_current;
   pd = th->tiParent;
   th->tiExitCode  = result;
   th->tiState     = THRD_FINISHED;

   // free threads, who waits for us
   update_wait_state(pd, THWR_TIDEXIT, th->tiTID);

   if (th->tiMiscFlags&TFLM_MAIN) {
      th->tiParent->piExitCode = result;
#if 0
      log_it(0, "exit main (%d) %X %d\n", pd->piThreads, th, th->tiTID);
#endif
      if (pd->piThreads>1) {
         th->tiWaitReason = THWR_TIDMAIN;
         th->tiWaitHandle = 0;
         // switch main thread to waiting state
         switch_context(0, SWITCH_WAIT);
         // restore lock
         mt_swlock();
      }
#if 0
      log_it(0, "exit main p2 (%d) %X %d %d\n", pd->piThreads, th, th->tiTID,
         th->tiState);
#endif
      // set own state
      th->tiState  = THRD_FINISHED;
      // release childs, who waits for mt_waitobject() from us
      update_lwait(pd->piPID, 0);
      // we have a parent?
      if (pd->piParentTID) {
         mt_thrdata *pth = get_by_tid(pd->piParent, pd->piParentTID);
         // unable to find exec caller?
         if (!pth || pth->tiState!=THRD_WAITING) THROW_ERR_PD(pd);
         // set result in waiting thread
         mt_seteax(pth->tiList + pth->tiFiberIndex, result);
         // unlock thread in parent
         pth->tiState = THRD_RUNNING;
         // exit process
         switch_context(pth, SWITCH_PROCEXIT);
      } else
         switch_context(0, SWITCH_PROCEXIT);
   } else {
      // check for possible main thread waiting for us
      if (pd->piThreads==2 && (th->tiMiscFlags&TFLM_MAIN)==0)
         update_wait_state(pd, THWR_TIDMAIN, 0);

      switch_context(0, SWITCH_EXIT);
   }
}

/// our`s usleep() replacement
void _std mt_usleep(u32t usec) {
   /* if sleep < 1/2 of tick - call original i/o delay based function,
      else use mt_waitobject, 1024 mks in 1 ms here ;) */
   if (usec>>5 <= tick_ms) _usleep(usec); else {
      mt_waitentry we = { QWHT_CLOCK, 0 };
      u32t        res;
      we.tme = sys_clock() + usec;
      mt_waitobject(&we, 1, 0, &res);
   }
}

static u32t _std sleeper(void *arg) {
   clock_t lct = sys_clock();
   u32t    cnt = 0;
   // mark self (this naming occurs in first idle time slice)
   mt_threadname("system idle");
   /* if we called too often during last 10 seconds - check for ctrl-alt
      pressed and then tries to get hotkey. This cause keypress loss */
   while (1) {
      __asm { hlt }
#if 0
      if (sys_clock()-lct > 10000000) {
         lct = sys_clock();
         cnt = 0;
      } else
      if (++cnt > 100) {
         if (((cnt-1)%25)==0) hlp_seroutchar('~');
         if ((key_status()&(KEY_ALT|KEY_CTRL))==(KEY_ALT|KEY_CTRL)) {
            u16t key = key_wait(0);
            if (key) log_hotkey(key);
         }
      }
#endif
   }
   return 0;
}

void start_idle_thread(void) {
   mt_ctdata  ctd;
   mt_tid     tid;

   memset(&ctd, 0, sizeof(mt_ctdata));
   ctd.size      = sizeof(mt_ctdata);
   ctd.stacksize = 16384;
   ctd.pid       = 1;

   tid = mt_createthread(sleeper, 0, &ctd, 0);
   if (tid) pt_sysidle = get_by_tid(pd_qsinit, tid);
   if (!pt_sysidle) {
      log_it(0, "idle thread error!\n");
      THROW_ERR_PD(pd_qsinit);
   } else {
      // eternal life! :)
      pt_sysidle->tiMiscFlags |= TFLM_SYSTEM|TFLM_NOSCHED|TFLM_NOFPU|TFLM_NOTRACE;
   }
}
