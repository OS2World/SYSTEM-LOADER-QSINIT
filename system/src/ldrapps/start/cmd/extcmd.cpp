//
// QSINIT "start" module
// external shell commands
//
#include "qsbase.h"
#include "stdlib.h"
#include "qs_rt.h"
#include "errno.h"
#include "vioext.h"
#include "syslocal.h"
#include "time.h"
#include "qsint.h"
#include "qsdm.h"
#include "stdarg.h"

#define SPACES_IN_WIDE_MODE       4      ///< spaces between names in wide "dir" mode
#define PRNSEQ_NP        (PRNSEQ_INITNOPAUSE^PRNSEQ_INIT)

void cmd_shellerr(u32t errtype, int errorcode, const char *prefix) {
   if (!prefix) prefix = "\r";

   char *msg = cmd_shellerrmsg(errtype, errorcode);
   if (!msg)
      msg = sprintf_dyn(errtype?"Error code %d.":"Error code %X.", errorcode);
   // enable ANSI while help message printing...
   u32t state = vio_setansi(1);
   printf("%s%s\n", prefix, msg);
   free(msg);
   if (!state) vio_setansi(0);
}

char* _std cmd_shellerrmsg(u32t errtype, int errorcode) {
   char msgn[16];
   snprintf(msgn, 16, errtype?"_C_%05d":"_E_%06X", errorcode);
   return cmd_shellgetmsg(msgn);
}

static int pause_check(int init_counter) {
   u32t  pp_lines,
        pp_lstart = mt_tlsget(QTLS_SHLINE),
         pp_flags = mt_tlsget(QTLS_SHFLAGS);
   vio_getmodefast(0, &pp_lines);

   if (init_counter>=0 && (pp_flags&PRNSEQ_NP)==0 && 
      (int)(vio_ttylines(-1)-pp_lstart)>=(int)pp_lines-1) 
   {
      vio_setcolor(VIO_COLOR_GRAY);
      vio_strout("Press any key to continue...");
      vio_setcolor(VIO_COLOR_RESET);

      u16t key = key_read();
      // exit by ESC
      if ((key&0xFF)==27) { vio_charout('\n'); return 1; }

      // write gray text & attribute
      vio_setcolor(VIO_COLOR_WHITE);
      vio_strout("\r                            \r");
      vio_setcolor(VIO_COLOR_RESET);
      mt_tlsset(QTLS_SHLINE, vio_ttylines(-1));
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
      mt_tlsset(QTLS_SHLINE, vio_ttylines(-1));
      mt_tlsset(QTLS_SHFLAGS, init_counter);
   }
   if (!line) return 0;
   u32t  pp_flags = mt_tlsget(QTLS_SHFLAGS);
   if (pp_flags&PRNSEQ_LOGCOPY) log_it(2,"echo: %s\n", line);

   FILE     *stdo = get_stdout();
   int    p_check = init_counter>=0 && (pp_flags&PRNSEQ_NP)==0 && isatty(fileno(stdo));
   if (p_check) {
      const char *lp = line;
      while (*lp) {
         char *eolp = strchr(lp, '\n');
         int    len = eolp?eolp-lp+1:strlen(lp);

         if (pause_check(init_counter)) return 1;

         if (color) vio_setcolor(color);
         fwrite(lp, 1, len, stdo);
         lp += len;
      }
   } else {
      if (color) vio_setcolor(color);
      fputs(line, stdo);
   }
   if (color) vio_setcolor(VIO_COLOR_RESET);
   fputc('\n', stdo);
   return 0;
}

int __cdecl cmd_printf(const char *fmt, ...) {
   if (!fmt) return 0;
   va_list  argptr;
   va_start(argptr, fmt);
   char *outstr = vsprintf_dyn(fmt, argptr);
   va_end(argptr);
   int    prnlen = strlen(outstr);
   FILE    *stdo = get_stdout();
   u32t pp_flags = mt_tlsget(QTLS_SHFLAGS);
   fputs(outstr, stdo);
   if (pp_flags&PRNSEQ_LOGCOPY) log_it(2,"%s", outstr);
   free(outstr);
   if ((pp_flags&PRNSEQ_NP)==0 && isatty(fileno(stdo))) pause_check(0);
   return prnlen;
}

u32t __cdecl cmd_printseq(const char *fmt, int flags, u8t color, ...) {
   if (!fmt) return pause_println(0, flags, color);
   va_list argptr;
   va_start(argptr, color);
   char *outstr = vsprintf_dyn(fmt, argptr);
   va_end(argptr);
   int   prnlen = strlen(outstr);
   u32t     res = pause_println(outstr, flags, color);
   free(outstr);
   return res;
}

int ask_yn(int allow_all, int allow_skip) {
   u16t chv = 0;
   while (1) {
      chv = key_read();
      char cch = toupper(chv&0xFF);
      if (allow_skip && cch=='S') return 3;
      if (allow_all && cch=='A') return 2;
      if (cch=='Y') return 1;
      if (cch=='N') return 0;
      if (cch==27)  return -1;
   }
}

static void process_args_common(TPtrStrings &al, u32t flags, char* args,
   short *values, va_list argptr)
{
   char  arg[144], *ap = args;
   int   noslash = flags&SPA_NOSLASH ? 1 : 0;
   // a bit of safeness
   if (strlen(args)>=144) { log_it(3, "too long args!"); return; }

   while (*ap) {
      char *ep = strchr(ap,'|');
      int  *vp = va_arg(argptr,int*);
      if (noslash) arg[0] = '/';
      if (ep) {
         strncpy(arg+noslash, ap, ep-ap);
         arg[ep-ap+noslash] = 0;
         ap = ep+1;
      } else {
         strcpy(arg+noslash, ap);
         ap += strlen(ap);
      }
      int idx = al.IndexOfName(arg);
      if (idx>=0) { *vp = *values; al.Delete(idx); }
      // accept '-key' as well as '/key'
      if (arg[0]=='/' && !(flags&SPA_NODASH)) {
         arg[0] = '-';
         idx = al.IndexOfName(arg);
         if (idx>=0) { *vp = *values; al.Delete(idx); }
      }
      values++;
   }
}

static void args2list(str_list *lst, TPtrStrings &al) {
   str_getstrs(lst,al);
}

void process_args(TPtrStrings &al, u32t flags, char* args, short *values, ...) {
   va_list   argptr;

   va_start(argptr, values);
   process_args_common(al, flags, args, values, argptr);
   va_end(argptr);
}

