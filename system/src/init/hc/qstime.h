//
// QSINIT
// time/timer functions
//
#ifndef QSINIT_TIMERS
#define QSINIT_TIMERS

#include "qstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/// return timer counter value (18.2 times per sec, for randomize, etc)
u32t _std tm_counter(void);

/// return time/date in dos format. flag CF is set for 1 second overflow
u32t _std tm_getdate(void);

/// set current time (with 2 seconds granularity)
void _std tm_setdate(u32t dostime);

/** calibrate I/O delay value.
    Must be called to recalculate i/o delay after major actions like
    "mtrr off" or cpu frequency change. */
void _std tm_calibrate(void);

#ifdef __cplusplus
}
#endif

#endif // QSINIT_TIMERS
