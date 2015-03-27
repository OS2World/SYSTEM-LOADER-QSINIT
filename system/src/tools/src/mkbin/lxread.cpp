//
// QSINIT
// lx -> binary converter
// ------------------------------------------------------------------
//
// This code gets LX DLL with 1-3 objects:
//  * 0/1 16-bit object with plain code for real mode (like COM file)
//  * 1/2 32-bit objects with PM code (and fixups between all of them)
//
// and convert it into plain binary with stored 32-bit fixups, which can
// be loaded by boot sector or micro-FSD code.
// Unpacking and setup of 32-bit object must be done by 16-bit RM object.
//
// Unfortunately, this method required because some of Intel modern
// CPUs does not like non-zero based FLAT objects (Intel Atom works NINE
// times slower on stack segment!).
// Non-zero based FLAT was a previous QSINIT memory model.
//
// Another way to use this binary - creating 32 bit part of mixed 32/64
// EFI executable (launching 32-bit QSINIT on 64-bit EFI host).
//

#include "qslxfmt.h"
#include "qsbinfmt.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "sp_defs.h"
#include "classes.hpp"

FILE    *mfile = 0;
void    *mdata = 0;
void    *mem16 = 0,
        *mem32 = 0;

IMPLEMENT_FAKE_COMPARATION(rels_info)
IMPLEMENT_FAKE_COMPARATION(relo_info)
IMPLEMENT_FAKE_COMPARATION(relr_info)

typedef List<rels_info> TrsList;
typedef List<relo_info> TroList;
typedef List<relr_info> TrrList;

TrsList  *rels = 0;   // selector fixups
TroList  *relo = 0;   // 32-bit offset fixups
TrrList  *relr = 0;   // 32-bit relative fixups

extern "C"
l Compress(l Length, b *Source, b *Destin);

void error(const char *fmt, ...) {
   va_list arglist;
   va_start(arglist,fmt);
   vprintf(fmt,arglist);
   va_end(arglist);
   if (mfile) fclose(mfile);
   if (mdata) free(mdata);
   if (mem16) free(mem16);
   if (mem32) free(mem32);
   if (rels) delete rels;
   if (relo) delete relo;
   if (relr) delete relr;
   exit(1);
}

extern int        verbose,
                  warnlvl;
static mz_exe_t       *mz = 0;
static u32t   oldhdr_size = 0;
static lx_exe_t       *eh = 0;
static lx_obj_t       *ot = 0;

#define FIXUP_MSG(msg) \
   error("fixup error (%s): object %d page %d\n", msg, obj+1, ii+1)
#define FIXUP_ERR(msg) \
   error("fixup error (%s): object %d page %d offset 0x%0X\n", msg, obj+1, ii+1, eofs)
#define FIXUP_WARN(msg) \
   printf("warning (%s): object %d page %d offset 0x%0X\n", msg, obj+1, ii+1, eofs)

