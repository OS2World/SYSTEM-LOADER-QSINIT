#include "clib.h"
#include "qsint.h"
#define MODULE_INTERNAL
#include "qsmod.h"
#include "qsutil.h"
#include "ff.h"
#include "seldesc.h"
#include "qsinit.h"
#include "qsbinfmt.h"
#include "qspdata.h"
#include "qserr.h"

/** delete module file from disk after success load.
    Flag is not published in qsmod.h - just for safeness ;) */
#define LDM_UNLINKFILE  0x0001

mod_addfunc *mod_secondary = 0; // secondary function table, inited by 2nd part
module           *mod_list = 0; // loaded module list
module          *mod_ilist = 0; // currently loading modules list
module           *mod_self = 0, // QSINIT module reference
                *mod_start = 0; // START module reference
u8t              *page_buf = 0; // 4k buffer for various needs
u32t           lastusedpid = 1; // unique pid for processes
#ifdef INITDEBUG
u8t              mod_delay = 0; // exe delayed unpack
#else
u8t              mod_delay = 1; // exe delayed unpack
#endif

static char     qsinit_str[] = MODNAME_QSINIT;
extern u32t  exptable_data[];
extern u16t    qsinit_size;
extern
MKBIN_HEADER  *pbin_header;
extern u32t       highbase;     // base of 32bit object
extern volatile
mt_proc_cb    mt_exechooks;

// launch routines
u32t _std launch32(module *md, u32t env, u32t cmdline);
u32t _std dll32init(module *md, u32t term);
int  _std mod_buildexps(module *mh, lx_exe_t *eh);

static u32t mod_makeaddr(module *md, u32t Obj, u32t offset) {
   if ((md->obj[--Obj].flags&OBJBIGDEF)==0) return 0; else
      return (u32t)md->obj[Obj].address+offset;
}

// link module to list
void mod_listadd(module **list, module *module) {
   module->prev = 0;
   if ((module->next = *list)) module->next->prev = module;
   *list = module;
}

// unlink module from list
void mod_listdel(module **list, module *module) {
   // not linked?
   if (!module->prev && !module->next && module!=*list) return;
   if (module->prev) module->prev->next=module->next; else *list=module->next;
   if (module->next) module->next->prev=module->prev;
   module->next=0;
   module->prev=0;
}

// merge two module lists
void mod_listlink(module **to, module *first) {
   module *lf=*to;
   if (!first) return;
   first->prev = 0;
   *to = first;
   if (!lf) return;
   while (first->next) first = first->next;
   lf->prev    = first;
   first->next = lf;
}

void mod_listflags(module *first, u32t fl_or, u32t fl_andnot) {
   module *lf = first;
   while (lf) {
      lf->flags |= fl_or;
      lf->flags &=~fl_andnot;
      lf = lf->next;
   }
}

mod_export *mod_findexport(module *mh, u16t ordinal) {
   //log_misc(3, "searching %s.%d\n", mh->name, ordinal);
   if (mh&&mh->exports&&mh->exps) {
      mod_export *exp = mh->exps;
      u32t        cnt = mh->exports;
      while (cnt--) {
         if (exp->ordinal==ordinal) {
            // resolve forward module entry
            if (exp->forward) {
               mod_export *mf;
               if (exp->sel==0xFFFF) return 0; // circular reference?
               exp->sel = 0xFFFF;
               mf = mod_findexport(mh->impmod[exp->forward-1],exp->address);
               if (!mf) { exp->sel=0; return 0; }
               // update export to actual values
               exp->is16    = mf->is16;
               exp->address = mf->address;
               exp->sel     = mf->sel;
               exp->forward = 0;
            }
            return exp;
         }
         exp++;
      }
   }
   return 0;
}

static void mod_unloadimports(module *md) {
   int idx=0;
   if (md->flags&MOD_IMPDONE) return;
   // flag here to prevent double free from used modules
   md->flags|=MOD_IMPDONE;
   while (md->impmod[idx]) idx++;
   while (--idx>=0) mod_free((u32t)md->impmod[idx]);
}

// call init/term for single dll module
static u32t mod_initterm(module *md, u32t term) {
   u32t rc;
   if ((md->flags&MOD_LIBRARY)==0) return 1;
   if (!md->start_ptr||(md->flags&(term?MOD_TERMDONE:MOD_INITDONE))!=0||
      term&&md->usage>1) return 1;
   rc = dll32init(md,term);
   // flag about function was called at least once ;)
   if (rc) md->flags|=term?MOD_TERMDONE:MOD_INITDONE;
   return rc;
}

/* call init/term for all modules, used by module "md" and for
   "md" itself in case of DLL */
static u32t mod_initterm_all(module *md, u32t term, u32t modlim) {
   u32t ii,rc=1;
   // terminate self first
   if (term&&(md->flags&MOD_LIBRARY)!=0) rc=mod_initterm(md,1);
   for (ii=0;ii<modlim&&md->impmod[ii];ii++) {
      module *mst=md->impmod[ii];
      rc=mod_initterm(mst,term);
      // failed on init?
      if (!rc&&!term) break;
   }
   // init self after all used modules
   if (rc&&!term&&(md->flags&MOD_LIBRARY)!=0) rc=mod_initterm(md,0);
   // error: terminating already inited modules and exit
   if (!rc&&!term) mod_initterm_all(md,1,ii);
   return rc;
}

