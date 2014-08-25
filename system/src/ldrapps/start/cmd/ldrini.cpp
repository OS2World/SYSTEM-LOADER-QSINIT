//
// QSINIT "start" module
// minor runtime functions
//
/**
@file ldrini.cpp
@ingroup CODE2
Minor runtime functions, C++ part.

@li ini file cached on first usage until 1st usage of another file
@li ini file can be loaded from virtual disk only.
@li os2ldr.ini is copied to the 1:\ on init
*/
#define LOG_INTERNAL
#define STORAGE_INTERNAL
#define SORT_BY_OBJECTS_ON
#include "qsutil.h"
#include "sp_ini.h"
#include "qsshell.h"
#include "qs_rt.h"
#include "qslog.h"
#include "time.h"
#include "stdlib.h"
#include "qsstor.h"
#include "qsinit.h"
#include "qsint.h"
#include "internal.h"

static const char *cfg_section="config",
                *shell_section="shell",
                     *ini_name="qsinit.ini";
static spstr msg_name,        // spstr do not use heap in constructor, so
            ecmd_name;        // can use static class here ...
/// ini file access cache
static TINIFile  *ini = 0,    // ini_* functions current file
                 *msg = 0,    // msg.ini
                *ecmd = 0;    // extcmd.ini

static char *bootstrs[4] = {"FAT", "FSD", "PXE", "SINGLE"};

static TINIFile *reinitini(const char *IniName) {
   u32t size;
   if (!IniName) return 0;
   if (ini&&IniName==ini->FileName()) return ini;
   delete ini;
   log_printf("caching %s!\n",IniName);
   ini=new TINIFile(IniName);
   return ini;
}

extern "C"
void reset_ini_cache(void) {
   if (ini) delete ini;
   ini = 0;
}

extern "C"
int get_ini_parms(void) {
   char *mainini = (char*)sto_data(STOKEY_INIDATA);
   u32t     size = sto_size(STOKEY_INIDATA);
   int        rc = 0;
   sto_del(STOKEY_INIDATA);

   u32t btype = hlp_boottype();
   if (btype>QSBT_NONE && btype<=QSBT_SINGLE) {
      setenv("BOOTTYPE",bootstrs[btype-1],1);
      log_printf("boot type: %s!\n",bootstrs[btype-1]);
   }

   if (mainini) {
      log_printf("ini: %d bytes!\n",size);
      TStrings ifl;
      ifl.SetText(mainini,size);
      hlp_memfree(mainini);
      if (!ifl.SaveToFile(ini_name)) {
         log_printf("failed to copy ini!\n");
         ini=new TINIFile(&ifl);
      } else
         reinitini(ini_name);
      if (ini) {
         char buf[24];
         u32t ini_dbport=ini->ReadInt(cfg_section,"DBPORT");
#if 0
         if (ini_dbport>0) {
            char buf[24];
            snprintf(buf,24,"0x%04X",ini_dbport);
            log_printf("dbport=%s\n",buf);
            setenv("DBPORT",buf,1);
         }
#endif
         u32t ini_pciscan=ini->ReadInt(cfg_section,"PCISCAN_ALL");
         sto_save(STOKEY_PCISCAN,&ini_pciscan,4,1);
         u32t ini_heapflags=ini->ReadInt(cfg_section,"HEAPFLAGS");
         memSetOptions(ini_heapflags&0xF);

         no_tgates = ini->ReadInt(cfg_section,"NOTASK");

         msg_name  = ini->ReadStr(shell_section, "EXTCOMMANDS");
         ecmd_name = ini->ReadStr(shell_section, "MESSAGES");

         rc = 1;
      }
   } else
      log_printf("no file!\n");
   // set default names if no ini file or key in it
   if (!msg_name)  msg_name  = "1:\\msg\\msg.ini";
   if (!ecmd_name) ecmd_name = "1:\\msg\\extcmd.ini";
   return rc;
}

extern "C"
void done_ini(void) {
   if (ini)  { delete ini; ini=0; }
   if (msg)  { delete msg; msg=0; }
   if (ecmd) { delete ecmd; ecmd=0; }
}

