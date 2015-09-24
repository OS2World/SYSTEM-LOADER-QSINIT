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

void _std percent_prn(u32t percent,u32t size) {
   if (percent)
      printf("\rLoading ... %02d%% ",percent); else
   if ((size & _1MB-1)==0)
      printf("\rLoading ... %d Mb ",size>>20);
}

u32t _std shl_unzip(const char *cmd, str_list *args) {
   int rc=-1;
   if (args->count>0) {
      int testmode=0, force=0, subdir=1, quiet=0, nopause=-1, frombp=0,
          fromstor=0, view=0;

      TPtrStrings al;
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      // process args
      static char *argstr   = "/t|/o|/e|/q|/p|/boot|/key|/test|/view|/np|/list|/v";
      static short argval[] = { 1, 1, 0, 1, 0,    1,   1,    1,   1,   1,    1, 1};
      process_args(al, argstr, argval,
                   &testmode, &force, &subdir, &quiet, &nopause, &frombp,
                   &fromstor, &testmode, &view, &nopause, &view, &view);
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
               if (!quiet) cmd_shellerr(rc, errname());
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

            ZIP   zip;
            u32t  zsize;
            void* zdata;

            if (fromstor) {
               zdata = sto_data(zipname());
               zsize = sto_size(zipname());
            } else
            if (frombp) zdata = hlp_freadfull(zipname(),&zsize,percent_prn);
               else zdata = freadfull(zipname(),&zsize);

            if (!zdata && frombp) {
               zipname+=".zip";
               zdata = hlp_freadfull(zipname(),&zsize,percent_prn);
            }
            if (!zdata) {
               rc = frombp||fromstor?ENOENT:get_errno();
               if (!quiet) cmd_shellerr(rc, errname());
               break;
            }
            zip_open(&zip, zdata, zsize);
            // init "paused" output
            if (!quiet) pause_println(0,1);

            char *getpath;
            u32t  getsize = 0,
                   errors = 0,
                   ftotal = 0;
            int      once = 0;
            u64t total_uc = 0;
            u32t  total_c = 0;

            if (view) {
               pause_println(" compr.    uncompr.   crc        date/time      name", nopause?-1:0);
               pause_println(zipviewline, nopause?-1:0);
            }

            while (zip_nextfile(&zip,&getpath,&getsize)) {
               spstr cname(getpath);
               cname.replacechar('/','\\');
               int ext_ok = 0,
                   is_dir = getsize==0 && cname.lastchar()=='\\';
               // flag one zip_nextfile() success
               once = 1;

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

                     if (!testmode && !force) {
                        unsigned attrs;
                        if (!_dos_getfileattr(cname(),&attrs) && (attrs&_A_SUBDIR)==0) {
                           printf("\rOverwrite %.44s (y/n/all/esc)?",cname());
                           doit = ask_yn(1);
                           printf("\r");
                           if (doit<0) { rc = EINTR; break; }
                           // "all"
                           if (doit==2) force = 1;
                        }
                     }
                     if (doit) {
                        // print process
                        if (!quiet) printf("\r%sing %-52s ", testmode?"Test":"Extract",
                           cname());
                        // unpack & save
                        if (!is_dir) filemem = zip_readfile(&zip);
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
                              if (mkdir(cname())) errors++;
                           } else {
                              // create file
                              FILE *out = fopen(cname(),"wb");
                              // no such path? trying to create it
                              if (!out && get_errno()==ENOENT) {
                                 int pos = cname.crpos('\\');
                                 if (pos>0) { // ignore leading '\'
                                    spstr cdir(cname,0,pos++);
                                    if (mkdir(cdir())) break;
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
               if (!once) printf("%snot a zip archive!\n", errname()); else
               if (view) {
                  pause_println(zipviewline, nopause?-1:0);
                  cmd_printseq(" %d files, %Lu bytes uncompressed, %u compressed\n",
                     nopause?-1:0, 0, ftotal, total_uc, total_c);
               } else
                  printf("\r%d files extracted with %d error(s)\n", ftotal, errors);
            zip_close(&zip);
            if (!fromstor) hlp_memfree(zdata);
            if (errors) rc=EIO;
            break;
         }
         //-------------------------
         if (rc<0) rc=0;
         return rc;
      }
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return rc;
}
