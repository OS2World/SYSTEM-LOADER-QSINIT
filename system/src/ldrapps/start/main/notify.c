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
#include "syslocal.h"

typedef struct {
   u32t     evmask;
   u32t        pid;
   sys_eventcb  cb;
} notify_entry;

static notify_entry *cblist = 0;
static u32t         cballoc = 0;

#define ALLOC_STEP  20
#define VALID_MASK  (SECB_QSEXIT|SECB_PAE|SECB_MTMODE|SECB_CMCHANGE|SECB_IODELAY|\
                     SECB_DISKADD|SECB_DISKREM|SECB_CPCHANGE|SECB_LOWMEM|\
                     SECB_SESTART|SECB_SEEXIT|SECB_DEVADD|SECB_DEVDEL|\
                     SECB_SETITLE)
#define ONCE_MASK   (SECB_QSEXIT|SECB_PAE|SECB_MTMODE)
// be more quiet in log
#define QUIET_MASK  (SECB_DEVADD|SECB_DEVDEL|SECB_SESTART|SECB_SEEXIT)
// be silent in log (settile is a noisy nightmare)
#define SILENT_MASK (SECB_SETITLE)

static qserr notifyevent(u32t eventmask, sys_eventcb cbfunc) {
   qserr rc = E_SYS_NOTFOUND;
   /// unknown bits
   if (eventmask&~(VALID_MASK|SECB_GLOBAL|SECB_THREAD))
      return E_SYS_INVPARM;
   /// unable to start thread while QSINIT exits
   if ((eventmask&(SECB_QSEXIT|SECB_THREAD))==(SECB_QSEXIT|SECB_THREAD))
      return E_SYS_INVPARM;
   /// unable to post caller pointers to threads
   if ((eventmask&(SECB_CPCHANGE|SECB_THREAD))==(SECB_CPCHANGE|SECB_THREAD))
      return E_SYS_INVPARM;

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
         rc = 0;
      } else
      if (pos>=0) {
         cblist[pos].cb     = 0;
         cblist[pos].evmask = 0;
         rc = 0;
      }
      mt_swunlock();
   }
   return rc;
}

u32t _std sys_notifyevent(u32t eventmask, sys_eventcb cbfunc) {
   return notifyevent(eventmask, cbfunc) ? 0 : 1;
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

static void _std set_arg_owner(mt_threadfunc thread, void *arg) {
   mem_threadblock(arg);
}
/** run notification function.
    @param   pid     0 - for direct exec, else pid of process where
                         to launch thread
    @param   type    notification code */
void run_notify(sys_eventcb cbfunc, u32t type, u32t info, u32t info2,
                u32t info3, u32t pid)
{
#if 0
   log_it(3, "run_notify(%08X, 0x%X, %08X, %08X, %08X, %u\n", cbfunc, type,
      info, info2, info3, pid);
#endif
   if (pid) {
      int err = 0;
      if (!in_mtmode) err = 1; else {
         // this call should be atomic
         qs_mtlib       mt = get_mtlib();
         mt_ctdata      ts;
         sys_eventinfo *ei = (sys_eventinfo*)malloc(sizeof(sys_eventinfo));
         u32t          tid;

         memset(&ts, 0, sizeof(ts));
         ts.size = sizeof(mt_ctdata);
         ts.stacksize  = _16KB;
         // use QSINIT pid for SECB_GLOBAL
         ts.pid        = pid;
         // guarantee garbage collection for "ei"
         ts.onenter    = set_arg_owner;
         ei->eventtype = type;
         ei->info      = info;
         ei->info2     = info2;
         ei->info3     = info3;
         // cbfunc & thread types are compatible, actually
         tid = mt->thcreate((mt_threadfunc)cbfunc, 0, &ts, ei);
         if (!tid) {
            err = 2;
            free(ei);
         }
      }
      if (err) log_it(2, "%s thread notification (pid %u, %08X)\n",
         err-1?"Error launching":"Unable to make", pid, cbfunc);
   } else {
      sys_eventinfo ei;
      ei.eventtype = type;
      ei.info      = info;
      ei.info2     = info2;
      ei.info3     = info3;
      /* we can recursively walking over this point (at least exit cb
         can cause cm reset and its notification) */
      cbfunc(&ei);
   }
}

void _std sys_notifyexec(u32t evtype, u32t infovalue) {
   u8t called = 0;
   evtype &= VALID_MASK;
   if (!evtype) return;

   mt_swlock();
   if (cblist) {
      u32t ii, once = evtype&ONCE_MASK;
      for (ii=0; ii<cballoc; ii++)
         if (cblist[ii].cb && (cblist[ii].evmask&evtype)) {
            // be a bit more quiet
            if (!called) {
               called = 1;
               if ((evtype&(QUIET_MASK|SILENT_MASK))==0)
                  log_it(2, "notify %06X,%X start\n", evtype, infovalue);
            }
            run_notify(cblist[ii].cb, evtype, infovalue, 0, 0, cblist[ii].evmask&
               SECB_THREAD?(cblist[ii].evmask&SECB_GLOBAL?1:cblist[ii].pid):0);
            // wipe signaled "once" bits and check for empty
            if (once)
               if (((cblist[ii].evmask&=~once)&VALID_MASK)==0)
                  { cblist[ii].cb=0; cblist[ii].evmask=0; }
         }
   }
   mt_swunlock();
   if ((evtype&SILENT_MASK)==0)
      log_it(2, "notify %06X,%X %s\n", evtype, infovalue, called?"done":"no users");
}
