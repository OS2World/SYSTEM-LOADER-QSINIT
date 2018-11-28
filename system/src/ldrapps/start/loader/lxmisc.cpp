//
// QSINIT "start" module
// secondary LE/LX support code
//
#define LOG_INTERNAL
#define STORAGE_INTERNAL
#include "qsbase.h"
#include "qslxfmt.h"
#include "malloc.h"
#include "qsconst.h"
#include "qsint.h"
#include "sysio.h"
#include "vioext.h"
#include "start_ord.h"
#include "syslocal.h"
#include "zip/unzip.h"
#include "qcl/bitmaps.h"
#include "classes.hpp"

static u8t              *M3Buffer = 0;
static qshandle         ldr_mutex = 0,
                        mfs_mutex = 0;
u8t                     mod_delay = 1;   ///< exe delayed unpack
static TStrings        *DelayList = 0;
static io_handle       mfs_handle = 0;
u32t                    mh_qsinit = 0;   ///< QSINIT module handle

extern "C" {
extern u32t          M3BufferSize;
extern u16t               IODelay;
extern char            aboutstr[];
extern char      aboutstr_local[];

#pragma aux aboutstr       "_*";
#pragma aux IODelay        "_*";

void         check_version(void);
void         setup_loader_mt(void);
void         setup_loader(void);
int          unpack_ldi  (void);
/// call trace on parocess start
void         trace_start_hook(module *mh);
/// call trace on module free
void         trace_unload_hook(module *mh);
/// call trace just after process start
void         trace_pid_start(void);

// decompression routines
u32t _std    DecompressM2(u32t DataLen, u8t* Src, u8t *Dst);
u32t _std    DecompressM3(u32t DataLen, u8t* Src, u8t *Dst, u8t *Buffer);
void _std    log_memtable(void*, void*, u8t*, u32t*, u32t, process_context **);
// cache routines
void _std    cache_ioctl (u8t vol, u32t action);
u32t _std    cache_read  (u32t disk, u64t pos, u32t ssize, void *buf);
u32t _std    cache_write (u32t disk, u64t pos, u32t ssize, void *buf);
u16t _std    io_mfs_open (const char *name, u32t *filesize);
u32t __cdecl io_mfs_read (u32t offset, void *buf, u32t readsize);
void __cdecl io_mfs_close(void);
}

#define checkexps(x) if (expsmax<=(x)) { \
   exp = mh->exps = (mod_export*)realloc(mh->exps,(expsmax*=2)*sizeof(mod_export)); \
   memset(exp+(expsmax>>1), 0, (expsmax>>1)*sizeof(mod_export)); \
}

/* build export table (full version, short in qsinit supports only 32bit and no
   forwards) */
