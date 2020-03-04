//
// QSINIT "cmd" module
//
#include "stdlib.h"
#include "qsbase.h"
#include "signal.h"
#include "vioext.h"
#include "errno.h"
#include "qcl/qsmt.h"
#include "qcl/qslist.h"
#include "direct.h"
#include "qsint.h"

extern 
char         *_CmdArgs;                ///< full command line (runtime variable)
u32t              echo = CMDR_ECHOOFF;
static char *in_stream = 0,
           *out_stream = 0;
int        interactive = 1;
char          *apppath = 0;            ///< argv[0]
ptr_list       history = 0;            ///< this handle in shared over system!

void set_errorlevel(int elvl) {
   char  errlvl[12];
   _utoa(elvl<0?255:elvl,errlvl,10);
   setenv("ERRORLEVEL",errlvl,1);
}

void set_title(const char *str) {
   char *title = str ? sprintf_dyn("cmd: %s", str) : 0;
   se_settitle(se_sesno(), title);
   if (title) free(title);
}

void _std ctrlc_processing(int sig);

u32t execute_command(char *cmd, int timer) {
   str_list *cmd_s = str_splitargs(cmd);
   char     *cmd_f = 0;
   u32t         rc = 0;

   if (cmd_s->count>=1 && cmd_s->item[0][0]) {
      char ext[_MAX_EXT+1], path[_MAX_PATH];
      int search = 0, tryext = 0, err = 0, is_batch = 0;
      ext[0] = 0;

      set_title(cmd);

      strcpy(path, cmd_s->item[0]);
      strupr(path);
      if (strcmp(path,"EXIT")==0) rc = CMDR_RETEND;

      if (path[1]==':'&&(path[2]==0||path[2]=='\\'&&path[3]==0)) {
         // change disk, just send to cmd_run()
      } else
      if (!cmd_shellquery(path)) {
         char   drv[_MAX_DRIVE], dir[_MAX_DIR], srch[_MAX_PATH];
         int  exist;
         dir_t   fi;

         _splitpath(path,drv,dir,0,ext);
         if (!*drv&&!*dir) search = 1;
         if (!*ext) tryext = 1;

         // skip directories
         exist = _dos_stat(path,&fi)==0;
         if (exist && (fi.d_attr&_A_SUBDIR)!=0) exist=0;
         
         if (!exist) {
            char srch[_MAX_PATH];
            int  namelen = strlen(path);
            strcpy(srch,path);
            // append possible extensions
            if (tryext) {
               strcpy(srch+namelen,".EXE");
               if (access(srch, F_OK)) {
                  strcpy(srch+namelen,".CMD");
                  if (access(srch, F_OK)) err = 1; else is_batch = 1;
               }
               if (!err) strcpy(path,srch); else strcpy(srch,path);
            }
            // search in path
            if (search&&(err||!tryext)) {
               err = 0;
               _searchenv(srch,"PATH",path);
               if (!*path) {
                  if (tryext) {
                     strcpy(srch+namelen,".EXE");
                     _searchenv(srch,"PATH",path);
                     if (!*path) {
                        strcpy(srch+namelen,".CMD");
                        _searchenv(srch,"PATH",path);
                        if (!*path) err = 1; else is_batch = 1;
                     }
                  } else err = 1;
               }
            }
            // replacing (with care about "") command name to found one
            if (!err) {
               char *ep = strstr(cmd,cmd_s->item[0]);
               if (ep) {
                  ep+=namelen;
                  while (*ep!=' '&&*ep!='\t'&&*ep!=0) ep++;
                  cmd_f = (char*)malloc(strlen(ep)+strlen(path)+3);
                  cmd_f[0] = '\"';
                  strcpy(cmd_f+1,path);
                  // temp!
                  //strcat(cmd_f,"\"");
                  strcat(cmd_f,ep);
                  cmd = cmd_f+1; //temp!!
               }
            }
         }
      }

      if (err) {
          printf("Unable to find command \"%s\"!\n",cmd_s->item[0]);
          rc = ENOENT;
      } else {
         qsclock  t1,t2;
         signal(SIGINT, ctrlc_processing);

         if (is_batch || strcmp(ext,".CMD")==0 || strcmp(ext,".BAT")==0) {
             if (timer) t1 = sys_clock();
             cmd_exec(path,cmd);
             if (timer) t2 = sys_clock();
         } else {
             cmd_state state = cmd_init(cmd,0);
             if (timer) t1 = sys_clock();
             cmd_run(state,echo);
             if (timer) t2 = sys_clock();
             echo = cmd_getflags(state);
             cmd_close(state);
         }
         signal(SIGINT, SIG_IGN);
         if (timer) {
            if (t2<t1) t2 = t1;
            t2 -= t1;
            if (!t2) printf("\n[Time calulation error]\n"); else
               printf("\n[Execution time %Lu.%04Lu sec]\n", t2/CLOCKS_PER_SEC,
                  t2%CLOCKS_PER_SEC/(CLOCKS_PER_SEC/10000));
         }
      }
   }
   if (cmd_f) free(cmd_f);
   free(cmd_s);
   return rc;
}