str_list* str_parseargs(str_list *lst, u32t firstarg, u32t flags, char* args,
   short *values, ...)
{
   va_list   argptr;
   TPtrStrings   al;

   str_getstrs2(lst,al,firstarg);
   va_start(argptr, values);
   process_args_common(al, flags, args, values, argptr);
   va_end(argptr);
   return flags&SPA_RETLIST ? str_getlist_local(al.Str) : 0;
}

#define AllowSet " ^\r\n"
#define CarrySet "-"

static u32t cmd_printtext(TStrings &res, int flags, u8t color) {
   if (flags!=0 && flags!=-1 && (flags&PRNSEQ_INIT)) {
      pause_println(0, flags);
      flags = flags&PRNSEQ_NP?-1:0;
   }
   if (pause_println(res.GetTextToStr().dellast()(), flags, color)) return 1;
   return 0;
}

u32t _std cmd_printtext(const char *text, int flags, u8t color) {
   if (!text) return 0;
   TStrings  res;
   u32t   dX, ln;
   int        np = flags==-1,
        helpmode = !np && (flags&PRNTXT_HELPMODE);
   /* "flags" is a bit crazy arg: -1 here is a special value, but bit flags are
      used too. Dumb way, but pause_println() used too often to change it */
   if (helpmode) flags &= ~PRNTXT_HELPMODE;

   vio_getmode(&dX,0);

   splittext(text, dX, res, helpmode?SplitText_HelpMode:0);
   return cmd_printtext(res, flags, color);
}

qserr _std cmd_printfile(qshandle handle, long len, int flags, u8t color) {
   if (!len) return E_SYS_INVPARM;
   // query file first
   u64t  fsz;
   qserr res = io_size(handle, &fsz);
   if (res) return res;
   u64t fpos = io_pos(handle);
   if (fpos==FFFF64) return io_lasterror(handle);
   // the same note as in cmd_printtext() above
   int split = flags!=-1 && (flags&PRNFIL_SPLITTEXT);
   if (split) flags &= ~PRNFIL_SPLITTEXT;
   // validate data length
   if (len<0) len = fsz-fpos>x7FFF ? x7FFF : fsz - fpos; else
      if (fsz-fpos < len) len = fsz-fpos;
   if (!len) return E_SYS_EOF;

   char *txbuf = (char*)hlp_memalloc(len+1, QSMA_RETERR|QSMA_NOCLEAR|QSMA_LOCAL);
   if (!txbuf) return E_SYS_NOMEM;

   if (io_read(handle, txbuf, len) < len) res = E_DSK_ERRREAD; else {
      if (txbuf[len-1]=='\n') len--;
      if (len && txbuf[len-1]=='\r') len--;
      if (len) {
         txbuf[len] = 0;
         if (split) {
            if (cmd_printtext(txbuf, flags, color)) res = E_SYS_UBREAK;
         } else {
            if (flags!=0 && flags!=-1 && (flags&PRNSEQ_INIT)) {
               pause_println(0, flags);
               flags = flags&PRNSEQ_NP?-1:0;
            }
            if (pause_println(txbuf, flags, color)) res = E_SYS_UBREAK;
         }
      }
   }
   hlp_memfree(txbuf);
   return res;
}


static void _std freadfull_callback(u32t percent, u32t readed) {
   char *cp = (char*)mt_tlsget(QTLS_SFINT1);
   if (cp) {
      u32t cols = 80,
            len = strlen(cp);
      vio_getmode(&cols, 0);
      // we need to cut long file name (once per load)
      if (len>cols-24) {
         char *dp = cp+len-cols+24+3;
         cp[0]='.'; cp[1]='.'; cp[2]='.';
         memmove(cp+3, dp, strlen(dp)+1);
      }
      if (percent)
         printf("\rcaching %s ... %02u%% ", cp, percent); else
      if ((readed & _1MB-1)==0)
         printf("\rcaching %s ... %u Mb ", cp, readed>>20);
   }
}


void* hlp_freadfull_progress(const char *name, u32t *bufsize) {
   char *srcname = 0;
   mt_tlsset(QTLS_SFINT1, (u32t)(srcname = strdup(name)));
   void *ptr = hlp_freadfull(name, bufsize, freadfull_callback);
   free(srcname);
   printf("\n");
   return ptr;
}

