/* 
   this is a first thread example (really), still no sync in many parts of
   QSINIT code, no mutexes, but this code should work :)
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
clock_t      stm;

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
   log_printf("I'm tid %2u and clock now is %LX\n", mt_getthread(), sys_clock());
   return 0;
}

static u32t _std threadfunc3(void *arg) {
   u32t  ii;
   // sleep a bit
   usleep(32000);
   // dump a nice table with 22 threads & wait list in 21 entry ;)
   mt_dumptree();
   // go!
   stm = sys_clock();
   for (ii=0; ii<TEST_THREADS; ii++) mt_resumethread(0, ta[ii]);
   return 0;
}

void main(int argc, char *argv[]) {
   mt_tid       tid;
   mt_waitentry  we[TEST_THREADS+1];
   u32t          ii, sig;
   qserr        res;
   char         *cp;
   if (!mt_active()) {
      res = mt_initialize();
      if (res) {
         cmd_shellerr(EMSG_QS, res, "MTLIB initialization error: ");
         return;
      }
   }
   cp  = argv[1] ? strdup(argv[1]) : 0;
   tid = mt_createthread(threadfunc1, 0, 0, cp);
   log_printf("mt_createthread() = %X\n", tid);

   mt_waitthread(tid);

   for (ii=0; ii<TEST_THREADS; ii++) {
      ta[ii] = mt_createthread(threadfunc2, MTCT_SUSPENDED, 0, 0);
      we[ii].htype = QWHT_TID;
      we[ii].tid   = ta[ii];
      we[ii].group = ii&1 ? 2 : 1;
   }
   we[TEST_THREADS].htype = QWHT_CLOCK;
   we[TEST_THREADS].group = 0;
   we[TEST_THREADS].tme   = sys_clock() + CLOCKS_PER_SEC;
   /* launch a thread to resume all 20, else some of them can exit before
      we enter mt_waitobject() */
   mt_createthread(threadfunc3, 0, 0, cp);

   /* AND logic for both groups - i.e. all odd or all even finished threads
      will signal here (or 1 sec timeout) */
   res = mt_waitobject(we, TEST_THREADS+1, 3, &sig);
   if (res) {
      cmd_shellerr(EMSG_QS, res, "mt_waitobject() error: ");
   } else {
      printf("Time spent: %Lu mks, ", sys_clock() - stm);
      switch (sig) {
         case 0: printf("timer win!\n"); break;
         case 1: printf("group 1 win!\n"); break;
         case 2: printf("group 2 win!\n"); break;
         default: printf("???\n");
      }
   }
   // here we exits, but main thread will wait for his secondary brothers
   mt_dumptree();
}