u32t mod_query(const char *name, u32t searchtype) {
   u32t len = (searchtype&MODQ_LENMASK)>>MODQ_LENSHIFT, ii, pos,
      noinc = (searchtype&MODQ_NOINCR)!=0;
   if (!len) len=strlen(name);
   if (len>E32MODNAME) return 0;
   if (!mod_self) {
      // adding QSINIT
      u32t   sz16 = Round16(sizeof(module) + 255*sizeof(mod_export));
      module *slf = hlp_memallocsig(sz16 + EXPORT_THUNK*256, "MODq", QSMA_READONLY);
      u32t   *exp = exptable_data, ord=0;
      u8t   noffs = 0;
      slf->baseaddr = slf;
      slf->usage    = slf->objects = 1;
      slf->sign     = MOD_SIGN;
      slf->flags    = MOD_SYSTEM;
      strcpy(slf->name,qsinit_str);
      slf->mod_path = slf->name;
      slf->exps     = (mod_export*)((u8t*)slf + sizeof(module));
      slf->thunks   = (u8t*)slf + sz16;
      memset(slf->thunks, 0xCC, EXPORT_THUNK*256);

      slf->obj[0].address = (void*)highbase;
      slf->obj[0].size    = pbin_header->pmSize;
      slf->obj[0].flags   = OBJBIGDEF|OBJREAD|OBJWRITE|OBJEXEC;
      slf->obj[0].sel     = get_flatcs();
      // setting up export table
      while (*exp!=0xFFFF0000) {
         if (*exp>>16==0xFFFF) {
            auto u32t nord = *exp&0xFFFF;
            if (nord>=0xFFF0) noffs = -(s32t)*exp; else ord = nord;
         } else {
            auto mod_export *eptr = slf->exps+slf->exports;
            // check for zero offset - (re)moved function and ignore it
            if ((eptr->direct=*exp)!=0) {
               slf->exports++;

               if (noffs) {
                  eptr->address = eptr->direct;
                  noffs--;
               } else {
                  u8t    *thunk = slf->thunks + ord*EXPORT_THUNK;
                  eptr->address = (u32t)thunk;
                  *thunk++      = 0xE9;
                  *(u32t*)thunk = eptr->direct - ((u32t)thunk+4);
               }
               eptr->ordinal = ord;
            }
            ord++;
         }
         exp++;
      }
      mod_listadd(&mod_list, mod_self = slf);
   }
   pos = (searchtype&MODQ_POSMASK) >> MODQ_POSSHIFT;
   searchtype&=3;
   // searching for module
   for (ii=0;ii<2;ii++)
      if (searchtype&&!(searchtype&1<<ii)) continue; else {
         module *list=ii?mod_ilist:mod_list;
         while (list) {
            if (!strnicmp(list->name,name,len) && strlen(list->name)==len)
               if (pos) pos--; else {
                  if (!noinc) list->usage++;
                  return (u32t)list;
               }
            list=list->next;
         }
      }
   // log_misc(2, "no mod: %s\n", name);
   return 0;
}

