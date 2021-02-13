//
// QSINIT "start" module
// unzip shell command
//
// file included into extcmd.cpp directly
//
extern "C" {
#include "../../../init/zip/unzip.h"
}

static const char *zipviewline = "--------- --------- -------- ---------------- -------------------------------";

u32t _std shl_unzip(const char *cmd, str_list *args) {
   int rc=-1;
   if (args->count>0) {
      int testmode=0, force=0, quiet=0, nopause=-1, frombp=0,
          fromstor=0, view=0, beep=0, preload = 0;
      TPtrStrings al;
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      // process args
      static char *argstr   = "t|o|s|c|q|p|boot|key|test|view|np|list|v|beep";
      static short argval[] = {1,1,-1,1,1,0,  1,  1,   1,   1, 1,   1,1,   1};
      process_args(al, SPA_NOSLASH, argstr, argval,
                   &testmode, &force, &force, &preload, &quiet, &nopause, &frombp,
                   &fromstor, &testmode, &view, &nopause, &view, &view, &beep);
      al.TrimEmptyLines();
      // by default pause off in "unzip" mode and on in "list"
      if (nopause==-1) nopause = view?0:1;
      // invalid key combination
      if (view && (testmode||quiet)) rc = EINVOP; else
      // else process it
      if (al.Count()>=1) {
         dir_t zipstat;
         spstr errname(al[0]);
         errname+=": ";

         if (frombp||fromstor) rc = 0; else {
            // check file presence
            rc = _dos_stat(al[0](),&zipstat);
            if (rc) {
               al[0]+=".zip";
               rc = _dos_stat(al[0](),&zipstat);
            }
            if (!rc && (zipstat.d_attr&_A_SUBDIR)) rc = EISDIR;
            if (rc) {
               rc = get_errno();
               if (!quiet) cmd_shellerr(EMSG_CLIB, rc, errname());
            }
         }

         while (!rc) {
            spstr tgtdir, zipname(al[0]);
            al.Delete(0);
            if (al.Count()>0) {
               if (!_dos_stat(al[0](),&zipstat) && (zipstat.d_attr&_A_SUBDIR)) {
                  tgtdir = al[0];
                  al.Delete(0);
                  tgtdir.replacechar('/','\\');
                  if (tgtdir.lastchar()!='\\') tgtdir+="\\";
               }
            }

            ZIP        zip;
            u32t     zsize = 0;
            void*    zdata = 0;
            qserr  zopnerr = 0;
#if 0
            log_it(3, "unzip \"%s\", key:%i, boot %i, cache %i\n", zipname(),
               fromstor, frombp, preload);
#endif
            if (fromstor || preload) {
               if (fromstor) {
                  zdata = sto_data(zipname());
                  zsize = sto_size(zipname());
               } else {
                  if (frombp) zdata = hlp_freadfull_progress(zipname(), &zsize);
                     else zdata = freadfull(zipname(),&zsize);
                  
                  if (!zdata && frombp) {
                     zipname+=".zip";
                     zdata = hlp_freadfull_progress(zipname(), &zsize);
                  }
                  if (!zdata) {
                     rc = frombp||fromstor?ENOENT:get_errno();
                     if (!quiet) cmd_shellerr(EMSG_CLIB, rc, errname());
                     break;
                  }
               }
               zopnerr = zip_open(&zip, zdata, zsize);
            } else {
               zopnerr = zip_openfile(&zip, zipname(), frombp);
               // for common file - .zip was appended above
               if (frombp && zopnerr==E_SYS_NOFILE) {
                  zipname+=".zip";
                  zopnerr = zip_openfile(&zip, zipname(), frombp);
               }
            }
            if (zopnerr) {
               if (!quiet)
                  if (zopnerr==E_SYS_BADFMT) printf("%snot a zip archive!\n", errname());
                     else cmd_shellerr(EMSG_QS, zopnerr, errname());
               // avoid final error print
               rc = 0;
               break;
            }
            // init "paused" output
            if (!quiet) pause_println(0,1);

            char *getpath;
            u32t  getsize = 0,
                   errors = 0,
                   ftotal = 0;
            u64t total_uc = 0;
            u64t  total_c = 0;

            if (view) {
               pause_println(" compr.    uncompr.   crc        date/time      name", nopause?-1:0);
               pause_println(zipviewline, nopause?-1:0);
            }

            while (zip_nextfile(&zip,&getpath,&getsize)==0) {
               spstr cname(getpath);
               cname.replacechar('/','\\');
               int ext_ok = 0,
                   is_dir = getsize==0 && cname.lastchar()=='\\';

               if (view) {
                  char  line[312];
                  struct tm    ft;
                  int         len;
                  dostimetotm(zip.dostime, &ft);

                  snprintf(line, 312, "%9u %9u %08X %02d.%02d.%04d %02d:%02d %s",
                     zip.compressed, zip.uncompressed, zip.stored_crc, ft.tm_mday,
                        ft.tm_mon+1, ft.tm_year+1900, ft.tm_hour, ft.tm_min, cname());
                  ftotal++;
                  total_uc += zip.uncompressed;
                  total_c  += zip.compressed;
                  pause_println(line, nopause?-1:0);
               } else {
                  if (is_dir) cname.dellast();
                  // ignore directory creation if file names supplied
                  if (al.Count()==0) ext_ok=1; else
                  for (int ii=0;ii<al.Count();ii++)
                     if (_matchpattern(al[ii].replacechar('/','\\')(),cname())) {
                        ext_ok=1;
                        break;
                     }

                  if (ext_ok) {
                     // allocate at least MAXPATH bytes ;)))
                     void *filemem = 0;
                     // path to extract
                     cname.insert(tgtdir,0);
                     // do_it flag
                     int doit = 1;

                     if (!testmode && force<=0) {
                        unsigned attrs;
                        if (!_dos_getfileattr(cname(),&attrs) && (attrs&_A_SUBDIR)==0) {
                           if (force<0) doit = 0; else {
                              {
                                 ansi_push_set color(1);
                                 printf(ANSI_RESET "\rOverwrite %.44s ("
                                    ANSI_WHITE "y" ANSI_RESET "/"
                                    ANSI_WHITE "n" ANSI_RESET "/"
                                    ANSI_WHITE "a" ANSI_RESET "ll/"
                                    ANSI_WHITE "s" ANSI_RESET "kip all/"
                                    ANSI_WHITE "e" ANSI_RESET "sc)?", cname());
                              }
                              doit = ask_yn(1,1);
                              printf("\r");
                              if (doit<0) { rc = EINTR; break; }
                              // "all"
                              if (doit==2) force = 1; else
                                 if (doit==3) force = -1;
                           }
                        }
                     }
                     if (doit) {
                        // print process
                        if (!quiet) printf("\r%sing %-52s ", testmode?"Test":"Extract",
                           cname());
                        // unpack & save
                        if (!is_dir) zip_readfile(&zip,&filemem);
                        // counter
                        ftotal++;

                        if (!is_dir && !filemem && getsize) {
                           errors++;
                           if (!quiet)
                              if (pause_println("failed",nopause?-1:0)) {
                                 rc = EINTR;
                                 break;
                              }
                        } else
                        if (testmode) {
                           if (!quiet)
                              if (pause_println("ok",nopause?-1:0)) {
                                 rc = EINTR;
                                 break;
                              }
                        } else {
                           u32t errprev=errors;

                           if (is_dir) {
                              if (mkdir(cname()))
                                 if (!hlp_isdir(cname())) errors++;
                           } else {
                              // create file
                              FILE *out = fopen(cname(),"wb");
                              // no such path? trying to create it
                              if (!out && get_errno()==ENOENT) {
                                 int pos = cname.crpos('\\');
                                 if (pos>0) { // ignore leading '\'
                                    spstr cdir(cname,0,pos++);
                                    if (!hlp_isdir(cdir())) mkdir(cdir());
                                 }
                                 // open file again
                                 out = fopen(cname(),"wb");
                              }
                              if (out) {
                                 if (getsize)
                                    if (fwrite(filemem,1,getsize,out)!=getsize) errors++;
                                 if (fclose(out)) errors++;
                              } else errors++;
                           }
                           if (!quiet)
                              if (pause_println(errprev==errors?"ok":"write error",
                                 nopause?-1:0)) { rc = EINTR; break; }

                           if (errprev==errors)
                              _dos_setfiletime(cname(),zip.dostime,_DT_MODIFY|_DT_CREATE);
                           else
                              unlink(cname());
                        }
                        if (filemem) hlp_memfree(filemem);
                     } else
                        printf("\rSkipped %-52s\n",cname());
                  }
               }
            }
            if (!quiet)
               if (view) {
                  pause_println(zipviewline, nopause?-1:0);
                  cmd_printseq(" %d files, %Lu bytes uncompressed, %Lu compressed\n",
                     nopause?-1:0, 0, ftotal, total_uc, total_c);
               } else
                  printf("\r%d files extracted with %d error(s)"
                         "                                   \n", ftotal, errors);
            zip_close(&zip);
            if (!fromstor && zdata) hlp_memfree(zdata);
            if (errors) rc=EIO;
            break;
         }
         //-------------------------
         if (rc<0) rc=0;
         // beeeeep!
         if (beep) {
            vio_beep(rc>0?200:700,40);
            while (vio_beepactive()) usleep(5000);
            vio_beep(rc>0?100:600,40);
         }
         return rc;
      }
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
   return rc;
}
