//
// QSINIT
// fill memory info
// ----------------------------------------------------------------
// only general process is here: parse of 1st 10 E820 entries or old
// int15 88/E8 output, sort it and write to initial physmem array.
//
#pragma code_seg ( CODE32, CODE )
#pragma data_seg ( DATA32, DATA )

#include "qstypes.h"
#include "qsint.h"
#include "qsutil.h"
#include "clib.h"
#include "qsstor.h"

// int 15h E820h function memory table
#define ACPIMEM_TABLE_SIZE  10
#define FINDERARRAY_CNT    128
extern u16t         Int1588Mem;                       // memory: int 15h ah=88h
extern u16t         Int15E8Mem;                       // memory: int 15h ax=E801h
extern AcpiMemInfo AcpiInfoBuf[ACPIMEM_TABLE_SIZE];   // memory: int 15h ax=E820h
extern u16t         AcpiMemCnt;
extern u32t          DiskBufPM;
extern physmem_block   physmem[PHYSMEM_TABLE_SIZE];
u16t           physmem_entries;

typedef struct {
   u32t              startaddr;     // start address of physical memory block
   u32t                endaddr;     // end address of physical memory block
} BlockInfo;

typedef struct {
   u32t                   addr;     // address itself
   u8t                   start;     // start or end address
} SortInfo;

void _std hlp_basemem(void) {
   u32t  fcount, scount, scnew, ccount, ii, jj;
   u8t     tmpf;
   // use disk buffer as array space to save stack and code size
   BlockInfo *ma = (BlockInfo*)DiskBufPM;
   SortInfo  *sa = (SortInfo*)(DiskBufPM + FINDERARRAY_CNT*sizeof(BlockInfo));
   physmem_block  *ppm = physmem;

   physmem_entries = 0;       // zero counter
   // 640KB
   fcount = 0;
   ma[fcount].startaddr = 0;
   ma[fcount].endaddr = ((u32t)int12mem()<<10) - 1;
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
      APCI array has 10 entries, so we must fit to 16 here ;) */
   if (ccount<PHYSMEM_TABLE_SIZE) {
      ii=0;
      while (ii<ccount)
         if (ppm[ii].blocklen>_16MB && ppm[ii].startaddr<_16MB) {
            memmove(ppm+ii+1,ppm+ii,(ccount-ii)*sizeof(physmem_block));
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
   // save storage key with array`s pointer and size
   sto_save(STOKEY_PHYSMEM,&physmem,physmem_entries * sizeof(physmem_block),0);
}

void getfullSMAP(void);

AcpiMemInfo* _std int15mem(void) {
   rmcall(getfullSMAP,0);
   return (AcpiMemInfo*)DiskBufPM;
}
