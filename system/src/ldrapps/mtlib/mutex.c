//
// QSINIT
// mutexes & attime handling
// ------------------------------------------------------------
//
#include "mtlib.h"

#define instance_ret(src,inst,err)       \
   mutex_data *inst = (mutex_data*)src;  \
   if (inst->sign!=MUTEX_SIGN) return err;

#define instance_void(src,inst)          \
   mutex_data *inst = (mutex_data*)src;  \
   if (inst->sign!=MUTEX_SIGN) return;

static u32t     mux_classid = 0;
qs_sysmutex     mux_hlpinst = 0;
qs_muxcvtfunc    hlp_cvtmux = 0;
qe_availfunc     hlp_qavail = 0;

/// common init call.
static void _exicc mutex_init(EXI_DATA, qs_muxcvtfunc sf1, qe_availfunc sf2,
   qshandle sys_queue)
{
   hlp_cvtmux = sf1;
   hlp_qavail = sf2;
   sys_q      = sys_queue;
}

/// create mutex.
static qserr _exicc mutex_create(EXI_DATA, mux_handle_int *res, u32t sft_no) {
   mutex_data *rc = (mutex_data*)calloc(1,sizeof(mutex_data));
   rc->sign    = MUTEX_SIGN;
   rc->sft_no  = sft_no;
   *res = (mux_handle_int)rc;
   return 0;
}

/// capture mutex to specified thread.
qserr mutex_capture(mux_handle_int muxh, mt_thrdata *who) {
   instance_ret(muxh, mx, E_SYS_INVOBJECT);
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
   instance_ret(muxh, mx, E_SYS_INVOBJECT);
   if (inc<0 && -inc>=mx->waitcnt) mx->waitcnt = 0; else mx->waitcnt+=inc;
//   log_it(0, "mutex %X (%u), waitcnt %u!\n", mx, mx->sft_no, mx->waitcnt);
   return 0;
}

/// release mutex.
static qserr _exicc mutex_release(EXI_DATA, mux_handle_int muxh) {
   qserr  res = 0;
   instance_ret(muxh, mx, E_SYS_INVOBJECT);

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

/// delete mutex.
static qserr _exicc mutex_free(EXI_DATA, mux_handle_int muxh, int force) {
   instance_ret(muxh, mx, E_SYS_INVOBJECT);
   /* should be free (this function called from module unload callback
      in _parent_ process, in this moment mutex must be released) */
   if (mx->owner)
      if (!force) return E_MT_SEMBUSY; else
         log_it(0, "Free mutex (sft %u), but it owned by pid %u tid %u!\n",
            mx->sft_no, mx->owner->tiPID, mx->owner->tiTID);
   if (mx->waitcnt)
      if (!force) return E_MT_SEMBUSY; else
         log_it(0, "Free mutex (sft %u) with %u waits!\n", mx->waitcnt);
   if (mx->owner) mutex_unlink(mx);
   mx->waitcnt = 0;
   mx->sign = 0;
   free(mx);
   return 0;
}

/// is mutex free?
int _exicc mutex_state(EXI_DATA, mux_handle_int muxh, qs_sysmutex_state *info) {
   instance_ret(muxh, mx, -1);
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

static void *m_list[] = { mutex_init, mutex_create, mutex_release, mutex_free,
                          mutex_state, callat, enumpd};

void register_mutex_class(void) {
   memset(&attcb, 0, sizeof(attcb));
   // something forgotten! interface part is not match to implementation
   if (sizeof(_qs_sysmutex)!=sizeof(m_list)) {
      log_printf("Function list mismatch\n");
      _throw_(xcpt_align);
   }
   /* register private(unnamed) class and making it EXCLUSIVE because we
      have only one instance for the START module */
   mux_classid = exi_register(0, m_list, sizeof(m_list)/sizeof(void*), 0,
                              EXIC_EXCLUSIVE, 0, 0, 0);
   mux_hlpinst = exi_createid(mux_classid, EXIF_SHARED);
   if (!mux_classid || !mux_hlpinst)
      log_printf("mutex class registration error!\n");
}
