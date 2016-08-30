//
// QSINIT "start" module
// global system event notification callbacks
// -------------------------------------
//
#include "qssys.h"
#include "clib.h"
#include "malloc.h"
#include "qsint.h"
#include "qslog.h"
#include "malloc.h"
#include "qsmodext.h"
#include "internal.h"

typedef struct {
   u32t     evmask;
   u32t        pid;
   sys_eventcb  cb;
} notify_entry;

static notify_entry *cblist = 0;
static u32t         cballoc = 0;

#define ALLOC_STEP  20
#define VALID_MASK  (SECB_QSEXIT|SECB_PAE|SECB_MTMODE|SECB_CMCHANGE|SECB_IODELAY|\
                     SECB_DISKADD|SECB_DISKREM)
#define ONCE_MASK   (SECB_QSEXIT|SECB_PAE|SECB_MTMODE)

u32t _std sys_notifyevent(u32t eventmask, sys_eventcb cbfunc) {
   u32t rc = 0;
   /// unable to start thread while QSINIT exits
   if ((eventmask&(SECB_QSEXIT|SECB_THREAD))==(SECB_QSEXIT|SECB_THREAD))
      return 0;
   if (cbfunc) {
      u32t   ii;
      s32t  pos = -1;
      mt_swlock();

      /* this handmade list allocation is used to be less dependent on outside
         code. Who knows - which parts can register self as user here.
         I.e., this function is useful just after mem_init() in mod_main() */
      if (cblist)
         for (ii=0; ii<cballoc; ii++)
            if (cblist[ii].cb==cbfunc) { pos = ii; break; }

      if (eventmask) {
         if (pos<0) {
            if (!cblist)
               cblist = (notify_entry*)calloc(cballoc = ALLOC_STEP, sizeof(notify_entry));
            for (ii=0; ii<cballoc; ii++)
               if (!cblist[ii].cb) { pos = ii; break; }
            if (pos<0) {
               pos      = cballoc;
               cballoc += ALLOC_STEP;
               cblist   = (notify_entry*)realloc(cblist, cballoc * sizeof(notify_entry));
               memset(cblist + pos, 0, ALLOC_STEP*sizeof(notify_entry));
            }
         }
         cblist[pos].cb     = cbfunc;
         cblist[pos].evmask = eventmask;
         cblist[pos].pid    = eventmask&SECB_GLOBAL?0:mod_getpid();
         rc = 1;
      } else
      if (pos>=0) {
         cblist[pos].cb     = 0;
         cblist[pos].evmask = 0;
         rc = 1;
      }
      mt_swunlock();
   }
   return rc;
}

void sys_notifyfree(u32t pid) {
   u32t ii;
   if (!pid) return;

   mt_swlock();
   if (cblist)
      for (ii=0; ii<cballoc; ii++)
         if (cblist[ii].pid==pid) { cblist[ii].cb=0; cblist[ii].evmask=0; }
   mt_swunlock();
}

void _std sys_notifyexec(u32t evtype, u32t infovalue) {
   evtype &= VALID_MASK;
   if (!evtype) return;

   log_it(2, "sys_notifyexec(%08X, %X) start\n", evtype, infovalue);
   mt_swlock();
   if (cblist) {
      u32t ii, once = evtype&ONCE_MASK;
      for (ii=0; ii<cballoc; ii++)
         if (cblist[ii].cb && (cblist[ii].evmask&evtype)) {
            if (cblist[ii].evmask&SECB_THREAD) {
               int err = 0;
               if (!in_mtmode) err = 1; else {
                  // this call should be atomic
                  qs_mtlib       mt = get_mtlib();
                  mt_ctdata      ts;
                  sys_eventinfo *ei = (sys_eventinfo*)malloc(sizeof(sys_eventinfo));
                  u32t          tid;

                  memset(&ts, 0, sizeof(ts));
                  ts.size = sizeof(mt_ctdata);
                  ts.stacksize  = _64KB;
                  // use QSINIT pid for SECB_GLOBAL
                  ts.pid        = cblist[ii].pid ? cblist[ii].pid : 1;
                  ei->eventtype = evtype;
                  ei->info      = infovalue;
                  // cbfunc & thread types are compatible, actually
                  tid = mt->createthread((mt_threadfunc)cblist[ii].cb,
                                         MTCT_SUSPENDED, &ts, ei);
                  if (!tid) {
                     err = 2;
                     free(ei);
                  } else {
                     // just guarantee garbage collection for this block
                     mem_threadblockex(ei, ts.pid, tid);
                     mt->resumethread(ts.pid, tid);
                  }
               }
               if (err) log_it(2, "%s thread notification (pid %u, %08X)\n",
                  err-1?"Error launching":"Unable to make", cblist[ii].pid,
                     cblist[ii].cb);
            } else {
               sys_eventinfo ei;
               ei.eventtype = evtype;
               ei.info      = infovalue;
               /* we can recursively walking over this point (at least exit cb
                  can cause cm reset and its notification) */
               cblist[ii].cb(&ei);
            }
             // wipe signaled "once" bits and check for empty
            if (once)
               if (((cblist[ii].evmask&=~once)&VALID_MASK)==0)
                  { cblist[ii].cb=0; cblist[ii].evmask=0; }
         }
   }
   mt_swunlock();
   log_it(2, "sys_notifyexec(%08X, %X) done\n", evtype, infovalue);
}
