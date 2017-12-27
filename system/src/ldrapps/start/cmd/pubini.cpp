//
// QSINIT "start" module
// ini files access shared class
//
#include "qcl/qsinif.h"
#include "syslocal.h"
#include "sp_ini.h"
#include "qs_rt.h"
#include "time.h"

#define INIF_SIGN  MAKEID4('I','N','I','F')

#define instance_ret(inst,err)              \
   ifldata *inst = (ifldata*)data;          \
   if (inst->sign!=INIF_SIGN) return err;

#define instance_void(inst)                 \
   ifldata *inst = (ifldata*)data;          \
   if (inst->sign!=INIF_SIGN) return;

struct ifldata {
   u32t         sign;
   TINIFile     *ifl;
   io_handle     ifh;
   u32t        flags;
   char        fpath[QS_MAXPATH+1];
};

extern str_list* get_keylist(TINIFile  *ini, const char *Section, str_list**values);

static qserr _exicc inif_flush(EXI_DATA, int force) {
   instance_ret(fd, E_SYS_INVOBJECT);
   if (!fd->ifl) return E_SYS_NONINITOBJ;

   qserr res = 0;

   if (fd->ifh) {
      spstr dtext = fd->ifl->GetFileText().GetTextToStr();

      if (io_seek(fd->ifh,0,IO_SEEK_SET)==FFFF64) res = E_DSK_ERRWRITE; else
      if (io_write(fd->ifh, dtext(), dtext.length())!=dtext.length())
         res = io_lasterror(fd->ifh); 
            else res = io_setsize(fd->ifh, dtext.length());
   } else
   if ((fd->flags&QSINI_READONLY)) res = E_SYS_ACCESS; else
   if (fd->fpath[0]==0) res = E_SYS_INVNAME; else
      res = fd->ifl->GetFileText().SaveToFile(fd->fpath)?0:E_SYS_ACCESS;
   return res;
}

static qserr _exicc inif_close(EXI_DATA) {
   instance_ret(fd, E_SYS_INVOBJECT);
   if (!fd->ifl) return E_SYS_NONINITOBJ;

   qserr res = fd->flags&QSINI_READONLY?0:inif_flush(data,0,1);
   if (fd->ifh) {
      io_close(fd->ifh);
      fd->ifh = 0;
   }
   delete fd->ifl;
   fd->flags    = 0;
   fd->fpath[0] = 0;
   fd->ifl      = 0;
   return res;
}

static qserr _exicc inif_open(EXI_DATA, const char *fname, u32t mode) {
   instance_ret(fd, E_SYS_INVOBJECT);
   if (!fname) return E_SYS_ZEROPTR;
   // unknown flags?
   if ((mode&~(QSINI_READONLY|QSINI_EXCLUSIVE|QSINI_EXISTING)))
      return E_SYS_INVPARM;
   if ((mode&(QSINI_READONLY|QSINI_EXCLUSIVE))==(QSINI_READONLY|QSINI_EXCLUSIVE))
      return E_SYS_INCOMPPARAMS;
   qserr  res = 0;
   char  *src = 0;
   // close it first
   if (fd->ifl) { res = inif_close(data,0); fd->ifl = 0; }

   while (!res) {
      int     is_ro = mode&QSINI_READONLY,
            is_pres = mode&QSINI_EXISTING,
            is_lock = mode&QSINI_EXCLUSIVE;
      u32t openmode = is_ro || is_pres? IOFM_OPEN_EXISTING : IOFM_OPEN_ALWAYS,
             action, maxavail;
      u64t      fsz;
      openmode |= IOFM_READ|IOFM_SHARE_READ;
      if (is_lock) openmode |= IOFM_WRITE;
      /* ignore fullpath & open errors if file presence was not forced and
         r/o bit is on. Returns empty valid ini in this case */
      res = io_fullpath(fd->fpath, fname, sizeof(fd->fpath));
      if (res)
         if (is_ro && !is_pres) res = E_SYS_NOFILE; else break;

      if (!res) res = io_open(fd->fpath, openmode, &fd->ifh, &action);
      if (res) {
         fd->ifh = 0;
         // make empty one
         if (is_ro && !is_pres && res==E_SYS_NOFILE) {
            fd->ifl   = new TINIFile();
            fd->flags = QSINI_READONLY;
            res = 0;
            break;
         } else
            break;
      }
      res = io_size(fd->ifh, &fsz);
      if (res) break;
      // file wants a fourth of maximum available block size? then drop it!
      hlp_memavail(&maxavail,0);
      if (fsz>>2 >= maxavail) { res = E_SYS_NOMEM; break; }

      src = (char*)malloc(fsz);
      if (!src) { res = E_SYS_NOMEM; break; }
      if (io_read(fd->ifh, src, fsz)<fsz) { 
         res = io_lasterror(fd->ifh);
         if (!res) res = E_DSK_ERRREAD;
         break; 
      }
      TStrings slst;
      slst.SetText(src,fsz);

      fd->flags = mode;
      fd->ifl   = new TINIFile(&slst);
      if (!is_lock) { io_close(fd->ifh); fd->ifh = 0; }
      break;
   }
   if (src) free(src);
   if (res) {
      if (fd->ifh) { io_close(fd->ifh); fd->ifh = 0; }
      fd->flags = 0;
   }
   return res;
}