void read_object(MKBIN_HEADER &bh, int obj, u32t offset, int savebss, u8t *destaddr) {
   lx_obj_t  *oe = ot+obj;
   u32t      pgc = oe->o32_mapsize,
             idx = oe->o32_pagemap-1 + pgc-1,
           flags = oe->o32_flags, ii;
   lx_map_t  *pt = (lx_map_t*)((char*)eh + eh->e32_objmap);
   int      is16 = (flags&OBJBIGDEF)==0,
        isdata32 = !is16 && (flags&OBJEXEC)==0;

   pt += idx;
   // do not store BSS space?
   if (pgc && (is16 || isdata32) && !savebss)
      if (is16)     bh.rmStored = (pgc-1 << PAGESHIFT) + pt->o32_pagesize; else
      if (isdata32) bh.pmStored = offset + (pgc-1 << PAGESHIFT) + pt->o32_pagesize;

   while (pgc) {
      u32t pgsize = pt->o32_pagesize,
            pgofs = eh->e32_datapage+ (pt->o32_pagedataoffset<<eh->e32_pageshift);
      u8t    *dst = (is16?(u8t*)mem16:(u8t*)mem32+offset) + (--pgc<<PAGESHIFT),
             *src = (u8t*)eh + pgofs;

      if (verbose) printf("obj %d page %d sz %4d fl %04X ofs %08X\n",
         obj+1, pgc, pgsize, pt->o32_pageflags, pgofs);

      if (pt->o32_pageflags==LX_VALID) memcpy(dst,src,pgsize);
      pt--;
   }
   // process fixups
   for (ii=0; ii<oe->o32_mapsize; ii++) {
      u32t *fxpos = (u32t*)((char*)eh+eh->e32_fpagetab)+ii+oe->o32_pagemap-1;
      u8t  *start = (u8t*)((char*)eh+eh->e32_frectab+fxpos[0]),
             *end = (u8t*)((char*)eh+eh->e32_frectab+fxpos[1]);

      while ((u32t)start<(u32t)end) {
         u8t   src = *start++,
             flags = *start++,
             slist = src & NRCHAIN,
             chfix = flags & NRICHAIN,
             count = slist?*start++:1,
            target = flags & NRRTYP;
         s16t* ofs = (s16t*)start;
         u16t ord, fsel = 0;
         u32t fofs = 0,
            chbase = 0;

         if (verbose>1) printf("src:%08X flags:%08X count:%d\n", src, flags, count);
         // we get internal fixups only
         if (target!=NRRINT) FIXUP_MSG("external fixup present");
         // new merlin fixups can`t be combined with source list
         if (!slist) start+=2; else
         if (chfix) FIXUP_MSG("source list with chained");
         // chain fixup with wrong src type
         if (chfix&&(src&NRSTYP)!=NROFF32) FIXUP_MSG("wrong src for chain fixup");

         if (flags & NR16OBJMOD) { ord = *(u16t*)start; start++; }
            else ord = *start;
         start++;

         if (target==NRRINT) {  // internal reference target
            lx_obj_t   *obi = ot + (ord-1);
            u8t       stype = src&NRSTYP;
            u32t     iflags = obi->o32_flags;
            char        big = iflags&OBJBIGDEF ? 1 : 0;

            if (!ord||ord>eh->e32_objcnt) FIXUP_MSG("invalid target object");
            fsel = big ? (iflags&OBJEXEC ? FXSEL32CS : FXSEL32DS) : FXSEL16;

            if (stype!=NRSSEG) {
               /* object address is not zero on 32-bit offset or
                  32-bit object & 16:32 pointer */
               if (stype>=NROFF32-big) fofs = offset;
               // target field
               fofs +=flags&NR32BITOFF?*(u32t*)start:*(u16t*)start;
               start+=flags&NR32BITOFF?4:2;
            }
            if (verbose>1)
               printf("fixup to obj %d: %d %04X %08X\n", ord+1, stype, fsel, fofs);
         }
         if (slist) { ofs = (s16t*)start; start+=count*2; }

         // target sel:ofs is ready here
         while (count--) {
            s32t  eofs = *ofs;
            u16t*  dst = (u16t*)(destaddr+(ii<<PAGESHIFT)+eofs);
            u32t  dofs = (ii<<PAGESHIFT) + eofs + offset;

            if (chfix) {
               *ofs = *(u32t*)dst >> 20;
               if (*ofs!=0xFFF) count++;
               if (!chbase) chbase = fofs - (*(u32t*)dst & 0xFFFFF);
               fofs = chbase + (*(u32t*)dst & 0xFFFFF);
               if (verbose>2) 
                  printf("page:%08X next offs %04X target %08X\n", destaddr+(ii<<PAGESHIFT), *ofs, fofs);
            } else
            if (slist) *ofs++;

            switch (src&NRSTYP) {
               case NRSBYT  :
                  FIXUP_ERR("byte fixup!");
                  break;
               case NRSSEG  : {
                  if (is16) FIXUP_ERR("selector fixup in RM object!");
                  if (fsel==FXSEL16) FIXUP_ERR("fixup to RM object selector!"); 
                  rels_info rel;
                  rel.rs_offset  = dofs;
                  rel.rs_seltype = fsel;
                  rels->Add(rel);
                  break;
               }
               case NRSPTR  :
                  FIXUP_ERR("16-bit far pointer fixup!");
                  break;
               case NRSOFF  : 
                  if (fsel!=FXSEL16) FIXUP_ERR("16-bit fixup to 32-bit object");
                  if (!is16 && warnlvl) FIXUP_WARN("16-bit fixup in 32-bit object");
                  // apply 16-bit offset (to RM object only, 32-bit was filtered above)
                  *dst=fofs; 
                  break;
               case NRPTR48 : {
                  if (is16) FIXUP_ERR("16:32 fixup in RM object!");
                  if (fsel==FXSEL16) FIXUP_ERR("16:32 fixup to RM object!"); 
                  relo_info rel;
                  rel.ro_offset  = dofs;
                  rel.ro_target  = fofs;
                  rel.ro_tgt16   = 0;
                  relo->Add(rel);
                  rels_info rls;
                  rls.rs_offset  = dofs + 4;
                  rls.rs_seltype = fsel;
                  rels->Add(rls);
                  break;
               }
               case NROFF32 : if (is16) FIXUP_ERR("32-bit offset in RM object!"); else
               {
                  relo_info rel;
                  rel.ro_offset  = dofs;
                  rel.ro_target  = fofs;
                  rel.ro_tgt16   = fsel==FXSEL16 ? 1 : 0;
                  relo->Add(rel);
                  /* actually this is NOT an error, but can be used to check
                     assembler/linker results */
                  if (fsel==FXSEL16 && warnlvl>1) FIXUP_WARN("32-bit fixup to 16-bit object");
                  break;
               }
               case NRSOFF32: 
                  /* apply fixups between 32-bit objects, but save one for
                     fixup from 32 to 16 */
                  if (is16) FIXUP_ERR("relative 32-bit fixup in RM object!"); else {
                     if (fsel==FXSEL16) {
                        relr_info rel;
                        rel.rr_offset  = dofs;
                        rel.rr_target  = fofs;
                        relr->Add(rel);
                     } else
                        *(u32t*)dst=fofs-dofs-4;
                  }
                  break;
               default:
                  error("bad fixup");
            }
         }
      }
   }
}

