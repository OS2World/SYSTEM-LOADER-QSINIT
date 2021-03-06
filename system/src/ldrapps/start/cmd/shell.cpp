//
// QSINIT "start" module
// common shell functions
//
#include "qsbase.h"
#include "classes.hpp"
#include "stdlib.h"
#include "errno.h"
#include "syslocal.h"
#include "qsconst.h"
#include "time.h"
#include "qsint.h"

#define SESS_SIGN     0x53534553
#define MODE_ECHOOFF      0x0001
#define MODE_DLAUNCH      0x0002

static const char *no_cmd_err="Unable to load batch file \"%s\"\n";

typedef Strings<cmd_eproc>  CmdList;
typedef CmdList*           PCmdList;

static CmdList *ext_shell = 0, *ext_mode = 0;
// these are used without mt lock, so can`t be a Strings<>
static str_list *embedded = 0,
                 *intvars = 0;

static const char *internal_commands = "IF\nECHO\nGOTO\nGETKEY\nCLS\n"
                    "PUSHKEY\nSET\nFOR\nSHIFT\nREM\nPAUSE\nEXIT\nCALL",
                  // index in this list is used in env_getvar_int()
                  *internal_variables = "CD\nDATE\nTIME\nRANDOM\nSAFEMODE\n"
                    "LINES\nCOLUMNS\nRAMDISK\nDBPORT\nMTMODE\nPAEMODE\nSESNO";

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

static void check_embedded(void) {
   // this should be called on the first line of START.CMD
   if (!embedded) embedded = str_settext(internal_commands,0,0);
}

static void check_intvars(void) {
   // this should be called on the first line of START.CMD
   if (!intvars) intvars = str_settext(internal_variables,0,0);
}

static spstr env_getvar_int(spstr &ename, int shint) {
   if (!intvars) check_intvars();
   spstr eval;
   ename.upper();

   int idx = str_findentry(intvars,ename(),0,0);
   switch (idx) {
      case  0:   // CD
         getcwd(eval.LockPtr(NAME_MAX+1),NAME_MAX+1);
         eval.UnlockPtr();
         break;
      case  1:   // DATE
      case  2: { // TIME
         struct tm  tv;
         time_t    tme;
         time(&tme);
         localtime_r(&tme,&tv);
         if (idx==1)
            eval.sprintf("%02d.%02d.%04d",tv.tm_mday,tv.tm_mon+1,tv.tm_year+1900);
         else
            eval.sprintf("%02d:%02d:%02d",tv.tm_hour,tv.tm_min,tv.tm_sec);
         break;
      }
      case  3:   // RANDOM
         eval = random(32768);
         break;
      case  4:   // SAFEMODE
         eval = hlp_insafemode();
         break;
      case  5: { // LINES
         u32t lines = 25;
         vio_getmode(0, &lines);
         eval = lines;
         break;
      }
      case  6: { // COLUMNS
         u32t cols = 80;
         vio_getmode(&cols, 0);
         eval = cols;
         break;
      }
      case  7: { // RAMDISK
         char *dn = (char*)sto_data(STOKEY_VDNAME);
         eval = dn?dn:"?";
         break;
      }
      case  8: { // DBPORT
         u32t port = hlp_seroutinfo(0);
         if (port) eval = port;
         break;
      }
      case  9:   // MTMODE
         eval = mt_active();
         break;
      case 10:   // PAEMODE
         eval = in_pagemode;
         break;
      case 11:   // SESNO
         eval = se_sesno();
         break;
      default:
         env_lock();
         eval = getenv(ename());
         env_unlock();
   }
   return eval;
}

