#define MODULE_INTERNAL
#include "qsmod.h"
#include "qschain.h"
#include "qsmodext.h"
#include "internal.h"
#include "stdlib.h"
#include "qsutil.h"
#include "wchar.h"

#define MAX_TRACE_MOD (32)         ///< maximum # of traced modules
#define TRACE_OWNER   (0x42435254)

#define TRACEF_ON     0x0001
#define TRACEF_CLASS  0x0002       ///< shared class

static int _std hookmain(mod_chaininfo *info);

class TraceFile {
   u8t*    data;
   u32t     pos, 
           size;
public:
   TraceFile(const spstr &fname) {
      spstr pathname;
      data=0; pos=0; size=0;

      _searchenv(fname(), "TRACEPATH", pathname.LockPtr(_MAX_PATH+1));
      pathname.UnlockPtr();
      if (pathname.length()) data = (u8t*)freadfull(pathname(), &size);
   }
   int IsValid() { return data?1:0; }
   int IsEOF() { return pos>=size; }

   ~TraceFile() {
      if (data) { hlp_memfree(data); data=0; }
      pos=size=0;
   }
   int GetStr(spstr &rc) {
      rc.clear();
      if (!data || pos>=size) return 0;
      u32t len = data[pos++];
      if (!len) return 1;
      if (pos+len>size) return 0;
      char *cptr = rc.LockPtr(len+1);
      memcpy(cptr, data+pos, len);
      rc.UnlockPtr(len);
      pos+=len;
      return 1;
   }
   int GetU16(u32t &rc) {
      if (!data || pos+2>size) return 0;
      rc = *(u16t*)(data+pos);
      pos+=2;
      return 1;
   }
};

struct TraceInfo {
   TStrings   groups;
   TStrings     name;
   TStrings      fmt;
   TList     classid;
};

/** List of trace file contents.
    Contain groups/names/format for every loaded TRC file. And group
    selection flag in groups.Objects() */
static TPtrStrings      *lti = 0;  

static volatile int listlock = 0;  ///< lists in changing state flag

static u32t   activelist[MAX_TRACE_MOD];  ///< list of loaded module*
static u32t     *ordlist[MAX_TRACE_MOD];  ///< list of ordinal lists
static char    **fmtlist[MAX_TRACE_MOD];  ///< list of format string lists
static char   **namelist[MAX_TRACE_MOD];  ///< list of function names lists
static u32t     ordinals[MAX_TRACE_MOD];  ///< number of ordinals
static u32t      modules = 0;             ///< number of modules

/// load trace file for module
static TraceInfo *trace_load(const char *module, int quiet) {
   spstr modname(module);
   modname+=".trc";
   modname.upper();

   TraceFile trf(modname);
   if (!trf.IsValid()) {
      if (!quiet) printf("Unable to find %s\n",modname());
      return 0;
   }
   spstr     str;
   int       err = 1;
   TraceInfo *ti = 0;
   do {
      if (!trf.GetStr(str) || str!="QSTRACE2") break;
      if (!trf.GetStr(str) || stricmp(str(),module)) break;
      u32t groups, ii;
      if (!trf.GetU16(groups)) break;
      if (!groups) return 0;

      ti = new TraceInfo;
      // read group names
      for (ii=0; ii<groups; ii++) {
         if (!trf.GetStr(str) || !str) { err+=100; break; }
         ti->groups.Add(str);
         ti->classid.Add(0);
      }
      if (err>1) break;
      // read groups
      for (ii=0; ii<groups; ii++) {
         u32t fcnt = 0, kk, scf;
         // read data
         if (!trf.GetU16(fcnt)) { err+=200; break; }
         // set flag
         scf   = fcnt&0x8000;
         ti->groups.Objects(ii) = scf ? TRACEF_CLASS : 0;
         fcnt &= 0x7FFF;

         for (kk=0; kk<fcnt; kk++) {
            /* function index in class stored as ordinal for
               classes and ordinal itself for functions */
            u32t idx = kk;
            if (!scf && !trf.GetU16(idx)) { err+=300; break; }
            if (!trf.GetStr(str) || !str) { err+=400; break; }
            ti->name.AddObject(str,idx);  // name<->index pair
            if (!trf.GetStr(str) || !str) { err+=500; break; }
            ti->fmt.AddObject(str,ii);    // name<->group pair
         }
         if (err>1) break;
      }
      if (err>1) break;
      err = 0;
   } while (0);
   if (err) {
      log_it(2, "trace file for '%s' -> err %i (%s)\n", module, err,
         ti && ti->name.Count()>0 ? ti->name[ti->name.Max()](): "null" );
      if (!quiet) printf("File %s format is invalid!\n",modname());
      if (ti) delete ti;
      return 0;
   } else
      log_it(2, "trace file for '%s' loaded, %d groups, %d funcs\n", module,
         ti->groups.Count(), ti->name.Count());

   return ti;
}

