//
// QSINIT "start" module
// minor runtime functions
//
/**
@file ldrini.cpp
@ingroup CODE2
*/
#define LOG_INTERNAL
#define STORAGE_INTERNAL
#define SORT_BY_OBJECTS_ON
#include "syslocal.h"
#include "qsbase.h"
#include "sp_ini.h"
#include "qs_rt.h"
#include "time.h"
#include "qsinit.h"
#include "qsint.h"
#include "errno.h"
#include "sysio.h"
#include "qcl/qslist.h"

static const char *cfg_section = "config",
                *shell_section = "shell",
                     *ini_name = "qsinit.ini",
                     *msg_path = "b:\\msg\\msg.ini",
                  *msg_section = "help";
#define ECMD_PATH                "b:\\msg\\extcmd.ini"
static spstr msg_name,        // spstr do not use heap in constructor, so
            ecmd_name;        // can use static class here ...
                              // (no heap in START`s init)
/// ini file access cache
static TINIFile  *msg = 0,    // msg.ini
                *ecmd = 0;    // extcmd.ini

static char *bootstrs[4] = {"FAT", "FSD", "PXE", "SINGLE"};
static char *hoststrs[4] = {"BIOS", "EFI"};

static TINIFile *new_ini(const char *IniName) {
   return new TINIFile(IniName);
}

extern "C"
int get_ini_parms(void) {
   char *mainini = (char*)sto_data(STOKEY_INIDATA);
   u32t     size = sto_size(STOKEY_INIDATA);
   u32t  pciscan = 0;
   int        rc = 0;
   spstr  dbcard;
   // init was saved by pointer, not as data copy
   sto_del(STOKEY_INIDATA);

   /* we have root environment here, so just storing keys for any other
      feature apps */
   u32t htype = hlp_hosttype();
   if (htype>=QSHT_BIOS && htype<=QSHT_EFI) {
      // scan all PCI buses on EFI by default
      if (htype==QSHT_EFI) pciscan = 1;
      setenv("HOSTTYPE", hoststrs[htype], 1);
   }

   u32t btype = hlp_boottype();
   if (btype>QSBT_NONE && btype<=QSBT_SINGLE) {
      setenv("BOOTTYPE", bootstrs[btype-1], 1);
      log_printf("boot type: %s!\n", bootstrs[btype-1]);
   }

   if (mainini) {
      log_printf("ini: %d bytes!\n",size);
      TStrings  ifl;
      ifl.SetText(mainini,size);
      hlp_memfree(mainini);
      if (!ifl.SaveToFile(ini_name)) log_printf("failed to copy ini!\n");

      TINIFile  ini(&ifl);
      dbcard    = ini.ReadStr(cfg_section,"DBCARD");
      pciscan   = ini.ReadInt(cfg_section, "PCISCAN_ALL", pciscan);
      no_tgates = ini.ReadInt(cfg_section,"NOTASK");
      msg_name  = ini.ReadStr(shell_section, "MESSAGES");
      ecmd_name = ini.ReadStr(shell_section, "EXTCOMMANDS");
      mod_delay = ini.ReadInt(cfg_section, "UNZALL", 0) ? 0 : 1;
      ctrln_d0  = ini.ReadInt(cfg_section,"CTRLN");
      rc        = ini.ReadInt(cfg_section,"REIPL");
      if (rc>0) reipltime = rc;
      // disable clock in menu
      rc        = ini.ReadInt(cfg_section,"NOCLOCK");
      if (rc>0 || hlp_insafemode()) setenv("MENU_NO_CLOCK", "YES", 1); else
         if (rc<0) env_setvar("MENU_NO_CLOCK", -1);

      u32t heapflags = ini.ReadInt(cfg_section,"HEAPFLAGS");
      mem_setopts(heapflags&0xF);

      rc = 1;
   } else
      log_printf("no file!\n");
   // save value
   sto_savedword(STOKEY_PCISCAN, pciscan);
   // set default names if no ini file or key in it
   if (!msg_name)  msg_name  = msg_path;
   if (!ecmd_name) ecmd_name = ECMD_PATH;
   // call DBCARD processing (only after pciscan was saved!)
   if (dbcard.length()) {
      rc = shl_dbcard(dbcard, log_printf);
      if (rc) log_printf("DBCARD setup err %d!\n", rc);
   }

   /* inherited shell history after RESTART command? */
   char *cmdh = (char*)sto_data(STOKEY_CMDHINIT);
   if (cmdh) {
      TStrings lst;
      lst.SetText(cmdh, sto_size(STOKEY_CMDHINIT));
      lst.TrimEmptyLines();
      hlp_memfree(cmdh);
      sto_del(STOKEY_CMDHINIT);

      if (lst.Count()) {
         ptr_list hl = NEW_GM(ptr_list);
         // this will be PID 1 onwed block - a safe case
         for (u32t ii=0; ii<lst.Count(); ii++) hl->add(strdup(lst[ii]()));
         // save pointer with 1 byte len (size ignored)
         sto_save(STOKEY_CMDSTORY, hl, 1, 0);
      }
   }
   return rc;
}

