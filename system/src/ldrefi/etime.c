#include "clib.h"
#include "qslog.h"
#include "qslocal.h"
#include "qsutil.h"
#include "qstime.h"
#include "qecall.h"
#include "qshm.h"
#include "qssys.h"

extern u16t               IODelay;
/* note that the same var present in BIOS host files, but with sligtly
   another functionality! */
extern u64t          countsIn55ms;
static u64t               lasttsc = 0, lasttime;
static u32t             tick_base = 0;
static u64t             tick_btsc = 0;

void _std tm_setdate(u32t dostime) {
   call64(EFN_TMSETDATE, 0, 1, &dostime);
}

static u32t get_ticks(u32t *mksrem) {
   u64t diff = hlp_tscread()-tick_btsc;
   u32t   rc = tick_base + (u32t)(diff/countsIn55ms);
   // 55 ms in clock tick
   if (mksrem) *mksrem = diff % countsIn55ms * 54932LL / countsIn55ms;
   return rc;
}

void _std tm_calibrate(void) {
   /* countsIn55ms can be changed by tm_calibrate(), just because of ugly
      EFI timers, this can cause DECREMENT of tm_counter() value.
      To fix this - we guarantee linear increment here. */
   mt_swlock();
   if (countsIn55ms) {
      tick_base = get_ticks(0);
      tick_btsc = hlp_tscread();
   }
   IODelay = call64(EFN_TMCALIBRATE, 0, 0);
   if (mod_secondary) mod_secondary->sys_notifyexec(SECB_IODELAY, IODelay);
   mt_swunlock();
   log_printf("new delay: %hu, tsc %LX\n", IODelay, countsIn55ms);
}

u32t _std tm_counter(void) {
   if (!countsIn55ms) tm_calibrate();
   return get_ticks(0);
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

/// both clock & sys_clock() walks here
u64t _std clockint(process_context *pq) {
   u32t         mksrem;
   u64t            res;
   if (!countsIn55ms) tm_calibrate();
   // 55 ms in clock tick
   res = (get_ticks(&mksrem) - (pq?pq->rtbuf[RTBUF_STCLOCK]:0)) * 54932;
   res+= mksrem;
   // it is inaccurate, but linear and really shows mks increment
   return res;
}