int _std mod_buildexps(module *mh, lx_exe_t *eh) {
   lx_exp_b    *ee = (lx_exp_b*)((u8t*)eh+eh->e32_enttab);
   u32t        idx = 1, cnt=0, expsmax=32, thcnt = 0;
   mod_export *exp = mh->exps = (mod_export*)calloc(expsmax, sizeof(mod_export));

   while (ee->b32_cnt) {
      u32t e32=0, bsize=0;
      switch (ee->b32_type&~TYPEINFO) {
         case EMPTY   : idx+=ee->b32_cnt; bsize=2; break;
         case ENTRY32 :
            e32 = 2;
         case ENTRY16 :
            if (ee->b32_obj==0||ee->b32_obj>eh->e32_objcnt) // invalid object?
               return E_MOD_BADEXPORT; else
            {
               u8t *entry = (u8t*)ee + (bsize=sizeof(lx_exp_b));
               while (ee->b32_cnt--) {
                  mod_object *obj = mh->obj + (ee->b32_obj-1);
                  exp[cnt].ordinal = idx++;
                  if (!e32) {
                     exp[cnt].is16 = 1;
                     exp[cnt].sel  = obj->sel;
                     exp[cnt].address = *(u16t*)++entry;
                  } else {
                     exp[cnt].address = *(u32t*)++entry + (u32t)obj->address;
                     // save object - for thunking code below
                     if (obj->flags & OBJEXEC) {
                        exp[cnt].direct = ee->b32_obj;
                        thcnt++;
                     }
                  }
                  entry+=2+e32; bsize+=3+e32;
                  checkexps(++cnt);
               }
            }
            break;
         case GATE16  : // ring 2 does not supported
            return E_MOD_UNSUPPORTED;
         case ENTRYFWD: // forward entry
            {
               u8t *entry = (u8t*)ee + (bsize=sizeof(lx_exp_b));
               while (ee->b32_cnt--) {
                  // forwarding to the name table is not supported
                  if ((*entry++&1)==0) return E_MOD_UNSUPPORTED;
                  exp[cnt].ordinal = idx++;
                  exp[cnt].forward = *(u16t*)entry;
                  // invalid module ordinal?
                  if (!exp[cnt].forward||exp[cnt].forward>eh->e32_impmodcnt)
                     return E_MOD_BADEXPORT;
                  entry+=2;
                  exp[cnt].address = *(u32t*)entry;
                  entry+=4; bsize+=7;
                  checkexps(++cnt);
               }
            }
            break;
         default:
            return E_MOD_BADEXPORT;
      }
      ee=(lx_exp_b*)((u8t*)ee+bsize);
   }
   mh->exports = cnt;
   /* creating thunks.
      It will be aligned to 16 bytes in memory because memmgr return such
      blocks. Thunks created only for entries, which points to 32bit CODE
      objects.
      Trace code will check it in the same way:
         if (entry - module->thunks <= 16*index) ... // this is thunk.
   */
   if (thcnt) {
      u32t ii;
      u8t *thunks = mh->thunks = (u8t*)malloc(EXPORT_THUNK*thcnt);
      memset(thunks, 0xCC, EXPORT_THUNK*thcnt);

      for (ii=0; ii<cnt; ii++) {
         u32t addr = exp[ii].address;
         if (exp[ii].direct) {
            u8t* thc        = thunks;
            exp[ii].address = (u32t)thc;
            *thc++          = 0xE9;
            *(u32t*)thc     = addr - ((u32t)thc+4);
            thunks         += EXPORT_THUNK;
         }
         exp[ii].direct = addr;
      }
   }
   return 0;
}

u32t _std mod_searchload(const char *name, u32t flags, qserr *error) {
   char path[QS_MAXPATH+1],
       mname[E32MODNAME+5], *epos;
   strncpy(mname,name,E32MODNAME);
   mname[E32MODNAME] = 0;
   epos = strrchr(mname,'.');
   if (!epos||strlen(epos)>4) {
      epos = mname + strlen(mname);
      strcpy(epos,".DLL");
   } else
      epos = 0;
   strupr(mname);

   if (error) *error = 0;
   *path=0;
   _searchenv(mname,"LIBPATH",path);
   if (!*path) _searchenv(mname,"PATH",path);
   if (!*path) _searchenv(name,"LIBPATH",path);
   if (!*path) _searchenv(name,"PATH",path);
   if (!*path&&epos) {
      strcpy(epos,".EXE");
      _searchenv(mname,"PATH",path);
   }
   if (!*path) return 0;
   return mod_load(path, flags, error, 0);
}

/* unpack code works as IBM one and it search for zero at the end of iterated
   data, but who can guarantee his presence? it really can be missed, at least
   in ITERDATA pages.
   so we add it ;) we can save 4 bytes after source data because both mdt.c and
   bootos2.exe code allocate buffer with additional dword.
   it is an impossible case to reach the end of memory block by modifying
   those 4 bytes, but we still add it ;)

   note: this comment applied to mod_unpack2 code too. */

