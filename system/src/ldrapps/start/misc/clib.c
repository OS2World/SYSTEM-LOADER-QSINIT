//
// QSINIT "start" module
// subset of C library functions
//
#include "stdlib.h"
#include "stdarg.h"
#include "fat/ff.h"
#include "errno.h"
#include "qsutil.h"
#include "direct.h"
#include "qslist.h"
#include "qsshell.h"
#include "internal.h"
#include "limits.h"

// this __stdcall save from linking errno as forward(!!) entry to itself ;)
int __stdcall errno=EZERO;
void set_errno(int errno_new) { errno=errno_new; }
int  get_errno(void) { return errno; }

int __stdcall atoi(const char *ptr) {
   register int value;
   char sign;
   if (!ptr) return 0;
   while (*ptr==' '||*ptr=='\t') ++ptr;
   sign=*ptr;
   if (sign=='+'||sign=='-') ++ptr;
   value=0;
   while (*ptr>='0'&&*ptr<='9') value=value*10+*ptr++-'0';
   if (sign=='-') value=-value;
   return value;
}

s64t __stdcall atoi64(const char *ptr) {
   register s64t value;
   char sign;
   if (!ptr) return 0;
   while (*ptr==' '||*ptr=='\t') ++ptr;
   sign=*ptr;
   if (sign=='+'||sign=='-') ++ptr;
   value=0;
   while (*ptr>='0'&&*ptr<='9') value=value*10+*ptr++-'0';
   if (sign=='-') value=-value;
   return value;
}

// ignoring any float ops
int __cdecl sscanf(const char *in_string, const char *format, ...) {
   return 0;
}
double __stdcall atof(const char *ptr) {
   return 0.0;
}

char* __stdcall strdup(const char *str) {
   u32t len=str?strlen(str):0;
   if (!str) return 0; else {
      char *rc = (char*)malloc(len+1);
      if (len) strcpy(rc,str); else *rc=0;
      return rc;
   }
}

int __stdcall stricmp( const char *s1, const char *s2 ) {
   return strnicmp(s1,s2,strlen(s1)+1);
}

int __stdcall strcmp(const char *s1, const char *s2) {
   return strncmp(s1,s2,strlen(s1)+1);
}

#define is_hexstr(p) (pp[0]=='0'&&(p[1]&~0x20)=='X')

static int radix_value(char cc) {
   if (cc>='0'&&cc<='9') return cc-'0';
   cc=tolower(cc);
   if (cc>='a'&&cc<='i') return cc-'a'+10;
   if (cc>='j'&&cc<='r') return cc-'j'+19;
   if (cc>='s'&&cc<='z') return cc-'s'+28;
   return 37;
}

static u32t _stol(const char *nptr,char **endptr,int base,int is_signed) {
   u32t  value, prev_value, ofcheck;
   const char *pp, *startp;
   int   digit,overflow;
   char  sign;

   if (endptr) *endptr=(char *)nptr;
   pp=nptr;
   while (*pp==' '||*pp=='\t') ++pp;
   sign=*pp;
   if (sign=='+'||sign=='-') ++pp;
   if (base==0) {
      if (is_hexstr(pp)) base=16; else
      if (*pp=='0') base=8; else base=10;
   }
   if (base<2||base>36) return 0;
   if (base==16)
      if (is_hexstr(pp)) pp+=2;
   startp  = pp;
   overflow= 0;
   value   = 0;
   for (ofcheck = ULONG_MAX/base;;) {
      digit=radix_value(*pp);
      if (digit>=base) break;
      if (value>ofcheck) overflow=1;
      prev_value=value;
      value=value*base+digit;
      if (value<prev_value) overflow=1;
      ++pp;
   }
   if (pp==startp) pp=nptr;
   if (endptr) *endptr=(char *)pp;
   if (is_signed&&value>=0x80000000)
      if (value!=0x80000000||sign!='-') overflow=1;
   if (overflow) {
      if (!is_signed) return ULONG_MAX;
      if (sign=='-') return LONG_MIN;
      return LONG_MAX;
   }
   if (sign=='-') value=-value;
   return value;
}

u32t  __stdcall strtoul(const char *ptr,char **endptr,int base) {
   return _stol(ptr,endptr,base,0);
}

s32t  __stdcall strtol(const char *ptr,char **endptr,int base) {
   return _stol(ptr,endptr,base,1);
}


