//
// QSINIT "start" module
// common shell functions
//
#include "qsutil.h"
#include "classes.hpp"
#include "stdlib.h"
#include "qsshell.h"
#include "vio.h"
#include "errno.h"
#include "qslog.h"
#include "internal.h"
#include "qsconst.h"
#include "direct.h"
#include "time.h"
#include "qshm.h"
#include "qsint.h"
#include "qssys.h"
#include "qsstor.h"
#include "qspage.h"
#include "qsmodext.h"
#include "qsclass.h"

#define SESS_SIGN     0x53534553
#define MODE_ECHOOFF      0x0001

static const char *no_cmd_err="Unable to load batch file \"%s\"\n";

typedef Strings<cmd_eproc>  CmdList;
typedef CmdList*           PCmdList;

static CmdList *ext_shell = 0, *embedded = 0, *ext_mode = 0;

static const char *internal_commands = "IF\nECHO\nGOTO\nGETKEY\nCLS\n"
                             "SET\nFOR\nSHIFT\nREM\nPAUSE\nEXIT\nCALL";

struct session_info {
   u32t          sign;
   TStrings      list;
   TStrings      args;
   u32t          mode;
   u32t      nextline;
   session_info *nest;
};

cmd_state _std cmd_init(const char *cmds,const char *args) {
   session_info *si = new session_info;
   si->sign = SESS_SIGN;
   si->mode = 0;
   si->nest = 0;
   si->nextline = 0;
   si->list.SetText(cmds);
   // parse args and add emtry one for file name
   if (args) si->args.ParseCmdLine(args);
   // insert line numbers
   u32t ii, jj;
   for (ii=0;ii<si->list.Count();ii++) si->list.Objects(ii)=ii+1;
#if 1
   // split & command line
   ii=0;
   while (ii<si->list.Count())
      if (si->list[ii].countchar('&')>0) {
         TStrings ins;
         ins.SplitString(si->list[ii],"&");
         ins.TrimEmptyLines();
         for (jj=0;jj<ins.Max();jj++) ins.Objects(jj)=ii+1;
         si->list.Delete(ii);
         si->list.AddStrings(ii,ins);
         ii+=ins.Count();
      } else ii++;
#endif
   return (void*)si;
}

cmd_state cmd_init2(TStrings &file, TStrings &args) {
   session_info *si = new session_info;
   si->sign = SESS_SIGN;
   si->mode = 0;
   si->nest = 0;
   si->list = file;
   si->args = args;
   si->nextline = 0;
   return (void*)si;
}

cmd_state _std cmd_initbatch(const str_list* cmds,const str_list* args) {
   TStrings file, arglist;
   str_getstrs(cmds,file);
   str_getstrs(args,arglist);
   return cmd_init2(file,arglist);
}

static spstr &subst_env(spstr &str,session_info *si) {
  l ll,epos,start=0;
  if (si) // replace script arguments
     for (ll=0;ll<10;ll++) {
        char argn[4];
        argn[0]='%'; argn[1]='0'+ll; argn[2]=0;
        str.replace(argn,si->args.Count()>ll?si->args[ll]():"");
     }
  do {     // replace env vars
     ll=str.bepos("%","%",epos,start);
     if (ll>=0) {
        spstr ename(str,ll+1,epos-ll-1), eval;

        if (ename.cpos(' ')>=0) start=epos;
        else {
           if (ename.upper()=="CD") {
              getcwd(eval.LockPtr(NAME_MAX+1),NAME_MAX+1);
              eval.UnlockPtr();
           } else
           if (ename=="DATE"||ename=="TIME") {
              struct tm tmv;
              time_t    tme;
              time(&tme);
              localtime_r(&tme,&tmv);
              if (ename[0]=='D')
                 eval.sprintf("%02d.%02d.%04d",tmv.tm_mday,tmv.tm_mon+1,tmv.tm_year+1900);
              else
                 eval.sprintf("%02d:%02d:%02d",tmv.tm_hour,tmv.tm_min,tmv.tm_sec);
           } else
           if (ename=="RANDOM") {
              eval = random(32768);
           } else
           if (ename=="LINES") {
              u32t lines = 25;
              vio_getmode(0, &lines);
              eval = lines;
           } else
           if (ename=="COLUMNS") {
              u32t cols = 80;
              vio_getmode(&cols, 0);
              eval = cols;
           } else
           if (ename=="RAMDISK") {
              char *dn = (char*)sto_data(STOKEY_VDNAME);
              eval = dn?dn:"?";
           } else

           if (ename=="DBPORT") {
              u32t port = hlp_seroutinfo(0);
              if (port) eval = port;
           } else
              eval = getenv(ename());
           str.replace(str(ll,epos-ll+1),eval);
           // pos to the end of replaced value
           start = ll+eval.length();
        }
     }
  } while (ll>=0);
  return str;
}