u32t _std mod_unpack1(u32t datalen, u8t* src, u8t *dst) {
   struct LX_Iter *ir = (struct LX_Iter*)src;
   u8t *page = dst;
   u32t save;
   if (datalen>=PAGESIZE) return 0;
   save = *(u32t*)(src+datalen);
   *(u32t*)(src+datalen) = 0;

   while (ir->LX_nIter) {
      switch (ir->LX_nBytes) {
         case 1:
            memset(dst, ir->LX_Iterdata, ir->LX_nIter);
            dst+=ir->LX_nIter;
            break;
         case 2:
            memsetw((u16t*)dst, *(u16t*)&ir->LX_Iterdata, ir->LX_nIter);
            dst+=ir->LX_nIter<<1;
            break;
         case 4:
            memsetd((u32t*)dst, *(u32t*)&ir->LX_Iterdata, ir->LX_nIter);
            dst+=ir->LX_nIter<<2;
            break;
         default: {
            u32t cnt;
            for (cnt=0; cnt<ir->LX_nIter; cnt++) {
               memcpy(dst,&ir->LX_Iterdata,ir->LX_nBytes);
               dst+=ir->LX_nBytes;
            }
            break;
         }
      }
      ir = (struct LX_Iter*)((u8t*)ir+4+ir->LX_nBytes);
   }
   *(u32t*)(src+datalen) = save;
   return dst - page;
}

u32t _std mod_unpack2(u32t datalen, u8t* src, u8t *dst) {
   u32t save, rc;
   if (datalen>=PAGESIZE) return 0;
   save = *(u32t*)(src+datalen);
   *(u32t*)(src+datalen) = 0;
   rc = DecompressM2(datalen, src, dst);
   *(u32t*)(src+datalen) = save;
   return rc;
}

u32t _std mod_unpack3(u32t datalen, u8t* src, u8t *dst) {
   if (!M3Buffer)
      M3Buffer = (u8t*)hlp_memallocsig(M3BufferSize, "M3", QSMA_READONLY);
   return DecompressM3(datalen,src,dst,M3Buffer);
}

u32t _std mod_getpid(void) { return mod_context()->pid; }

u32t _std mod_appname(char *name, u32t parent) {
   process_context *pq = mod_context();
   if (!name||!pq||parent>1) return 0; else {
      module *src = parent?pq->parent:pq->self;
      if (!src) return 0;
      strncpy(name,src->name,E32MODNAME);
      name[E32MODNAME] = 0;
      strupr(name);
      return pq->pid;
   }
}

char *_std mod_getname(u32t mh, char *buffer) {
   module *md = (module*)mh;
   char  *res = 0;
   if (in_mtmode) mt_muxcapture(mod_secondary->ldr_mutex);
   if (md->sign==MOD_SIGN) {
      if (buffer) {
         strncpy(buffer, md->name, E32MODNAME);
         buffer[E32MODNAME] = 0;
         strupr(buffer);
         res = buffer;
      } else
      if (md->name_buf) res = md->name_buf; else {
         res = strdup(md->name);
         mem_modblockex(res, mh);
         strupr(res);
         md->name_buf = res;
      }
   }
   if (in_mtmode) mt_muxrelease(mod_secondary->ldr_mutex);
   return res;
}

static void release_ldi(void) {
   mt_swlock();
   void *mem = sto_data(STOKEY_ZIPDATA);
   sto_del(STOKEY_ZIPDATA);
   mt_swunlock();

   hlp_memfree(mem);
}

/// called just after MT mode start in background thread
void unzip_delaylist(void) {
   u32t md_count = 0;
   if (in_mtmode) mt_muxcapture(mod_secondary->ldr_mutex);
   if (DelayList) {
      md_count = DelayList->Count();
      if (md_count) {
         spstr cmd;
         cmd.sprintf("/o /q /key %s b:\\ ", STOKEY_ZIPDATA);
         cmd += DelayList->MergeCmdLine();
         cmd_shellcall(shl_unzip, cmd(), 0);
      }
      delete DelayList;
      DelayList = 0;
   }
   if (in_mtmode) mt_muxrelease(mod_secondary->ldr_mutex);
   release_ldi();
   if (md_count)
      log_it(2, "Delayed unpack (%d modules) done\n", md_count);
}

