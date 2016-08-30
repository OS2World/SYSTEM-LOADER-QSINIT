//
// QSINIT
// le/lx module init/runtime code
//

#define ARGV_LIMIT 128
#define EXTL_LIMIT  32

char *_argv[ARGV_LIMIT];

#include "clib.h"
#include "qsmodint.h"
#include "qsutil.h"

extern char *_CmdLine;
extern u32t _RandSeed;

typedef void (*exit_func)(void);

static exit_func exlst_data [EXTL_LIMIT];
static int       exlst_count = 0;

u32t __stdcall _wc_static_init();
void __stdcall _exit(int status);

/** parse command line and fill _argv.
    @param   cmd    Application path in environment
    @param   argvb  Buffer for argv array, allocated from app stack 
    @return argc */
u32t __stdcall _parse_cmdline(char *cmd, char *argvb) {
   char     *cch, *tch;
   u32t   argc=1, qout = 0;
   process_context *pq = 0;

   if (!_wc_static_init()) _exit(255);

   _RandSeed = tm_counter();
   // save process start time
   pq = mod_context();
   if (pq) pq->rtbuf[RTBUF_STCLOCK] = _RandSeed;
   // parse command line
   _argv[0]  = _CmdLine;
   cch       = _CmdLine+strlen(_CmdLine)+1;
   tch       = argvb;
   while (true) {
      if (*cch) while (*cch==' '||*cch==9) cch++;
      if (*cch==0) break;
      if (argc>=ARGV_LIMIT-1) break;
      _argv[argc++]=tch;
      while (true) {
        int copy    =1;
        int numslash=0;
        while (*cch=='\\') { ++cch; ++numslash; }
        if (*cch=='\"') {
          if ((numslash&1)==0) {
            if (qout) {
              if (cch[1]=='\"') cch++; else copy=0;
            } else copy=0;
            qout=!qout;
          }
          numslash>>=1;
        }
        while (numslash--) *tch++='\\';
        if (*cch==0||(!qout&&(*cch==' '||*cch==9))) break;
        if (copy) *tch++=*cch;
        ++cch;
      }
      if (*cch) cch++;   // skip space which we`re replacing to 0 in next line
      *tch++=0;
   }
   _argv[argc]=0;

   return argc;
}

void __stdcall _process_exit_list(void) {
   while (exlst_count) exlst_data[--exlst_count]();
}

int __stdcall atexit(void (*func)(void)) {
   if (!func||exlst_count==EXTL_LIMIT) return -1;
   exlst_data[exlst_count++] = func;
   return 0;
}