static u64t _stol64(const char *nptr,char **endptr,int base,int is_signed) {
   u64t  value, prev_value, ofcheck;
   const char *pp, *startp;
   int   digit,overflow;
   char  sign;

   if (endptr) *endptr=(char *)nptr;
   pp=nptr;
   while (*pp==' '||*pp=='\t') ++pp;
   sign=*pp;
   if (sign=='+'||sign=='-') ++pp;
   if (base==0) {
      if (is_hexstr(pp)) base=16; else
      if (*pp=='0') base=8; else base=10;
   }
   if (base<2||base>36) return 0;
   if (base==16)
      if (is_hexstr(pp)) pp+=2;
   startp  = pp;
   overflow= 0;
   value   = 0;
   for (ofcheck = ULLONG_MAX/base;;) {
      digit=radix_value(*pp);
      if (digit>=base) break;
      if (value>ofcheck) overflow=1;
      prev_value=value;
      value=value*base+digit;
      if (value<prev_value) overflow=1;
      ++pp;
   }
   if (pp==startp) pp=nptr;
   if (endptr) *endptr=(char *)pp;
   if (is_signed&&value>=0x8000000000000000)
      if (value!=0x8000000000000000||sign!='-') overflow=1;
   if (overflow) {
      if (!is_signed) return ULLONG_MAX;
      if (sign=='-') return LLONG_MIN;
      return LLONG_MAX;
   }
   if (sign=='-') value=-value;
   return value;
}

u64t  __stdcall strtoull(const char *ptr,char **endptr,int base) {
   return _stol64(ptr,endptr,base,0);
}

s64t  __stdcall strtoll(const char *ptr,char **endptr,int base) {
   return _stol64(ptr,endptr,base,1);
}


char* __stdcall strupr(char *str) {
   if (!str) return str; else {
      char *cp = str;
      while (*cp) { *cp=toupper(*cp); cp++; }
   }
   return str;
}

char* __stdcall strlwr(char *str) {
   if (!str) return str; else {
      char *cp = str;
      while (*cp) { *cp=tolower(*cp); cp++; }
   }
   return str;
}

char* __stdcall strcat(char *dst, const char *src) {
   strcpy(dst+strlen(dst), src);
   return dst;
}

char* __stdcall strcat_dyn(char *dst, const char *src) {
   int slen = src?strlen(src)+1:0,
       dlen = 0;
   if (!dst) dst = (char*)malloc(slen+10); else {
      dlen = strlen(dst);
      if (memBlockSize(dst)<dlen+slen+1) dst = (char*)realloc(dst,dlen+slen+10);
   }
   if (slen) memcpy(dst+dlen,src,slen); else
   if (!dlen) *dst = 0;
   return dst;
}

static const unsigned bits[]={0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};

static void _setbits(u8t *vector, const char *charset) {
   u8t cc;
   memset(vector,0,32);
   for (;cc=*charset;++charset) vector[cc>>3]|=bits[cc&0x07];
}

char *__stdcall strpbrk(const char *str, const char *charset) {
   u8t vector[32];
   if (!charset||!*charset) return 0;
   _setbits(vector,charset);
   while (*str) {
      u8t ch = *str;
      if ((vector[ch>>3]&bits[ch&0x07])!=0) return (char*)str;
      str++;
   }
   return 0;
}

// do not use START_EXPORT() here (used from trap screen)
char *__stdcall strncat(char *dst, const char *src, size_t n) {
   int lend = strlen(dst),
       lens = strlen(src);
   if (n < lens) lens = n;
   memcpy(dst+lend, src, lens);
   dst[lend+lens] = 0;
   return dst;
}

int __cdecl sprintf(char *buf, const char *format, ...) {
   u32t sz;
   va_list arglist;
   va_start(arglist,format);
   sz=_vsnprintf(buf,65536,format,arglist);
   va_end(arglist);
   return sz;
}

int __stdcall puts(const char *buf) {
   int rc = printf(buf);
   return rc + printf("\n");
}

void __stdcall perror(const char *prefix) {
   printf(prefix);
   printf(": ");
   cmd_shellerr(errno,0);
}

u32t __stdcall strccnt(const char *str, char ch) {
   char *pch;
   u32t   rc = 0;
   if (!str) return 0;
   pch = strchr(str, ch);
   while (pch++) {
      rc++;
      pch = strchr(pch,ch);
   }
   return rc;
}

void __stdcall setbits(void *dst, u32t pos, u32t count, u32t flags) {
   if (count&&dst) {
      if (flags&SBIT_STREAM) { // bit stream bits array
         u8t       *ptr = (u8t*)dst + (pos>>3);
         auto u32t mcnt = pos&7;

         if (mcnt) {
            auto u8t mask = count<8 ? 0xFF << 8-count : 0xFF;
            if (flags&SBIT_ON) *ptr++ |= mask >> mcnt;
               else *ptr++ &= ~(mask >> mcnt);
            if (count<8-mcnt) count=0; else count-=8-mcnt;
         }
         mcnt = count>>3;
         if (mcnt) memset(ptr, flags&SBIT_ON ? 0xFF : 0, mcnt);
         ptr += mcnt;
         mcnt = count&7;
         if (mcnt) {
            auto u8t mask = 0xFF << 8-mcnt;
            if (flags&SBIT_ON) *ptr++ |= mask;
               else *ptr++ &= ~mask;
         }
      } else {                 // dword stream bits array
         u32t *ptr = (u32t*)dst + (pos>>5);
         auto u32t mcnt = pos&0x1F;

         if (mcnt) {
            auto u32t mask = count<31 ? FFFF << 32-count : FFFF;
            if (flags&SBIT_ON) *ptr++ |= mask >> mcnt;
               else *ptr++ &= ~(mask >> mcnt);
            if (count<32-mcnt) count=0; else count-=32-mcnt;
         }
         mcnt = count>>5;
         if (mcnt) memsetd(ptr, flags&SBIT_ON ? FFFF : 0, mcnt);
         ptr += mcnt;
         mcnt = count&0x1F;
         if (mcnt) {
            auto u32t mask = FFFF << 32-mcnt;
            if (flags&SBIT_ON) *ptr++ |= mask;
               else *ptr++ &= ~mask;
         }
      }
   }
}

