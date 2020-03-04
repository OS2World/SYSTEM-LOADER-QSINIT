//
// QSINIT "start" module
// queue processing
// -------------------------------------
//

#include "syslocal.h"
#include "qstask.h"
#include "qserr.h"
#include "qssys.h"
#include "sysio.h"
#include "classes.hpp"

#define SQUE_SIGN        0x45555153   ///< SQUE string
#define SQEV_SIGN        0x56455153   ///< SQEV string
#define SQWE_SIGN        0x45575153   ///< SQWE string

/** queue internal data.
    @attention access to lists must use MT lock, because queue state check
          can be done from wait object processing duaring thread switching! */
struct sys_queue {
   u32t         sign;
   TPtrList       uq;  ///< use queue
   TPtrList       fq;  ///< future events (scheduled)
   u32t       sft_no;
};

struct sched_event {
   u32t         sign;
   sched_event *prev,
               *next;  ///< next entry
   qshandle    owner;  ///< queue`s handle, was used to schedule
   qe_event      *ev;  ///< event to post
   sys_queue     *qi;  ///< queue to post (cached value for callback)
};

// system global sorted event list
static sched_event *se_list = 0;

/* called from context switching!!
   should be fast and stealth! */
static qsclock _std cbattime(void **pusrdata) {
   sched_event *chk = (sched_event*)*pusrdata;
   if (chk!=se_list) {
      log_it(0, "cb attime head mismatch! (%X->%X %X->%X)\n", 
         se_list, se_list?se_list->next:0, chk, chk?chk->next:0);
      return 0;
   } else {
      clock_t et = se_list->ev->evtime;
      // post all events with 100 mks resolution
      do {
         se_list->qi->uq.Add(se_list->ev);

         int idx = se_list->qi->fq.IndexOf(se_list);
         if (idx>=0) se_list->qi->fq.Delete(idx);

         hlp_blistdel(se_list, offsetof(sched_event,prev), (void**)&se_list, 0);
      } while (se_list && se_list->ev->evtime - et <= 100);
      // update for next check
      *pusrdata = se_list;

      return se_list ? se_list->ev->evtime-et : 0;
   }
}

static sched_event* eid_add(qshandle owner, sys_queue *qi, qe_event *ev) {
   sched_event *res = new sched_event;
   clock_t      eft = ev->evtime;
   res->sign    = SQWE_SIGN;
   res->owner   = owner;
   res->ev      = ev;
   res->qi      = qi;
   // insert to save accending time order
   if (!se_list) {
      res->next = 0;
      res->prev = 0;
      se_list   = res;
   } else {
      sched_event *elp = se_list,
                  *epp = 0;
      while (elp)
         if (elp->ev->evtime<eft) { epp = elp; elp = elp->next; } else break;
      if (epp) epp->next = res; else se_list = res;
      res->next = elp;
      res->prev = epp;
      if (elp) elp->prev = res;
   }
   if (mtmux) mtmux->callat(se_list?se_list->ev->evtime:0, cbattime, se_list);
   return res;  
}

static void eid_unlink(sched_event *ev_l, sys_queue *qi) {
   hlp_blistdel(ev_l, offsetof(sched_event,prev), (void**)&se_list, 0);

   int idx = qi->fq.IndexOf(ev_l);
   if (idx<0) log_it(0, "qe_eid %X not found in queue!\n");
      else qi->fq.Delete(idx);

   ev_l->sign = 0;
   ev_l->prev = 0;
   ev_l->next = 0;
   if (mtmux) mtmux->callat(se_list?se_list->ev->evtime:0, cbattime, se_list);
}

/** creates a new queue.
   @param  name             Global queue name (optional, can be 0). Named
                            queues can be opened via qe_open().
   @param  [out] res        Queue handle on success.
   @retval 0                on success
   @retval E_MT_DUPNAME     duplicate queue name */
