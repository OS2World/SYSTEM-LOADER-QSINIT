//
// QSINIT "cmd" module
//
#include "stdlib.h"
#include "qsshell.h"
#include "qsutil.h"
#include "vioext.h"
#include "errno.h"
#include "qsstor.h"
#include "qsio.h"
#include "qssys.h"
#include "qcl/qslist.h"
#include "direct.h"

extern 
char    *_CmdArgs;
u32t         echo = CMDR_ECHOOFF;
ptr_list  history = 0;

#define HISTORY_KEY  "cmd_hist_list"

void set_errorlevel(int elvl) {
   char  errlvl[12];
   _utoa(elvl<0?255:elvl,errlvl,10);
   setenv("ERRORLEVEL",errlvl,1);
}

u32t execute_command(char *cmd) {
   str_list *cmd_s = str_splitargs(cmd);
   char     *cmd_f = 0;
   u32t         rc = 0;

   if (cmd_s->count>=1 && cmd_s->item[0][0]) {
      char ext[_MAX_EXT+1], path[_MAX_PATH];
      int search = 0, tryext = 0, err = 0, is_batch = 0;
      ext[0] = 0;

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
            // replacing (with care about "") command name to founded one
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
      } else
      if (is_batch || strcmp(ext,".CMD")==0 || strcmp(ext,".BAT")==0) {
          cmd_exec(path,cmd);
      } else {
          cmd_state state = cmd_init(cmd,0);
          cmd_run(state,echo);
          echo = cmd_getflags(state);
          cmd_close(state);
      }
   }
   if (cmd_f) free(cmd_f);
   free(cmd_s);
   return rc;
}

static void read_history(void) {
   history = (ptr_list)sto_data(HISTORY_KEY);
   if (!history) { // we are the first shell instance - creating history ...
      history = NEW(ptr_list);
      // save pointer with 1 byte len (size ignored)
      sto_save(HISTORY_KEY,history,1,0);
   }
}

// trim command and save it in history, return 0 if command was empty.
static char* push_history(char *cmd) {
   if (history && cmd) {
      u32t ii;
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

      for (ii=0; ii<history->count(); ii++)
         if (!strcmp((char*)history->value(ii),cmd)) {
            // duplicate entry? move it to the top of history:
            history->exchange(ii,history->max());
            free(cmd);
            return (char*)history->value(history->max());
         }
      // share it in this way to prevent garbage in file name in memmgr dump
      __set_shared_block_info(cmd, "cmd history", 0);
      history->add(cmd);
      return cmd;
   }
   free(cmd);
   return 0;
}

static int _std editline_cb(u16t key, key_strcbinfo *dta) {
   if (history) {
      u8t  keyh=key>>8;
      if (keyh==0x48 || keyh==0x50) {
         long up = keyh==0x48?-1:1;
         dta->userdata+=up;
         if ((long)dta->userdata<0) dta->userdata = history->count(); else
         if (dta->userdata>history->count()) dta->userdata = 0;
         // 0 - empty string, 1...count = history
         if (!dta->userdata) {
            dta->pos=0; dta->scroll=0; dta->clen=0; *dta->rcstr=0;
         } else {
            char *str = (char*)history->value(dta->userdata-1);
            u32t  len = strlen(str);

            if (dta->bsize<len) dta->rcstr = (char*)realloc(dta->rcstr, 
               dta->bsize = Round32(len+2));
            strcpy(dta->rcstr, str);
            dta->clen = len;    
            if (len>=dta->width) {
               dta->scroll = len-dta->width+1;
               dta->pos = len-dta->scroll;
            } else {
               dta->scroll = 0;
               dta->pos = len;
            }
         }
         return 3;
      }
   }
   return 0;
}

int cmdloop(const char *init) {
   u32t     rc;
   qserr   err;
   read_history();
   do {
      char *cmd, cdir[QS_MAXPATH+1];
      vio_defshape(VIO_SHAPE_LINE);

      err  = io_curdir(cdir, QS_MAXPATH+1);
      printf("\n%s=>", err?"??":cdir);
      cmd  = key_getstrex(editline_cb,-1,-1,-1,init);
      init = 0;
      printf("\n");
      cmd = push_history(cmd);
      if (cmd) rc = execute_command(cmd);
   } while (rc!=CMDR_RETEND);
   return 0;
}

int main(int argc,char *argv[]) {
   int argnext = 1, leave = 1, editcmd = 0;
   char last = 0, *init = 0;

   while (argnext<argc) {
      if (argv[argnext][0]=='/') {
         if (stricmp(argv[argnext]+1,"C")==0) {
            last = 'C'; leave = 0; argnext++;
         } else 
         if (stricmp(argv[argnext]+1,"K")==0) {
            last = 'K'; argnext++;
         } else 
         if (stricmp(argv[argnext]+1,"E")==0) {
            last = 'E'; editcmd = 1; argnext++;
         } else 
         if (stricmp(argv[argnext]+1,"?")==0) {
            printf("Command processor:\n\nCMD [options] [commands]\n"
                   "  /c      execute the specified command and then terminates\n"
                   "  /k      execute the specified command but remains\n"
                   "  /e      put command to edit line, but not run it\n");
            set_errorlevel(0);
            return 0;
         }
      } else break;
   }
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
            if (editcmd) { leave = 1; init  = pos+1; }
               else execute_command(pos+1);
      }
   }

   if (leave) {
       u32t ver_len = sys_queryinfo(QSQI_VERSTR, 0);
       char  *about = (char*)malloc(ver_len), 
             *cmpos;
       sys_queryinfo(QSQI_VERSTR, about);
       cmpos = strrchr(about,',');
       if (cmpos) *cmpos = 0;
       printf("\n  Welcome to %s shell! ;)\n",about);
       free(about);
       return cmdloop(init);
   }
   return 0;
}