/// ctrl-c signal handler
void _std ctrlc_processing(int sig) {
   printf("\nProcess terminated\n");
   if (interactive) {
      u32t mod = mod_load(apppath, 0, 0, 0);
      if (mod) {
         /* chain a new copy of self to the same pid - this will release any
            lost resources but does not break the parent process, which waits
            for us */
         mod_chain(mod, 0, "/q /k");
      }
   }
   // this call links MTLIB statically ...
   //mod_stop(0, 0, EXIT_FAILURE);
   // terminate self without any signal
   NEW(qs_mtlib)->stop(0, 0, EXIT_FAILURE);
}

/*
   history rules are simple:
   1). only add and never del
   2). never modify added strings itself (pointer and/or data)
   3). any call to list is protected by a mutex in MT mode

   note, that list is used by exit_restart() function.
*/
static void open_history(void) {
   mt_swlock();
   history = (ptr_list)sto_data(STOKEY_CMDSTORY);
   if (!history) {
      /* we are the first shell instance - creating history ...
         history list object is shared and covered by a mutex. Mutex will be
         created by system during MT mode initialization */
      history = NEW_GM(ptr_list);
      // save pointer with 1 byte len (size ignored)
      sto_save(STOKEY_CMDSTORY, history, 1, 0);
   }
   mt_swunlock();
}

// trim command and save it in history, return 0 if command was empty.
static char* push_history(char *cmd) {
   if (history && cmd) {
      u32t  ii, hcnt;
      char *cn = cmd;
      while (*cn==' '||*cn=='\t') cn++;
      ii = strlen(cn);
      if (ii) {
         char *ce;
         for (ce=cn+ii; ce!=cn; ce--)
            if (ce[-1]==' '||ce[-1]=='\t') ce[-1]=0; else break;
         ii = ce-cn;
      }
      // command is empty?
      if (!ii) { free(cmd); return 0; }
      // trying to free some memory, because pointer will saved until reboot
      if (cmd!=cn) memmove(cmd,cn,ii+1);
      cmd = realloc(cmd,ii+1);
      // lock entire walking over list to be safe with async cmd_historywipe()
      exi_lock(history);
      for (ii=0, hcnt=history->count(); ii<hcnt; ii++) {
         char *vii = (char*)history->value(ii);
         // duplicate entry? move it to the top of history
         if (!strcmp(vii,cmd)) {
            history->exchange(ii,history->max());
            exi_unlock(history);
            free(cmd);
            return vii;
         }
      }
      exi_unlock(history);
      // share block in this way to avoid garbage in a file name in memmgr dump
      __set_shared_block_info(cmd, 0, 0);
      history->add(cmd);
      return cmd;
   }
   free(cmd);
   return 0;
}

static void replace_line(key_strcbinfo *dta, char *newline) {
   u32t len = strlen(newline);

   if (dta->bsize<len)
      dta->rcstr = (char*)realloc(dta->rcstr, dta->bsize = Round32(len+2));
   strcpy(dta->rcstr, newline);
   dta->clen = len;    
   if (len>=dta->width) {
      dta->scroll = len-dta->width+1;
      dta->pos    = len-dta->scroll;
   } else {
      dta->scroll = 0;
      dta->pos    = len;
   }
}

// up/down processing in edit line
static int _std editline_cb(u16t key, key_strcbinfo *dta) {
   if (history) {
      u8t  keyh=key>>8;
      if (keyh==0x48 || keyh==0x50) {
         long   up = keyh==0x48?-1:1;
         u32t hcnt;
         dta->userdata+=up;
         // lock entire list access
         exi_lock(history);
         hcnt = history->count();

         if ((long)dta->userdata<0) dta->userdata = hcnt; else
            if (dta->userdata>hcnt) dta->userdata = 0;
         // 0 - empty string, 1...count = history
         if (!dta->userdata) {
            dta->pos=0; dta->scroll=0; dta->clen=0; *dta->rcstr=0;
         } else
            replace_line(dta, (char*)history->value(dta->userdata-1));
         exi_unlock(history);
         return 3;
      } else
      if (keyh==0x49 || keyh==0x51) { // PgUp/PgDn
         str_list *lst = cmd_historyread();
         int    action = 2;

         if (lst && lst->count) {
            vio_listref *rf = malloc_th(sizeof(vio_listref)+sizeof(vio_listkey));
            u32t id;
            rf->size  = sizeof(vio_listref);
            rf->items = lst->count;
            rf->text  = lst;
            rf->id    = 0;
            rf->subm  = 0;

            rf->akey[0].key   = 0x5300;  // Del
            rf->akey[0].mask  = 0xFF00;
            rf->akey[0].id_or = 0x80000000;
            rf->akey[1].key   = 0;

            id = vio_showlist("Shell history list", rf, MSG_GRAY|MSG_LEFT, 0);
            free(rf);
            if (id&0x80000000) cmd_historywipe(); else 
            if (id && id<=lst->count) {
               replace_line(dta, lst->item[id-1]);
               action|=1;
            }
         }
         if (lst) free(lst);
         return action;
      }
   }
   return 0;
}

