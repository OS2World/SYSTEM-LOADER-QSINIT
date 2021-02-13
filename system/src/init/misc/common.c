#define LOG_INTERNAL
#define STORAGE_INTERNAL
#include "qslocal.h"
#include "qsvdata.h"
#include "qsxcpt.h"
#include "qcl/sys/qsvioint.h"

extern stoinit_entry    storage_w[STO_BUF_LEN];
extern u8t*              logrmbuf;
extern u16t              logrmpos;
static int           in_exit_call = 0;
static u8t            exit_called = 0;
extern u16t              text_col;
extern u8t             cvio_blink;
#ifndef EFI_BUILD
extern u16t             logbufseg;
extern u8t                restirq; // restore IRQs flag

// real mode far functions!
void rmvtimerirq(void);
void getfullSMAP(void);
void poweroffpc(void);
void biostest(void);
#endif

// some externals
int  _std _snprint(char *buf, u32t count, const char *fmt, long *argp);
void _std cpuhlt(void);
u16t _std key_read_int(void);
void _std cvio_getmodefast(u32t *cols, u32t *lines);
void _std cvio_getpos(u32t *line, u32t *col);
void _std cvio_setpos(u8t line, u8t col);
void _std cvio_setcolor(u16t color);
u16t _std cvio_getshape(void);
void _std cvio_setshape(u8t start, u8t end);
void _std cvio_writebuf(u32t col, u32t line, u32t width, u32t height, void *buf, u32t pitch);
void _std cvio_readbuf(u32t col, u32t line, u32t width, u32t height, void *buf, u32t pitch);
int  _std cvio_setmodeex(u32t cols, u32t lines);
void _std cvio_intensity(u8t value);
void _std cb_codepage(sys_eventinfo *info);

/// put message to real mode log delay buffer
void log_buffer(int level, const char* msg);
void hlp_fdone(void);

/** internal call: exit_prepare() was called or executing just now.
    @return 0 - for no, 1 - if called already and 2 if you called from
            exit_handler() callback */
u32t _std exit_inprocess(void) {
   if (exit_called) return 2;
   if (in_exit_call) return 1;
   return 0;
}

void _std exit_prepare(void) {
   if (exit_called) return;
   // lock it FOREWER!
   mt_swlock();
   // recursive call from exit callbacks? then exit
   if (in_exit_call) {
      mt_swunlock();
      return;
   } else {
      in_exit_call = 1;
      // process exit list
      if (mod_secondary) mod_secondary->sys_notifyexec(SECB_QSEXIT, 0);
#ifndef EFI_BUILD
      /* remove real mode irq0 handler, used for speaker sound & tsc calibration,
         call should be after exit notification */
      rmcall(rmvtimerirq,0);
#endif
      // shutdown internal i/o
      hlp_fdone();
      exit_called  = 1;
      in_exit_call = 0;
   }
}

void _std exit_restirq(int on) {
#ifndef EFI_BUILD
   restirq = on?1:0;
#endif
}

// ******************************************************************

static u16t check_rate[10] = {150,300,600,1200,2400,4800,9600,19200,38400,57600};

int _std checkbaudrate(u32t baudrate) {
   return baudrate==115200 || memchrw(check_rate,baudrate,10)!=0;
}

int _std hlp_seroutset(u16t port, u32t baudrate) {
   int rc;
   if (baudrate)
      if (checkbaudrate(baudrate)) BaudRate = baudrate; else
         return 0;
   mt_swlock();
   rc = 1;
   if (port==0xFFFF) ComPortAddr = 0; else {
      if (port) {
         if (!serial_init(ComPortAddr = port)) {
            ComPortAddr = 0;
            rc = 0;
         }
      } else
      if (baudrate && ComPortAddr) hlp_serialrate(ComPortAddr, baudrate);
      // reset lock
      if (mod_secondary && mod_secondary->dbport_lock)
         mod_secondary->dbport_lock = 0;
   }
   mt_swunlock();
   return rc;
}

int _std hlp_serialset(u16t port, u32t baudrate) {
   int rc;
   if (!port) return 0;
   if (!baudrate) baudrate = 115200;
   if (!checkbaudrate(baudrate)) return 0;

   mt_swlock();
   if (port==ComPortAddr)
      if (mod_secondary) mod_secondary->dbport_lock = 1; else ComPortAddr = 0;
   rc = serial_init(port);
   if (rc) hlp_serialrate(port, baudrate); else
      if (mod_secondary) mod_secondary->dbport_lock = 0; 
   mt_swunlock();

   return rc;
}

// ******************************************************************
#ifndef EFI_BUILD
u32t _std hlp_querybios(u32t index) {
   return rmcall(biostest,1,index);
}

u32t _std exit_poweroff(int suspend) {
   u32t ver = hlp_querybios(QBIO_APM);
   if (ver&0x10000 || !suspend && ver<0x102) return 0;
   vio_clearscr();
   if (!suspend) {
      exit_prepare();
      hlp_seroutstr("bye!\n");
   }
   rmcall(poweroffpc,1,suspend?2:3);
   return 1;
}

void int15mem_int(void) {
   rmcall(getfullSMAP,0);
}

void init_host(void) {}
#else
void int15mem_int(void);
#endif