static int rels_compare(const void *b1, const void *b2) {
   rels_info *rp1 = (rels_info*)b1,
             *rp2 = (rels_info*)b2;
   if (rp1->rs_seltype == rp2->rs_seltype) return 0;
   if (rp1->rs_seltype > rp2->rs_seltype) return 1;
   return -1;
}

static int relo_compare(const void *b1, const void *b2) {
   relo_info *rp1 = (relo_info*)b1,
             *rp2 = (relo_info*)b2;
   if (rp1->ro_tgt16 > rp2->ro_tgt16) return 1;
   if (rp1->ro_tgt16 < rp2->ro_tgt16) return -1;
   if (rp1->ro_target == rp2->ro_target) return 0;
   if (rp1->ro_target > rp2->ro_target) return 1;
   return -1;
}

static int relr_compare(const void *b1, const void *b2) {
   relr_info *rp1 = (relr_info*)b1,
             *rp2 = (relr_info*)b2;
   if (rp1->rr_target == rp2->rr_target) return 0;
   if (rp1->rr_target > rp2->rr_target) return 1;
   return -1;
}

// find ready header in 16-bit object
static u32t find_header(u32t len) {
   if (!mem16) return 0;
   u8t *pos = (u8t*)mem16;
   while (pos - (u8t*)mem16 < len - sizeof(MKBIN_HEADER)) {
      pos = (u8t*)memchr(pos, MKBIN_SIGN&0xFF, len - sizeof(MKBIN_HEADER) - (pos - (u8t*)mem16));
      if (!pos) return 0;

      MKBIN_HEADER *hc = (MKBIN_HEADER*)pos;
      if (hc->mkbSign==MKBIN_SIGN && hc->mkbSize==sizeof(MKBIN_HEADER)) {
         if (!hc->rmStored) error("Header found, but BSS position in it is zero!\n");
         return pos - (u8t*)mem16;
      }
      pos++;
   }
   return 0;
}

