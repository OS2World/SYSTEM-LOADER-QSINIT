//
// QSINIT
// fill memory info
// ----------------------------------------------------------------
// this is 16-bit RM code!
// ----------------------------------------------------------------
// only general process is here: parse of 1st 10 E820 entries or old
// int15 88/E8 output, sort it and write to initial physmem array.
//
#include "qstypes.h"
#include "qsint.h"
#include "qsinit.h"
#include "filetab.h"
#include "qsbinfmt.h"

void* __stdcall memmove16(void *dst, const void *src, u16t length);

// int 15h E820h function memory table
#define ACPIMEM_TABLE_SIZE  10
#define FINDERARRAY_CNT    128
extern u16t         Int1588Mem;                       // memory: int 15h ah=88h
extern u16t         Int15E8Mem;                       // memory: int 15h ax=E801h
extern AcpiMemInfo AcpiInfoBuf[ACPIMEM_TABLE_SIZE];   // memory: int 15h ax=E820h
extern u16t         AcpiMemCnt;
extern physmem_block   physmem[PHYSMEM_TABLE_SIZE];
extern u16t    physmem_entries;
extern u16t          int12size;     // int 12 value
extern 
struct filetable_s   filetable;
extern MKBIN_HEADER bin_header;
extern u32t          memblocks;     // number of 64k blocks
extern u32t           availmem;     // total avail memory (above 16M, including those arrays)
extern u32t          phmembase;     // physical address of used memory (16Mb or larger on PXE)
extern u32t           highbase,     // base of 32bit object
                     highstack;     // end of 32bit object stack (initial esp)
extern u8t        dd_bootflags;     // boot flags
extern u32t        minifsd_ptr;     // buffer for mini-FSD
extern u32t          unpbuffer;
extern u32t          stacksize;

typedef struct {
   u32t              startaddr;     // start address of physical memory block
   u32t                endaddr;     // end address of physical memory block
} BlockInfo;

typedef struct {
   u32t                   addr;     // address itself
   u8t                   start;     // start or end address
} SortInfo;