u32t _std shl_copy(const char *cmd, str_list *args) {
   char  *errstr = 0;
   int     quiet = 0, rc = -1;
   u32t  errtype = EMSG_CLIB;

   if (args->count>0) {
      int frombp = 0, cpattr = 0, beep = 0;

      TPtrStrings al;
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      // process args
      static char *argstr   = "boot|q|a|beep";
      static short argval[] = {   1,1,1,   1};
      process_args(al, SPA_NOSLASH, argstr, argval, &frombp, &quiet, &cpattr, &beep);

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
               u32t  *bt_len = frombp? new u32t[al.Count()] : 0,
                     bt_type = hlp_boottype(), opena,
                       bt_vi = frombp? hlp_volinfo(DISK_BOOT, 0) : FST_NOTMOUNTED;
               time_t sftime = 0,
                      sctime = 0;
               // zero source pointers/handles
               for (idx=0; idx<al.Count(); idx++) al.Objects(idx) = 0;
               rc = 0;
               for (idx=0;idx<al.Count();idx++) {
                  is_boot[idx] = 0;
                  // try to allow directories in micro-FSD
                  if (frombp)
                     if (al[idx][1]!=':' /*&& al[idx].cpos('\\')<0 &&
                        al[idx].cpos('/')<0*/) is_boot[idx] = 1;
                  /* switch it to normal file i/o on FAT (if mounted one, for
                     HPFS/JFS micro-FSD is still used to have an alternative
                     way of FS access) */
                  if (is_boot[idx] && bt_vi>=FST_FAT12 && bt_vi<FST_OTHER) {
                     al[idx].insert("A:\\",0);
                     is_boot[idx] = 0;
                  }
                  // from boot partition?
                  if (is_boot[idx]) {
                     if (bt_type==QSBT_PXE) {
                        void *ptr = quiet?hlp_freadfull(al[idx](), bt_len+idx, 0):
                                          hlp_freadfull_progress(al[idx](), bt_len+idx);
                        if (!ptr) { rc=ENOENT; break; }
                        al.Objects(idx) = ptr;
                     } else {
                        // only check presence here
                        if (hlp_fopen(al[idx]())==FFFF) { rc=ENOENT; break; }
                        hlp_fclose();
                        al.Objects(idx) = 0;
                     }
                  } else {
                     // get time of first source for target
                     if (cpattr && !idx) {
                        dir_t fi;
                        if (!_dos_stat(al[idx](),&fi)) {
                           sftime = fi.d_wtime;
                           sctime = fi.d_ctime;
                           /* creation time zeroed by OS/2 on FAT and often
                              can be equal, flag it as missing too in the
                              second case */
                           if (sftime==sctime) sctime = 0;
                           sfattr = fi.d_attr;
                        }
                     }
                     // open source file
                     io_handle  ff = 0;
                     rc = io_open(al[idx](), IOFM_OPEN_EXISTING|IOFM_READ|
                                  IOFM_SHARE_READ|IOFM_SHARE_DEL|IOFM_SHARE_REN, &ff, &opena);
                     if (rc) {
                        errtype = EMSG_QS;
                        errstr  = sprintf_dyn("%s : ", al[idx]());
                        break;
                     }
                     al.Objects(idx) = (void*)ff;
                  }
               }
               if (!rc) {
                  io_handle dstf = 0;
                  // close on del on destination! will be reset on success
                  rc = io_open(dst(), IOFM_CREATE_ALWAYS|IOFM_WRITE|IOFM_SHARE_READ|
                               IOFM_CLOSE_DEL, &dstf, &opena);
                  if (rc) {
                     io_handle_info  di;
                     if (io_pathinfo(dst(), &di)==0) {
                        // is this a dir? try to merge file name to it
                        if (di.attrs&IOFA_DIR) {
                           spstr  name, ext;
                           _splitpath(al[0](),0,0,name.LockPtr(QS_MAXPATH+1), ext.LockPtr(QS_MAXPATH+1));
                           name.UnlockPtr(); ext.UnlockPtr();
                           dst += "\\";
                           dst += name+ext;
                           rc   = io_open(dst(), IOFM_CREATE_ALWAYS|IOFM_WRITE|
                              IOFM_SHARE_READ|IOFM_CLOSE_DEL, &dstf, &opena);
                        } else
                        // is it a device? IOFM_CLOSE_DEL is not acceptable
                        if (di.attrs&IOFA_DEVICE)
                           rc   = io_open(dst(), IOFM_CREATE_ALWAYS|IOFM_WRITE|
                              IOFM_SHARE_READ|IOFM_SHARE_WRITE, &dstf, &opena);
                     }
                  }

                  if (rc) {
                     errtype = EMSG_QS;
                     errstr  = sprintf_dyn("%s : ", dst());
                  } else {
                     u32t memsize = _128KB, maxblock;
                     hlp_memavail(&maxblock, 0);
                     // 1-2Mb buffer
                     if (maxblock>_64MB) memsize<<=4; else
                        if (maxblock>_32MB) memsize<<=3;
                     void *buf = hlp_memalloc(memsize,QSMA_RETERR|QSMA_LOCAL);
                     if  (!buf) rc = ENOMEM;

                     idx = 0;
                     while (idx<al.Count() && !rc) {
                        if (!quiet) printf("%s ",al[idx]());
                        if (is_boot[idx] && bt_type==QSBT_PXE) {
                           if (bt_len[idx])
                              if (io_write(dstf, al.Objects(idx), bt_len[idx]) != bt_len[idx]) {
                                 errtype = EMSG_QS;
                                 rc = io_lasterror(dstf);
                              }
                        } else {
                           io_handle sf = (io_handle)al.Objects(idx);
                           u64t     len, pos = 0;

                           if (sf) {
                              rc = io_size(sf, &len);
                              if (rc) errtype = EMSG_QS;
                           } else {
                              len = hlp_fopen(al[idx]());
                              if (len==FFFF) rc = EIO;
                           }
                           if (rc) break;

                           while (len) {
                              u32t copysize = len<memsize?len:memsize;
                              u32t   actual = sf?io_read(sf, buf, copysize):
                                              hlp_fread(pos, buf, copysize);
                              if (actual!=copysize) { rc = EIO; break; }
                              actual = io_write(dstf, buf, copysize);
                              if (actual!=copysize) {
                                 errtype = EMSG_QS;
                                 rc = io_lasterror(dstf);
                              }
                              len-=copysize; pos+=copysize;
                              // allow Ctrl-Esc during very long file copying
                              if (!mt_active()) key_pressed();
                           }
                           if (!sf) hlp_fclose();
                        }
                        if (!quiet) printf("\n");
                        idx++;
                     }
                     // we have no error - then leave file on its place
                     if (!rc) {
                        io_setstate(dstf, IOFS_DELONCLOSE, 0);
                        rc = io_close(dstf);
                     } else
                        io_close(dstf);
                     // creation time can be 0
                     if (!rc && cpattr && sftime) {
                        io_handle_info  hi;
                        hi.attrs = sfattr;
                        io_timetoio(&hi.wtime, sftime);
                        io_timetoio(&hi.ctime, sctime?sctime:sftime);
                        io_setinfo(dst(), &hi, IOSI_ATTRS|IOSI_CTIME|IOSI_WTIME);
                     }
                     if (buf) hlp_memfree(buf);
                  }
               }
               for (idx=0;idx<al.Count();idx++)
                  if (al.Objects(idx))
                     if (is_boot[idx]) hlp_memfree(al.Objects(idx));
                        else io_close((io_handle)al.Objects(idx));
               delete bt_len;
               delete is_boot;
            }
         }
         if (rc>0) log_it(3, "copy err %X, file \"%s\"\n", rc, dst());
         if (beep) {
            vio_beep(rc>0?200:700,40);
            while (vio_beepactive()) usleep(5000);
            vio_beep(rc>0?100:600,40);
         }
      }
   }
   if (rc<0) rc = EINVAL;
   if (!quiet && rc) cmd_shellerr(errtype, rc, errstr);
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
      static char *argstr   = "boot|np";
      static short argval[] = {   1, 1};
      process_args(al, SPA_NOSLASH, argstr, argval, &frombp, &nopause);

      al.TrimEmptyLines();
      if (al.Count()>=1) {
         u32t bt_vi = frombp? hlp_volinfo(DISK_BOOT, 0) : FST_NOTMOUNTED;
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
            if (is_boot)
               if (bt_vi!=FST_NOTMOUNTED) {
                  name.insert("A:\\",0);
                  is_boot = 0;
               } else
                  f_data = (char*)hlp_freadfull(name(), &f_size, 0);
            // non-boot source
            if (!is_boot) f_data = (char*)freadfull(name(), &f_size);
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
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
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
   u32t      dir_cnt;
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
   cbi->sizetotal += fp->d_size>>10;
   if ((fp->d_size&_1KB-1)) cbi->sizetotal++;

   // full relative name
   spstr pp;
   dir_mkrelpath(cbi, fp, pp);
   // insert date & size
   if (!cbi->shortfmt) {
      struct tm tme;
      char      buf[64];
      if (fp->d_attr&_A_SUBDIR) strcpy(buf, "   <DIR>     ");
         else sprintf(buf," %10Lu  ", fp->d_size);
      pp.insert(buf,0);
      // cvt selected time
      localtime_r(cbi->crtime?&fp->d_ctime:&fp->d_wtime, &tme);
      // format time
      sprintf(buf,"%02d.%02d.%04d  %02d:%02d ", tme.tm_mday, tme.tm_mon+1,
         tme.tm_year+1900, tme.tm_hour, tme.tm_min);
      pp.insert(buf,0);
   }
   return readtree_prn(cbi, pp);
}