static void init_msg_ini(void) {
   if (!msg) {
      TStrings     ht;
      spstr   secname("help");
      msg = new_ini(msg_name());

      msg->ReadSection(secname, ht);
      /* merge strings, ended by backslash. ugly, but allows more friendly
         editing of msg.ini */
      l ii=0, lp;
      while (ii<ht.Count()) {
         if (ht[ii].trim().lastchar()=='\\') {
            ht[ii].dellast()+=ht.MergeBackSlash(ii+1,&lp);
            ht.Delete(ii+1, lp-ii-1);
         }
         ii++;
      }
      msg->WriteSection(secname, ht, true);
   }
}


extern "C"
char* msg_readstr(const char *section, const char *key) {
   MTLOCK_THIS_FUNC _lk;
   if (!msg) init_msg_ini();
   spstr ss(msg->ReadString(section,key));
   ss.trim();
   return !ss?0:strdup(ss());
}

void _std cmd_shellsetmsg(const char *topic, const char *text) {
   MTLOCK_THIS_FUNC _lk;
   if (!msg) init_msg_ini();
   msg->WriteString(msg_section, topic, text);
}

char* _std cmd_shellgetmsg(const char *cmd) {
   MTLOCK_THIS_FUNC _lk;
   char *msg = msg_readstr(msg_section, cmd);
   if (!msg) {
      log_it(3,"no help str \"%s\"\n",cmd);
      return 0;
   }
   int cnt=0;
   while (msg[0]=='@'&&msg[1]=='@') {
      char *redirect = msg_readstr("help", msg+2);
      if (redirect) { free(msg); msg=redirect; }
      // too redirected!
      if (++cnt==16) {
         if (msg) free(msg);
         return 0;
      }
   }
   return msg;
}

spstr ecmd_readstr(const char *section, const char *key) {
   MTLOCK_THIS_FUNC _lk;
   if (!ecmd) ecmd = new_ini(ecmd_name());
   spstr sn(section), kn(key), ss(ecmd->ReadString(sn.trim(),kn.trim()));
   return ss.trim();
}

/** read section from extcmd.ini.
    @param       section   Section name
    @param [out] lst       Section text
    @return success flag (1/0), i.e. existence flag. */
int ecmd_readsec(const spstr &section, TStrings &lst) {
   MTLOCK_THIS_FUNC _lk;
   if (!ecmd) ecmd = new_ini(ecmd_name());
   ecmd->ReadSection(section,lst);
   return ecmd->SectionExists(section);
}

void ecmd_commands(TStrings &rc) {
   MTLOCK_THIS_FUNC _lk;
   if (!ecmd) ecmd = new_ini(ecmd_name());
   ecmd->ReadSectionKeys("COMMANDS",rc);
   // trim spaces and empty lines
   rc.TrimEmptyLines();
   rc.TrimAllLines();
   for (int ii=0; ii<rc.Count(); ii++) rc[ii].upper();
}

