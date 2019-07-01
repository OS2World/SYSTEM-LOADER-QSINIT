/*
   threads/mutexes/events/queue usage test & example
*/
#include "stdlib.h"
#include "qsutil.h"
#include "qstask.h"
#include "qsshell.h"
#include "qsmodext.h"
#include "qsdump.h"
#include "time.h"
#include "math.h"
#include "qsio.h"
#include "qslog.h"

#define TEST_THREADS     20
#define TEST_SCHEDULES    4

mt_tid        ta[TEST_THREADS];
qshandle   mutex,
           event;
u32t     threads,
          tlsvar;

static void _std start_hook(mt_threadfunc thread, void *arg) {
   u32t pid = mod_getpid(),
        tid = mt_getthread();
   log_printf("Hi! I`m thread! :)\n");
   log_printf("My pid is %d, tid %d, arg=\"%s\"\n", pid, tid, arg);
}

static u32t _std threadfunc1(void *arg) {
   log_printf("Dumping process tree with me:\n");
   mod_dumptree(0);
   if (arg) free(arg);
   return 777;
}

static u32t _std threadfunc2(void *arg) {
   qserr   res;
   u64t    now = sys_clock();
   int      i1;
   double   f1;

   res = mt_tlsset(tlsvar, now);
   if (res) log_printf("I'm tid %2u, mt_tlsset() = %05X\n", mt_getthread(), res);

   log_printf("I'm tid %2u and clock now is %LX\n", mt_getthread(), now);

   i1 = random(2000);
   f1 = i1/2.5;
   log_printf("I'm tid %2u, float test: x=%i==%g 0.4x=%g sqrt=%g\n", mt_getthread(),
      i1, f1*2.5, f1, sqrt(i1));

   res = mt_muxcapture(mutex);
   if (res) log_printf("I'm tid %2u, mt_muxcapture() = %05X\n", mt_getthread(), res);

   threads++;
   log_printf("I'm tid %2u in mutex and clock is %LX\n", mt_getthread(), sys_clock());

//   if (threads==TEST_THREADS-2) mt_dumptree();

   res = mt_muxrelease(mutex);
   if (res) {
      log_printf("I'm tid %2u, mt_muxrelease() = %05X\n", mt_getthread(), res);
      mod_dumptree(0);
   }
   return 0;
}

static u32t _std threadfunc3(void *arg) {
   u32t  ii;
   // mark self in thread dump
   mt_threadname("launcher");
#if 1
   /* wait until main thread reach mt_waitobject(),
      see qspdata.inc (3 == THRD_WAITING) */
   while (mt_checkpidtid(mod_getpid(), 1, &ii)==0)
      if ((ii&0xFFFF)==3) break; else mt_yield();
#else
   // sleep a bit
   usleep(32000);
#endif
   // dump a nice table with 22 threads & wait list in 21 entry ;)
   mod_dumptree(0);
   // go!
   for (ii=0; ii<TEST_THREADS; ii++) mt_resumethread(0, ta[ii]);
   return 0;
}

static u32t _std threadfunc4(void *arg) {
   static once = 0;
   u32t size = hlp_fopen("QSINIT.LDI");

   if (size!=FFFF) {
      /* can use static var here, because hlp_fopen() should lock mutex and
         second thread will wait on it until hlp_fclose() below */
      if (++once==1) {
         log_printf("I'm tid %2u, grab file and going to sleep a bit\n", mt_getthread());
         usleep(CLOCKS_PER_SEC*5);
      } else {
         log_printf("I'm unlucky tid %2u, just close file and exit\n", mt_getthread());
      }
      hlp_fclose();
   } else
      log_printf("No QSINIT.LDI?\n");
   return 0;
}

static u32t _std threadfunc5(void *arg) {
   mt_waithandle(event, FFFF);
   //usleep(16000);
   log_printf("I'm event thread %2u and I got it!\n", (u32t)arg);
   return 0;
}