extern "C"
char* msg_readstr(const char *section, const char *key) {
   if (!msg) msg = new TINIFile(msg_name());
   spstr ss(msg->ReadString(section,key));
   ss.trim();
   return !ss?0:strdup(ss());
}

spstr ecmd_readstr(const char *section, const char *key) {
   if (!ecmd) ecmd = new TINIFile(ecmd_name());
   spstr ss(ecmd->ReadString(section,key));
   return ss.trim();
}

void ecmd_commands(TStrings &rc) {
   if (!ecmd) ecmd = new TINIFile(ecmd_name());
   rc = ecmd->ReadSectionKeys("COMMANDS");
   // trim spaces and empty lines
   rc.TrimEmptyLines();
   rc.TrimAllLines();
   for (int ii=0; ii<rc.Count(); ii++) rc[ii].upper();
}


#define makelist() {                                     \
   char *cc=ss.GetText("\x01");                          \
   res=strlen(cc)>BufSize-1?BufSize-2:strlen(cc);        \
   while (strlen(cc)>BufSize-1) *strrchr(cc,'\x01')=0;   \
   cc[strlen(cc)]=0;                                     \
   strcpy(Buf,cc);                                       \
   free(cc);                                             \
   char *cp=Buf;                                         \
   while (true) {                                        \
      cp=strchr(cp,'\x01');                              \
      if (cp) *cp++=0; else break;                       \
   }                                                     \
}

u32t _std ini_getstr(const char *Section, const char *Key, const char *Def,
                     char *Buf, u32t BufSize, const char *IniName)
{
   d res=0;
   /*if (Log_It) Log("GetPrivateProfileString(%s,%s,%s,%8.8x,%d,%s)",
     Section,Key,Def,Buf,BufSize,IniName);*/
   if (!reinitini(IniName)) {
      if (Section==NULL||Key==NULL) Buf[0]=0; else {
         strncpy(Buf,Def,BufSize-1);
         Buf[BufSize-1]=0;
         res=strlen(Buf);
      }
   } else {
      if (Section==NULL) {
         TStrings ss=ini->ReadSections();
         makelist();
      } else
      if (Key==NULL) {
         TStrings ss=ini->ReadSectionKeys(Section);
         makelist();
      } else {
         spstr ss=ini->ReadString(Section,Key,Def);
         res=ss.length()>BufSize-1?BufSize-1:ss.length();
         strncpy(Buf,ss(),BufSize-1);
         Buf[BufSize-1]=0;
      }
   }
   return res;
}

char* _std ini_readstr(const char *IniName, const char *Section, const char *Key) {
   spstr ss;
   if (!Section) {
      TStrings plain;
      if (!plain.LoadFromFile(IniName)) return 0;
      ss=plain.Value(Key);
   } else {
      if (!reinitini(IniName)) return 0;
      ss=ini->ReadString(Section,Key);
   }
   ss.trim();
   return !ss?0:strdup(ss());
}

u32t _std ini_setstr(const char *Section, const char *Key, const char *Str,
                     const char *IniName)
{
   if (!reinitini(IniName)) return false;
   ini->WriteString(Section,Key,Str);
   ini->Flush();
   return true;
}

u32t _std ini_setint(const char *Section, const char *Key, long Value,
                     const char *IniName)
{
   if (!reinitini(IniName)) return false;
   ini->WriteInteger(Section,Key,Value);
   ini->Flush();
   return true;
}


long _std ini_getint(const char *Section, const char *Key, long Value,
                     const char *IniName)
{
   spstr ss;
   char *cc=ss.LockPtr(128);
   d cnt=ini_getstr(Section,Key,"",cc,128,IniName);
   cc[cnt]=0;
   ss.UnlockPtr();
   l res=ss.length()?ss.Int():Value;
   return res;
}

// split string to items. result must be freed by single free() call
str_list* __stdcall str_split(const char *str,const char *separators) {
   TStrings lst;
   lst.SplitString(str,separators);
   for (int ii=0;ii<=lst.Max();ii++) lst[ii].trim();
   return str_getlist(lst);
}