/// convert TStrings to string list for usage in pure C code
str_list* str_getlist_local(TStringVector &lst) {
   str_list *res = str_getlist(lst);
   mem_localblock(res);
   return res;
}

// split string to items. result must be freed by single free() call
str_list* __stdcall str_split(const char *str,const char *separators) {
   TStrings lst;
   lst.SplitString(str,separators);
   for (int ii=0;ii<=lst.Max();ii++) lst[ii].trim();
   return str_getlist_local(lst.Str);
}

str_list* _std str_settext(const char *text, u32t len) {
   TStrings lst;
   lst.SetText(text,len);
   for (int ii=0;ii<=lst.Max();ii++) lst[ii].trimright();
   return str_getlist_local(lst.Str);
}

str_list* __stdcall str_splitargs(const char *str) {
   TStrings lst;
   lst.ParseCmdLine(str);
   return str_getlist_local(lst.Str);
}

str_list* _std str_getargs(u32t first, u32t last) {
   TStrings lst;
   if (first<=last) {
      process_context *pq = mod_context();
      char         **argv = (char**)pq->rtbuf[RTBUF_ARGV];
      u32t           argc = pq->rtbuf[RTBUF_ARGC];

      // such method makes list _exactly_ the same with argv array
      if (argv && argc)
         while (first<=last && first<argc) lst.Add(argv[first++]);
   }
   return str_getlist_local(lst.Str);
}

void _std str_delentry(str_list *list, u32t index) {
   if (!list) return;
   if (index>=list->count) return;

   if (index<list->count-1)
      memmove(&list->item[index], &list->item[index+1], (list->count-index-1)*
         sizeof(ptr));
   list->count--;
}

str_list* get_keylist(TINIFile  *ini, const char *Section, str_list**values) {
   TStrings lst;
   ini->ReadSectionKeys(Section,lst);
   if (!values) {
      lst.TrimEmptyLines();
   } else {
      TStrings vlst;
      ini->ReadSectionValues(Section,vlst);
      int ii = 0;
      while (ii<=lst.Max()) {
         spstr sk(lst[ii]), sv(vlst[ii]);
         if (!sk.trim().length() && !sv.trim().length())
            { lst.Delete(ii); vlst.Delete(ii); } else ii++;
      }
      *values = str_getlist_local(vlst.Str);
   }
   return str_getlist_local(lst.Str);
}

str_list* _std cmd_shellmsgall(str_list**values) {
   MTLOCK_THIS_FUNC _lk;
   if (!msg) init_msg_ini();
   return get_keylist(msg, "help", values);
}

char* _std str_gettostr(const str_list*list, char *separator) {
   TStrings lst;
   str_getstrs(list,lst);
   char *res = lst.GetText(separator?separator:"");
   mem_localblock(res);
   return res;
}

char* _std str_mergeargs(str_list*list) {
   TStrings lst;
   str_getstrs(list,lst);
   return strdup(lst.MergeCmdLine()());
}

str_list* _std str_copylines(str_list*list, u32t first, u32t last) {
   TStrings lst;
   if (first<=last) {
      str_getstrs(list,lst);
      if (lst.Count())
         if (first>=lst.Count()) lst.Clear(); else {
            if (last>=lst.Count()) last=lst.Max();
            if (++last!=lst.Count()) lst.Delete(last,lst.Count()-last);
            if (first) lst.Delete(0,first);
         }
   }   
   return str_getlist_local(lst.Str);
}

str_list* _std str_newentry(str_list*list, u32t pos, const char *str,
                            char delimeter, int freelist)
{
   TStrings lst;
   if (list) {
      str_getstrs(list,lst);
      if (freelist) free(list);
   }
   if (pos>lst.Count()) pos = lst.Count();
   if (str) {
      const char *ep = str-1;
      while (ep) {
         ep = strchr(str=++ep, delimeter);
         if (ep) {
            lst.Insert(pos++, spstr(str, ep-str));
            if (!delimeter && ep[1]==0) break;
         } else
            lst.Insert(pos++, spstr(str));
      }
   }
   return str_getlist_local(lst.Str);
}

