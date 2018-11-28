//
// QSINIT "start" module
// i/o C library functions
//
#include "stdlib.h"
#include "stdarg.h"
#include "errno.h"
#include "qsutil.h"
#include "qsint.h"
#include "qcl/qslist.h"
#include "syslocal.h"
#include "limits.h"
#include "qsmodext.h"
#include "vioext.h"

#define FILE_SIGN     (0x454C4946)
#define ALIAS_SIGN    (0x4C414C46)
#define FP_IOBUF_LEN   64

typedef struct {
   u32t        sign;
   int      lasterr;
   u32t         pid;              ///< file opened by this process
   int          fno;              ///< fileno, used to check std handles only
   char        *inp;              ///< stdin input buffer
   char         uch;              ///< ungetc character
   io_handle     fi;              ///< file handle
   char        path[_MAX_PATH+1]; ///< file path
} FileInfo;

typedef struct {
   u32t        sign;              ///< sign to check
   u32t       rtidx;              ///< index of file handle in pq->rtbuf
} AliasInfo;

static AliasInfo h1_route = {ALIAS_SIGN, RTBUF_STDIN},
                 h2_route = {ALIAS_SIGN, RTBUF_STDOUT},
                 h3_route = {ALIAS_SIGN, RTBUF_STDERR},
                 h4_route = {ALIAS_SIGN, RTBUF_STDAUX},
                 h5_route = {ALIAS_SIGN, RTBUF_STDNUL};

FILE *stdin = 0, *stdout = 0, *stderr = 0, *stdaux = 0, *stdnul = 0;

static ptr_list opened_files = 0;
static int        fileno_idx = STDNUL_FILENO+1;

static void set_errno1(qserr err, FileInfo *fs) {
   fs->lasterr = set_errno_qserr(err);
}

static void set_errno2(FileInfo *fs) {
   fs->lasterr = set_errno_qserr(io_lasterror(fs->fi));
}

FILE* get_stdout(void) { return stdout; }
FILE* get_stdin (void) { return stdin;  }

static int check_pid(FileInfo *fs) {
   if (fs->pid) {
      u32t pid;
      if (!(pid=mod_getpid())) { set_errno(EFAULT); return 0; } else
      if (fs->pid!=pid) { set_errno(fs->lasterr=EPERM); return 0; }
   }
   return 1;
}

#define checkret_err(x)          \
   FileInfo *ff = (FileInfo*)fp; \
   if (ff&&ff->sign==ALIAS_SIGN) \
      ff = (FileInfo*)mod_context()->rtbuf[((AliasInfo*)ff)->rtidx]; \
   if (!ff||ff->sign!=FILE_SIGN) { set_errno(EINVAL); return x; }

#define checkret_void()          \
   FileInfo *ff = (FileInfo*)fp; \
   if (ff&&ff->sign==ALIAS_SIGN) \
      ff = (FileInfo*)mod_context()->rtbuf[((AliasInfo*)ff)->rtidx]; \
   if (!ff||ff->sign!=FILE_SIGN) return;

static void add_new_file(FileInfo *ff) {
   // update global file list, this list will be used until reboot
   mt_swlock();
   if (!opened_files) opened_files = NEW_G(ptr_list);
   if (opened_files) opened_files->add(ff);
   mt_swunlock();
}

