//
// QSINIT
// wait objects
// ------------------------------------------------------------
// most of code here should not use any API.
// exceptions are:
//  * memcpy/memset (imported by direct pointer)
//  * 64-bit mul/div (exported by direct pointer in QSINIT)
//  * bsf32/bsr32 :)
//
#include "mtlib.h"
#include "time.h"
#include "qcl/sys/qsmuxapi.h"

#define WOBJ_OK             (1)              ///< entry signaled
#define WOBJ_MUXFREE        (2)              ///< entry is/was a free mutex at check time
#define WOBJ_SAVERES        (4)              ///< entry requires result saving
#define WOBJ_EVFREE         (8)              ///< entry is/was a free event at check time

/** Objects added to the bottom of list (we_last) and processed from the top
    (we_first). This is the only help for long waiting threads - they are
    always will be first in check */
static we_list_entry  *we_first = 0,
                       *we_last = 0;

qserr _std mt_waitobject(mt_waitentry *list, u32t count, u32t glogic, u32t *signaled) {
   u32t             ii;
   qserr           err;
   mt_thrdata      *ct;
   we_list_entry  *nwe;
   clock_t       now_t = 0;
   u64t          now_c = 0;
   if (!list ) return E_SYS_ZEROPTR;
   if (!count) return E_SYS_INVPARM;
   if (!mt_on) return E_MT_DISABLED;

   mt_swlock();
   ct = (mt_thrdata*)pt_current;

   // log_it(0, "mt_waitobject (%u:%u) [%u %LX] %u\n", ct->tiPID, ct->tiTID, list->htype, list->tme, count);

   /* copy list data first, because QWHT_HANDLE must be replaced to valid
      type in the next loop */
   ii  = sizeof(we_list_entry) + (count-1)*sizeof(mt_waitentry);
   nwe = (we_list_entry*)malloc(count+ii);
   // wait list entry setup
   nwe->sigf     = (u8t*)nwe + ii;
   nwe->caller   = (mt_thrdata*)pt_current;
   nwe->signaled = -1;
   nwe->reterr   = 0;
   nwe->ecnt     = count;
   nwe->clogic   = glogic;
   memcpy(&nwe->we, list, count*sizeof(mt_waitentry));
   memset(nwe->sigf, 0, count);
   // check parameters
   for (ii=0, err=0; ii<count && !err; ii++) {
      mt_waitentry *we = nwe->we + ii;
      // convert QWHT_HANDLE to a real handle type
      if (we->htype==QWHT_HANDLE) {
         u32t ht = io_handletype(we->wh);
         switch (ht) {
            case IOFT_MUTEX: we->htype = QWHT_MUTEX; break;
            case IOFT_QUEUE: we->htype = QWHT_QUEUE; break;
            case IOFT_EVENT: we->htype = QWHT_EVENT; break;
            default:
               err = E_SYS_INVPARM; 
         }
      }
      if (err) break;

      if (we->htype>QWHT_EVENT) err = E_SYS_INVPARM; else {
         switch (we->htype) {
            case QWHT_TID:
               if (we->tid && get_by_tid(pt_current->tiParent, we->tid)==0)
                  err = E_MT_BADTID;
               break;
            case QWHT_PID:
               if (get_by_pid(we->pid)) err = E_MT_BADPID;
               break;
            case QWHT_EVENT:
            case QWHT_MUTEX:
               if (!hlp_cvtmux) err = E_SYS_UNSUPPORTED; else
                  err = hlp_cvtmux(we->wh, (mux_handle_int*)&we->reserved);
               break;
            case QWHT_QUEUE:
               // check both for handle and access
               if (qe_available(we->wh)<0) err = E_SYS_INVPARM;
               break;
         }
      }
   }
   if (err) { mt_swunlock(); free(nwe); return err; }

   for (ii=0; ii<count; ii++) {
      mt_waitentry *we = nwe->we + ii;

      switch (we->htype) {
         // convert clock_t to rdtsc ticks
         case QWHT_CLOCK: {
            // can be slowed by calibrate, so call only on first real request
            if (!now_t) { now_t = sys_clock(); now_c = hlp_tscread(); }
            // start tsc value
            we->reserved = now_c;

            if (we->tme<now_t) we->tme = 0; else {
               we->tme = (we->tme - now_t + 50) / 100;
               // if timeout > 80 days - set it to "forewer", else calc real value
               if (we->tme > _4GBLL*16) we->tme = FFFF64; else
                  we->tme *= tsc_100mks;
            }
            break;
         }
         case QWHT_MUTEX:
         case QWHT_EVENT:
            mutex_wcounter(we->reserved, 1);
            break;
         case QWHT_PID  :
            // release we->pid process if it waits for us (QEXS_WAITFOR flag)
            update_lwait(ct->tiPID, we->pid);
            break;
      }
   }
   w_add(nwe);
   /* if condition is ready just now - then no additional context switching,
      else switch current thread to waiting state (this reset MT lock to 0!) */
   if (!w_check_conditions(0,0,nwe)) {
      pt_current->tiWaitReason = THWR_WAITOBJ;
      pt_current->tiWaitHandle = (u32t)nwe;
      switch_context(0, SWITCH_WAIT);
      // wake up with ready condition
      mt_swlock();
   }
   err = nwe->reterr;
   if (!err)
      if (signaled) *signaled = nwe->signaled;
   w_del(nwe);
   mt_swunlock();
   free(nwe);
   return err;
}

