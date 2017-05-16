#define LOG_INTERNAL
#define STORAGE_INTERNAL
#include "qslocal.h"
#include "qsvdata.h"
#include "qsxcpt.h"

extern stoinit_entry    storage_w[STO_BUF_LEN];
extern u8t*              logrmbuf;
extern u16t              logrmpos;
static int           in_exit_call = 0;
static u8t            exit_called = 0;
extern u16t              text_col;
extern u8t             cvio_blink;
extern u32t         cvio_ttylines;
#ifndef EFI_BUILD
extern u16t             logbufseg;
extern u8t                restirq; // restore IRQs flag

// real mode far functions!
void rmvtimerirq(void);
void getfullSMAP(void);
void poweroffpc(void);
void biostest(void);
#endif
void setbaudrate(void);
void earlyserinit(void);

// some externals
int  _std _snprint(char *buf, u32t count, const char *fmt, long *argp);
void _std cache_ctrl(u32t action, u8t vol);
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
u8t  _std ckey_pressed(void);
u16t _std key_read_nw(void);
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
      cache_ctrl(CC_FLUSH, DISK_LDR);
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

int _std hlp_seroutset(u16t port, u32t baudrate) {
   if (baudrate)
      if (baudrate==115200 || memchrw(check_rate,baudrate,10)!=0)
         BaudRate = baudrate;
      else
         return 0;
   mt_swlock();
   if (port==0xFFFF) ComPortAddr = 0; else
   if (port) {
      ComPortAddr = port;
      earlyserinit();
   } else
   if (baudrate && ComPortAddr) setbaudrate();
   mt_swunlock();
   return 1;
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
#else
void int15mem_int(void);
#endif

AcpiMemInfo* _std hlp_int15mem(void) {
   if (!mod_secondary) return 0; else {
      AcpiMemInfo *rc, *rtbl;
      u32t        cnt;

      mt_swlock();
      int15mem_int();
      rtbl = (AcpiMemInfo*)DiskBufPM;
      cnt  = 0;
      while (rtbl[cnt].LengthLow || rtbl[cnt].LengthHigh) cnt++;
      cnt++;
      cnt *= sizeof(AcpiMemInfo);
      rc   = (AcpiMemInfo*)mod_secondary->mem_realloc(0, cnt);
      memcpy(rc, rtbl, cnt);
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

int _std log_vprintf(const char *fmt, long *argp) {
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
#ifdef EFI_BUILD
static u8t hltflag = 0;
#define SLEEP_TIME  10
#else
static u8t hltflag = 1;
#define SLEEP_TIME  20
#endif

int _std key_waithlt(int on) {
   int prev = hltflag;
   hltflag  = on?1:0;
   return prev;
}

u16t _std ckey_wait(u32t seconds) {
   u32t    btime, diff;
   u16t  keycode = key_read_nw();

   if (keycode || !seconds) return keycode;

   btime = tm_counter();
   diff  = 0;
   while (seconds>0) {
       if (hltflag) cpuhlt(); else usleep(SLEEP_TIME*1000); // 10-20 ms
       keycode = key_read_nw();

       if (keycode) return keycode; else {
          u32t now = tm_counter();
          if (now != btime) {
              if ((diff+=now-btime)>=18) { seconds--; diff = 0; }
              btime = now;
          }
       }
   }
   return 0;
}

u16t _std ckey_read() {
#ifdef EFI_BUILD
   return ckey_wait(x7FFF);
#else
   // goes to real mode for a while until MTLIB start
   return mt_exechooks.mtcb_yield ? ckey_wait(x7FFF) : key_read_int();
#endif
}

/**************************************************************************/
/* this function called in MT locked state only, during session switching,
   so be careless here */
vio_session_data* _std cvio_savestate(void) {
   vio_session_data *rc;
   u32t     cols, lines;
   cvio_getmodefast(&cols, &lines);
   // mod_secondary should be here
   rc = (vio_session_data*)mod_secondary->mem_alloc(QSMEMOWNER_SESTATE, 0,
        sizeof(vio_session_data) + cols*lines*2);
   rc->vs_sign  = SESDATA_SIGN;
   rc->vs_x     = cols;
   rc->vs_y     = lines;
   rc->vs_color = text_col;
   rc->vs_shape = cvio_getshape();
   cvio_getpos(&rc->vs_cy, &rc->vs_cx);
   rc->vs_lines = cvio_ttylines;
   rc->vs_flags = cvio_blink?0:VSF_INTENSITY;
   cvio_readbuf(0, 0, cols, lines, rc+1, cols*2);
   return rc;
}

// the same as cvio_savestate()
void  _std cvio_reststate(vio_session_data *state) {
   u32t     cols, lines;
   vio_session_data *sd = (vio_session_data*)state;

   if (sd->vs_sign!=SESDATA_SIGN || (sd->vs_flags&VSF_GRAPHIC))
      mod_secondary->sys_throw(xcpt_intbrk, __FILE__, __LINE__);
   cvio_getmodefast(&cols, &lines);
   if (cols!=sd->vs_x || lines!=sd->vs_y) cvio_setmodeex(sd->vs_x, sd->vs_y);
   cvio_intensity(sd->vs_flags&VSF_INTENSITY?1:0);
   cvio_writebuf(0, 0, sd->vs_x, sd->vs_y, sd+1, sd->vs_x*2);
   cvio_setpos(sd->vs_cy, sd->vs_cx);
   cvio_setshape(sd->vs_shape>>8, sd->vs_shape);
   cvio_setcolor(sd->vs_color);
   cvio_ttylines = sd->vs_lines;
}