/// find or load trace file entry for module
static TraceInfo *trace_find(const char *module, int quiet, int add=0) {
   if (!module) return 0;
   if (!lti) lti = new TPtrStrings;
   TraceInfo *mti = 0;
   int idx = lti->IndexOfICase(module);

   if (idx<0) {
      if (add) mti = trace_load(module, quiet);
      if (!mti) return 0; else lti->AddObject(module, mti);
   } else
      mti = (TraceInfo*)lti->Objects(idx);
   return mti;
}

static char** trace_getstrlist(u32t pool, TStrings &lst) {
   if (!lst.Count()) return 0;
   spstr flst(lst.GetTextToStr(" "));
   char **out = (char**) memAlloc(TRACE_OWNER,pool,sizeof(char*)*lst.Count()),
        *data = (char*) memAlloc(TRACE_OWNER,pool,flst.length()+1);
   for (u32t ii=0; ii<lst.Count(); ii++) {
      out[ii] = data;
      strcpy(data, lst[ii]());
      data += lst[ii].length()+1;
   }
   return out;
}

static u32t trace_makelists(TraceInfo *mti, u32t mh, u32t **ords, 
   char ***fmts, char ***names)
{
   TStrings namelist(mti->name);
   u32t  cnt = mti->name.Count(), ii;

   if (!cnt) *ords = 0; else {
      *ords = (u32t*) memAlloc(TRACE_OWNER, mh, sizeof(u32t)*cnt);
      memcpy(*ords, mti->name.LList.Value(), sizeof(u32t)*cnt);
      // zero ordinals present?
      if (memchrd(*ords, 0, cnt)) {
         for (ii=0; ii<cnt; ii++) {
            u32t grp = mti->fmt.Objects(ii);
            if ((mti->groups.Objects(grp)&TRACEF_CLASS)) {
               if (!mti->classid[grp])
                  mti->classid[grp] = exi_queryid(mti->groups[grp]());
               pvoid *thlist = exi_thunklist(mti->classid[grp]);
               /* because ordinals are always short (1..65535), we can put
                  both ordinals & addresses of thunks in the same array. This
                  is bad, but saves code & time ;) */
               if (thlist) (*ords)[ii] = (u32t)thlist[(*ords)[ii]]; else
                  (*ords)[ii] = 0;
               // make printable name in form "class->method"
               namelist[ii].insert("->",0);
               namelist[ii].insert(mti->groups[grp],0);
            }
         }
      }
   }
   *fmts  = trace_getstrlist(mh,mti->fmt);
   *names = trace_getstrlist(mh,namelist);
   return cnt;
}

/** add module handle to handle list.
    @param module   module handle
    @param mti      trace file info for this module name
    @param quiet    be quiet on errors
    @return index in static handle arrays or -1 on error */