void w_add(we_list_entry *entry) {
   entry->prev = we_last;
   entry->next = 0;
   if (we_last) we_last->next = entry;
   if (!we_first) we_first = entry;
   we_last     = entry;
}

void w_del(we_list_entry *entry) {
   u32t     ii;
   if (entry->prev) entry->prev->next = entry->next; else we_first = entry->next;
   if (entry->next) entry->next->prev = entry->prev; else we_last  = entry->prev;
   entry->next = 0;
   entry->prev = 0;

   for (ii=0; ii<entry->ecnt; ii++) {
      mt_waitentry *we = entry->we + ii;
      if (we->htype==QWHT_MUTEX || we->htype==QWHT_EVENT)
         mutex_wcounter(we->reserved, -1);
   }
}

void w_term(mt_thrdata *wth, u32t newstate) {
   if (wth->tiState!=THRD_WAITING || wth->tiWaitReason!=THWR_WAITOBJ) {
      log_it(0, "w_term(%08X) but state not match (%u)\n", wth, wth->tiState);
   } else {
      we_list_entry *nwe = (we_list_entry*)wth->tiWaitHandle;
      w_del(nwe);
      wth->tiState      = newstate;
      wth->tiWaitReason = 0;
   }
}

/* warning! called from timer interrupt!
   Any call, catched by TRACE in the code below can/will trap us! */