str_list* __stdcall str_splitargs(const char *str) {
   TStrings lst;
   lst.ParseCmdLine(str);
   return str_getlist(lst);
}

static str_list* get_keylist(TINIFile  *ini, const char *Section, str_list**values) {
   TStrings lst=ini->ReadSectionKeys(Section);
   if (!values) {
      lst.TrimEmptyLines();
   } else {
      TStrings vlst = ini->ReadSectionValues(Section);
      int ii;
      while (ii<=lst.Max()) {
         spstr sk(lst[ii]), sv(vlst[ii]);
         if (!sk.trim().length() && !sv.trim().length())
            { lst.Delete(ii); vlst.Delete(ii); } else ii++;
      }
      *values = str_getlist(vlst);
   }
   return str_getlist(lst);
}

str_list* _std str_keylist(const char *IniName, const char *Section,
                           str_list**values)
{
   if (!reinitini(IniName)) return 0; else
      return get_keylist(ini, Section, values);
}

str_list* _std cmd_shellmsgall(str_list**values) {
   if (!msg) msg = new TINIFile(msg_name());
   return get_keylist(msg, "help", values);
}

str_list* _std str_seclist(const char *IniName) {
   if (!reinitini(IniName)) return 0; else {
      TStrings lst=ini->ReadSections();
      lst.TrimEmptyLines();
      return str_getlist(lst);
   }
}

str_list* _std str_getsec(const char *IniName,const char *Section, int noempty) {
   if (!reinitini(IniName)) return 0; else {
      TStrings lst=ini->ReadSection(Section);
      if (noempty) {
         l ii;
         lst.TrimEmptyLines();
         ii=lst.Count();
         while (ii-->0)
            if (lst[ii].trim()[0]==';') lst.Delete(ii);
      }
      return str_getlist(lst);
   }
}

char*_std str_gettostr(str_list*list, char *separator) {
   TStrings lst;
   str_getstrs(list,lst);
   return lst.GetText(separator?separator:"");
}

char*_std str_mergeargs(str_list*list) {
   TStrings lst;
   str_getstrs(list,lst);
   return strdup(lst.MergeCmdLine()());
}

/// create list from array of char*
str_list* _std str_fromptr(char **list,int size) {
   TStrings lst;
   if (list&&size>0)
      while (size--) lst.Add(*list++);
   return str_getlist(lst);
}

void _std log_printlist(char *info, str_list*list) {
   u32t ii, cnt = list?list->count:0;
   log_printf("%s [%08X, %d items]\n", info?info:"", list, cnt);
   for (ii=0; ii<cnt; ii++) {
      log_printf("%3d. %08X [%s]\n", ii+1, list->item[ii], list->item[ii]);
   }
}

struct getloginfo {
   u32t   flags;
   char     *rc;
};

