#include "clib.h"
#include "qslog.h"
#include "qsint.h"
#include "qsutil.h"
#include "qstime.h"
#include "qecall.h"
#include "../ldrapps/hc/qshm.h"

extern u16t IODelay;
/* note that the same var present in BIOS host files, but with sligtly
   another functionality! */
extern u64t countsIn55ms;
static u64t      lasttsc = 0, lasttime;
static u32t    tick_base = 0;
static u64t    tick_btsc = 0;

void _std tm_setdate(u32t dostime) {
   call64(EFN_TMSETDATE, 0, 1, &dostime);
}

static u32t get_ticks(void) {
   return tick_base + (u32t)((hlp_tscread()-tick_btsc)/countsIn55ms);
}

void _std tm_calibrate(void) {
   /* countsIn55ms can be changed by tm_calibrate(), just because of ugly
      EFI timers, to fix this - here we guarantee LINEAR increment of 
      tm_counter() value */
   if (countsIn55ms) {
      tick_base = get_ticks();
      tick_btsc = hlp_tscread();
   }
   IODelay = call64(EFN_TMCALIBRATE, 0, 0);
   log_printf("new delay: %hu, tsc %LX\n", IODelay, countsIn55ms);
}

u32t _std tm_counter(void) {
   if (!countsIn55ms) tm_calibrate();
   return get_ticks();
}

u64t tm_getdateint(void) {
   /* return current second at least 550 ms since last gettime,
      this eliminate too often 64-bit host calls */
   if (lasttsc && countsIn55ms) {
      u64t diff = hlp_tscread() - lasttsc;
      if (diff/countsIn55ms < 10) return lasttime;
   }
   lasttime = call64l(EFN_TMGETDATE, 0, 0);
   lasttsc  = hlp_tscread();
   return lasttime;
}

u64t _std hlp_tscin55ms(void) {
   return countsIn55ms;
}