/* unzip one of delayed EXE/DLL from the saved QSINIT.LDI data.
   When all files will be done - LDI data released */
int _std unzip_ldi(void *mem, u32t size, const char *path) {
   char drv = toupper(path[0]);
   // we should receive a full path here, but it can be both 1: & B:
   if (drv!='B' && drv!='1' || path[1]!=':') return 0;

   spstr callstr, cpath(path+2);
   cpath.replacechar('/','\\');
   while (cpath[0]=='\\') cpath.del(0);

   callstr.sprintf("/o /q /key %s b:\\ %s", STOKEY_ZIPDATA);
   callstr+=cpath;
   // protect DelayList by loader mutex (any way, it needed for mod_load)
   if (in_mtmode) mt_muxcapture(mod_secondary->ldr_mutex);
   u32t  dm_cnt = DelayList?DelayList->Count():0;
   if (DelayList) {
      l idx = DelayList->IndexOfICase(cpath());
      if (idx>=0) {
         DelayList->Delete(idx);
         if (DelayList->Count()==0) delete DelayList;
      } else
         dm_cnt = 0;
   }
   if (in_mtmode) mt_muxrelease(mod_secondary->ldr_mutex);
   // ???
   if (!dm_cnt) return 0;

   log_it(2, "Delayed unpack: %s, %d bytes (%d)\n", cpath(), size, dm_cnt);
   if (cmd_shellcall(shl_unzip, callstr(), 0)) return 0;

   u32t  bsize = 0;
   void *fdata = freadfull(path, &bsize);
   if (bsize==size && fdata) {
      // all delayed modules unpacked! then free ZIP data (500k now!)
      if (dm_cnt<=1) release_ldi();
      memcpy(mem, fdata, size);
   } else bsize=0;
   if (fdata) hlp_memfree(fdata);

   return bsize?1:0;
}

int unpack_ldi(void) {
   void     *zip = sto_data(STOKEY_ZIPDATA);
   u32t    zsize = sto_size(STOKEY_ZIPDATA);
   ZIP        zz;
   char  *zfpath;
   u32t   zfsize = 0;
   int    errors = 0, errprev;

   if (zip_open(&zz, zip, zsize)) return 0;

   while (zip_nextfile(&zz, &zfpath, &zfsize)==0) {
      int      skip = 0;
      void *filemem = 0;
      char     path[QS_MAXPATH+1];
      if (!zfsize) continue;
      // skip START module - because we are here already ;)
      if (stricmp(MODNAME_START,zfpath)==0) continue;
      // full path (x:\name)
      snprintf(path, QS_MAXPATH, "%c:\\%s", 'A'+DISK_LDR, zfpath);
      errprev = errors;

      if (mod_delay) {
         char *ep = strchr(zfpath,'.');
         // delayed unpack (for large modules only)
         if (ep && (stricmp(ep,".EXE")==0 || stricmp(ep,".DLL")==0)) {
            if (!DelayList) DelayList = new TStrings;
            DelayList->AddObject(spstr(zfpath).replacechar('/','\\'), zz.dostime);
            skip = 1;
         }
      }
      if (!skip) {
         zip_readfile(&zz, &filemem);
         if (!filemem) errors++;
      }
      if (skip || filemem) {
         qserr res = 0;
         while (1) {
            io_handle fh;
            res = io_open(path, IOFM_CREATE_ALWAYS|IOFM_WRITE, &fh, 0);
            // log_printf("io_open(%s) err %08X!\n", path, res);
            if (!res) {
               if (skip) {
                  res = io_setsize(fh,zfsize);
                  if (!res) {
                     u32t data = UNPDELAY_SIGN;
                     if (io_write(fh, &data, 4)!=4) res = E_DSK_ERRWRITE;
                  }
               } else
               if (io_write(fh, filemem, zfsize)!=zfsize) res = E_DSK_ERRWRITE;
               io_close(fh);
               break;
            } else {
               char dir[QS_MAXPATH+1];
               _splitfname(path, dir, 0);
               res = io_mkdir(dir);
               if (res) break; else continue;
            }
         }
         if (filemem) hlp_memfree(filemem);
         if (res) errors++;
         /* ugly way to detect verbose build.
            (these messages scroll up actual info in it) */
         u32t lines;
         vio_getmode(0, &lines);
         if (lines<50)
            log_printf("%s %s, %d bytes\n", skip?"skip ":"unzip", path, zfsize);
         // set file time on success
         if (errprev!=errors) log_printf("%s: error (%08X)!\n", path, res); else {
            io_handle_info fi;
            io_timetoio(&fi.ctime, dostimetotime(zz.dostime));
            fi.atime = fi.ctime;
            fi.wtime = fi.ctime;
            io_setinfo(path, &fi, IOSI_CTIME|IOSI_ATIME|IOSI_WTIME);
         }
      }
   }
   zip_close(&zz);
#if 0
   str_list *lst = str_getlist(DelayList->Str);
   log_printlist("list:", lst);
   free(lst);
#endif
   if (errors && !mod_delay && !hlp_insafemode()) {
      log_printf("%u errors in QSINIT.LDI!\n", errors);
      return 0;
   }
   return 1;
}