static void dir_total(unsigned drive, dir_cbinfo *cbi) {
   if (!cbi->shortfmt && cbi->inited) {
      spstr sum;
      if (cbi->file_cnt) {
         u32t fcnt = cbi->file_cnt-cbi->dir_cnt,
              dcnt = cbi->dir_cnt+1;
         sum.sprintf(dcnt>1 ? "\n%s in %u file%s and %u director%s" : 
            "\n%s in %u file%s", dsk_formatsize(1024,cbi->sizetotal,10,0),
               fcnt, fcnt>1?"s":"", dcnt, dcnt>1?"ies":"y");
         pause_println(sum(), cbi->dir_npmode);
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
         if (pd[ii].d_subdir) {
            spstr dirname;
            dir_mkrelpath(cbi, pd+ii, dirname);
            dstack.AddObject(dirname, pd[ii].d_subdir);
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

      cbi->sizetotal += pd[ii].d_size>>10;
      if ((pd[ii].d_size&_1KB-1)) cbi->sizetotal++;

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
   static char *argstr   = "s|np|b|tc|w";
   static short argval[] = {1, 1,1, 1,1};
   process_args(al, SPA_NOSLASH, argstr, argval, &subdirs, &nopause, &shortfmt,
                &crtime, &wide);

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
      cbi.dir_cnt    = 0;
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
            if (!drive) getcurdir(drive); else {
               char *fp = _fullpath(0,drive(),0);
               drive    = fp;
               free(fp);
            }
            // print header on dir mismatch only
            if (!cbi.shortfmt && (!idx||stricmp(drive(),al[idx-1]())!=0)) {
               disk_volume_data vinf;
               spstr            head;
               int           needeol = cbi.inited; // eol beteen total & next dir header
               dir_total(ldrive, &cbi);

               hlp_volinfo(drive[0]-'A', &vinf);
               if (vinf.FsVer==0) {
                  head.sprintf("File system in drive %c is not recognized\n", drive[0]);
               } else {
                  int lbon = vinf.Label[0];
                  head.sprintf("  Directory of %s\n"
                               "  Volume in drive %c %ss %s\n"
                               "  Volume Serial Number is %04X-%04X\n", drive(),
                                  drive[0], lbon?"i":"ha", lbon?vinf.Label:"no label",
                                  vinf.SerialNum>>16,vinf.SerialNum&0xFFFF);
               }
               if (needeol) head.insert("\n",0);
               if (pause_println(head(),cbi.dir_npmode)) return EINTR;
               if (vinf.FsVer==0) continue;
            }
            dir_t *fdl = 0;
            // update for comparing above
            al[idx] = drive;
            /* read tree:
               * in normal mode - one by one, via callback
               * in wide mode - read all, then print */
            u32t count = _dos_readtree(drive(), name(), wide?&fdl:0, subdirs,
                                       &cbi.dir_cnt, wide?0:readtree_cb, &cbi);
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
               // nobody home? checks directory and set error
               if (!count && cbi.ppath_len==-1) {
                  dir_t info;
                  if (_dos_stat(drive(), &info)) error = ENOENT; else
                     if ((info.d_attr&_A_SUBDIR)==0) error = ENOTDIR;
               }
               // last cycle print
               ldrive = drive[0]-'A';
               if (rc<0 && !error && idx==al.Max()) dir_total(ldrive, &cbi);
            }
         }
         if (error) cmd_shellerr(EMSG_CLIB,rc=error,0);
      }
      if (rc<0) rc=0;
   } else {
      if (rc<0) rc = EINVAL;
      if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
   }
   return rc;
}

u32t _std shl_restart(const char *cmd, str_list *args) {
   int rc=-1, nosend=0;
   if (args->count>0) {
      TPtrStrings al;
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      // process args
      static char *argstr   = "/nosend";
      static short argval[] = { 1 };
      process_args(al, 0, argstr, argval, &nosend);

      // process
      al.TrimEmptyLines();
      if (al.Count()>=1) {
         qserr res;
         if (hlp_hosttype()==QSHT_EFI) res = E_SYS_EFIHOST; else {
            spstr  ldr(al[0]);
            char  *env = 0;
            al.Delete(0);
            // we have keys!
            if (al.Count()) {
               str_list *lst = str_getlist(al.Str);
               env = env_create(lst, 0);
               free(lst);
            }
            res = exit_restart((char*)ldr(), env, nosend);
            if (env) free(env);
         }
         // we should never reach this point by default, so print it always
         cmd_shellerr(EMSG_QS, res, 0);
         rc = 0;
      }
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
   return rc;
}

int _std cmd_shellhelp(const char *cmd, u8t color) {
   char *msg = cmd_shellgetmsg(cmd);
   if (!msg) return 0;
   // enable ANSI for the time of help message printing...
   ansi_push_set a_on(1);

   cmd_printtext(msg, PRNSEQ_INIT|PRNTXT_HELPMODE, color);
   free(msg);

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
      cmd_printtext("Help:\n\nHELP [command]\n", PRNSEQ_INIT, CLR_HELP);
      spstr cmlist(list);
      free(list);
      cmlist.insert("available commands: ",0);
      cmd_printtext(cmlist(), 0, CLR_HELP);
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
            cmd_printtext(msg(), PRNSEQ_INIT, CLR_HELP);
         }
         rc = 0;
      }
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
   return rc;
}