void set_errorlevel(int elvl) {
   char  errlvl[12];
   _utoa(elvl<0?255:elvl,errlvl,10);
   setenv("ERRORLEVEL",errlvl,1);
}

static void check_embedded(void) {
   if (!embedded) {
      embedded = new CmdList;
      embedded->SetText(internal_commands);
   }
}

u32t shl_extcall(spstr &cmd, TStrings &plist) {
   if (!ext_shell || !ext_shell->IsMember(cmd)) return EINVAL;
   /* str_getlist() must be used here, because it split spaces around =
      in parameter list */
   str_list* args = str_getlist(plist.Str);
   int        idx = ext_shell->IndexOf(cmd);
   cmd_eproc func = ext_shell->Objects(idx);
   // ext_shell content can be changed inside call
   u32t ii = func((char*)cmd(),args);
   free(args);
   return ii;
}

u32t cmd_shellcall(cmd_eproc func, const char *argsa, str_list *argsb) {
   if (!func) return EINVAL;
   spstr cmdname;
   // query command name by function ptr
   if (ext_shell) {
      int idx = ext_shell->IndexOfObject(func);
      if (idx>=0) cmdname = (*ext_shell)[idx];
   }
   // query MODE device name by function ptr
   if (ext_mode && !cmdname) {
      int idx = ext_mode->IndexOfObject(func);
      if (idx>=0) cmdname = (*ext_mode)[idx];
   }
   // and finally call it!
   if (!argsa && !argsb) {
      str_list nl;
      nl.count = 0;
      return (*func)(cmdname(),&nl);
   } else {
      TStrings arga, argb;
      if (argsa) arga.ParseCmdLine(argsa);
      if (argsb) str_getstrs(argsb,argb);
      if (argsa && argsb) arga.AddStrings(arga.Count(),argb);
      /* str_getlist() must be used here, because it split spaces around =
         in parameter list */
      str_list *lst = str_getlist(argsa?arga.Str:argb.Str);
      u32t rc = (*func)(cmdname(),lst);
      free(lst);

      return rc;
   }
}

static u32t cmd_process(spstr ln,session_info *si);

