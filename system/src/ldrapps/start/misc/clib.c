//
// QSINIT "start" module
// subset of C library functions
//
#include "stdarg.h"
#include "qserr.h"
#include "qsio.h"
#include "errno.h"
#include "qsutil.h"
#include "qcl/qslist.h"
#include "sys/stat.h"
#include "qsshell.h"
#include "syslocal.h"
#include "limits.h"

int* __stdcall _get_errno(void) { 
   u64t *rc;
   mt_tlsaddr(QTLS_ERRNO, &rc);
   return (int*)rc;
}

// translated to _get_errno() call
int set_errno(int errv) { errno = errv; return errv; }

int get_errno(void) { return errno; }

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

char* __stdcall strdup(const char *str) {
   u32t len=str?strlen(str):0;
   if (!str) return 0; else {
      char *rc = (char*)malloc_local(len+1);
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

u32t __stdcall str2ulong(const char *str) {
   while (*str==' ' || *str==' ') str++;
   while (*str=='0'&&str[1]>='0'&&str[1]<='9') str++;
   return _stol(str,0,0,0);
}

s64t __stdcall str2int64(const char *str) {
   while (*str==' ' || *str==' ') str++;
   while (*str=='0'&&str[1]>='0'&&str[1]<='9') str++;
   return _stol64(str,0,0,1);
}

u64t __stdcall str2uint64(const char *str) {
   while (*str==' ' || *str==' ') str++;
   while (*str=='0'&&str[1]>='0'&&str[1]<='9') str++;
   return _stol64(str,0,0,0);
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
   if (!dst) dst = (char*)malloc_local(slen+10); else {
      dlen = strlen(dst);
      if (mem_blocksize(dst)<dlen+slen+1) dst = (char*)realloc(dst,dlen+slen+10);
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

size_t __stdcall strspn(const char *str, const char *charset) {
   size_t  len = 0, tc;
   u8t  vector[32];
   _setbits(vector, charset);

   for (;tc = (u8t)*str; ++str,++len)
      if ((vector[tc>>3]&bits[tc&0x07])==0) break;
   return len;
}

char *__stdcall _strspnp(const char *str, const char *charset) {
   size_t index = strspn(str, charset);
   return str[index] ? (char*)str + index : 0;
}

char* __stdcall trimleft(char *src, char *charset) {
   u32t len = strspn(src, charset);
   if (len) memmove(src, src+len, strlen(src+len)+1);
   return src;
}

char* __stdcall trimright(char *src, char *charset) {
   u32t len = strlen(src);
   while (len--)
      if (strchr(charset,src[len])) src[len] = 0; else break;
   return src;
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
   // ugly, isn`t it? ;)
   sz = _vsnprintf(buf,65536,format,arglist);
   va_end(arglist);
   return sz;
}

void __stdcall perror(const char *prefix) {
   u32t value = get_errno();
   printf(prefix);
   printf(": ");
   cmd_shellerr(EMSG_CLIB, value, 0);
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

char __stdcall strclast(const char *str) {
   if (!str || !*str) return 0;
   return str[strlen(str)-1];
}

u32t __stdcall replacechar(char *str, char from, char to) {
  if (!str || from==to || !from || !to) return 0; else {
     char  *ch = strchr(str,from);
     u32t  res = 0;
     while (ch) {
        *ch++=to; res++;
        if (*ch!=from) ch = strchr(ch,from);
     }
     return res;
  }
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
            if (flags&SBIT_ON) *ptr++|=mask; else *ptr++&=~mask;
         }
      } else {                 // dword stream bits array
         u32t *ptr = (u32t*)dst + (pos>>5);
         auto u32t mcnt = pos&0x1F;

         if (mcnt) {
            auto u32t mask = count<31 ? FFFF >> 32-count : FFFF;
            if (flags&SBIT_ON) *ptr++ |= mask << mcnt;
               else *ptr++ &= ~(mask << mcnt);
            if (count<32-mcnt) count=0; else count-=32-mcnt;
         }
         mcnt = count>>5;
         if (mcnt) memsetd(ptr, flags&SBIT_ON ? FFFF : 0, mcnt);
         ptr += mcnt;
         mcnt = count&0x1F;
         if (mcnt) {
            auto u32t mask = FFFF >> 32-mcnt;
            if (flags&SBIT_ON) *ptr++|=mask; else *ptr++&=~mask;
         }
      }
   }
}

int __stdcall isspace(int cc) {
   return cc==' '||cc>='\t'&&cc<='\r'?1:0;
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

int __stdcall isxdigit(int cc) {
   return cc>='0'&&cc<='9'||cc>='a'&&cc<='f'||cc>='A'&&cc<='F'?1:0;
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

int __stdcall qserr2errno(qserr errv) {
   switch (errv) {
      case E_OK               :return 0;
      case E_SYS_DONE         :return EALREADY;
      case E_SYS_NOMEM        :return ENOMEM;
      case E_SYS_NOFILE       :
      case E_SYS_NOPATH       :return ENOENT;
      case E_SYS_EXIST        :return EEXIST;
      case E_SYS_ACCESS       :return EACCES;
      case E_SYS_FILES        :return EMFILE;
      case E_SYS_SHARE        :return EAGAIN;
      case E_SYS_TIMEOUT      :return EBUSY;
      case E_SYS_UNSUPPORTED  :return ENOLCK;
      case E_SYS_INVNAME      :
      case E_SYS_INCOMPPARAMS :
      case E_SYS_INVPARM      :
      case E_SYS_INVOBJECT    :
      case E_SYS_ZEROPTR      :
      case E_DSK_BADVOLNAME   :
      case E_SYS_INVHTYPE     :
      case E_SYS_DISKMISMATCH :
      case E_SYS_INVTIME      :return EINVAL;
      case E_SYS_LONGNAME     :return ENAMETOOLONG;
      case E_SYS_FSLIMIT      :return EFBIG;
      case E_SYS_BUFSMALL     :return ERANGE;
      case E_SYS_PRIVATEHANDLE:return EPERM;
      case E_SYS_SEEKERR      :return ESPIPE;
      case E_SYS_DELAYED      :return EINPROGRESS;
      case E_SYS_BROKENFILE   :return EPIPE;
      case E_SYS_NONINITOBJ   :
      case E_SYS_INITED       :return EINVOP;
      case E_SYS_READONLY     :return EROFS;
      case E_SYS_TOOLARGE     :
      case E_SYS_TOOSMALL     :return EINVAL;
      case E_DSK_ERRREAD      :
      case E_DSK_ERRWRITE     :return EIO;
      case E_DSK_NOTREADY     :
      case E_DSK_WP           :return EROFS;
      case E_DSK_UNKFS        :return ENOMNT;
      case E_DSK_NOTMOUNTED   :return ENOMNT;
   }
   return ERANGE;
}

int set_errno_qserr(u32t err) { 
   int rc = qserr2errno(err);
   set_errno(rc);
   return rc;
}

char* __stdcall START_EXPORT(getcwd)(char *buf,size_t size) {
   qserr err = io_curdir(buf, size);
   if (err) set_errno_qserr(err);
   return err?0:buf;
}

int __stdcall START_EXPORT(chdir)(const char *path) {
   qserr err = io_chdir(path);
   if (err) set_errno_qserr(err);
   return err?-1:0;
}

int __stdcall START_EXPORT(mkdir)(const char *path) {
   qserr err = io_mkdir(path);
   if (err) set_errno_qserr(err);
   return err?-1:0;
}

int __stdcall START_EXPORT(rmdir)(const char *path) {
   qserr err = io_rmdir(path);
   if (err) set_errno_qserr(err);
   return err?-1:0;
}

#define STAT_SIGN   (0x52494452)

typedef struct {
   u32t            sign;
   dir_handle        dh;
   io_direntry_info  de;
   int             call;   // call type - 1 = opendir, 2 = _dos_findfirst
   u16t           attrs;
   char         dirpath[QS_MAXPATH+1];
   char            mask[QS_MAXPATH+1];
} StatInfo;

#define checkret_direrr(x,calltype)           \
   StatInfo *si = (StatInfo*)(fp->d_sysdata); \
   if (!si || si->sign!=STAT_SIGN || si->call!=calltype) \
      { set_errno(EINVAL); return x; }

dir_t* __stdcall START_EXPORT(opendir)(const char *dpath) {
   qserr      err;
   u32t       len = sizeof(dir_t) + sizeof(StatInfo);
   dir_t      *rc = (dir_t*)calloc(len,1);
   StatInfo   *si = (StatInfo*)(rc+1);
   rc->d_openpath = si->dirpath;
   // set process as block owner
   mem_localblock(rc);

   err = io_fullpath (si->dirpath, dpath, QS_MAXPATH+1);
   if (!err) err = io_diropen(si->dirpath, &si->dh);
   if (err) {
      free(rc);
      set_errno_qserr(err);
      return 0;
   }
   if (strclast(si->dirpath)!='\\') strcat(si->dirpath, "\\");
   rc->d_sysdata = si;
   si->sign      = STAT_SIGN;
   si->call      = 1;
   return rc;
}

dir_t* __stdcall START_EXPORT(readdir)(dir_t *fp) {
   qserr      err;
   checkret_direrr(0,1);

   err = io_dirnext(si->dh, &si->de);
   if (err) { set_errno_qserr(err); return 0; }

   fp->d_size   = si->de.size;
   fp->d_wtime  = io_iototime(&si->de.wtime);
   fp->d_ctime  = io_iototime(&si->de.ctime);
   fp->d_attr   = si->de.attrs;
   strcpy(fp->d_name, si->de.name);
   return fp;
}

int __stdcall START_EXPORT(closedir)(dir_t *fp) {
   qserr      err;
   checkret_direrr(EINVAL,1);
   err = io_dirclose(si->dh);
   if (err) set_errno_qserr(err);
   fp->d_sysdata = 0;
   si->sign      = 0;
   free(fp);
   // it would be nice in trace
   return err/*?1:0*/;
}

qserr __stdcall START_EXPORT(_dos_stat)(const char *path, dir_t *fp) {
   io_handle_info  hi;
   qserr          err;
   if (!path||!fp) { set_errno(EINVAL); return E_SYS_INVPARM; }

   err = io_pathinfo(path, &hi);
   if (err) set_errno_qserr(err); else {
      fp->d_wtime = io_iototime(&hi.wtime);
      fp->d_ctime = io_iototime(&hi.ctime);
      fp->d_size  = hi.size;
      fp->d_attr  = hi.attrs;

      if (io_fullpath(fp->d_name, path, NAME_MAX+1)) fp->d_name[0] = 0;
      fp->d_name[NAME_MAX] = 0;
      fp->d_openpath = &fp->d_name[NAME_MAX];
      fp->d_sysdata  = 0;
   }
   return err;
}

int __stdcall stat(const char *path, struct stat *fi) {
   io_handle_info  hi;
   qserr          err;
   if (!path||!fi) { set_errno(EINVAL); return -1; }

   err = io_pathinfo(path, &hi);
   if (err) set_errno_qserr(err); else {
      memset(fi, 0, sizeof(struct stat));
      fi->st_nlink = 1;
      fi->st_size  = hi.size;
      fi->st_atime = io_iototime(&hi.wtime);
      fi->st_mtime = fi->st_atime;
      fi->st_ctime = io_iototime(&hi.ctime);
      fi->st_dev   = hi.vol;

      fi->st_mode  = S_IRUSR|S_IRGRP|S_IROTH;
      fi->st_mode |= hi.attrs&IOFA_DIR?S_IFDIR:S_IFREG;
      if ((hi.attrs&IOFA_READONLY)==0) fi->st_mode |= S_IWUSR|S_IWGRP|S_IWOTH;
   }
   return err?-1:0;
}

int __stdcall START_EXPORT(_matchpattern)(const char *pattern, const char *str) {
   static char *all="*.*";
   if (!pattern||!*pattern) pattern = all;
   if (!str) return 0;

   for (;;++str) {
      char strc     = toupper(*str);
      char patternc = toupper(*pattern++);
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

/* this code should be wrong in many cases, but it is used as a first time
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

qserr __stdcall START_EXPORT(_dos_findfirst)(const char *path, u16t attributes, dir_t *fp) {
   StatInfo *si = (StatInfo*)calloc(sizeof(StatInfo),1);
   qserr    err = io_fullpath(si->dirpath, path, QS_MAXPATH+1);
   if (!err) {
      char *cp = strrchr(si->dirpath, '\\');
      if (!cp) {
         strcpy(si->mask, si->dirpath);
         si->dirpath[0] = 0;
      } else {
         strcpy(si->mask, ++cp);
         *cp = 0;
      }
      err = io_diropen(si->dirpath, &si->dh);
   }
   if (err) {
      free(si);
      set_errno_qserr(err);
      return err;
   }
   fp->d_openpath = si->dirpath;
   fp->d_sysdata  = si;
   si->sign       = STAT_SIGN;
   si->call       = 2;
   si->attrs      = attributes;
   // set process as block owner
   mem_localblock(si);
   return _dos_findnext(fp);
}

qserr __stdcall START_EXPORT(_dos_findclose)(dir_t *fp) {
   qserr      err;
   checkret_direrr(E_SYS_INVPARM,2);
   err = io_dirclose(si->dh);
   if (err) set_errno_qserr(err);
   fp->d_sysdata = 0;
   si->sign      = 0;
   free(si);
   return err;
}

qserr __stdcall START_EXPORT(_dos_findnext)(dir_t *fp) {
   checkret_direrr(E_SYS_INVPARM,2);

   while (true) {
      qserr err = io_dirnext(si->dh, &si->de);
      if (err) { set_errno_qserr(err); return err; }
      // filter by subdir flag only
      if (((si->attrs&_A_SUBDIR)==0 || (si->de.attrs&IOFA_DIR)!=0) &&
         _matchpattern(si->mask,si->de.name)) 
      {
         fp->d_size     = si->de.size;
         fp->d_wtime    = io_iototime(&si->de.wtime);
         fp->d_ctime    = io_iototime(&si->de.ctime);
         fp->d_attr     = si->de.attrs;
         fp->d_openpath = si->dirpath;
         strcpy(fp->d_name, si->de.name);
         break;
      }
   }
   return 0;
}

qserr __stdcall _dos_getdiskfree(unsigned drive, diskfree_t *dspc) {
   disk_volume_data vi;
   qserr           err;

   drive = !drive?io_curdisk():drive-1;
   err   = io_volinfo(drive, &vi);
   if (err) { set_errno_qserr(err); return err; }

   dspc->total_clusters      = vi.ClTotal;
   dspc->avail_clusters      = vi.ClAvail;
   dspc->sectors_per_cluster = vi.ClSize;
   dspc->bytes_per_sector    = vi.SectorSize;
   return 0;
}

int __stdcall unlink(const char *path) {
   qserr err = io_remove(path);
   if (err) set_errno_qserr(err);
   return err?-1:0;
}

int __stdcall START_EXPORT(rename)(const char *oldname, const char *newname) {
   qserr err = io_move(oldname, newname);
   if (err) set_errno_qserr(err);
   return err?-1:0;
}

void __stdcall _dos_getdrive(unsigned *drive) {
   if (drive) *drive = io_curdisk()+1;
}

void __stdcall _dos_setdrive(unsigned drive, unsigned *total) {
   if (drive) io_setdisk(drive-1);
   if (total) {
      u8t lmount = 1;
      io_ismounted(1, &lmount);
      *total = lmount+1;
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
   while ((path[0]=='/'||path[0]=='\\') && (path[1]=='/'||path[1]=='\\'))
      path++;
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
   qserr err;
   char  *rc = io_fullpath_int(buffer, path, size, &err);
   //log_it(2, "_fullpath(%08X,%s,%u) -> %08X (%s) (err:%08X)\n", buffer, path, size, rc, rc, err);
   if (err) set_errno_qserr(err);
   return err?0:rc;
}

#define allmask(mask) (!mask||*mask==0||strcmp(mask,"*")==0||strcmp(mask,"*.*")==0)

static long readtree(const char *Dir, const char *Mask, dir_t **info,
   long owner, long pool, int subdirs, _dos_readtree_cb callback, void *cbinfo)
{
   long   count, all=allmask(Mask), rc=0, ii;
   dir_t  *iout, *rdh, *rdi;
   qserr   errv;
   // it is a bad idea to alloc 300 bytes in stack recursively ;)
   char     *nm = (char*)malloc(QS_MAXPATH+1),
            *np = all?0:strupr(strdup(Mask)), *dptr;
   /* log_printf("readtree(%s,%s,%08X,%08X,%08X,%i,%08X,%08X)\n",Dir,Mask,info,
       owner,pool,subdirs,callback,cbinfo); */
   if (!owner) mem_uniqueid(&owner, &pool);
   // convert name and save it for dir_t.d_openpath
   dptr  = (char*)mem_alloc(owner, pool, QS_MAXPATH+1);
   errv  = io_fullpath(dptr, Dir, QS_MAXPATH+1);
   if (!errv && strclast(dptr)!='\\') strcat(dptr, "\\");
   *info = 0;
   iout  = 0;
   ii    = 0;
   count = 0;
   rdh   = !errv?opendir(dptr):0;
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
                  /* alloc memory (note, that with initial ii=128 it eats all
                     3Gb ram on partition with >10000 directories ;) */
                  if (count+2>=ii) {
                     if (!ii) iout = (dir_t*)mem_alloc(owner,pool,(ii=8)*sizeof(dir_t));
                        else iout = (dir_t*)mem_realloc(iout,(ii*=2)*sizeof(dir_t));
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
            nestcnt = readtree(nm,Mask,&iout2,owner,pool,1,callback,cbinfo);
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
      // reset it to not confuse by previous error & zero count
      set_errno(0);
      rc = dir?readtree(dir,mask,&iout,0,0,subdirs,callback,cbinfo):0;
      if (!info || rc<=0) {
         _dos_freetree(iout);
         iout = 0;
      }
      if (info) *info = iout;
      return rc<0?0:rc;
   }
}

qserr __stdcall START_EXPORT(_dos_freetree)(dir_t *info) {
   long  Owner,Pool;
   if (!info) return E_SYS_INVPARM;
   // memmgr block?
   if (!mem_getobjinfo(info,&Owner,&Pool)) return E_SYS_INVPARM;
   // check memGetUniqueID`s owner to prevent free() opendir`s dir_t
   if (Owner==-2) mem_freepool(Owner,Pool); else return E_SYS_INVPARM;
   return 0;
}

int __stdcall START_EXPORT(_access)(const char *path, int mode) {
   io_handle_info hi;
   qserr         err = io_pathinfo(path, &hi);
   if (!err)
      if ((mode&W_OK) && (hi.attrs&IOFA_READONLY) ||
         (hi.attrs&IOFA_DIR) && (mode&(W_OK|R_OK|X_OK))) err = E_SYS_ACCESS;
   if (err) set_errno_qserr(err);
   return err?-1:0;
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

qserr __stdcall START_EXPORT(_dos_setfileattr)(const char *path, unsigned attributes) {
   if (!path||(attributes&(_A_VOLID|_A_SUBDIR))!=0)  { 
      set_errno(EINVAL);
      return E_SYS_INVPARM;
   } else {
      io_handle_info  hi;
      qserr          err;
      hi.attrs = attributes;
      err      = io_setinfo(path, &hi, IOSI_ATTRS);

      if (err) set_errno_qserr(err);
      return err;
   }
}

qserr __stdcall START_EXPORT(_dos_getfileattr)(const char *path, unsigned *attributes) {
   io_handle_info  hi;
   qserr          err;
   if (attributes) *attributes = 0;
   if (!attributes||!path) { set_errno(EINVAL); return E_SYS_INVPARM; }

   err = io_pathinfo(path, &hi);
   if (err) set_errno_qserr(err); else *attributes = hi.attrs;
   return err;
}

qserr __stdcall START_EXPORT(_dos_setfiletime)(const char *path, u32t dostime, int type) {
   io_handle_info  hi;
   u32t           flv = 0;
   time_t         tmv;
   qserr          err;
   if (!path||!type||(type&~(_DT_MODIFY|_DT_CREATE)))
      { set_errno(EINVAL); return E_SYS_INVPARM; }

   tmv = dostimetotime(dostime);
   if (type&_DT_MODIFY) { io_timetoio (&hi.wtime, tmv); flv|=IOSI_WTIME; }
   if (type&_DT_CREATE) { io_timetoio (&hi.ctime, tmv); flv|=IOSI_CTIME; }
   err = io_setinfo(path, &hi, flv);

   if (err) set_errno_qserr(err);
   return err;
}

qserr __stdcall START_EXPORT(_dos_getfiletime)(const char *path, u32t *dostime, int type) {
   io_handle_info  hi;
   qserr          err;
   if (!path||!dostime||!type||(type&~(_DT_MODIFY|_DT_CREATE)))
      { set_errno(EINVAL); return E_SYS_INVPARM; }

   err = io_pathinfo(path, &hi);
   if (err) set_errno_qserr(err); else {
      time_t tmv = io_iototime(type&_DT_CREATE?&hi.ctime:&hi.wtime);
      *dostime   = timetodostime(tmv);
   }
   return err;
}