u32t _std shl_mkdir(const char *cmd, str_list *args) {
   u32t rc = E_SYS_INVPARM;
   if (args->count>0) {
      TPtrStrings al;
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      // process
      al.TrimEmptyLines();
      if (al.Count()>=1) {
         rc = io_mkdir(al[0]());
         if (rc>0) log_it(3, "mkdir err %d, dir \"%s\"\n", rc, al[0]());
      }
   }
   if (rc) cmd_shellerr(EMSG_QS,rc,0);
   return qserr2errno(rc);
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
      if (disk>='0' && disk<='9') disk+='A'-'0';
      io_setdisk(disk - 'A');
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
      spstr cdir;
      getcurdir(cdir);
      printf("%s\n", cdir());
      rc = EZERO;
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
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
            stack.AddObject(cname, di->d_subdir);
            if (dirs) dirs->AddObject(cname, di->d_attr);
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
          edirs=0, breakable=1, fileonly=-1, ignore=0;

      TPtrStrings al;
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      // process args
      static char *argstr   = "p|f|s|q|e|qn|np|nb|ad|af|i";
      static short argval[] = {1,1,1,1,1, 1, 1, 0, 0, 1,1};
      process_args(al, SPA_NOSLASH, argstr, argval,
                   &askall, &force, &subdir, &quiet, &edirs, &nquiet,
                   &nopause, &breakable, &fileonly, &fileonly, &ignore);

      al.TrimEmptyLines();
      if (al.Count()>=1) {
         u32t fcount = 0;
         // init "paused" output
         pause_println(0,1);
         // delete files
         for (idx=0; idx<al.Count(); idx++) {
            dir_t   fi;
            spstr  dir, name, pp(al[idx]);
            
            if (!fullpath(pp)) rc = get_errno(); else {
               splitfname(pp, dir, name);
               mask = name.cpos('*')>=0 || name.cpos('?')>=0;

               if (!mask) {
                  if (_dos_stat(pp(), &fi)) rc = ENOENT; else
                  if (fi.d_attr&_A_SUBDIR) {
                     if (fileonly>0) rc = EISDIR; else {
                        dir   = pp;
                        name  = "*.*";
                        mask  = true;
                     }
                  } else
                  if (fileonly==0) rc = ENOTDIR;
               }
            }
            if (rc>0) {
               if (ignore) {
                  spstr np(pp); np+=": ";
                  cmd_shellerr(EMSG_CLIB, rc, np());
                  rc = 0;
                  continue;
               } else break;
            }
            if (dir.length() && dir.lastchar()!='\\') dir+="\\";
            TStrings  dellist, dirlist;
            spstr dname = dir + name;

            if (mask || subdir) {
               // read tree
               dir_t *info = 0;
               u32t cnt = _dos_readtree(dir(), name(), &info, subdir, 0, 0, 0);
               if (info) {
                  if (cnt) dir_to_list(dir, info, (force?0:_A_RDONLY)|
                     _A_SUBDIR|_A_VOLID, dellist, edirs?&dirlist:0);
                  _dos_freetree(info);

                  u32t todel = dellist.Count()+dirlist.Count();

                  if (todel && !quiet && (mask || todel>1)) {
                      printf("Delete %s (%d files) (y/n/esc)?", dname(), todel);
                      int yn = ask_yn();
                      printf("\n");
                      if (yn<0) return EINTR;
                      if (!yn) { dellist.Clear(); dirlist.Clear(); }
                  }
               }
            } else
               dellist.AddObject(dname, fi.d_attr);

            if (dellist.Count() + dirlist.Count()) {
               int ii;
               spstr msg;  // put it here to prevent malloc on every file

               for (ii=0; ii<dellist.Count(); ii++) {
                  int doit = 1;
                  spstr dfname(dellist[ii]);
                  dfname.replacechar('/','\\');
                  
                  if (askall) {
                     printf("Delete %s (y/n/esc)?", dfname());
                     doit = ask_yn();
                     printf("\n");
                     if (doit<0) return EINTR;
                  } else
                  if (breakable && (ii&7)==0) // check for ESC key pressed
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
                            _dos_getfileattr(dfname(), &attributes);
                            _dos_setfileattr(dfname(), attributes&~_A_RDONLY);
                        }
                     qserr res = dellist.Objects(ii)&_A_SUBDIR? io_rmdir(dellist[ii]()) :
                                 io_remove(dellist[ii]());
                     if (res==0) {
                        msg.sprintf("Deleted file - \"%s\"", dfname());
                        fcount++;
                     } else {
                        char *emsg = cmd_shellerrmsg(EMSG_QS, res);
                        msg.sprintf("Unable to delete \"%s\" (%s)", dfname(), emsg);
                        free(emsg);
                     }
                     if (!nquiet)
                        if (pause_println(msg(),nopause?-1:0)) return EINTR;
                  }
               }
               for (ii=dirlist.Max(); ii>=0; ii--) rmdir(dirlist[ii]());
            }
         }
         if (rc<0) rc=0;
         if (!rc && !nquiet) printf("%d file(s) deleted.\n",fcount);
      }
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
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
      static char *argstr   = "w|b";
      static short argval[] = {1,1};
      process_args(al, SPA_NOSLASH, argstr, argval, &wait, &before);
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
   static char *argstr   = "q|s";
   static short argval[] = {1,1};
   process_args(al, SPA_NOSLASH, argstr, argval, &quiet, &subdir);
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
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
   return rc;
}

