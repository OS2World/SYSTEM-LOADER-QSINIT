//
// QSINIT
// context switching and scheduler
// ------------------------------------------------------------
// most of code here should not use any API.
// exceptions are:
//  * memcpy/memset (imported by direct pointer)
//  * 64-bit mul/div (exported by direct pointer in QSINIT)
//
#include "mtlib.h"
#include "qshm.h"

volatile mt_thrdata *pt_current = 0;
static   u64t     last_shed_tsc = 0;

/// non-reenterable tree walking start
mt_prcdata *walk_start(void) {
   pd_qsinit->piTmpWalk = 0;
   return pd_qsinit;
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
   For SWITCH_TIMER interrupts should be disabled at this moment. */
void _std switch_context(mt_thrdata *thread, u32t reason) {
   mt_thrdata *th = (mt_thrdata*)pt_current;
   u64t       now = hlp_tscread();
   u32t    pfiber = th->tiFiberIndex;

   if (last_shed_tsc)
      th->tiTime += (now - last_shed_tsc)/tsc_100mks;
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
      case SWITCH_PROCEXIT:
         // no context saving
         th = 0;
         break;
      case SWITCH_WAIT:
         // tiWaitReason must be filled by caller
         th->tiState      = THRD_WAITING;
         break;
      case SWITCH_EXIT:
         // free it, but leave slot for re-use
         mt_freethread(th,0);
         // inform switch code below about to skip context saving
         th = 0;
         break;
      case SWITCH_FIBER:
         thread           = th;
         th->tiState      = THRD_RUNNING;  // just update
         th->tiFiberIndex = th->tiWaitReason;
         th->tiWaitReason = 0;
         break;
      default:
         th->tiState      = THRD_RUNNING;  // just update
         break;
   }

   // search for suitable thread
   if (!thread) {
      u64t        cdiff = 0;
      u32t       ti, ii;
      mt_thrdata *ztime = 0;

      for (ti=0; ti<pid_alloc; ti++) {
         mt_prcdata  *pd = pid_ptrs[ti];
         mt_thrdata *cth;

         if (!pd) continue;
         
         cth = pd->piList;
         // walking over running threads
         for (ii=0; ii<pd->piListAlloc; ii++, cth++)
            if ((cth->tiMiscFlags&TFLM_AVAIL)==0 && cth->tiState==THRD_RUNNING) {
               /* thread with zero LastTime has priority above all, which cause
                  timeslice for any new thread and for all threads just after
                  MTLIB start */
               if (!cth->tiLastTime) { thread = cth; pd = 0; break; } else {
                  u64t tdiff = now - cth->tiLastTime;
                  if (tdiff>cdiff) { thread = cth; cdiff = tdiff; }
                     else ztime = cth;
               }
            }
         if (!pd) break;
      }
      // this is me again :)
      if (!thread && ztime) thread = ztime;
#if 0
      if (reason!=SWITCH_TIMER)
         log_it(0, "tid %i -> %d %X %X\n", th?th->tiTID:-1, thread?thread->tiTID:-1, th, thread);
#endif
   }
   if (!thread && th && th->tiState==THRD_RUNNING) thread = th;
   // we will hang on next lines, so dump tree here without any precaution
   if (!thread) mt_dumptree();

   if (thread!=th || reason==SWITCH_FIBER) {
      // swap pt_current, process context and xcpt_top in START module
      if (th) th->tiList[pfiber].fiXcptTop = *pxcpt_top;
      mt_exechooks.mtcb_ctxmem = thread->tiParent->piContext;
      mt_exechooks.mtcb_ctid   = thread->tiTID;
      pt_current    = thread;
      *pxcpt_top    = thread->tiList[thread->tiFiberIndex].fiXcptTop;
   }
   last_shed_tsc = hlp_tscread();

   // swap registers (real switching point for all cases, except SWITCH_TIMER)
   if (reason!=SWITCH_TIMER) {
      struct tss_s *fbold= th ? &th->tiList[pfiber].fiRegisters : 0,
                   *fbnew= &pt_current->tiList[pt_current->tiFiberIndex].fiRegisters;

      if (fbold!=fbnew) swapregs32(fbold,fbnew);
   }
}


void _std switch_context_timer(void *regs, int is64) {
   struct tss_s *fbregs = &pt_current->tiList[pt_current->tiFiberIndex].fiRegisters,
                 *fbnew;
   if (is64) regs64to32(fbregs, (struct xcpt64_data*)regs); else
      memcpy(fbregs, regs, sizeof(struct tss_s));

   switch_context(0, SWITCH_TIMER);

   fbnew = &pt_current->tiList[pt_current->tiFiberIndex].fiRegisters;
   // thread was changed, update regs in interrupt/mt_yield stack
   if (fbnew!=fbregs)
      if (is64) regs32to64((struct xcpt64_data*)regs, fbnew); else
         memcpy(regs, fbnew, sizeof(struct tss_s));
}