//must be called inside lock
AcpiMemInfo* int15mem_copy(void) {
   typedef struct {
      u64t   BaseAddr;
      u64t     Length;
      u32t       Type;
   } MemEntry;
   MemEntry  *rc, 
            *tbl = (MemEntry*)DiskBufPM;
   u32t      cnt = 0;
   while (tbl[cnt].Length) cnt++;
   cnt++;
   cnt *= sizeof(MemEntry);
   rc   = (MemEntry*)mod_secondary->mem_realloc(0, cnt);
   // optimize it a bit (merge blocks)
   cnt  = 0;
   while (tbl->Length) {
      if (cnt && rc[cnt-1].Type==tbl->Type &&
         rc[cnt-1].BaseAddr+rc[cnt-1].Length==tbl->BaseAddr)
      {
         rc[cnt-1].Length+=tbl->Length;
         tbl++;
         continue;
      }
      memcpy(rc+cnt++, tbl++, sizeof(MemEntry));
   }
   memset(rc+cnt, 0, sizeof(MemEntry));

   return (AcpiMemInfo*)rc;
}

AcpiMemInfo* _std hlp_int15mem(void) {
   if (!mod_secondary) return 0; else {
      AcpiMemInfo *rc;
      // static buffer (disk i/o buffer) is used here
      mt_swlock();
      int15mem_int();
      rc = int15mem_copy();
      mt_swunlock();

      return rc;
   }
}

/**************************************************************************/

/* internal version of log_it, start module will export
   actual version for all other users */
static int _std _log_it(int level, const char *fmt, long *argp) {
   char buf[144], *dst=&buf;
   int rc=_snprint(dst, 144, fmt, argp);
   hlp_seroutstr(dst);
#ifndef EFI_BUILD
   // init log delay buffer in first printf
   if (!logrmbuf) logrmbuf = (u8t*)hlp_segtoflat(logbufseg);
#endif
   // print to log or to delay buffer
   if (mod_secondary) (*mod_secondary->log_push)(level, dst, 0);
      else log_buffer(level|LOGIF_DELAY,dst);
   return rc;
}

int __cdecl log_printf(const char *fmt,...) {
   return _log_it(1, fmt, ((long*)&fmt)+1);
}

int _std _log_vprintf(const char *fmt, long *argp) {
   return _log_it(1, fmt, argp);
}

int __cdecl log_it(int level, const char *fmt, ...) {
   return _log_it(level&LOGIF_LEVELMASK, fmt, ((long*)&fmt)+1);
}

void _std log_flush(void) {
   volatile static u32t recursive = 0, firsttime = 1;
   // START still not loaded
   if (!mod_secondary) return;
   // we`re called from log_push()
   if (mt_cmpxchgd(&recursive,1,0)) return;
   mt_swlock();
   /* flush log entries after rm/efi call
      note: logrmpos points to zero in last string, this used to append
            string in RM/EFI part */
   if (logrmpos) {
      u8t *lp = logrmbuf, *ep = logrmbuf+logrmpos;
      while (lp<ep) {
         int  lvl = *lp++;
         u32t len, time;
         if (!lvl) break;
         // message time
         time = *(u32t*)lp; lp+=4;
         // message text
         len = strlen(lp);

         mod_secondary->log_push(lvl,lp,time);
         lp+=len+1;
      }
      logrmpos = 0;
   }
   // flush storage changes in rm
   if (storage_w[0].name[0]) mod_secondary->sto_flush();
   recursive = 0;
   mt_swunlock();
   // at this time START module is ready to register notifications
   if (firsttime) {
      firsttime = 0;
      mod_secondary->sys_notifyevent(SECB_CPCHANGE|SECB_GLOBAL, cb_codepage);
   }
}

/**************************************************************************/

/* internal version of sto_save, start module will export
   actual version for all other users.
   Only 4 bytes length accepted here and short key names (<=11 chars) */
void sto_save(const char *entry, void *data, u32t len, int copy) {
   if (!entry) return;
   if (mod_secondary) mod_secondary->sto_save(entry,data,len,copy); else {
      int ii, lz=-1;
      if (data && copy && len!=4) return;
      /* this code used only before START will be ready, then "storage_w" can
         be filled by real mode code only (and flushed in log_flush()) */
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

/**************************************************************************/
/** wait key with timeout.
    Be careful, key_wait() uses seconds in timeout, but this function - milliseconds.

    @param       ms       time to wait for, ms. Value of 0xFFFFFFFF means forever.
    @param [out] status   ptr to keyboard status at the moment of key press,
                          can be 0.
    @return key code or 0 */
u16t _std ckey_waitex(u32t ms, u32t *status) {
   qsclock  sclk;
   u16t  keycode = key_read_nw(status);
   if (keycode || !ms) return keycode;

   sclk  = sys_clock();
   while (1) {
#ifdef EFI_BUILD
      usleep(10000);
#else
      cpuhlt();
#endif
      keycode = key_read_nw(status);

      if (keycode) return keycode; else 
      // it will never be greater, than 0xFFFFFFFF
      if ((u32t)((sys_clock()-sclk)/1000) > ms) break;
   }
   return 0;
}

// this function goes via current vio router
u16t _std key_wait(u32t seconds) {
   return key_waitex(seconds>FFFF>>10?FFFF:seconds*1000, 0);
}

u32t _std key_clear(void) {
   u32t cnt = 0;
   while (key_waitex(0,0)) cnt++;
   return cnt;
}

u16t _std ckey_read() {
#ifndef EFI_BUILD
   // goes into the real mode for a while - until MTLIB activation
   if (!mt_exechooks.mtcb_yield) {
      if (check_cbreak()) return KEYC_CTRLBREAK;
      return key_read_int();
   }
#endif
   return ckey_waitex(FFFF,0);
}

/// trap screen on missing function in active VIO handler
void vioroute_panic(u32t offset) {
   char err[64];
   vio_handler *vh = VHTable[VHI_ACTIVE];
   snprintf(err, 64, "VIO handler \"%s\" missing entry %u", vh->vh_name,
      offset-FIELDOFFSET(vio_handler,vh_setmode)>>2);
   mod_secondary->sys_throw(xcpt_syserr, err, 0);
}