void read_lx(const char *fname, const char *outname, int bss16, int pack, int ehdr) {
   mfile  = fopen(fname,"rb");
   if (!mfile) error("No LX module file \"%s\"!\n",fname);
   d size = fsize(mfile);
   if (!size) error("Empty module file!\n");
   mdata  = malloc(size);
   if (!mdata) error("Out of memory!\n");
   if (fread(mdata,1,size,mfile)!=size) error("Error reading module!\n");
   fclose(mfile);

   mz = (mz_exe_t*)mdata;
   oldhdr_size = mz->e_magic!=EMAGIC?0:mz->e_lfanew;
   eh = (lx_exe_t*)((u8t*)mdata+oldhdr_size);
   ot = (lx_obj_t*)((u8t*)eh+eh->e32_objtab);

   // invalid signature
   if (eh->e32_magic!=LXMAGIC) error("LX module format required!\n");
   // invalid constant fields
   if (eh->e32_border||eh->e32_worder||eh->e32_level||eh->e32_pagesize!=PAGESIZE)
      error("Wrong LX format contstants!\n");
   // module with errors
   if (eh->e32_mflags&E32NOLOAD) error("Module marked as non-loadable!\n");
   // module without executable code
   if (!eh->e32_mpages||!eh->e32_objmap||!eh->e32_restab||!eh->e32_objcnt||
      !eh->e32_objtab||eh->e32_startobj>eh->e32_objcnt)
         error("Module without executable code!\n");
   // import table is not empty
   if (eh->e32_impmodcnt>0) error("Imports present in module!\n");
   // broken file
   if (size-oldhdr_size-eh->e32_objcnt*sizeof(lx_obj_t)<eh->e32_objtab)
      error("Broken object table!\n");
   if (size-oldhdr_size-(eh->e32_magic==LXMAGIC?sizeof(lx_map_t):
      sizeof(le_map_t))*eh->e32_mpages<eh->e32_objmap)
         error("Broken page table!\n");
   // pre-applied fixups
   if ((eh->e32_mflags&E32NOINTFIX)!=0)
      error("There is no fixup information in module!\n");
   // not DLL (if we allow EXE - linker can annoy us with stack searching/adding)
   if ((eh->e32_mflags&E32MODMASK)!=E32MODDLL)
      error("Module type if not DLL!\n");
   // modify offsets to LX header start
   eh->e32_datapage -= oldhdr_size;

   u32t ll;
   int  idx16 = -1,
       code32 = -1,
       data32 = -1;

   for (ll=0; ll<eh->e32_objcnt; ll++) {
      u32t flags=ot[ll].o32_flags;

      if (flags&(OBJINVALID|OBJRSRC)||!ot[ll].o32_size) {
         error("Unsupported type of object #%d!\n", ll+1);
      } else {
         lx_obj_t  *oe = ot+ll;
         u32t    pages = oe->o32_mapsize,
                   idx = oe->o32_pagemap-1;
         lx_map_t  *pt = (lx_map_t*)((char*)eh + eh->e32_objmap);
      
         if ((idx+=pages-1)>eh->e32_mpages)
            error("Invalid page entries in object %d!\n", ll+1);

         if ((flags&OBJBIGDEF)!=0) {
            if ((flags&OBJEXEC)!=0) {
               if (code32<0) code32 = ll; else
                  error("Object %d is second 32-bit code object!\n", ll+1);
            } else {
               if (data32<0) data32 = ll; else
                  error("Object %d is second 32-bit data object!\n", ll+1);
            }
         } else {
            if (idx16<0) idx16 = ll; else
               error("Object %d is second 16-bit object!\n", ll+1);
         }
         pt+=idx;
         while (pages--) {
            if (pt->o32_pageflags!=LX_VALID && pt->o32_pageflags!=LX_ZEROED)
               error("Compressed page %u in object %d!\n", pages+1, ll+1);
            pt--;
         }
      }
   }
   if (data32<0) error("At least one writable 32-bit object must exist!\n");
   if (eh->e32_startobj && eh->e32_startobj-1 == idx16)
      error("Entry point in 16-bt object!\n");

   MKBIN_HEADER  bh, *rh = 0;
   memset(&bh, 0, sizeof(bh));
   bh.mkbSign = MKBIN_SIGN;
   bh.mkbSize = sizeof(MKBIN_HEADER);

   if (idx16>=0) {
      bh.rmStored = bh.rmSize = ot[idx16].o32_size;

      mem16 = malloc(Round4k(bh.rmSize));
      memset(mem16, 0, Round4k(bh.rmSize));
   }
   if (code32>=0) bh.pmCodeLen = Round4k(ot[code32].o32_size);
   bh.pmStored = bh.pmSize = bh.pmCodeLen + ot[data32].o32_size;

   mem32 = malloc(Round4k(bh.pmSize));
   memset(mem32, 0, Round4k(bh.pmSize));

   rels = new TrsList();
   relo = new TroList();
   relr = new TrrList();

   if (idx16 >=0) {
      read_object(bh, idx16, 0, bss16, (u8t*)mem16);
      if (ehdr) {
         u16t hdrofs = find_header(bh.rmStored);
         if (hdrofs) {
            rh = (MKBIN_HEADER*)((u8t*)mem16 + hdrofs);
            if (rh->pmStored<bh.rmStored && !bss16) bh.rmStored = rh->rmStored;
            if (rh->pmStored>bh.rmStored)
               error("Invalid 16-bit BSS position value (%X)!\n", rh->pmStored);
         } else
            error("Unable to find header position!\n");
      }
   }

   if (code32>=0) read_object(bh, code32, 0, 1, (u8t*)mem32);
   read_object(bh, data32, bh.pmCodeLen, 0, (u8t*)mem32 + bh.pmCodeLen);
   // entry point address
   if (eh->e32_startobj) {
      bh.pmEntry  = eh->e32_startobj-1 == data32 ? bh.pmCodeLen : 0;
      bh.pmEntry += eh->e32_eip;
   }
   // sort fixup lists
   qsort((void*)rels->Value(), bh.fxSelCnt = rels->Count(), sizeof(rels_info), rels_compare);
   qsort((void*)relo->Value(), bh.fxOfsCnt = relo->Count(), sizeof(relo_info), relo_compare);
   qsort((void*)relr->Value(), bh.fxRelCnt = relr->Count(), sizeof(relr_info), relr_compare);
   
   if (pack && bh.pmStored<1024) pack = 0;

   bh.pmTotal = bh.pmStored + bh.fxSelCnt*sizeof(rels_info) +
                bh.fxOfsCnt*sizeof(relo_info) + bh.fxRelCnt*sizeof(relr_info);
   u8t    *in = (u8t*)malloc(bh.pmTotal), *inp;

   memcpy(in, mem32, bh.pmStored);
   inp  = in + bh.pmStored;
   memcpy(inp, rels->Value(), bh.fxSelCnt*sizeof(rels_info));
   inp += bh.fxSelCnt*sizeof(rels_info);
   memcpy(inp, relo->Value(), bh.fxOfsCnt*sizeof(relo_info));
   inp += bh.fxOfsCnt*sizeof(relo_info);
   memcpy(inp, relr->Value(), bh.fxRelCnt*sizeof(relr_info));
   free(mem32);

   mem32 = in;
   if (verbose) {
      printf("16-bit size: %d bytes (%X, +BSS %X)\n", bh.rmStored, bh.rmStored, bh.rmSize);
      printf("32-bit size: %d bytes (%X, +BSS %X)\n", bh.pmStored, bh.pmStored, bh.pmSize);
      printf("fixups size: %d bytes (%d,%d,%d)\n", bh.pmTotal - bh.pmStored, 
         bh.fxSelCnt, bh.fxOfsCnt, bh.fxRelCnt);
   }

   if (pack) {
      u8t *out = (u8t*)malloc(bh.pmTotal * 2);

      bh.pmPacked = Compress(bh.pmTotal, in, out);
      if (bh.pmPacked) {
         mem32 = out; 
         free(in);
      } else {
         free(out);
         pack  = 0;
      }
   }
   if (!pack) bh.pmPacked = bh.pmTotal;

   mfile  = fopen(outname, "wb");
   if (!mfile) error("Unable to create file \"%s\"!\n",outname);
   // embedded header present?
   if (rh) memcpy(rh, &bh, sizeof(bh));
   // write 16bit part
   if (mem16) {
      if (fwrite(mem16,1,bh.rmStored,mfile)!=bh.rmStored) error("File write error!\n");
      free(mem16); mem16 = 0;
   }
   // write header if no embedded one
   if (!rh) 
      if (fwrite(&bh,1,sizeof(bh),mfile)!=sizeof(bh)) error("File write error!\n");
   // write 32bit part & fixups
   if (fwrite(mem32,1,bh.pmPacked,mfile)!=bh.pmPacked) error("File write error!\n");
   fclose(mfile);
   free(mem32);
   if (rels) delete rels;
   if (relo) delete relo;
   if (relr) delete relr;
}
