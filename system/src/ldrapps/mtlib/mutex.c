//
// QSINIT
// mutexes & attime handling
// ------------------------------------------------------------
//
#include "mtlib.h"

#define mutex_instance_ret(src,inst,err) \
   mutex_data *inst = (mutex_data*)src;  \
   if (inst->sign!=MUTEX_SIGN) return err;

#define event_instance_ret(src,inst,err) \
   event_data *inst = (event_data*)src;  \
   if (inst->sign!=EVENT_SIGN) return err;

#define instance_void(src,inst)          \
   mutex_data *inst = (mutex_data*)src;  \
   if (inst->sign!=MUTEX_SIGN) return;

static u32t     mux_classid = 0;
qs_sysmutex     mux_hlpinst = 0;
qs_muxcvtfunc    hlp_cvtmux = 0;
qe_availfunc     hlp_qavail = 0;
qe_getschfunc    hlp_qslist = 0;

/// common init call.
static void _exicc mutex_init(EXI_DATA, qs_muxcvtfunc sf1, qe_availfunc sf2,
   qshandle sys_queue, qe_getschfunc sf3)
{
   hlp_cvtmux = sf1;
   hlp_qavail = sf2;
   hlp_qslist = sf3;
   sys_q      = sys_queue;
}

/// capture mutex to specified thread.
qserr mutex_capture(mux_handle_int muxh, mt_thrdata *who) {
   mutex_instance_ret(muxh, mx, E_SYS_INVOBJECT);
/*
   log_it(0, "mutex %X (%u) capture, waitcnt %u, owner %08X, lock %u, who %08X!\n",
      mx, mx->sft_no, mx->waitcnt, mx->owner, mx->lockcnt, who);
*/
   if (mx->owner)
      if (mx->owner==who) {
         if (mx->lockcnt>=MUTEX_MAXLOCK) return E_MT_LOCKLIMIT;
            else mx->lockcnt++;
         return 0;
      } else
         return E_MT_SEMBUSY;
   mx->owner   = who;
   mx->lockcnt = 1;
   // link into list
   mx->prev    = 0;
   mx->next    = who->tiFirstMutex;
   if (who->tiFirstMutex) ((mutex_data*)who->tiFirstMutex)->prev = mx;
   who->tiFirstMutex = mx;
   return 0;
}

/// create event/mutex.
static qserr _exicc muxev_create(EXI_DATA, mux_handle_int *res, u32t sft_no,
                                 int event, int signaled)
{
   if (event) {
      event_data *rc = (event_data*)calloc(1,sizeof(event_data));
      rc->sign    = EVENT_SIGN;
      rc->sft_no  = sft_no;
      rc->state   = signaled?1:0;
      *res = (mux_handle_int)rc;
   } else {
      mutex_data *rc = (mutex_data*)calloc(1,sizeof(mutex_data));
      rc->sign    = MUTEX_SIGN;
      rc->sft_no  = sft_no;
      *res = (mux_handle_int)rc;
      if (signaled) mutex_capture(*res, (mt_thrdata*)pt_current);
   }
   return 0;
}

/** event action.
    warning! w_check_conditions() calls it with action==QEVA_RESET,
    this means interrupt time call! */
qserr _exicc event_action(EXI_DATA, evt_handle_int evh, u32t action) {
   event_instance_ret(evh, ev, E_SYS_INVOBJECT);
   // process requested action
   switch (action) {
      case QEVA_RESET   : ev->state = 0; return 0;
      case QEVA_SET     :
         ev->state = 1;
         if (ev->waitcnt) w_check_conditions(0,0,0);
         return 0;
      case QEVA_PULSE   :
      case QEVA_PULSEONE:
         if (ev->waitcnt) {
            ev->state = action==QEVA_PULSEONE?2:1;
            w_check_conditions(0,0,0);
         }
         /* 1. it may be always cleared after w_check_conditions()
            2. this is PULSE, so reset it even if was set before */
         ev->state = 0;
         return 0;
   }
   return E_SYS_INVPARM;
}

