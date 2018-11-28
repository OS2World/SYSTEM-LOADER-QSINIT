#include <stdlib.h>
#include <signal.h>

int tvMain(int argc,char *argv[]);

int main(int argc,char *argv[]) {
   // disable Ctrl-C
   signal(SIGINT,SIG_IGN);

   return tvMain(argc,argv);
}