void main(int argc, char *argv[]) {
   mt_tid       tid;
   mt_waitentry  we[TEST_THREADS+1];
   u32t          ii, sig;
   clock_t      stm;
   qserr        res;
   qshandle     que;
   mt_ctdata    tsd;
   char         *cp;
   if (!mt_active()) {
      res = mt_initialize();
      if (res) {
         cmd_shellerr(EMSG_QS, res, "MTLIB initialization error: ");
         return;
      }
   }
   tlsvar = mt_tlsalloc();
   cp     = argv[1] ? strdup(argv[1]) : 0;
   memset(&tsd, 0, sizeof(tsd));
   tsd.size      = sizeof(tsd);
   tsd.stacksize = 8192;
   tsd.onenter   = start_hook;

   tid    = mt_createthread(threadfunc1, 0, &tsd, cp);
   log_printf("mt_createthread() = %X\n", tid);

   mt_waitthread(tid);

   res    = mt_muxcreate(0, "test mutex", &mutex);
   if (res) { cmd_shellerr(EMSG_QS, res, "Mutex creation error: "); return; }

   for (ii=0; ii<TEST_THREADS; ii++) {
      ta[ii] = mt_createthread(threadfunc2, MTCT_SUSPENDED, 0, 0);
      we[ii].htype   = QWHT_TID;
      we[ii].tid     = ta[ii];
      we[ii].group   = ii&1 ? 2 : 1;
      we[ii].resaddr = 0;
   }
   we[TEST_THREADS].htype = QWHT_CLOCK;
   we[TEST_THREADS].group = 0;
   we[TEST_THREADS].tme   = (stm = sys_clock()) + CLOCKS_PER_SEC;
   /* launch a thread to resume all 20, else some of them can exit before
      we enter mt_waitobject() */
   mt_createthread(threadfunc3, 0, 0, cp);

   /* AND logic for both groups - i.e. all odd or all even finished threads
      will signal here (or 1 sec timeout). */
   res = mt_waitobject(we, TEST_THREADS+1, 3, &sig);
   stm = sys_clock() - stm;
   if (res) {
      cmd_shellerr(EMSG_QS, res, "mt_waitobject() error: ");
   } else {
      printf("Time spent: %Lu mks, ", stm);
      switch (sig) {
         case 0: printf("timer win!\n"); break;
         case 1: printf("group 1 win!\n"); break;
         case 2: printf("group 2 win!\n"); break;
         default: printf("???\n");
      }
   }
   // wait for remaining threads
   for (sig=0;!sig;) {
      mt_muxcapture(mutex);
      if (threads==TEST_THREADS) sig = 1;
      mt_muxrelease(mutex);
   }

   res = mt_closehandle(mutex);
   if (res) { cmd_shellerr(EMSG_QS, res, "Mutex free error: "); return; }

   res = mt_eventcreate(0, "test_event", &event);
   if (res) { cmd_shellerr(EMSG_QS, res, "Event creation error: "); return; }

   /* just a test of QEVA_PULSEONE event handling: create 20 threads and wait for
      an event in all of them.
      Then, in loop - call pulse one and wait for any thread 20 times */
   for (ii=1; ii<=TEST_THREADS; ii++) mt_createthread(threadfunc5, 0, 0, (void*)ii);
   /* let threads above to reach mt_waithandle() string - else first pulse will be
      be lost and code stopped on mt_waitthread() forever */
   usleep(32000);
   for (ii=0; ii<TEST_THREADS; ii++) {
      mt_eventact(event, QEVA_PULSEONE);
      mt_waitthread(0);
   }

   // this test will fail on PXE (no hlp_fopen())
   log_printf("boot file i/o sync test\n", tid);
   mt_createthread(threadfunc4, 0, 0, 0);
   mt_createthread(threadfunc4, 0, 0, 0);

   /* queue event scheduling test.
      Post 4 events to the future and then check delivery time */
   res = qe_create(0, &que);
   if (res) { cmd_shellerr(EMSG_QS, res, "Queue creation error: "); return; } else 
   {
      static u32t timediff[TEST_SCHEDULES] = { 32, 38, 64, 126 };
      qe_eid          eids[TEST_SCHEDULES];
      stm = sys_clock();

      for (ii=0; ii<TEST_SCHEDULES; ii++) {
         eids[ii] = qe_schedule(que, stm+timediff[ii]*1000, ii+1, 0, 0, 0);
         if (!eids[ii]) printf("Schedule # %u error!\n", ii+1); else
            if (eids[ii]==QEID_POSTED) printf("Schedule # %u is too late!\n", ii+1);
      }
      for (ii=0; ii<TEST_SCHEDULES; ii++) {
         qe_event *ev = qe_waitevent(que, 5000);
         if (!ev) {
            printf("Event is lost? (%Lu)\n", sys_clock() - stm);
            break;
         } else {
            clock_t  now = sys_clock(),
                  wanted = stm+timediff[ev->code-1]*1000;

            printf("Event %u, time now %LX, should be %LX, diff = %Li mks\n",
               ev->code, now, wanted, now-wanted);
            free(ev);
         }
      }
   }
}