static void _std getlog(log_header *log, void *extptr) {
   getloginfo *pli = (getloginfo*)extptr;
   int    puretext = (pli->flags&(LOGTF_DATE|LOGTF_TIME|LOGTF_LEVEL))==0;

   if (puretext) {
      spstr ltext;
      while (log->offset) {
         if ((log->flags&LOGIF_USED)!=0 && ((pli->flags&LOGTF_LEVEL)==0 ||
           (pli->flags&LOGTF_LEVELMASK)>=(log->flags&LOGIF_LEVELMASK)))
         {
            ltext+=(char*)(log+1);
         }
         log+=log->offset;
      }
      if (pli->flags&LOGTF_DOSTEXT) ltext.replace("\n","\r\n");
      // make copy a bit faster
      pli->rc = (char*)malloc(ltext.length()+1);
      memcpy(pli->rc, ltext(), ltext.length());
      pli->rc[ltext.length()] = 0;
   } else {
      TStrings    lst;
      struct tm    dt;
      u32t      pdtme = 0;
      time_t     ptme;

      while (log->offset) {
         if ((log->flags&LOGIF_USED)!=0 && ((pli->flags&LOGTF_LEVEL)==0 ||
           (pli->flags&LOGTF_LEVELMASK)>=(log->flags&LOGIF_LEVELMASK)))
         {
            if (pdtme!=log->dostime) {
               dostimetotm(pdtme=log->dostime,&dt);
               ptme = mktime(&dt);
            }
            spstr estr, astr;
      
            if (pli->flags&LOGTF_DATE)
               estr.sprintf("%02d.%02d.%02d ",dt.tm_mday,dt.tm_mon+1,dt.tm_year-100);
            if (pli->flags&LOGTF_TIME) {
               int sec = dt.tm_sec;
               if (log->flags&LOGIF_SECOND) sec++;
               astr.sprintf("%02d:%02d:%02d ",dt.tm_hour, dt.tm_min, sec);
               estr+=astr;
            }
            if (pli->flags&LOGTF_LEVEL) {
               if (pli->flags&LOGTF_FLAGS)
                  astr.sprintf("[%c%d%c] ",log->flags&LOGIF_REALMODE?'r':' ',
                     log->flags&LOGIF_LEVELMASK,log->flags&LOGIF_DELAY?'d':' ');
               else
                  astr.sprintf("[%d] ",log->flags&LOGIF_LEVELMASK);
               estr+=astr;
            }
            estr+=(char*)(log+1);
            if (estr.lastchar()=='\n') estr.dellast();
            lst.AddObject(estr,ptme);
         }
         log+=log->offset;
      }
      int ii,jj;
      const char *eol = pli->flags&LOGTF_DOSTEXT?"\r\n":"\n";
      
      log_it(2, "%d lines of log quered\n", lst.Count());
      
      for (ii=1;ii<lst.Count();ii++)
         // we`re can`t fill entire log for 3 seconds
         if (lst.Objects(ii-1)-lst.Objects(ii)>3) break;
      // re-order cyclic added lines
      if (ii<lst.Count()) {
         spstr t1 = lst.GetTextToStr(eol,ii),
               t2 = lst.GetTextToStr(eol,0,ii);
         pli->rc = (char*)malloc(t1.length()+t2.length()+1);
         memcpy(pli->rc, t1(), t1.length());
         memcpy(pli->rc+t1.length(), t2(), t2.length()+1);
      } else
         pli->rc = lst.GetText(eol);
   }
}

char* _std log_gettext(u32t flags) {
   getloginfo li = {flags, 0};
   log_query(getlog,&li);
   return li.rc;
}

spstr changeext(const spstr &src,const spstr &newext) {
  spstr dmp(src);
  int ps=dmp.crpos('.');
  if (dmp.crpos('/')>ps||dmp.crpos('\\')>ps) ps=-1;
  if (!newext) {
    if (ps>=0) dmp.del(ps,dmp.length()-ps);
    return dmp;
  }
  if (ps<0) { dmp+='.'; ps=dmp.length(); }
  if (ps<dmp.length()) dmp.del(ps+1,65536);
  return dmp+=newext;
}

char* _std _changeext(const char *name, const char *ext, char *path) {
   if (!path||!name) return 0;
   spstr rc(changeext(name,ext));
   strncpy(path,rc(),_MAX_PATH);
   path[_MAX_PATH-1] = 0;
   return path;
}

void splitfname(const spstr &fname, spstr &dir, spstr &name) {
  int ps=fname.crpos('/');
  if (ps<0) ps=fname.crpos('\\');
  if (ps<0) { dir.clear(); name=fname; return; }
  dir = fname(0,ps+1);
  dir.replacechar('/','\\');
  if (!(dir.length()==1 || dir.length()==3 && dir[1]==':')) dir.dellast();
  name= fname(ps+1,fname.length()-ps);
}

char* _std _splitfname(const char *fname, char *dir, char *name) {
   if (!fname||!name) return 0;
   spstr _dir, _name;
   splitfname(fname,_dir,_name);
   if (dir) {
      strncpy(dir,_dir(),_MAX_PATH);
      dir[_MAX_PATH-1] = 0;
   }
   strncpy(name,_name(),_MAX_PATH);
   name[_MAX_PATH-1] = 0;
   return name;
}