static int trace_mhadd(u32t module, TraceInfo *mti, int quiet) {
   u32t    *ords, *pos;
   char   **fmts, **names;

   if (modules==MAX_TRACE_MOD) {
      if (!quiet) printf("Too many active modules!\n");
      return -1;
   }

   listlock++;
   if (modules==0) {
      memset(&activelist, 0, sizeof(activelist));
   }
   pos = memchrd(activelist, module, MAX_TRACE_MOD);
   if (!pos) {
      pos = memchrd(activelist, 0, MAX_TRACE_MOD);
      if (!pos) { listlock--; return -1; } // must occur only on broken structs
      modules++;
   }
   u32t idx = pos-activelist,
        cnt = trace_makelists(mti, module, &ords, &fmts, &names);
   activelist[idx] = module;
   fmtlist   [idx] = fmts;
   namelist  [idx] = names;
   ordlist   [idx] = ords;
   ordinals  [idx] = cnt;

   listlock--;
   return idx;
}

/** find module pos in handle list.
    @param module   module handle
    @return index in static handle arrays or -1 if no entry */
static int trace_mhfind(u32t module) {
   if (modules==0 || !module) return -1;
   listlock++;
   u32t *pos = memchrd(activelist, module, MAX_TRACE_MOD);
   listlock--;
   if (!pos) return -1;
   return pos-activelist;
}

/** common hook install/remove processing.
    @param  mti     trace info for this module
    @param  idx     index of handle in handle arrays
    @param  list    list of group indexes to add or remove
    @param  add     action flag (add or remove handling) */
static void trace_mhcommon(TraceInfo *mti, int idx, TList &list, int add) {
   int ii;
#if 0
   log_it(2,"trace_mhcommon(%s,%d)\n", ((module*)activelist[idx])->name, add);
#endif
   for (ii=0; ii<list.Count(); ii++) {
      int   fidx = mti->fmt.IndexOfObject(list[ii],0),
         isclass = mti->groups.Objects(list[ii])&TRACEF_CLASS;
      u32t  clid = 0;
      pvoid *cth = 0;
      if (isclass) {
         clid = mti->classid[list[ii]];
         if (clid) cth = exi_thunklist(clid);
         if (!cth) {
            log_it(2,"unable to trace class \"%s\"\n", mti->groups[list[ii]]());
            continue;
         }
      }
      while (fidx>=0) {
         u32 ord = mti->name.Objects(fidx);
         if (add) {
            if (isclass)
               mod_fnchain(activelist[idx],cth[ord],APICN_ONENTRY|APICN_FIRSTPOS,hookmain);
            else
               mod_apichain(activelist[idx],ord,APICN_ONENTRY|APICN_FIRSTPOS,hookmain);
            // add exit hook on non-void result or out parameter
            if (mti->fmt[fidx][0]!='v' || mti->fmt[fidx].cpos('&')>=0 ||
               mti->fmt[fidx].cpos('!')>=0)
               if (isclass)
                  mod_fnchain(activelist[idx],cth[ord],APICN_ONEXIT,hookmain);
               else
                  mod_apichain(activelist[idx],ord,APICN_ONEXIT,hookmain);
         } else {
            if (isclass) {
               mod_fnunchain (activelist[idx],cth[ord],APICN_ONENTRY,hookmain);
               mod_fnunchain (activelist[idx],cth[ord],APICN_ONEXIT,hookmain);
            } else {
               mod_apiunchain(activelist[idx],ord,APICN_ONENTRY,hookmain);
               mod_apiunchain(activelist[idx],ord,APICN_ONEXIT,hookmain);
            }
         }
         fidx = mti->fmt.IndexOfObject(list[ii],fidx+1);
      }
   }
}

