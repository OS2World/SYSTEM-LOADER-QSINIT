//
// QSINIT
// context switching and scheduler
// ------------------------------------------------------------
// most of code here should not use any API.
// exceptions are:
//  * memcpy/memset (imported by direct pointer)
//  * 64-bit mul/div (exported by direct pointer in QSINIT)
//  * bsf32/bsr32 :)
//
#include "mtlib.h"

volatile mt_thrdata *pt_current = 0;
static   u64t     last_shed_tsc = 0;
attime_entry              attcb[ATTIMECB_ENTRIES];

/// non-reenterable tree walking start
mt_prcdata *walk_start(mt_prcdata *top) {
   mt_prcdata *res = top?top:pd_qsinit;
   res->piTmpWalk = 0;
   return res;
}

/// non-reenterable tree walking get next
mt_prcdata *walk_next(mt_prcdata *cur) {
   mt_prcdata *rc = cur->piFirstChild,
            *next = cur->piNext;
   if (next) next->piTmpWalk = cur->piTmpWalk;
   if (rc) rc->piTmpWalk = next?next:cur->piTmpWalk;
      else rc = next;
   if (!rc) rc = cur->piTmpWalk;

   return rc;

   /*-*-*-*-*-*
      |   |   |
      *-* |   *-*
        | |
        * *-*
            |
            */
}

/* Function must be called inside lock for manual context switch
   (anything except SWITCH_TIMER) - and it will reset lock to 0 (!)
   on exit in this case.
   For SWITCH_TIMER interrupts should be disabled at this moment.

   Thread/process exit must go through this func because is calls
   w_check_conditions() for it and free belonging wait lists */
