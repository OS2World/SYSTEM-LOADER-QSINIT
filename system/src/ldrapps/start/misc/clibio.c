//
// QSINIT "start" module
// i/o C library functions
//
#include "stdlib.h"
#include "stdarg.h"
#include "fat/ff.h"
#include "errno.h"
#include "qsutil.h"
#include "qsint.h"
#include "qcl/qslist.h"
#include "internal.h"
#include "limits.h"
#include "qsmodext.h"
#include "vioext.h"

#define FILE_SIGN     (0x454C4946)
#define FP_IOBUF_LEN   64
FILE    *stdin = 0, *stdout = 0, *stderr = 0, *stdaux = 0, **pstdout = 0, **pstdin = 0;

typedef struct {
   u32t     sign;
   int   lasterr;
   u32t      pid;              ///< file opened by this process
   int       fno;              ///< fileno, used to check std handles only
   char     *inp;              ///< stdin input buffer
   FIL        fi;              ///< file struct
   char     path[_MAX_PATH+1]; ///< file path
} FileInfo;

static ptr_list opened_files = 0;
static int        fileno_idx = STDAUX_FILENO+1;

int ffErrToErrno[FR_INVALID_PARAMETER+1]={ EZERO, EIO,
    ENFILE, EBUSY, ENOENT, ENOENT, ENAMETOOLONG, EACCES, EACCES,
    EACCES, EROFS, ENXIO, ENOMNT, ENOMNT, EINVAL, EAGAIN, EBUSY,
    ENAMETOOLONG, EMFILE, EINVAL};

static void set_errno1(int x, FileInfo *fs) {
   set_errno(fs->lasterr=(x)>=0&&(x)<=FR_INVALID_PARAMETER?ffErrToErrno[x]:EIO);
}

void set_errno2(int x) {
   set_errno((x)>=0&&(x)<=FR_INVALID_PARAMETER?ffErrToErrno[x]:EIO);
}

FILE* get_stdout(void) { return *pstdout; }
FILE* get_stdin (void) { return *pstdin;  }

static int check_pid(FileInfo *fs) {
   u32t pid;
   if (!(pid=mod_getpid())) { set_errno(EFAULT); return 0; } else
      if (fs->pid && fs->pid!=pid) { set_errno(fs->lasterr=EPERM); return 0; }
   return 1;
}

#define checkret_err(x)          \
   FileInfo *ff=(FileInfo*)fp;   \
   if (!ff||ff->sign!=FILE_SIGN) { set_errno(EINVAL); return x; }
#define checkret_void()          \
   FileInfo *ff=(FileInfo*)fp;   \
   if (!ff||ff->sign!=FILE_SIGN) return;

// warning! function can add one symbol to str (1:->1:\)
void pathcvt(const char *src,char *dst) {
   size_t len;
   strcpy(dst,src);
   if (dst[1]==':') {
      char dltr=toupper(dst[0]);
      if (dltr>='A'&&dltr<='Z') dst[0]=dltr-'A'+'0';
      if (dst[2]==0) { dst[2]='\\'; dst[3]=0; }
   }
   /* remove trailing \\ & / if this is directory and longer than "1:\"
      this must help to all dir open/create/remove functions */
   len = strlen(dst);
   if ((dst[1]!=':'||len>3)&&(dst[len-1]=='\\'||dst[len-1]=='/')) dst[len-1]=0;
}

static void add_new_file(FileInfo *ff) {
   // update global file list, this list will be used until reboot
   if (!opened_files) opened_files = NEW(ptr_list);
   if (opened_files) opened_files->add(ff);
}

FILE* __stdcall fdopen(int handle, const char *mode) {
   FileInfo *fout;
   u32t       pid;
   if (handle<0 || handle>STDAUX_FILENO) { set_errno(EBADF); return 0; }
   pid = mod_getpid();
   if (!pid) { set_errno(EFAULT); return 0; }
   fout = (FileInfo*)malloc(sizeof(FileInfo));
   memZero(fout);

   fout->sign    = FILE_SIGN;
   fout->lasterr = 0;
   fout->pid     = pid;
   fout->fno     = handle;
   fout->inp     = 0;
   fout->path[0] = 0;
   // update global file list
   add_new_file(fout);
   return (FILE*)fout;
}