qserr _std qe_create(const char *name, qshandle *res) {
   qserr  rc = 0;
   u32t  fno;
   if (!res) return E_SYS_ZEROPTR;
   if (name && !*name) name = 0;
   *res = 0;

   mt_swlock();
   fno  = sft_find(name, NAMESPC_QUEUE);

   if (fno) rc = E_MT_DUPNAME; else {
      sys_queue      *sq = 0;
      u32t           pid = mod_getpid();
      u32t          ifno = ioh_alloc(),
                     fno = sft_alloc();
      sft_entry      *fe = 0;

      if (!ifno || !fno) rc = E_SYS_NOMEM; else {
         sq = new sys_queue;
         if (sq) {
            sq->sign    = SQUE_SIGN; 
            sq->sft_no  = fno;
         } else
            rc = E_SYS_NOMEM;
      }
      if (!rc) {
         fe = (sft_entry*)malloc(sizeof(sft_entry));
         fe->sign       = SFTe_SIGN;
         fe->type       = IOHT_QUEUE;
         fe->fpath      = name?strdup(name):0;
         fe->name_space = NAMESPC_QUEUE;
         fe->pub_fno    = SFT_PUBFNO+fno;
         fe->que.qi     = sq;
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

/** open existing queue.
   @param  name             Queue name.
   @param  [out] res        Queue handle on success.
   @retval 0                on success
   @retval E_SYS_NOPATH     no such queue */
qserr _std qe_open(const char *name, qshandle *res) {
   qserr  rc = 0;
   u32t  fno;
   if (!res) return E_SYS_ZEROPTR;
   *res = 0;
   if (name && !*name) name = 0;
   if (!name) return E_SYS_INVPARM;

   mt_swlock();
   fno  = sft_find(name, NAMESPC_QUEUE);

   if (!fno) rc = E_SYS_NOPATH; else {
      u32t        pid = mod_getpid();
      u32t       ifno = ioh_alloc();
      sft_entry   *fe = sftable[fno];

      if (!ifno) rc = E_SYS_NOMEM; else
      if (fe->type!=IOHT_QUEUE) rc = E_SYS_INVHTYPE;

      if (!rc) {
         ioh_setuphandle(ifno, fno, pid);
         if (!rc) *res = IOH_HBIT+ifno;
      }
   }
   mt_swunlock();
   return rc;
}

qserr _std qe_close(qshandle queue) {
   io_handle_data *fh;
   sft_entry      *fe;
   qserr          err;
   // this call LOCKs us if err==0
   err = io_check_handle(queue, IOHT_QUEUE, &fh, &fe);
   if (err) return err; else {
      queue ^= IOH_HBIT;
      // unlink it from sft entry & ioh array
      ioh_unlink(fe, queue);
      fh->sign = 0;
      // we reach real close here!
      if (fe->ioh_first==0) {
         sys_queue *qi = (sys_queue*)fe->que.qi;
         // free all scheduled entries
         for (u32t ii=0; ii<qi->fq.Count(); ii++) {
            sched_event *ev_l = (sched_event*)qi->fq[ii];
            eid_unlink(ev_l, qi);
            free(ev_l->ev);
            delete ev_l;
         }
         qi->fq.Clear();
         // common SFT free part
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

qserr _std qe_clear(qshandle queue) {
   sft_entry *fe;
   // this call LOCKs us if err==0
   qserr     err = io_check_handle(queue, IOHT_QUEUE, 0, &fe);
   if (!err) {
      sys_queue *qi = (sys_queue*)fe->que.qi;
      if (qi->uq.Count()) {
         // make a copy of events array and free ptrs after unlock
         TPtrList l2f(qi->uq);
         qi->uq.Clear();
         mt_swunlock();
         for (u32t ii=0; ii<l2f.Count(); ii++) free(l2f[ii]);
      } else
         mt_swunlock();
   }
   return err;
}

/* Low level function for wait object implementation, can be called
   from context switching (timer interrupt). Have no locks inside, i.e.
   assumes MT locked state. */
int _std qe_available_spec(qshandle queue, u32t *ppid, u32t *sft_no) {
   sft_entry      *fe;
   io_handle_data *fh;
   qserr          err = io_check_handle_spec(queue, IOHT_QUEUE, ppid?&fh:0, &fe);
   if (!err) {
      sys_queue *qi = (sys_queue*)fe->que.qi;
      if (ppid)     *ppid = fh->pid;
      if (sft_no) *sft_no = qi->sft_no;
      return qi->uq.Count();
   }
   return -1;
}

// call for io_dumpsft() dump, MT lock assumed
extern "C"
int _std qe_available_info(void *pi, u32t *n_sched) {
   sys_queue *qi = (sys_queue*)pi;
   if (n_sched) *n_sched = 0;
   if (qi->sign!=SQUE_SIGN) return -1;
   if (n_sched) *n_sched = qi->fq.Count();
   return qi->uq.Count();
}

static qserr get_index(qshandle queue, u32t index, qe_event **rce, int peek = 0) {
   sft_entry *fe;
   // this call LOCKs us if err==0
   qserr     err = io_check_handle(queue, IOHT_QUEUE, 0, &fe);
   *rce = 0;
   if (!err) {
      sys_queue *qi = (sys_queue*)fe->que.qi;
      // log_it(2, "qi = %X, uq size: %u, index = %u \n", qi, qi->uq.Max(), index);
      qe_event *res = (qe_event *)(index<qi->uq.Count() ? qi->uq[index] : 0);
      if (res && !peek) qi->uq.Delete(index);
      mt_swunlock();
      *rce = res;
   }
   return err;
}

qe_event* _std qe_takeevent(qshandle queue, u32t index) {
   qe_event *rc = 0;
   get_index(queue, index, &rc);
   // make it caller process owned
   if (rc) mem_localblock(rc);
   return rc;
}

qe_event* _std qe_peekevent(const qshandle queue, u32t index) {
   qe_event *rc = 0;
   get_index(queue, index, &rc, 1);
   return rc;
}

qe_event* _std qe_waitevent(qshandle queue, u32t timeout_ms) {
   qe_event *rc = 0;
   qserr    err = get_index(queue, 0, &rc);

   if (err) return 0; else
   if (!rc && timeout_ms && in_mtmode) {
      qs_mtlib     mt = get_mtlib();
      mt_waitentry we[3] = {{QWHT_QUEUE,1}, {QWHT_SIGNAL,2}, {QWHT_CLOCK,0}};
      u32t        sig;
      we[0].wh  = queue;
      if (timeout_ms!=FFFF) we[2].tme = sys_clock() + (u64t)timeout_ms*1000;
      while (1) {
         sig = 0;
         err = mt->waitobject(we, timeout_ms==FFFF?2:3, 0, &sig);
         //log_it(2, "qe_waitevent(%08X) err = %X, sig = %u \n", queue, err, sig);
         // error or timeout
         if (err || sig==0) break;
         if (sig==1) { err = get_index(queue, 0, &rc); break; }
      }
   }
   // make it caller`s process owned
   if (rc) mem_localblock(rc);
   return rc;
}

int _std qe_available(qshandle queue) {
   sft_entry   *fe;
   qserr       err;
   // this call LOCKs us if err==0
   err = io_check_handle(queue, IOHT_QUEUE, 0, &fe);
   if (!err) {
      sys_queue *qi = (sys_queue*)fe->que.qi;
      int       res = qi->uq.Count();
      mt_swunlock();
      return res;
   }
   return -1;
}

// assumes MT locked state
static qe_eid pushto(qshandle owner, sys_queue *qi, qe_event *src, clock_t at = 0) {
   /* here we allocate block in this module context, this makes it global. All
      of event returning functions will change owner to the current process */
   qe_event *ev = (qe_event*)malloc(sizeof(qe_event));
   qe_eid   res = QEID_POSTED;
   clock_t  now = sys_clock();
   ev->sign     = SQEV_SIGN;
   ev->code     = src->code;
   ev->evtime   = at?at:now;
   ev->a        = src->a;
   ev->b        = src->b;
   ev->c        = src->c;
   ev->id       = 0;
   if (now<at) {
      sched_event *ev_l = eid_add(owner, qi, ev);
      qi->fq.Add(ev_l); 
      res    = (qe_eid)ev_l;
      ev->id = res;
   } else
      qi->uq.Add(ev);
   return res;
}

qserr _std qe_postevent(qshandle queue, u32t code, long a, long b, long c) {
   sft_entry   *fe;
   qserr       err;
   // this call LOCKs us if err==0
   err = io_check_handle(queue, IOHT_QUEUE, 0, &fe);
   if (!err) {
      qe_event ev;
      ev.code=code; ev.a=a; ev.b=b; ev.c=c;
      pushto(queue, (sys_queue*)fe->que.qi, &ev);
      mt_swunlock();
   }
   return err;
}

qserr _std qe_sendevent(qshandle queue, qe_event *event) {
   sft_entry   *fe;
   qserr       err;
   if (!event) err = E_SYS_ZEROPTR;
   // this call LOCKs us if err==0
   if (!err) err = io_check_handle(queue, IOHT_QUEUE, 0, &fe);
   if (!err) {
      pushto(queue, (sys_queue*)fe->que.qi, event);
      mt_swunlock();
   }
   return err;
}

qe_eid _std qe_schedule(qshandle queue, clock_t attime, u32t code, 
                        long a, long b, long c)
{
   sft_entry   *fe;
   qserr       err;
   qe_eid      res = 0;
   if (!in_mtmode) return 0;
   // this call LOCKs us if err==0
   err = io_check_handle(queue, IOHT_QUEUE, 0, &fe);
   if (!err) {
      qe_event ev;
      ev.code=code; ev.a=a; ev.b=b; ev.c=c;
      res = pushto(queue, (sys_queue*)fe->que.qi, &ev, attime);
      mt_swunlock();
   }
   return res;
}

qe_eid _std qe_reschedule(qe_eid eventid, clock_t attime) {
   sft_entry     *fe;
   qserr         err;
   qe_eid        res = 0;
   sched_event *ev_l = (sched_event*)eventid;
   // check eventID
   if (!eventid || eventid==QEID_POSTED || ev_l->sign!=SQWE_SIGN)
      err = E_SYS_INVPARM;
   // this call LOCKs us if err==0
   if (!err) err = io_check_handle(ev_l->owner, IOHT_QUEUE, 0, &fe);
   if (!err) {
      sys_queue *qi = (sys_queue*)fe->que.qi;
      // unlink it
      eid_unlink(ev_l, qi);
      // and link back at new pos
      res = pushto(ev_l->owner, qi, ev_l->ev, attime);
      // update event id field in event
      ev_l->ev->id = res;
      mt_swunlock();
      delete ev_l;
   }
   return res;
}

qe_event* _std qe_unschedule(qe_eid eventid) {
   sft_entry     *fe;
   qserr         err;
   qe_event   *retev = 0;
   sched_event *ev_l = (sched_event*)eventid;
   // check eventID
   if (!eventid || eventid==QEID_POSTED || ev_l->sign!=SQWE_SIGN)
      err = E_SYS_INVPARM;
   // this call LOCKs us if err==0
   if (!err) err = io_check_handle(ev_l->owner, IOHT_QUEUE, 0, &fe);
   if (!err) {
      sys_queue *qi = (sys_queue*)fe->que.qi;
      // unlink it
      eid_unlink(ev_l, qi);
      mt_swunlock();
      retev     = ev_l->ev;
      retev->id = 0;
      delete ev_l;
   }
   // make it caller process owned
   if (retev) mem_localblock(retev);
   return retev;
}