//**********************************************************************

struct sto_entry {
   u32t      size;
   u32t   isalloc;
   void     *data;
   u32t   ddvalue;
};

static Strings <sto_entry*> *storage = 0;

void _std sto_save(const char *entry, void *data, u32t len, int copy) {
   if (!storage||!entry) return;
   int idx = storage->IndexOfICase(entry);
   sto_entry *ste = idx>=0?storage->Objects(idx):0;
   // delete entry
   if (!data) {
      if (!ste) return;
      if (ste->isalloc && ste->data) free(ste->data);
      delete ste;
      storage->Delete(idx);
   } else {
      if (!ste) ste = new sto_entry; else
      if (ste->isalloc && ste->size>4) free(ste->data);

      if (!copy) ste->data = data; else
      if (len>4) {
         ste->data = malloc(len);
         memcpy(ste->data,data,len);
      } else {
         ste->data = 0;
         ste->ddvalue = *((u32t*)data);
      }
      ste->size    = len;
      ste->isalloc = copy;
      if (idx<0) storage->AddObject(entry,ste); else
         storage->Objects(idx) = ste;
   }
}

void _std sto_flush(void) {
   int ii;
   stoinit_entry *stl = sto_init();

   for (ii=0;ii<STO_BUF_LEN;ii++) {
      stoinit_entry *ste = stl+ii;
      if (ste->name[0]) {
         void *data = ste->isptr?(void*)ste->data:&ste->data;
         // log_printf("key %s(%d): %4lb\n", ste->name, ste->len, data);
         sto_save(ste->name, data, ste->len, !ste->isptr);
         ste->name[0] = 0;
      }
   }
}

static sto_entry *sto_find(const char *entry) {
   if (!storage||!entry) return 0;
   int idx = storage->IndexOfICase(entry);
   if (idx<0) return 0;
   return storage->Objects(idx);
}

u32t _std sto_dword(const char *entry) {
   sto_entry *ste = sto_find(entry);
   if (!ste) return 0;
   return ste->isalloc && ste->size<=4?ste->ddvalue:*((u32t*)ste->data);
}

/// get pointer to stored data
void *_std sto_data(const char *entry) {
   sto_entry *ste = sto_find(entry);
   if (!ste) return 0;
   return ste->isalloc && ste->size<=4?&ste->ddvalue:ste->data;
}

/// get size of to stored data
u32t  _std sto_size(const char *entry) {
   sto_entry *ste = sto_find(entry);
   return ste?ste->size:0;
}

extern "C"
void setup_storage() {
   if (storage) return;
   storage = new Strings <sto_entry*>;
   sto_flush();
}

u32t _std hlp_insafemode(void) {
   return sto_dword(STOKEY_SAFEMODE);
}

//**********************************************************************

#define AllowSet " ^\r\n"
#define CarrySet "-"

void splittext(const char *text, u32t width, TStrings &lst) {
   lst.Clear();
   if (!text || width<8) return;
   const char *ps = text, *pps;
   int         ln = 0;
   lst.Add(spstr());
   do {
      while (*ps&&strchr(AllowSet,*ps)) {
         register char cr=*ps++;
         if (cr!=' ') { 
            lst.Add(spstr()); 
            if (lst[ln].lastchar()==' ') lst[ln].dellast();
            if (cr=='\r'&&*ps=='\n') ps++; 
            ln++; 
         }
      }
      if (*ps==0) break;
      pps = strpbrk(ps, CarrySet AllowSet);
      int carry = pps&&strchr(CarrySet,*pps)?1:0;
      spstr curr(ps,pps?pps-ps+carry:0), sum;
      sum = lst[ln]+curr;
      sum.expandtabs(4);
      if (sum.length()<width-2) lst[ln]=sum; else {
         lst.Add(curr); 
         if (lst[ln].lastchar()==' ') lst[ln].dellast(); 
         ln++; 
      }
      if (!carry||pps&&pps[1]==' ') lst[ln]+=' ';
      ps=pps;
      if (carry) ps++;
   } while (pps);
}