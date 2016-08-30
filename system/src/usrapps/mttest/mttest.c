/*
   this is a first thread example (really), still no sync in many critical parts of
   QSINIT code, but this code should work :)
*/
#include "stdlib.h"
#include "qsutil.h"
#include "qstask.h"
#include "qsshell.h"
#include "qsmodext.h"
#include "time.h"
#include "qslog.h"

#define TEST_THREADS     20

mt_tid        ta[TEST_THREADS];
qshandle   mutex;
u32t     threads,
          tlsvar;

static u32t _std threadfunc1(void *arg) {
   u32t pid = mod_getpid(),
        tid = mt_getthread();
   log_printf("Hi! I`m thread! :)\n");
   log_printf("My pid is %d, tid %d, arg=\"%s\"\n", pid, tid, arg);
   log_printf("Dumping process tree with me:\n");
   mt_dumptree();
   if (arg) free(arg);
   return 777;
}

static u32t _std threadfunc2(void *arg) {
   qserr  res;
   u64t   now = sys_clock();

   res = mt_tlsset(tlsvar, now);
   if (res) log_printf("I'm tid %2u, mt_tlsset() = %05X\n", mt_getthread(), res);

   log_printf("I'm tid %2u and clock now is %LX\n", mt_getthread(), now);

   res = mt_muxcapture(mutex);
   if (res) log_printf("I'm tid %2u, mt_muxcapture() = %05X\n", mt_getthread(), res);

   threads++;
   log_printf("I'm tid %2u in mutex and clock is %LX\n", mt_getthread(), sys_clock());

//   if (threads==TEST_THREADS-2) mt_dumptree();

   res = mt_muxrelease(mutex);
   if (res) {
      log_printf("I'm tid %2u, mt_muxrelease() = %05X\n", mt_getthread(), res);
      mt_dumptree();
   }
   return 0;
}

static u32t _std threadfunc3(void *arg) {
   u32t  ii;
   // mark self in thread dump
   mt_threadname("launcher");
   // sleep a bit
   usleep(32000);
   // dump a nice table with 22 threads & wait list in 21 entry ;)
   mt_dumptree();
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
         mt_dumptree();
      } else {
         log_printf("I'm unlucky tid %2u, just close file and exit\n", mt_getthread());
      }
      hlp_fclose();
   } else
      log_printf("No QSINIT.LDI?\n");
   return 0;
}

void main(int argc, char *argv[]) {
   mt_tid       tid;
   mt_waitentry  we[TEST_THREADS+1];
   u32t          ii, sig;
   clock_t      stm;
   qserr        res;
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
   tid    = mt_createthread(threadfunc1, 0, 0, cp);
   log_printf("mt_createthread() = %X\n", tid);

   mt_waitthread(tid);

   res    = mt_muxcreate(0, "test mutex", &mutex);
   if (res) { cmd_shellerr(EMSG_QS, res, "Mutex creation error: "); return; }

   for (ii=0; ii<TEST_THREADS; ii++) {
      ta[ii] = mt_createthread(threadfunc2, MTCT_SUSPENDED, 0, 0);
      we[ii].htype = QWHT_TID;
      we[ii].tid   = ta[ii];
      we[ii].group = ii&1 ? 2 : 1;
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

   // this test will fail on PXE (no hlp_fopen())
   log_printf("boot file i/o sync test\n", tid);
   mt_createthread(threadfunc4, 0, 0, 0);
   mt_createthread(threadfunc4, 0, 0, 0);
   // here we exits, but main thread waits for secondary brothers
}