static u32t process_for(TStrings &plist, session_info *si) {
   u32t rc = 0;
   int ppos=0, isdir=0, isrec=0, isstep=0, relpath=0, breakable=1;

   while (!rc && plist.Max()>=ppos && plist[ppos][0]=='/') {
      char cmd1 = toupper(plist[ppos][1]),
           cmd2 = toupper(plist[ppos][2]);

      if (cmd1=='N' && cmd2=='B') breakable=0; else {
         if (plist[ppos].length()>2) rc=EINVAL;
         if (cmd1=='R') relpath=1; else
         if (cmd1=='S') {
            if (isstep) rc=EINVAL; else isrec=1;
         } else
         if (cmd1=='D') {
            if (isstep) rc=EINVAL; else isdir=1;
         } /*else
         if (cmd1=='L') {
            if (isrec||isdir) rc=EINVAL; else isstep=1;
         }*/
      }
      ppos++;
   }
   if (plist.Max()<ppos+4) rc=EINVAL;

   if (!rc) {
      char var[4];
      int  vps=0;
      if (plist[ppos][0]=='%'&&plist[ppos][1]=='%'&&plist[ppos].length()>2)
         var[vps++]='%';
      do {
         rc=EINVAL;
         if (plist[ppos].length()!=2+vps || plist[ppos][vps]!='%' ||
            !isalpha(plist[ppos][vps+1])) break;
         var[vps++] = '%';
         var[vps]   = plist[ppos][vps];
         var[++vps] = 0;

         if (stricmp(plist[++ppos](),"IN")!=0) break;
         ppos++;
         if (plist[ppos][0]!='(' || plist[ppos].lastchar()!=')') break;

         spstr mask(plist[ppos],1,plist[ppos].length()-2), dir, name;
         if (!mask.length()) break;
         if (stricmp(plist[++ppos](),"DO")!=0) break;
         ppos++;

         dir_t *info = 0;
         splitfname(mask,dir,name);
         u32t cnt = _dos_readtree(dir(),name(),&info,isrec,0,0);

         if (cnt) {
            TStrings flst,dlst;
            dir_to_list(dir,info,(isdir?0:_A_SUBDIR)|_A_VOLID,flst,isdir?&dlst:0);
            _dos_freetree(info);
            if (isdir) flst = dlst;
            dlst = plist;
            dlst.Delete(0,ppos);

            for (int ii=0;ii<flst.Count();ii++) flst[ii].replacechar('/','\\');

            if (relpath && dir.length()) {
               if (dir.lastchar()!='\\') dir+="\\";
               for (int ii=0;ii<flst.Count();ii++)
                  if (strncmp(flst[ii](),dir(),dir.length())==0)
                     flst[ii].del(0,dir.length());
            }

            for (int ii=0;ii<flst.Count();ii++) {
               TStrings rlst(dlst);
               /* replace it by words to take MergeCmdLine a chanáe
                  to put quotes around spaces */
               for (int ww=0; ww<rlst.Count(); ww++)
                  rlst[ww].replace(var,flst[ii]());
               spstr rln(rlst.MergeCmdLine());
               rlst.Clear();
               // check for ESC key pressed
               if (breakable)
                  if (key_pressed())
                     if ((key_read()&0xFF)==27) {
                        printf("Break this FOR command execution (y/n)?");
                        int yn = ask_yn();
                        if (yn==1) { printf("\nUser break!\n"); break; }
                           else printf("\n");
                     }
               u32t svline = si->nextline;
               rc = cmd_process(rln,si);

               if (rc==CMDR_RETEND || si->nextline!=svline+1) break;
               si->nextline = svline;
            }
         }
         rc=0;
      } while (0);
      if (rc) printf("\"%s\" is unexpected at this time\n",plist[ppos]());
   } else
      cmd_shellerr(rc,0);
   return rc;
}