// this function called from module start callback
void init_stdio(process_context *pq) {
   pq->rtbuf[RTBUF_STDIN]  = (u32t)fdopen(STDIN_FILENO , "r");
   pq->rtbuf[RTBUF_STDOUT] = (u32t)fdopen(STDOUT_FILENO, "w");
   pq->rtbuf[RTBUF_STDERR] = (u32t)fdopen(STDERR_FILENO, "w");
   pq->rtbuf[RTBUF_STDAUX] = (u32t)fdopen(STDAUX_FILENO, "w");
}

void setup_fileio(void) {
   process_context* pq = mod_context();
   // init START process handles
   if (pq) init_stdio(pq); else
      log_printf("warning! zero process context!\n");
}

FILE* __stdcall START_EXPORT(fopen)(const char *filename, const char *mode) {
   char namebuf[_MAX_PATH];
   char      mc=toupper(*mode++);
   int      upd=*mode=='+'?*mode++:0;
   char    type=*mode?toupper(*mode++):'B';
   FileInfo *ff=0;
   u32t   flags=0,
            pid=mod_getpid();
   FRESULT   rc=FR_OK;

   if (!pid) { set_errno(EFAULT); return 0; }
   if (mc!='R'&&mc!='W'&&mc!='A'||type!='B') { set_errno(EINVAL); return 0; }
   ff = (FileInfo*)malloc(sizeof(FileInfo));
   memZero(ff);
   switch (mc) {
      case 'A': flags|=FA_OPEN_ALWAYS|FA_WRITE|(upd?FA_READ:0);   break;
      case 'R': flags|=FA_OPEN_EXISTING|FA_READ|(upd?FA_WRITE:0); break;
      case 'W': flags|=FA_CREATE_ALWAYS|FA_WRITE|(upd?FA_READ:0); break;
   }
   // recode A:-Z: path to 0:-3:
   pathcvt(filename, namebuf);
   // open file
   rc=f_open(&ff->fi, namebuf, flags);
   set_errno1(rc,ff);
   if (rc!=FR_OK) {
      //log_it(3,"fopen(%s,%s), rc=%d\n",namebuf,mode,rc);
      free(ff);
      return 0;
   }
   if (mc=='A'&&ff->fi.fsize) f_lseek(&ff->fi, ff->fi.fsize);
   ff->sign = FILE_SIGN;
   ff->pid  = pid;
   ff->inp  = 0;
   ff->fno  = fileno_idx++;
   // save full path for stat/dump funcs
   _fullpath(ff->path, namebuf, _MAX_PATH+1);
   // this list will be used until reboot
   add_new_file(ff);
   return (FILE*)ff;
}

FILE* __stdcall freopen(const char *filename, const char *mode, FILE *fp) {
   FILE  *nfp;
   checkret_err(0);
   if (!check_pid(ff)) return 0;
   // opens file
   nfp = fopen(filename, mode);
   if (!nfp) return 0;
   // exchange file infos
   memxchg(nfp, fp, sizeof(FileInfo));
   // and close old handle
   fclose(nfp);
   return fp;
}


int __stdcall START_EXPORT(fclose)(FILE *fp) {
   FRESULT   rc;
   checkret_err(1);
   if (!check_pid(ff)) return 1;
   // std i/o or normal file?
   if (ff->fno>=0 && ff->fno<=STDAUX_FILENO) {
      rc = 0;
   } else {
      rc = f_close(&ff->fi);
   }
   set_errno1(rc,ff);

   ff->sign = 0;
   if (ff->inp) { free(ff->inp); ff->inp = 0; }

   if (opened_files) {
      long idx = opened_files->indexof(fp,0);
      if (idx<0) log_printf("warning! closing unknown file %08X (%d)\n",fp,rc);
         else opened_files->del(idx,1);
   }
   if (ff->pid) {
      u32t cpid = mod_getpid();
      if (cpid!=ff->pid) log_printf("pid %d closed by pid %d\n",ff->pid,cpid);
   }
   free(fp);
   return rc!=FR_OK?1:0;
}