u32t mod_load(char *path, u32t flags, qserr *error, void *extdta) {
   if (error) *error=0;
   if (!path) return 0; else {
      char   *buf;
      u32t     rc = 0, size;
      char   *img;
      void   *mod = 0;                              // module location
      module  *md = 0;                              // module data

      if (mod_secondary) {
         buf = (char*)mod_secondary->freadfull(path, &size);
         img = buf;
         if (!img) rc = E_SYS_NOFILE; else
            if (*(u32t*)img==0) // file is empty, may be delayed \ unpack?
               if (!mod_secondary->unzip_ldi(img, size, path)) rc = E_MOD_CRCERROR;
      } else {
         u32t  len = strlen(path),
              used = len+4+sizeof(FIL)+4;           // +4 bytes for unpack
         FIL   *fl;
         buf = (char*)hlp_memallocsig(_64KB, "MODb", 0);
         fl  = (FIL*)(buf+len+4);

         if (path[1]!=':') {
            buf[0]='0'+DISK_LDR; buf[1]=':'; buf[2]='\\';
            strcpy(buf+3, path);
         } else strcpy(buf, path);
         // reading module
         do {
            rc   = f_open(fl, buf, FA_READ);
            if (rc!=FR_OK) { rc=E_SYS_NOFILE; break; }
            size = fl->fsize;
            buf  = (char*)hlp_memrealloc(buf, size+used);
            fl   = (FIL*)(buf+len+4);                  // update pointer
            img  = buf + used;                         // module dest.
            rc   = f_read(fl, img, size, (UINT*)&used);
            if (rc!=FR_OK) { rc=E_MOD_READERROR; break; }
            f_close(fl);
         } while (false);
         // for mod_path below
         path = buf;
      }
      if (!rc && size<=sizeof(lx_exe_t)) rc = E_MOD_EMPTY;    
      // early error occured, exiting
      if (rc) {
         log_printf("lx read(%s): %d\n", path, rc);
         if (buf) hlp_memfree(buf);
         if (error) *error = rc;
         return 0;
      }

      rc = E_MOD_NOT_LX;
      /* error codes: 1 - not LE/LX, 2 - bad flags, 3 - empty module,
                      4 - unsupported feature, 5 - broken file,
                      6 - no free selector, 7 - obj load error,
                      8 - invalid page table, 9 - additional code not loaded
                      10 - too many imported modules, 11 - invalid fixup,
                      12 - no ordinal, 13 - decompress error */
      do {
         mz_exe_t          *mz = (mz_exe_t*)img;
         u32t      oldhdr_size = mz->e_magic!=EMAGIC?0:mz->e_lfanew;
         register lx_exe_t *eh = (lx_exe_t*)((u8t*)img + oldhdr_size);
         lx_obj_t          *ot = (lx_obj_t*)((u8t*)eh + eh->e32_objtab);
         u32t        ii, vsize = 0,
             selbase, selcount = 0;
         u8t*             tptr;

         // invalid signature
         if (eh->e32_magic!=LXMAGIC && eh->e32_magic!=LEMAGIC) break;
         // invalid constant fields
         if (eh->e32_border||eh->e32_worder||eh->e32_level||
             eh->e32_pagesize!=PAGESIZE) break;
         rc++;
         // module with errors or pre-upplied fixups or pdd/vdd
         if ((eh->e32_mflags&(E32NOLOAD|E32NOINTFIX))||
             (eh->e32_mflags&E32MODMASK)>E32MODDLL) break;
         rc++;
         // module without executable code
         if (!eh->e32_mpages||!eh->e32_startobj||!eh->e32_objmap||!eh->e32_restab||
             !eh->e32_objcnt||!eh->e32_objtab||eh->e32_startobj>eh->e32_objcnt) break;
         rc++;
         // unsupported feature
         if (eh->e32_instpreload) break;
         // too many used modules
         if (eh->e32_impmodcnt>MAX_IMPMOD) { rc=E_MOD_MODLIMIT; break; }
         rc++;
         // broken file
         if (size-oldhdr_size-eh->e32_objcnt*sizeof(lx_obj_t)<eh->e32_objtab) // broken object table
            break;
         if (size-oldhdr_size-(eh->e32_magic==LXMAGIC?sizeof(lx_map_t):       // broken page table
            sizeof(le_map_t))*eh->e32_mpages<eh->e32_objmap) break;

         // calculate objects offset & size
         for (ii=0;ii<eh->e32_objcnt;ii++) {
            u32t flags=ot[ii].o32_flags;
            if (flags&(OBJINVALID|OBJRSRC)||!ot[ii].o32_size) {
               ot[ii].o32_reserved = 2;      // unused object
            } else {
               u32t sizef = vsize;
               if (flags&OBJALIAS16||(flags&OBJBIGDEF)==0) { selcount++; sizef++; }
               ot[ii].o32_reserved = sizef;  // new base + sel flag in low bit
               vsize += PAGEROUND(ot[ii].o32_size);
            }
         }
         rc++;
         // no free selector
         if (selcount)
            if ((selbase=hlp_selalloc(selcount))==0) break;

         // allocate physical storage for all loadable objects
         ii = sizeof(module)+sizeof(mod_object)*(eh->e32_objcnt-1);
         rc = mod_secondary?0:(EXPORT_THUNK+sizeof(mod_export))*MAX_EXPSTART;
         mod = hlp_memallocsig(vsize+ii+rc + QS_MAXPATH+2, "MODl", 0);

         // fill module handle data (note: memory zeroed in malloc)
         md  = (module*)((char*)mod + rc + vsize);
         // for internal export processor
         if (!mod_secondary) {
            md->thunks = (char*)mod + vsize;  // must be paragraph aligned
            md->exps   = (mod_export*)((u8t*)md->thunks + EXPORT_THUNK*MAX_EXPSTART);
         }
         md->mod_path  = (char*)md + ii;
         md->sign      = MOD_SIGN;
         md->usage     = 1;
         md->objects   = eh->e32_objcnt;
         md->baseaddr  = mod;
         md->flags     = (eh->e32_mflags&E32MODMASK)==E32MODDLL?MOD_LIBRARY:0;
         // make full path if possible
         if (mod_secondary) mod_secondary->fullpath(md->mod_path, path, QS_MAXPATH+1);
            else strncpy(md->mod_path, buf, QS_MAXPATH);
         // copy module name
         tptr = (u8t*)eh+eh->e32_restab;
         ii   = *tptr&E32MODNAME;
         memcpy(md->name, tptr+1, E32MODNAME);
         // flag pre-applied fixups (not used now)
         //if ((eh->e32_mflags&E32NOINTFIX)!=0) md->flags|=MOD_NOFIXUPS;

         rc = 0;
         // setup selectors & base addr for objects
         for (ii=0,selcount=0; ii<eh->e32_objcnt; ii++) {
            u16t objsel = 0;
            u32t oflags = ot[ii].o32_flags;
            if (ot[ii].o32_reserved!=2)
               md->obj[ii].address = (void*)((ot[ii].o32_reserved&~1) + (u32t)mod);

            if (ot[ii].o32_reserved&1) {
               objsel = selbase + selcount++*SEL_INCR;
               // check for start & stack objects
               if (ii+1==eh->e32_startobj) rc = E_MOD_START16;
                  else
               if ((md->flags&MOD_LIBRARY)==0 && ii+1==eh->e32_stackobj)
                  rc = E_MOD_STACK16;
            } else
            if (oflags&OBJBIGDEF) {
               objsel = get_flatcs();
               if ((oflags&OBJEXEC)==0) objsel += 8;
            }

            md->obj[ii].sel   = objsel;
            md->obj[ii].flags = oflags;
         }
         if (rc) break;
         // modify offsets to LX header start
         eh->e32_datapage-=oldhdr_size;
         // adding self into the loading modules list
         md->flags|=MOD_LOADING|(!mod_ilist?MOD_LOADER:0);
         mod_listadd(&mod_ilist, md);
         // fill entry table
         if (eh->e32_enttab) {
            u8t *cnt = (u8t*)eh+eh->e32_enttab; // is entry table data present?
            if (!*cnt) eh->e32_enttab=0; else
            rc = mod_secondary?(*mod_secondary->mod_buildexps)(md,eh):
                 mod_buildexps(md,eh);
         }
         if (rc) break;

         // fill import table (and loading dependencies)
         if (eh->e32_impmodcnt) {
            u8t *table = (u8t*)eh+eh->e32_impmod;
            for (ii=0;ii<eh->e32_impmodcnt;ii++) {
               u8t len=*table++;
               module *dll = (module*)mod_query(table, len<<8);
               if (!dll)
                  if (!mod_secondary) rc = E_MOD_NOEXTCODE; else {
                     u8t svbyte=table[len]; table[len]=0;
                     dll = (module*)(*mod_secondary->mod_searchload)(table,&rc);
                     table[len]=svbyte;
                  }
               if (!dll) break;
               md->impmod[ii]=dll;
               table+=len;
            }
         }
         if (rc) break;

         // unpack objects
         for (ii=0;ii<eh->e32_objcnt;ii++)
            if (ot[ii].o32_reserved!=2) {
               u16t objsel, flatcs = get_flatcs();
               rc = mod_unpackobj(md, eh, ot, ii, (u32t)md->obj[ii].address, 0);
               if (rc) break;
               md->obj[ii].size    = ot[ii].o32_size;
               md->obj[ii].orgbase = ot[ii].o32_base;
               /* setup object selector (but do not touch FLAT selectors, assigned
                  to the big objects ;) */
               objsel = md->obj[ii].sel;
               if (objsel && (objsel<flatcs || objsel>flatcs+8)) {
                  u32t  size = ot[ii].o32_size;
                  int  bytes = size<_64KB;
                  hlp_selsetup(objsel, (u32t)md->obj[ii].address,
                     (size<_64KB?size:_64KB)-1, (ot[ii].o32_flags&OBJEXEC?QSEL_CODE:0)|
                        QSEL_16BIT|QSEL_BYTES|QSEL_LINEAR);
               }
            }
         if (rc) break;

         // make start/stack pointers
         md->start_ptr = mod_makeaddr(md,eh->e32_startobj,eh->e32_eip);
         if ((md->flags&MOD_LIBRARY)==0)
            md->stack_ptr = mod_makeaddr(md,eh->e32_stackobj,eh->e32_esp);

         log_misc(2, "%s: base %08X start %08X stack %08X\n", md->name, md->baseaddr,
            md->start_ptr, md->stack_ptr);
      } while (false);

      // this module was first in recursive calls, so here we are done
      if (!rc&&md&&(md->flags&MOD_LOADER)!=0) {
         u32t rci = mod_initterm_all(md,0,FFFF);
         if (!rci) rc=E_MOD_INITFAILED; else {
            // drop loader flags
            mod_listflags(mod_ilist,0,MOD_LOADING|MOD_LOADER);
            // merge loading and loaded lists
            mod_listlink(&mod_list, mod_ilist);
            mod_ilist=0;
         }
      }
      // process error
      if (error) *error=rc;
      if (rc) {
         log_printf("lx err(%s): %X\n", md?md->mod_path:path, rc);
         if (md) {
            u32t oi;  // free selectors (zero value will be ignored)
            for (oi=0;oi<md->objects;oi++) hlp_selfree(md->obj[oi].sel);
            // free all modules, used by this module
            mod_unloadimports(md);
            // unlink self from loading list
            mod_listdel(&mod_ilist, md);
         }
         if (mod) hlp_memfree(mod);
         return 0;
      } else
      if (flags&LDM_UNLINKFILE)
         if (mod_secondary) mod_secondary->unlink(md->mod_path); else
            f_unlink(md->mod_path);
      // free module image
      hlp_memfree(buf);
      // callback to START
      if (md&&mod_secondary) (*mod_secondary->mod_loaded)(md);

      return (u32t)md;
   }
}