static FILE* __stdcall fdopen_as(int handle, const char *mode, u32t pid) {
   FileInfo *fout;
   if (handle<0 || handle>STDNUL_FILENO) { set_errno(EBADF); return 0; }
   fout = (FileInfo*)malloc(sizeof(FileInfo));
   mem_zero(fout);

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

FILE* __stdcall fdopen(int handle, const char *mode) {
   u32t pid = mod_getpid();
   return fdopen_as(handle, mode, pid);
}

// this function called from module start callback
void init_stdio(process_context *pq) {
   pq->rtbuf[RTBUF_STDIN]  = (u32t)fdopen_as(STDIN_FILENO , "r", pq->pid);
   pq->rtbuf[RTBUF_STDOUT] = (u32t)fdopen_as(STDOUT_FILENO, "w", pq->pid);
   pq->rtbuf[RTBUF_STDERR] = (u32t)fdopen_as(STDERR_FILENO, "w", pq->pid);
   pq->rtbuf[RTBUF_STDAUX] = (u32t)fdopen_as(STDAUX_FILENO, "w", pq->pid);
   pq->rtbuf[RTBUF_STDNUL] = (u32t)fdopen_as(STDNUL_FILENO, "w+", pq->pid);
   /* nobody should change those values, but just overwrite it with constant
      on every module start */
   stdin  = (FILE*)&h1_route;
   stdout = (FILE*)&h2_route;
   stderr = (FILE*)&h3_route;
   stdaux = (FILE*)&h4_route;
   stdnul = (FILE*)&h5_route;
}

void setup_fileio(void) {
   // init START process handles
   init_stdio(mod_context());
}

FILE* __stdcall _fsopen(const char *filename, const char *mode, int share) {
   char namebuf[_MAX_PATH];
   char      mc = toupper(*mode++);
   int      upd = *mode=='+'?*mode++:0;
   char    type = *mode?toupper(*mode++):'B';
   FileInfo *ff = 0;
   u32t   flags = 0,
            pid = mod_getpid();
   qserr     rc = 0;

   if (!pid) { set_errno(EFAULT); return 0; }
   // check args
   if (mc!='R' && mc!='W' && mc!='A' || type!='B' || share!=SH_COMPAT &&
      share!=SH_DENYRW && share!=SH_DENYWR && share!=SH_DENYRD &&
         share!=SH_DENYNO) { set_errno(EINVAL); return 0; }
   // select share
   switch (share) {
      case SH_COMPAT: // this one works not like in DOS
         flags = IOFM_SHARE_READ|(mc=='R'&&!upd?IOFM_SHARE_WRITE:0)|IOFM_SHARE_DEL;
         break;
      case SH_DENYRW: // deny read/write mode
         break;
      case SH_DENYWR: // deny write mode
         flags = IOFM_SHARE_READ;
         break;
      case SH_DENYRD: // deny read mode
         flags = IOFM_SHARE_WRITE|IOFM_SHARE_DEL|IOFM_SHARE_REN;
         break;
      case SH_DENYNO:
         flags = IOFM_SHARE_READ|IOFM_SHARE_WRITE|IOFM_SHARE_DEL|IOFM_SHARE_REN;
         break;
   }
   ff = (FileInfo*)calloc(sizeof(FileInfo),1);
   switch (mc) {
      case 'A': flags|=IOFM_OPEN_ALWAYS|IOFM_WRITE|(upd?IOFM_READ:0); break;
      case 'R': flags|=IOFM_OPEN_EXISTING|IOFM_READ|(upd?IOFM_WRITE:0); break;
      case 'W': flags|=IOFM_CREATE_ALWAYS|IOFM_WRITE|(upd?IOFM_READ:0); break;
   }
   // open file
   rc = io_open(filename, flags, &ff->fi, 0);

   if (rc) {
      //log_it(3,"fopen(%s,%s), rc=%d\n",namebuf,mode,rc);
      set_errno_qserr(rc);
      free(ff);
      return 0;
   }

   if (mc=='A')
      if (io_seek(ff->fi, 0, IO_SEEK_END)==FFFF64) {
         set_errno_qserr(io_lasterror(ff->fi));
         io_close(ff->fi);
         free(ff);
         return 0;
      }
   ff->sign = FILE_SIGN;
   ff->pid  = pid;
   ff->inp  = 0;
   ff->fno  = fileno_idx++;
   // save full path for stat/dump funcs
   _fullpath(ff->path, filename, _MAX_PATH+1);
   // this list will be used until reboot
   add_new_file(ff);
   // set process as block owner
   mem_localblock(ff);
   return (FILE*)ff;
}

FILE* __stdcall START_EXPORT(fopen)(const char *filename, const char *mode) {
   return _fsopen(filename, mode, SH_COMPAT);
}


FILE* __stdcall freopen(const char *filename, const char *mode, FILE *fp) {
   FILE  *nfp;
   checkret_err(0);
   if (!check_pid(ff)) return 0;
   // opens file
   nfp = fopen(filename, mode);
   if (!nfp) return 0;
   /* exchange file infos.
      "ff" shuld be here, because of alias headers for std handles */
   memxchg(nfp, ff, sizeof(FileInfo));
   // and close old handle
   fclose(nfp);
   return fp;
}

int __stdcall START_EXPORT(fclose)(FILE *fp) {
   qserr    res;
   checkret_err(1);
   if (!check_pid(ff)) return 1;
   // std i/o or normal file?
   if (ff->fno>=0 && ff->fno<=STDNUL_FILENO) {
      static int rec_mtx[STDNUL_FILENO+1] = {RTBUF_STDIN, RTBUF_STDOUT,
                              RTBUF_STDERR, RTBUF_STDAUX, RTBUF_STDNUL};
      process_context *pq = mod_context();
      int           index = rec_mtx[ff->fno];
      // zero ptr to std handle in process context
      if (pq->rtbuf[index]==(u32t)ff) pq->rtbuf[index] = 0;

      res = 0;
   } else {
      res = io_close(ff->fi);
   }
   if (res) set_errno_qserr(res);

   ff->sign = 0;
   if (ff->inp) { free(ff->inp); ff->inp = 0; }

   mt_swlock();
   if (opened_files) {
      long idx = opened_files->indexof(fp,0);
      if (idx<0) log_printf("warning! unknown file %08X (%d)\n", fp, res);
         else opened_files->del(idx,1);
   }
   mt_swunlock();

   free(fp);
   return res;
}

size_t __stdcall START_EXPORT(fread)(void *buf, size_t elsize, size_t nelem, FILE *fp) {
   u32t  readed,
         toread = elsize*nelem;
   char    *dst = (char*)buf;
   int     stdh;
   checkret_err(0);
   if (!check_pid(ff)) return 0;

   if (elsize==0 || !buf) { set_errno(EINVAL); return 0; }
   if (toread==0) { set_errno(EZERO); return 0; }
   stdh   = ff->fno>=0 && ff->fno<=STDNUL_FILENO;
   readed = 0;
   // reject any std handles before ungetc processing
   if (stdh) {
      if (elsize!=1) { set_errno(EINVAL); return 0; }
      if (ff->fno!=STDIN_FILENO) {
         if (ff->fno!=STDNUL_FILENO) set_errno(EACCES);
         return 0;
      }
   }
   // ungetc processing
   if (ff->uch) {
      *dst++  = ff->uch;
      ff->uch = 0;
      // one char
      if (++readed==toread) return readed;
      toread--;
   }
   if (stdh) {
      // ask console
      if (!ff->inp) ff->inp = key_getstr(0);
      // add EOL
      if (ff->inp) ff->inp = strcat_dyn(ff->inp,"\n");
      // process buffered read
      if (ff->inp) {
         u32t len = strlen(ff->inp);
         if (len<=toread) {
            memcpy(dst, ff->inp, len);
            free(ff->inp);
            ff->inp = 0;
            readed += len;
         } else {
            memcpy(dst, ff->inp, toread);
            memmove(ff->inp, ff->inp+toread, len-toread+1);
            readed += toread;
         }
      }
   } else {
      readed += io_read(ff->fi, dst, toread);
      if (readed!=nelem*elsize) set_errno2(ff);
   }
   return readed;
}

size_t __stdcall START_EXPORT(fwrite)(const void *buf, size_t elsize, size_t nelem, FILE *fp) {
   u32t   saved,
        towrite = elsize*nelem;
   int     stdh;
   checkret_err(0);
   stdh = ff->fno>=0 && ff->fno<=STDNUL_FILENO;
   // do not check pid on every printf char
   if (!stdh && !check_pid(ff)) return 0;

   if (elsize==0) { set_errno(EINVAL); return 0; }
   if (towrite==0) { set_errno(EZERO); return 0; }
   saved = 0;

   if (stdh) {
      if (ff->fno==STDIN_FILENO) { set_errno(EACCES); return 0; } else
      if (ff->fno==STDNUL_FILENO) { ff->uch = 0; return towrite; } else {
         char *cpb = (char*)buf;
         u32t  len = towrite;

         while (len>0) {
            u32t cpy;
            /* line-based printing to log and binary - to console...
               This is because of log set an individual time mark for every
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
      // seek to previous char, but ignore error (this can be pos 0)
      if (ff->uch) io_seek(ff->fi, -1, IO_SEEK_CUR);
      saved = io_write(ff->fi, buf, towrite);
      if (saved!=towrite) set_errno2(ff);
   }
   // drop ungetc
   ff->uch = 0;
   return saved;
}

int __stdcall ferror(FILE *fp) {
   checkret_err(1);
   //if (!check_pid(ff)) return 1;
   return ff->lasterr?1:0;
}

void __stdcall clearerr(FILE *fp) {
   checkret_void();
   ff->lasterr = 0;
}

// do not emulate eof flag
int __stdcall feof(FILE *fp) {
   checkret_err(1);
   if (!check_pid(ff)) return 1;
   if (ff->fno==STDNUL_FILENO) return 1;
   return 0;//ff->fi.fptr==ff->fi.fsize?1:0;
}

void __stdcall START_EXPORT(rewind)(FILE *fp) {
   checkret_void();
   if (!check_pid(ff)) return;

   // must be a file
   if (ff->fno>=0 && ff->fno<=STDNUL_FILENO) {
      set_errno(ff->lasterr=ENOTBLK);
   } else {
      // function clears error, as ANSI says
      if (io_seek(ff->fi,0,IO_SEEK_SET)==FFFF64) set_errno2(ff); else {
         ff->lasterr = 0;
         ff->uch     = 0;
      }
   }
}

int __stdcall START_EXPORT(fseek)(FILE *fp, s32t offset, int where) {
   checkret_err(1);
   if (!check_pid(ff)) return 1;

   // must be a file
   if (ff->fno>=0 && ff->fno<=STDNUL_FILENO) {
      set_errno(ff->lasterr=ENOTBLK);
      return 1;
   } else {
      qserr errv = 0;

      if (io_seek(ff->fi, offset, where)==FFFF64) {
         errv = io_lasterror(ff->fi);
         set_errno1(errv, ff);
      } else
         ff->uch = 0;
      return errv;
   }
}

int __stdcall _chsizei64(int fp, u64t size) {
   checkret_err(-1);
   if (!check_pid(ff)) return -1;
   // must be a file
   if (ff->fno>=0 && ff->fno<=STDNUL_FILENO) {
      set_errno(ff->lasterr=ENOTBLK);
      return -1;
   } else {
      qserr rc = io_setsize(ff->fi, size);
      if (rc) set_errno1(rc, ff);
      return rc?-1:0;
   }
}

s64t __stdcall _lseeki64(int fp, s64t offset, int origin) {
   checkret_err(-1);
   if (!check_pid(ff)) return -1;

   // must be a file
   if (ff->fno>=0 && ff->fno<=STDNUL_FILENO) {
      set_errno(ff->lasterr=ENOTBLK);
      return -1;
   } else {
      qserr  errv = 0;
      u64t  fpnew = io_seek(ff->fi, offset, origin);

      if (fpnew==FFFF64) {
         errv = io_lasterror(ff->fi);
         set_errno1(errv, ff);
      } else
         ff->uch = 0;
      return errv?-1:fpnew;
   }
}

s32t __stdcall lseek(int handle, s32t offset, int origin) {
   s64t npos = _lseeki64(handle, offset, origin);
   if (npos>x7FFF) {
      // handle was checked in _lseeki64
      set_errno(((FileInfo*)handle)->lasterr=ERANGE);
      return -1;
   }
   return npos;
}

int  __stdcall fflush(FILE *fp) {
   checkret_err(1);
   if (!check_pid(ff)) return 1;

   // ignore console handles
   if (ff->fno>=0 && ff->fno<=STDNUL_FILENO) {
      return 0;
   } else {
      qserr res = io_flush(ff->fi);
      if (res) set_errno1(res, ff); else ff->uch = 0;
      return res;
   }
}

s32t __stdcall ftell(FILE *fp) {
   checkret_err(-1);
   if (!check_pid(ff)) return -1;
   if (ff->fno>=0 && ff->fno<=STDNUL_FILENO) {
      set_errno(ff->lasterr=ENOTBLK);
      return -1;
   } else {
      u64t pos = io_pos(ff->fi);

      if (pos && ff->uch) pos--;
      if (pos>x7FFF) set_errno(ff->lasterr=ERANGE);

      return pos>x7FFF?-1:pos;
   }
}

s64t __stdcall _telli64(int fp) {
   checkret_err(-1);
   if (!check_pid(ff)) return -1;
   if (ff->fno>=0 && ff->fno<=STDNUL_FILENO) {
      set_errno(ff->lasterr=ENOTBLK);
      return -1;
   } else {
      u64t pos = io_pos(ff->fi);

      if (pos && ff->uch) pos--;
      if (pos>x7FFF64) {
         set_errno(ff->lasterr=ERANGE);
         return -1;
      }
      return pos;
   }
}

s64t __stdcall _filelengthi64(int fp) {
   checkret_err(-1);
   if (!check_pid(ff)) return -1;
   if (ff->fno>=0 && ff->fno<=STDNUL_FILENO) {
      set_errno(ff->lasterr=ENOTBLK);
      return -1;
   } else {
      u64t  size;
      qserr  res = io_size(ff->fi, &size);
      if (res) set_errno1(res, ff);
      return res?-1:size;
   }
}

qshandle __stdcall _os_handle(int fp) {
   checkret_err(0);
   if (!check_pid(ff)) return 0;
   if (ff->fno>=0 && ff->fno<=STDNUL_FILENO) return 0;
   return ff->fi;
}

long __stdcall filelength(int fp) {
   s64t res = _filelengthi64(fp);
   if (res>x7FFF) {
      set_errno(((FileInfo*)fp)->lasterr=ERANGE);
      return -1;
   }
   return res;
}

int  __stdcall isatty(int fp) {
   checkret_err(0);
   if (ff->fno>=0 && ff->fno<=STDNUL_FILENO) return 1;
   return 0;
}

int  __stdcall fdetach(FILE *fp) {
   checkret_err(1);
   if (!check_pid(ff)) return 1;
   /* do not allow to detach std i/o handles, so all of it will be catched by
      fcloseall() */
   if (ff->fno>=0 && ff->fno<=STDNUL_FILENO) {
      set_errno(ff->lasterr=EBADF);
      return E_SYS_INVHTYPE;
   } else {
      qserr err = io_setstate (ff->fi, IOFS_DETACHED, 1);
      if (!err) {
         ff->pid = 0;
         // share FILE* data!
         mem_shareblock(ff);
      } else set_errno1(err, ff);
      return err;
   }
}

int fcloseall_as(u32t pid) {
   if (opened_files) {
      int  rc = 0;
      u32t ii = 0;

      mt_swlock();
      while (ii<opened_files->count()) {
         FileInfo *fp=(FileInfo*)opened_files->value(ii);
         if (pid==fp->pid) {
            fp->pid = 0;
            if (fclose((FILE*)fp)) { rc=EOF; break; }
               else { rc++; continue; }
         }
         ii++;
      }
      mt_swunlock();
      return rc;
   }
   return 0;
}

int __stdcall fcloseall(void) { return fcloseall_as(mod_getpid()); }

void _std log_ftdump(void) {
   log_it(2,"== clib file handles ==\n");
   if (opened_files) {
      u32t ii=0;

      mt_swlock();
      if (opened_files->count())
         log_it(2, "     file name          |   fno   | pid |  handle  | lasterr\n");
      for (ii=0; ii<opened_files->count(); ii++) {
         char buf[128];
         FileInfo *fp = (FileInfo*)opened_files->value(ii);

         if (fp->fno<=STDNUL_FILENO && fp->path[0]==0) continue; else {
            int   pos = sprintf(buf, "%3d. %-18s |%8i |", ii+1, fp->path, fp->fno);
            
            if (fp->pid) pos+=sprintf(buf+pos, "%4u ", fp->pid); else
               pos+=sprintf(buf+pos, " --- ");
            pos+=sprintf(buf+pos, "| %08X | %d", fp->fi, fp->lasterr);

            buf[pos++] = '\n';
            buf[pos] = 0;
            log_it(2, buf);
         }
      }
      mt_swunlock();
   }
}

int __stdcall START_EXPORT(_chsize)(int fp, u32t size) {
   checkret_err(-1);
   if (!check_pid(ff)) return -1;
   // check file type
   if (ff->fno>=0 && ff->fno<=STDNUL_FILENO) {
      set_errno(ff->lasterr=ENOTBLK);
      return -1;
   } else {
      qserr res = io_setsize(ff->fi, size);
      if (res) set_errno1(res, ff);
      return res?-1:0;
   }
}

// open, read, close file & return buffer with it
void* __stdcall freadfull(const char *name, unsigned long *bufsize) {
   io_handle  fh;
   void     *res=0;
   qserr     err;
   u64t      fsz;

   if (!name||!bufsize) return 0;
   *bufsize = 0;

   err = io_open(name, IOFM_OPEN_EXISTING|IOFM_READ|IOFM_SHARE_READ, &fh, 0);
   if (err) { set_errno_qserr(err); return 0; }
   err = io_size(fh, &fsz);
   if (err || fsz>x7FFF) {
      if (err) set_errno_qserr(err); else set_errno(ENOMEM);
      return 0;
   }
   res = hlp_memallocsig(fsz, "file", QSMA_RETERR|QSMA_LOCAL);
   if (!res) set_errno(ENOMEM); else
      if ((*bufsize = io_read(fh,res,fsz))!=fsz) {
         set_errno_qserr(io_lasterror(fh));
         hlp_memfree(res);
         res = 0;
      }
   err = io_close(fh);
   if (err) set_errno_qserr(err);
   return res;
}

int __stdcall fwritefull(const char *name, void *buf, unsigned long bufsize) {
   qserr  err;
   if (!name || Xor(buf,bufsize)) err = E_SYS_INVPARM; else {
      io_handle fh;
      err = io_open(name, IOFM_CREATE_ALWAYS|IOFM_WRITE|IOFM_SHARE_READ, &fh, 0);
      if (!err && bufsize)
         if (io_write(fh,buf,bufsize)!=bufsize) {
            err = io_lasterror(fh);
            io_close(fh);
         }
      if (!err) err = io_close(fh);
   }
   return err?set_errno_qserr(err):0;
}

typedef struct {
   FILE       *fp;
   int        len;
   int       bpos;
   char       buf[FP_IOBUF_LEN];
   int      flush;
   int       errv;
} fprninfo;

static void _std fileprn(int ch, void *stream) {
   fprninfo *fi = stream;
   if (ch) fi->buf[fi->bpos++] = ch;
   // flush buffer when it full or char was \r or \n
   if (fi->bpos && (fi->flush || fi->bpos==FP_IOBUF_LEN) || ch=='\n' || ch=='\r') {
      if (fwrite(fi->buf, 1, fi->bpos, fi->fp)!=fi->bpos)
         fi->errv = get_errno();
      else
         fi->len += fi->bpos;
      fi->bpos = 0;
   }
}

int __cdecl START_EXPORT(vfprintf)(FILE *fp, const char *fmt, long *args) {
   fprninfo  fpi;
   fpi.fp    = fp;
   fpi.len   = 0;
   fpi.bpos  = 0;
   fpi.flush = 0;
   fpi.errv  = 0;
   _prt_common(&fpi, fmt, args, fileprn);
   if (!fpi.errv && fpi.bpos) {
     fpi.flush = 1;
     fileprn(0, &fpi);
   }
   return fpi.errv?-1:fpi.len;
}

int __cdecl START_EXPORT(fprintf)(FILE *fp, const char *fmt, long args) {
   return START_EXPORT(vfprintf)(fp, fmt, &args);
}

static void _std vioprn(int ch, void *stream) {
   vio_charout(ch);
}

int __cdecl START_EXPORT(printf)(const char *fmt, long args) {
   if (stdout)
      return START_EXPORT(vfprintf)(stdout, fmt, &args);
   else
      return _prt_common(0, fmt, &args, vioprn);
}

int __cdecl START_EXPORT(tprintf)(const char *fmt, long args) {
   FILE *tf = (FILE*)mt_tlsget(QTLS_TPRINTF);
   if (tf) {
      FileInfo *ff = (FileInfo*)tf;
      // check it first!
      if (ff->sign!=ALIAS_SIGN && ff->sign!=FILE_SIGN) {
         set_errno(EBADFD);
         return 0;
      }
   }
   if (tf || stdout)
      return START_EXPORT(vfprintf)(tf?tf:stdout, fmt, &args);
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

/// vsprintf to dynamically allocated buffer
char* __stdcall START_EXPORT(vsprintf_dyn)(const char *fmt, long *argp) {
   sprninfo  di;
   di.rc  = (char*)malloc_local(32);
   di.len = 16;
   di.pos = 0;
   _prt_common(&di, fmt, argp, sprndyn);
   // put zero
   sprndyn(0, &di);
   return di.rc;
}

/// sprintf to dynamically allocated buffer
char* __cdecl START_EXPORT(sprintf_dyn)(const char *fmt, long args) {
   return START_EXPORT(vsprintf_dyn)(fmt, &args);
}

int __stdcall START_EXPORT(vprintf)(const char *fmt, long *argp) {
   if (stdout)
      return START_EXPORT(vfprintf)(stdout, fmt, argp);
   else
      return _prt_common(0, fmt, argp, vioprn);
}

int __cdecl START_EXPORT(sscanf)(const char *src, const char *fmt, char *args) {
   return _scanf_common((void*)src, fmt, &args, 0, 0);
}

int __stdcall START_EXPORT(vsscanf)(const char *src, const char *fmt, char **argp) {
   return _scanf_common((void*)src, fmt, argp, 0, 0);
}

int __cdecl START_EXPORT(fscanf)(FILE *fp, const char *fmt, char *args) {
   return _scanf_common(fp, fmt, &args, (int _std(*)(void*))getc,
      (int _std(*)(int,void*))ungetc);
}

int __stdcall START_EXPORT(vfscanf)(FILE *fp, const char *fmt, char **argp) {
   return _scanf_common(fp, fmt, argp, (int _std(*)(void*))getc,
      (int _std(*)(int,void*))ungetc);
}

int __cdecl START_EXPORT(scanf)(const char *fmt, char *args) {
   return stdin ? START_EXPORT(vfscanf)(stdin, fmt, &args) : EOF;
}

int __stdcall fputs(const char *buf, FILE *fp) {
   if (!buf || !*buf) return 0; else {
      u32t   len = strlen(buf);
      size_t wrc = fwrite(buf, 1, len, fp);
      return !wrc?EOF:len;
   }
}

int __stdcall puts(const char *buf) {
   if (stdout) {
      int rc = fputs(buf, stdout);
      if (rc==EOF) return EOF; else
         if (fputc('\n', stdout)==EOF) return EOF;
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
   char  ch;
   checkret_err(EOF);
   if (fread(&ch, 1, 1, fp)) return ch;
   return EOF;
}

int __stdcall ungetc(int ch, FILE *fp) {
   checkret_err(EOF);
   if (ff->uch) return EOF;
   ff->uch = ch;
   return ch;
}

int __stdcall ungetch(int ch) {
   return stdin ? ungetc(ch,stdin) : EOF;
}

int getchar(void) {
   return stdin?getc(stdin):EOF;
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
      mt_swlock();    // ugly, but should work
      idx = pq->rtbuf[RTBUF_TMPNAME]++;
      mt_swunlock();
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
   char tdir[NAME_MAX+1], *rc;
   int found = 0;
   if (dir && strlen(dir)>=NAME_MAX) { set_errno(ENAMETOOLONG); return 0; }
   rc    = (char*)malloc_local(NAME_MAX+1);
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

FILE* __stdcall tmpfile(void) {
   u32t   pass;
   char prefix[24];
   snprintf(prefix, 24, "tmpfile_%06X_", mod_getpid());
   // try it three times
   for (pass=0; pass<3; pass++) {
      char *name = _tempnam(0, prefix);
      if (name) {
         FILE *res = fopen(name, "w+");
         free(name);

         if (res) {
            io_setstate(_os_handle(fileno(res)), IOFS_DELONCLOSE, 1);
            return res;
         }
      }
   }
   return 0;
}

u32t __stdcall hlp_isdir(const char *dir) {
   dir_t fd;
   if (!_dos_stat(dir,&fd))
      if (fd.d_attr&_A_SUBDIR) return 1;
   return 0;
}