/** micro-FSD file open.
    All io_mfs_* functions called under __microfsd_mux__ and
    have protection from second open in caller. */
u16t _std io_mfs_open(const char *name, u32t *filesize) {
   if (mfs_handle) return 1; else {
      char      fnn[24];
      u32t   action;
      u64t      fsz;
      qserr      rc;
      snprintf(fnn, 24, "A:\\%s", name);
      rc = io_open(fnn, IOFM_READ|IOFM_OPEN_EXISTING|IOFM_SHARE_READ|
         IOFM_SHARE_REN|IOFM_SHARE_DEL, &mfs_handle, &action);
      if (rc) log_printf("mfs open(%s) = %X\n", fnn, rc);
      if (!rc) rc = io_size(mfs_handle, &fsz);
      if (!rc && fsz>_2GB+_1GB) rc = E_SYS_TOOLARGE;
      if (!rc) rc = io_setstate(mfs_handle, IOFS_DETACHED, 1);
      if (rc) {
         if (mfs_handle) { io_close(mfs_handle); mfs_handle = 0; }
         return rc;
      }
      *filesize = fsz;
      return 0;
   }
}

u32t __cdecl io_mfs_read(u32t offset, void *buf, u32t readsize) {
   if (!mfs_handle || !readsize || !buf) return 0;
   if (io_seek(mfs_handle, offset, IO_SEEK_SET)==FFFF64) return 0;
   return io_read(mfs_handle, buf, readsize);
}

void __cdecl io_mfs_close(void) {
   if (mfs_handle) { io_close(mfs_handle); mfs_handle = 0; }
}

static u32t _std bitfind(void *data, u32t size, int on, u32t len, u32t hint) {
   bit_map bmp = NEW(bit_map);
   if (!bmp) return FFFF;
   bmp->init(data,size);
   u32t rc = bmp->find(on, len, hint);
   DELETE(bmp);
   return rc;
}

/** module loaded callback.
    Called before returning ok to user. But module is not guaranteed to
    be in loaded list at this time, it can be the one of loading imports
    for someone.
    ldr_mutex is captured at this time */
void _std mod_loaded(module *mh) {
   trace_start_hook(mh);
}

/** module free callback.
    ldr_mutex is captured at this time */