int __stdcall isspace(int cc) {
   return cc==' '||cc=='\f'||cc=='\n'||cc=='\r'||cc=='\t'||cc=='\v'?1:0;
}

int __stdcall isalpha(int cc) {
   return cc>='a'&&cc<='z'||cc>='A'&&cc<='Z'?1:0;
}

int __stdcall isalnum(int cc) {
   return cc>='a'&&cc<='z'||cc>='A'&&cc<='Z'||cc>='0'&&cc<='9'?1:0;
}

int __stdcall isdigit(int cc) {
   return cc>='0'&&cc<='9'?1:0;
}

int __stdcall abs(int value) {
   return value<0?-value:value;
}

static void swap(register u8t *s1, register u8t *s2, int nn) {
   for (++nn, --s1,--s2; --nn;) { 
      register u8t tmp = *++s1; 
      *s1 = *++s2; 
      *s2 = tmp; 
   }
}

void __stdcall qsort(void *base, size_t num, size_t width,
   int __stdcall (*compare)(const void *, const void *))
{
   if (num>1) {
      size_t iCnt = num, 
          iOffset = iCnt/2;
      while (iOffset) {
         size_t iLimit = iCnt-iOffset, iRow, iSwitch;
         do {
            iSwitch = 0;
            for (iRow=0; iRow<iLimit; iRow++) {
               u8t *fp1 = (u8t*)base + iRow*width,
                   *fp2 = fp1 + iOffset*width;
               if ((*compare)(fp1,fp2)>0) { 
                  swap(fp1, fp2, width);
                  iSwitch = iRow; 
               }
            }
         } while (iSwitch);
         iOffset = iOffset/2;
      }
   } 
}

int __stdcall START_EXPORT(chdir)(const char *path) {
   char namebuf[_MAX_PATH];
   u8t     chd = hlp_curdisk(), setd;
   FRESULT err;
   pathcvt(path,namebuf);

   if (namebuf[1]==':') {
      setd = namebuf[0]-'0';
      if (!hlp_chdisk(setd)) { errno=ENOENT; return -1; }
   }
   if (!hlp_chdir(namebuf)) { errno=ENOENT; return -1; }
   if (namebuf[1]==':') hlp_chdisk(chd);

   errno=EZERO;
   return 0;
}

char* __stdcall START_EXPORT(getcwd)(char *buf,size_t size) {
   char *fp = hlp_curdir(hlp_curdisk());
   int  len;
   if (!fp) return 0;
   len = strlen(fp);
   if (buf && size<len+3) { errno=ERANGE; return 0; }

   if (!buf) buf=(char*)malloc(len+3);
   if (fp[1]==':') {
      strcpy(buf,fp);
      *buf+='A'-'0';
   } else {
      buf[0] = 'A'+hlp_curdisk();
      buf[1] = ':';
      strcpy(buf+2,fp);
   }
   errno=EZERO;
   return buf;
}

int __stdcall START_EXPORT(mkdir)(const char *path) {
   if (!path || !*path) errno=EINVAL; else {
      char namebuf[_MAX_PATH], *cp;
      FRESULT rc;
      pathcvt(path,namebuf);
      // fix name
      cp = namebuf;
      do {
         cp = strchr(cp,'/');
         if (cp) *cp='\\';
      } while (cp);
      // create path.
      cp = strchr(namebuf,'\\');
      while (cp) {
         *cp  = 0;
         rc   = f_mkdir(namebuf);
         *cp++= '\\';
         if (rc!=FR_EXIST && rc!=FR_OK) break;
         cp = strchr(cp,'\\');
      }
      // final target
      if (!cp) rc = f_mkdir(namebuf);

      if (rc==FR_OK) { errno=EZERO; return 0; }
      set_errno2(rc);
   }
   return -1;
}

int __stdcall START_EXPORT(rmdir)(const char *path) {
   if (!path) errno=EINVAL; else {
      char namebuf[_MAX_PATH];
      FRESULT err;
      pathcvt(path,namebuf);
      err = f_unlink(namebuf);
      if (err==FR_OK) { errno=EZERO; return 0; }
      set_errno2(err);
   }
   return -1;
}

