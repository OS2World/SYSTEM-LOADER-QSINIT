//
// QSINIT API
// malloc/free
//

#ifndef QSINIT_TIME
#define QSINIT_TIME

#ifdef __cplusplus
extern "C" {
#endif

#include "clib.h"

typedef u32t       time_t;
typedef qsclock   clock_t;

struct  tm {
   int      tm_sec;  // seconds after the minute -- [0,61]
   int      tm_min;  // minutes after the hour   -- [0,59]
   int     tm_hour;  // hours after midnight     -- [0,23]
   int     tm_mday;  // day of the month         -- [1,31]
   int      tm_mon;  // months since January     -- [0,11]
   int     tm_year;  // years since 1900

   int     tm_wday;  // days since Sunday        -- [0,6]
   int     tm_yday;  // days since January 1     -- [0,365]
   int    tm_isdst;  // Daylight Savings Time flag
};

/** clib time function
    warning - it highly inaccurate! */
time_t  _std time(time_t *tloc);

time_t  _std mktime(struct tm *timeptr);

struct tm* _std localtime_r(const time_t *timer,struct tm * res);
#define localtime(tme) localtime_r(tme,0)
#define _localtime   localtime_r

char*   _std ctime_r(const time_t *timer, char* buffer);
#define ctime(tme)   ctime_r(tme,0)
#define _ctime       ctime_r

char*   _std asctime_r(const struct tm *timeptr, char* buffer);
#define asctime(tme) asctime_r(tme,0)
#define _asctime     asctime_r

/** time and date format.
    Function supports all of watcom runtime format directives and some
    additional posix ones, like %C, %e, %F, %k, %l, %R, %s.
    Since no locale in QSINIT, %Z(%z) is always empty and %x/%c fixed
    to be dd/mm/yy 24h */
u32t    _std strftime(char *dst, u32t len, const char *fmt, const struct tm *tmv);

/// convert DOS time to struct tm value
void    _std dostimetotm(u32t dostime, struct tm *dt);

/// convert time_t to DOS time
u32t    _std timetodostime(time_t time);

/// convert DOS time to time_t
time_t  _std dostimetotime(u32t dostime);

#define CLOCKS_PER_SEC   1000000

clock_t _std clock(void);

/// return # of microseconds since reboot
qsclock _std sys_clock(void);

/// neutral time value for the unknown/error cases (now is 1.1.2017)
#define ERROR_TIME_T     0x58681C50

#ifdef __cplusplus
}
#endif

#endif // QSINIT_TIME