void _std switch_context(mt_thrdata *thread, u32t reason) {
   mt_thrdata *th = (mt_thrdata*)pt_current;
   u64t       now = hlp_tscread(),
            rtime = (now - last_shed_tsc)/tsc_100mks;
   u32t    pfiber = th->tiFiberIndex, ci;

   if (last_shed_tsc) th->tiTime += rtime;
   // attime callbacks
   for (ci=0; ci<ATTIMECB_ENTRIES; ci++) {
      register attime_entry *ate = attcb+ci;
      while (ate->cbf && (!ate->diff_tsc || now-ate->start_tsc > ate->diff_tsc)) {
         qsclock next = ate->cbf(&ate->usrdata);
         // we got DIFF value here, this allows direct calculation
         if (!next) ate->cbf = 0; else {
            ate->start_tsc+= ate->diff_tsc;
            ate->diff_tsc  = ((next + 50) / 100) * tsc_100mks;
         }
      }
   }
   // convert to tics
   now >>= tsc_shift;
   // convert to tics
   th->tiLastTime = now;

   switch (reason) {
      case SWITCH_EXEC:
         th->tiWaitReason = THWR_CHILDEXEC;
         th->tiState      = THRD_WAITING;
         th->tiWaitHandle = thread->tiParent->piPID;
         thread->tiState  = THRD_RUNNING;
         break;
      case SWITCH_PROCEXIT: {
         mt_prcdata *pd = th->tiParent;
         // callback to START in thread context & release mutexes
         mt_pexitcb(th, 0);
         mutex_release_all(th);
         // main thread exit
         w_check_conditions(th->tiPID, 0, 0);
         // drop FPU context
         if (mt_exechooks.mtcb_cfpt==th) mt_exechooks.mtcb_cfpt = 0;
         // session exit? then ask someone to free it
         if (pd->piMiscFlags&PFLM_NOPWAIT)
            qe_postevent(sys_q, SYSQ_SESSIONFREE, (long)pd, 0, 0);
         // no context saving
         th    = 0;
         break;
      }
      case SWITCH_WAIT:
         // tiWaitReason must be filled by caller
         th->tiState      = THRD_WAITING;
         break;
      case SWITCH_EXIT:
         // callback to START in thread context & release mutexes
         mt_pexitcb(th, 0);
         mutex_release_all(th);
         // we must call it when thread data still valid!
         w_check_conditions(th->tiPID, th->tiTID, 0);
         /* free it (it should zero FPU context too).
            Note, that after this string and until context switch - environment
            is undefined.
            All thread data released, but stack exists, because mt_freefiber()
            is smart enough to post an event about to free it later. This,
            also, applied to SWITCH_FIBEXIT */
         mt_freethread(th);
         // inform switch code below about to skip context saving
         th    = 0;
         break;
      case SWITCH_FIBEXIT:
      case SWITCH_FIBER:
         thread           = th;
         th->tiState      = THRD_RUNNING;  // just update
         th->tiFiberIndex = th->tiWaitReason;
         th->tiWaitReason = 0;
         break;
      case SWITCH_SUSPEND:
         th->tiState      = THRD_SUSPENDED;
         break;
      case SWITCH_MANUAL:
      default:
         th->tiState      = THRD_RUNNING;  // just update
         break;
   }
   /* check waiting conditions (except exit cases, was called above),
      args *must* be 0 for SWITCH_TIMER */
   if (th) w_check_conditions(0,0,0);
   // search for suitable thread
   if (!thread) {
      u64t        cdiff = 0, ediff = 0;
      mt_thrdata  *zthr = 0, *ethr = 0;
      u32t       ti, ii;
      int         eater = rtime/10 > tick_ms*2;

      for (ti=0; ti<pid_alloc; ti++) {
         mt_prcdata  *pd = pid_ptrs[ti];
         if (!pd) continue;

         // walking over running threads
         for (ii=0; ii<pd->piListAlloc; ii++) {
            mt_thrdata *cth = pd->piList[ii];

            if (cth && cth->tiState==THRD_RUNNING &&
                (cth->tiMiscFlags&TFLM_NOSCHED)==0)
            {
               /* thread with zero LastTime has priority above all, this cause
                  a timeslice for any new thread and for all threads just after
                  MTLIB start */
               if (!cth->tiLastTime) { thread = cth; pd = 0; break; } else {
                  u64t tdiff = now - cth->tiLastTime;

                  if (now==cth->tiLastTime) zthr = cth;
                     else
                  if ((cth->tiMiscFlags&TFLM_TLP)==0) {
                     if (tdiff>cdiff) { thread = cth; cdiff = tdiff; }
                  } else {
                     if (tdiff>ediff) { ethr = cth; ediff = tdiff; }
                     cth->tiMiscFlags&=~TFLM_TLP;
                  }
               }
            }
         }
         if (!pd) break;
      }
      if (th && eater) th->tiMiscFlags|=TFLM_TLP;
      if (!thread && ethr) thread = ethr;
      // this is me again :)
      if (!thread && zthr) thread = zthr;
      // primitive switch tracing
      if (sched_trace) {
         if (eater) hlp_seroutchar('e');
         hlp_seroutchar('~');
         if (ethr && thread==ethr) hlp_seroutchar('!');
         if (thread) { 
            hlp_seroutchar(thread->tiPID%10 + '0');
            hlp_seroutchar(thread->tiTID%10 + '0');
         }
      }
   }
   if (!thread) thread = pt_sysidle;
   // fiber exit - free _current_ fiber here, so no context save below
   if (reason==SWITCH_FIBEXIT) { mt_freefiber(th, pfiber); th = 0; }
   /* we have at least two threads - system & the last console. This mean
      that we can wait for a thread switch and swap fiber in it to an existing
      signal APC fiber, because at this moment this is safe */
   if (thread!=th && thread->tiSigQueue && thread->tiSigFiber && !thread->tiFiberIndex)
      thread->tiFiberIndex = thread->tiSigFiber;

   if (thread!=th || reason==SWITCH_FIBER) {
      // swap pt_current, process context and xcpt_top in START module
      if (th) th->tiList[pfiber].fiXcptTop = *pxcpt_top;
      mt_exechooks.mtcb_ctxmem = thread->tiParent->piContext;
      mt_exechooks.mtcb_cth    = thread;
      mt_exechooks.mtcb_sesno  = thread->tiSession;

      pt_current    = thread;
      *pxcpt_top    = thread->tiList[thread->tiFiberIndex].fiXcptTop;
      // let the START module will setup FPU
      fpu_intdata->switchto(thread);
   }
   last_shed_tsc = hlp_tscread();

   // swap registers (real switching point for all cases, except SWITCH_TIMER)
   if (reason!=SWITCH_TIMER) {
      struct tss_s *fbold= th ? &th->tiList[pfiber].fiRegs : 0,
                   *fbnew= &pt_current->tiList[pt_current->tiFiberIndex].fiRegs;

      if (fbold!=fbnew) swapregs32(fbold,fbnew);
   }
}


void _std switch_context_timer(void *regs, int is64) {
   struct tss_s *fbregs = &pt_current->tiList[pt_current->tiFiberIndex].fiRegs,
                 *fbnew;
   if (is64) regs64to32(fbregs, (struct xcpt64_data*)regs); else
      memcpy(fbregs, regs, sizeof(struct tss_s));

   /* disable custom yield() / yield in mt_swunlock()!
      this is critical, because function below calls a tonns of api with
      lock/unlock */
   next_rdtsc = FFFF64;

   switch_context(0, SWITCH_TIMER);

   fbnew = &pt_current->tiList[pt_current->tiFiberIndex].fiRegs;
   // thread was changed, update regs in interrupt/mt_yield stack
   if (fbnew!=fbregs)
      if (is64) regs32to64((struct xcpt64_data*)regs, fbnew); else
         memcpy(regs, fbnew, sizeof(struct tss_s));
}