static void _exicc inif_create(EXI_DATA, str_list *text) {
   instance_void(fd);
   if (fd->ifl) inif_close(data,0);
   if (text) {
      TStrings srt;
      str_getstrs(text, srt);
      fd->ifl = new TINIFile(&srt);
   } else
      fd->ifl = new TINIFile();
   fd->ifh      = 0;
   fd->flags    = QSINI_READONLY;
   fd->fpath[0] = 0;
}

static qserr _exicc inif_fstat(EXI_DATA, io_direntry_info *rc) {
   instance_ret(fd, E_SYS_INVOBJECT);
   if (!fd->ifl) return E_SYS_NONINITOBJ;
   if (!rc) return E_SYS_ZEROPTR;
   qserr res;
   // file is ours!
   if (fd->ifh) res = io_info(fd->ifh, rc); else 
   // no path at all
   if (fd->fpath[0]==0) res = E_SYS_INVNAME; else {
   // query via common stat
      io_handle_info ti;
      res = io_pathinfo(fd->fpath, &ti);
      if (!res) {
         rc->attrs  = ti.attrs;
         rc->fileno = ti.fileno;
         rc->size   = ti.size;
         rc->ctime  = ti.ctime;
         rc->atime  = ti.atime;
         rc->wtime  = ti.wtime;
         rc->dir    = 0;
         strcpy(rc->name,fd->fpath);
      }
   }
   return res;
}

/// save file copy
static qserr _exicc inif_savecopy(EXI_DATA, const char *fname) {
   instance_ret(fd, E_SYS_INVOBJECT);
   if (!fd->ifl) return E_SYS_NONINITOBJ;
   if (!fd->ifl->SaveCopy(fname)) return E_SYS_ACCESS;
   return 0;
}

static void _exicc inif_setstr(EXI_DATA, const char *sec, const char *key, const char *value) {
   instance_void(fd);
   if (!fd->ifl) return;
   fd->ifl->WriteString(sec, key, value);
}

static void _exicc inif_setint(EXI_DATA, const char *sec, const char *key, s32t value) {
   instance_void(fd);
   if (!fd->ifl) return;
   fd->ifl->WriteInteger(sec, key, value);
}

static void _exicc inif_setuint(EXI_DATA, const char *sec, const char *key, u32t value) {
   instance_void(fd);
   if (!fd->ifl) return;
   char buf[16];
   _utoa(value, buf, 10);
   fd->ifl->WriteString(sec, key, buf);
}

static void _exicc inif_seti64(EXI_DATA, const char *sec, const char *key, s64t value) {
   instance_void(fd);
   if (!fd->ifl) return;
   char buf[32];
   _itoa64(value, buf, 10);
   fd->ifl->WriteString(sec, key, buf);
}

static void _exicc inif_setui64(EXI_DATA, const char *sec, const char *key, u64t value) {
   instance_void(fd);
   if (!fd->ifl) return;
   char buf[32];
   _utoa64(value, buf, 10);
   fd->ifl->WriteString(sec, key, buf);
}

static char*_exicc inif_getstr(EXI_DATA, const char *sec, const char *key, const char *def) {
   // strdup will return 0 if def is 0, so we`re ok here
   instance_ret(fd, strdup(def));
   if (!fd->ifl) return strdup(def);

   spstr value = fd->ifl->ReadStr(sec,key,def).trim();
   return !value&&!def?0:strdup(value());
}

static s32t _exicc inif_getint(EXI_DATA, const char *sec, const char *key, s32t def) {
   instance_ret(fd, def);
   if (!fd->ifl) return def;
   return fd->ifl->ReadInt(sec,key,def);
}

static u32t _exicc inif_getuint(EXI_DATA, const char *sec, const char *key, u32t def) {
   instance_ret(fd, def);
   if (!fd->ifl) return def;
   if (!fd->ifl->ValueExists(sec, key)) return def;
   return fd->ifl->ReadStr(sec,key,"0").Dword();
}

static s64t _exicc inif_geti64(EXI_DATA, const char *sec, const char *key, s64t def) {
   instance_ret(fd, def);
   if (!fd->ifl) return def;
   if (!fd->ifl->ValueExists(sec, key)) return def;
   return fd->ifl->ReadStr(sec,key,"0").Int64();
}

static u64t _exicc inif_getui64(EXI_DATA, const char *sec, const char *key, u64t def) {
   instance_ret(fd, def);
   if (!fd->ifl) return def;
   if (!fd->ifl->ValueExists(sec, key)) return def;
   return fd->ifl->ReadStr(sec,key,"0").Dword64();
}