#define STAT_SIGN   (0x52494452)

typedef struct {
   u32t     sign;
   DIR       dir;
   int      call;   // call type - 1 = opendir, 2 = _dos_findfirst
   u16t    attrs;
   char     mask[_MAX_PATH+1];
} StatInfo;

#define checkret_direrr(x,calltype)         \
   StatInfo *si=(StatInfo*)(fp->d_sysdata); \
   if (!si||si->sign!=STAT_SIGN||si->call!=calltype) { errno=EINVAL; return x; }

dir_t* __stdcall START_EXPORT(opendir)(const char*dpath) {
   FRESULT  err;
   u32t     len = sizeof(dir_t)+strlen(dpath)+2+sizeof(StatInfo);
   dir_t    *rc = (dir_t*)malloc(len);
   StatInfo *si = (StatInfo*)(rc+1);
   memZero(rc);
   rc->d_openpath = (char*)(si+1);
   // recode "A:-Z:" path to "0:-3:\"
   pathcvt(dpath,rc->d_openpath);

   err = f_opendir(&(si->dir),rc->d_openpath);
   if (err!=FR_OK) {
      //log_it(2,"opendir(%s)=%d\n",rc->d_openpath,err);
      free(rc);
      set_errno2(err);
      return 0;
   }
   rc->d_sysdata = si;
   si->sign      = STAT_SIGN;
   si->call      = 1;
   errno = EZERO;
   return rc;
}

dir_t* __stdcall START_EXPORT(readdir)(dir_t *fp) {
   char     lfn[_MAX_PATH+1];
   FRESULT  err;
   FILINFO  fid;
   checkret_direrr(0,1);

   fid.lfname = lfn;
   fid.lfsize = _MAX_PATH;
   lfn[0] = 0;

   err = f_readdir(&(si->dir),&fid);
   if (err!=FR_OK||!fid.fname[0]) {
      set_errno2(err);
      return 0;
   }
   fp->d_size = fid.fsize;
   fp->d_date = fid.fdate;
   fp->d_time = fid.ftime;
   fp->d_attr = fid.fattrib;
   if (*fid.lfname) strncpy(fp->d_name,fid.lfname,NAME_MAX+1);
      else strncpy(fp->d_name,fid.fname,13);
   errno = EZERO;
   return fp;
}

int __stdcall START_EXPORT(closedir)(dir_t *fp) {
   FRESULT  err;
   checkret_direrr(EINVAL,1);
   err = f_closedir(&(si->dir));
   if (err) set_errno2(err);
   free(fp);
   return err?1:0;
}

u16t __stdcall START_EXPORT(_dos_stat)(const char *path, dir_t *fp) {
   FILINFO fi;
   FRESULT err;
   char namebuf[_MAX_PATH+1], lfn[_MAX_PATH+1];
   if (!fp) return errno=EINVAL;
   pathcvt(path,namebuf);
   memset(fp,0,sizeof(dir_t));
   fi.lfname = lfn;
   fi.lfsize = _MAX_PATH;
   lfn[0] = 0;

   err = f_stat(namebuf,&fi);
   if (err!=FR_OK) {
      char  extbuf[_MAX_EXT];
      FRESULT err2;
      DIR       dt;
      err2   = f_opendir(&dt,namebuf);
      if (err2!=FR_OK) {
         set_errno2(err);
         return errno;
      }
      _splitpath(namebuf,0,0,fp->d_name,extbuf);
      strcat(fp->d_name,extbuf);
      fp->d_attr = _A_SUBDIR;
      f_closedir(&dt);
   } else {
      fp->d_size = fi.fsize;
      fp->d_date = fi.fdate;
      fp->d_time = fi.ftime;
      fp->d_attr = fi.fattrib;

      if (*fi.lfname) {
         strncpy(fp->d_name,fi.lfname,NAME_MAX+1);
         fp->d_name[NAME_MAX] = 0;
      } else {
         strncpy(fp->d_name,fi.fname,13);
         fp->d_name[12] = 0;
      }
   }
   fp->d_openpath = &fp->d_name[NAME_MAX];
   fp->d_sysdata  = 0;
   errno = EZERO;
   return 0;
}

int __stdcall START_EXPORT(_matchpattern)(const char *pattern, const char *str) {
   static char *all="*.*";
   if (!pattern||!*pattern) pattern = all;
   if (!str) return 0;

   for (;;++str) {
      char strc =toupper(*str);
      char patternc=toupper(*pattern++);
      switch (patternc) {
         case 0:
            return strc==0;
         case '?':
            if (strc == 0) return false;
            break;
         case '*':
            if (*pattern==0) return true;
            if (*pattern=='.') {
               char *dot;
               if (pattern[1]=='*' && pattern[2]==0) return true;
               dot=strchr(str,'.');
               if (pattern[1]==0) return dot==NULL || dot[1]==0;
               if (dot!=NULL) {
                  str=dot;
                  if (strpbrk(pattern,"*?")==NULL && strchr(str+1,'.')==NULL)
                     return stricmp(pattern+1,str+1)==0;
               }
            }
            while (*str)
               if (_matchpattern(pattern,str++)) return true;
            return false;
         default:
            if (patternc != strc)
               if (patternc=='.' && strc==0) return _matchpattern(pattern,str);
                  else return false;
            break;
      }
   }
}