size_t __stdcall START_EXPORT(fread)(void *buf, size_t elsize, size_t nelem, FILE *fp) {
   FRESULT   rc;
   UINT  readed;
   checkret_err(0);
   if (!check_pid(ff)) return 0;

   if (elsize==0) { set_errno(EINVAL); return 0; }
   if (elsize*nelem==0) { set_errno(EZERO); return 0; }
   readed = 0;
   if (ff->fno>=0 && ff->fno<=STDAUX_FILENO) {
      if (elsize!=1) { set_errno(EINVAL); return 0; } else
      if (ff->fno!=STDIN_FILENO) { set_errno(EACCES); return 0; } else {
         // ask console
         if (!ff->inp) ff->inp = key_getstr(0);
         // process buffered read
         if (ff->inp) {
            u32t len = strlen(ff->inp);
            if (len<=nelem) {
               memcpy(buf, ff->inp, len);
               free(ff->inp);
               ff->inp = 0;
               readed  = len;
            } else {
               memcpy(buf, ff->inp, nelem);
               memmove(ff->inp, ff->inp+nelem, len-nelem+1);
               readed  = nelem;
            }
         }
      }
   } else {
      rc = f_read(&ff->fi,buf,elsize*nelem,&readed);
      if (elsize>1) readed/=elsize;
      set_errno1(rc,ff);
   }
   return readed;
}

size_t __stdcall START_EXPORT(fwrite)(const void *buf, size_t elsize, size_t nelem, FILE *fp) {
   FRESULT   rc;
   UINT   saved;
   int     stdh;
   checkret_err(0);
   stdh = ff->fno>=0 && ff->fno<=STDAUX_FILENO;
   // do not check pid on every printf char
   if (!stdh && !check_pid(ff)) return 0;

   if (elsize==0) { set_errno(EINVAL); return 0; }
   if (elsize*nelem==0) { set_errno(EZERO); return 0; }
   saved = 0;

   if (stdh) {
      if (ff->fno==STDIN_FILENO) { set_errno(EACCES); return 0; } else {
         char *cpb = (char*)buf;
         u32t  len = elsize * nelem;

         while (len>0) {
            u32t cpy;
            /* line-based printing to log and binary - to console...
               This is because log set an individual time mark for every
               log_push() message */
            if (ff->fno==STDAUX_FILENO) {
               char *eolp = strchr(cpb, '\n');
               u32t    ii;
               if (eolp) cpy = eolp-cpb+1; else cpy = len;

               // print to log and debug COM port separately
               log_pushb(LOG_MISC, cpb, cpy, 0);
               for (ii=0; ii<cpy; ii++) {
                  char ch = cpb[ii];
                  if (ch=='\n' && ii && cpb[ii-1]!='\r') hlp_seroutstr("\n");
                     else hlp_seroutchar(ch);
               }
            } else {
               char  pbuf[FP_IOBUF_LEN+1];
               cpy = len>FP_IOBUF_LEN?FP_IOBUF_LEN:len;
               memcpy(pbuf, cpb, cpy);
               pbuf[cpy] = 0;
               //vio_strout(pbuf);
               ansi_strout(pbuf);
            }
            len   -= cpy;
            cpb   += cpy;
            saved += cpy;
         }
      }
   } else {
      rc = f_write(&ff->fi,buf,elsize*nelem,&saved);
      if (elsize>1) saved/=elsize;
      set_errno1(rc,ff);
   }
   return saved;
}

int __stdcall ferror(FILE *fp) {
   checkret_err(1);
   //if (!check_pid(ff)) return 1;
   return ff->lasterr!=EZERO?1:0;
}

void __stdcall clearerr(FILE *fp) {
   checkret_void();
   ff->lasterr=0;
}

// do not emulate eof flag
int __stdcall feof(FILE *fp) {
   checkret_err(1);
   if (!check_pid(ff)) return 1;
   return 0;//ff->fi.fptr==ff->fi.fsize?1:0;
}

void __stdcall START_EXPORT(rewind)(FILE *fp) {
   checkret_void();
   if (!check_pid(ff)) return;
   // must be a file
   if (ff->fno>=0 && ff->fno<=STDAUX_FILENO) {
      set_errno(ff->lasterr=ENOTBLK);
   } else {
      f_lseek(&ff->fi, 0);
      set_errno1(0,ff);
   }
}

