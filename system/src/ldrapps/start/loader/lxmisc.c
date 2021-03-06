//
// QSINIT "start" module
// secondary LE/LX support code
//
#define LOG_INTERNAL
#define STORAGE_INTERNAL
#include "qsbase.h"
#include "qsmodext.h"
#include "qslxfmt.h"
#include "malloc.h"
#include "stdlib.h"
#include "qsconst.h"
#include "qsint.h"
#include "sysio.h"
#include "vioext.h"
#include "start_ord.h"
#include "internal.h"

extern u16t      _std IODelay;
extern u32t      M3BufferSize;
static u8t          *M3Buffer = 0;
extern FILE         **pstdout,
                     **pstdin;
extern char _std     aboutstr[];
extern char    aboutstr_local[];
static qshandle     ldr_mutex = 0,
                    mfs_mutex = 0;
// decompression routines
u32t _std DecompressM2(u32t DataLen, u8t* Src, u8t *Dst);
u32t _std DecompressM3(u32t DataLen, u8t* Src, u8t *Dst, u8t *Buffer);
void _std log_memtable(void*, void*, u8t*, u32t*, u32t);
/// call trace on parocess start
void  trace_start_hook(module *mh);
/// call trace on module free
void  trace_unload_hook(module *mh);
// cache routines
void _std cache_ioctl(u8t vol, u32t action);
u32t _std cache_read (u32t disk, u64t pos, u32t ssize, void *buf);
u32t _std cache_write(u32t disk, u64t pos, u32t ssize, void *buf);

extern mod_addfunc* _std mod_secondary;

#define checkexps(x) if (expsmax<=(x)) { \
   exp=mh->exps=realloc(mh->exps,(expsmax*=2)*sizeof(mod_export)); \
   memset(exp+(expsmax>>1),0,(expsmax>>1)*sizeof(mod_export)); \
}

/* build export table (full version, short in qsinit supports only 32bit and no
   forwards) */
int _std mod_buildexps(module *mh, lx_exe_t *eh) {
   lx_exp_b    *ee = (lx_exp_b*)((u8t*)eh+eh->e32_enttab);
   u32t        idx = 1, cnt=0, expsmax=32, thcnt = 0;
   mod_export *exp = mh->exps = malloc(sizeof(mod_export)*expsmax);
   mem_zero(exp);

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
      u8t *thunks = mh->thunks = malloc(EXPORT_THUNK*thcnt);
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
   module *md=(module*)mh;
   if (md->sign==MOD_SIGN) {
      if (buffer) {
         strncpy(buffer,md->name,E32MODNAME);
         buffer[E32MODNAME] = 0;
         strupr(buffer);
         return buffer;
      }
      return md->name;
   }
   return 0;
}

/* unzip one of delayed EXE/DLL from saved QSINIT.LDI data.
   When all files will be done - LDI data released */