/* this code must be wrong in many cases, but it`s used as first time 
   implementation */
int __stdcall _replacepattern(const char *frompattern, const char *topattern,
                              const char *from, char *to)
{
   if (!to) return 0;
   *to = 0;
   if (!frompattern||!topattern||!from) return 0;
   if (!_matchpattern(frompattern, from)) return 0;

   while (true) {
      char patternc = *frompattern++,
              reppc = *topattern++;
      // end of destination pattern
      if (!reppc) { *to=0; break; }
      // end of source pattern or file name
      if (!patternc || !*from) {
         if (strpbrk(--topattern,"*?")) return 0;
         strcpy(to,topattern);
         break;
      }

      if (patternc=='?') {
         *to++ = reppc!='?'?reppc:*from;
         from++;
      } else
      if (patternc=='*') {
         char *pos = strpbrk(frompattern,"*?");
         if (pos==frompattern) return 0;
         // last entry?
         if (!*frompattern) {
            if (reppc=='*') strcpy(to, from); else {
               if (strpbrk(--topattern,"*?")) return 0;
               strcpy(to,topattern);
            }
            return 1;
         } else {
            int len=-1;
            if (!pos) pos = (char*)(frompattern+strlen(frompattern));

            while (from[++len])
               if (from[len]==*frompattern)
                  if (!strnicmp(frompattern,from+len,pos-frompattern)) break;

            if (reppc=='*')
               while (len--) *to++ = *from++;
            else {
               from+=len;
               len = -1; topattern--;
               while (topattern[++len])
                  if (topattern[len]==*frompattern)
                     if (!strnicmp(frompattern,topattern+len,pos-frompattern)) break;
               while (len--)
                  if (strchr("*?",*to++=*topattern++)) return 0;
            }
         }
      } else { 
         if (reppc=='*') {
            int   len = -1;
            char *pos = strpbrk(frompattern,"*?");
            if (!pos) pos = (char*)(topattern+strlen(topattern));

            while (from[++len])
               if (from[len]==*topattern)
                  if (!strnicmp(topattern,from+len,pos-topattern)) break;

            while (len--) *to++ = *from++;
            frompattern += len;
         } else {
            *to++ = reppc!='?'?reppc:*from;
            from++;
         }
      }
   }
   return 1;
}

u16t __stdcall START_EXPORT(_dos_findfirst)(const char *path, u16t attributes, dir_t *fp) {
   int      len;
   FRESULT  err;
   char b_drv[_MAX_DRIVE], b_dir[_MAX_DIR],
        b_nam[_MAX_FNAME], b_ext[_MAX_EXT];
   StatInfo *si   = (StatInfo*)malloc(sizeof(StatInfo)+strlen(path)+2);
   memZero(si);
   fp->d_openpath = (char*)(si+1);
   si->call       = 2;
   si->attrs      = attributes;
   // recode A:-Z: path to 0:-3:
   pathcvt(path,fp->d_openpath);

   _splitpath(fp->d_openpath,b_drv,b_dir,b_nam,b_ext);
   strcpy(fp->d_openpath,b_drv);
   strcat(fp->d_openpath,b_dir);
   len = strlen(fp->d_openpath);
   // remove trailing / \ else FAT code will fail
   if (len>3 && fp->d_openpath[1]==':' && (fp->d_openpath[len-1]=='\\' || fp->d_openpath[len-1]=='/'))
      fp->d_openpath[len-1] = 0;

   strcpy(si->mask,b_nam);
   strcat(si->mask,b_ext);

   //log_printf("_dos_findfirst(%s%s,%04X,%08X)\n",fp->d_openpath,si->mask,si->attrs,fp);

   err = f_opendir(&(si->dir),fp->d_openpath);
   if (err!=FR_OK) {
      free(si);
      //log_printf("_dos_findfirst error %d\n",err);
      set_errno2(err);
      return err;
   }
   si->sign       = STAT_SIGN;
   fp->d_sysdata  = si;

   return _dos_findnext(fp);
}

u16t __stdcall START_EXPORT(_dos_findclose)(dir_t *fp) {
   FRESULT  err;
   checkret_direrr(FR_INVALID_PARAMETER,2);
   err = f_closedir(&(si->dir));
   if (err) set_errno2(err);
   fp->d_sysdata = 0;
   si->sign      = 0;
   free(si);
   return err;
}

