#include "..\..\..\hc\qslxfmt.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "sp_defs.h"

FILE *mfile = 0;
void *mdata = 0;
void *rcmem = 0;

static void error(const char *fmt, ...) {
   va_list arglist;
   va_start(arglist,fmt);
   vprintf(fmt,arglist);
   va_end(arglist);
   if (mfile) fclose(mfile);
   if (mdata) free(mdata);
   if (rcmem) free(rcmem);
   exit(1);
}

void *read_lxobject(const char *fname, u32t object, u32t rdofs, u32t &rdsize) {
   rcmem  = 0;
   mfile  = fopen(fname,"rb");
   if (!mfile) error("No LX module file \"%s\"!\n",fname);
   d size = fsize(mfile);
   if (!size) error("Empty module file!\n");
   mdata  = malloc(size);
   if (!mdata) error("Out of memory!\n");
   if (fread(mdata,1,size,mfile)!=size) error("Error reading module!\n");
   fclose(mfile);

   mz_exe_t          *mz = (mz_exe_t*)mdata;
   u32t      oldhdr_size = mz->e_magic!=EMAGIC?0:mz->e_lfanew;
   register lx_exe_t *eh = (lx_exe_t*)((u8t*)mdata+oldhdr_size);
   lx_obj_t          *ot = (lx_obj_t*)((u8t*)eh+eh->e32_objtab);

   // invalid signature
   if (eh->e32_magic!=LXMAGIC) error("LX module format required!\n");
   // invalid constant fields
   if (eh->e32_border||eh->e32_worder||eh->e32_level||
      eh->e32_pagesize!=PAGESIZE)
         error("Wrong LX format contstants!\n");
   // module with errors or pre-upplied fixups or pdd/vdd
   if (eh->e32_mflags&E32NOLOAD)
      error("Module marked as non-loadable!\n");
   // module without executable code
   if (!eh->e32_mpages||!eh->e32_startobj||!eh->e32_objmap||!eh->e32_restab||
      !eh->e32_objcnt||!eh->e32_objtab||eh->e32_startobj>eh->e32_objcnt)
         error("Module without executable code!\n");
   // broken file
   if (size-oldhdr_size-eh->e32_objcnt*sizeof(lx_obj_t)<eh->e32_objtab)
      error("Broken object table!\n");

   if (size-oldhdr_size-(eh->e32_magic==LXMAGIC?sizeof(lx_map_t):
      sizeof(le_map_t))*eh->e32_mpages<eh->e32_objmap)
         error("Broken page table!\n");

   if (object>eh->e32_objcnt)
      error("No object %d in LX file!\n", object);

   u32t flags=ot[--object].o32_flags;

   if (flags&(OBJINVALID|OBJRSRC)||!ot[object].o32_size) {
      error("Unsupported object type!\n");
   } else {
      lx_obj_t  *oe = ot+object;
      u32t    pages = oe->o32_mapsize,
                idx = oe->o32_pagemap-1, p1st, plast;
      lx_map_t  *pt = (lx_map_t*)((char*)eh + eh->e32_objmap);

      if ((idx+=pages-1)>eh->e32_mpages)
         error("Invalid page entries in object %d!\n", object+1);

      if (rdofs>=oe->o32_size || rdofs+rdsize>oe->o32_size)
         error("Reading past the end of object!\n");

      p1st = rdofs>>PAGESHIFT;
      if (!rdsize) rdsize=oe->o32_size-rdofs;
      plast = rdofs+rdsize>>PAGESHIFT;

      rcmem = malloc(Round4(rdsize));
      memset(rcmem, 0, Round4(rdsize));

      pt+=idx;
      while (pages--) {
         u32t pgsize = pt->o32_pagesize,
             poffset = pages<<PAGESHIFT;
         u8t    *src = (u8t*)mz+eh->e32_datapage+
                       (pt->o32_pagedataoffset<<eh->e32_pageshift);

         if (pages>=p1st && pages<=plast) {
            if (pt->o32_pageflags!=LX_VALID && pt->o32_pageflags!=LX_ZEROED)
               error("Compressed page %u in object %d!\n", pages+1, object+1);
            if (pt->o32_pageflags==LX_ZEROED) {
               printf("Warning! Zeroed page %u as data source!\n", pages+1);
            } else {
               if (poffset<rdofs) {
                  u32t ncsize = rdofs-poffset;
                  // copying only present data
                  if (ncsize<pgsize) {
                     u32t cpy = pgsize-ncsize;
                     if (cpy>rdsize) cpy = rdsize;
                     memcpy(rcmem, src+ncsize, cpy);
                  }
               } else {
                  u32t epos = rdofs+rdsize,
                      stpos = poffset-rdofs;
                  if (epos>poffset+pgsize)
                     memcpy((char*)rcmem+stpos, src, pgsize);
                  else
                     memcpy((char*)rcmem+stpos, src, epos-poffset);
               }
            }
         }
         pt--; idx--;
      }
      return rcmem;
   }
   return 0;
}
