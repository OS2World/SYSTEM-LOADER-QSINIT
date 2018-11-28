//
// QSINIT
// printf support
// -------------------------------------------------------
//
#include "qstypes.h"
/* code is independent from anything, except fptostr() which can be
   eliminated by this define (with float printing, of course) */
#ifndef NO_FPU_PRINTF
#include "qsmodint.h"
extern mod_addfunc *mod_secondary;
#else
#define mod_secondary 0
#endif

typedef void _std (*print_outf)(int ch, void *stream);
u32t  _std strlen(const char *str);
u32t  _std wcslen(const u16t *str);
u16t ff_convert(u16t chr, unsigned int dir);

#ifdef EFI_BUILD
const char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
#else
extern const char alphabet[];
#endif

#ifdef __WATCOMC__
unsigned _udiv_(unsigned, unsigned*);
#pragma aux _udiv_ =       \
   "xor  edx,edx"          \
   "div  dword ptr [ebx]"  \
   "mov  [ebx],eax"        \
   parm [eax] [ebx]        \
   modify exact [eax edx]  \
   value [edx];
#endif

char *_std _utoa(unsigned value, char *buffer, int radix) {
   char     *pp=buffer, *qq;
   unsigned        rem,quot;
   char             buf[34];

   buf[0]=0;
   qq=buf+1;
   do {
#ifdef __WATCOMC__
      quot=radix;
      rem =_udiv_(value,&quot);
#else
      rem =value%radix;
      quot=value/radix;
#endif
      *qq =alphabet[rem];
      ++qq;
      value = quot;
   } while (value!=0);
   while ((*pp++=(char)*--qq)) ;
   return buffer;
}

char *_std _utoa64(u64t value, char *buffer, int radix) {
   char     *pp=buffer, *qq;
   u64t            rem,quot;
   char             buf[34];

   buf[0]=0;
   qq=buf+1;
   do {
      rem =value%radix;
      quot=value/radix;
      *qq =alphabet[rem];
      ++qq;
      value = quot;
   } while (value!=0);
   while ((*pp++=(char)*--qq)) ;
   return buffer;
}

char *_std _itoa(int value, char *buffer, int radix) {
   char *pp=buffer;

   if (radix==10 )
      if (value<0) { *pp++='-'; value=-value; }
   _utoa(value,pp,radix);
   return buffer;
}

char *_std _itoa64(s64t value, char *buffer, int radix) {
   char *pp=buffer;

   if (radix==10 )
      if (value<0) { *pp++='-'; value=-value; }
   _utoa64(value,pp,radix);
   return buffer;
}

typedef char (*fnextch)(char **cp);

static char nextch(char **cp) {  // read asc string
   char rc = (*cp)[0];
   *cp += 1;
   return rc;
}

static char nextchw(char **cp) { // read unicode string
   char rc = ff_convert(*(u16t*)(*cp),0);
   *cp += 2;
   return rc ? rc : '?';
}