u16t __stdcall START_EXPORT(_dos_findnext)(dir_t *fp) {
   char     lfn[_MAX_PATH+1];
   FRESULT  err;
   FILINFO   fi;
   checkret_direrr(FR_INVALID_PARAMETER,2);
   fi.lfname = lfn;
   fi.lfsize = _MAX_PATH;
   lfn[0] = 0;

   while (true) {
      char *nameptr;
      err     = f_readdir(&(si->dir),&fi);
      nameptr = *fi.lfname?fi.lfname:fi.fname;
      // error?
      if (err!=FR_OK||!*nameptr) {
         if (err==FR_OK) err=FR_NO_FILE;
         set_errno2(err);
         return err;
      }
      // filter by subdir flag only
      if (((si->attrs&_A_SUBDIR)==0||(fi.fattrib&AM_DIR)!=0) && _matchpattern(si->mask,nameptr)) {
         fp->d_size = fi.fsize;
         fp->d_date = fi.fdate;
         fp->d_time = fi.ftime;
         fp->d_attr = fi.fattrib;

         if (*fi.lfname) {
            strncpy(fp->d_name,fi.lfname,NAME_MAX+1);
         } else {
            strncpy(fp->d_name,fi.fname,13);
            fp->d_name[12] = 0;
         }
         fp->d_name[NAME_MAX] = 0;
         // point it to \0
         fp->d_openpath = &fp->d_name[NAME_MAX];
         break;
      }
   }
   errno = EZERO;
   return 0;
}

u32t  __stdcall _dos_getdiskfree(unsigned drive, diskfree_t *dspc) {
   static char path[12] = "0:\\";
   FATFS *pfat = 0;
   DWORD nclst = 0;
   if (!dspc) { errno=EINVAL; return 1; }

   drive   = !drive?hlp_curdisk():drive-1;
   path[0] = '0'+drive;
   if (f_getfree(path,&nclst,&pfat)!=FR_OK) { errno=EINVAL; return 1; }

   dspc->total_clusters      = hlp_disksize(QDSK_VIRTUAL|drive,0,0)/pfat->csize;
   dspc->avail_clusters      = nclst;
   dspc->sectors_per_cluster = pfat->csize;
   dspc->bytes_per_sector    = pfat->ssize;
   return 0;
}

int __stdcall unlink(const char *path) {
   char namebuf[_MAX_PATH];
   FRESULT err;
   pathcvt(path,namebuf);
   err = f_unlink(namebuf);
   if (err==FR_OK) { errno=EZERO; return 0; }
   set_errno2(err);
   return -1;
}


int __stdcall START_EXPORT(rename)(const char *oldname, const char *newname) {
   char nbold[_MAX_PATH], nbnew[_MAX_PATH];
   FRESULT err;
   pathcvt(oldname,nbold);
   pathcvt(newname,nbnew);
   err = f_rename(nbold,nbnew[1]==':'?nbnew+2:nbnew);
   if (err==FR_OK) { errno=EZERO; return 0; }
   set_errno2(err);
   return -1;
}

void __stdcall _dos_getdrive(unsigned *drive) {
   u8t drv = hlp_curdisk();
   if (drive) *drive = drv+1;
}

void __stdcall _dos_setdrive(unsigned drive, unsigned *total) {
   if (drive) hlp_chdisk(drive-1);
   if (total) {
      u8t drv = 1;
      while (hlp_curdir(drv)) drv++;
      *total = drv;
   }
}

void __stdcall START_EXPORT(_splitpath)(const char *path, char *drive, char *dir,
   char *fname, char *ext)
{
   char *rs1,*rs2;
   if (drive) *drive=0;
   if (dir)   *dir  =0;
   if (fname) *fname=0;
   if (ext)   *ext  =0;
   if (!path||!*path) return;
   if (path[1]==':') {
      if (drive) { *drive++=path[0]; *drive++=':'; *drive++=0; }
      path+=2;
   }
   // skip duplicates
   while (path[1]=='/'||path[1]=='\\') path++;
   rs1=strrchr(path,'\\');
   rs2=strrchr(path,'/');

   if (rs1&&rs2) rs1=rs1-path>rs2-path?rs1:rs2; else
      if (rs2) rs1=rs2;
   if (rs1) {
      if (dir) strncat(dir,path,++rs1-path);
      path=rs1;
   }
   rs1=strrchr(path,'.');
   if (rs1) {
      if (ext) strcpy(ext,rs1);
      if (fname) strncat(fname,path,rs1-path);
   } else
   if (fname) strcpy(fname,path);
}

