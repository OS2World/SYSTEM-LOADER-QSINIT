//
// QSINIT "start" module
// time C library functions
//
#include "stdlib.h"
#include "time.h"
#include "qstime.h"
#include "qslog.h"
#include "qsmod.h"
#include "internal.h"

static struct tm lct_res;
static char  timestr[32];

#define CONST_DATE_D0     1461
#define CONST_DATE_D1   146097
#define CONST_DATE_D2  1721119
#define CONST_SECINDAY   86400

static int GregorianToJulian(u16t Year,u16t Month,u16t Day) {
   int Century, XYear;
   if (Month<=2) { Year--; Month+=12; }
   Month  -= 3;
   Century = Year/100;
   XYear   = Year%100;
   Century = Century*CONST_DATE_D1>>2;
   XYear   = XYear  *CONST_DATE_D0>>2;
   return (Month*153+2)/5+Day+CONST_DATE_D2+XYear+Century;
}

static void JulianToGregorian(int Julian,int *Year,int *Month,int *Day) {
   int Temp = ((Julian-CONST_DATE_D2<<2)-1),
      XYear = Temp%CONST_DATE_D1|3,YYear,YMonth,YDay;
   Julian = Temp/CONST_DATE_D1;
   YYear  = XYear/CONST_DATE_D0;
   Temp   = (XYear%CONST_DATE_D0+4>>2)*5-3;
   YMonth = Temp/153;
   if (YMonth>=10) { YYear++; YMonth-=12; }
   YMonth+= 3;
   YDay   = Temp%153;
   YDay   = (YDay+5)/5;
   *Year  = YYear+Julian*100;
   *Month = YMonth;
   *Day   = YDay;
}

static int GetWeekDay(int year,int month,int day) {
   return (((3*year-(7*(year+(month+9)/12))/4+(23*month)/9+day+2+
     ((year-(month<3?1:0))/100+1)*3/4 - 15)%7));
}

static void UnPackUnixTime(time_t uts, struct tm *dt) {
   int Temp = uts/CONST_SECINDAY,
      j1970 = GregorianToJulian(1970,1,1),
       jNow = j1970+Temp;
   JulianToGregorian(jNow,&dt->tm_year,&dt->tm_mon,&dt->tm_mday);
   Temp         = uts%CONST_SECINDAY;
   dt->tm_hour  = Temp/3600;
   dt->tm_min   = (Temp%3600)/60;
   dt->tm_sec   = Temp%60;
   dt->tm_isdst = 0;
   dt->tm_yday  = 0; // ignored now
   dt->tm_wday  = GetWeekDay(dt->tm_year,dt->tm_mon,dt->tm_mday);
   dt->tm_mon--;
   dt->tm_year-=1900;
}

time_t __stdcall mktime(struct tm *dt) {
   int j1970 = GregorianToJulian(1970,1,1),
        jNow = GregorianToJulian(dt->tm_year+1900,dt->tm_mon+1,dt->tm_mday),
         uts = (jNow-j1970)*CONST_SECINDAY,
        temp = dt->tm_hour*3600+dt->tm_min*60+dt->tm_sec;
   return uts+temp;
}

struct tm* __stdcall localtime_r(const time_t *timer, struct tm *res) {
   if (!res) res = &lct_res;
   UnPackUnixTime(*timer,res);
   return res;
}

void __stdcall dostimetotm(u32t dostime, struct tm *dt) {
   int jYear, jNow;
   dt->tm_sec   = (dostime&0x1F)<<1;
   dt->tm_min   = dostime>>5&0x3F;
   dt->tm_hour  = dostime>>11&0x1F;
   dt->tm_mday  = dostime>>16&0x1F;
   dt->tm_mon   = (dostime>>21&0xF) - 1;
   dt->tm_year  = (dostime>>25) + 80;

   jYear = GregorianToJulian(dt->tm_year+1900,1,1);
   jNow  = GregorianToJulian(dt->tm_year+1900,dt->tm_mon+1,dt->tm_mday);

   dt->tm_yday  = (jNow-jYear)/CONST_SECINDAY;
   dt->tm_wday  = GetWeekDay(dt->tm_year+1900,dt->tm_mon+1,dt->tm_mday);
   dt->tm_isdst = 0;
}

time_t __stdcall time(time_t *tloc) {
   struct tm  dt;
   u32t  dostime;
   u8t    onesec;
   __asm {
      call  tm_getdate
      setc  onesec
      mov   dostime,eax
   }
   dostimetotm(dostime, &dt);
   dt.tm_sec += onesec;
   dostime    = mktime(&dt);
   if (tloc) *tloc = dostime;
   return dostime;
}

/* The ctime functions convert the time into a string containing 
exactly 26 characters.  This string has the form shown in the 
following example:
     Sat Mar 21 15:58:27 1987\n\0 
*/
static const char *mtitle[12]={"Jan","Feb","Mar","Apr","May","Jun",
   "Jul","Aug","Sep","Oct","Nov","Dec"};
static const char *dtitle[7]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

static char* _asctime_buffer(void) {
   if (!in_mtmode) return &timestr; else {
      qs_mtlib  mt = get_mtlib();
      char    *res = (char*)(u32t)mt_tlsget(QTLS_ASCTMBUF);
      if (!res) {
         res = (char*)malloc_thread(48);
         mt_tlsset(QTLS_ASCTMBUF, (u32t)res);
      }
      return res;
   }
}

char* __stdcall ctime_r(const time_t *timer, char* buffer) {
   struct tm tme;
   localtime_r(timer,&tme);
   return asctime_r(&tme,buffer);
}

char* __stdcall asctime_r(const struct tm *dt, char* buffer) {
   if (!buffer) buffer = _asctime_buffer();
   snprintf(buffer,32,"%s %s %02d %02d:%02d:%02d %4d\n", dtitle[dt->tm_wday],
      mtitle[dt->tm_mon], dt->tm_mday, dt->tm_hour, dt->tm_min, dt->tm_sec,
         dt->tm_year+1900);
   return buffer;
}

u32t __stdcall timetodostime(time_t time) {
   struct tm tmi;
   localtime_r(&time,&tmi);
   return (tmi.tm_sec>>1) + (tmi.tm_min<<5) + (tmi.tm_hour<<11) + 
      (tmi.tm_mday<<16) + (tmi.tm_mon+1<<21) + (tmi.tm_year-80<<25);
}

time_t __stdcall dostimetotime(u32t dostime) {
   int j1970 = GregorianToJulian(1970,1,1),
        jNow = GregorianToJulian((dostime>>25)+1980,dostime>>21&0xF,dostime>>16&0x1F),
         uts = (jNow-j1970)*CONST_SECINDAY,
        temp = (dostime>>11&0x1F)*3600+(dostime>>5&0x3F)*60+((dostime&0x1F)<<1);
   return uts+temp;
}
