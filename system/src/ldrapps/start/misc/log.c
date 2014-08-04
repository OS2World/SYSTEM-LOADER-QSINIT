#include "clib.h"
#define LOG_INTERNAL
#include "qslog.h"
#include "stdarg.h"
#include "qsutil.h"

#define LOG_SIZE         _64KB  // log size
#define LOG_PRINTSIZE     384
#define LOG_SIZEFP       (LOG_SIZE/sizeof(log_header))
#define LOG_EntrySize(x) (Round16(x)>>4)

static log_header* logptr = 0;
static u8t        logused = 0;
static u32t       logfptr = 0;

static void log_setdate(log_header* lp) {
   u32t  dostime;
   u8t    onesec;
   __asm {
      call  tm_getdate
      setc  onesec
      mov   dostime,eax
   }
   lp->dostime = dostime;
   if (onesec) lp->flags|=LOGIF_SECOND; else lp->flags&=~LOGIF_SECOND;
}

void _std log_clear(void) {
   if (!logptr) logptr = (log_header*)hlp_memalloc(LOG_SIZE,QSMA_READONLY);
      else memset(logptr, 0, LOG_SIZE);
   logfptr = 0;
   // first entry
   logptr->sign   = LOG_SIGNATURE;
   logptr->flags  = 0;
   logptr->offset = LOG_SIZEFP-1;
   // last entry
   logptr[LOG_SIZEFP-1].sign   = LOG_SIGNATURE;
   logptr[LOG_SIZEFP-1].flags  = LOGIF_USED;
   logptr[LOG_SIZEFP-1].offset = 0;
}

void setup_log() {
   log_clear();
}

static int log_check(u32t pos) {
   static char errprn[144];
   if (pos>=LOG_SIZEFP) 
      snprintf(errprn,144,"<< log panic: pos too large (%d) >>\n",pos);
   else {
      log_header* lp = logptr + pos;
      if (lp->sign!=LOG_SIGNATURE)
         snprintf(errprn,144,"<< log panic: sign mismatch (%d: %4lb) >>\n",pos,lp);
      else
         return 1;
   }
   hlp_seroutstr(errprn);
   return 0;
}

static log_header *log_alloc(u32t size) {
   while (size && logptr && !logused) {
      log_header* lp = logptr + logfptr;
      if (!log_check(logfptr)) break;
      // round size to 16
      size = LOG_EntrySize(size);
      // too large
      if (size>=LOG_SIZEFP-1) return 0;
      if (!lp->offset) lp = logptr;
      // search free block in cycled buffer
      while (lp->offset<size+1) {
         log_header* lpnext = lp+lp->offset;
         // broken header
         if (!log_check(lpnext-logptr)) return 0;
         if (!lpnext->offset) { lp = logptr; continue; }
         // merge next block
         lpnext->sign = 0;
         lp->offset  += lpnext->offset;
      }
      if (!log_check(lp-logptr)) return 0;
      // split too long free entry
      if (lp->offset>size+3) {
         log_header* lpnext = lp+size+1;
         lpnext->sign   = LOG_SIGNATURE;
         lpnext->flags  = 0;
         lpnext->offset = lp->offset-size-1;
         lp->offset     = size+1;
      }
      // update "next free"
      logfptr = (lp-logptr)+lp->offset;
      // make "empty string" in first pos
      *(char*)(lp+1) = 0;
      // mark as used
      lp->flags = LOGIF_USED;
      // return
      return lp;
   }
   return 0;
}

static void log_commit(log_header *lp) {
   u32t size;
   if (!logptr || !lp || lp-logptr>LOG_SIZEFP) {
      hlp_seroutstr("<< log panic: bad commit >>\n");
      return;
   }
   if (!log_check(lp-logptr)) return;
   lp->flags  |= LOGIF_USED;
   log_setdate(lp);
   // put zero to the last byte of this entry!
   ((char*)(lp+lp->offset))[-1] = 0;
   size = strlen((char*)(lp+1));
   size = LOG_EntrySize(size+1);

   // split too long free entry if next entry is "free"
   if (lp->offset>size+1 && logfptr == (lp-logptr)+lp->offset) {
      log_header *lpnext = lp+size+1, 
                *lpNnext = lp+lp->offset;

      if (lpNnext->offset) {
         lpnext->sign   = LOG_SIGNATURE;
         lpnext->flags  = 0;
         lpnext->offset = lp->offset - (size+1) + lpNnext->offset;
         lp->offset     = size+1;
         lpNnext->sign  = 0;
         // update "next free"
         logfptr        = lpnext-logptr;
      }
   }
}

int log_pushb(int level, const char *str, int len) {
   log_header *lp = log_alloc(len);
   if (lp) {
      memcpy(lp+1,str,len);
      lp->flags = level&(LOGIF_LEVELMASK|LOGIF_DELAY|LOGIF_REALMODE)|LOGIF_USED;
      log_setdate(lp);
   } else
      hlp_seroutstr("<< log is locked, log_push failed >>\n");
   return lp?1:0;
}

int _std log_push(int level, const char *str) {
   int  len = strlen(str)+1;
   return log_pushb(level, str, len);
}

int __cdecl log_it(int level, const char *fmt, ...) {
   log_header  *lp = log_alloc(LOG_PRINTSIZE);
   va_list arglist;
   char       *dst = (char*)(lp+1);
   int          rc;
   if (!lp) {
      static char buf[LOG_PRINTSIZE];
      hlp_seroutstr("log is locked: ");
      dst = &buf;
   }
   va_start(arglist,fmt);
   rc = _vsnprintf(dst,LOG_PRINTSIZE,fmt,arglist);
   va_end(arglist);
   hlp_seroutstr(dst);
   // commit new log entry
   if (lp) {
      lp->flags = lp->flags&~LOGIF_LEVELMASK|level&LOGIF_LEVELMASK;
      log_commit(lp);
   }
   return rc;
}

void _std log_query(log_querycb cbproc, void *extptr) {
   if (!logptr||!cbproc) return;
   logused++;
   cbproc(logptr,extptr);
   logused--;
}