/// create list from array of char*
str_list* _std str_fromptr(char **list, int size) {
   TStrings lst;
   if (list&&size>0)
      while (size--) lst.Add(*list++);
   return str_getlist_local(lst.Str);
}

// is key present? for subsequent search of the same parameter
char* _std str_findkey(str_list *list, const char *key, u32t *pos) {
   u32t ii=pos?*pos:0, len = strlen(key);
   if (!list||ii>=list->count) return 0;
   for (;ii<list->count;ii++) {
      u32t plen = strlen(list->item[ii]);
      if (plen>len+1 && list->item[ii][len]=='=' || plen==len)
         if (strnicmp(list->item[ii],key,len)==0) {
            char *rc=list->item[ii]+len;
            if (*rc=='=') rc++;
            if (pos) *pos=ii;
            return rc;
         }
   }
   return 0;
}

int _std str_findentry(str_list *list, const char *str, u32t pos, int icase) {
   u32t len = strlen(str);
   int _std (*cf)(const char*, const char*, u32t) = icase?strnicmp:strncmp;

   if (!list || pos>=list->count) return -1;
   for (;pos<list->count;pos++) {
      char *li = list->item[pos];
      if (li) {
         u32t plen = strlen(li);
         if (plen==len && cf(li,str,len)==0) return pos;
      }
   }
   return -1;
}

char* _std str_keyvalue(const char *src, char **key) {
   if (key) *key = 0;
   if (!src) return 0;
   spstr str(src);
   // replace tabs to spaces
   str.replacechar('\t', ' ');
   if (key) {
      spstr keystr = str.word(1,"=").trim();
      *key = strdup(keystr());
   }
   l pos = str.cpos('=');
   if (pos<0) return (char*)src + str.length();
   while (str[++pos]==' ') ;
   return (char*)src + pos;
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

      log_it(2, "%d lines of log queried\n", lst.Count());

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
   while (log_query(getlog,&li)==EBUSY) mt_yield();
   // change block context to current process
   if ((flags&LOGTF_SHARED)==0) mem_localblock(li.rc);
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
   strncpy(path, rc(), _MAX_PATH);
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
   if (!fname || !name&&!dir) return 0;
   spstr _dir, _name;
   splitfname(fname, _dir, _name);
   if (dir) {
      strncpy(dir, _dir(), _MAX_PATH);
      dir[_MAX_PATH-1] = 0;
   }
   if (name) {
      strncpy(name, _name(), _MAX_PATH);
      name[_MAX_PATH-1] = 0;
      return name;
   } else
      return dir;
}