char* _std env_getvar(const char *name, int shint) {
   if (!name) return 0;
   spstr ename(name),
           res = env_getvar_int(ename, shint);
   char    *rc = 0;
   if (res.length()) mem_localblock(rc = strdup(res()));
   return rc;
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
        spstr ename(str,ll+1,epos-ll-1), eval, opstr;
        l     oppos = ename.cpos(':');

        if (oppos>=0) {
           opstr = ename.deltostr(oppos+1, ename.length()-oppos-1);
           ename.dellast();
        }
        if (!ename || ename.cpos(' ')>=0) start=epos;
        else {
           eval = env_getvar_int(ename, 1);
           // substring expansion/replacement (same as in WinXP cmd.exe)
           if (opstr.length()) {
              if (opstr[0]=='~') {
                int ofs = opstr.del(0).word_Int(1,","),
                    len = opstr.word_Int(2,",");
                if (len<=0) len += eval.length();
                if (ofs<0) ofs += eval.length();
                eval = eval(ofs,len);
              } else {
                 spstr src = opstr.word(1,"=");
                 if (src[0]=='*') {
                    l rps = eval.pos(src.del(0));
                    if (rps>0) eval.del(0,rps);
                 } else
                 if (src.lastchar()=='*') {
                    l rps = eval.pos(src.dellast());
                    if (rps>=0) {
                       rps+=src.length();
                       eval.del(rps, eval.length()-rps);
                    }
                 }
                 eval.replace(src,opstr.word(2,"="));
              }
           }
           // this thing replaces ALL same entries of %string:op% in the line!
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

u32t shl_extcall(spstr &cmd, TStrings &plist) {
   mt_swlock();
   if (!ext_shell || !ext_shell->IsMember(cmd)) {
      mt_swunlock();
      return EINVAL;
   } else {
      /* str_getlist() must be used here, because it trim spaces around =
         in parameter list */
      str_list* args = str_getlist(plist.Str);
      int        idx = ext_shell->IndexOf(cmd);
      cmd_eproc func = ext_shell->Objects(idx);
      mt_swunlock();
      // ext_shell content can be changed inside call
      u32t res = func((char*)cmd(),args);
      free(args);
      return res;
   }
}

extern "C"
u32t _std START_EXPORT(cmd_shellcall)(cmd_eproc func, const char *argsa, str_list *argsb) {
   if (!func) return EINVAL;
   spstr cmdname;
   mt_swlock();
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
   mt_swunlock();
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

static void process_set(spstr line) {
   if (!line) {
      str_list* env = str_getenv();
      for (u32t ii=0; ii<env->count; ii++) printf("%s\n", env->item[ii]);
      free(env);
   } else {
      if (line.word(1).upper()=="/P") {
         if (line.words()==1) printf("Variable name is missing\n"); else {
            line.del(0,line.wordpos(2));
            spstr  vn = line.word(1,"=").trim();
            if (vn.length()) {
               // allow set /p var = "text : "
               spstr prompt(line.word(2,"="));
               prompt.trim().unquote();
               printf(prompt());
               char *value = key_getstrex(0, -1, -1, -1, 0, 0);
               if (value) {
                  setenv(vn(), value, 1);
                  free(value);
               }
            }
         }
      } else {
         int vp = line.wordpos(2,"=");
         if (vp>0) {
            spstr   value = line.right(line.length()-vp).trim();
            setenv(line.word(1,"=").trim()(), !value?0:value(),1);
         } else {
            str_list* env = str_getenv();
            int      once = 0;
            for (u32t ii=0; ii<env->count; ii++)
               if (strnicmp(line(), env->item[ii], line.length())==0) {
                  printf("%s\n", env->item[ii]);
                  once++;
               }
            free(env);
            if (!once) printf("Environment variable \"%s\" not defined\n", line());
         }
      }
   }
}

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
         u32t cnt = _dos_readtree(dir(),name(),&info,isrec,0,0,0);

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
               /* replace it by words to take MergeCmdLine a chan�e
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
      cmd_shellerr(EMSG_CLIB,rc,0);
   return rc;
}

static u32t cmd_process(spstr ln, session_info *si) {
   spstr   cmd = subst_env(ln,si).word(1).trim(), parm;
   int  noecho = cmd[0]=='@', linediff=1;
   u32t     rc = 0, ii;
   int     ps2 = ln.wordpos(2);
   if (ps2>0) parm = ln.right(ln.length()-ps2).trim();

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
   if (plist.IndexOf("/?")>=0 && str_findentry(embedded,cmd(),0,0)>=0) {
      plist[0] = cmd;
      cmd = "HELP";
   }

   // ugly written IF processing :(
   if (cmd=="IF") {
      if (!plist.Count()) {
         cmd_shellerr(EMSG_CLIB,EINVAL,0);
      } else {
         plist[0].upper();
         // /b cannot be mixed with /d because micro-FSD has no dir/attrs support
         int icase = plist[0]=="/I", exec=0, bootvol = plist[0]=="/B",
             isdir = plist[0]=="/D";
         if (icase || bootvol || isdir) plist.Delete(0);
         int  _not = plist[0].upper()=="NOT"?1:0;
         if (_not) plist.Delete(0);
         spstr next(plist[0].upper());

         if (plist.Count()<3) {
            printf("Incomplete IF statement in line %d\n",si->nextline);
         } else
         if (next=="EXIST"||next=="ERRORLEVEL"||next=="LOADED"||next=="SIZE"||
            next=="FILESYSTEM")
         {
            int usedargs = 2;
            if (next[2]=='Z') {
               u64t size;
               if (bootvol) {
                  size = hlp_fopen(plist[1]());
                  if (size!=FFFF) hlp_fclose(); else size = 0;
               } else {
                  io_handle_info fi;
                  qserr iores = io_pathinfo(plist[1](), &fi);

                  if (iores || (fi.attrs&IOFA_DIR)) size = 0; else
                     size = fi.size;
               }
               usedargs++;
               exec = size >= str2uint64(plist[2]());
            } else
            if (next[2]=='A') {
               exec = mod_query(plist[1](),MODQ_NOINCR)!=0;
            } else
            if (next[2]=='I') {
               if (bootvol) {
                  exec = hlp_fexist(plist[1]())?1:0;
               } else {
                  io_handle_info fi;
                  qserr iores = io_pathinfo(plist[1](), &fi);
                  // without /d it accepts any type, with /d - dirs only
                  exec = iores==0 && (!isdir || (fi.attrs&IOFA_DIR));
               }
            } else
            if (next[2]=='L') {
               exec = fullpath(plist[1]);

               if (exec) {
                  disk_volume_data vi;
                  u8t vol = toupper(plist[1][0]) - 'A';

                  exec = io_volinfo(vol,&vi)==0;

                  if (exec) {
                     if (!vi.FsVer) vi.FsName[0] = 0;
                     exec = !stricmp(vi.FsName, plist[2]());
                     // allow FAT16/FAT32 for FAT string
                     if (!exec && plist[2].upper()=="FAT")
                        exec = !strnicmp(vi.FsName, "FAT", 3);
                  }
               }
               usedargs++;
            } else {
               exec = atoi(getenv("ERRORLEVEL"))>=plist[1].Int();
            }
            if (_not) exec=!exec;
            if (exec) {
               plist.Delete(0,usedargs);
               parm = plist.MergeCmdLine();
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
      int ps = parm.upper().pos("/R");
      if (ps>=0) {
         parm.del(ps,2);
         key_clear();
      }
      ii = parm.trim().Int();
      if (ii) {
         ii = key_wait(ii);
         // get default value
         if (ii==0 && plist.Count()>1) ii = parm.word_Dword(2);
      } else
         ii = key_read();
      set_errorlevel(ii);
   } else
   if (cmd=="PUSHKEY") {
      ii = parm.Dword();
      if (!ii) ii = 0x3920;
      key_push(ii);
   } else
   if (cmd=="CLS") {
      vio_clearscr();
   } else
   if (cmd=="SET") {
      process_set(parm);
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
      if (!parm) parm = "Press any key when ready...";
      printf("%s\n", parm());
      key_read();
   } else
   if (cmd=="EXIT") {
      linediff = si->list.Count()-si->nextline;
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
   } else {
      mt_swlock();
      int is_ext = ext_shell && ext_shell->IsMember(cmd);
      mt_swunlock();
      // process installed external command
      if (is_ext) {
         ii = shl_extcall(cmd,plist);
         if (ii<CMDR_NOFILE) set_errorlevel(ii); else rc = ii;
      } else
      if (cmd.length()) {
         u32t error=0;
         if (cmd[1]==':')
            if (cmd[0]>='0'&&cmd[0]<='9') cmd[0]+='A'-'0';
         if (cmd[1]==':'&&cmd.length()==2) {
            error = io_setdisk(cmd[0]-'A');
            set_errorlevel(error?ENODEV:EZERO);
            if (error) cmd_shellerr(EMSG_QS,error,0);
         } else {
            // launch or search module
            module* md=cmd[1]==':'?(module*)mod_load((char*)cmd(),0,&error,0):
                                   (module*)mod_searchload(cmd(),0,&error);
            if (md) {
               s32t rc;
               if (si->mode&MODE_DLAUNCH) {
                  qs_mtlib mt = get_mtlib();
                  error = mt ? mt->execse((u32t)md,0,parm(),QEXS_DETACH,0,0,0) :
                               E_MT_DISABLED;
                  rc    = qserr2errno(error);
               } else {
                  rc = mod_exec((u32t)md, 0, parm(), 0);
                  if (rc<0) printf("Unable to launch module \"%s\"\n", cmd());
               }
               // set errorlevel env. var
               set_errorlevel(rc);
               rc = 0;
            } else {
               printf("Error loading module \"%s\"\n", cmd());
               char *msg = cmd_shellerrmsg(EMSG_QS, error);
               if (msg) {
                  printf("(%s)\n", msg);
                  free(msg);
               }
               set_errorlevel(error = ENOENT);
            }
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
   if (flags&CMDR_DETACH) si->mode|=MODE_DLAUNCH;

   while (true) {
      //log_printf("calling line %d nest %x lines %d\n", si->nextline, si->nest,si->list.Count());
      if (si->nest) {
         rc = cmd_run(si->nest, flags|CMDR_NESTCALL);
         if (rc==CMDR_RETEND) {
            cmd_close(si->nest);
            si->nest=0;
            rc = 0;
         }
         continue;
      } else
      if (si->nextline>=si->list.Count()) {
         rc = CMDR_RETEND; break;
      } else
         rc = cmd_process(si->list[si->nextline], si);

      if (flags&CMDR_ONESTEP) break;
   }
   // restore previous mode in nested cmd_runs
   if (flags&CMDR_NESTCALL) si->mode = svmode;
   return rc;
}

u32t _std cmd_getflags(cmd_state commands) {
   session_info *si = (session_info*)commands;
   MTLOCK_THIS_FUNC lk; // this should guarantee block presence until end of func
   if (!si||si->sign!=SESS_SIGN) return 0;
   u32t rc = 0;
   if ((si->mode&MODE_ECHOOFF)!=0) rc|=CMDR_ECHOOFF;
   if ((si->mode&MODE_DLAUNCH)!=0) rc|=CMDR_DETACH;
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

static cmd_eproc common_add(const char *name, cmd_eproc proc, PCmdList &lst) {
   if (!name||!proc) return proc;
   spstr nm(name);
   if (!nm.trim().upper()) return proc;
   MTLOCK_THIS_FUNC lk;
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
   MTLOCK_THIS_FUNC lk;
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
   mt_swlock();
   int rc = ext_shell && ext_shell->IndexOf(nm)>=0;
   mt_swunlock();
   if (rc) return 1;
   if (!embedded) check_embedded();
   if (str_findentry(embedded,nm(),0,0)>=0) return 1;
   return 0;
}

str_list* _std cmd_shellqall(int ext_only) {
   CmdList lst;
   // fast lock of list access
   mt_swlock();
   if (ext_shell) lst.AddStrings(0,*ext_shell);
   mt_swunlock();
   int ii;
   if (!ext_only) {
      if (!embedded) check_embedded();
      for (ii=0; ii<embedded->count; ii++) lst.Add(embedded->item[ii]);
   }
   lst.Sort();
   // remove duplicate names
   ii = 0;
   while (ii<lst.Max())
      if (lst[ii]==lst[ii+1]) lst.Delete(ii+1); else ii++;
   return str_getlist_local(lst.Str);
}

cmd_eproc _std cmd_modeadd(const char *name, cmd_eproc proc) {
   return common_add(name, proc, ext_mode);
}

cmd_eproc _std cmd_modermv(const char *name, cmd_eproc proc) {
   return common_rmv(name, proc, ext_mode);
}

str_list* _std cmd_modeqall(void) {
   if (ext_mode) {
      MTLOCK_THIS_FUNC lk;
      TStrings lst(*ext_mode);
      lst.Sort();
      return str_getlist_local(lst.Str);
   }
   str_list *rc = (str_list*)malloc_local(sizeof(str_list));
   rc->count    = 0;
   rc->item[0]  = 0;
   return rc;
}

module* load_module(spstr &name, u32t *error) {
   name.trim().upper();
   // launch or search module
   return name[1]==':'?(module*)mod_load(name(), 0, error, 0):
                       (module*)mod_searchload(name(), 0, error);
}

u32t _std cmd_argtype(char *arg) {
   if (!arg || !*arg) return ARGT_UNKNOWN;
   spstr path(arg);
   path.upper();
   if (path[1]==':'&&(path.length()==2||path[2]=='\\'&&path.length()==3)) {
      return ARGT_DISKSTR;
   } else
   if (cmd_shellquery(path())) return ARGT_SHELLCMD; else {
      int search = 0, tryext = 0;
      spstr  dir, name;

      splitfname(path, dir, name);

      if (!dir) search = 1;
      if (name.cpos('.')<0) tryext = 1;
      // skip directories
      io_handle_info  fi;
      int exist = io_pathinfo(path(), &fi)==0;
      if (exist && (fi.attrs&IOFA_DIR)!=0) exist = 0;
      
      if (exist) fullpath(path); else {
         int    err = 0;
         spstr srch;
         // append possible extensions
         if (tryext) {
            srch=path; srch+=".EXE";
            // dir will be denied by our`s access on R_OK
            if (access(srch(), R_OK)) {
               srch=path; srch+=".CMD";
               if (access(srch(), R_OK)) err = 1;
            }
            if (!err) { path=srch; fullpath(path); }
         }
         // search in path
         if (search && (err||!tryext)) {
            char lp[QS_MAXPATH+1];
            err  = 0;
            _searchenv(path(), "PATH", lp);
            if (!lp[0])
               if (tryext) {
                  srch=path; srch+=".EXE";
                  _searchenv(srch(), "PATH", lp);
                  if (!lp[0]) {
                     srch=path; srch+=".CMD";
                     _searchenv(srch(), "PATH", lp);
                     if (!lp[0]) err = 1;
                  }
               } else err = 1;
            if (!err) path = lp;
         }
         if (err || path.length()>QS_MAXPATH) return ARGT_UNKNOWN;
      }
      spstr ext = path.upper().right(4);
      strcpy(arg, path());
      return ext==".CMD"||ext==".BAT"?ARGT_BATCHFILE:ARGT_FILE;
   }
}
