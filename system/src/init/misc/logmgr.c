#define LOG_INTERNAL
#define MODULE_INTERNAL
#define STORAGE_INTERNAL
#include "clib.h"
#include "qslog.h"
#include "qsmod.h"
#include "qsint.h"
#include "qsinit.h"
#include "vio.h"
#include "qsutil.h"
#include "qsstor.h"
#include "qsinit.h"

#define CNT_EXITLIST    256  // exitlist size

u32t int_mem_need = sizeof(exit_callback)*CNT_EXITLIST;
extern stoinit_entry storage_w[STO_BUF_LEN];

extern u16t             logbufseg;
extern u8t*              logrmbuf;
extern u16t              logrmpos;
extern u16t           ComPortAddr;
extern u32t              BaudRate;
extern mod_addfunc *mod_secondary; // secondary function table, from "start" module
static int           in_exit_call = 0;
volatile exit_callback  *exitlist;
u32t             FileCreationTime; // custom file creation time (for unzip)

// real mode far functions!
void setbaudrate(void);
void earlyserinit(void);
void rmvtimerirq(void);
void poweroffpc(void);

// some externals
int  _std _snprint(char *buf, u32t count, const char *fmt, long *argp);
void _std cache_ctrl(u32t action, u8t vol);

/// put message to real mode log delay buffer
void log_buffer(int level, const char* msg);

void _std exit_prepare(void) {
   u32t ii;
   // remove real mode irq0 handler, used for speaker sound
   rmcall(rmvtimerirq,0);
   cache_ctrl(CC_FLUSH, DISK_LDR);
   // process exit list
   if (in_exit_call||!exitlist) return;
   in_exit_call = 1;

   for (ii=0;ii<CNT_EXITLIST;ii++)
      if (exitlist[ii]) (*exitlist[ii])();
   in_exit_call = 0;
}

void _std exit_handler(exit_callback func, u32t add) {
   u32t ii, *ptr;
   if (!exitlist) return;
   //log_misc(2,"exit_handler(%08X,%d)\n",func,add);
   do {
      ptr = memchrd ((u32t*)exitlist, (u32t)func, CNT_EXITLIST);
      if (ptr) *ptr = 0;
   } while (ptr);
   if (add) {
      ptr = memchrd ((u32t*)exitlist, 0, CNT_EXITLIST);
      if (ptr) *ptr = (u32t)func;
   }
}

// ******************************************************************

static u16t check_rate[10] = {150,300,600,1200,2400,4800,9600,19200,38400,57600};

int _std hlp_seroutset(u16t port, u32t baudrate) {
   if (baudrate)
      if (baudrate==115200 || memchrw(check_rate,baudrate,10)!=0) 
         BaudRate = baudrate;
      else 
         return 0;
   if (port==0xFFFF) ComPortAddr = 0; else
   if (port) {
      ComPortAddr = port;
      rmcall(earlyserinit,0);
   } else
   if (baudrate && ComPortAddr) rmcall(setbaudrate,0);
   return 1;
}

// ******************************************************************

void biostest(void);

u32t _std hlp_querybios(u32t index) {
   return rmcall(biostest,1,index);
}

u32t _std exit_poweroff(int suspend) {
   u32t ver = hlp_querybios(QBIO_APM);
   if (ver&0x10000 || !suspend && ver<0x102) return 0;
   vio_clearscr();
   if (!suspend) {
      hlp_fdone();
      exit_prepare();
      hlp_seroutstr("bye!\n");
   }
   rmcall(poweroffpc,1,suspend?2:3);
   return 1;
}

/**************************************************************************/

void exit_init(u8t *memory) {
   exitlist = (exit_callback*)memory;
}

/* internal version of log_it, start module will export
   actual version for all other users */
static int _std _log_it(int level, const char *fmt, long *argp) {
   char buf[144], *dst=&buf;
   int rc=_snprint(dst, 144, fmt, argp);
   hlp_seroutstr(dst);
   // init log delay buffer in first printf
   if (!logrmbuf) logrmbuf = (u8t*)hlp_segtoflat(logbufseg);
   // print to log or to delay buffer
   if (mod_secondary) (*mod_secondary->log_push)(level, dst); 
      else log_buffer(level|LOGIF_DELAY,dst);
   return rc;
}

int __cdecl log_printf(const char *fmt,...) {
   return _log_it(1, fmt, ((long*)&fmt)+1);
}

int _std log_vprintf(const char *fmt, long *argp) {
   return _log_it(1, fmt, argp);
}

int __cdecl log_it(int level, const char *fmt, ...) {
   return _log_it(level&LOGIF_LEVELMASK, fmt, ((long*)&fmt)+1);
}

void _std log_flush(void) {
   if (!mod_secondary) return;
   /* flush log entries after rm
      note: logrmpos points to zero in last string, this used to append
            string in RM */
   if (logrmpos) {
      u8t *lp = logrmbuf, *ep = logrmbuf+logrmpos;
      while (lp<ep) {
         int  lvl = *lp++;
         u32t len;
         if (!lvl) break;
         len = strlen(lp);

         (*mod_secondary->log_push)(lvl,lp);
         lp+=len+1;
      }
      logrmpos = 0;
   }
   // flush storage changes in rm
   if (storage_w[0].name[0]) (*mod_secondary->sto_flush)();
}

/**************************************************************************/

u32t get_fattime(void) {
   if (FileCreationTime) return FileCreationTime;
   return mod_secondary?mod_secondary->getdate():0;
}

/**************************************************************************/

/* internal version of sto_save, start module will export
   actual version for all other users.
   Only 4 bytes length accepted here and short key names (<=11 chars) */
void sto_save(const char *entry, void *data, u32t len, int copy) {
   if (!entry) return;
   if (mod_secondary) (*mod_secondary->sto_save)(entry,data,len,copy); else {
      int ii, lz=-1;
      if (data && copy && len!=4) return;

      for (ii=0;ii<STO_BUF_LEN;ii++) {
         stoinit_entry *se = storage_w+ii;
         if (!se->name[0]) {
            if (lz<0) lz=ii; 
         } else
         if (strnicmp(se->name,entry,11)==0)
            if (!data) { se->name[0]=0; return; } else 
               { lz=ii; break; }
      }
      if (lz>=0) {
         stoinit_entry *se = storage_w+lz;
         strncpy(se->name,entry,11); se->name[10]=0;
         se->data  = copy?*(u32t*)data:(u32t)data;
         se->len   = len;
         se->isptr = !copy;
      }
   }
}

stoinit_entry *sto_init(void) {
   return storage_w;
}