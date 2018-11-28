#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include "qsmodext.h"

sig_atomic_t   signal_count;
sig_atomic_t  signal_number;
jmp_buf              sigout;

void IntHandler( int signo ) {
   signal_count++;
   signal_number = signo;
}

void AlarmHandler( int signo ) {
   signal_count++;
   signal_number = signo;
   /* here we in secondary fiber of a main thread,
      dump process tree to log to check this */
   mod_dumptree();

   siglongjmp(sigout,1);
}

int main( void ) {
   int ii;

   signal_count = 0;
   signal_number = 0;
   signal(SIGINT, IntHandler);
   signal(SIGALRM, AlarmHandler);

   alarm(20);

   if (setjmp(sigout)==0) {
      printf( "Press Ctrl/C\n" );
      for (ii=0; ii<50; ii++) {
         printf( "Iteration # %d\n", ii);
         usleep(500000); /* sleep for 1/2 second */
         if (signal_count>0) break;
      }
   } else {
      printf( "SIGALRM: we`re still alive after siglongjmp() ;)\n");
   }
   printf("Signal count %d number %d\n", signal_count, signal_number );
   /// stop timer
   alarm(0);

   signal_count = 0;
   signal_number = 0;
   signal(SIGINT, SIG_DFL);      /* Default action */
   printf("Default signal handling\n");
   for (ii=0; ii<50; ii++) {
      printf( "Iteration # %d\n", ii);
      usleep(500000); /* sleep for 1/2 second */
      if (signal_count>0) break; /* Won't happen */
   }
   return signal_count;
}