int trace_common(const char *module, const char *group, int quiet, int on) {
   TraceInfo *mti = trace_find(module,quiet,on); // load only on "trace on"
   if (!mti) return 0;
   TList difflist;
   int         ii;
   // this must this bit only!
   on = on?TRACEF_ON:0;
   if (!group||!*group) {
      for (ii=0; ii<mti->groups.Count(); ii++) 
         if ((mti->groups.Objects(ii)^on)) {
            if (on) mti->groups.Objects(ii)|=TRACEF_ON; else
               mti->groups.Objects(ii)&=~TRACEF_ON;
            difflist.Add(ii);
         }
   } else {
      ii = mti->groups.IndexOf(group);
      if (ii>=0 && (mti->groups.Objects(ii)^on)) {
         if (on) mti->groups.Objects(ii)|=TRACEF_ON; else
            mti->groups.Objects(ii)&=~TRACEF_ON;
         difflist.Add(ii);
      }
   }
   if (difflist.Count()==0) return 1;

   if (on) {
      // process trace for all loaded modules with the same name
      for (ii=0; ii<256; ii++) {
         u32t mh = mod_query(module, ii<<MODQ_POSSHIFT|MODQ_LOADED|MODQ_NOINCR);
         if (!mh) break;
         int idx = trace_mhfind(mh);
         if (idx<0) idx = trace_mhadd(mh, mti, quiet);
         trace_mhcommon(mti, idx, difflist, 1);
      }
   } else {
      int on_cnt = 0;
      for (ii=0; ii<mti->groups.Count(); ii++) 
         if ((mti->groups.Objects(ii)&TRACEF_ON)) on_cnt++;
      int full = difflist.Count() >= on_cnt, idx;

      // process un-trace for all loaded modules with the same name
      for (ii=0; ii<256; ii++) {
         u32t mh = mod_query(module, ii<<MODQ_POSSHIFT|MODQ_LOADED|MODQ_NOINCR);
         if (!mh) break;
         idx = trace_mhfind(mh);
         if (idx>=0) {
            trace_mhcommon(mti, idx, difflist, 0);
            // clear module list entry - if _all_ hooks was removed
            if (full) {
               activelist[idx] = 0;
               memFreePool(TRACE_OWNER, mh);
            }
         }
      }
   }
   log_it(2,"trace_common done, %d group(s) changed\n", difflist.Count());
   return 1;
}

int trace_onoff(const char *module, const char *group, int quiet, int on) {
   log_it(2,"trace_onoff(%s,%s,%i,%i)\n",module,group,quiet,on);

   if (!module || strcmp(module,"*")==0) {
      if (!lti) return 1;

      for (int ii=0; ii<lti->Count(); ii++) {
         char *modname = strdup((*lti)[ii]());
         log_it(2,"trace_onoff(%s)\n",modname);
         trace_common(modname, group, quiet, on);
         free(modname);
      }
      return 1;
   }
   return trace_common(module, group, quiet, on);
}

void trace_list(const char *module, const char *group, int pause) {
   if (!module) {
      if (!lti || !lti->Count()) {
         printf("There is no active trace files\n");
         return;
      }
      cmd_printseq(0,1,0);
      for (int ii=0; ii<lti->Count(); ii++) {
         int cnt=0, jj;
         TraceInfo *mti = (TraceInfo*)lti->Objects(ii);
         for (jj=0; jj<mti->groups.Count(); jj++) 
            cnt += mti->groups.Objects(jj)&TRACEF_ON?1:0;

         cmd_printseq("%-16s  %d groups (%d active)",pause?0:-1,0,(*lti)[ii](),
            mti->groups.Count(), cnt);
      }
   } else {
      TraceInfo *mti = trace_find(module,1,1);
      if (!mti) {
         printf("There is no trace info for module '%s'\n", module);
         return;
      }
      if (!group) {
         cmd_printseq(0,1,0);
         for (int ii=0; ii<mti->groups.Count(); ii++)
            cmd_printseq("%c  %-30s : %s",pause?0:-1,0,
               mti->groups.Objects(ii)&TRACEF_CLASS?'c':'g',
                  mti->groups[ii](), mti->groups.Objects(ii)&TRACEF_ON?"ON":"OFF");
      } else {
         int  idx = mti->groups.IndexOf(group), isclass;
         if (idx<0) {
            printf("There is no group '%s' in module '%s'\n", group, module);
            return;
         }
         isclass = mti->groups.Objects(idx)&TRACEF_CLASS;
         cmd_printseq(0,1,0);
         for (int ii=0; ii<mti->name.Count(); ii++)
            if (mti->fmt.Objects(ii)==idx)
               if (isclass)
                  cmd_printseq("method %s (%s)",pause?0:-1,0, mti->name[ii](),
                     mti->fmt[ii]());
               else
                  cmd_printseq("ordinal %4d, name %s (%s)",pause?0:-1,0,
                     mti->name.Objects(ii), mti->name[ii](), mti->fmt[ii]());
      }
   }
}