char* __stdcall START_EXPORT(_fullpath)(char *buffer, const char *path, size_t size) {
   char tmp[_MAX_PATH+1], *str=tmp;
   u32t plen = path?strlen(path):0;
   if (plen>=_MAX_PATH-3) { errno = ERANGE; return 0; }

   if (path&&path[1]==':') {
      strcpy(tmp,path);
      if (tmp[0]>='0'&&tmp[0]<='9') tmp[0]+='A'-'0';
         else tmp[0]=toupper(tmp[0]);
   } else
   if (!path||path[0]!='/'&&path[0]!='\\') {
      u32t mplen;
      getcwd(tmp,_MAX_PATH+1);
      mplen = strlen(tmp);
      if (tmp[mplen-1]!='\\') { strcat(tmp,"\\"); mplen++; }
      if (mplen+plen+1>=_MAX_PATH) { errno = ERANGE; return 0; }
      strcat(tmp,path);
   } else {
      tmp[0] = 'A'+hlp_curdisk();
      tmp[1] = ':';
      tmp[2] = '\\';
      strcpy(tmp+3,path+1);
   }
   if (!buffer) buffer = (char*)malloc(size = _MAX_PATH+1);
   buffer[0]=0;

   do {
      str=strpbrk(str,"/.\\");
      if (!str) break;
      switch (*str) {
         case '.':
            if (*(str-1)=='\\') {
               u32t idx=1,dbl;
               if ((dbl=str[1]=='.')!=0) idx++; //  /./  /.\ /../ /..\\ /. /..
               if (str[idx]=='/'||str[idx]=='\\'||!str[idx]) {
                  char *ps=str-1;
                  if (dbl&&str>tmp+1) { // "/../path" work as "/path"
                     ps=str-2;
                     while (ps!=tmp&&*ps!='\\') ps--;
                  }
                  memmove(ps,str+idx,strlen(str+idx)+1);
                  str=ps-1;
               }
            } else
            if (str[1]==0) *str=0;
            break;
         case '/': *str='\\';
         case '\\':
            if (str!=tmp)
               if (*(str-1)=='\\')
                  memmove(str,str+1,strlen(str+1)+1);
            break;
      }
   } while (*++str);

   if (tmp[1]==':'&&tmp[2]==0) strcat(tmp,"\\");

   if (size<strlen(tmp)+1) {
      errno = ERANGE;
      return 0;
   } else {
      strcpy(buffer,tmp);
      return buffer;
   }
}

#define allmask(mask) (!mask||*mask==0||strcmp(mask,"*")==0||strcmp(mask,"*.*")==0)

static long readtree(const char *Dir, const char *Mask, dir_t **info,
   long owner, long pool, int subdirs, _dos_readtree_cb callback, void *cbinfo)
{
   long  count, all=allmask(Mask), rc=0, ii;
   dir_t *iout, *rdh, *rdi;
   // we're will NOT alloc 300 bytes in stack recursively ;)
   char *nm = (char*)malloc(NAME_MAX+1),
        *np = all?0:strupr(strdup(Mask)), *dptr;
   /* log_printf("readtree(%s,%s,%08X,%08X,%08X,%i,%08X,%08X)\n",Dir,Mask,info,
       owner,pool,subdirs,callback,cbinfo); */
   if (!owner) memGetUniqueID(&owner,&pool);
   // convert name and save it for dir_t.d_openpath
   dptr = (char*)memAlloc(owner,pool,strlen(Dir)+3);
   pathcvt(Dir,dptr);

   *info = 0;
   iout  = 0;
   ii    = 0;
   rdh   = opendir(dptr);
   count = 0;
   if (rdh) {
      do {
         rdi = readdir(rdh);
         if (rdi) {
            int isdir=rdi->d_attr&_A_SUBDIR?1:0;

            if (isdir && strcmp(rdi->d_name,".")==0) continue;
            else {
               char  *st=all?0:strupr(strdup(rdi->d_name));
               int match=all?1:_matchpattern(np,st);
               if (st) free(st);

               if (match||isdir) {
                  if (isdir&&!match) rdi->d_attr|=0x80; else rdi->d_attr&=0x7F;
                  // ask callback
                  if (callback) {
                     int cbrc = callback(rdi,cbinfo);
                     if (cbrc<0) { rc=-1; count=0; break; } else
                     if (!cbrc) continue;
                  }
                  // alloc memory
                  if (count+2>=ii) {
                     if (!ii) iout = (dir_t*)memAlloc(owner,pool,(ii=128)*sizeof(dir_t));
                        else iout = (dir_t*)memRealloc(iout,(ii*=2)*sizeof(dir_t));
                  }
                  memcpy(iout+count,rdi,sizeof(dir_t));
                  iout[count].d_sysdata  = 0;
                  iout[count].d_openpath = dptr;
                  count++;
               }
            }
         }
      } while (rdi);
      if (count) memset(iout+count,0,sizeof(dir_t));
      closedir(rdh);
   }

   if (count && subdirs)
      for (ii=0;ii<count;ii++)
         if ((iout[ii].d_attr&_A_SUBDIR)!=0 && strcmp(iout[ii].d_name,"..")!=0) {
            long     tmp = strlen(dptr), nestcnt;
            dir_t *iout2 = 0;

            strcpy(nm,dptr);
            if (nm[--tmp]=='\\') nm[tmp]='/';
            if (nm[tmp]!='/') strcat(nm+tmp,"/");
            strcat(nm+tmp,iout[ii].d_name);
            nestcnt=readtree(nm,Mask,&iout2,owner,pool,1,callback,cbinfo);
            if (nestcnt<0) { rc=-1; break; } // stopped in nest dir
            rc+=nestcnt;
            iout[ii].d_sysdata = iout2;
         }

   if (rc>=0) { rc+=count; *info=iout; }
   if (np) free(np);
   free(nm);
   return rc;
}

