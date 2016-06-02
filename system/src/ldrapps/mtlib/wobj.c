//
// QSINIT
// wait objects
// ------------------------------------------------------------
//
#include "mtlib.h"
#include "time.h"
#include "qshm.h"
#include "qcl/sys/qsmuxapi.h"

typedef struct {
   we_list_entry   *wle;
   u32t        we_index;
   mux_handle_int   mux;
} w_mux_list;

static we_list_entry  *we_first = 0;

qserr _std mt_waitobject(mt_waitentry *list, u32t count, u32t glogic, u32t *signaled) {
   u32t     ii, groups, zero;
   qserr           err;
   mt_thrdata      *ct;
   we_list_entry  *nwe;
   clock_t       now_t = 0;
   u64t          now_c = 0;
   if (!list || !signaled) return E_SYS_ZEROPTR;
   if (!count) return E_SYS_INVPARM;
   if (!mt_on) return E_MT_DISABLED;

   mt_swlock();
   ct = (mt_thrdata*)pt_current;
   // check parameters
   for (ii=0, err=0, groups=0, zero=0; ii<count && !err; ii++) {
      mt_waitentry *we = list+ii;

      if (we->htype>QWHT_MUTEX) err = E_SYS_INVPARM; else {
         if (!we->group) zero++; else groups|=we->group;
         switch (we->htype) {
            case QWHT_TID:
               if (!we->tid || get_by_tid(pt_current->tiParent, we->tid)==0)
                  err = E_MT_BADTID;
               break;
            case QWHT_PID:
               if (get_by_pid(we->pid)) err = E_MT_BADPID;
               break;
            case QWHT_MUTEX:
               err = E_SYS_UNSUPPORTED;
               break;
         }
      }
   }
   if (err) { mt_swunlock(); return err; }
   ii  = sizeof(we_list_entry) + (count-1)*sizeof(mt_waitentry);
   nwe = (we_list_entry*)malloc(count+ii);
   // wait list entry setup
   nwe->sigf     = (u8t*)nwe + ii;
   nwe->caller   = (mt_thrdata*)pt_current;
   nwe->signaled = -1;
   nwe->ecnt     = count;
   nwe->clogic   = glogic;
   memcpy(&nwe->we, list, count*sizeof(mt_waitentry));
   memset(nwe->sigf, 0, count);

   // convert clock_t to rdtsc ticks
   for (ii=0; ii<count; ii++) {
      mt_waitentry *we = nwe->we + ii;
      if (we->htype==QWHT_CLOCK) {
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
      }
   }
   w_add(nwe);
   
   /* if condition is ready just now - then no additional context switching,
      else switch main thread to waiting state (this reset MT lock to 0!) */ 
   if (!w_check_conditions(0,0,nwe)) {
      pt_current->tiWaitReason = THWR_WAITOBJ;
      pt_current->tiWaitHandle = (u32t)nwe;
      switch_context(0, SWITCH_WAIT);
      // wake up with ready condition
      mt_swlock();
   }
   *signaled = nwe->signaled;
   w_del(nwe);
   mt_swunlock();
   free(nwe);
   return 0;
}

void w_add(we_list_entry *entry) {
   entry->prev = 0;
   entry->next = we_first;
   we_first    = entry;
   if (entry->next) entry->next->prev = entry;
}

void w_del(we_list_entry *entry) {
   if (entry->prev) entry->prev->next = entry->next; else we_first = entry->next;
   if (entry->next) entry->next->prev = entry->prev;
   entry->next = 0;
   entry->prev = 0;
}

/* warning! called from timer interrupt! */
int w_check_conditions(u32t pid, u32t tid, we_list_entry *special) {
   we_list_entry *we = we_first;
   u64t          now = hlp_tscread();
   u32t       result = 0;

   while (we) {
      u32t ii, upcnt = 0;
      // check - is it subject to remove? occurs while process/thread exit only
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
            if (!we->sigf[ii]) {
               mt_waitentry *cwe = we->we+ii;
               switch (cwe->htype) {
                  case QWHT_TID  :
                     if (pid && tid)
                        if (we->caller->tiPID==pid && cwe->tid==tid)
                           we->sigf[ii] = 1;
                     break;
                  case QWHT_PID  :
                     if (pid && !tid)
                        if (cwe->pid==pid) we->sigf[ii] = 1;
                     break;
                  case QWHT_CLOCK:
                     if (cwe->tme!=FFFF64)
                        if (!cwe->tme || now - cwe->reserved >cwe->tme)
                           we->sigf[ii] = 1;
                     break;
                  case QWHT_MUTEX:
                     break;
               }
               upcnt += we->sigf[ii];
            }
         // lucky entires here, check conditions
         if (upcnt) {
            u32t gs = we->clogic, zs = 0;
         
            for (ii=0; ii<we->ecnt; ii++) {
               mt_waitentry *cwe = we->we+ii;
               u8t            on = we->sigf[ii];
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
               if (special==we) result = 1;
               // release mt_waitobject() thread
               update_wait_state(we->caller->tiParent, THWR_WAITOBJ, (u32t)we);
            }
         }
      }
      we = we->next;
   }
   return result;
}