static void fixup_err(char where, u8t *prev, u8t *curr, u32t pos) {
   static char fmt[64];
   snprintf(fmt,64,"fix(%%c): prev %%%db,\n\tcurr %%16b, pos %%d\n", prev?curr-prev:32);
   log_printf(fmt,where,prev,curr,pos);
}

#define FIXUP_ERR(where) { fixup_err(where,prev,cstart,start-cstart); return E_MOD_BADFIXUP; }
#define FIXUP_ORD(where) { fixup_err(where,prev,cstart,start-cstart); return E_MOD_NOORD; }
#define FIXUP_SUP(where) { fixup_err(where,prev,cstart,start-cstart); return E_MOD_UNSUPPORTED; }

/** load and unpack objects
    e32_datapage must be changed to offset from LX header, not begin of file */
int mod_unpackobj(module *mh, lx_exe_t *eh, lx_obj_t *ot, u32t object,
   u32t destaddr, void *extreq)
{
   register lx_obj_t  *oe=ot+object;
   modobj_extreq *strange=(modobj_extreq*)extreq;
   u32t  size = PAGEROUND(oe->o32_size),
           ii = oe->o32_mapsize,
          idx = oe->o32_pagemap-1,
        pages = size>>PAGESHIFT,
       sflags = strange?strange->flags:0;
   if ((idx+=ii-1)>eh->e32_mpages) return E_MOD_OBJLOADERR;

   log_misc(2, "obj %d base %08X sz %d\n",object+1,destaddr,oe->o32_size);
   // zero-fill entire object
   memset((void*)destaddr,0,size);
   // LE page table
   if (eh->e32_magic==LEMAGIC) {
      le_map_t *pt = (le_map_t*)((char*)eh + eh->e32_objmap);
      pt+=idx;
      while (ii) {
         u32t pgsize = idx+1==eh->e32_mpages?eh->e32_pageshift:PAGESIZE;
         ii--;
/*
log_misc(2, "obj %d page %d sz %d fl %04X ofs %08X\n",object,ii,pgsize,
   pt->pageflags,eh->e32_datapage+(idx<<PAGESHIFT));*/

         if (LEPAGEIDX(*pt)!=idx+1) return E_MOD_INVPAGETABLE;
         switch (pt->pageflags&LE_ZEROED) {
            case LE_VALID:
               memcpy((char*)destaddr+(ii<<PAGESHIFT), (char*)eh+eh->e32_datapage+
                  (idx<<PAGESHIFT),pgsize);
               break;
            case LE_ITERDATA:
               return E_MOD_UNSUPPORTED;
               break;
         }
         pt--; idx--;
      }
   } else { // LX page table
      lx_map_t *pt = (lx_map_t*)((char*)eh + eh->e32_objmap);
      pt+=idx;
      while (ii) {
         u32t pgsize = pt->o32_pagesize, failed=0;
         u8t    *dst = (u8t*)destaddr+(--ii<<PAGESHIFT),
                *src = (u8t*)eh+eh->e32_datapage+
                       (pt->o32_pagedataoffset<<eh->e32_pageshift);
/*
log_misc(2, "obj %d page %d sz %d fl %04X ofs %08X\n",object,ii,pgsize,
   pt->o32_pageflags,eh->e32_datapage+(pt->o32_pagedataoffset<<eh->e32_pageshift));
*/
         switch (pt->o32_pageflags) {
            case LX_VALID:
               memcpy(dst,src,pgsize);
               break;
            case LX_ITERDATA:
               if (!mod_secondary) failed=E_MOD_NOEXTCODE; else {
                  pgsize = (*mod_secondary->mod_unpack1)(pgsize,src,dst);
                  if (!pgsize) failed=E_MOD_ITERPAGEERR;
               }
               break;
            case LX_ITERDATA2:
               if (!mod_secondary) failed=E_MOD_NOEXTCODE; else {
                  pgsize = (*mod_secondary->mod_unpack2)(pgsize,src,dst);
                  if (!pgsize) failed=E_MOD_ITERPAGEERR;
               }
               break;
            case LX_ITERDATA3:
               if (!mod_secondary) failed=E_MOD_NOEXTCODE; else {
                  pgsize = (*mod_secondary->mod_unpack3)(pgsize,src,dst);
                  if (!pgsize) failed=E_MOD_ITERPAGEERR;
               }
               break;
            /* ignore ZEROED and INVALID types, both will be zero-filled.
               INVALID used in Warp 3 kernel */
            case LX_RANGE:
               failed = E_MOD_UNSUPPORTED;
               break;
         }
         if (failed) {
            if (failed==E_MOD_ITERPAGEERR) log_printf("unp err, pg %d\n",idx+ii);
            return failed;
         }
         pt--; idx--;
      }
   }
   // callback exist?
   if ((sflags&MODUNP_OBJLOADED) && strange->objloaded)
      (*strange->objloaded)(mh,object);
#ifdef INITDEBUG
   //log_misc(3, "fixuping %s, obj %d\n", mh->name, object+1);
#endif
   // process fixups
   for (ii=0;ii<oe->o32_mapsize;ii++) {
      u32t *fxpos = (u32t*)((char*)eh+eh->e32_fpagetab)+ii+oe->o32_pagemap-1;
      u8t  *start = (u8t*)((char*)eh+eh->e32_frectab+fxpos[0]),
             *end = (u8t*)((char*)eh+eh->e32_frectab+fxpos[1]),
            *prev = 0, *cstart = start;  // error dump support

      while ((u32t)start<(u32t)end) {
         u8t   src = *start++,
             flags = *start++,
             slist = src & NRCHAIN,
             chfix = flags & NRICHAIN,
             count = slist?*start++:1,
            target = flags & NRRTYP,
            fxcall = (sflags & MODUNP_APPLYFIXUP)!=0;
         s16t* ofs = (s16t*)start;
         u16t ord, fsel = 0;
         u32t fofs = 0,
            chbase = 0;
         mod_object *obj = 0;

         //log_misc(2, "src:%08X flags:%08X count:%d\n", src, flags, count);

         // new merlin fixups can`t be combined with source list
         if (!slist) start+=2; else
         if (chfix) FIXUP_ERR('1');
         // fixup by name or chain fixup with wrong src type
         if (chfix&&(src&NRSTYP)!=NROFF32 || target==NRRNAM)
            FIXUP_SUP('2');

         if (flags & NR16OBJMOD) { ord = *(u16t*)start; start++; }
            else ord = *start;
         start++;

         if (target==NRRINT) {  // internal reference target
            mod_object *obj = &mh->obj[ord-1];
            u8t       stype = src&NRSTYP;

            if (!ord||ord>mh->objects) FIXUP_ERR('3');
            fsel = obj->sel;
            if (stype!=NRSSEG) {
               char big = obj->flags&OBJBIGDEF?1:0;
               if (stype==NRSOFF && big) FIXUP_SUP('4');
               /* object address is not zero on 32-bit offset or
                  32-bit object & 16:32 pointer */
               if (stype>=NROFF32-big)
                  fofs = sflags&MODUNP_ALT_FIXADDR?obj->fixaddr:(u32t)obj->address;
               // target field
               fofs +=flags&NR32BITOFF?*(u32t*)start:*(u16t*)start;
               start+=flags&NR32BITOFF?4:2;
            }
#if 0
            if (sflags)
               log_misc(2,"fixup to obj %d: %d %04X %08X\n",ord,stype,fsel,fofs);
#endif
         } else {               // entry table target
            module     *imh = mh;
            mod_export *imp = 0;
            if (target==NRRORD) { // external entry
               if (ord>MAX_IMPMOD || chfix) FIXUP_ERR('5');
               imh = mh->impmod[ord-1];
               if (!imh) FIXUP_ERR('6');
               ord = *start++;
               if ((flags&NR8BITORD)==0) {
                  ord |= *start++<<8;   // no 32 bit ordinals
                  if (flags&NR32BITOFF) FIXUP_SUP('7');
               }
            }
            if (sflags&MODUNP_FINDEXPORT) {
               imp=(*strange->findexport)(imh, ord);
               if (sflags&MODUNP_FINDEXPORTFX) fxcall = 1;
            } else
               imp = mod_findexport(imh, ord);
            if (!imp) {
               // FIXUP_ORD('8');
               log_printf("no %s.%d\n",imh->name,ord);
               return E_MOD_NOORD;
            }
            fofs = imp->address;
            fsel = imp->sel;
            if (flags&NRADD) {
               if (flags&NR32BITADD) {
                  fofs+=*(u32t*)start; start+=4;
               } else {
                  fofs+=*(u16t*)start; start+=2;
               }
            }
         }
         if (slist) { ofs = (s16t*)start; start+=count*2; }

         // target sel:ofs is ready here
         while (count--) {
            u16t* dst = (u16t*)(destaddr+(ii<<PAGESHIFT)+(s32t)*ofs);

            if (chfix) {
               *ofs = *(u32t*)dst >> 20;
               if (*ofs!=0xFFF) count++;
               if (!chbase) chbase = fofs - (*(u32t*)dst & 0xFFFFF);
               fofs = chbase + (*(u32t*)dst & 0xFFFFF);
               // log_printf("page:%08X next offs %04X target %08X\n", destaddr+(ii<<PAGESHIFT), *ofs, fofs);
            } else
            if (slist) *ofs++;

            // fixup apply callback
            if (fxcall && strange->applyfixup)
               if (!(*strange->applyfixup)(mh,dst,src&NRSTYP,fsel,fofs))
                  continue;
            switch (src&NRSTYP) {
               case NRSBYT  : *(u8t*)dst=fofs; break;
               case NRSSEG  : *dst=fsel; if (!fsel) FIXUP_ERR('9'); break;
               case NRSPTR  : *dst++=fofs; *dst=fsel;
                              if (!fsel) FIXUP_ERR('A');
                              break;
               case NRSOFF  : *dst=fofs; break;
               case NRPTR48 : *(u32t*)dst=fofs; dst+=2; *dst=fsel;
                              if (!fsel) FIXUP_ERR('B');
                              break;
               case NROFF32 : *(u32t*)dst=fofs; break;
               case NRSOFF32: *(u32t*)dst=fofs-(u32t)dst-4; break;
               default:
                  return E_MOD_BADFIXUP;
            }
         }
         prev = cstart; cstart = start;
      }
   }
   // log_misc(2, "obj %d done\n", object+1);
   return 0;
}