int w_check_conditions(u32t pid, u32t tid, we_list_entry *special) {
   we_list_entry *we = we_first;
   u64t          now = hlp_tscread();
   u32t       result = 0;

   while (we) {
      u32t ii, upcnt = 0;
      /* check - is it subject to remove?
         occurs while process/thread exit only */
      if (pid && we->caller->tiPID==pid)
         if (!tid || we->caller->tiTID==tid) {
            we_list_entry *rwe = we;
            we = we->next;
            w_del(rwe);
            free(rwe);
            continue;
         }

      if (we->signaled<0) {
         for (ii=0; ii<we->ecnt; ii++)
            if ((we->sigf[ii]&WOBJ_OK)==0) {
               mt_waitentry *cwe = we->we+ii;
               switch (cwe->htype) {
                  case QWHT_TID  :
                     if (pid && tid)
                        if (we->caller->tiPID==pid && (cwe->tid==tid || !cwe->tid)) {
                           we->sigf[ii]  = WOBJ_OK|(cwe->resaddr?WOBJ_SAVERES:0);
                           // use high part to save result
                           cwe->reserved = cwe->reserved&FFFF|(u64t)(we->caller->
                              tiParent->piList[tid-1]->tiExitCode)<<32;
                        }
                     break;
                  case QWHT_PID  :
                     if (pid && !tid)
                        if (cwe->pid==pid) {
                           we->sigf[ii]  = WOBJ_OK|(cwe->resaddr?WOBJ_SAVERES:0);
                           // let it hangs on zero - because this shouldn`t be
                           cwe->reserved = (u64t)(get_by_pid(pid)->piExitCode)<<32|
                              cwe->reserved&FFFF;
                        }
                     break;
                  case QWHT_CLOCK:
                     if (cwe->tme!=FFFF64)
                        if (!cwe->tme || now - cwe->reserved >cwe->tme)
                           we->sigf[ii] = WOBJ_OK;
                     break;
                  case QWHT_EVENT: {
                     int e_state = event_state(cwe->reserved);
                     if (e_state<0) we->reterr = E_SYS_INVOBJECT; else
                        if (e_state==1) we->sigf[ii] = WOBJ_OK; else
                           if (e_state==2) we->sigf[ii] = WOBJ_EVFREE;
                              else we->sigf[ii] = 0;
                     break;
                  }
                  case QWHT_MUTEX: {
                     qs_sysmutex_state ms;
                     int   m_state = mutex_state(0,0, cwe->reserved, &ms);
                     /* set bit 1 to signal about free mutex, which we need
                        to catch on success with everybody else */
                     if (m_state<0) we->reterr = E_SYS_INVOBJECT; else
                     if (m_state==0 || we->caller->tiPID==ms.pid &&
                        we->caller->tiTID==ms.tid) we->sigf[ii] = WOBJ_MUXFREE;
                     else
                        we->sigf[ii] = 0;
                     break;
                  }
                  case QWHT_KEY  :
                     if (key_available(we->caller->tiSession))
                        we->sigf[ii] = WOBJ_OK;
                     break;
                  case QWHT_QUEUE: {
                     int evnum = hlp_qavail(cwe->wh, 0, 0);
                     if (evnum<0) we->reterr = E_SYS_INVOBJECT; else
                        we->sigf[ii] = evnum>0?WOBJ_OK:0;
                     break;
                  }
                  case QWHT_SIGNAL:
                     if (we->caller->tiSigQueue && *we->caller->tiSigQueue)
                        we->sigf[ii] = WOBJ_OK;
                     break;
               }
               upcnt += we->sigf[ii];
            }
         // just return error code to user
         if (we->reterr) we->signaled = 0; else
         // lucky entries here, check conditions
         if (upcnt) {
            u32t  gs = we->clogic, zs = 0;
            u8t  onf = 0;

            for (ii=0; ii<we->ecnt; ii++) {
               mt_waitentry *cwe = we->we+ii;
               u8t            on = we->sigf[ii];
               onf |= on;
               if (cwe->group) {
                  u32t m_and = cwe->group&we->clogic,
                        m_or = cwe->group^m_and;
                  if (on) {
                     if (m_or) { gs|=m_or; break; }
                  } else
                     gs&=~m_and;
               } else
               if (on) { zs = 1; break; } // the simplest - ungrouped entry
            }
            if (zs) we->signaled = 0; else
            if (gs) we->signaled = bsf32(gs)+1;

            if (we->signaled>=0) {
               /* we have a free mutex here or at least one tid/pid with resaddr,
                  so walk over winner and apply requirements */
               if ((onf&(WOBJ_MUXFREE|WOBJ_EVFREE|WOBJ_SAVERES))) {
                  u32t cv = we->signaled ? 1<<we->signaled-1 : 0;

                  for (ii=0; ii<we->ecnt; ii++) {
                     mt_waitentry *cwe = we->we+ii;
                     if (cwe->group==cv || (cwe->group&cv))
                        if (cwe->htype==QWHT_MUTEX && (we->sigf[ii]&WOBJ_MUXFREE)) {
                           // grab mutex finally
                           we->sigf[ii]  = WOBJ_OK;
                           we->reterr    = mutex_capture(cwe->reserved, we->caller);
                           /// we have an error!
                           if (we->reterr) break;
                        } else
                        if (cwe->htype==QWHT_EVENT && (we->sigf[ii]&WOBJ_EVFREE)) {
                           we->sigf[ii]  = WOBJ_OK;
                           /* reset it here. First, who reach this point, will stop
                              QEVA_PULSEONE action */
                           we->reterr    = event_action(0,0, cwe->reserved, QEVA_RESET);
                           if (we->reterr) break;
                        } else
                        if ((we->sigf[ii]&WOBJ_SAVERES)) {
                           we->sigf[ii]  = WOBJ_OK;
                           // must be non-zero, else data was damaged
                           *cwe->resaddr = cwe->reserved>>32;
                        }
                  }
               }
               // is it wanted just now?
               if (special==we) result = 1;
            }
         }
         // release mt_waitobject() thread
         if (we->signaled>=0)
            update_wait_state(we->caller->tiParent, THWR_WAITOBJ, (u32t)we);
      }
      we = we->next;
   }
   return result;
}