int __stdcall START_EXPORT(fseek)(FILE *fp, s32t offset, int where) {
   checkret_err(1);
   if (!check_pid(ff)) return 1;

   // must be a file
   if (ff->fno>=0 && ff->fno<=STDAUX_FILENO) {
      set_errno(ff->lasterr=ENOTBLK);
      return 1;
   } else {
      FRESULT  rc;
      u32t   cpos = ff->fi.fptr;
      switch (where) {
         case SEEK_CUR:if (offset<0&&-offset>cpos) cpos=0;
            else cpos+=offset;
            break;
         case SEEK_END:if (offset<0&&-offset>ff->fi.fsize) cpos=0;
            else cpos=ff->fi.fsize+offset;
            break;
         case SEEK_SET:if (offset>=0) { cpos=offset; break; }
         default:
            set_errno(EINVAL);
            return 1;
      }
      rc = f_lseek(&ff->fi, cpos);
      set_errno1(rc,ff);
      return rc!=FR_OK?1:0;
   }
}

int  __stdcall fflush(FILE *fp) {
   checkret_err(1);
   if (!check_pid(ff)) return 1;

   // ignore console handles
   if (ff->fno>=0 && ff->fno<=STDAUX_FILENO) {
      return 0;
   } else {
      FRESULT rc = f_sync(&ff->fi);
      set_errno1(rc,ff);
      return rc!=FR_OK?1:0;
   }
}

s32t __stdcall ftell(FILE *fp) {
   checkret_err(-1);
   if (!check_pid(ff)) return -1;
   if (ff->fno>=0 && ff->fno<=STDAUX_FILENO) return -1;
   return ff->fi.fptr;
}

long __stdcall filelength(int fp) {
   checkret_err(-1);
   if (!check_pid(ff)) return -1;
   if (ff->fno>=0 && ff->fno<=STDAUX_FILENO) return -1;
   return ff->fi.fsize;
}

int  __stdcall isatty(int fp) {
   checkret_err(0);
   if (ff->fno>=0 && ff->fno<=STDAUX_FILENO) return 1;
   return 0;
}

int  __stdcall fdetach(FILE *fp) {
   checkret_err(1);
   if (!check_pid(ff)) return 1;
   /* do not allow to detach std i/o handles, so all of it will be catched by
      fcloseall() */
   if (ff->fno>=0 && ff->fno<=STDAUX_FILENO) {
      set_errno(ff->lasterr=EBADF);
      return 1;
   }
   ff->pid=0;
   set_errno1(0,ff);
   return 0;
}

int  __stdcall fcloseall(void) {
   if (opened_files) {
      u32t ii=0, rc=0, pid=mod_getpid();
      if (!pid) { set_errno(EFAULT); return EOF; }

      while (ii<opened_files->count()) {
         FileInfo *fp=(FileInfo*)opened_files->value(ii);
         if (pid==fp->pid)
            if (fclose((FILE*)fp)) return EOF; else { rc++; continue; }
         ii++;
      }
      return rc;
   }
   return 0;
}

void _std log_ftdump(void) {
   log_it(2,"== Active handles ==\n");
   if (opened_files) {
      u32t ii=0;
      if (opened_files->count())
         log_it(2, "     file name          |   fno   | pid |    size   |    pos    | err\n");
      for (ii=0; ii<opened_files->count(); ii++) {
         char buf[128];
         FileInfo *fp = (FileInfo*)opened_files->value(ii);
         int      pos = sprintf(buf, "%3d. %-18s |%8i |", ii+1, fp->path, fp->fno);

         if (fp->pid) pos+=sprintf(buf+pos, "%4u ", fp->pid); else
            pos+=sprintf(buf+pos, " --- ");

         if (fp->fi.fs)
            pos+=sprintf(buf+pos, "|%10u |%10u |%3d", fp->fi.fsize, fp->fi.fptr, fp->fi.err);
         else
            pos+=sprintf(buf+pos, "|           |           |");
         buf[pos++] = '\n';
         buf[pos] = 0;
         log_it(2, buf);
      }
   }
}