int cmdloop(const char *init) {
   u32t     rc;
   qserr   err;
   open_history();
   signal(SIGINT, SIG_IGN);
   do {
      char *cmd, cdir[QS_MAXPATH+1], *title;
      vio_setshape(VIO_SHAPE_LINE);

      err   = io_curdir(cdir, QS_MAXPATH+1);
      set_title(err?"invalid directory":cdir);

      printf("\n%s=>", err?"??":cdir);

      cmd  = key_getstrex(editline_cb,-1,-1,-1,init,0);
      init = 0;
      printf("\n");
      cmd = push_history(cmd);
      if (cmd) rc = execute_command(cmd,0);
   } while (rc!=CMDR_RETEND);
   set_title(0);
   return 0;
}

int io_replacement(const char *fname, const char *mode, FILE *stdf) {
   char  *fn = _fullpath(0, fname, 0);
   int errNo = 0;
   if (!freopen(fn, mode, stdf)) {
      char *emsg = cmd_shellerrmsg(EMSG_CLIB, errNo=errno);
      printf("%s stream \"%s\" error: %s\n", *mode=='r'?"Input":"Output", fn, emsg);
      free(emsg);
   }
   free(fn);
   return errNo;
}

int main(int argc,char *argv[]) {
   int argnext = 1, editcmd = 0, quiet = 0, timer = 0;
   char last = 0, *init = 0;
   apppath = argv[0];

   while (argnext<argc) {
      if (argv[argnext][0]=='/') {
         if (stricmp(argv[argnext]+1,"C")==0) {
            last = 'C'; interactive = 0; argnext++;
         } else 
         if (stricmp(argv[argnext]+1,"K")==0) {
            last = 'K'; argnext++;
         } else 
         if (stricmp(argv[argnext]+1,"E")==0) {
            last = 'E'; editcmd = 1; argnext++;
         } else 
         if (stricmp(argv[argnext]+1,"Q")==0) {
            quiet = 1; argnext++;
         } else 
         if (stricmp(argv[argnext]+1,"TM")==0) {
            timer = 1; argnext++;
         } else 
         if (strnicmp(argv[argnext]+1,"I:",2)==0) in_stream = argv[argnext++] + 3;
            else 
         if (strnicmp(argv[argnext]+1,"O:",2)==0) out_stream = argv[argnext++] + 3;
            else 
         if (stricmp(argv[argnext]+1,"?")==0) {
            cmd_shellhelp("CMD",CLR_HELP);
            set_errorlevel(0);
            return 0;
         } else 
         if (last) break; else {
            printf("\rInvalid parameter: \"%s\"\n", argv[argnext]);
            return EINVAL;
         }
      } else break;
   }
   se_sigfocus(1);

   if (argnext<argc && last) {
      char srch[3] = "/", *pos;
      srch[1] = last; srch[2] = 0;
      // search for the end of options
      pos = strstr(_CmdArgs,srch);
      if (!pos) {
         srch[1] = tolower(last);
         pos = strstr(_CmdArgs,srch);
      }
      if (pos) {
         pos = strchr(pos, ' ');
         if (pos)
            if (editcmd) { interactive = 1; init = pos+1; } else {
               if (last=='C') {
                  if (in_stream) {
                     int res = io_replacement(in_stream, "r", stdin);
                     if (res) return res;
                  }
                  if (out_stream) {
                     int res = io_replacement(out_stream, "w", stdout);
                     if (res) return res;
                  }
               }
               execute_command(pos+1, timer && !interactive);
            }
      }
   }

   if (interactive) {
      if (!quiet) {
         u32t ver_len = sys_queryinfo(QSQI_VERSTR, 0);
         char  *about = (char*)malloc(ver_len), 
               *cmpos;
         sys_queryinfo(QSQI_VERSTR, about);
         cmpos = strrchr(about,',');
         if (cmpos) *cmpos = 0;
         printf("\n  Welcome to %s shell! ;)\n",about);
         free(about);
      }
      return cmdloop(init);
   }
   set_title(0);
   return 0;
}
