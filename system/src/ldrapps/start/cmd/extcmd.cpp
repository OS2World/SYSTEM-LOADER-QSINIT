//
// QSINIT "start" module
// external shell commands
//
#include "qsshell.h"
#include "stdlib.h"
#include "qs_rt.h"
#include "qsutil.h"
#include "qsstor.h"
#include "errno.h"
#include "vioext.h"
#include "internal.h"
#include "direct.h"
#include "time.h"
#include "qsint.h"
#include "qsdm.h"
#include "qshm.h"
#include "stdarg.h"
#include "qspage.h"
#include "qssys.h"
#include "qsmodext.h"

#define SPACES_IN_WIDE_MODE       4      ///< spaces between names in wide "dir" mode

void cmd_shellerr(int errorcode, const char *prefix) {
   if (!prefix) prefix = "\r";
   printf("%s",prefix);
   switch (errorcode) {
      case E2BIG  : printf("Argument list too big. \n"); break;
      case EINVAL : printf("The syntax of the command is incorrect. \n"); break;
      case ENOENT : printf("The system cannot find the file specified. \n"); break;
      case ENOMEM : printf("Out of memory. \n"); break;
      case ENOSPC : printf("Disk is full. \n"); break;
      case EACCES : printf("Access denied. \n"); break;
      case EIO    : printf("I/O error. \n"); break;
      case ENODEV : printf("There is no such device. \n"); break;
      case ENOTDIR: printf("Not a directory. \n"); break;
      case EISDIR : printf("Specified name is a directory. \n"); break;
      case ENOMNT : printf("Device is not mounted. \n"); break;
      case EEXIST : printf("A duplicate file name exists. \n"); break;
      case EMFILE : printf("Too many open files. \n"); break;
      case ENOTBLK: printf("Block device required. \n"); break;
      case EFAULT : printf("Memory access error. \n"); break;
      case EBUSY  : printf("Device or resource busy. \n"); break;
      case ENAMETOOLONG : printf("File name too long. \n"); break;
      case ENOSYS : printf("Function is not supported on EFI host. \n"); break;
      case EINVOP : printf("Invalid operation. \n"); break;
      case ELIBACC: printf("Can not access a needed DLL module. \n"); break;
      case EINTR  : break;
      default:
         printf("Error code %d. \n", errorcode); break;
   }
}

static u32t   pp_lines = 25,
             pp_lstart = 0,
            pp_logcopy = 0,
            pp_forcenp = 0; // force no pause until next init
static int   pp_buflen = 0;
static char *pp_buffer = 0; // static buffer up to 32k

extern "C" module *mod_list; // loaded module list
#pragma aux mod_list "_*";

static int pause_check(int init_counter) {
   /* pause (use signed comparition because ANSI commands can make negative
      difference */
   if (init_counter>=0 && !pp_forcenp && (int)(vio_ttylines-pp_lstart)>=(int)pp_lines-1) {
       vio_setcolor(VIO_COLOR_GRAY);
       vio_strout("Press any key to continue...");
       vio_setcolor(VIO_COLOR_RESET);
       do {
          u16t key = key_read();
          // exit by ESC
          if ((key&0xFF)==27) { vio_charout('\n'); return 1; }
          if (!log_hotkey(key)) break;
       } while (1);
       // write gray text & attribute
       vio_setcolor(VIO_COLOR_WHITE);
       vio_strout("\r                            \r");
       vio_setcolor(VIO_COLOR_RESET);
       pp_lstart = vio_ttylines;
   }
   return 0;
}

/** print command`s output.
    @param line          string to print, without \\n
    @param init_counter  <0 - no pause, 1 - init counter, 3 - copy to log
                         all strings until next init, 0 - print with pause,
                         bit 2 - force no pause until next init
    @param color         line color, 0 - default
    @return 1 if ESC was pressed in pause. */
static u32t pause_println(const char *line, int init_counter=0, u8t color=0) {
   // begin of sequence
   if (init_counter>0) {
      vio_getmode(0,&pp_lines);
      pp_lstart  = vio_ttylines;
      pp_logcopy = init_counter & 2;
      pp_forcenp = init_counter & 4;
   }
   if (!line) return 0;

   FILE *stdo = get_stdout();
   if (color) vio_setcolor(color);
   fputs(line,stdo);
   if (color) vio_setcolor(VIO_COLOR_RESET);
   fputc('\n',stdo);

   if (pp_logcopy) log_it(2,"echo: %s\n",line);
   // check pause if we have real console here
   return init_counter>=0 && !pp_forcenp && isatty(fileno(stdo)) ?
          pause_check(init_counter) : 0;
}

int __cdecl cmd_printf(const char *fmt, ...) {
   if (!fmt) return 0;
   int      prnlen;
   va_list  argptr;
   va_start(argptr, fmt);

   if (!pp_buffer) pp_buffer = (char*)malloc(pp_buflen=4096);
   while (1) {
      prnlen = vsnprintf(pp_buffer, pp_buflen, fmt, argptr);
      if (prnlen<pp_buflen || prnlen>32768) break;
      pp_buffer = (char*)realloc(pp_buffer, prnlen+1);
   }
   va_end(argptr);

   FILE *stdo = get_stdout();
   fputs(pp_buffer,stdo);
   if (pp_logcopy) log_it(2,"%s",pp_buffer);

   if (!pp_forcenp && isatty(fileno(stdo))) pause_check(0);

   return prnlen;
}

u32t __cdecl cmd_printseq(const char *fmt, int flags, u8t color, ...) {
   if (!fmt) return pause_println(0, flags, color);
   va_list argptr;
   va_start(argptr, color);
   if (!pp_buffer) pp_buffer = (char*)malloc(pp_buflen=4096);
   while (1) {
      int prnlen = vsnprintf(pp_buffer, pp_buflen, fmt, argptr);
      if (prnlen<pp_buflen || prnlen>32768) break;
      pp_buffer = (char*)realloc(pp_buffer, prnlen+1);
   }
   va_end(argptr);
   return pause_println(pp_buffer, flags, color);
}

int ask_yn(int allow_all) {
   u16t chv = 0;
   while (1) {
      chv = key_read();
      if (log_hotkey(chv)) continue;
      char cch = toupper(chv&0xFF);
      if (cch=='A') return 2;
      if (cch=='Y') return 1;
      if (cch=='N') return 0;
      if (cch==27)  return -1;
   }
}

static void process_args_common(TPtrStrings &al, char* args, short *values,
   va_list argptr)
{
   char  arg[128], *ap = args;
   // a bit of safeness
   if (strlen(args)>=128) { log_it(3, "too long args!"); return; }

   while (*ap) {
      char *ep = strchr(ap,'|');
      int  *vp = va_arg(argptr,int*);
      if (ep) {
         strncpy(arg, ap, ep-ap);
         arg[ep-ap] = 0;
         ap = ep+1;
      } else {
         strcpy(arg, ap);
         ap += strlen(ap);
      }
      int idx = al.IndexOfName(arg);
      if (idx>=0) { *vp = *values; al.Delete(idx); }
      values++;
   }
}

static void args2list(str_list *lst, TPtrStrings &al) {
   str_getstrs(lst,al);
}

void process_args(TPtrStrings &al, char* args, short *values, ...) {
   va_list   argptr;

   va_start(argptr, values);
   process_args_common(al, args, values, argptr);
   va_end(argptr);
}

str_list* str_parseargs(str_list *lst, u32t firstarg, int ret_list, char* args,
   short *values, ...)
{
   va_list   argptr;
   TPtrStrings   al;

   str_getstrs2(lst,al,firstarg);
   va_start(argptr, values);
   process_args_common(al, args, values, argptr);
   va_end(argptr);
   return ret_list ? str_getlist(al.Str) : 0;
}

#define AllowSet " ^\r\n"
#define CarrySet "-"

u32t _std cmd_printtext(const char *text, int pause, int init, u8t color) {
   if (!text) return 0;
   TStrings res;
   u32t     dX, ln;

   vio_getmode(&dX,0);
   splittext(text, dX, res);

   if (init) pause_println(0,1);
   for (ln=0;ln<res.Count();ln++)
      if (pause_println(res[ln](),pause?0:-1,color)) return 1;
   return 0;
}