/// start module callback (need to enum all modules here!)
extern "C"
void trace_start_hook(module *mh) {
   TraceInfo *mti = trace_find(mh->name,1);
   if (!mti) return;
   TList difflist;
   int ii;
   for (ii=0; ii<mti->groups.Count(); ii++) 
      if ((mti->groups.Objects(ii)&TRACEF_ON)) difflist.Add(ii);
   if (difflist.Count()) {
      log_it(2,"trace_start_hook(%s) -> %d groups to activate\n", mh->name,
         difflist.Count());
      ii = trace_mhfind((u32t)mh);
      if (ii<0) ii = trace_mhadd((u32t)mh, mti, 1);
      trace_mhcommon(mti, ii, difflist, 1);
   }
}

/// free module callback
extern "C"
void trace_unload_hook(module *mh) {
   TraceInfo *mti = trace_find(mh->name,1);
   if (!mti) return;
   /* mod_apiunchain() was called in lxmisc.c, classes trace thunks must
      be removed by unregister, so only clearing own structs here */
   int idx = trace_mhfind((u32t)mh);
   if (idx>=0) {
      activelist[idx] = 0;
      memFreePool(TRACE_OWNER, (u32t)mh);
   }
}

static u32t printarg(int idx, int fidx, char *buf, mod_chaininfo *info, int in) {
   char *fmt = fmtlist[idx][fidx],
     *fnname = namelist[idx][fidx], errn=0;
   int   arg = 0, pos=0,
        outs = in?strccnt(fmt,'&')+strccnt(fmt,'!'):0, outpcnt = 0;
   u32t *esp = (u32t*)(info->mc_regs->pa_esp + 4),
               // buffer for/with otuput values
      *ehbuf = in && outs? (u32t*)memAlloc(TRACE_OWNER, activelist[idx], 
               sizeof(u32t)*outs) : (u32t*)info->mc_userdata;

   if (*fmt=='e') { errn=1; pos++; }
   strcpy(buf, in?"In : ":"Out: ");
   strcpy(buf+5, fnname);
   buf+=strlen(buf);
   // non-void result
   if (!in && fmt[pos]!='v') *buf++='=';

   do { 
      char ch = fmt[pos++], out=0, ptr=0, hex=0, inout=0;
      // prefixes
      while (ch=='&' || ch=='*' || ch=='@' || ch=='!') {
         switch (ch) {
            case '&': out=1; inout=0; break;
            case '!': out=1; inout=1; break;
            case '*': ptr=1; break;
            case '@': hex=1; break;
         }
         ch=fmt[pos++]; 
      }
      if (out&&!ptr&&ch!='s'&&ch!='S') out=0;
      if (in&&out&&!inout) ch='p';

      u64t llvalue = 0;
      u32t   value = 0;
      // stored out parameters in the order of presence in call
      u32t *pvalue = !in&&out ? ehbuf+outpcnt++ : 0;

      if (ch=='q') {
         // must be a ptr for out parameter
         if (pvalue) value = *(u32t*)pvalue; else
         if (in) {
            if (ptr) value = *esp++; else
            if (arg) { llvalue = *(u64t*)esp; esp+=2; }
         } else
         if (!arg)
            llvalue = (u64t)info->mc_regs->pa_eax | (u64t)info->mc_regs->pa_edx<<32;
      } else {
         if (pvalue) value = *(u32t*)pvalue; else 
         if (in) value = arg?*esp++:0; else
         if (!arg) value = info->mc_regs->pa_eax;
      }
      if (in&&out) ehbuf[outpcnt++] = value;
      // pointer to value. print (null) if it NUL
      if (ptr&&arg&&(in&&(!out||inout)||pvalue))
         if (!value) ch='s'; else 
            if (ch=='q') llvalue=*(u64t*)value; else value=*(u32t*)value;

      *buf = 0;

      switch (ch) {
         case 'S':
         case 's':
            if (Xor(in,!arg) || pvalue) {
               if (!value) strcpy(buf,"(null)"); else {
                  *buf++='\"';
                  if (ch=='s') {
                     strncpy(buf,(char*)value,32);
                     buf[31]=0;
                  } else {
                     char fmt[8];
                     u32t len = wcslen((wchar_t*)value);
                     if (len>31) len = 31;
                     sprintf(fmt, "%%%uS", len);
                     sprintf(buf, fmt, (wchar_t*)value);
                  }
                  strcat(buf,"\"");
               }
            }
            break;
         case 'b':
            if (Xor(in,!arg) || pvalue) 
               if (hex) sprintf(buf,"0x%02X",value&0xFF); else 
                  sprintf(buf,"%u",value&0xFF);
            break;
         case 'C':
            if (Xor(in,!arg) || pvalue) 
               if (hex) sprintf(buf,"0x%04X",value&0xFF); else 
                  sprintf(buf,"%C",value);
            break;
         case 'c':
            if (Xor(in,!arg) || pvalue) 
               if (hex) sprintf(buf,"0x%02X",value&0xFF); else 
                  sprintf(buf,"%c",value);
            break;
         case 'i':
            if (Xor(in,!arg) || pvalue) 
               if (hex) sprintf(buf,"0x%X",value); else 
                  sprintf(buf,"%i",value);
            break;
         case 'u':
            if (Xor(in,!arg) || pvalue) 
               if (hex) sprintf(buf,"0x%X",value); else 
                  sprintf(buf,"%u",value);
            break;
         case 'h':
            if (Xor(in,!arg) || pvalue) 
               if (hex) sprintf(buf,"0x%hX",value); else 
                  sprintf(buf,"%hu",value);
            break;
         case 'q': if (Xor(in,!arg) || pvalue) {
               if (hex) sprintf(buf,"0x%LX",llvalue); else 
                  sprintf(buf,"%Lu",llvalue);
            }
            break;
         case 'p':
            if (Xor(in,!arg) || pvalue) sprintf(buf,"%08X",value);
            break;
         case 'v': break;
         case '.': 
            if (in) strcpy(buf,"...");
            break;
      }
      
      if (*buf) buf+=strlen(buf);
      if (arg) {
         *buf++ = !fmt[pos]?')':',';
      } else {
         *buf++ = ' ';
         *buf++ = '(';
      }
      arg++;
   } while (fmt[pos]);
   // buffer from entry hook
   if (!in && info->mc_userdata>4096) free((void*)info->mc_userdata);

   *buf++ = '\n';
   *buf   = 0;
   // buffer for exit hook or 0
   return (u32t)ehbuf;
}