void hlp_basemem(void) {
   u32t   fcount, scount, scnew, ccount, ii, jj;
   u8t      tmpf;
   // used stack size ~2400!!
   BlockInfo  ma[FINDERARRAY_CNT];
   SortInfo   sa[FINDERARRAY_CNT * 2];
   physmem_block  *ppm = physmem;

   physmem_entries = 0;       // zero counter
   // 640KB
   fcount = 0;
   ma[fcount].startaddr = 0;
   ma[fcount].endaddr = ((u32t)int12size<<10) - 1;
   // parse ACPI memory table
   if (AcpiMemCnt) {
      for (ii=0; ii<AcpiMemCnt; ii++) {
         AcpiMemInfo *ci = AcpiInfoBuf + ii;
         if (ci->BaseAddrHigh) continue;        // ignore memory above 4Gb
         if (ci->BaseAddrLow<_1MB) continue;    // and below 1Mb
         // truncate too large block
         if (ci->LengthHigh) {
            ci->LengthLow = _4GB - PAGESIZE;
            ci->LengthHigh= 0;
         }
         // ignore too small blocks
         if (ci->LengthLow<_256KB) continue;
         // block overflow 4Gb
         if (ci->BaseAddrLow+ci->LengthLow<ci->BaseAddrLow)
            ci->LengthLow = _4GB - PAGESIZE - ci->BaseAddrLow;

         ma[++fcount].startaddr = ci->BaseAddrLow;
         ma[  fcount].endaddr   = ci->BaseAddrLow + ci->LengthLow - 1;
      }
   } else {
      // memory between 1Mb to 64Mb
      if (Int1588Mem) {
         ma[++fcount].startaddr = _1MB;
         ma[  fcount].endaddr   = ((u32t)Int1588Mem<<10) + _1MB - 1;
      }
      // another old way to find memory between 16Mb to 64Mb
      if (Int15E8Mem) {
         ma[++fcount].startaddr = _16MB;
         ma[  fcount].endaddr   = ((u32t)Int15E8Mem<<16) + _16MB - 1;
      }
   }
   // sort and eliminate duplications
   ii     = 0; 
   scount = 0;
   while (ii<=fcount) {
      sa[scount  ].addr  = ma[ii].startaddr;
      sa[scount++].start = 1;           // start address
      sa[scount  ].addr  = ma[ii].endaddr;
      sa[scount++].start = 0;           // end address
      ii++;
   }
   scount--;

   scnew = 0;
   for (ii=0; ii<scount; ii++) {
      for (jj=ii+1; jj<=scount; jj++) {
         register SortInfo *sai = sa+ii,
                           *saj = sa+jj;
         if (sai->addr > saj->addr) {
            u32t tmpdata = sai->addr;
            u8t  tmpflag = sai->start;
            sai->addr    = saj->addr;
            sai->start   = saj->start;
            saj->addr    = tmpdata;
            saj->start   = tmpflag;
         }
      }
      if (ii>0) {
         if (sa[ii].addr > sa[scnew].addr) {
            scnew++;
            sa[scnew].addr  = sa[ii].addr;
            sa[scnew].start = sa[ii].start;
         }
      }
   }
   if (sa[scount].addr > sa[scnew].addr) {
      scnew++;
      sa[scnew].addr  = sa[scount].addr;
      sa[scnew].start = sa[scount].start;
   }
   // build physmem array, blocklen holds end of block address here!
   scount = scnew;
   scnew  = 0;
   ppm[scnew].startaddr = sa[0].addr;
   tmpf   = sa[0].start;

   for (ii=1; ii<=scount; ii++) {
      if (tmpf) {                  // previous data was a start address
         if (sa[ii].start) {
            ppm[scnew].blocklen    = sa[ii].addr-1;
            ppm[++scnew].startaddr = sa[ii].addr;
         } else {
            ppm[scnew].blocklen    = sa[ii].addr;
            tmpf = 0;
         }
      } else {                     // previous data was an end address
         if (sa[ii].start) {
            ppm[++scnew].startaddr = sa[ii].addr;
            tmpf = 1;
         } else {
            ppm[scnew+1].startaddr = ppm[scnew].blocklen + 1;
            ppm[++scnew].blocklen  = sa[ii].addr;
         }
      }
   }
   // searching physmem for adjacent blocks and merge it
   ii     = 1;
   ccount = 0;
   while (ii<=scnew) {
      if (ppm[ii].startaddr - ppm[ccount].blocklen == 1)
         ppm[ccount].blocklen = ppm[ii++].blocklen;
      else {
         if (++ccount!=ii) {
            ppm[ccount].startaddr = ppm[ii].startaddr;
            ppm[ccount].blocklen  = ppm[ii].blocklen;
         }
         ii++;
      }
   }
   ccount++;
   /* round all high memory entries to page size.
      some strange BIOSes require this action. */
   ii=0;
   while (ii<ccount) {
      physmem_block *pb = ppm+ii++;
      if (pb->startaddr >= _1MB) {
         pb->blocklen  = (pb->blocklen + 1 & ~PAGEMASK) - 1;
         pb->startaddr = PAGEROUND(pb->startaddr);
      }
   }
   /* split block on 16M border.
      ACPI array has 10 entries, so we must fit to 16 here ;) */
   if (ccount<PHYSMEM_TABLE_SIZE) {
      ii=0;
      while (ii<ccount)
         if (ppm[ii].blocklen>_16MB && ppm[ii].startaddr<_16MB) {
            memmove16(ppm+ii+1,ppm+ii,(ccount-ii)*sizeof(physmem_block));
            ppm[ii+1].startaddr=_16MB;
            ppm[ii].blocklen   =_16MB-1;
            ccount++;
            break;
         } else ii++;
   }
   // blocklen now is the length of block actually
   ii = 0;
   while (ii<ccount) {
      ppm[ii].blocklen -= ppm[ii].startaddr - 1;
      ii++;
   }
   physmem_entries = ccount;
}

int init16(void) {
   u32t  ii, mpos, fsdlen = 0, obj32size;

   hlp_basemem();
   // use 16Mb as low own memory border
   mpos = _16MB;
   // and PXE cache can grow above it :(
   if (filetable.ft_cfiles==5) {
      u32t resend = filetable.ft_reslen + filetable.ft_resofs;
      if (resend > mpos) mpos = Round64k(resend);
   }
   // searching for whole 8Mb
   for (ii=0; ii<physmem_entries; ii++) {
      register physmem_block *pmb = physmem + ii;
      if (pmb->startaddr>=_16MB && pmb->startaddr+pmb->blocklen>mpos) {
         if (pmb->startaddr>mpos) mpos = pmb->startaddr;
         availmem = pmb->startaddr + pmb->blocklen - mpos;
         if (availmem>=_16MB/2) break;
      }
   }
   // no - error!
   if (ii>=physmem_entries) return 8; // QERR_NOMEMORY

   if (dd_bootflags&BF_MINIFSD) fsdlen = filetable.ft_mfsdlen;
   /* get memory for 32-bit part & mini-FSD buffer.
      Space for code/data, BSS, fixups, unpacker, stack and FSD buffer.
      Stack, actually, will be much larger (by fixups, unpacker buffer memory
      and page rounding. */
   stacksize = Round4k(unpbuffer);
   if (stacksize<QSM_STACK32) stacksize = QSM_STACK32;

   obj32size = Round4k(bin_header.pmSize + (bin_header.pmTotal - bin_header.pmStored));
   ii        = Round64k(obj32size + stacksize + fsdlen);
   stacksize = ii - obj32size - fsdlen & ~0xF;
   highbase  = mpos;
   unpbuffer = mpos + obj32size;
   highstack = unpbuffer + stacksize;
   if (fsdlen) minifsd_ptr = highstack;
   phmembase = mpos + ii;
   // truncate to nearest 64k
   availmem  = availmem - ii & 0xFFFF0000;
   memblocks = availmem>>16;
   return 0;
}