int _exicc event_state(evt_handle_int evh) {
   event_instance_ret(evh, ev, -1);
   return ev->state;
}

static void mutex_unlink(mutex_data *mx) {
   mutex_data *prev = mx->prev,
              *next = mx->next;
   if (prev) prev->next = mx->next; else
      mx->owner->tiFirstMutex = mx->next;
   if (next) next->prev = mx->prev;
   mx->prev = 0;
   mx->next = 0;
}

qserr mutex_wcounter(mux_handle_int muxh, int inc) {
   u32t sign = *(u32t*)muxh;
   if (sign==MUTEX_SIGN) {
      mutex_data *mx = (mutex_data*)muxh;
      if (inc<0 && -inc>=mx->waitcnt) mx->waitcnt = 0; else mx->waitcnt+=inc;
//      log_it(0, "mutex %X (%u), waitcnt %u!\n", mx, mx->sft_no, mx->waitcnt);
   } else
   if (sign==EVENT_SIGN) {
      event_data *ev = (event_data*)muxh;
      if (inc<0 && -inc>=ev->waitcnt) ev->waitcnt = 0; else ev->waitcnt+=inc;
//      log_it(0, "event %X (%u), waitcnt %u!\n", mx, ev->sft_no, ev->waitcnt);
   } else
      return E_SYS_INVOBJECT;
   return 0;
}

/// release mutex.
static qserr _exicc mutex_release(EXI_DATA, mux_handle_int muxh) {
   qserr  res = 0;
   mutex_instance_ret(muxh, mx, E_SYS_INVOBJECT);

   if (mx->owner==pt_current) {
      if (mx->lockcnt>1) mx->lockcnt--; else {
         mutex_unlink(mx);
         mx->owner   = 0;
         mx->lockcnt = 0;
      }
/*      log_it(0, "mutex %X (%u) release, owner %08X, lock %u!\n", mx,
         mx->sft_no, mx->owner, mx->lockcnt); */
   } else
      res = E_MT_NOTOWNER;

   return res;
}

/// release all mutexes, owned by thread
void mutex_release_all(mt_thrdata *th) {
   u32t cnt = 0;
   if (th->tiSign!=THREADINFO_SIGN) {
      log_it(0, "mutex_release_all(%X)!\n", th);
      THROW_ERR_PQ(mt_exechooks.mtcb_ctxmem);
   }
   // walk over owned mutexes and free one by one
   while (th->tiFirstMutex) {
      mutex_data *mx = (mutex_data*)th->tiFirstMutex;
      if (mx->owner!=th) {
         log_it(0, "bad mutex %u owner (%08X), should be (%08X)\n", mx->sft_no,
            mx->owner, th);
         THROW_ERR_PD(th->tiParent);
      }
      log_it(0, "mutex (sft # %u) is lost!\n", mx->sft_no);
      // force it!
      if (mx->lockcnt>1) mx->lockcnt = 1;
      // and call common release function
      mutex_release(0, 0, (mux_handle_int)mx);
      cnt++;
   }
   if (cnt) log_it(3, "pid %u tid %u (%X) - %u mutex(es) lost!\n", th->tiPID,
      th->tiTID, th, cnt);
}

/// delete event/mutex.
static qserr _exicc muxev_free(EXI_DATA, mux_handle_int muxh, int force) {
   u32t sign = *(u32t*)muxh;
   if (sign==MUTEX_SIGN) {
      mutex_data *mx = (mutex_data*)muxh;
      /* should be free (this function called from module unload callback
         in _parent_ process, at this moment mutex must be released) */
      if (mx->owner)
         if (!force) return E_MT_SEMBUSY; else
            log_it(0, "Free mutex (sft %u), but it owned by pid %u tid %u!\n",
               mx->sft_no, mx->owner->tiPID, mx->owner->tiTID);
      if (mx->waitcnt)
         if (!force) return E_MT_WAITACTIVE; else
            log_it(0, "Free mutex (sft %u) with %u waits!\n", mx->waitcnt);
      if (mx->owner) mutex_unlink(mx);
      mx->waitcnt = 0;
      mx->sign = 0;
      free(mx);
   } else
   if (sign==EVENT_SIGN) {
      event_data *ev = (event_data*)muxh;
      if (ev->waitcnt)
         if (!force) return E_MT_WAITACTIVE; else
            log_it(0, "Free event (sft %u) with %u waits!\n", ev->waitcnt);
      ev->waitcnt = 0;
      ev->sign = 0;
      free(ev);
   } else
      return E_SYS_INVOBJECT;

   return 0;
}