static int _std hookmain(mod_chaininfo *info) {
   u32t mh = (u32t)info->mc_orddata->od_handle;
   // locked, mark for exit and skip
   if (listlock) { info->mc_userdata=1; return 1; }
   listlock++;

   // reset debug check for own alloc/free time
   u32t opt = memGetOptions();
   if ((opt&QSMEMMGR_PARANOIDALCHK)!=0)
      memSetOptions(opt&~QSMEMMGR_PARANOIDALCHK);

   // process output
   u32t *pos = memchrd(activelist, mh, MAX_TRACE_MOD);
   if (pos) {
      ordinal_data *od = info->mc_orddata;
      int          idx = pos-activelist;
      pos = memchrd(ordlist[idx], od->od_ordinal?od->od_ordinal:(u32t)od->od_thunk,
                    ordinals[idx]);
      if (pos) {
         int fidx = pos-ordlist[idx];
         // do not make any checks of length limit now :((
         static char buf[512];
         info->mc_userdata = printarg(idx, fidx, buf, info, !info->mc_userdata);
         // can`t use printf here
         log_push(3,buf);
         hlp_seroutstr(buf);
      }
   }
   if (!info->mc_userdata) info->mc_userdata=1;
   listlock--;

   if ((opt&QSMEMMGR_PARANOIDALCHK)!=0) {
      memCheckMgr();
      memSetOptions(opt);
   }

   return 1;
}