// get pointer to module function by index
void *mod_getfuncptr(u32t mh, u32t index) {
   module  *md=(module*)mh;
   if (md->sign==MOD_SIGN) {
      mod_export *exp = mod_findexport(md, index);
      if (exp) return (void*)exp->address;
   }
   return 0;
}

// the same, but direct
void *mod_apidirect(u32t mh, u32t index) {
   module  *md=(module*)mh;
   if (md->sign==MOD_SIGN) {
      mod_export *exp = mod_findexport(md, index);
      if (exp) return (void*)exp->direct;
   }
   return 0;
}

static u32t env_length(const char *env) {
   u32t   total=0;
   char    *pos=(char*)env;
   do {
      u32t len=strlen(pos);
      total+=++len; pos+=len;
   } while (*pos);
   return total+1;
}

/* run module. return -1 if failed (invalid handle, etc)
   env\0
   env\0\0
   module path\0
   params:
   module path\0
   arguments string\0\0 */
s32t mod_exec(u32t mh, const char *env, const char *params) {
   u32t      envlen, parmlen,
         pathlen, rc, env_sz,
                   alloc_len;
   char    *envseg, *cmdline;
   process_context   *newctx;
   module                *md = (module*)mh;

   if (md->sign!=MOD_SIGN) return -1;
   if (md->flags&MOD_LIBRARY) return -1;
   if (md->flags&MOD_EXECPROC) return -1;

   envlen    = env?env_length(env):0;
   parmlen   = params?strlen(params)+1:0;
   pathlen   = strlen(md->mod_path)+1;
   /* envinonment allocation.
      hlp_memalloc(r) is used for the first launch only */
   env_sz    = envlen + 2 + parmlen + 2 + pathlen*2;
   alloc_len = env_sz +sizeof(process_context);
   envseg    = mod_secondary?mod_secondary->mem_alloc(QSMEMOWNER_MODLDR, mh, alloc_len):
               hlp_memallocsig(alloc_len, "MCtx", QSMA_NOCLEAR|QSMA_READONLY);
   // zero it all
   memset(envseg, 0, alloc_len);
   // here we entering critical part
   mt_swlock();
   // fill process context data
   newctx = (process_context*)(envseg + env_sz);
   newctx->size   = sizeof(process_context);
   newctx->pid    = lastusedpid++;
   newctx->envptr = envseg;
   newctx->self   = md;
   newctx->pctx   = mt_exechooks.mtcb_ctxmem;
   newctx->parent = newctx->pctx?newctx->pctx->self:0;
   newctx->flags  = mod_secondary?0:PCTX_BIGMEM;

   // copying env/cmdline data
   cmdline  = envseg;
   if (envlen) memcpy(cmdline, env, envlen); else envlen = 2;
   cmdline += envlen;
   memcpy(cmdline, md->mod_path, pathlen);
   cmdline += pathlen;
   memcpy(cmdline, md->mod_path, pathlen);
   if (parmlen) memcpy(cmdline+pathlen, params, parmlen);
   // command line pointer
   newctx->cmdline = cmdline;

   // allocates process data
   newctx->rtbuf[RTBUF_PROCDAT] = (u32t)mt_exechooks.mtcb_init(newctx);
   // flag we`re running
   md->flags |= MOD_EXECPROC;

   // leaving critical part
   mt_swunlock();
   rc = 0;
   if (md!=mod_self) {
      /* callback.
         note, that callbacks are called in caller context! */
      rc = mod_secondary->start_cb(newctx);
      // launch it if was not denied by callback
      if (!rc) {
         log_it(LOG_HIGH,"exec %s\n",md->mod_path);
         /* if we have MT exec - it process context switching for us, we just
            calls it here it sleeps until exit.
            In non-MT mode - just swap current context here. */
         if (mt_exechooks.mtcb_exec) rc = mt_exechooks.mtcb_exec(newctx); else {
            mt_exechooks.mtcb_ctxmem = newctx;
            mt_exechooks.mtcb_ctid   = 1;
            rc = launch32(md, (u32t)envseg, (u32t)cmdline);
            // restore context only if still no MT mode!
            if (!mt_exechooks.mtcb_exec)
               mt_exechooks.mtcb_ctxmem = newctx->pctx;
         }
         rc = mod_secondary->exit_cb(newctx, rc);
      }
      if (mt_exechooks.mtcb_fini(newctx)) {
         // free original env. segment
         mod_secondary->mem_free(envseg);
         // free reallocated env. data
         if (newctx->flags&PCTX_ENVCHANGED)
            if (newctx->envptr!=envseg)
               if (newctx->flags&PCTX_BIGMEM) hlp_memfree(newctx->envptr); else
                  mod_secondary->mem_free(newctx->envptr);
      }
      mt_safedand(&md->flags, ~MOD_EXECPROC);
   } else {
      /* START module launching.
         This is, actually, NOT DLL init (it already called by mod_load),
         but ptr to START.189 - main function in START.
         Called via dll32init() to use its simple save/restore features. */
      mt_exechooks.mtcb_ctxmem = newctx;
      dll32init(md,0);
      //log_printf("warning! \"start\" module exited!\n");
   }
   return rc;
}

