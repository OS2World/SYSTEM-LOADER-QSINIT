//
// QSINIT "start" module
// secondary LE/LX support code
//
#define LOG_INTERNAL
#define MODULE_INTERNAL
#define STORAGE_INTERNAL
#include "qslog.h"
#include "clib.h"
#include "qsutil.h"
#include "qsmod.h"
#include "qsmodext.h"
#include "qslxfmt.h"
#include "malloc.h"
#include "stdlib.h"
#include "qsconst.h"
#include "qsstor.h"
#include "qsshell.h"
#include "qsint.h"
#include "vio.h"
#include "start_ord.h"
#include "internal.h"

extern u16t  _std IODelay;
extern u32t  M3BufferSize;
static u8t  *M3Buffer = 0;
extern FILE     **pstdout;

// decompression routines
u32t _std DecompressM2(u32t DataLen, u8t* Src, u8t *Dst);
u32t _std DecompressM3(u32t DataLen, u8t* Src, u8t *Dst, u8t *Buffer);
int  _std log_push(int level, const char *str);
void _std log_memtable(void*, void*, u8t*, u32t*, u32t);
/// reset ini file cache
void  reset_ini_cache(void);
/// call trace on parocess start
void  trace_start_hook(module *mh);
/// call trace on module free
void  trace_unload_hook(module *mh);
// cache routines
void _std cache_ioctl(u8t vol, u32t action);
u32t _std cache_read (u32t disk, u64t pos, u32t ssize, void *buf);
u32t _std cache_write(u32t disk, u64t pos, u32t ssize, void *buf);

extern mod_addfunc* _std mod_secondary;
extern mod_export*  _std mod_findexport(module *mh, u16t ordinal);

#define checkexps(x) if (expsmax<=(x)) { \
   exp=mh->exps=realloc(mh->exps,(expsmax*=2)*sizeof(mod_export)); \
   memset(exp+(expsmax>>1),0,(expsmax>>1)*sizeof(mod_export)); \
}

/* build export table (full version. short in qsinit support only 32bit and no
   forwards) */
int _std mod_buildexps(module *mh, lx_exe_t *eh) {
   lx_exp_b    *ee = (lx_exp_b*)((u8t*)eh+eh->e32_enttab);
   u32t        idx = 1, cnt=0, expsmax=32, thcnt = 0;
   mod_export *exp = mh->exps = malloc(sizeof(mod_export)*expsmax);
   memZero(exp);

   while (ee->b32_cnt) {
      u32t e32=0, bsize=0;
      switch (ee->b32_type&~TYPEINFO) {
         case EMPTY   : idx+=ee->b32_cnt; bsize=2; break;
         case ENTRY32 :
            e32 = 2;
         case ENTRY16 :
            if (ee->b32_obj==0||ee->b32_obj>eh->e32_objcnt) // invalid object?
               return MODERR_BADEXPORT; else
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
            return MODERR_UNSUPPORTED;
         case ENTRYFWD: // forward entry
            {
               u8t *entry = (u8t*)ee + (bsize=sizeof(lx_exp_b));
               while (ee->b32_cnt--) {
                  // forwarding to the name table is not supported
                  if ((*entry++&1)==0) return MODERR_UNSUPPORTED;
                  exp[cnt].ordinal = idx++;
                  exp[cnt].forward = *(u16t*)entry;
                  // invalid module ordinal?
                  if (!exp[cnt].forward||exp[cnt].forward>eh->e32_impmodcnt)
                     return MODERR_BADEXPORT;
                  entry+=2;
                  exp[cnt].address = *(u32t*)entry;
                  entry+=4; bsize+=7;
                  checkexps(++cnt);
               }
            }
            break;
         default:
            return MODERR_BADEXPORT;
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

void _std mod_freeexps(module *mh) {
   mh->exports=0;
   if (mh->thunks) { free(mh->thunks); mh->thunks=0; }
   if (mh->exps)   { free(mh->exps);   mh->exps  =0; }
   mod_apiunchain((u32t)mh,0,0,0);
   trace_unload_hook(mh);
}

u32t _std mod_searchload(const char *name, u32t *error) {
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
   return mod_load(path, 0, error, 0);
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

u32t _std mod_getpid(void) {
   process_context *pq = mod_context();
   return pq?pq->pid:0;
}

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
   cpath = callstr + snprintf(callstr, 128, "/o /q /key %s 1:\\ ", STOKEY_ZIPDATA);
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

int _std mod_startcb(process_context *pq) {
   reset_ini_cache();
   trace_start_hook(pq->self);
   init_stdio(pq);
   return 0;
}

s32t _std mod_exitcb(process_context *pq, s32t rc) {
   fcloseall();
   if (pq->rtbuf[RTBUF_PUSHDST]) {
      pushd_free((void*)pq->rtbuf[RTBUF_PUSHDST]);
      pq->rtbuf[RTBUF_PUSHDST] = 0;
   }
   if (pq->rtbuf[RTBUF_ANSIBUF]) {
      free((void*)pq->rtbuf[RTBUF_ANSIBUF]);
      pq->rtbuf[RTBUF_ANSIBUF] = 0;
   }

   if (!memCheckMgr()) {
      vio_clearscr();
      vio_setcolor(VIO_COLOR_LRED);
      printf("\n Application %s made unrecoverably damage in\n"
             " system heap manager structures...\n", pq->self->name);
      printf(" This will produce trap or deadlock in nearest time.\n"
             " Reboot or press any key to continue...\n");
      vio_setcolor(VIO_COLOR_RESET);
      key_wait(60);
   }
   log_printf("memory check done\n");
   mod_apiunchain((u32t)pq->self,0,0,0);
   return rc;
}

mod_addfunc table = { sizeof(mod_addfunc)/sizeof(void*)-1, // number of entries
   &mod_buildexps, &mod_searchload, &mod_unpack1, &mod_unpack2, &mod_unpack3,
   &mod_freeexps, &memAlloc, &memRealloc, &memFree, &freadfull, &log_push,
   &sto_save, &sto_flush, &unzip_ldi, &mod_startcb, &mod_exitcb, &log_memtable,
   &hlp_memcpy};

extern module*_std _Module;

void setup_loader(void) {
   process_context *pq;
   // export function table
   mod_secondary=&table;
   // flush log first time
   log_flush();
   // minor print
   log_printf("i/o delay value: %d\n",IODelay);

   /* because process context is a CONSTANT value for all processes (it swapped
      on start/exit) - we just replace fake stdin/stdout/stderr exports to
      pointers to process context data.
      So every module (including DLLs!) will be fixuped with stdin, which
      points to process context of current executing module */
   pq = mod_context();
   if (pq) {
      mod_export *me = mod_findexport(_Module, ORD_START_stdin);
      me->address = me->direct = (u32t)&pq->rtbuf[RTBUF_STDIN];

      me = mod_findexport(_Module, ORD_START_stdout);
      me->address = me->direct = (u32t)&pq->rtbuf[RTBUF_STDOUT];
      pstdout = (FILE**)me->direct;

      me = mod_findexport(_Module, ORD_START_stderr);
      me->address = me->direct = (u32t)&pq->rtbuf[RTBUF_STDERR];

      me = mod_findexport(_Module, ORD_START_stdaux);
      me->address = me->direct = (u32t)&pq->rtbuf[RTBUF_STDAUX];
   }
}