int __stdcall START_EXPORT(_chsize)(int fp, u32t size) {
   FRESULT  rc;
   u32t   fptr;
   checkret_err(-1);
   if (!check_pid(ff)) return -1;
   // check file type
   if (ff->fno>=0 && ff->fno<=STDAUX_FILENO) {
      set_errno(ff->lasterr=ENOTBLK);
      return -1;
   } else
   if (size==ff->fi.fsize) { set_errno(ff->lasterr=EZERO); return 0; }
   // save file pos
   fptr=ff->fi.fptr;
   rc  =f_lseek(&ff->fi,size);
   if (rc==FR_OK && size<ff->fi.fsize) rc=f_truncate(&ff->fi);
   set_errno1(rc,ff);
   // and restore it
   f_lseek(&ff->fi,fptr);
   if (ff->lasterr==EIO) set_errno(ff->lasterr=ENOSPC); else
   if (ff->lasterr==EINVAL) set_errno(ff->lasterr=EBADF);
   return rc!=FR_OK?-1:0;
}

// open, read, close file & return buffer with it
void* __stdcall freadfull(const char *name, unsigned long *bufsize) {
   char namebuf[_MAX_PATH];
   void    *res=0;
   FIL       fi;
   FRESULT   rc=FR_OK;

   if (!name||!bufsize) return 0;
   *bufsize=0;
   // recode A:-Z: path to 0:-3:
   pathcvt(name,namebuf);

   rc=f_open(&fi,namebuf,FA_OPEN_EXISTING|FA_READ);
   set_errno2(rc);
   if (rc!=FR_OK) return 0;

   if (fi.fsize) {
      UINT  readed;
      res = hlp_memallocsig(fi.fsize,"file",QSMA_RETERR);
      if (!res) {
         f_close(&fi);
         set_errno(ENOMEM);
         return 0;
      }
      rc=f_read(&fi,res,fi.fsize,&readed);
      set_errno2(rc);
      *bufsize=readed;
   }
   rc=f_close(&fi);
   set_errno2(rc);
   return res;
}

typedef struct {
   FILE       *fp;
   int        len;
   int       bpos;
   char       buf[FP_IOBUF_LEN];
   int      flush;
   int      errno;
} fprninfo;

static void _std fileprn(int ch, void *stream) {
   fprninfo *fi = stream;
   if (ch) fi->buf[fi->bpos++] = ch;
   // flush buffer if it full or char was a \r or \n
   if (fi->bpos && (fi->flush || fi->bpos==FP_IOBUF_LEN) || ch=='\n' || ch=='\r') {
      if (fwrite(fi->buf, 1, fi->bpos, fi->fp)!=fi->bpos)
         fi->errno = get_errno();
      else
         fi->len  += fi->bpos;
      fi->bpos = 0;
   }
}

int __cdecl START_EXPORT(vfprintf)(FILE *fp, const char *fmt, long *args) {
   fprninfo  fpi;
   fpi.fp    = fp;
   fpi.len   = 0;
   fpi.bpos  = 0;
   fpi.flush = 0;
   fpi.errno = 0;
   _prt_common(&fpi, fmt, args, fileprn);
   if (!fpi.errno && fpi.bpos) {
     fpi.flush = 1;
     fileprn(0, &fpi);
   }
   return fpi.errno?-1:fpi.len;
}

int __cdecl START_EXPORT(fprintf)(FILE *fp, const char *fmt, long args) {
   return START_EXPORT(vfprintf)(fp, fmt, &args);
}

static void _std vioprn(int ch, void *stream) {
   vio_charout(ch);
}

int __cdecl START_EXPORT(printf)(const char *fmt, long args) {
   if (pstdout)
      return START_EXPORT(vfprintf)(*pstdout, fmt, &args);
   else
      return _prt_common(0, fmt, &args, vioprn);
}

typedef struct {
   char       *rc;
   u32t       len;
   u32t       pos;
} sprninfo;

static void _std sprndyn(int ch, void *stream) {
   sprninfo *di = stream;
   if (di->pos+1 == di->len) {
      char *np = (char*)realloc(di->rc, di->len*2);
      if (!np) { di->rc[di->pos]=0; return; }
      di->rc  = np;
      di->len*= 2;
   }
   di->rc[di->pos++] = ch;
}