int _std unzip_ldi(void *mem, u32t size, const char *path) {
   char  callstr[_MAX_PATH+128], *cpath, *unpp;
   void  *rfdata;
   u32t    bsize = 0, delaycnt = sto_dword(STOKEY_DELAYCNT);
   delaycnt &= 0xFF;
   // trying to use one buffer  in stack
   cpath = callstr + snprintf(callstr, 128, "/o /q /key %s b:\\ ", STOKEY_ZIPDATA);
   unpp  = _fullpath(cpath, path, _MAX_PATH);

   if (!unpp || toupper(unpp[0])!='B') return 0;
   unpp+=3;
   while (*unpp=='/'||*unpp=='\\') unpp++;
   memmove(cpath, unpp, strlen(unpp)+1);
   log_it(2, "Delayed unpack: %s, %d bytes (%d)\n", cpath, size, delaycnt);
   if (cmd_shellcall(shl_unzip,callstr,0)) return 0;

   rfdata = freadfull(path,&bsize);
   if (bsize==size && rfdata) {
      // all delayed modules unpacked!
      if (--delaycnt==0) {
         void *mem = sto_data(STOKEY_ZIPDATA);
         hlp_memfree(mem);

         sto_save(STOKEY_DELAYCNT,0,0,0);
         sto_save(STOKEY_ZIPDATA,0,0,0);
      } else
         sto_save(STOKEY_DELAYCNT, &delaycnt, 4, 1);

      memcpy(mem, rfdata, size);
   } else bsize=0;
   if (rfdata) hlp_memfree(rfdata);

   return bsize?1:0;
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
   mh->exports=0;
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
    function is not covered by MT lock, nor by ldr_mutex!
    note, that callbacks are called in CALLER context! */
int _std mod_startcb(process_context *pq) {
   init_stdio(pq);
   return 0;
}

/** process exit callback.
    note, that callbacks are called in CALLER context! 
    To catch something in CALLEE context use mt_pexitcb() for tid 1.
    Function is not covered by MT lock, nor by ldr_mutex! */
s32t _std mod_exitcb(process_context *pq, s32t rc) {
   int errtype = 0;
   // !!!
   fcloseall_as(pq->pid);
   io_close_as(pq->pid, IOHT_FILE|IOHT_DIR|IOHT_MUTEX);

   if (pq->rtbuf[RTBUF_PUSHDST]) {
      pushd_free((void*)pq->rtbuf[RTBUF_PUSHDST]);
      pq->rtbuf[RTBUF_PUSHDST] = 0;
   }
   if (pq->rtbuf[RTBUF_ANSIBUF]) {
      free((void*)pq->rtbuf[RTBUF_ANSIBUF]);
      pq->rtbuf[RTBUF_ANSIBUF] = 0;
   }

   if (!mem_checkmgr()) errtype = 1; else
   if (!exi_checkstate()) errtype = 2;

   if (errtype) {
      char prnbuf[128];
      vio_clearscr();
      vio_setcolor(VIO_COLOR_LRED);
      // can`t use printf here, if was closed above ;)
      snprintf(prnbuf, 128, "\n Application \"%s\"\n made unrecoverable damage in"
        " system %s structures...\n\n", pq->self->name, errtype==1?
           "heap manager":"shared classes");
      vio_strout(prnbuf);
      snprintf(prnbuf, 128, " This will produce trap or deadlock in nearest time.\n"
         " Reboot or press any key to continue...\n");
      vio_strout(prnbuf);
      vio_setcolor(VIO_COLOR_RESET);
      key_wait(60);
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

u32t _std mod_getmodpid(u32t module) {
   if (!in_mtmode) {
      process_context *pq = mod_context();
      while (pq) {
         if ((u32t)pq->self==module) return pq->pid;
         pq = pq->pctx;
      }
   } else
      return get_mtlib()->getmodpid(module);
   return 0;
}

mod_addfunc table = { sizeof(mod_addfunc)/sizeof(void*)-1, // number of entries
   mod_buildexps, mod_searchload, mod_unpack1, mod_unpack2, mod_unpack3,
   mod_freeexps, mem_alloc, mem_realloc, mem_free, freadfull, log_pushtm,
   sto_save, sto_flush, unzip_ldi, mod_startcb, mod_exitcb, log_memtable,
   hlp_memcpy, mod_loaded, sys_exfunc4, hlp_volinfo, io_fullpath,
   getcurdir_dyn, io_remove, mem_freepool, sys_notifyexec, 0, 0, 0,
   mt_muxcapture, mt_muxwait, mt_muxrelease};

void setup_loader_mt(void) {
   qserr   rc;
   if (table.in_mtmode) log_it(0, "????\n");
   table.in_mtmode = 1;

   if (mt_muxcreate(0, "__lxloader_mux__", &table.ldr_mutex) ||
      io_setstate(table.ldr_mutex, IOFS_DETACHED, 1) ||
         mt_muxcreate(0, "__microfsd_mux__", &table.mfs_mutex) ||
            io_setstate(table.mfs_mutex, IOFS_DETACHED, 1))
               log_it(0, "ldr/fsd mutexes err!\n");
}

void setup_loader(void) {
   // export function table
   mod_secondary = &table;
   // flush log first time
   log_flush();
   // minor print
   log_printf("i/o delay value: %d\n",IODelay);
}