u32t _std shl_copy(const char *cmd, str_list *args) {
   char *errstr = 0;
   int    quiet = 0, rc = -1;
   if (args->count>0) {
      int frombp = 0, cpattr = 0;

      TPtrStrings al;
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      // process args
      static char *argstr   = "/boot|/q|/a";
      static short argval[] = {    1, 1, 1};
      process_args(al, argstr, argval, &frombp, &quiet, &cpattr);

      al.TrimEmptyLines();
      if (al.Count()>=2) {
         spstr dst = al[al.Max()];
         al.Delete(al.Max());
         if (al[al.Max()].lastchar()!='+') {
            // must catch all combination of "+", quotes and spaces (i hope ;)
            spstr merged = al.GetTextToStr("\x01");
            al.ParseCmdLine(merged.replace("\x01",""),'+');
            al.TrimEmptyLines();
            if (al.Count()>0) {
               u8t  *is_boot = new u8t [al.Count()],
                      sfattr = _A_ARCH;
               u32t *bt_len  = frombp?new u32t[al.Count()]:0,
                     bt_type = hlp_boottype(),
                      sftime = 0,
                      sctime = 0;

               for (idx=0;idx<al.Count();idx++) al.Objects(idx)=0;
               rc = 0;
               for (idx=0;idx<al.Count();idx++) {
                  is_boot[idx] = 0;
                  if (frombp)
                     if (al[idx][1]!=':' && al[idx].cpos('\\')<0 &&
                        al[idx].cpos('/')<0) is_boot[idx] = 1;
                  // switch it to normal file i/o on FAT
                  if (is_boot[idx] && (bt_type==QSBT_FAT || bt_type==QSBT_SINGLE)) {
                     al[idx].insert("0:\\",0);
                     is_boot[idx] = 0;
                  }
                  // from boot partition?
                  if (is_boot[idx]) {
                     if (bt_type==QSBT_PXE) {
                        if (!quiet) printf("caching %s ",al[idx]());
                        void *ptr = hlp_freadfull(al[idx](),bt_len+idx,0);
                        printf("\n");
                        if (!ptr) { rc=ENOENT; break; }
                        al.Objects(idx)=ptr;
                     } else
                     if (bt_type==QSBT_FSD) {
                        // only check presence here
                        if (hlp_fopen(al[idx]())==FFFF) { rc=ENOENT; break; }
                        hlp_fclose();
                     } else {
                        // unknown boot type or no partition
                        rc=ENODEV; break;
                     }
                  } else {
                     // get time of first source for target
                     if (cpattr && !idx) {
                        dir_t fi;
                        if (!_dos_stat(al[idx](),&fi)) {
                           sftime = ((u32t)fi.d_date)<<16|fi.d_time;
                           sctime = ((u32t)fi.d_crdate)<<16|fi.d_crtime;
                           /* creation time zeroed by OS/2 on FAT and often
                              can be equal, flag it as missing too in the
                              second case */
                           if (sftime==sctime) sctime = 0;
                           sfattr = fi.d_attr;
                        }
                     }
                     // opens source
                     FILE *ff = fopen(al[idx](),"rb");
                     if (!ff) {
                        rc     = get_errno();
                        errstr = sprintf_dyn("%s : ", al[idx]());
                        break;
                     }
                     al.Objects(idx)=ff;
                  }
               }
               if (!rc) {
                  FILE  *dstf = fopen(dst(),"wb");
                  int openerr = get_errno(); // save it for printing below
                  if (!dstf) {
                     // is this a dir? try to merge file name to it
                     dir_t dstinfo;
                     if (dst.lastchar()==('/')||dst.lastchar()==('\\')) dst.dellast();
                     if (_dos_stat(dst(),&dstinfo)==0) {
                        if (dstinfo.d_attr&_A_SUBDIR) {
                           spstr name, ext;
                           _splitpath(al[0](),0,0,name.LockPtr(_MAX_FNAME+1),
                              ext.LockPtr(_MAX_EXT+1));
                           name.UnlockPtr(); ext.UnlockPtr();
                           dst +="\\";
                           dst +=name+ext;
                           dstf = fopen(dst(),"wb");
                        }
                     }
                  }

                  if (!dstf) {
                     rc     = openerr;
                     errstr = sprintf_dyn("%s : ", dst());
                  } else {
                     const u32t memsize = _128KB;
                     void *buf = hlp_memalloc(memsize,QSMA_RETERR);
                     if  (!buf) rc=ENOMEM;

                     idx = 0;
                     while (idx<al.Count() && !rc) {
                        if (!quiet) printf("%s ",al[idx]());
                        if (is_boot[idx] && bt_type==QSBT_PXE) {
                           if (bt_len[idx])
                              if (fwrite(al.Objects(idx),1,bt_len[idx],dstf)!=bt_len[idx])
                                 rc=ENOSPC;
                        } else {
                           FILE *sf = (FILE*)al.Objects(idx);
                           u32t len = sf?filelength(fileno(sf)):hlp_fopen(al[idx]()),
                                pos = 0;
                           if (len==FFFF) { rc=EIO; break; }

                           while (len) {
                              u32t copysize = len<memsize?len:memsize;
                              u32t   actual = sf?fread(buf,1,copysize,sf):
                                              hlp_fread(pos,buf,copysize);
                              if (actual!=copysize) { rc=EIO; break; }
                              actual = fwrite(buf,1,copysize,dstf);
                              if (actual!=copysize) { rc=ENOSPC; break; }
                              len-=copysize;
                              pos+=copysize;
                           }
                           if (!sf) hlp_fclose();
                        }
                        if (!quiet) printf("\n");
                        idx++;
                     }
                     fclose(dstf);
                     // creation time can be 0
                     if (cpattr && sftime) {
                        if (sctime) _dos_setfiletime(dst(), sctime, _DT_CREATE);
                        // if no creation time - set both to the same in one call
                        _dos_setfiletime(dst(), sftime, (sctime?0:_DT_CREATE)|
                           _DT_MODIFY);
                        _dos_setfileattr(dst(), sfattr);
                     }
                     if (buf) hlp_memfree(buf);
                  }
               }
               for (idx=0;idx<al.Count();idx++)
                  if (al.Objects(idx))
                     if (is_boot[idx]) hlp_memfree(al.Objects(idx));
                        else fclose((FILE*)al.Objects(idx));
               delete bt_len;
               delete is_boot;
            }
         }
         if (rc>0) log_it(3, "copy err %d, file \"%s\"\n", rc, dst());
      }
   }
   if (rc<0) rc = EINVAL;
   if (!quiet&&rc) cmd_shellerr(rc,errstr);
   if (errstr) free(errstr);
   return rc;
}