/// sprintf to dynamically allocated buffer
char* __cdecl START_EXPORT(sprintf_dyn)(const char *fmt, long args) {
   sprninfo  di;
   di.rc  = (char*)malloc(16);
   di.len = 16;
   di.pos = 0;
   _prt_common(&di, fmt, &args, sprndyn);
   // put zero
   sprndyn(0, &di);
   return di.rc;
}

int _stdcall START_EXPORT(vprintf)(const char *fmt, long *argp) {
   if (pstdout)
      return START_EXPORT(vfprintf)(*pstdout, fmt, argp);
   else
      return _prt_common(0, fmt, argp, vioprn);
}

int __stdcall fputs(const char *buf, FILE *fp) {
   if (!buf || !*buf) return 0; else {
      u32t   len = strlen(buf);
      size_t wrc = fwrite(buf, 1, len, fp);
      return !wrc?EOF:len;
   }
}

int __stdcall puts(const char *buf) {
   if (pstdout) {
      int rc = fputs(buf, *pstdout);
      if (rc==EOF) return EOF; else
         if (fputc('\n', *pstdout)==EOF) return EOF;
      return rc+1;
   } else {
      int len = strlen(buf)+1;
      vio_strout(buf);
      vio_charout('\n');
      return len;
   }
}

int __stdcall fputc(int c, FILE *fp) {
   return fwrite(&c,1,1,fp)?c:EOF;
}

int getc(FILE *fp) {
   checkret_err(EOF);

   if (ff->fno==STDIN_FILENO) return key_read(); else {
      char rc;
      if (fread(&rc, 1, 1, fp)) return rc;
   }
   return EOF;
}

int getchar(void) {
   process_context* pq = mod_context();
   if (pq) return getc((FILE*)pq->rtbuf[RTBUF_STDIN]);
   return EOF;
}

static int gettempname(char *prefix, char *dir, char *buffer) {
   process_context *pq = mod_context();
   u32t   ii,idx;
   char    *name;
   if (!pq) { set_errno(EFAULT); return 0; }

   if (dir) {
      strcpy(buffer,dir);
      name = buffer + strlen(buffer);
      if (name[-1]!='\\' && name[-1]!='/') *name++ = '\\';
   } else {
      *buffer = 0;
      name    = buffer;
   }
   // check final name length
   ii = name - buffer;
   if (prefix) ii+=strlen(prefix);
   if (ii+6>=NAME_MAX) { set_errno(ENAMETOOLONG); return 0; }

   if (prefix) {
      strcpy(name, prefix);
      name += strlen(name);
   }

   do {
      char *np = name;
      idx = pq->rtbuf[RTBUF_TMPNAME]++;
      ii  = TMP_MAX/36;
      while (ii) {
         u32t cc = idx/ii;
         idx   = idx%ii;
         *np++ = cc>9 ? 'A'+ (cc-10) : '0'+ cc;
         ii   /= 36;
      }
      *np = 0;
      if (_access(name,F_OK)) break;
   } while (idx<TMP_MAX);

   if (idx==TMP_MAX) { set_errno(EEXIST); return 0; }
   return 1;
}

char* __stdcall _tempnam(char *dir, char *prefix) {
   char tdir[NAME_MAX+1],
         *rc = (char*)malloc(NAME_MAX+1);
   int found = 0;

   if (dir && strlen(dir)>=NAME_MAX) { set_errno(ENAMETOOLONG); return 0; }

   found = tmpdir(tdir)!=0;
   if (!found) { // no TMP dir, set it temporary and check for existence
      setenv("TEMPDIR", dir, 0);
      found = tmpdir(tdir)!=0;
      setenv("TEMPDIR", 0, 1);
   }
   found = gettempname(prefix, found?tdir:0, rc);
   if (!found) { free(rc); return 0; }
   return rc;
}

char* __stdcall tmpnam(char *buffer) {
   static char name[NAME_MAX+1];

   if (!gettempname("TMP",0,buffer?buffer:name)) return 0;
   if (buffer) strcpy(buffer, name);

   return buffer?buffer:name;
}

u32t __stdcall hlp_isdir(const char *dir) {
   dir_t fd;
   if (!_dos_stat(dir,&fd))
      if (fd.d_attr&_A_SUBDIR) return 1;
   return 0;
}
