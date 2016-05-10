#include "qstypes.h"
#define MODULE_INTERNAL
#include "qsmod.h"
#include "qsutil.h"
#include "qsint.h"
#include "clib.h"
#include "vio.h"

#define TEXTMEM_SEG  0xB800
extern u64t  countsIn55ms,
                rdtscprev;
extern u8t  rtdsc_present;

int  _std tolower(int cc);
void _std usleep(u32t usec);
void _std mt_yield(void);
u64t _std hlp_tscread(void);
int  _std sys_intstate(int on);
// ******************************************************************

// real mode far functions, do not call it directly!
u16t getkeyflags(void);
void settextmode(u32t lines);
void checkresetmode(u32t clear);
void setkeymode(u32t clear);
u8t  istextmode(void);
void calibrate_delay(void);
void vio_updateinfo(void);

// get system keys status
u32t key_status(void) {
   static u16t translate[16]={KEY_SHIFTRIGHT|KEY_SHIFT, KEY_SHIFTLEFT|KEY_SHIFT,
      KEY_CTRL, KEY_ALT, KEY_SCROLLLOCK, KEY_NUMLOCK, KEY_CAPSLOCK, KEY_INSERT,
      KEY_CTRLLEFT|KEY_CTRL, KEY_ALTLEFT|KEY_ALT, KEY_CTRLRIGHT|KEY_CTRL, 
      KEY_ALTRIGHT|KEY_ALT, KEY_SCROLLLOCK, KEY_NUMLOCK, KEY_CAPSLOCK, KEY_SYSREQ };
   u16t flags=rmcall(getkeyflags,0);
   u32t rc=0,ii;

   for (ii=0;ii<16;ii++)
      if ((flags&1<<ii)) rc|=translate[ii];
   return rc;
}

void vio_setmode(u32t lines) {
   u32t mode=lines==43?1:(lines==50?2:0x202);
   rmcall(settextmode,2,mode);
   // reset internal cursor pos & vio data
   vio_getmode(0,0);
   vio_setpos(0,0);
   vio_defshape(VIO_SHAPE_LINE);
}

void _std vio_resetmode(void) {
   rmcall(checkresetmode,2,1);
   vio_updateinfo();
   vio_defshape(VIO_SHAPE_LINE);
}

/* this is stub, it will be replaced by CONSOLE.DLL on loading to allow
   virtual console modes */
int _std vio_setmodeex(u32t cols, u32t lines) {
   if (cols!=80) return 0;
   if (lines!=25 && lines!=43 && lines!=50) return 0;
   vio_setmode(lines);
   return 1;
}

extern u16t max_x,max_y;
extern u16t IODelay;
extern u32t NextBeepEnd;

static u8t _rate = 0, _delay = 0;

u8t _std vio_beepactive(void) {
   return NextBeepEnd?1:0;
}

void _std key_speed(u8t rate, u8t delay) {
   _rate  = rate  &= 0x1F;
   _delay = delay &= 3;
   rmcall(setkeymode,2,(u32t)delay<<8|rate);
}

void _std key_getspeed(u8t *rate, u8t *delay) {
   if (delay) *delay = _delay;
   if (rate )  *rate = _rate;
}

void _std vio_getmodefast(u32t *cols, u32t *lines) {
   vio_updateinfo();
   if (lines) *lines=max_y;
   if (cols) *cols=max_x;
}

u8t  _std vio_getmode(u32t *cols, u32t *lines) {
   u8t rc=rmcall(istextmode,0);
   vio_getmodefast(cols,lines);
   return rc;
}

// jumped here from vio_writebuf/vio_readbuf, so order of args is constant!
void _std vio_bufcommon(int toscr, u32t col, u32t line, u32t width,
   u32t height, void *buf, u32t pitch)
{
   u32t cols, lines;
   vio_getmodefast(&cols,&lines);
   if (line>=lines||col>=cols) return;
   if (!pitch) pitch = width*2;
   if (col+width  > cols ) width  = cols - col;
   if (line+height> lines) height = lines- line;
   if (width && height) {
      u8t *bptr = (u8t*)buf,
          *bscr = (u8t*)hlp_segtoflat(TEXTMEM_SEG) + (line*cols + col)*2;
      for (;height>0;height--) {
         memcpy(toscr?bscr:bptr,toscr?bptr:bscr,width*2);
         bscr+=cols*2; bptr+=pitch;
      }
   }
}

void _std tm_calibrate(void) {
   rmcall(calibrate_delay,0);
   log_printf("new delay: %hu\n",IODelay);
}

u64t _std hlp_tscin55ms(void) {
   u32t  counter = 0;
   // start timer rdtsc calculation, if it was not done before
   if (!rtdsc_present) hlp_tscread();
   /* we can be the first user or value was reset to 0 in tm_calibrate() call,
      so waits here until timer irq in real mode calculate it again since
      one period (55 ms) */
   while (!countsIn55ms) {
      if (++counter&1) mt_yield(); else usleep(20000);
      if (counter>=50) {
         log_printf("timer error!\n");
         break;
      }
   }
   return countsIn55ms;
}

u64t _std clock(void) {
   process_context *pq = mod_context();
   // init everything
   if (hlp_tscin55ms()) {
      int    state = sys_intstate(0);
      u64t     res = (tm_counter() - pq->rtbuf[RTBUF_STCLOCK]) * 54932; // 55 ms in clock tick
      u32t  mksrem = 0;
      // tsc since last timer interrupt
      if (rdtscprev) mksrem = (hlp_tscread() - rdtscprev) * 54932LL / countsIn55ms;
      // restore interrupts state
      sys_intstate(state);

      return res + mksrem;
   }
   return 0;
}

#pragma aux key_filter "_*" parm [eax] value [eax];

// try to fix some of EFI wrong key codes
u32t key_filter(u32t key) {
   register u16t kc = key;
   // zero char code in all Fx keys (single F1..F12 and with Alt/Ctrl/Shift)
   if (kc>=0x3B00 && kc<=0x44FF || kc>=0x5400 && kc<=0x71FF ||
      kc>=0x8500 && kc<=0x8CFF) kc&=0xFF00;
   return kc;
}