int _std _prt_common(void *fp, const char *fmt, long *arg, print_outf outc) {
   char          *str;
   int          width;   // field width
   int           prec;   // precision
   int         n, len;
   int            nch;   // number of ready chars
   char          fmtc;   // format char
   char      buf[384];   // buffer
   int      pad_field;   // count used to shift "prec" inside "field"
   int     prefix_cnt;   // -, +, ' ', or '0x' prefix
   int      prec_flag;   // precision was specified
   int       char_prn;   // character print active
   u8t       fmt_size;   // format: h l or L
   int       prec_fix;
   char         leftj;   // true if left-justified
   char          plus;   // true if to print +/- for signed value
   char         space;   // ' ' if needed for signed value
   char           alt;   // alternate form active
   char       padchar;   // padding char
   fnextch      strin;

   nch = 0;

   while (fmtc=*fmt++) {
      if (fmtc!='%') {
         (*outc)(fmtc,fp);
         nch++;
      } else {
         prefix_cnt = 0; char_prn = 0; leftj = 0; plus = 0; 
         space = 0; alt = 0;
         strin = nextch;
         padchar = ' ';

         while (true) {
            switch (*fmt) {
               case '-' : fmt++; leftj++; padchar = ' ';    continue;
               case '0' : fmt++; if (!leftj) padchar = '0'; continue;
               case '+' : fmt++; plus++; space = 0;         continue;
               case ' ' : fmt++; if (plus == 0) space++;    continue;
               case '#' : fmt++; alt++;                     continue;
            }
            break;
         }
         if (*fmt == '*') {
            width = *arg++;
            if (width < 0) { width = -width; leftj++; }
            fmt++;
         } else {
            width = 0;
            while (*fmt >= '0' && *fmt <= '9') {
               width = width * 10 + (*fmt - '0');
               fmt++;
            }
         }
         prec = -1;          // reset precision
         prec_flag = 0;
         if (*fmt == '.') {
            prec_flag = 1;
            fmt++;
            if (*fmt == '*') {
               prec = *arg++;   // get prec from arg
               fmt++;
            } else {
               prec = 0;
               while (*fmt >= '0' && *fmt <= '9') {
                  prec = prec * 10 + (*fmt - '0');
                  fmt++;
               }
            }
         }
         n = prec_fix = 0;
         fmt_size = 1;

         switch (*fmt) {
            case 'l': fmt_size=4;
                      if (*++fmt!='l') break; // l or ll specified
            case 'L': fmt++; fmt_size=8; break; // L specified
            case 'h': fmt++; fmt_size=2; break; // h specified
         }
         str = buf;
         switch (fmtc = *fmt++) {
            case 'C': {
               char ch = ff_convert(*(u16t*)arg,0);
               buf[0] = ch ? ch : '?';
               arg += 2;
            }
            case 'c':
               if (fmtc=='c') buf[0] = *arg++;
               char_prn = true;
               buf[1] = 0;
               len = 1;
               break;
            case 'd':
            case 'i': {
               s64t value = fmt_size==8?*(s64t*)arg:(fmt_size==2?(long)(s16t)*arg:*arg);
               arg++; if (fmt_size==8) arg++;
               if (prec_flag) padchar=' ';

               _itoa64(value,++str,10);
               len = strlen(str);

               if (*str!='-' && (plus||space)) { *--str=plus?'+':' '; len++; }
               prec_fix = 1;
               break;
            }
            case 'E':
            case 'e':
            case 'f':
            case 'G':
            case 'g': 
               if (!mod_secondary) {
                  char_prn = true;
                  str = "?.?";
                  len = 3;
               } 
#ifndef NO_FPU_PRINTF
                 else {
                  double value = *(double*)arg;
                  arg += 2;
                  len = mod_secondary->fptostr(value,++str,fmtc|(char)(alt?0x80:0),prec);
                  
                  if (*str == '-') prefix_cnt = 1; else
                  if (plus||space) { *--str=plus?'+':' '; len++; prefix_cnt = 1; }
                  prec = -1;
               }
#endif
               break;
            case 'b':
            case 'B': {
               u8t *baptr = (u8t*)*arg;
               // if it zero - print (null) in string below
               if (baptr) {
                  arg++;
                  if (!width) width = 1;
                  prec = fmt_size;
                  
                  while (width--) {
                     u64t value;
                     switch (prec) {
                        case 2:  value = *(u16t*)baptr; break;
                        case 4:  value = *(u32t*)baptr; break;
                        case 8:  value = *(u64t*)baptr; break;
                        default: value = *baptr; break;
                     }
                     _utoa64(value,str = buf,16);
                     baptr+= prec;
                     len   = strlen(str);
                     if (alt) { 
                        (*outc)('0',fp); 
                        (*outc)('x'-'b'+fmtc,fp); 
                        nch += 2; 
                     }
                     while (len++<prec*2) (*outc)('0',fp);
                     while (*str) (*outc)(*str++,fp);
                     nch  += len;
                     if (width) { (*outc)(' ',fp); nch++; }
                  }
                  continue;
               }
            }
            case 'S':
            case 's':
               char_prn = true;
               str = (char*)*arg++;
               if (!str) { str = "(null)"; fmtc='s'; }
               if (fmtc=='S') {
                  strin = nextchw;
                  len = wcslen((u16t*)str);
               } else
                  len = strlen(str);
               if (prec<len && prec>=0) len = prec;
               break;
            case 'u': n = 2;
            case 'o': n+= 8;
            case 'X':
            case 'x': {
               u64t tempval;
               if (!n) n = 16;
               if (prec_flag) padchar = ' ';

               tempval = fmt_size==8?*(u64t*)arg:(fmt_size==2?(u16t)*arg:(u32t)*arg);
               arg++; if (fmt_size==8) arg++;

               _utoa64(tempval, str += 2, n);
               len = strlen(str);

               if (alt && (n != 10)) {
                  if (n==8 && tempval) {
                     if (prec <= len) prec = len+1;
                  } else {
                     if (n==16 && tempval) {
                        prefix_cnt=2; len+=2; str-=2;
                        *str='0'; str[1]='X';
                     }
                  }
               }
               if (fmtc=='x') {
                  char *tempStr = str;

                  for (;*tempStr;tempStr++)
                     if (*tempStr>='A') *tempStr+='a'-'A';
               }
               prec_fix = 1;
               break;
            }
            case 'p':
               _utoa((u32t)*arg++, str, 16);
               len = strlen(str);
               prec_fix = 1;
               break;
            case 'n':
               if (fmt_size==2) *(short*)(*arg++) = nch; else
                  *(long*)(*arg++) = nch;
               continue;
            default:
               (*outc)(fmtc,fp);
               nch++;
               continue;
         }

         if (prec_fix) {
            char *strt = str,
                 clead = strin(&strt);
            if (clead=='-'||clead=='+'||clead==' ') prefix_cnt = 1;

            if (prec>=0) {
               if (prec < len - prefix_cnt) prec = len - prefix_cnt;
               if (width < prec + prefix_cnt) width = prec + prefix_cnt;
            }
         }

         // extra space
         if ((n = width - len) > 0) nch+=n;

         if (!leftj) {    /* if not left-justified */
            int filled = prec+prefix_cnt;
            pad_field  = width - (filled>len?filled:len);

            if (padchar=='0' && len>0) {
               while (prefix_cnt-- > 0) {
                  (*outc)(strin(&str), fp);
                  nch++; len--;
               }
            }

            for (++pad_field;--pad_field>0;) {
               (*outc)(padchar, fp);
               n--;
            }

            if (padchar == ' ' && (len > 0)) {
               while (prefix_cnt-- > 0) {
                  (*outc)(strin(&str), fp);
                  nch++; len--;
               }
            }
            // print pad
            for (++n; --n > 0;) (*outc)(char_prn?' ':'0',fp);
         }

         nch += len;

         if (len > 0) {
            while (prefix_cnt-- > 0) {
               (*outc)(strin(&str), fp);
               len--;
            }
         }

         if (n > 0) {            // true for left just
            pad_field = prec - len;  // leading zeros?
            for (++pad_field; --pad_field > 0;) {
               (*outc)('0', fp);
               --n;
            }
         }

         for (++len; --len > 0;) (*outc)(strin(&str), fp);
         // pad on right w. blanks
         for (++n; --n > 0;) (*outc)(' ', fp);
      }
   }
   return nch;
}

typedef struct {
   u32t     len;
   u32t     pos;
   char    *buf;
} bufinfo;

static void _std bufprn(int ch, void *stream) {
   bufinfo *bi = stream;
   if (bi->pos<bi->len) bi->buf[bi->pos++]=ch;
}

// actually this is vsnprintf
int __stdcall _snprint(char *buf, u32t count, const char *fmt, long *argp) {
   bufinfo bi;
   int     rc;
   bi.len = count; bi.pos = 0; bi.buf = buf;
   rc = _prt_common(&bi,fmt,argp,bufprn);
   buf[bi.pos<count?bi.pos:count-1] = 0;
   return rc;
}

int __cdecl snprintf(char *buf, u32t count, const char *fmt, long args) {
   return _snprint(buf, count, fmt, &args);
}
