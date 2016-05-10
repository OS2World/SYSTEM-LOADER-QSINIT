/* 
   this is a first thread example (really), still no sync in most of
   QSINIT code, no waiting, no mutexes, but this code should work :)
*/
#include "stdlib.h"
#include "qsutil.h"
#include "qstask.h"
#include "qsshell.h"
#include "qsmodext.h"
#include "qslog.h"

static u32t _std threadfunc(void *arg) {
   u32t pid = mod_getpid(),
        tid = mt_getthread();
   log_printf("Hi! I`m thread! :)\n");
   log_printf("My pid is %d, tid %d, arg=\"%s\"\n", pid, tid, arg);
   log_printf("Dumping process tree with me:\n");
   mt_dumptree();
   if (arg) free(arg);
   return 777;
}

void main(int argc, char *argv[]) {
   mt_tid tid;
   char   *cp;
   if (!mt_active()) {
      u32t err = mt_initialize();
      if (err) {
         cmd_shellerr(EMSG_QS, err, "MTLIB initialization error: ");
         return;
      }
   }
   cp  = argv[1] ? strdup(argv[1]) : 0;
   tid = mt_createthread(threadfunc, 0, 0, cp);
   log_printf("mt_createthread() = %X\n", tid);
   // here we exits, but main thread will wait his secondary brothers
}