u32t _std shl_type(const char *cmd, str_list *args) {
   int rc=-1;
   if (args->count>0) {
      int frombp=0, nopause=0;

      TPtrStrings al;
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      // process args
      static char *argstr   = "/boot|/np";
      static short argval[] = {    1,  1};
      process_args(al, argstr, argval, &frombp, &nopause);

      al.TrimEmptyLines();
      if (al.Count()>=1) {
         u32t bt_type = hlp_boottype();
         // init "paused" output
         pause_println(0, nopause?PRNSEQ_INITNOPAUSE:PRNSEQ_INIT);
         // print files
         for (idx=0; idx<al.Count(); idx++) {
            spstr name(al[idx]);
            u32t  is_boot = 0, f_size = 0;
            char *f_data = 0;

            if (frombp)
               if (name[1]!=':' && name.cpos('\\')<0 && name.cpos('/')<0)
                  is_boot = 1;
            if (is_boot) {
               if (bt_type==QSBT_FAT) {
                  name.insert("0:\\",0);
                  is_boot = 0;
               } else {
                  if (bt_type==QSBT_PXE||bt_type==QSBT_FSD)
                     f_data = (char*)hlp_freadfull(name(),&f_size,0);
                        else { rc=ENODEV; break; }
               }
            }
            if (!is_boot) f_data = (char*)freadfull(name(),&f_size);
            if (!f_data) { rc=ENOENT; break; }

            if (al.Count()>1) {
               name.insert("** ",0);
               name+=" **";
               pause_println(name(), 0, 0x0A);
            }
            TPtrStrings ft;
            ft.SetText(f_data, f_size);
            hlp_memfree(f_data);
            for (u32t ii=0;ii<ft.Count();ii++)
               if (pause_println(ft[ii](),0)) return EINTR;
         }
         if (rc<0) rc=0;
      }
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return rc;
}

struct dir_cbinfo {
   int    dir_npmode;
   int       dir_rec;
   u64t    sizetotal; // size in kb
   int     ppath_len;
   int      shortfmt;
   int       mask_on;
   long     file_cnt;
   int        inited;
   int        crtime;
   int     check_esc;
   u32t   lincounter; // counter for esc check
   int        ubreak; // user breaks this
   u32t      widelen; // max len for wide, else
};

static int readtree_prn(dir_cbinfo *cbi, spstr &pp) {
   int rc = pause_println(pp(), cbi->dir_npmode)?-1:1;
   // check for ESC every 1024 files in non-pause mode
   if (cbi->dir_npmode<0 && cbi->check_esc && (++cbi->lincounter&1023)==0) {
      u16t key = key_wait(0);
      if (key && !log_hotkey(key))
         if ((key&0xFF)==27) rc = -1;
   }
   // flag user break
   if (rc<0) cbi->ubreak = 1;
   return rc;
}

static void dir_mkrelpath(dir_cbinfo *cbi, dir_t *fp, spstr &out) {
   out = fp->d_openpath;
   out.replacechar('/','\\');
   if (out.lastchar()!='\\') out+="\\";
   out += fp->d_name;
   // first time in cycle
   if (cbi->ppath_len<0) cbi->ppath_len = strlen(fp->d_openpath);
   out.del(0, cbi->ppath_len);
   if (out[0]=='\\') out.del(0,1);
}

static int __stdcall readtree_cb(dir_t *fp, void *cbinfo) {
   dir_cbinfo *cbi = (dir_cbinfo*)cbinfo;
   int   firsttime = cbi->ppath_len<0;
   // directory filteing: skip . / not matched / .. in child dirs
   if ((fp->d_attr&_A_SUBDIR)!=0) {
      int match = (fp->d_attr&0x80)==0;

      if (match && (firsttime||strcmp(fp->d_name,"..")))
      {} else {
         cbi->file_cnt--;
         return 1;
      }
   }
   // print it
   cbi->sizetotal += Round1k(fp->d_size)>>10;
   // full relative name
   spstr pp;
   dir_mkrelpath(cbi, fp, pp);
   // insert date & size
   if (!cbi->shortfmt) {
      struct tm tme;
      char      buf[64];
      if (fp->d_attr&_A_SUBDIR) strcpy(buf,"  <DIR>      ");
         else sprintf(buf," %10u  ",fp->d_size);
      pp.insert(buf,0);
      // cvt selected time
      dostimetotm(cbi->crtime ? (u32t)fp->d_crdate<<16|fp->d_crtime :
         (u32t)fp->d_date<<16|fp->d_time, &tme);
      // format time
      sprintf(buf,"%02d.%02d.%04d  %02d:%02d ",tme.tm_mday,tme.tm_mon+1,
         tme.tm_year+1900,tme.tm_hour,tme.tm_min);
      pp.insert(buf,0);
   }
   return readtree_prn(cbi, pp);
}

static void dir_total(unsigned drive, dir_cbinfo *cbi) {
   if (!cbi->shortfmt && cbi->inited) {
      spstr sum;
      if (cbi->file_cnt) {
         sum.sprintf("\n%s in %d files", dsk_formatsize(1024,cbi->sizetotal,10,0),
            cbi->file_cnt);
         pause_println(sum(),cbi->dir_npmode);
      }
      diskfree_t df;
      if (!_dos_getdiskfree(drive+1,&df)) {
         sum.sprintf("%s free", dsk_formatsize(df.bytes_per_sector,
            (u64t)df.sectors_per_cluster*df.avail_clusters,10,0));
         pause_println(sum(),cbi->dir_npmode);
      }
      cbi->sizetotal = 0;
      cbi->file_cnt  = 0;
   }
   cbi->inited = 1;
}

static void dir_wide(dir_t *pd, dir_cbinfo *cbi, TPtrStrings &dstack) {
   u32t ii, maxlen = 0, nperline = 1, cols;

   for (ii=0; pd[ii].d_name[0]; ii++) {
      u32t ln = strlen(pd[ii].d_name);
      // +2 for []
      if ((pd[ii].d_attr&_A_SUBDIR)!=0) {
         ln+=2;
         if (pd[ii].d_sysdata) {
            spstr dirname;
            dir_mkrelpath(cbi, pd+ii, dirname);
            dstack.AddObject(dirname, pd[ii].d_sysdata);
         }
      }
      if (ln>maxlen) maxlen = ln;
   }
   vio_getmode(&cols, 0);
   if (maxlen) nperline = cols/(maxlen+SPACES_IN_WIDE_MODE);
   if (nperline==0) nperline++;

   spstr   outline;
   char     *cname = new char[maxlen+SPACES_IN_WIDE_MODE+1], *cp;

   for (ii=0; pd[ii].d_name[0]; ii++) {
      int   dir = pd[ii].d_attr&_A_SUBDIR,
            eol = ii%nperline==nperline-1 || pd[ii+1].d_name[0]==0;
      cp = cname;

      if (dir) *cp++='[';
      strcpy(cp, pd[ii].d_name);  cp+=strlen(cp);
      if (dir) *cp++=']';
      if (!eol)
         while (cp-cname < maxlen+SPACES_IN_WIDE_MODE) *cp++=' ';
      *cp = 0;
      outline += cname;

      cbi->sizetotal += Round1k(pd[ii].d_size)>>10;
      if (eol) {
         readtree_prn(cbi, outline);
         outline.clear();
         if (cbi->ubreak) break;
      }
   }
   delete cname;
}


static void splitpath(spstr &fp, spstr &drv, spstr &dir, spstr &name, spstr &ext) {
   fp.replace('/','\\');
   if (fp.lastchar()=='\\' && (fp.length()>3||fp[1]!=':'&&fp.length()>1))
      fp.dellast();
   // split name
   _splitpath(fp(), drv.LockPtr(_MAX_DRIVE+1), dir.LockPtr(_MAX_DIR+1),
      name.LockPtr(_MAX_FNAME+1), ext.LockPtr(_MAX_EXT+1));
   drv.UnlockPtr(); dir.UnlockPtr(); name.UnlockPtr(); ext.UnlockPtr();
   drv+=dir; name+=ext;
}


u32t _std shl_dir(const char *cmd, str_list *args) {
   int rc=-1, shortfmt=0, nopause=0, subdirs=0, crtime=0, wide=0;

   TPtrStrings al;
   str_getstrs(args,al);
   // is help?
   int idx = al.IndexOf("/?");
   if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
   // process args
   static char *argstr   = "/s|/np|/b|/tc|/w";
   static short argval[] = { 1,  1, 1, 1, 1};
   process_args(al, argstr, argval, &subdirs, &nopause, &shortfmt, &crtime, &wide);

   al.TrimEmptyLines();
   if (!al.Count()) al<<spstr(".");
   if (al.Count()>=1) {
      FILE *stdo = get_stdout();
      // init "paused" output
      pause_println(0,1);
      dir_cbinfo cbi;
      cbi.dir_npmode = nopause?-1:0;
      cbi.dir_rec    = subdirs;
      cbi.sizetotal  = 0;
      cbi.shortfmt   = shortfmt;
      cbi.file_cnt   = 0;
      cbi.inited     = 0;
      cbi.crtime     = crtime;
      cbi.check_esc  = nopause && isatty(fileno(stdo));
      cbi.lincounter = 0;
      cbi.ubreak     = 0;

      for (idx=0; idx<al.Count(); idx++) {
         // fix and split path
         spstr pp(al[idx]), drive, dir, name, ext;
         splitpath(pp,drive,dir,name,ext);

         cbi.ppath_len   = -1;
         cbi.mask_on     = name.cpos('*')>=0 || name.cpos('?')>=0;
         int       error =  0;
         unsigned ldrive =  0;

         if (!cbi.mask_on) {
            dir_t fi;
            if (_dos_stat(pp(),&fi)) {
               if (!subdirs) error = get_errno();
            } else
            if (fi.d_attr&_A_SUBDIR) { drive=pp; name.clear(); }
         }
         if (!error) {
            if (!drive) drive = hlp_curdir(hlp_curdisk()); else {
               char *fp = _fullpath(0,drive(),0);
               fp[0] = fp[0]-'A'+'0';
               drive = fp;
               free(fp);
            }
            // print header on dir mismatch only
            if (!cbi.shortfmt && (!idx||stricmp(drive(),al[idx-1]())!=0)) {
               spstr head;
               disk_volume_data vinf;
               dir_total(ldrive, &cbi);

               hlp_volinfo(drive[0]-'0',&vinf);
               int lbon = vinf.Label[0];
               head.sprintf("  Directory of %s\n"
                            "  Volume in drive %c %ss %s\n"
                            "  Volume Serial Number is %04X-%04X\n", drive(),
                               drive[0], lbon?"i":"ha", lbon?vinf.Label:"no label",
                               vinf.SerialNum>>16,vinf.SerialNum&0xFFFF);

               if (pause_println(head(),cbi.dir_npmode)) return EINTR;
            }
            dir_t *fdl = 0;
            // update for comparing above
            al[idx] = drive;
            /* read tree:
               * in normal mode - one by one, via callback
               * in wide mode - read all, then print */
            u32t count = _dos_readtree(drive(), name(), wide?&fdl:0, subdirs,
                                       wide?0:readtree_cb, &cbi);
            if (wide && count>0) {
               TPtrStrings dstack;
               dstack.AddObject("", fdl);
               // print tree
               while (dstack.Count() && !cbi.ubreak) {
                  if (dstack[0].length())
                     if (cmd_printseq("\n  Directory of %s", cbi.dir_npmode, 0, dstack[0]())) {
                        cbi.ubreak = 1;
                        break;
                     }
                  dir_wide((dir_t*)dstack.Objects(0), &cbi, dstack);
                  dstack.Delete(0);
               }
            }
            if (fdl) _dos_freetree(fdl);

            if (cbi.ubreak) {
               printf("User break\n");
               return EINTR;
            } else {
               // total files
               cbi.file_cnt += count;
               if (!count && cbi.ppath_len==-1) error = get_errno();
               // last cycle print
               ldrive = drive[0]-'0';
               if (rc<0 && !error && idx==al.Max()) dir_total(ldrive, &cbi);
            }
         }
         if (error) cmd_shellerr(rc=error,0);
      }
      if (rc<0) rc=0;
   } else {
      if (rc<0) rc = EINVAL;
      if (rc) cmd_shellerr(rc,0);
   }
   return rc;
}

u32t _std shl_restart(const char *cmd, str_list *args) {
   int rc=-1;
   if (args->count>0) {
      TPtrStrings al;
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      // process
      al.TrimEmptyLines();
      if (al.Count()>=1) {
         if (hlp_hosttype()==QSHT_EFI) rc = ENOSYS; else {
            exit_restart((char*)al[0]());
            rc = ENOENT;
         }
      }
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return rc;
}

int _std cmd_shellhelp(const char *cmd, u8t color) {
   char *msg = cmd_shellgetmsg(cmd);
   if (!msg) return 0;
   // enable ANSI for the time of help message printing...
   u32t state = vio_setansi(1);

   cmd_printseq(0,1,color);
   cmd_printtext(msg,1,1,color);
   free(msg);

   if (!state) vio_setansi(0);

   return 1;
}

u32t _std shl_help(const char *cmd, str_list *args) {
   int rc=-1;
   TStrings al;
   str_getstrs(args,al);

   if (!args->count || al.IndexOf("/?")>=0) {
      str_list *lst = cmd_shellqall(0);
      char    *list = str_gettostr(lst,", "),
              *cend = strrchr(list,',');
      if (cend) *cend = 0;
      cmd_printtext("Help:\n\nHELP [command]\n",1,1,CLR_HELP);
      spstr cmlist(list);
      free(list);
      cmlist.insert("available commands: ",0);
      cmd_printtext(cmlist(),1,0,CLR_HELP);
      return 0;
   }

   al.TrimEmptyLines();
   if (al.Count()>=1) {
      spstr cmh(al[0]);
      cmh.upper();
      if (cmd_shellrmv(cmh(),0)) {
         al[0]="/?";
         rc = shl_extcall(cmh,al);
      } else {
         if (!cmd_shellhelp(cmh(),CLR_HELP)) {
            spstr msg;
            msg.sprintf("\nThere is no help entry for \"%s!\"\n",cmh());
            cmd_printtext(msg(),1,1,CLR_HELP);
         }
         rc = 0;
      }
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return rc;
}

u32t _std shl_mkdir(const char *cmd, str_list *args) {
   int rc=-1;
   if (args->count>0) {
      TPtrStrings al;
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      // process
      al.TrimEmptyLines();
      if (al.Count()>=1) {
         mkdir(al[0]());
         rc = get_errno();
      }
      if (rc>0) log_it(3, "mkdir err %d, dir \"%s\"\n", rc, al[0]());
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return rc;
}

/// internal chdir for CD / PUSHD / POPD commands
static int chdir_int(const char *path) {
   char *fp = _fullpath(0, path, 0);
   spstr pstr(fp);
   free(fp);
   int rc = chdir(pstr());
   if (rc) rc = get_errno();

   if (!rc && pstr[1]==':') {
      char disk = toupper(pstr[0]);
      if (disk>='A'&&disk<='Z') disk+='0'-'A';
      hlp_chdisk(disk - '0');
   }
   return rc;
}

u32t _std shl_chdir(const char *cmd, str_list *args) {
   int rc=-1;
   TPtrStrings al;
   str_getstrs(args,al);
   // is help?
   int idx = al.IndexOf("/?");
   if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
   // process
   al.TrimEmptyLines();
   if (al.Count()>=1) {
      rc = chdir_int(al[0]());
   } else {
      printf("%s\n",hlp_curdir(hlp_curdisk()));
      rc = EZERO;
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return rc;
}

void dir_to_list(spstr &Dir, dir_t *info, d exclude_attr, TStrings &rc, TStrings *dirs) {
   TPtrStrings  stack;
   rc.Clear();
   if (!info) return;
   dir_t *di = info;

   spstr dir(Dir);
   if (dir.length()) {
      if (dir.lastchar()=='\\') dir.dellast();
      if (dir.lastchar()!='/') dir+='/';
   }

   do {
      while (di->d_name[0]) {
         spstr cname(dir);
         cname+=di->d_name;
         // add file
         if ((di->d_attr&exclude_attr)==0) rc.AddObject(cname,di->d_attr);
         // add dir to stack
         if ((di->d_attr&_A_SUBDIR) && strcmp(di->d_name,"..")) {
            stack.AddObject(cname,di->d_sysdata);
            if (dirs) dirs->AddObject(cname,di->d_attr);
         }
         di++;
      }
      while (stack.Count()) {
         di  = (dir_t*)stack.Objects(0);
         dir = stack[0];
         dir+= "/";
         stack.Delete(0);
         if (di) break;
      }
   } while (di&&di->d_name[0]||stack.Count());
   rc.Sort();
}

u32t _std shl_del(const char *cmd, str_list *args) {
   int rc=-1;
   if (args->count>0) {
      int askall=0, force=0, subdir=0, quiet=0, nopause=0, mask=0, nquiet=0,
          edirs=0, breakable=1;

      TPtrStrings al;
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      // process args
      static char *argstr   = "/p|/f|/s|/q|/e|/qn|/np|/nb";
      static short argval[] = { 1, 1, 1, 1, 1,  1,  1,  0};
      process_args(al, argstr, argval,
                   &askall, &force, &subdir, &quiet, &edirs,
                   &nquiet, &nopause, &breakable);

      al.TrimEmptyLines();
      if (al.Count()>=1) {
         u32t fcount = 0;
         // init "paused" output
         pause_println(0,1);
         // delete files
         for (idx=0; idx<al.Count(); idx++) {
            // fix and split path
            spstr pp(al[idx]), drive, dir, name, ext;
            splitpath(pp,drive,dir,name,ext);

            mask = name.cpos('*')>=0 || name.cpos('?')>=0;

            int  error = 0;
            dir_t   fi;

            if (!mask) {
               if (_dos_stat(pp(),&fi)) {
                  if (!subdir) error = ENOENT;
               } else
               if (fi.d_attr&_A_SUBDIR) {
                  drive = pp;
                  name  = "*.*";
                  mask  = true;
               }
            }
            if (!error) {
               if (!drive) drive = hlp_curdir(hlp_curdisk()); else {
                  char *fp = _fullpath(0,drive(),0);
                  fp[0] = fp[0]-'A'+'0';
                  drive = fp;
                  free(fp);
               }
               TStrings  dellist, dirlist;
               spstr     dname(drive);
               if (dname.length() && dname.lastchar()!='\\') dname+="\\";
               dname+=name;

               if (mask || subdir) {
                  // read tree
                  dir_t *info = 0;
                  u32t cnt = _dos_readtree(drive(),name(),&info,subdir,0,0);
                  if (info) {
                     if (cnt) dir_to_list(drive, info, (force?0:_A_RDONLY)|
                        _A_SUBDIR|_A_VOLID, dellist, edirs?&dirlist:0);
                     _dos_freetree(info);

                     u32t todel = dellist.Count()+dirlist.Count();

                     if (todel && !quiet && (mask || todel>1)) {
                         printf("Delete %s (%d files) (y/n/esc)?",dname(),todel);
                         int yn = ask_yn();
                         printf("\n");
                         if (yn<0) return EINTR;
                         if (!yn) { dellist.Clear(); dirlist.Clear(); }
                     }
                  }
               } else
                  dellist.AddObject(dname,fi.d_attr);

               if (dellist.Count() + dirlist.Count()) {
                  int ii;
                  spstr msg;  // put it here to prevent malloc on every file

                  for (ii=0; ii<dellist.Count(); ii++) {
                     int doit = 1;
                     if (askall) {
                        printf("Delete %s (y/n/esc)?",dellist[ii]());
                        doit = ask_yn();
                        printf("\n");
                        if (doit<0) return EINTR;
                     } else
                     if (breakable) // check for ESC key pressed
                        if (key_pressed())
                           if ((key_read()&0xFF)==27) {
                              printf("Break this DEL command (y/n)?");
                              int yn = ask_yn();
                              printf("\n");
                              if (yn==1) return EINTR;
                           }
                     if (doit) {
                        if ((dellist.Objects(ii)&_A_RDONLY)!=0)
                           if (force) {
                               unsigned attributes = _A_NORMAL;
                               _dos_getfileattr(dellist[ii](),&attributes);
                               _dos_setfileattr(dellist[ii](),attributes&~_A_RDONLY);
                           }
                        if (unlink(dellist[ii]())==0) {
                           msg.sprintf("Deleted file - \"%s\"",dellist[ii]());
                           fcount++;
                        } else {
                           msg.sprintf("Unable to delete \"%s\". Error %d",dellist[ii](),get_errno());
                        }
                        if (!nquiet)
                           if (pause_println(msg(),nopause?-1:0)) return EINTR;
                     }
                  }
                  for (ii=dirlist.Max(); ii>=0; ii--) rmdir(dirlist[ii]());
               }
            }
         }
         if (rc<0) rc=0;
         if (!rc && !nquiet) printf("%d file(s) deleted.\n",fcount);
      }
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return rc;
}

u32t _std shl_beep(const char *cmd, str_list *args) {
   int  wait=0, before=0, idx;
   TPtrStrings al;
   str_getstrs(args,al);
   // process
   al.TrimEmptyLines();
   if (al.Count()>=1) {
      // is help?
      idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      // process args
      static char *argstr   = "/w|/b";
      static short argval[] = { 1, 1};
      process_args(al, argstr, argval, &wait, &before);
   }
   pause_println(0,1);
   if (before)
      while (vio_beepactive()) usleep(10*1000);

   if (!al.Count()) vio_beep(500,500);
      else
   for (idx=0; idx<al.Count(); idx++) {
      u32t freq = al[idx].word_Dword(1,","),
            dur = al[idx].word_Dword(2,",");
      if (!freq||!dur) continue;
      vio_beep(freq,dur);

      if (idx<al.Max())
         while (vio_beepactive()) usleep(10*1000);
   }
   if (wait)
      while (vio_beepactive()) usleep(10*1000);
   return EZERO;
}

u32t _std shl_rmdir(const char *cmd, str_list *args) {
   int rc=-1, subdir=0, quiet=0;
   TPtrStrings al;
   str_getstrs(args,al);
   // is help?
   int idx = al.IndexOf("/?");
   if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
   // process args
   static char *argstr   = "/q|/s";
   static short argval[] = { 1, 1};
   process_args(al, argstr, argval, &quiet, &subdir);
   // process command
   al.TrimEmptyLines();
   if (al.Count()>=1)
   do {
      dir_t fi;
      char *fp = _fullpath(0,al[0](),0);
      al[0] = fp;
      free(fp);

      if (_dos_stat(al[0](),&fi)) { rc=ENOENT; break; }
      if ((fi.d_attr&_A_SUBDIR)==0) { rc=ENOTDIR; break; }
      if (subdir) {
         spstr dcs;
         dcs.sprintf("/s /qn /np /e /f %s \"%s\"",quiet?"/q":"",al[0]());
         rc = cmd_shellcall(shl_del,dcs(),0);
      }
      if (rc<=0) {
         rc = rmdir(al[0]());
         if (rc) rc = get_errno();
      }
   } while (false);

   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return rc;
}

u32t _std shl_loadmod(const char *cmd, str_list *args) {
   int rc=-1, quiet=0, unload=0, verb=0, list=0, nopause = 0;
   if (args->count>0) {
      TPtrStrings     al;
      ansi_push_set ansi(1);

      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      // process args
      static char *argstr   = "/q|/u|/v|/list|/l|/np";
      static short argval[] = { 1, 1, 1,    1, 1,  1};
      process_args(al, argstr, argval, &quiet, &unload, &verb, &list, &list, &nopause);
      // process command
      al.TrimEmptyLines();
      // wrong args?
      if (unload && list || !Xor(list,al.Count())) rc = EINVAL; else {
         // init line counter
         pause_println(0, nopause?PRNSEQ_INITNOPAUSE:PRNSEQ_INIT);

         if (list) {
            if (verb) log_mdtdump_int(cmd_printf); else {
               module *md = mod_list;
               u32t   cnt = 0;

               while (md) {
                  int islib = md->flags&MOD_LIBRARY,
                      issys = md->flags&MOD_SYSTEM;
                  char pstr[32];

                  if (md->flags&MOD_EXECPROC) {
                     snprintf(pstr, 32, "pid:%u", mod_getmodpid((u32t)md));
                  } else 
                     pstr[0] = 0;

                  cmd_printf("%3u. " ANSI_WHITE "%-20s" ANSI_RESET " %s %s%s\n",
                     ++cnt, md->name, islib?ANSI_YELLOW "DLL" ANSI_RESET:
                        ANSI_LGREEN "EXE" ANSI_RESET, issys?
                           ANSI_LRED "system " ANSI_RESET:"", pstr);
                  md = md->next;
               }
            }
            rc = 0;
         } else {
            for (idx=0; idx<al.Count(); idx++) {
               spstr mdn(al[idx]);
               if (unload) {
                  // test of module unload
                  u32t mod = mod_query(mdn(), MODQ_NOINCR), rc;
                  if (!mod) {
                     if (!quiet) cmd_printf("There is no active module \"%s\"\n", mdn());
                     rc = ENOENT;
                  } else {
                     u32t res = mod_free(mod);
                     if (res && !quiet) {
                        switch (res) {
                           case MODFERR_SELF:
                              cmd_printf("Unable to free self (\"%s\")\n", mdn());
                              break;
                           case MODFERR_SYSTEM:
                              cmd_printf("Unable to free system module (\"%s\")\n", mdn());
                              break;
                           case MODFERR_LIBTERM:
                              cmd_printf("DLL term function denied unloading (\"%s\")\n", mdn());
                              break;
                           case MODFERR_EXECINPROC:
                              cmd_printf("Exec in progress (\"%s\")\n", mdn());
                              break;
                           default: 
                              cmd_printf("Unload error %d for module \"%s\"\n", rc, mdn());
                        }
                     }
                     if (res) rc = EBUSY;
                  }
               } else {
                  u32t error = 0;
                  module *md = load_module(mdn, &error);
                  if (md) rc = 0; else {
                     rc = ENOENT;
                     if (!quiet) {
                        cmd_printf("Error loading module \"%s\".\n",mdn());
                        spstr msg;
                        msg.sprintf("_MOD%02d",error);
                        cmd_shellhelp(msg(),VIO_COLOR_GRAY);
                     }
                  }
               }
            }
            if (rc<0) rc = 0;
         }
      }
   }
   if (rc<0) rc = EINVAL;
   if (rc && rc!=ENOENT && rc!=EBUSY) cmd_shellerr(rc,0);
   return rc;
}

u32t _std shl_power(const char *cmd, str_list *args) {
   int quiet=0, rc=-1;
   if (args->count>0) {
      TPtrStrings al;
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      idx = al.IndexOfName("/q");
      if (idx>=0) { quiet=1; al.Delete(idx); }

      if (al.Count()>0) {
         if (al[0].upper()=="ON") {
            printf("This PC is already powered on.\n");
            rc = 0;
         } else
         if (al[0]=="OFF" || al[0]=="S" || al[0]=="SUSPEND") {
            int susp = al[0][0]=='S'?1:0;

            if (hlp_hosttype()==QSHT_EFI) {
               // suspend is not supported on EFI host
               if (susp) {
                  rc = ENOSYS;
                  if (quiet) return rc;
               }
            } else {
               u32t ver = hlp_querybios(QBIO_APM);
               int apmerr = ver&0x10000;
               if (apmerr) {
                  log_it(2,"APM error %02X\n", ver&0xFF);
                  if (!quiet) printf("There is no APM on this PC.\n");
                  return ENODEV;
               }
               log_it(2,"APM version %d.%d\n", ver>>8&0xFF,ver&0xFF);
            }
            if (rc<=0) {
               int yn = 1;
               if (!susp && !quiet) {
                  printf("Power off (y/n)?");
                  yn = ask_yn();
               }
               if (yn==1) exit_poweroff(susp);
               rc = 0;
            }
         }
      }
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return 0;
}

// abort entering on -1
static int _std label_cb(u16t key, key_strcbinfo *inputdata) {
   char cch = toupper(key&0xFF);
   if (cch==27) return -1;
   return 0;
}

u32t _std shl_label(const char *cmd, str_list *args) {
   int rc=-1;
   TPtrStrings al;
   str_getstrs(args,al);
   // is help?
   int idx = al.IndexOf("/?");
   if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }

   if (al.Count()>0) {
      u8t drive;
      int   pos = 0;
      if (al[0][1]==':') {
         char dch = toupper(al[0][0]);
         // correct A:..Z:
         if (dch>='A'&&dch<='Z') dch-='A'-'0';
         drive = dch-'0';
         pos++;
      } else
         drive = hlp_curdisk();
      rc = hlp_vollabel(drive,al.Count()>pos?al[pos]():0)?0:EACCES;
   } else {
      disk_volume_data vinf;
      u8t drive = hlp_curdisk();
      hlp_volinfo(drive,&vinf);
      int  lbon = vinf.Label[0];

      printf(lbon?"Volume in drive %c is %s\n":"Volume in drive %c has no label\n",
         drive+'0', vinf.Label);
      printf("Volume Serial Number is %04X-%04X\n",
         vinf.SerialNum>>16,vinf.SerialNum&0xFFFF);
      printf("Volume label (11 characters, ENTER - none, ESC)? ");
      char* label = key_getstr(label_cb);
      if (label) {
         rc = hlp_vollabel(drive,*label?label:0)?0:EACCES;
         free(label);
      } else {
         rc = 0;
      }
      printf("\n");
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return 0;
}

u32t _std shl_trace(const char *cmd, str_list *args) {
   int quiet=0, rc=-1, nopause=0;
   if (args->count>0) {
      TPtrStrings al;
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      static char *argstr   = "/q|/np";
      static short argval[] = { 1,  1};
      process_args(al, argstr, argval, &quiet, &nopause);

      if (al.Count()>0) {
         al[0].upper();
         if (al[0]=="ON" || al[0]=="OFF") {
            int on = al[0][1]=='N';
            // don`t allow empty module name in ON
            if (!on || al.Count()>1) {
               rc = 0;
               if (al.Count()<=2) trace_onoff(al.Count()>1?al[1]():0,0,quiet,on);
                  else
               for (int ii=2; ii<al.Count(); ii++) {
                  rc = 0;
                  if (!trace_onoff(al[1](),al[ii](),quiet,on)) break;
               }
            }
         } else
         if (al[0]=="LIST") {
            trace_list(al.Count()>1?al[1]():0,al.Count()>2?al[2]():0,!nopause);
            rc = 0;
         }
      }
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return 0;
}

/** common RENAME/MOVE processing.
    @param  al       argument list
    @param  is_ren   action type (ren/move)
    @param  quiet    be quiet flag
    @retval 0        on success
    @retval EINVAL   invalid argument
    @retval EINVOP   rename/move to another drive or directory
    @retval ENAMETOOLONG  result name too long */
static int processRenMove(TPtrStrings &al, int is_ren, int quiet) {
   spstr target(al[al.Max()]), tgt_path, tgt_name;
   int   tgt_mask = target.cpos('?')>=0 || target.cpos('*')>=0;
   int         rc = -1;
   u32t   counter = 0;
   al.Delete(al.Max());

   splitfname(target, tgt_path, tgt_name);
   if (!fullpath(tgt_path)) rc = ENAMETOOLONG; else {
      if (tgt_path.lastchar()!='\\') tgt_path+="\\";
      // wildcards
      if (tgt_path.cpos('?')>=0||tgt_path.cpos('*')>=0) rc = EINVAL;
   }
   // target is directory
   if (!is_ren && tgt_name.length())
       if (hlp_isdir(target())) {
          tgt_path += tgt_name;
          tgt_path += "\\";
          tgt_name.clear();
       }
   if (is_ren && al.Count()>1) rc = EINVAL;

   if (rc>=0) {
      if (!quiet) cmd_shellerr(rc,0);
   } else
   while (al.Count()) {
      spstr dir, name;
      splitfname(al[0], dir, name);
      if (!fullpath(dir)) return ENAMETOOLONG;
      if (dir.lastchar()!='\\') dir+="\\";

      if (is_ren && stricmp(dir(),tgt_path())) {
         // rename to another directory
         if (!quiet) printf("Rename to another directory is not possible.\n");
         return EINVOP;
      } else
      if (dir[0]!=tgt_path[0]) {
         // move to another drive
         if (!quiet) printf("Rename/move to another drive is not possible.\n");
         return EINVOP;
      } else
      if (dir.length()>=NAME_MAX-1 || dir.length()+tgt_name.length()>=NAME_MAX) {
         rc = ENAMETOOLONG;
         if (!quiet) cmd_shellerr(rc,0);
         break;
      } else {
         char buf[NAME_MAX+NAME_MAX/2];
         int matchcount = 0, idx;
         strcpy(buf,dir());
         TStrings slst, dlst;

         if (name.cpos('?')>=0||name.cpos('*')>=0) {
            dir_t *dt = opendir(dir()), *drec = dt;
            if (!dt) rc = ENOENT; else {
               while (drec) {
                  drec = readdir(dt);
                  if (drec)
                     if (_replacepattern(name(),!tgt_name?name():tgt_name(),drec->d_name,buf)) {
                        slst.Add(dir+spstr(drec->d_name));
                        dlst.Add(tgt_path+spstr(buf));
                        matchcount++;
                     }
               }
               closedir(dt);
            }
         } else {
            if (!tgt_mask) {
               slst.Add(dir+name);
               dlst.Add(tgt_path+tgt_name);
               matchcount++;
            } else
            if (_replacepattern(name(),tgt_name(),name(),buf)) {
               slst.Add(dir+name);
               dlst.Add(tgt_path+spstr(buf));
               matchcount++;
            }
         }

         if (!matchcount) {
            if (!quiet)
               printf("There is no replacement for specified criteria.\n");
            return ENOENT;
         } else
         for (idx=0; idx<slst.Count(); idx++) {
            u32t newrc;
            if (dlst[idx].length()>NAME_MAX) newrc = ENAMETOOLONG;
               else newrc = rename(slst[idx](), dlst[idx]()) ?get_errno():0;
            if (!newrc) counter++;
            if (newrc && !quiet) cmd_shellerr(newrc,0);
            if (newrc || rc<=0) rc = newrc;
         }
      }
      al.Delete(0);
   }
   if (counter && !quiet && !is_ren) printf("%d file(s) moved.\n",counter);
   return rc<0?0:rc;
}

u32t _std shl_ren(const char *cmd, str_list *args) {
   int quiet=0, rc=-1;
   if (args->count>0) {
      TPtrStrings al;
      str_getstrs(args,al);
      // is it help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      idx = al.IndexOfName("/q");
      if (idx>=0) { quiet=1; al.Delete(idx); }

      rc = processRenMove(al, 1, quiet);
   }
   if (rc<0) cmd_shellerr(rc = EINVAL,0);
   return rc;
}

u32t _std shl_date(const char *cmd, str_list *args) {
   int setd=0, quiet=0, rc=-1, idx;
   TPtrStrings al;
   struct tm tme;
   time_t    tmv;

   str_getstrs(args,al);
   // process
   al.TrimEmptyLines();
   if (al.Count()>=1) {
      // is help?
      idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      // process args
      static char *argstr   = "/q|/s";
      static short argval[] = { 1, 1};
      process_args(al, argstr, argval, &quiet, &setd);
   }
   if (al.Count()==0 || al.Count()==1 && !setd) {
      if (al.Count()==0 && !quiet) {
         time(&tmv);
         localtime_r(&tmv,&tme);
         printf("The current date is: %02d.%02d.%04d\n", tme.tm_mday, tme.tm_mon+1,
            tme.tm_year+1900);
      }

      if (setd) {
         if (al.Count()!=0) rc = EINVAL; else {
            printf("Enter new date in DD.MM.YYYY format: ");
            char* tms = key_getstr(label_cb);
            printf("\n");
            if (tms) {
               al.Add(tms); al[0].trim();
               // empty string?
               if (!al[0]) rc = 0;
               free(tms);
            } else rc = 0;
         }
      }
      if (rc<0 && al.Count()==1) {
         u32t dostime = 0;
         time(&tmv);
         localtime_r(&tmv,&tme);

         if (al[0].words(".")>=2) {
            int svyear = tme.tm_year+1900;
            tme.tm_mday = al[0].word_Int(1,".");
            tme.tm_mon  = al[0].word_Int(2,".")-1;
            tme.tm_year = al[0].word_Int(3,".");
            if (!tme.tm_year) tme.tm_year = svyear;

            if (tme.tm_mday>=1 && tme.tm_mday<32 && tme.tm_mon<12 &&
               tme.tm_mon>=0 && tme.tm_year>=1980 && tme.tm_year<=2099)
            {
               tme.tm_year-=1900;
               tmv = mktime(&tme);
               dostime = timetodostime(tmv);
               tm_setdate(dostime);
               if (!setd && !quiet) printf("The new date is: %02d.%02d.%04d\n",
                  tme.tm_mday, tme.tm_mon+1, tme.tm_year+1900);
            }
         }
         if (!dostime) printf("Date format is invalid!\n");
      }
      if (rc<0) rc = 0;
   }

   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return 0;
}

u32t _std shl_time(const char *cmd, str_list *args) {
   int setd=0, quiet=0, rc=-1, idx;
   TPtrStrings al;
   struct tm tme;
   time_t    tmv;

   str_getstrs(args,al);
   // process
   al.TrimEmptyLines();
   if (al.Count()>=1) {
      // is help?
      idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      // process args
      static char *argstr   = "/q|/s";
      static short argval[] = { 1, 1};
      process_args(al, argstr, argval, &quiet, &setd);
   }
   if (al.Count()==0 || al.Count()==1 && !setd) {
      if (al.Count()==0 && !quiet) {
         time(&tmv);
         localtime_r(&tmv,&tme);
         printf("The current time is: %02d:%02d:%02d\n", tme.tm_hour, tme.tm_min,
            tme.tm_sec);
      }

      if (setd) {
         if (al.Count()!=0) rc = EINVAL; else {
            printf("Enter new time in hh:mm:sec format (24 hour time): ");
            char* tms = key_getstr(label_cb);
            printf("\n");
            if (tms) {
               al.Add(tms); al[0].trim();
               // empty string?
               if (!al[0]) rc = 0;
               free(tms);
            } else rc = 0;
         }
      }
      if (rc<0 && al.Count()==1) {
         u32t dostime = 0;
         time(&tmv);
         localtime_r(&tmv,&tme);

         if (al[0].words(":")>=2) {
            tme.tm_hour = al[0].word_Int(1,":");
            tme.tm_min  = al[0].word_Int(2,":");
            tme.tm_sec  = al[0].word_Int(3,":");

            if (tme.tm_hour<24 && tme.tm_min<60 && tme.tm_sec<60 &&
               tme.tm_hour>=0 && tme.tm_min>=0 && tme.tm_sec>=0)
            {
               tmv = mktime(&tme);
               dostime = timetodostime(tmv);
               tm_setdate(dostime);
               if (!setd && !quiet) printf("The new time is: %02d:%02d:%02d\n",
                  tme.tm_hour, tme.tm_min, tme.tm_sec);
            }
         }
         if (!dostime) printf("Time format is invalid!\n");
      }
      if (rc<0) rc = 0;
   }

   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return 0;
}

u32t _std shl_msgbox(const char *cmd, str_list *args) {
   int rc=-1, idx, flags=0, ii;
   TPtrStrings al;

   str_getstrs(args,al);
   // process
   al.TrimEmptyLines();
   if (al.Count()>=1) {
      // is help?
      idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }

      if (al.Count()>=2) {
         static const char *opts[] = { "GRAY", "CYAN", "GREEN", "LGREEN",
            "BLUE", "LBLUE", "RED", "LRED", "WHITE", "CENTER", "LEFT", "RIGHT",
            "JUSTIFY", "WIDE", "OK", "OKCANCEL", "YESNO", "YNCANCEL",
            "DEF1", "DEF2", "DEF3", 0 };
         static u16t values[] = { MSG_GRAY, MSG_CYAN, MSG_GREEN, MSG_LIGHTGREEN,
             MSG_BLUE, MSG_LIGHTBLUE, MSG_RED, MSG_LIGHTRED, MSG_WHITE, MSG_CENTER,
             MSG_LEFT, MSG_RIGHT, MSG_JUSTIFY, MSG_WIDE, MSG_OK, MSG_OKCANCEL,
             MSG_YESNO, MSG_YESNOCANCEL, MSG_DEF1, MSG_DEF2, MSG_DEF3 };

         for (idx=2; idx<al.Count(); idx++) {
            for (ii=0; opts[ii]; ii++)
               if (stricmp(al[idx](),opts[ii])==0) { flags|=values[ii]; break; }
            if (!opts[ii]) { rc = EINVAL; break; }
         }
         if (rc<0) {
            rc = vio_msgbox(al[0](), al[1](), flags, 0);
            set_errorlevel(rc);
            return rc;
         }
      }
   }
   // return 0 (cancel), but display EINVAL error message
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return 0;
}

u32t _std shl_mode(const char *cmd, str_list *args) {
   int rc=-1, idx, flags=0, ii;
   TPtrStrings al;

   str_getstrs(args,al);
   // process
   al.TrimEmptyLines();
   if (al.Count()>=1) {
      // is help?
      idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }

      if (al.Count()>=2) {
         cmd_eproc proc = cmd_modermv(al[0](),0);
         // no proc for this device type - trying to load it
         if (!proc) {
            spstr ename, mdname;
            ename.sprintf("MODE_%s_HANDLER",al[0]());
            ename = getenv(ename());
            ename.trim();
            if (!ename) {
               mdname = ecmd_readstr("MODE", al[0]());
               if (mdname.trim().length()) ename = ecmd_readstr("MODULES", mdname());
            }
            // load module from env. var OR extcmd.ini list
            if (ename.trim().length()) {
               u32t error = 0;
               if (!load_module(ename, &error)) {
                  log_it(2,"unable to load MODE %s handler, error %u\n", al[0](), error);
               } else
                  proc = cmd_modermv(al[0](),0);
            }
         }
         if (!proc) rc = ENODEV;
            else
         return cmd_shellcall(proc, 0, args);
      }
   }
   // return 0 (cancel), but display EINVAL error message
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return 0;
}

u32t _std shl_mode_sys(const char *cmd, str_list *args) {
   int rc = -1;
   TPtrStrings al;
   str_getstrs(args,al);
   al.TrimEmptyLines();
   if (al.Count()>=2) {
      if (al[1].upper()=="STATUS") {
         u32t      rate = 0,
                   host = hlp_hosttype(),
                    cmv = hlp_cmgetstate();
         u16t      port = hlp_seroutinfo(&rate);
         qs_mtlib mtlib = get_mtlib();

         printf("%s host.\n", host==QSHT_EFI?"EFI":"BIOS");
         cmd_printseq("%s mode.", -1, VIO_COLOR_LWHITE,
            host==QSHT_EFI?"64-bit paging":(in_pagemode?"PAE paging":"Flat non-paged"));
         if (mtlib)
            if (mtlib->active()) printf("MT is on.\n");
         if (!port) {
            printf("There is no debug COM port in use now.\n");
         } else {
            printf("Debug COM port address: %04hX, baudrate: %d.\n", port, rate);
         }
         if (cmv && cmv<CPUCLK_MAXFREQ) {
            cmv = 10000/CPUCLK_MAXFREQ*cmv;
            printf("CPU at %u.%2.2u%% speed.\n", cmv/100, cmv%100);
         }
         rc = 0;
      } else
      if (al[1]=="LIST") {
         printf("Unsupported for this device.\n");
         rc = 0;
      } else {
         int ii;
         u32t  port = al.DwordValue("DBPORT"),
               baud = al.DwordValue("BAUD"),
                cmv = al.DwordValue("CM");
         // call this first, else DBCARD will use wrong BAUD
         if (port || baud) {
            if (!hlp_seroutset(port, baud)) printf("Invalid baud rate.\n");
            rc = 0;
         }
         if (cmv) {
             if (!hlp_cmgetstate()) printf("There is no clock modulation in CPU.\n"); else
             if (cmv==0 || cmv>CPUCLK_MAXFREQ) printf("Invalid CM value.\n"); else
             if (hlp_cmsetstate(cmv)) printf("CM setup error.\n");
             rc = 0;
         }
         if (al.IndexOfName("HEAPFLAGS")>=0) {
            spstr mstr = al.Value("HEAPFLAGS");
            if (!mstr) {
               u32t flags = memGetOptions();
               mstr.clear();
               if (flags&QSMEMMGR_PARANOIDALCHK) mstr+="paranoidal, ";
               if (flags&QSMEMMGR_ZEROMEM) mstr+="zero memory, ";
               if (flags&QSMEMMGR_HALTONNOMEM) mstr+="halt on lack of memory";
               if (mstr.length()) {
                  mstr.insert("(",0);
                  if (mstr.lastchar()==' ') mstr.del(mstr.length()-2,2);
                  mstr+=")";
               }
               printf("Current heap flags: %02X %s\n", flags, mstr());
            } else {
               u32t flags = mstr.Dword();
               if ((flags&~(QSMEMMGR_PARANOIDALCHK|QSMEMMGR_ZEROMEM|QSMEMMGR_HALTONNOMEM))) {
                  printf("Invalid heap flags value.\n");
               } else
                  memSetOptions(flags);
            }
            rc = 0;
         }
         if (al.IndexOfName("NORESET")>=0) {
            spstr  mstr = al.Value("NORESET");
            // empty value acts as 1 here
            u32t svmode = !mstr ? 1 : mstr.Dword();

            sto_save(STOKEY_CMMODE, &svmode, 4, 1);
            rc = 0;
         }
         if (al.IndexOfName("DBCARD")>=0) {
            rc = shl_dbcard(al.Value("DBCARD"), printf);
            if (rc!=EINVAL) rc = 0;
         }
         if (al.IndexOfICase("PAE")>=0) {
            int err = pag_enable();
            rc = 0;
            if (err==EEXIST) printf("QSINIT already in paging mode.\n"); else
            if (err==ENOSYS) printf("QSINIT is EFI hosted and paging mode cannot be changed.\n"); else
            if (err==ENODEV) printf("There is no PAE support in CPU unit.\n");
               else rc = err;
         }
         if (al.IndexOfICase("MT")>=0) {
            qs_mtlib mtlib = get_mtlib();
            rc = 0;
            if (!mtlib) printf("MTLIB module is missing.\n"); else {
               u32t err = mtlib->initialize();
               if (err==EINTR)  printf("MT mode is disabled (by set MTLIB).\n"); else
               if (err==EEXIST) printf("QSINIT already in MT mode.\n"); else
               if (err==ENODEV) printf("Not supported on this CPU.\n"); else
               if (err==EBUSY)  printf("Unable to install interrupt vectors.\n");
                  else rc = err;
            }
         }
      }
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return 0;
}

u32t _std shl_autostub(const char *cmd, str_list *args) {
   int rc=-1, idx, flags=0, ii;
   TPtrStrings al;

   str_getstrs(args,al);
   al.TrimEmptyLines();
   // print help without loading module
   if (al.Count()>=1) {
      idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
   }
   // try to load module, assigned to this command
   spstr mdname = ecmd_readstr("commands", cmd);
   if (mdname.trim().length()) {
      spstr mdfname = ecmd_readstr("MODULES", mdname());

      if (mod_query(mdfname(),MODQ_NOINCR)==0) {
         u32t error = 0;
         module *md = load_module(mdfname, &error);
         if (md) {
            /* module must replace our function in ext.cmd shell list,
               query current command processor - if it was changed - call it,
               else fall to "not loaded" message */
            cmd_eproc func = cmd_shellrmv(cmd,0);

            if (func!=shl_autostub) return cmd_shellcall(func, 0, args);
         } else
            log_printf("Error %d on loading \"%s\" handler module \"%s\"\n",
               error, cmd, mdfname());
      }
   }
   printf("Command is not loaded.\n");
   return 0;
}

#if 0
u32t _std shl_log(const char *cmd, str_list *args) {
   int rc=-1, nopause=0, usedate=0, usetime=1, usecolor=1, level=-1, useflags=0;
   if (args->count>0) {
      TPtrStrings al;
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      static char *argstr   = "/f|/d|/np|/nt|/nc";
      static short argval[] = { 1, 1,  1, 0, 0};
      process_args(al, argstr, argval,
                   &useflags, &usedate, &nopause, &usetime, &usecolor);

      idx = al.IndexOfName("/l");
      if (idx>=0) {
         spstr lv(al.Value(idx));
         level = !lv?3:lv.Int();
         al.Delete(idx);
      }

      if (al.Count()>0 && level>=-1 && level<=3) {
         al[0].upper();
         // init "paused" output
         pause_println(0,1);

         if (al[0]=="SAVE" || al[0]=="TYPE") {
            int is_save = al[0][0]=='S';

            if (!is_save || al.Count()==2) {
               u32t flags = LOGTF_DOSTEXT;

               if (usedate) flags|=LOGTF_DATE;
               if (usetime) flags|=LOGTF_TIME;
               if (useflags) flags|=LOGTF_FLAGS;
               if (level>=0) flags|=LOGTF_LEVEL|level; else
               if (usecolor) flags|=LOGTF_LEVEL|LOG_GARBAGE;

               char *cp = log_gettext(flags);
               TPtrStrings ft;
               ft.SetText(cp);
               free(cp);

               if (is_save) rc = ft.SaveToFile(al[1]()) ? 0 : EACCES; else {
                  u32t ii;
                  for (ii=0; ii<ft.Count(); ii++) {
                     spstr pln(ft[ii]);
                     u8t   color = 0;

                     if (usecolor) {
                        int cp = pln.cpos('[');
                        if (cp>=0 && cp<19) {
                           int lp = cp;
                           while (!isdigit(pln[lp]) && pln[lp]) lp++;
                           switch (pln[lp]) {
                              case '0': color = VIO_COLOR_YELLOW; break;
                              case '1': color = VIO_COLOR_LWHITE; break;
                              case '2': color = VIO_COLOR_WHITE; break;
                              case '3': color = VIO_COLOR_CYAN; break;
                           }
                           if (level<0) {
                              int ep = pln.cpos(']', cp);
                              if (ep>0 && ep-cp<=4) pln.del(cp,ep-cp+2);
                           }
                        }
                     }
                     if (pause_println(pln(), nopause?-1:0, color)) return EINTR;
                  }
                  rc = 0;
               }
            }
         } else
         if (al[0]=="CLEAR") {
            log_clear();
            rc = 0;
         }
      }
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return 0;
}
#endif

u32t _std shl_attrib(const char *cmd, str_list *args) {
   TPtrStrings al;
   int rc=-1, subdir=0, nopause=0, mask=0, idirs=0, a_R=0, a_A=0, a_S=0,
       a_H=0, quiet=0, idx;

   if (args->count>0) {
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      // process args
      static char *argstr   = "/s|/d|/np|+r|-r|+s|-s|+h|-h|+a|-a|/q";
      static short argval[] = { 1, 1,  1, 1,-1, 1,-1, 1,-1, 1,-1, 1};

      process_args(al, argstr, argval,
                   &subdir, &idirs, &nopause, &a_R, &a_R, &a_S, &a_S,
                   &a_H, &a_H, &a_A, &a_A);
      al.TrimEmptyLines();
   }
   if (!al.Count()) al.Add("*");
   // init "paused" output
   pause_println(0,1);
   // process multiple paths
   for (idx=0; idx<al.Count(); idx++) {
      // fix and split path
      spstr pp(al[idx]), drive, dir, name, ext;
      splitpath(pp,drive,dir,name,ext);

      mask = name.cpos('*')>=0 || name.cpos('?')>=0;

      int  error = 0;
      dir_t   fi;

      if (!mask) {
         if (_dos_stat(pp(),&fi)) {
            if (!subdir) error = ENOENT;
         } else
         if (fi.d_attr&_A_SUBDIR) {
            drive = pp;
            name  = "*.*";
            mask  = true;
         }
      }
      if (!error) {
         if (!drive) drive = hlp_curdir(hlp_curdisk()); else {
            char *fp = _fullpath(0,drive(),0);
            fp[0] = fp[0]-'A'+'0';
            drive = fp;
            free(fp);
         }
         TStrings  flist;
         spstr     dname(drive);
         if (dname.length() && dname.lastchar()!='\\') dname+="\\";
         dname+=name;

         if (mask || subdir) {
            // read tree
            dir_t *info = 0;
            u32t cnt = _dos_readtree(drive(),name(),&info,subdir,0,0);
            if (info) {
               if (cnt) dir_to_list(drive, info, (idirs?0:_A_SUBDIR)|
                  _A_VOLID, flist, 0);
               _dos_freetree(info);
            }
         } else
            flist.AddObject(dname,fi.d_attr);

         if (flist.Count()) {
            int ii;
            u32t andmask = FFFF, ormask = 0;
            if (a_R||a_A||a_S||a_H) {
               if (a_R) {
                  andmask&=~_A_RDONLY; if (a_R>0) ormask|=_A_RDONLY;
               }
               if (a_H) {
                  andmask&=~_A_HIDDEN; if (a_H>0) ormask|=_A_HIDDEN;
               }
               if (a_A) {
                  andmask&=~_A_ARCH;   if (a_A>0) ormask|=_A_ARCH;
               }
               if (a_S) {
                  andmask&=~_A_SYSTEM; if (a_S>0) ormask|=_A_SYSTEM;
               }
            }

            spstr msg;
            for (ii=0; ii<flist.Count(); ii++) {
               unsigned attrs = _A_NORMAL;
               _dos_getfileattr(flist[ii](), &attrs);

               if (andmask!=FFFF) {
                  u32t err = _dos_setfileattr(flist[ii](), attrs&andmask|ormask);
                  if (!err || quiet) msg.clear(); else
                     msg.sprintf("Unable to set attributes for \"%s\". Error %d",
                        flist[ii](), err);
               } else {
                  msg.sprintf("%c%c%c%c%c  %s", attrs&_A_ARCH?'A':' ',
                     attrs&_A_RDONLY?'R':' ', attrs&_A_HIDDEN?'H':' ',
                        attrs&_A_SYSTEM?'S':' ', attrs&_A_SUBDIR?'D':' ',
                           flist[ii]());
               }
               if (msg.length())
                  if (pause_println(msg(),nopause?-1:0)) return EINTR;
            }
         }
      }
   }
   if (rc<0) rc = 0;
   if (rc) cmd_shellerr(rc,0);
   return rc;
}

// free pushd stack
void pushd_free(void* lptr) {
   if (lptr) delete (TStrings*)lptr;
}

u32t _std shl_pushd(const char *cmd, str_list *args) {
   int quiet=0, rc=-1;
   TPtrStrings al;

   if (args->count>0) {
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      idx = al.IndexOfName("/q");
      if (idx>=0) { quiet=1; al.Delete(idx); }
   }
   if (al.Count()<=1) {
      process_context *pq = mod_context();
      // GS register damaged?
      if (!pq) rc = EFAULT; else {
         char cd[NAME_MAX+1];
         TStrings *pdlist = (TStrings*)pq->rtbuf[RTBUF_PUSHDST];
         if (!pdlist) pdlist = new TStrings;

         getcwd(cd, NAME_MAX+1);
         pdlist->Add(cd);
         pq->rtbuf[RTBUF_PUSHDST] = (u32t)pdlist;

         if (al.Count()==1) rc = chdir_int(al[0]());
         if (rc<0) rc = 0;
      }
   }
   if (quiet) return rc;
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return 0;
}

u32t _std shl_popd(const char *cmd, str_list *args) {
   int quiet=0, rc=-1;
   TPtrStrings al;

   if (args->count>0) {
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      idx = al.IndexOfName("/q");
      if (idx>=0) { quiet=1; al.Delete(idx); }
   }
   // get PUSHD stack from process context data
   process_context *pq = mod_context();
   // GS register damaged?
   if (!pq) rc = EFAULT; else {
      TStrings *pdlist = (TStrings*)pq->rtbuf[RTBUF_PUSHDST];
      if (pdlist)
         if (pdlist->Count()) {
            rc = chdir_int((*pdlist)[pdlist->Max()]());
            pdlist->Delete(pdlist->Max());
         }
      if (rc<0) rc = 0;
   }
   if (quiet) return rc;
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return 0;
}

u32t _std shl_ansi(const char *cmd, str_list *args) {
   int quiet=0, rc=-1;
   TPtrStrings al;

   if (args->count>0) {
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
   }
   if (al.Count()>0) {
      if (al[0].upper()=="ON") { vio_setansi(1); rc = 0; } else
      if (al[0]=="OFF") { vio_setansi(0); rc = 0; }
   } else {
      printf("ANSI now is %s.\n", vio_setansi(-1)?"on":"off");
      rc = 0;
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return 0;
}

u32t _std shl_move(const char *cmd, str_list *args) {
   int quiet=0, rc=-1;
   if (args->count>0) {
      TPtrStrings al;
      str_getstrs(args,al);
      // is it help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      idx = al.IndexOfName("/q");
      if (idx>=0) { quiet=1; al.Delete(idx); }

      rc = processRenMove(al, 0, quiet);
   }
   if (rc<0) cmd_shellerr(rc = EINVAL,0);
   return rc;
}

u32t _std shl_reboot(const char *cmd, str_list *args) {
   int quiet=0, rc=-1, warm=1;
   TPtrStrings al;

   if (args->count>0) {
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      // process args
      static char *argstr   = "/q|WARM|COLD";
      static short argval[] = { 1,   1,   0};
      process_args(al, argstr, argval, &quiet, &warm, &warm);
      al.TrimEmptyLines();
   }
   if (al.Count()==0) {
      int yn = 1;
      if (!quiet) {
         printf("Reboot (y/n)?");
         yn = ask_yn();
      }
      if (yn==1) exit_reboot(warm);
      rc = 0;
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return 0;
}

#include "zz.cpp"
#include "shpci.cpp"

extern "C"
void setup_shell(void) {
   // install shell commands
   cmd_shelladd("COPY"   , shl_copy   );
   cmd_shelladd("TYPE"   , shl_type   );
   cmd_shelladd("DIR"    , shl_dir    );
   cmd_shelladd("RESTART", shl_restart);
   cmd_shelladd("HELP"   , shl_help   );
   cmd_shelladd("MKDIR"  , shl_mkdir  );
   cmd_shelladd("MD"     , shl_mkdir  );
   cmd_shelladd("CHDIR"  , shl_chdir  );
   cmd_shelladd("CD"     , shl_chdir  );
   cmd_shelladd("DEL"    , shl_del    );
   cmd_shelladd("ERASE"  , shl_del    );
   cmd_shelladd("BEEP"   , shl_beep   );
   cmd_shelladd("RMDIR"  , shl_rmdir  );
   cmd_shelladd("RD"     , shl_rmdir  );
   cmd_shelladd("UNZIP"  , shl_unzip  );
   cmd_shelladd("LM"     , shl_loadmod);
   cmd_shelladd("LOADMOD", shl_loadmod);
   cmd_shelladd("POWER"  , shl_power  );
   cmd_shelladd("LABEL"  , shl_label  );
   cmd_shelladd("TRACE"  , shl_trace  );
   cmd_shelladd("REN"    , shl_ren    );
   cmd_shelladd("DATE"   , shl_date   );
   cmd_shelladd("TIME"   , shl_time   );
   cmd_shelladd("MSGBOX" , shl_msgbox );
   cmd_shelladd("MODE"   , shl_mode   );
//   cmd_shelladd("LOG"    , shl_log    );
   cmd_shelladd("ATTRIB" , shl_attrib );
   cmd_shelladd("PUSHD"  , shl_pushd  );
   cmd_shelladd("POPD"   , shl_popd   );
   cmd_shelladd("ANSI"   , shl_ansi   );
   cmd_shelladd("MOVE"   , shl_move   );
   cmd_shelladd("REBOOT" , shl_reboot );

   // install MODE SYS handler
   cmd_modeadd("SYS", shl_mode_sys);
   /* install stubs for pre-defined external commands */
   TStrings autoload;
   ecmd_commands(autoload);
   for (int ii=0; ii<autoload.Count(); ii++) cmd_shelladd(autoload[ii](), shl_autostub);
}