void _std mod_freeexps(module *mh) {
   mh->exports = 0;
   // free exports, not used below
   if (mh->exps) { free(mh->exps); mh->exps=0; }
   /* unchain ordinals first, because it rebuild thunks internally,
      then free thunks ;) */
   mod_apiunchain((u32t)mh,0,0,0);
   if (mh->thunks) { free(mh->thunks); mh->thunks=0; }
   // free trace info
   trace_unload_hook(mh);
   // free other possible function thunks
   mod_freethunk((u32t)mh,0);
   // free heap blocks, belonging to this module
   mem_freepool(QSMEMOWNER_COLIB, (u32t)mh);
}

/** process start callback.
    Function is not covered by any locks!
    This is the first call in the main thread of a new process - before main()
    itself.
    mod_exec() calls it in non-MT mode and MTLIB app start code in MT mode */
void _std mod_startcb(void) {
   init_stdio(mod_context());
   init_signals();
   trace_pid_start();
}

/** process exit callback.
    note, that this callback is in the CALLER context!
    To catch something in CALLEE context use mt_pexitcb() for tid 1.
    Function is not covered by MT lock, nor by ldr_mutex! */
s32t _std mod_exitcb(process_context *pq, s32t rc) {
   int errtype = 0;
   // !!!
   fcloseall_as(pq->pid);
   io_close_as(pq->pid, IOHT_FILE|IOHT_DIR|IOHT_MUTEX|IOHT_QUEUE|IOHT_EVENT);
   // free PUSHD stack
   if (pq->rtbuf[RTBUF_PUSHDST]) {
      pushd_free((void*)pq->rtbuf[RTBUF_PUSHDST]);
      pq->rtbuf[RTBUF_PUSHDST] = 0;
   }
   // free ansi specific data
   if (pq->rtbuf[RTBUF_ANSIBUF]) {
      free((void*)pq->rtbuf[RTBUF_ANSIBUF]);
      pq->rtbuf[RTBUF_ANSIBUF] = 0;
   }

   if (!mem_checkmgr()) errtype = 1; else
   if (!exi_checkstate()) errtype = 2;

   if (errtype) {
      char   prnbuf[256];
      int  ses_exit = in_mtmode && sys_con && se_sesno()==sys_con;
      // can`t use printf during common exit here, if was closed above ;)
      snprintf(prnbuf, 256, "\n Application \"%s\"\n made unrecoverable damage in"
        " system %s structures...\n\n This will produce trap or deadlock in nearest time.\n"
           " Reboot or %s to continue...\n", pq->self->name, errtype==1?"heap manager":
              "shared class", ses_exit?"switch back to your session":"press any key");
      panic_no_memmgr(prnbuf);
   }
   log_printf("memory check done\n");
   mod_apiunchain((u32t)pq->self,0,0,0);
   mod_fnunchain((u32t)pq->self,0,0,0);
   return rc;
}

void check_version(void) {
   int len = strlen(aboutstr_local);
   // at every place on Earth we can find a hero, who gets such message ;)
   if (strncmp(aboutstr, aboutstr_local, len)) {
      char *about = strdup(aboutstr), *msg;
      about[len] = 0;
      msg = sprintf_dyn("Boot module is \"%s\",^but LDI version is \"%s\"."
                         "^^Continue at own risc.", about, aboutstr_local);
      vio_msgbox("QSINIT.LDI version mismatch!", msg, MSG_OK|MSG_RED|MSG_WIDE, 0);
      free(about);
      free(msg);
   }
}

u32t _std mod_getmodpid(u32t module, u32t *parent) {
   if (!in_mtmode) {
      process_context *pq = mod_context();
      while (pq) {
         if ((u32t)pq->self==module) {
            if (parent) *parent = pq->pctx?pq->pctx->pid:0;
            return pq->pid;
         }
         pq = pq->pctx;
      }
   } else
      return get_mtlib()->getmodpid(module,parent);
   return 0;
}

u32t _std mod_checkpid(u32t pid) {
   if (pid)
      if (!in_mtmode) {
         process_context *pq = mod_context();
         while (pq) {
            if (pq->pid==pid) return 1;
            pq = pq->pctx;
         }
      } else
         return get_mtlib()->checkpidtid(pid,0)==0 ? 1 : 0;
   return 0;
}