void __stdcall bswap(void*mem, size_t size, size_t len) {
   // just re-use of sprtlib function
   memswap(mem, size, len);
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
   MTLOCK_THIS_FUNC _lk;

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

void _std sto_savedword(const char *entry, u32t value) {
   sto_save(entry, &value, 4, 1);
}

void _std sto_flush(void) {
   MTLOCK_THIS_FUNC _lk;

   int ii, cnt = 0;
   stoinit_entry *stl = sto_init();

   spstr keylist;

   for (ii=0;ii<STO_BUF_LEN;ii++) {
      stoinit_entry *ste = stl+ii;
      if (ste->name[0]) {
         char  lbuf[64];
         void *data = ste->isptr?(void*)ste->data:&ste->data;
         /* this dump is always useful because of limit on key length in the
            init code */
         snprintf(lbuf, 64, "%s(%u),", ste->name, ste->len);
         keylist += lbuf;
         sto_save(ste->name, data, ste->len, !ste->isptr);
         ste->name[0] = 0;
         cnt++;
      }
   }
   if (cnt) {
      keylist.dellast();
      log_printf("%d keys added:\n", cnt);
      log_printf("%s\n", keylist());
   }
}

// locked by callers
static sto_entry *sto_find(const char *entry) {
   if (!storage||!entry) return 0;
   int idx = storage->IndexOfICase(entry);
   if (idx<0) return 0;
   return storage->Objects(idx);
}

u32t _std sto_dword(const char *entry) {
   MTLOCK_THIS_FUNC _lk;
   static u32t mask[4] = {0, 0xFF, 0xFFFF, 0xFFFFFF};
   u32t       value;


   sto_entry *ste = sto_find(entry);
   if (!ste) return 0;
   value = ste->isalloc && ste->size<=4?ste->ddvalue:*((u32t*)ste->data);
   if (ste->size>=4) return value;
   return value & mask[ste->size];
}

/// get pointer to stored data
void *_std sto_data(const char *entry) {
   MTLOCK_THIS_FUNC _lk;

   sto_entry *ste = sto_find(entry);
   if (!ste) return 0;
   return ste->isalloc && ste->size<=4?&ste->ddvalue:ste->data;
}

void *_std sto_datacopy(const char *entry, u32t *psize) {
   MTLOCK_THIS_FUNC _lk;

   sto_entry *ste = sto_find(entry);
   if (psize) *psize = 0;
   if (!ste) return 0;
   void *rc = malloc_local(ste->size);
   if (psize) *psize = ste->size;
   if (!rc) return 0;
   memcpy(rc, ste->isalloc && ste->size<=4?&ste->ddvalue:ste->data, ste->size);
   return rc;
}

/// get size of to stored data
u32t  _std sto_size(const char *entry) {
   MTLOCK_THIS_FUNC _lk;

   sto_entry *ste = sto_find(entry);
   return ste?ste->size:0;
}

str_list* _std sto_list(const char *match, int type) {
   if (!storage) return 0;

   MTLOCK_THIS_FUNC _lk;
   TStrings res;
   u32t      ii;

   for (ii=0; ii<storage->Count(); ii++)
      if (!match || !*match || _matchpattern(match,(*storage)[ii]()))
         if (type) {
            sto_entry *ste = storage->Objects(ii);
            res.Add((ste->isalloc?'*':'@') + (*storage)[ii]);
         } else
            res.Add((*storage)[ii]);

   if (!res.Count()) return 0;

   return str_getlist_local(res.Str);
}

extern "C"
void setup_storage() {
   // init function, there is no file i/o and many other things here!
   if (storage) return;
   storage = new Strings <sto_entry*>;
   sto_flush();
}

u32t _std hlp_insafemode(void) {
   if (!storage) return 0;

   static int sfcache = -1;
   if (sfcache<0) sfcache = sto_dword(STOKEY_SAFEMODE);
   return sfcache;
}

//**********************************************************************

#define AllowSet " ^\r\n"
#define CarrySet "-"

void splittext(const char *text, u32t width, TStrings &lst, u32t flags) {
   lst.Clear();
   if (!text || width<8) return;
   const char *ps = text, *pps;
   char    softcr = flags&SplitText_HelpMode?'&':0;
   int         ln = 0;
   spstr linehead;
   lst.Add(spstr());
   do {
      while (*ps && (strchr(AllowSet,*ps) || *ps==softcr)) {
         register char cr=*ps++;
         if (cr!=' ') {
            if (cr==softcr) {
               pps = ps;
               if (*pps) pps++;
               while (*pps==' ' || *pps=='\t') pps++;
               /* get 1 char after eol and all spaces after it and use result
                  as a header for all next lines until real cr */
               linehead = spstr(ps, pps-ps).expandtabs(4);
               ps = pps;
            } else {
               lst.Add(spstr());
               if (cr=='\r' && *ps=='\n') ps++;
               linehead.clear();
               if (lst[ln].lastchar()==' ') lst[ln].dellast();
               ln++;
            }
         }
      }
      if (*ps==0) break;
      pps = strpbrk(ps, softcr ? CarrySet AllowSet "&" : CarrySet AllowSet);
      int carry = pps&&strchr(CarrySet,*pps)?1:0;
      spstr curr(ps,pps?pps-ps+carry:0), sum;
      sum = lst[ln]+curr;
      sum.expandtabs(4);

      if ((flags&SplitText_NoAnsi?strlen:str_length)(sum())<width-2)
         lst[ln]=sum;
      else {
         lst.Add(linehead+curr);
         if (lst[ln].lastchar()==' ') lst[ln].dellast();
         ln++;
      }
      if (!carry||pps&&pps[1]==' ') lst[ln]+=' ';
      ps=pps;
      if (carry) ps++;
   } while (pps);
}

str_list* _std cmd_splittext(const char *text, u32t width, u32t flags) {
   TStrings res;
   splittext(text, width, res, flags);
   return str_getlist_local(res.Str);
}

/// make full path
int fullpath(spstr &path) {
   spstr rc;
   int  res = _fullpath(rc.LockPtr(NAME_MAX+2), path(), NAME_MAX+2)?1:0;
   rc.UnlockPtr();
   if (res) path = rc;
   return res;
}

/// get current directory
int getcurdir(spstr &path) {
   char *cp = path.LockPtr(_MAX_PATH+1);
   int  res = io_curdir(cp, _MAX_PATH+1)?0:1;
   if (!res) *cp = 0;
   path.UnlockPtr();
   return res;
}

/// get current directory of active process (string is malloc-ed)
char* _std getcurdir_dyn(void) {
   spstr cd;
   if (!getcurdir(cd)) return 0;
   char *rc = strdup(cd());
   __set_shared_block_info(rc, "getcurdir()", 0);
   return rc;
}

// save code size a bit
#include "pubini.cpp"

//**********************************************************************

/** return FS detection list.
    Function returns parsed [fsdetect] section contents, in the single module
    owned heap block. Last entry in this list is zero-filled */
fs_detect_list* ecmd_readfsdetect() {
   TStrings lst;
   if (!ecmd_readsec("fsdetect",lst)) return 0;

   u32t ii, blen = 0, nlen = 0, ecnt = 0;
   for (ii=0; ii<lst.Count(); ii++) {
      // filter out commented lines
      if (lst[ii][0]!=';') {
         spstr val = lst.Value(ii);
         if (val.words(",")==2) {
            blen += val.word(2,",").words();
            nlen += lst.Name(ii).trim().length()+1;
            ecnt++;
            continue;
         }
      }
      lst[ii].clear();
   }
   if (!blen || !nlen) return 0;
   // alloc/calc block pointers
   u32t        listlen = sizeof(fs_detect_list)*(ecnt+1), idx;
   fs_detect_list *res = (fs_detect_list*)malloc(listlen+blen+nlen);
   char          *nptr = (char*)res + listlen;
   u8t           *dptr = (u8t*)nptr + nlen;
   // zero last entry
   memset(res+ecnt, 0, sizeof(fs_detect_list));
   // build return information in the single memory block
   for (ii=0, idx=0; ii<lst.Count(); ii++)
      if (lst[ii].length()) {
         spstr  val = lst.Value(ii),
                key = lst.Name(ii).trim();
         spstr bstr = val.word(2,",");

         res[idx].fsname  = nptr;
         res[idx].offset  = val.word_Dword(1,",");
         res[idx].size    = bstr.words();
         res[idx].cmpdata = dptr;
         // fa name
         memcpy(nptr, key(), key.length()+1);
         nptr += key.length()+1;
         // binary data to cmp
         for (u32t ll=0; ll<res[idx].size; ll++)
            *dptr++ = strtoul(bstr() + bstr.wordpos(ll+1), 0, 16);
         idx++;
      }
   return res;
}

void ecmd_loadfslib(const char *fsname) {
   spstr mdname = ecmd_readstr("fslist", fsname);

   if (mdname.trim().length()) {
      spstr mdfname = ecmd_readstr("MODULES", mdname());

      if (mod_query(mdname(),MODQ_NOINCR)==0)
         if (load_module(mdfname, 0))
            log_printf("Module \"%s\" loaded to handle %s\n", mdname(), fsname);
   }
}
