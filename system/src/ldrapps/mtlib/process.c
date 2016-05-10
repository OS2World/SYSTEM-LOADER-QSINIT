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
#include "qsxcpt.h"
#include "cpudef.h"

process_context *pq_qsinit = 0;
mt_prcdata      *pd_qsinit = 0;
pmt_new          org_mtnew = 0;
pmt_exit          org_exit = 0;

u32t             *pid_list = 0;
mt_prcdata      **pid_ptrs = 0;
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
   struct tss_s *rd = &fd->fiRegisters;
   memcpy(rd, regs, sizeof(struct tss_s));
}

void mt_seteax(mt_fibdata *fd, u32t eaxvalue) {
   fd->fiRegisters.tss_eax = eaxvalue;
}

mt_thrdata* get_by_tid(mt_prcdata *pd, u32t tid) {
   mt_thrdata *rc = pd->piList;
   u32t       cnt = pd->piThreads;

   while (cnt--)
      if (rc->tiTID==tid) return rc; else rc++;
   return 0;
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
static void _std pageson(void) {
   /* actually, here must be SS replacement for all fibers, but
      let it be as it is :)
      just save new ss value for all new threads, else it can be
      cloned from the older one. */
   force_ss = get_flatss();
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
    state (single thread, single fiber, ... ;)
    Then it also allocates initial TLS */
static void check_prcdata_early(process_context *pq, mt_prcdata *pd) {
   mt_thrdata *td = pd?pd->piList:0;
   mt_fibdata *fd = td?td->tiList:0;
   u32t        ii;

   if (!pd || !td || !fd || pd->piSign!=PROCINFO_SIGN || pd->piPID!=pq->pid ||
      pd->piContext!=pq || pq->rtbuf[RTBUF_PROCDAT]!=(u32t)pd || pd->piThreads!=1 ||
         (pd->piMiscFlags&PFLM_EMBLIST)==0 || pd->piListAlloc!=1 || pd->piTLSSize ||
            td->tiTID!=1 || td->tiSign!=THREADINFO_SIGN || td->tiPID!=pd->piPID ||
               td->tiFibers!=1 || td->tiTLSArray || td->tiFiberIndex ||
                  td->tiMiscFlags!=(TFLM_MAIN|TFLM_EMBLIST) || fd->fiSign!=FIBERSTATE_SIGN ||
                     fd->fiStack || fd->fiStackSize || fd->fiType!=FIBT_MAIN)
                        THROW_ERR_PQ(pq);
   // tls - with zero ptrs
   pd->piTLSSize   = tls_prealloc?Round64(tls_prealloc):64;
   td->tiTLSArray  = (u64t**)calloc_shared(pd->piTLSSize,sizeof(u64t*));
   for (ii=0; ii<tls_prealloc; ii++)
      td->tiTLSArray[ii] = (u64t*)alloc_thread_memory(pq->pid, 1, TLS_VARIABLE_SIZE);
}

/** complex check of process data.
    @return 1 if data is good, else 0 or trap screen (depends on throwerr arg) */
static int check_prcdata(process_context *pq, mt_prcdata *pd, int throwerr) {
   mt_thrdata *td = pd->piList;
   u32t       err = 0, ii;

   if (!pd || !td || pd->piSign!=PROCINFO_SIGN || pd->piPID!=pq->pid ||
      pd->piContext!=pq || pq->rtbuf[RTBUF_PROCDAT]!=(u32t)pd ||
         pd->piListAlloc<pd->piThreads || !pd->piParent && pd!=pd_qsinit ||
            !pd->piThreads) err = 1;

   if (!err) {
      if ((pd->piMiscFlags&PFLM_EMBLIST)==0) {
         if (!mem_getobjinfo(td,0,0)) err = 2;  // is it from heap?
      } else
      if ((void*)td!=pd+1) err = 3;            // list must be really embedded
   }
   // check parent links
   if (!err && pd->piParent) {
      mt_prcdata *ppd = pd->piParent, *pl;
      pl = ppd->piFirstChild;
      while (pl)
         if (pl==pd) break; else pl = pl->piNext;
      if (!pl) err = 4;                        // is it in parent list?
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
   for (ii=0; !err && ii<pd->piListAlloc; ii++) {
      mt_thrdata *tl = td + ii;
      u32t       fii;
                                               // check sign & fields
      if (tl->tiSign!=THREADINFO_SIGN || tl->tiPID!=pd->piPID || Xor(tl->tiListAlloc,tl->tiList) ||
         tl->tiState>THRD_STATEMAX || tl->tiFibers>tl->tiListAlloc ||
            tl->tiListAlloc && tl->tiFiberIndex>=tl->tiListAlloc) {
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
      }
   }
   if (err) {
      log_it(0, "prcdata err %u (%08X,%u,%s)\n", err, pd, pq->pid, pq->self->name);
      if (throwerr) THROW_ERR_PQ(pq);
      return 0;
   }
   return 1;
}

mt_prcdata* _std mt_new(process_context *pq) {
   struct tss_s  rd;
   mt_prcdata   *pd = org_mtnew(pq), *cpd,
               *ppd = pt_current->tiParent;
   mt_thrdata   *td = pd->piList;
   mt_fibdata   *fd = td->tiList;

   pd->piNext       = 0;
   pd->piFirstChild = 0;
   pd->piParent     = ppd;
   pd->piParentTID  = pt_current->tiTID; //? for switch_context()
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
   /* exec module (this thread will be stopped until exit, new thread
      context data was filled by mt_new() above */
   switch_context(pd->piList, SWITCH_EXEC);
   // return exit code
   return pd->piExitCode;
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
   // ZERO and free threads and memory blocks
   for (ii=0; ii<pd->piListAlloc; ii++) {
      mt_thrdata *th = pd->piList+ii;
      if ((th->tiMiscFlags&TFLM_AVAIL)==0) mt_freethread(th,1);
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
   pt_current = current->piList;
   /* patch all launch32() calls (except START module) to split
      processes really */
   while (pq->parent) {
      module          *mod = pq->self;
      launch32_stack  *pst = (launch32_stack*)mod->stack_ptr - 1;
      process_context *ppq = pq->pctx;
      mt_prcdata       *pd = (mt_prcdata*)pq->rtbuf[RTBUF_PROCDAT],
                      *ppd = (mt_prcdata*)ppq->rtbuf[RTBUF_PROCDAT];
      mt_thrdata       *td = ppd->piList;
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
   /// check is it really QSINIT
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
   mt_exechooks.mtcb_pgoncb = &pageson;
   mt_exechooks.mtcb_glock  = 0;

   mt_dumptree();
}

void update_wait_state(mt_prcdata *pd, u32t waitreason, u32t waitvalue) {
   u32t ii;
   // enum threads
   for (ii=0; ii<pd->piThreads; ii++) {
      mt_thrdata *tl = pd->piList+ii;
      if (tl->tiState==THRD_WAITING && tl->tiWaitReason==waitreason &&
         tl->tiWaitHandle==waitvalue) tl->tiState = THRD_RUNNING;
   }
}

u32t _std mt_setfiber(mt_tid tid, u32t fiber) {
   mt_thrdata  *th;
   mt_prcdata  *pd;
   mt_thrdata *dth;
   u32t         rc = 0;

   mt_swlock();
   th  = (mt_thrdata*)pt_current;
   pd  = th->tiParent;
   dth = tid ? get_by_tid(pd, tid) : th;
   // no tid
   if (!dth) rc = E_MT_BADTID; else
   // thread is gone or this is main, waiting for secondaries
   if (dth->tiState==THRD_FINISHED || dth->tiState==THRD_WAITING &&
      dth->tiWaitReason==THWR_TIDMAIN) rc = E_MT_GONE; else
   // waiting for anything?
   if (dth->tiState==THRD_WAITING) rc = E_MT_BUSY; else
   // check fiber index
   if (dth->tiListAlloc>=fiber || dth->tiList[fiber].fiType==FIBT_AVAIL)
      rc = E_MT_BADFIB; else
   if (dth->tiFiberIndex==fiber) rc = E_SYS_DONE;

   if (!rc)
      if (th==dth) {
         // switch fiber in current active thread
         dth->tiWaitReason = fiber;
         switch_context(0, SWITCH_FIBER);
      } else
         dth->tiFiberIndex = fiber;
   mt_swunlock();
   return rc;
}

mt_tid _std mt_createthread(mt_threadfunc thread, u32t flags, mt_ctdata *optdata, void *arg) {
   mt_prcdata  *where = 0;
   u32t     stacksize = _64KB;
   u32t        rstack = 0, rc;
   mt_thrdata     *th;
   // ...
   if (!mt_on || !thread) return 0;
   // the only valid flags value now
   if (flags && flags!=MTCT_SUSPENDED) return 0;

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
      if (rstack) th->tiList[0].fiRegisters.tss_esp = rstack + stacksize;
      // sub space for argument & ret addr
      th->tiList[0].fiRegisters.tss_esp -= 8;
      // create stack
      stackptr = (u32t*)th->tiList[0].fiRegisters.tss_esp;
      stackptr[0] = (u32t)&mt_exitthreada;
      stackptr[1] = (u32t)arg;

      if (optdata) {
         th->tiCbStart = optdata->onenter;
         th->tiCbExit  = optdata->onexit;
         th->tiCbTerm  = optdata->onterm;
      }
      if ((flags&MTCT_SUSPENDED)==0) th->tiState = THRD_RUNNING;
      rc = th->tiTID;
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
   if (!dth) rc = E_MT_BADTID; else
   // thread is gone or this is main, waiting for secondaries
   if (dth->tiState==THRD_FINISHED || dth->tiState==THRD_WAITING &&
      dth->tiWaitReason==THWR_TIDMAIN) rc = E_MT_GONE; else
   // main or system?
   if ((dth->tiMiscFlags&(TFLM_SYSTEM|TFLM_MAIN))) rc = E_MT_ACCESS; else
   // waiting for child?
   if (dth->tiState==THRD_WAITING && dth->tiWaitReason==THWR_CHILDEXEC)
      rc = E_MT_BUSY;
   if (rc || th->tiTID==tid) {
      mt_swunlock();
      if (rc) return rc;
      // ah!
      mt_exitthread(result);
   }
   // create fiber & then launch it
   fibIndex = mt_allocfiber(dth, FIBT_MAIN, PAGESIZE, (u32t)&mt_exitthreada);
   if (fibIndex==FFFF) {
      rc = E_MT_ACCESS;
      mt_swunlock();
   } else {
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

   // this lock will be reset in any of switch_context() below
   mt_swlock();
   th = (mt_thrdata*)pt_current;
   pd = th->tiParent;
   th->tiExitCode  = result;
   th->tiState     = THRD_FINISHED;

   // free threads, who waits for us
   update_wait_state(pd, THWR_TIDEXIT, th->tiTID);

   if (th->tiMiscFlags&TFLM_MAIN) {
      mt_thrdata *pth;
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
         // th can be changed, actually
         th = (mt_thrdata*)pt_current;
         pd = th->tiParent;
      }
#if 0
      log_it(0, "exit main p2 (%d) %X %d %d\n", pd->piThreads, th, th->tiTID,
         th->tiState);
#endif
      pth = get_by_tid(pd->piParent, pd->piParentTID);
      // unable to find exec caller?
      if (!pth || pth->tiState!=THRD_WAITING) THROW_ERR_PD(pd);
      // set result in waiting thread
      mt_seteax(pth->tiList + pth->tiFiberIndex, result);
      // set own state
      th->tiState  = THRD_FINISHED;
      // unlock thread in parent
      pth->tiState = THRD_RUNNING;
      // exit process actually
      switch_context(pth, SWITCH_PROCEXIT);
   } else {
      // check for possible main thread waiting for us
      if (pd->piThreads==2 && (th->tiMiscFlags&TFLM_MAIN)==0)
         update_wait_state(pd, THWR_TIDMAIN, 0);

      switch_context(0, SWITCH_EXIT);
   }
}