u32t _std shl_loadmod(const char *cmd, str_list *args) {
   int rc=-1, quiet=0, unload=0, list=0, nopause = 0;
   if (args->count>0) {
      TPtrStrings     al;
      ansi_push_set ansi(1);

      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      // process args
      static char *argstr   = "q|u|list|l|np";
      static short argval[] = {1,1, 1,  1, 1};
      process_args(al, SPA_NOSLASH, argstr, argval, &quiet, &unload, &list,
                   &list, &nopause);
      // process command
      al.TrimEmptyLines();
      // wrong args?
      if (unload && list || !Xor(list,al.Count())) rc = EINVAL; else {
         // init line counter
         pause_println(0, nopause?PRNSEQ_INITNOPAUSE:PRNSEQ_INIT);

         if (list) {
            module_information *mi = 0;
            u32t     cmi = mod_enum(&mi), ii;
            int  ioredir = !isatty(fileno(get_stdout()));

            cmd_printf(" Nø  module name               spec   pid   parent  code sz   data sz\n"
                       "--- --------------------- --- ------ ------ ------ --------- ---------\n");

            for (ii=0; ii<cmi; ii++) {
               int islib = mi[ii].flags&MOD_LIBRARY,
                   issys = mi[ii].flags&MOD_SYSTEM;
               char pstr[48], cstr[24], dstr[24];

               if (mi[ii].flags & MOD_EXECPROC) {
                  u32t ppid = 0,
                        pid = mod_getmodpid(mi[ii].handle, &ppid);
                  snprintf(pstr, 48, ppid?"%6u %6u ":"%6u        ", pid, ppid);
               } else 
                  strcpy(pstr, "              ");

               u32t dsz = 0, csz = 0, obj, osz, ofl;
               for (obj=0; obj<mi[ii].objects; obj++)
                  if (!mod_objectinfo(mi[ii].handle, obj, 0, &osz, &ofl, 0))
                     if (ofl&4) csz+=osz; else // OBJEXEC
                        if ((ofl&0x88)==0) dsz+=osz; // all except OBJRSRC & OBJINVALID
               if (csz<1024) strcpy(cstr, csz?"    <1 kb ":"          "); else
                  snprintf(cstr, 24, " %8s ", dsk_formatsize(1,csz,0,0));
               if (dsz<1024) strcpy(dstr, dsz?"    <1 kb ":"          "); else
                  snprintf(dstr, 24, " %8s ", dsk_formatsize(1,dsz,0,0));

               if (ioredir)
                  cmd_printf("%3u  %-20s %s %s%s%s%s\n", ii+1, mi[ii].name,
                     islib?"lib":"EXE", issys?"system":"      ", pstr, cstr, dstr);
               else
                  cmd_printf("%3u  " ANSI_WHITE "%-20s" ANSI_RESET " %s %s%s%s%s\n",
                     ii+1, mi[ii].name, islib?ANSI_YELLOW "lib" ANSI_RESET:
                        ANSI_LGREEN "EXE" ANSI_RESET, issys?
                           ANSI_LRED "system" ANSI_RESET:"      ", pstr, cstr, dstr);
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
                        char *errmsg = cmd_shellerrmsg(EMSG_QS, res);
                        if (errmsg) {
                           cmd_printf("%s (\"%s\")\n", errmsg, mdn());
                           free(errmsg);
                        } else
                           cmd_printf("Unload error %X for module \"%s\"\n", rc, mdn());
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
                        char *msg = cmd_shellerrmsg(EMSG_QS, error);
                        if (msg) {
                           cmd_printseq("(%s)",0,VIO_COLOR_GRAY,msg);
                           free(msg);
                        }
                     }
                  }
               }
            }
            if (rc<0) rc = 0;
         }
      }
   }
   if (rc<0) rc = EINVAL;
   if (rc && rc!=ENOENT && rc!=EBUSY) cmd_shellerr(EMSG_CLIB,rc,0);
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
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
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
         if (dch>='0'&&dch<='9') dch+='A'-'0';
         drive = dch-'A';
         pos++;
      } else
         drive = io_curdisk();

      rc = io_vollabel(drive,al.Count()>pos?al[pos]():0);
      if (rc) { cmd_shellerr(EMSG_QS, rc, 0); rc = EACCES; }
   } else {
      disk_volume_data vinf;
      u8t drive = io_curdisk();
      hlp_volinfo(drive,&vinf);
      int  lbon = vinf.Label[0];

      printf(lbon?"Volume in drive %c is %s\n":"Volume in drive %c has no label\n",
         drive+'A', vinf.Label);
      printf("Volume Serial Number is %04X-%04X\n",
         vinf.SerialNum>>16,vinf.SerialNum&0xFFFF);
      printf("Volume label (11 characters, ENTER - none, ESC)? ");
      char* label = key_getstr(label_cb);
      printf("\n");
      if (label) {
         rc = io_vollabel(drive,*label?label:0);
         if (rc) { cmd_shellerr(EMSG_QS, rc, 0); rc = EACCES; }
         free(label);
      } else {
         rc = 0;
      }
   }
   if (rc<0) cmd_shellerr(EMSG_CLIB, rc = EINVAL, 0); else rc = qserr2errno(rc);
   return rc;
}

u32t _std shl_trace(const char *cmd, str_list *args) {
   int quiet=0, rc=-1, nopause=0;
   if (args->count>0) {
      TPtrStrings al;
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      static char *argstr   = "q|np";
      static short argval[] = {1, 1};
      process_args(al, SPA_NOSLASH, argstr, argval, &quiet, &nopause);

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
         } else
         if (al[0]=="PIDLIST") {
            if (al.Count()==1) {
               trace_pid_list(!nopause);
               rc = 0;
            }
         } else {
            int lnot = 0, lnew = 0;
            if (al[0][0]=='!') { lnot=1; al[0].del(0); }
            if (al[0].lastchar()=='+') { lnew=1; al[0].dellast(); }

            if (al[0]=="PID") {
               TList lpid;
               for (int ii=1; ii<al.Count(); ii++) {
                  u32t pv = al[ii].Dword();
                  if (!pv || !mod_checkpid(pv)) { rc=ESRCH; break; }
                  lpid.Add(pv);
               }
               if (rc<0) {
                  rc = 0;
                  trace_pid(lpid, lnot, lnew);
               }
            }
         }
      }
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
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
      if (!quiet) cmd_shellerr(EMSG_CLIB,rc,0);
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
         if (!quiet) cmd_shellerr(EMSG_CLIB,rc,0);
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
            if (newrc && !quiet) cmd_shellerr(EMSG_CLIB,newrc,0);
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
   if (rc<0) cmd_shellerr(EMSG_CLIB,rc = EINVAL,0);
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
      static char *argstr   = "q|s";
      static short argval[] = {1,1};
      process_args(al, SPA_NOSLASH, argstr, argval, &quiet, &setd);
   }
   if (al.Count()==0 || al.Count()==1 && !setd) {
      if (al.Count()==0 && !quiet) {
         char str[32];
         time(&tmv);
         localtime_r(&tmv,&tme);
         strftime(str, sizeof(str), "%A, %d.%m.%Y", &tme);
         printf("The current date is %s\n", str);
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
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
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
      static char *argstr   = "q|s";
      static short argval[] = {1,1};
      process_args(al, SPA_NOSLASH, argstr, argval, &quiet, &setd);
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
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
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
            "DEF1", "DEF2", "DEF3", "POPUP", 0 };
         static u16t values[] = { MSG_GRAY, MSG_CYAN, MSG_GREEN, MSG_LIGHTGREEN,
             MSG_BLUE, MSG_LIGHTBLUE, MSG_RED, MSG_LIGHTRED, MSG_WHITE, MSG_CENTER,
             MSG_LEFT, MSG_RIGHT, MSG_JUSTIFY, MSG_WIDE, MSG_OK, MSG_OKCANCEL,
             MSG_YESNO, MSG_YESNOCANCEL, MSG_DEF1, MSG_DEF2, MSG_DEF3, MSG_POPUP };

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
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
   return 0;
}