/// is mutex free?
int _exicc mutex_state(EXI_DATA, mux_handle_int muxh, qs_sysmutex_state *info) {
   mutex_instance_ret(muxh, mx, -1);
   if (info) {
      int      free = !mx->owner;
      info->pid     = free ? 0 : mx->owner->tiPID;
      info->tid     = free ? 0 : mx->owner->tiTID;
      info->state   = free ? 0 : mx->lockcnt;
      info->waitcnt = mx->waitcnt;
      info->sft_no  = mx->sft_no;
      info->owner   = mx->owner==pt_current;
   }
   //if (mx->owner) log_printf("owner: %08X (%u)\n", mx->owner, mx->lockcnt);

   return !mx->owner?0:mx->lockcnt;
}


static qserr _exicc callat(EXI_DATA, qsclock at, qs_croncbfunc cb, void *usrdata) {
   int ci, eidx = -1;
   qserr    res = 0;

   mt_swlock();
   // search for empty slot or self
   for (ci=0; ci<ATTIMECB_ENTRIES; ci++) {
      attime_entry *ate = attcb+ci;
      if (!ate->cbf && eidx<0) eidx = ci; else
         if (ate->cbf==cb) { eidx = ci; break; }
   }
   if (eidx<0) res = E_SYS_SOFTFAULT; else
   if (!at) attcb[eidx].cbf = 0; else {
      attime_entry *ate = attcb+eidx;
      qsclock     now_t = sys_clock();
      u64t        now_c = hlp_tscread();

      ate->start_tsc = now_c;
      ate->cbf       = cb;
      ate->usrdata   = usrdata;

      if (at<now_t) at = 0; else {
         at = (at - now_t + 50) / 100;
         // > 80 days
         if (at > _4GBLL*16) { ate->cbf = 0; res = E_SYS_TOOLARGE; }
            else at *= tsc_100mks;
      }
      ate->diff_tsc  = at;

      // log_printf("callat %u %LX %LX\n", eidx, ate->start_tsc, ate->diff_tsc);
   }
   mt_swunlock();
   return res;
}

static void _exicc enumpd(EXI_DATA, qe_pdenumfunc cb, void *usrdata) {
   enum_process(cb, usrdata);
}

static void  _exicc setregs(EXI_DATA, mt_fibdata *fd, struct tss_s *regs) {
   mt_setregs(fd, regs);
}

static mt_prcdata* _exicc getpd(EXI_DATA, u32t pid) {
   mt_swlock();
   /* no unlock here, function exits in MT locked state to preserve process
      data for the caller */
   return get_by_pid(pid);
}

static void *m_list[] = { mutex_init, muxev_create, mutex_release,
                          event_action, muxev_free, mutex_state, callat,
                          enumpd, setregs, getpd};

void register_mutex_class(void) {
   memset(&attcb, 0, sizeof(attcb));
   // something forgotten! interface part is not match to implementation
   if (sizeof(_qs_sysmutex)!=sizeof(m_list)) {
      log_printf("Function list mismatch\n");
      _throw_(xcpt_align);
   }
   /* register private(unnamed) class and make it EXCLUSIVE because we
      have only ONE instance for the START module */
   mux_classid = exi_register(0, m_list, sizeof(m_list)/sizeof(void*), 0,
                              EXIC_EXCLUSIVE, 0, 0, 0);
   mux_hlpinst = exi_createid(mux_classid, EXIF_SHARED);
   if (!mux_classid || !mux_hlpinst)
      log_printf("mutex class registration error!\n");
}