static u32t cmd_process(spstr ln,session_info *si) {
   spstr   cmd=subst_env(ln,si).word(1).trim(), parm;
   int  noecho=cmd[0]=='@', linediff=1;
   u32t     rc=0, ii;
   int     ps2=ln.wordpos(2);
   if (ps2>0) parm=ln.right(ln.length()-ps2).trim();

   TStrings plist; // list of parameters
   plist.ParseCmdLine(parm);
   //log_printf("line %s (%s)\n", ln(), parm());
   if (!embedded) check_embedded();
   // echo command
   if (!noecho && (si->mode&MODE_ECHOOFF)==0 && ln[0]!=':') {
      vio_strout(ln()); vio_charout('\n');
   }
   // del @
   if (noecho) cmd.del(0,1);
   cmd.upper();
   // call help about internal commands
   if (embedded->IndexOf(cmd)>=0 && plist.IndexOf("/?")>=0) {
      plist[0] = cmd;
      cmd = "HELP";
   }
   // ugly written IF processing :(
   if (cmd=="IF") {
      if (!plist.Count()) {
         cmd_shellerr(EINVAL,0);
      } else {
         int icase = plist[0].upper()=="/I"?1:0, exec=0;
         if (icase) plist.Delete(0);
         int  _not = plist[0].upper()=="NOT"?1:0;
         if (_not) plist.Delete(0);
         spstr next(plist[0].upper());

         if (next=="EXIST"||next=="ERRORLEVEL"||next=="LOADED") {
            if (next[1]=='O') {
               exec = mod_query(plist[1](),MODQ_NOINCR)!=0;
            } else
            if (next[1]=='X') {
               char buf[QS_MAXPATH+1];
               *buf = 0;
               _searchenv(plist[1](), 0, buf);
               exec = *buf!=0;
            } else {
               exec = atoi(getenv("ERRORLEVEL"))>=plist[1].Int();
            }
            if (_not) exec=!exec;
            if (exec) {
               ps2=parm.wordpos(3+_not+icase);
               if (ps2>0) parm=parm.right(parm.length()-ps2).trim(); else
                  parm.clear();
            }
         } else {
            int quot=0, arg2=parm.wordpos(1+_not+icase);
            if (arg2>0) parm=parm.right(parm.length()-arg2).trim(); else
            if (arg2<0) parm.clear();

            arg2=0; ii=0;
            while (ii<parm.length())
               if (parm[ii]=='"') { quot=!quot; parm.del(ii); } else
               if (!quot&&parm[ii]=='='&&parm[ii+1]=='=') {
                  next=parm(0,ii); arg2=ii+=2;
               } else
               if (arg2&&(parm[ii]==' '||parm[ii]=='\t')) break;
                  else ii++;
            if (ii>=parm.length()) {
               printf("Incorrect IF statement in line %d\n",si->nextline);
            } else {
               spstr cmpa(parm,arg2,ii-arg2);
               exec = Xor(icase?stricmp(next(),cmpa())==0:next==cmpa,_not);
               if (exec) parm=parm.right(parm.length()-ii);
            }
         }
         // do no increment line below
         if (exec) return cmd_process(parm,si);
      }
   } else
   if (cmd=="ECHO") {
      if (stricmp(parm(),"ON")==0) {
         si->mode&=~MODE_ECHOOFF;
         printf("echo is on\n");
      } else
      if (stricmp(parm(),"OFF")==0) {
         si->mode|=MODE_ECHOOFF;
      } else
         printf("%s\n",parm());
   } else
   if (cmd=="GOTO") {
      for (ii=0;ii<=si->list.Max();ii++)
         if (si->list[ii].trim()[0]==':')
            if (stricmp(parm(),si->list[ii]()+1)==0) {
               linediff+=ii-si->nextline-1;
               break;
            }
   } else
   if (cmd=="GETKEY") {
      ii = parm.Int();
      if (ii) {
         ii = key_wait(ii); 
         // get default value
         if (ii==0 && plist.Count()>1) ii = plist[1].Dword();
      } else 
         while (log_hotkey(ii=key_read()));
      set_errorlevel(ii);
   } else
   if (cmd=="CLS") {
      vio_clearscr();
   } else
   if (cmd=="SET") {
      if (!parm) {
         str_list* env=str_getenv();
         for (ii=0;ii<env->count;ii++) printf("%s\n",env->item[ii]);
         free(env);
      } else {
         int vp=parm.wordpos(2,"=");
         if (vp>0) {
            spstr value;
            value=parm.right(parm.length()-vp).trim();
            setenv(parm.word(1,"=").trim()(), !value?0:value(),1);
         } else {
            str_list* env=str_getenv();
            int      once=0;
            for (ii=0;ii<env->count;ii++)
               if (strnicmp(parm(),env->item[ii],parm.length())==0) {
                  printf("%s\n",env->item[ii]);
                  once++;
               }
            free(env);
            if (!once) printf("Environment variable \"%s\" not defined\n",parm());
         }
      }
   } else
   if (cmd=="FOR") {
      rc = process_for(plist,si);
   } else
   if (cmd=="SHIFT") {
      u32t spos = 1;
      if (parm[0]=='/') spos = parm.Dword(1);
      if (spos<10) si->args.Delete(spos);
   } else
   if (cmd=="REM"||cmd[0]==':') {
   } else
   if (cmd=="PAUSE") {
      if (!parm) parm="Press any key when ready...";
      printf("%s\n",parm());
      while (log_hotkey(key_read()));
   } else
   if (cmd=="EXIT") {
      linediff=si->list.Count()-si->nextline;
      rc = CMDR_RETEND;
   } else
   if (cmd=="CALL") {
      u32t  size = 0;
      void   *bf = 0;

      if (plist.Count()>0) bf = freadfull(plist[0](),&size);
      if (!bf) {
         rc = get_errno();
         if (!rc) rc = ENOENT;
         printf(no_cmd_err,plist[0]());
      } else {
         char *bfnew = (char*)hlp_memrealloc(bf,size+1);
         TStrings btxt;
         btxt.SetText(bfnew,size);
         hlp_memfree(bfnew);
         si->nest = (session_info*)cmd_init2(btxt,plist);
      }
   } else
   // process installed external command
   if (ext_shell && ext_shell->IsMember(cmd)) {
      ii = shl_extcall(cmd,plist);
      if (ii<CMDR_NOFILE) set_errorlevel(ii); else rc = ii;
   } else
   if (cmd.length()) {
      u32t error=0;
      // correct TV direct A:..Z: path
      if (cmd[1]==':')
         if (cmd[0]>='A'&&cmd[0]<='Z') cmd[0]-='A'-'0';
      if (cmd[1]==':'&&cmd.length()==2) {
         error = hlp_chdisk(cmd[0]-'0')?EZERO:ENODEV;
         set_errorlevel(error);
         if (error) cmd_shellerr(error,0);
      } else {
         // launch or search module
         module* md=cmd[1]==':'?(module*)mod_load((char*)cmd(),0,&error,0):
                                (module*)mod_searchload(cmd(), &error);
         if (md) {
            char *env=envcopy(mod_context(), 0);
            s32t   rc=mod_exec((u32t)md, env, parm());
            if (rc<0) printf("Unable to launch module \"%s\"\n",cmd());
            free(env);
            mod_free((u32t)md);
            // set errorlevel env. var
            set_errorlevel(rc);
            rc = 0;
         } else {
            printf("Error loading module \"%s\"\n",cmd());
            spstr msg;
            msg.sprintf("_MOD%02d",error);
            cmd_shellhelp(msg(),0);
            set_errorlevel(error = ENOENT);
         }
      }
   }
   si->nextline+=linediff;
   return rc;
}