// free and unload module (decrement usage)
u32t mod_free(u32t mh) {
   process_context *pq = mod_context();
   module          *md = (module*)mh;
   u32t             oi;

   if (md->sign!=MOD_SIGN) return E_MOD_HANDLE;
   // do not allow to free self while running
   if (pq && pq->self == md) return E_MOD_FSELF;
   // block main modules final free
   if ((md==mod_start || md==mod_self || (md->flags&MOD_SYSTEM)) && md->usage==1)
      return E_MOD_FSYSTEM;
   // dec usage
   if (--md->usage>0) return 0;
   log_printf("unload %s\n",md->mod_path);
   // is it running check for EXE and call term function for DLL
   if ((md->flags&MOD_LIBRARY)==0) {
      if ((md->flags&MOD_EXECPROC)) {
         log_printf("\"%s\" is running!\n",md->mod_path);
         md->usage++;
         return E_MOD_EXECINPROC;
      }
   } else
      if (mod_initterm(md,1)==0) {
         log_printf("\"%s\" deny unload\n",md->mod_path);
         md->usage++;
         return E_MOD_LIBTERM;
      }
   // free all used modules
   mod_unloadimports(md);
   // free export table (common unload callback to START)
   if (mod_secondary) (*mod_secondary->mod_freeexps)(md);
   // unlink self from lists
   mod_listdel(md->flags&MOD_LOADING?&mod_ilist:&mod_list, md);
   /* free selectors (zero values will be ignored) and fill module memory
      with 0xCC (code objects) and 0 (data objects). This increasing a chance
      to catch forgotten references to it, at least until this memory will be
      reused */
   for (oi=0; oi<md->objects; oi++) {
      mod_object *obj = md->obj + oi;
      hlp_selfree(obj->sel);
      memset(obj->address, obj->flags&OBJEXEC?0xCC:0x00, obj->size);
   }
   // free module header(md itself) + module objects
   md->sign=0;
   hlp_memfree(md->baseaddr);
   return 0;
}

