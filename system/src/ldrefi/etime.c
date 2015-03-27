#include "clib.h"
#include "qslog.h"
#include "qsutil.h"
#include "qstime.h"
#include "qecall.h"

extern u16t IODelay;

u32t _std tm_counter(void) {
   return call64(EFN_TMCOUNTER, 0, 0);
}

void _std tm_setdate(u32t dostime) {
   call64(EFN_TMSETDATE, 0, 1, &dostime);
}

void _std tm_calibrate(void) {
   IODelay = call64(EFN_TMCALIBRATE, 0, 0);
   log_printf("new delay: %hu\n",IODelay);
}