/** return process context for the specified process id.
    Function exported, but unpublished in headers because it dangerous.
    It TURNS ON MT lock if MT mode active and returned value is non-zero */
process_context* _std mod_pidctx(u32t pid) {
   if (pid)
      if (!in_mtmode) {
         process_context *pq = mod_context();
         while (pq) {
            if (pq->pid==pid) return pq;
            pq = pq->pctx;
         }
      } else {
         // this call turns MT lock on in any case
  	     mt_prcdata      *pd = mtmux->getpd(pid);
  	     process_context *pq = pd?pd->piContext:0;
  	     // check pq<->pd match
  	     if ((mt_prcdata*)pq->rtbuf[RTBUF_PROCDAT]!=pd)
  	        sys_exfunc4(xcpt_prcerr, pq->self->name, pq->pid);

  	     if (!pq) mt_swunlock();
  	     return pq;
      }
   return 0;
}

/** query list of running processes.
    Function replaced by MTLIB on its load.
    @return pid list with zero in the last entry - in the process owned heap
            block. */
extern "C" u32t* _std START_EXPORT(mod_pidlist)(void) {
   u32t *rc = 0;
   if (!in_mtmode) {
      u32t cnt = 0, pass, ii = 0;

      for (pass=0; pass<2; pass++) {
         process_context *pq = mod_context();
         while (pq) {
            if (pass) rc[ii++] = pq->pid; else cnt++;
            pq = pq->pctx;
         }
         if (!pass) rc = (u32t*)malloc_local(sizeof(u32t)*(cnt+1)); else {
            // sort in in ascending order
            bswap(rc, sizeof(u32t), cnt);
            rc[cnt] = 0;
         }
      }
   }
   /* function should be catched and replaced by MTLIB, so just return 0 to
      hang in user code in case of malfunction. */
   return rc;
}

mod_addfunc table = { sizeof(mod_addfunc)/sizeof(void*)-1, 0, // number of entries
   mod_buildexps, mod_searchload, mod_unpack1, mod_unpack2, mod_unpack3,
   mod_freeexps, mem_alloc, mem_realloc, mem_free, freadfull, log_pushtm,
   sto_save, sto_flush, unzip_ldi, mod_startcb, mod_exitcb, log_memtable,
   hlp_memcpy, mod_loaded, sys_exfunc4, hlp_volinfo, io_fullpath,
   getcurdir_dyn, io_remove, mem_freepool, sys_notifyexec, sys_notifyevent,
   0,0,0, mt_muxcapture, mt_waithandle, mt_muxrelease, env_copy, 0, 0, fptostr,
   fpu_stsave, fpu_strest, 0, io_mfs_open, io_mfs_read, io_mfs_close,
   io_open, io_read, io_write, io_seek, io_size, io_setsize, io_close,
   io_lasterror, bitfind, setbits, mempanic, mt_tlsget, mt_tlsaddr,
   mt_tlsset };

void setup_loader_mt(void) {
   qserr   rc;
   if (table.in_mtmode) log_it(0, "????\n");
   table.in_mtmode = 1;

   if (mt_muxcreate(0, "__lxloader_mux__", &table.ldr_mutex) ||
      io_setstate(table.ldr_mutex, IOFS_DETACHED, 1) ||
         mt_muxcreate(0, "__microfsd_mux__", &table.mfs_mutex) ||
            io_setstate(table.mfs_mutex, IOFS_DETACHED, 1))
               log_it(0, "ldr/fsd mutexes err!\n");

   setup_fio_mt(&mod_secondary->dsk_canread, &mod_secondary->dsk_canwrite);
}

void setup_loader(void) {
   // export function table
   mod_secondary = &table;
   // flush log first time
   log_flush();
   // minor print
   log_printf("i/o delay value: %d\n", IODelay);
}