// build export table (boot time only, for START module)
int _std mod_buildexps(module *mh, lx_exe_t *eh) {
   lx_exp_b    *ee = (lx_exp_b*)((u8t*)eh+eh->e32_enttab);
   u32t        idx = 1, cnt=0;
   mod_export *exp = mh->exps;
   u8t     *thunks = mh->thunks;

   while (ee->b32_cnt) {
      u32t bsize=0;
      switch (ee->b32_type&~TYPEINFO) {
         case EMPTY   : idx+=ee->b32_cnt; bsize=2; break;
         case ENTRY32 : {
               u8t *entry = (u8t*)ee + (bsize=sizeof(lx_exp_b));
               while (ee->b32_cnt--) {
                  mod_object *obj = mh->obj + (ee->b32_obj-1);
                  auto  u32t addr = *(u32t*)++entry + (u32t)obj->address;
                  exp[cnt].direct = addr;
                  // create thunk for code object only
                  if (obj->flags & OBJEXEC) {
                     u8t* thc         = thunks;
                     exp[cnt].address = (u32t)thc;
                     *thc++           = 0xE9;
                     *(u32t*)thc      = addr - ((u32t)thc+4);
                     thunks          += EXPORT_THUNK;
                  } else
                     exp[cnt].address = addr;
                  //
                  entry+=4; bsize+=5;
                  exp[cnt].ordinal = idx++;
                  if (++cnt==MAX_EXPSTART) return E_MOD_EXPLIMIT;
               }
            }
            break;
         default:
            return E_MOD_UNSUPPORTED;
      }
      ee=(lx_exp_b*)((u8t*)ee+bsize);
   }
   mh->exports = cnt;
   return 0;
}

int start_it() {
   mod_export *stmain;
   u32t           err;
   /* loading START module (this is DLL now, not EXE like it was until rev.287)
      file also will be deleted ramdisk to guarantee some free space */
   mod_start = (module*)mod_load(MODNAME_START, LDM_UNLINKFILE, &err, 0);
   if (!mod_start) return err;
   // mark it as "system"
   mod_start->flags |= MOD_SYSTEM;
   // query "main" address
   stmain    = mod_findexport(mod_start, 189);
   if (!stmain) return E_MOD_NOORD;
   // make sure mod_self is ready
   if (!mod_self) mod_query(MODNAME_QSINIT, 0);
   // REPLACE init function to use it in mod_exec()
   mod_self->start_ptr = stmain->direct;
   /* some kind of hack - simulate "QSINIT" launch to create 1st process
      context (mod_exec() knows what to do in this case) */
   mod_exec((u32t)mod_self, 0, 0);
   return 0;
}