u32t  __stdcall START_EXPORT(_dos_readtree)(const char *dir, const char *mask,
  dir_t **info, int subdirs, _dos_readtree_cb callback, void *cbinfo)
{
   if (!info&&!callback) return 0; else {
      long rc;
      dir_t *iout = 0;
      rc = dir?readtree(dir,mask,&iout,0,0,subdirs,callback,cbinfo):0;
      if (!info||rc<=0) {
         int sverr = errno;
         _dos_freetree(iout);
         iout  = 0;
         errno = sverr;
      }
      if (info) *info = iout;
      return rc<0?0:rc;
   }
}

int  __stdcall START_EXPORT(_dos_freetree)(dir_t *info) {
   long  Owner,Pool;
   if (!info) return errno=EINVAL;
   // memmgr block?
   if (!memGetObjInfo(info,&Owner,&Pool)) return errno=EINVAL;
   // check memGetUniqueID`s owner to prevent free() opendir`s dir_t
   if (Owner==-2) memFreePool(Owner,Pool); else return errno=EINVAL;
   return errno=EZERO;
}

int __stdcall START_EXPORT(_access)(const char *path, int mode) {
   dir_t  fd;
   if (path)
      if (!_dos_stat(path,&fd)) { errno=EZERO; return 0; }
   errno=ENOENT;
   return -1;
}

char* __stdcall tmpdir(char *buffer) {
   static const char *tnames[4] = {"TMP",  "TEMP",  "TMPDIR", "TEMPDIR"};
   static char ncopy[NAME_MAX+1];
   u32t ii;

   if (!buffer) buffer = ncopy;

   for (ii=0; ii<4; ii++) {
      char *dp = getenv(tnames[ii]);
      if (dp) {
         dir_t  fd;
         int   len = strlen(dp);
         if (len>0 && len<NAME_MAX) {
            strcpy(buffer, dp);
            // remove trailing \, but not from "A:\" and from "/"
            if (len>1 && (len!=3 || buffer[1]!=':')  && (buffer[len-1]=='/' ||
               buffer[len-1]=='\\')) buffer[len-1] = 0;

            if (!_dos_stat(buffer,&fd))
               if (fd.d_attr&_A_SUBDIR) return buffer;
         }
      }
   }
   *buffer = 0;
   return 0;
}

u16t __stdcall START_EXPORT(_dos_setfileattr)(const char *path, unsigned attributes) {
   if (!path||(attributes&(_A_VOLID|_A_SUBDIR))!=0) return errno=EINVAL;
   else {
      char namebuf[_MAX_PATH];
      FRESULT err;
      pathcvt(path,namebuf);
      err = f_chmod(namebuf,attributes,AM_RDO|AM_ARC|AM_SYS|AM_HID);
      if (err==FR_OK) { errno=EZERO; return 0; }
      set_errno2(err);
      return errno;
   }
}

u16t __stdcall START_EXPORT(_dos_getfileattr)(const char *path, unsigned *attributes) {
   dir_t  fd;
   if (attributes) *attributes = 0;
   if (!attributes||!path) return errno=EINVAL;
   if (!_dos_stat(path,&fd)) {
      *attributes = fd.d_attr;
      errno=EZERO;
      return 0;
   }
   return errno;
}

u16t __stdcall START_EXPORT(_dos_setfiletime)(const char *path, u32t dostime) {
   char namebuf[_MAX_PATH];
   FRESULT err;
   FILINFO fi;
   if (!path) return errno=EINVAL;
   pathcvt(path,namebuf);
   fi.fdate = dostime>>16;
   fi.ftime = (WORD)dostime;
   err = f_utime(namebuf,&fi);
   if (err==FR_OK) { errno=EZERO; return 0; }
   set_errno2(err);
   return errno;
}

u16t __stdcall START_EXPORT(_dos_getfiletime)(const char *path, u32t *dostime) {
   dir_t  fd;
   if (!path||!dostime) return errno=EINVAL;
   if (!_dos_stat(path,&fd)) {
      errno=EZERO;
      return ((u32t)fd.d_date)<<16|fd.d_time;
   }
   if (!errno) errno=ENOENT;
   return errno;
}