u32t _std cmd_run(cmd_state commands, u32t flags) {
   session_info *si = (session_info*)commands;
   if (!si||si->sign!=SESS_SIGN) return CMDR_RETERROR;
   u32t rc=0, svmode=si->mode;

   if (flags&CMDR_ECHOOFF) si->mode|=MODE_ECHOOFF;

   while (true) {
      //log_printf("calling line %d nest %x lines %d\n", si->nextline, si->nest,si->list.Count());
      if (si->nest) {
         rc=cmd_run(si->nest,flags|CMDR_NESTCALL);
         if (rc==CMDR_RETEND) {
            cmd_close(si->nest);
            si->nest=0;
            rc=0;
         }
         continue;
      } else
      if (si->nextline>=si->list.Count()) {
         rc=CMDR_RETEND; break;
      } else
         rc=cmd_process(si->list[si->nextline],si);

      if (flags&CMDR_ONESTEP) break;
   }
   // restore previous mode in nested cmd_runs
   if (flags&CMDR_NESTCALL) si->mode = svmode;
   return rc;
}

u32t _std cmd_getflags(cmd_state commands) {
   session_info *si = (session_info*)commands;
   if (!si||si->sign!=SESS_SIGN) return 0;
   u32t rc = 0;
   if ((si->mode&MODE_ECHOOFF)!=0) rc|=CMDR_ECHOOFF;
   return rc;
}

void _std cmd_close(cmd_state commands) {
   session_info *si = (session_info*)commands;
   if (!si||si->sign!=SESS_SIGN) return;
   if (si->nest) { cmd_close(si->nest); si->nest=0; }
   si->sign=0;
   delete si;
}

u32t _std cmd_exec(const char *file, const char *args) {
   TStrings cmd;
   if (!cmd.LoadFromFile(file)) {
      printf(no_cmd_err,file);
      return CMDR_NOFILE;
   }
   cmd_state  cst = cmd_init(cmd.GetTextToStr()(), args?args:file);
   u32t        rc = cmd_run(cst, CMDR_ECHOOFF);
   cmd_close(cst);
   return rc;
}

u32t _std cmd_execint(const char *section, const char *args) {
   TStrings    lst;
   spstr   autorun("exec_");
   autorun += section;
   // if section exists - then launch it!
   if (!ecmd_readsec(autorun, lst)) return CMDR_NOFILE;
   cmd_state  cst = cmd_init(lst.GetTextToStr()(), args?args:section);
   u32t        rc = cmd_run(cst, CMDR_ECHOOFF);
   cmd_close(cst);
   return rc;
}