// query/load handler for the "mode" device
cmd_eproc _std cmd_modeget(const char *dev) {
   cmd_eproc proc = cmd_modermv(dev,0);
   // no proc for this device type - trying to load it
   if (!proc) {
      spstr ename, mdname;
      ename.sprintf("MODE_%s_HANDLER",dev);
      ename = getenv(ename());
      ename.trim();
      if (!ename) {
         mdname = ecmd_readstr("MODE", dev);
         if (mdname.trim().length())
            ename = ecmd_readstr("MODULES", mdname());
      }
      // load module from env. var OR extcmd.ini list
      if (ename.trim().length()) {
         u32t error = 0;
         if (!load_module(ename, &error)) {
            log_it(2,"unable to load MODE %s handler, error %u\n", dev, error);
         } else
            proc = cmd_modermv(dev,0);
      }
   }
   return proc;
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

      // mode x,y support
      if (al.Count() && isdigit(al[0][0])) {
         spstr ln = al.GetTextToStr(" ");
         if (ln.words(",")==2) {
            u32t cols = ln.word_Dword(1,","),
                lines = ln.word_Dword(2,",");

            if (cols && lines) {
               cmd_eproc proc = cmd_modeget("CON");
               if (!proc) {
                  rc = vio_setmodeex(cols,lines) ? 0 : ENODEV;
               } else {
                  char ln[32];
                  snprintf(ln, sizeof(ln), "CON cols=%u lines=%u", cols, lines);

                  return cmd_shellcall(proc, ln, 0);
               }
            }
         }
      } else 
      if (al.Count()>=2) {
         cmd_eproc proc = cmd_modeget(al[0]());
         if (!proc) rc = ENODEV;
            else
         return cmd_shellcall(proc, 0, args);
      }
   }
   // return 0 (cancel), but display EINVAL error message
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
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

         printf("%s host.\n", host==QSHT_EFI?"EFI":"BIOS");
         cmd_printseq("%s mode.", -1, VIO_COLOR_LWHITE,
            host==QSHT_EFI?"64-bit paging":(in_pagemode?"PAE paging":"Flat"));
         // should not use get_mtlib() here, because it forces MTLIB loading 
         if (mt_active())
            cmd_printseq("MT mode is active.\n", -1, VIO_COLOR_LGREEN);
         if (!port) {
            printf("There is no debug COM port in use now.\n");
         } else {
            printf("Debug COM port address: %04hX, baudrate: %d%s.\n", port, rate,
               mod_secondary->dbport_lock?" (output disabled)":"");
         }
         if (cmv && cmv<CPUCLK_MAXFREQ) {
            cmv = 10000/CPUCLK_MAXFREQ*cmv;
            printf("CPU at %u.%2.2u%% speed.\n", cmv/100, cmv%100);
         }
         rc = 0;
      } else
      if (al[1]=="LIST") {
         if (al.Count()==3 && al[2].upper()=="FS") fs_list_int(printf);
            else printf("Unsupported for this device.\n");
         rc = 0;
      } else {
         u32t  port = al.DwordValue("DBPORT"),
               baud = al.DwordValue("BAUD"),
                cmv = al.DwordValue("CM");
         // call this first, else DBCARD will use wrong BAUD
         if (port || baud) {
            if (!hlp_seroutset(port, baud)) printf("Invalid port or baud rate value.\n");
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
               u32t flags = mem_getopts();
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
                  mem_setopts(flags);
            }
            rc = 0;
         }
         if (al.IndexOfName("CPU")>=0) {
            u32t  len = sys_queryinfo(QSQI_CPUSTRING,0), data[4],
                  avf = sys_isavail(FFFF);
            char *cpu = 0;
            
            if (len>1) {
               cpu = (char*)malloc_th(len);
               sys_queryinfo(QSQI_CPUSTRING,cpu);
            }
            if (!cpu) cpu = (char*)sprintf_dyn("CPU is %s", avf&SFEA_INTEL?
               "Intel":(avf&SFEA_AMD?"AMD":"rare"));
            // function will return 0 on 486dx :)
            if (hlp_getcpuid(1,data)) {
               char sb[40];
               sprintf(sb, " (%u.%u.%u", data[0]>>8&0xF, data[0]>>4&0xF, data[0]&0xF);
               cpu = strcat_dyn(cpu, sb);
               len = avf&(SFEA_PAE|SFEA_PAT|SFEA_X64|SFEA_LAPIC);
               if (avf&len) {
                  sprintf(sb, " %s", avf&SFEA_X64?"x64":(avf&SFEA_PAE?"PAE":""));
                  if (avf&SFEA_PAT) strcat(sb, " PAT");
                  if (avf&SFEA_LAPIC) strcat(sb, " LAPIC");
                  cpu = strcat_dyn(cpu, sb);
               }
               cpu = strcat_dyn(cpu, ")");
            }
            printf("%s\n", cpu);
            free(cpu);
            
            len = fpu_statesize();
            if (len) printf("FPU state    : %u bytes\n", len);
            len = sys_acpiroot();
            if (len) printf("ACPI root    : %08X\n", len);

            static const char *vn[] = { "BIOS Vendor", "BIOS Version", "BIOS Date",
               "Sys Vendor", "Sys Product", "Sys Version", "Sys Serial", "Sys UUID",
               "MB Vendor", "MB Product", "MB Version", "MB Serial" };
            for (u32t ii=SMI_BIOS_Vendor; ii<=SMI_MB_Serial; ii++) {
               char *info = sys_dmiinfo(ii);
               if (info) printf("%-13s: %s\n", vn[ii], info);
               if (info) free(info);
            }
            rc = 0;
         }
         if (al.IndexOfName("NORESET")>=0) {
            spstr  mstr = al.Value("NORESET");
            // empty value acts as 1 here
            u32t svmode = !mstr ? 1 : mstr.Dword();

            sto_savedword(STOKEY_CMMODE, svmode);
            rc = 0;
         }
         if (al.IndexOfName("DBCARD")>=0) {
            rc = shl_dbcard(al.Value("DBCARD"), printf);
            if (rc!=EINVAL) rc = 0;
         }
         if (al.IndexOfICase("PAE")>=0) {
            int err = pag_enable();
            rc = 0;
            if (err==E_SYS_INITED) printf("QSINIT already in paging mode.\n"); else
            if (err==E_SYS_EFIHOST) printf("QSINIT is EFI hosted and paging mode cannot be changed.\n"); else
            if (err==E_SYS_UNSUPPORTED) printf("There is no PAE support in CPU unit.\n");
               else rc = err;
         }
         if (al.IndexOfICase("MT")>=0) {
            qs_mtlib mtlib = get_mtlib();
            rc = 0;
            if (!mtlib) printf("MTLIB module is missing.\n"); else {
               u32t err = mtlib->initialize();
               if (err==E_SYS_DONE) printf("QSINIT already in MT mode.\n"); else
                  if (err) cmd_shellerr(EMSG_QS, err, 0);
            }
         }
      }
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
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
            log_printf("Error %08X on loading \"%s\" handler module \"%s\"\n",
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
      static char *argstr   = "f|d|np|nt|nc";
      static short argval[] = {1,1, 1, 0, 0};
      process_args(al, SPA_NOSLASH, argstr, argval,
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
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
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

      process_args(al, SPA_NODASH, argstr, argval,
                   &subdir, &idirs, &nopause, &a_R, &a_R, &a_S, &a_S,
                   &a_H, &a_H, &a_A, &a_A, &quiet);
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
         if (!drive) getcurdir(drive); else {
            char *fp = _fullpath(0,drive(),0);
            drive    = fp;
            free(fp);
         }
         TStrings  flist;
         spstr     dname(drive);
         if (dname.length() && dname.lastchar()!='\\') dname+="\\";
         dname+=name;

         if (mask || subdir) {
            // read tree
            dir_t *info = 0;
            u32t cnt = _dos_readtree(drive(),name(),&info,subdir,0,0,0);
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
               flist[ii].replacechar('/','\\');
               _dos_getfileattr(flist[ii](), &attrs);

               if (andmask!=FFFF) {
                  u32t   err = _dos_setfileattr(flist[ii](), attrs&andmask|ormask);
                  if (!err || quiet) msg.clear(); else {
                     char *emsg = cmd_shellerrmsg(EMSG_QS, err);
                     msg.sprintf("Unable to set attributes for \"%s\". Error %X: %s",
                        flist[ii](), err, emsg?emsg:"");
                     if (emsg) free(emsg);
                  }
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
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
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
      if (!pq) rc = EFAULT; else {
         char cd[NAME_MAX+1];
         mt_swlock();
         TStrings *pdlist = (TStrings*)pq->rtbuf[RTBUF_PUSHDST];
         if (!pdlist) pdlist = new TStrings;

         getcwd(cd, NAME_MAX+1);
         pdlist->Add(cd);
         pq->rtbuf[RTBUF_PUSHDST] = (u32t)pdlist;
         mt_swunlock();

         if (al.Count()==1) rc = chdir_int(al[0]());
         if (rc<0) rc = 0;
      }
   }
   if (quiet) return rc;
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
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
   if (!pq) rc = EFAULT; else {
      mt_swlock();
      TStrings *pdlist = (TStrings*)pq->rtbuf[RTBUF_PUSHDST];
      if (pdlist)
         if (pdlist->Count()) {
            rc = chdir_int((*pdlist)[pdlist->Max()]());
            pdlist->Delete(pdlist->Max());
         }
      if (rc<0) rc = 0;
      mt_swunlock();
   }
   if (quiet) return rc;
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
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
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
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
   if (rc<0) cmd_shellerr(EMSG_CLIB,rc = EINVAL,0);
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
      process_args(al, 0, argstr, argval, &quiet, &warm, &warm);
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
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
   return 0;
}

u32t _std shl_delay(const char *cmd, str_list *args) {
   int rc = -1;
   if (args->count==1) {
      // is it help?
      if (strcmp(args->item[0],"/?")==0) {
         cmd_shellhelp(cmd,CLR_HELP); 
         return 0; 
      } else {
         u32t value = str2ulong(args->item[0]);
         if (value && value<FFFF/1000) { usleep(value*1000); rc = 0; }
      }
   } else
   if (args->count==0) {
      usleep(CLOCKS_PER_SEC);
      rc = 0;
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
   return 0;
}

u32t _std shl_ver(const char *cmd, str_list *args) {
   int quiet=0;
   if (args->count>0) {
      TPtrStrings al;
      str_getstrs(args,al);
      // is it help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      idx = al.IndexOfName("/q");
      if (idx>=0) { quiet=1; al.Delete(idx); }
   }
   u32t  vlen = sys_queryinfo(QSQI_VERSTR, 0);
   char *vstr = (char*)malloc_th(vlen+1), *cpos;

   sys_queryinfo(QSQI_VERSTR,vstr);
   // cut it to "QSinit x.yy, rev zzzz"
   cpos = strrchr(vstr,',');
   if (cpos) *cpos = 0;

   if (quiet) {
      cpos = strrchr(vstr,' ');
      if (cpos) setenv("QSBUILD",cpos+1,1);
      cpos = strrchr(vstr,',');
      if (cpos) *cpos = 0;
      cpos = strchr(vstr,' ');
      if (cpos) setenv("QSVER",cpos+1,1);
   } else
      printf("%s\n", vstr);
   free(vstr);

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
   cmd_shelladd("ATTRIB" , shl_attrib );
   cmd_shelladd("PUSHD"  , shl_pushd  );
   cmd_shelladd("POPD"   , shl_popd   );
   cmd_shelladd("ANSI"   , shl_ansi   );
   cmd_shelladd("MOVE"   , shl_move   );
   cmd_shelladd("REBOOT" , shl_reboot );
   cmd_shelladd("DELAY"  , shl_delay  );
   cmd_shelladd("SM"     , shl_sm     );
   cmd_shelladd("VER"    , shl_ver    );

   // install MODE SYS handler
   cmd_modeadd("SYS", shl_mode_sys);
   /* install stubs for pre-defined external commands */
   TStrings autoload;
   ecmd_commands(autoload);
   for (int ii=0; ii<autoload.Count(); ii++) cmd_shelladd(autoload[ii](), shl_autostub);
}