/** return uppercased or original section names list.
    @param   org       Return original names (1) or uppercased (0).
    @return string list, must be free(d). */
static str_list* _exicc inif_sections(EXI_DATA, int org) {
   instance_ret(fd, 0);
   if (!fd->ifl) return 0;
   TStrings lst;
   if (org) fd->ifl->ReadSectionOrgNames(lst); else fd->ifl->ReadSections(lst);
   return str_getlist_local(lst.Str);
}

static void read_section(TINIFile *ini, const char *sec, u32t flags, TStrings &out) {
   ini->ReadSection(sec, out);
   if (flags) {
      l ii;
      if (flags&GETSEC_NOEMPTY) out.TrimEmptyLines();
      ii = out.Count();
      while (ii-->0) {
         out[ii].trim();
         if (out[ii].length())
            if ((flags&GETSEC_NOEMPTY) && out[ii][0]==';' ||
               (flags&GETSEC_NOEKEY) && out[ii][0]=='=' ||
                  (flags&GETSEC_NOEVALUE) && out[ii].cpos('=')==out[ii].length()-1)
                     out.Delete(ii);
      }
   }
}

static int _exicc inif_secexist(EXI_DATA, const char *sec, u32t flags) {
   instance_ret(fd, 0);
   if (!fd->ifl) return 0;
   int rc = fd->ifl->SectionExists(sec)?1:0;
   if (flags && rc) {
      TStrings lst;
      read_section(fd->ifl, sec, flags, lst);
      return lst.Count()?1:0;
   }
   return rc;
}

static str_list* _exicc inif_getsec(EXI_DATA, const char *sec, u32t flags) {
   instance_ret(fd, 0);
   if (!fd->ifl) return 0;
   TStrings lst;
   read_section(fd->ifl, sec, flags, lst);

   str_list *rc = str_getlist_local(lst.Str);
   // log_printlist("!!!", rc);
   return rc;
}

static int _exicc inif_exist(EXI_DATA, const char *sec, const char *key) {
   instance_ret(fd, 0);
   if (!fd->ifl) return 0;
   return fd->ifl->ValueExists(sec,key)?1:0;
}

/** return list of keys in specified section.
    @param       sec     Section name
    @param [out] values  List of values for returning keys (can be 0)
    @return keys list or 0 */
static str_list* _exicc inif_keylist(EXI_DATA, const char *sec, str_list**values) {
   instance_ret(fd, 0);
   if (!fd->ifl) return 0;
   return get_keylist(fd->ifl, sec, values);
}

/** return entire file text.
    @return string list, must be free(d). */
static str_list* _exicc inif_text(EXI_DATA) {
   instance_ret(fd, 0);
   if (!fd->ifl) return 0;
   // we can`t send original list - str_getlist() will trim it (source file!)
   TStringVector tmpl(fd->ifl->GetFileText().Str);
   return str_getlist_local(tmpl);
}

/** write complete section text.
    @param   sec       Section name
    @param   text      Section text (without header) */
static void _exicc inif_secadd(EXI_DATA, const char *sec, str_list *text) {
   instance_void(fd);
   if (!fd->ifl) return;
   TStrings srt;
   str_getstrs(text, srt);
   fd->ifl->WriteSection(sec, srt);
}

static void _exicc inif_secerase(EXI_DATA, const char *sec) {
   instance_void(fd);
   if (!fd->ifl) return;
   fd->ifl->EraseSection(sec);
}

static void _exicc inif_erase(EXI_DATA, const char *sec, const char *key) {
   instance_void(fd);
   if (!fd->ifl) return;
   fd->ifl->DeleteKey(sec, key);
}

static void _std inif_initialize(void *instance, void *data) {
   ifldata *ifd = (ifldata*)data;
   ifd->sign    = INIF_SIGN;
   // other fields zeroed by caller
}

static void _std inif_finalize(void *instance, void *data) {
   instance_void(ifd);
   if (ifd->ifl) inif_close(data,0);
   ifd->sign = 0;
   ifd->ifl  = 0;
   ifd->ifh  = 0;
}

static void *method_list[] = { inif_open, inif_create, inif_close, inif_fstat,
   inif_savecopy, inif_flush, inif_setstr, inif_setint, inif_setuint,
   inif_seti64, inif_setui64, inif_getstr, inif_getint, inif_getuint,
   inif_geti64, inif_getui64, inif_sections, inif_secexist, inif_exist,
   inif_getsec, inif_keylist, inif_text, inif_secadd, inif_secerase,
   inif_erase };

void exi_register_inifile(void) {
   exi_register("qs_inifile", method_list, sizeof(method_list)/sizeof(void*),
      sizeof(ifldata), 0, inif_initialize, inif_finalize, 0);
}