int _std log_hotkey(u16t key) {
   u8t kh = key>>8;
   //log_printf("key %04X state: %08X\n", key, key_status());
   if ((key_status()&(KEY_CTRL|KEY_ALT))==(KEY_CTRL|KEY_ALT)) {
      switch (kh) {
         // Ctrl-Alt-F3: class list
         case 0x3D: case 0x60: case 0x6A: exi_dumpall(); return 1;
         // Ctrl-Alt-F4: file handles
         case 0x3E: case 0x61: case 0x6B: log_ftdump(); return 1;
         // Ctrl-Alt-F5: process tree
         case 0x3F: case 0x62: case 0x6C: mod_dumptree(); return 1;
         // Ctrl-Alt-F6: gdt dump
         case 0x40: case 0x63: case 0x6D: sys_gdtdump(); return 1;
         // Ctrl-Alt-F7: idt dump
         case 0x41: case 0x64: case 0x6E: sys_idtdump(); return 1;
         // Ctrl-Alt-F8: page tables dump
         case 0x42: case 0x65: case 0x6F: pag_printall(); return 1;
         // Ctrl-Alt-F9: dump pci config space
         case 0x43: case 0x66: case 0x70: log_pcidump(); return 1;
         // Ctrl-Alt-F10: dump module table
         case 0x44: case 0x67: case 0x71: log_mdtdump(); return 1;
         // Ctrl-Alt-F11: dump main memory table
         case 0x85: case 0x89: case 0x8B: hlp_memcprint(); hlp_memprint(); return 1;
         // Ctrl-Alt-F12: dump memory log
         case 0x86: case 0x8A: case 0x8C: memDumpLog("User request"); return 1;
      }
   }
   return 0;
}

static cmd_eproc common_add(const char *name, cmd_eproc proc, PCmdList &lst) {
   if (!name||!proc) return proc;
   spstr nm(name);
   if (!nm.trim().upper()) return proc;
   // we create it, but never delete ;)
   if (!lst) lst = new CmdList;
   int idx = lst->IndexOf(nm);
   cmd_eproc rc = idx>=0?lst->Objects(idx):0;
   lst->InsertObject(0,nm,proc);
   return rc;
}

static cmd_eproc common_rmv(const char *name, cmd_eproc proc, PCmdList lst) {
   if (!name||!lst) return 0;
   spstr nm(name);
   if (!nm.trim().upper()) return 0;
   int idx;
   if (proc) {
      idx = lst->IndexOfObject(proc);
      if (idx>=0) lst->Delete(idx);
   }
   idx = lst->IndexOf(nm);
   return idx>=0?lst->Objects(idx):0;
}

cmd_eproc _std cmd_shelladd(const char *name, cmd_eproc proc) {
   return common_add(name, proc, ext_shell);
}

cmd_eproc _std cmd_shellrmv(const char *name, cmd_eproc proc) {
   return common_rmv(name, proc, ext_shell);
}

int _std cmd_shellquery(const char *name) {
   spstr nm(name);
   nm.trim().upper();
   if (ext_shell&&ext_shell->IndexOf(nm)>=0) return 1;
   if (!embedded) check_embedded();
   if (embedded &&embedded ->IndexOf(nm)>=0) return 1;
   return 0;
}

str_list* _std cmd_shellqall(int ext_only) {
   CmdList lst;
   if (!ext_only&&!embedded) check_embedded();
   if (ext_shell) lst.AddStrings(0,*ext_shell);
   if (!ext_only&&embedded) lst.AddStrings(0,*embedded);
   lst.Sort();
   // remove duplicate names
   int ii = 0;
   while (ii<lst.Max())
      if (lst[ii]==lst[ii+1]) lst.Delete(ii+1); else ii++;
   return str_getlist(lst.Str);
}

cmd_eproc _std cmd_modeadd(const char *name, cmd_eproc proc) {
   return common_add(name, proc, ext_mode);
}

cmd_eproc _std cmd_modermv(const char *name, cmd_eproc proc) {
   return common_rmv(name, proc, ext_mode);
}

str_list* _std cmd_modeqall(void) {
   if (ext_mode) {
      TStrings lst(*ext_mode);
      lst.Sort();
      return str_getlist(lst.Str);
   }
   str_list *rc = (str_list*)malloc(sizeof(str_list));
   rc->count    = 0;
   rc->item[0]  = 0;
   return rc;
}

module* load_module(spstr &name, u32t *error) {
   name.trim().upper();
   // correct TV direct A:..Z: path
   if (name[1]==':')
      if (name[0]>='A'&&name[0]<='Z') name[0]-='A'-'0';
   // launch or search module
   return name[1]==':'?(module*)mod_load((char*)name(),0,error,0):
                       (module*)mod_searchload(name(),error);
}
