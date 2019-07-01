/*
   some notes about "too easy" processing and assumes:
     * hlp_memalloc() memory is always page aligned and zero filled.
     * malloc() memory is always aligned to 16 bytes.
*/

#include "seldesc.h"
#include "qspage.h"
#include "qsutil.h"
#include "pagedesc.h"
#include "qsinit.h"
#include "meminit.h"
#include "cpudef.h"
#include "qssys.h"
#include "errno.h"
#include "vio.h"
#include "syslocal.h"
#include "qspdata.h"

// used memory size in pdmem block
#define PDMEMSIZE    (4*PAGESIZE)

#define PTEINPAGE    512                  // PAE paging
#define BIGPAGE      (1<<PD2M_ADDRSHL)    // big page size (2Mb)
#define PDESHIFT     29                   // page dir index in PAE mode

#define PAGES4GB     _1MB                 // total number of pages in 4Gb
// number of 64k blocks for Page Tables (= 128)
#define PTAENTRIES   (PAGES4GB/PTEINPAGE*_4KB>>16)
// number of pages in 64k
#define PTAPAGES     (PTEINPAGE*16)
// get pta array index
#define PTAE(addr)   (((addr)>>PAGESHIFT)/PTAPAGES)
// get page index from start of its pta array
#define PTAIDX(addr) ((addr)>>PAGESHIFT & PTAPAGES-1)
// page number of 1st page in pta array "idx"
#define PTA1ST(idx)  ((idx)*PTAPAGES)
// get big page index in its page directory
#define PDEIDX(addr) (((1<<PDESHIFT)-1 & (addr)) >> PD2M_ADDRSHL)
// size of page table entry (8 bytes)
#define PTESIZE      (_4KB/PTEINPAGE)

static u64t    *pdpt = 0,         // 32-aligned pointer
              *pdptf = 0;         // real pointer, for free
static u64t   *pdmem = 0;         // page dirs
u8t      in_pagemode = 0;         // QSINIT in page mode
static int    global = 0;         // PGE extension present
static u32t     vcr3 = 0,         // cr3 value
                vcr4 = 0;         // cr4 value

// page tables (128 blocks of 64kb, only allocated if used one).
static void     *pta[PTAENTRIES];
static qshandle pmux = 0;

extern qshandle              mhimux;
extern u64t       _std   page0_fptr;   // 48-bit pointer to page 0 in QSINIT
extern mt_proc_cb _std mt_exechooks;   // MT service hooks

void      sys_tsscr3(u32t cr3);

int _std sys_pagemode(void) {
   return in_pagemode;
}

/** change one or more 64k PT arrays.
    @param  ptaidx    Index of array
    @param  start     Page index from array`s start
    @param  len       Number of pages to set, can overflow array size */
static void ptset(u32t ptaidx, u32t start, u32t len, u64t phys, u32t flags) {
   // setup flags
   u32t pattr = flags&MEM_DIRECT ? global : 0;
   if ((flags&MEM_READONLY)==0) pattr|=PT_WRITE;

   if (start>=PTAPAGES) {
      log_it(2, "ptset invalid start (%d)\n", start);
      return;
   }
#if 0
   log_it(2, "ptset(%d,%d,%d,%010LX,%08X)\n", ptaidx, start, len, phys, flags);
#endif
   while (len) {
      u32t leave = PTAPAGES - start;
      if (leave>len) leave = len;
      // process current array changes
      if (leave) {
         u32t  pdi, pdlen, pdcnt, rem;
         u64t  *pde = pdmem + (ptaidx*PTAPAGES + start>>PD2M_ADDRSHL-PAGESHIFT);

         len -= leave;
         if (!pta[ptaidx]) pta[ptaidx] = hlp_memallocsig(_64KB, "ptab", 0);
         // update page directory
         pdi   = start/PTEINPAGE;
         pdcnt = (leave + (start&PTEINPAGE-1) + PTEINPAGE-1) / PTEINPAGE;
#if 0
         log_it(2, "pdi %d, cnt %d\n", pdi, pdcnt);
#endif
         while (pdcnt--)
            *pde++ = PT_PRESENT|PT_WRITE|(u32t)pta[ptaidx] + (pdi++<<PAGESHIFT);
         // update page table
         if (flags) {
            u64t *pte = (u64t*)pta[ptaidx] + start;
            while (leave) {
              *pte++ = PT_PRESENT|pattr|phys;
              phys  += PAGESIZE;
              leave--;
            }
         } else
            memset((u8t*)pta[ptaidx] + start*PTESIZE, 0, PTESIZE*leave);
      }
      // next pta array
      start = 0;
      ptaidx++;
   }
}

/* function cannot handle partial unmap of 2Mb pages, but single our`s user
   pag_physunmap() must not use such requests */
static int map_common(u32t virt, u32t len, u64t phys, u32t flags) {
   int intsv;
   if ((virt&PAGEMASK) || (len&PAGEMASK) || (phys&PAGEMASK)) return 0;
#if 0 // removed, else ramdisk will annoy us terribly ;)
   log_it(2, "map_common(%08X,%08X,%010LX,%08X)\n", virt, len, phys, flags);
#endif
   // disable interrupts
   intsv = sys_intstate(0);
   // unmap / can use 2Mb pages for this map?
   if (len>=BIGPAGE && (!flags || (virt&BIGPAGE-1)==(phys&BIGPAGE-1))) {
      u32t rem;
      // start address is not big page aligned
      if ((rem = virt&BIGPAGE-1)!=0) {
         rem   = BIGPAGE-rem;
         // log_it(2, "1: virt %08X, rem %08X\n", virt, BIGPAGE-rem);
         ptset(PTAE(virt), PTAIDX(virt), rem>>PAGESHIFT, phys, flags);
         virt += rem;
         phys += rem;
         len  -= rem;
      }
      // end address is not big page aligned
      if (len && (rem = virt+len&BIGPAGE-1)!=0) {
         // log_it(2, "2: virt %08X, rem %08X\n", virt+len, rem);
         ptset(PTAE(virt+len), PTAIDX(virt+len-rem), rem>>PAGESHIFT,
            phys+len-rem, flags);
         len  -= rem;
      }
      // map/unmap big pages
      if (len) {
         u64t  *pde = pdmem + (virt>>PD2M_ADDRSHL);
         u32t pattr = flags&MEM_DIRECT ? global : 0;
         if ((flags&MEM_READONLY)==0) pattr|=PT_WRITE;

         len >>=PD2M_ADDRSHL;
         while (len) {
            if (flags) {
               *pde = PT_PRESENT|PD_BIGPAGE|pattr|phys;
               phys+= BIGPAGE;
            } else {
               /* clear page table 4k (else we get old-mapped entries on next
                  partial use of these 2Mb. pta entry must exist here! */
               if ((*pde&(PD_BIGPAGE|PT_PRESENT))==PT_PRESENT)
                  memset((u8t*)pta[PTAE(virt)] + (PTAIDX(virt)<<PAGESHIFT), 0, PAGESIZE);
               *pde = 0;
               virt+= BIGPAGE;
            }
            len--; pde++;
         }
      }
   } else
      ptset(PTAE(virt), PTAIDX(virt), len>>PAGESHIFT, phys, flags);

   if (in_pagemode) sys_setcr3(vcr3,FFFF);

   sys_intstate(intsv);
   return 1;
}

int pag_mappages(u32t virt, u32t len, u64t phys, u32t flags) {
   // supply only "direct" flag - for global page mapping
   return map_common(virt, len, phys, 1 | flags & (MEM_DIRECT|MEM_READONLY));
}

int pag_unmappages(u32t virt, u32t len) {
   return map_common(virt, len, 0, 0);
}

void pag_lock(void) {
   if (in_mtmode) mt_muxcapture(pmux);
}

void pag_unlock(void) {
   if (in_mtmode) mt_muxrelease(pmux);
}

void setup_pagman_mt(void) {
   if (mt_muxcreate(0, "__pgmem_mux__", &pmux) ||
      io_setstate(pmux, IOFS_DETACHED, 1) ||
         mt_muxcreate(0, "__hicopymux__", &mhimux) ||
            io_setstate(mhimux, IOFS_DETACHED, 1))
               log_it(0, "pager mutex err!\n");
}

qserr _std pag_enable(void) {
   u32t   map0, ii,
         diff0 = hlp_segtoflat(0);
   u16t flatds = get_flatcs() + 8;

   if (hlp_hosttype()==QSHT_EFI) return E_SYS_EFIHOST;

   ii = sys_isavail(SFEA_PAE|SFEA_PGE);
   // no PAE?
   if ((ii&SFEA_PAE)==0) return E_SYS_UNSUPPORTED;
   // add global flag if PGE supported
   if ((ii&SFEA_PGE)) global = PT_GLOBAL;

   mt_swlock();
   if (pdptf) { mt_swunlock(); return E_SYS_INITED; }

   // malloc always returning memory aligned to 16 bytes
   pdpt = pdptf = (u64t*)malloc(8*4+16);
   // align PDPT to 32 bytes
   if ((u32t)pdpt&0x10) pdpt+=2;
   // no 64k? then trap!
   pdmem = (u64t*)hlp_memallocsig(PDMEMSIZE, "pdir", QSMA_READONLY);
   // zero page table pointers
   memset(&pta, 0, sizeof(pta));
   // fill 4 PDPTEs
   for (ii=0; ii<4; ii++)
      pdpt[ii] = PT_PRESENT | (u32t)pdmem + (ii<<PAGESHIFT);
   //log_it(2, "PDPTE: %4Lb\n", pdpt);
   // call meminit.cpp to map all available memory via pag_mappages()
   pag_inittables();
   log_it(2, "tables created\n");
#if 0
   pag_printall(0);
#endif
   /* change reversed FLAT data selector to plain mode.
      registers will be updated after next switch to RM (interrupt) or
      key_status() call below */
   if (!hlp_insafemode()) {
      struct desctab_s sd;
      u32t   fbase = 0;
      sd.d_limit   = 0xFFFF;
      sd.d_loaddr  = fbase;
      sd.d_hiaddr  = fbase>>16;
      sd.d_access  = D_DATA0;
      sd.d_attr    = D_DBIG|D_GRAN4K|D_AVAIL|0x0F;
      sd.d_extaddr = fbase>>24;

      sys_seldesc(flatds, &sd);
   }
   // make sure - PAT:0 have WB cache type
   if (sys_isavail(SFEA_PAT)) {
      u32t  patlo, pathi;
      hlp_readmsr(MSR_IA32_PAT, &patlo, &pathi);
      if ((patlo&7)!=MSR_PAT_WB) {
         log_it(2, "Warning! PAT:0 type was %d\n", patlo&7);

         patlo = patlo&~7|MSR_PAT_WB;
         hlp_writemsr(MSR_IA32_PAT, patlo, pathi);
      }
   }
   // calc cr3 & cr4 values
   vcr3 = (u32t)pdpt;
   vcr4 = getcr4() | (global?CPU_CR4_PGE:0) | CPU_CR4_PAE;
   log_it(2, "cr3=%08X, cr4=%08X\n", vcr3, vcr4);
   // go on!
   ii = sys_intstate(0);
   sys_tsscr3(vcr3);
   sys_setcr3(vcr3, vcr4);
   /* First interrupt or RM call in key_status() will switch us to PAE
      while returning from RM.
      Page 0 is r/w here - until the end of this function. */
#if 0
   __asm {
      mov eax, cr0
      or  eax, CPU_CR0_PG
      mov cr0, eax
   }
#endif
   sys_intstate(ii);
   key_status();
   // flag and print Hello!
   in_pagemode = 1;
   log_it(0, "Hello, PAE world!\n");
   // replace FLAT:0 access pointer in QSINIT
   map0       = (u32t)pag_physmap(0, _4KB, PHMAP_FORCE);
   page0_fptr = MAKEFAR32(flatds, map0);
   // remap first page as read-only
   pag_mappages(0, _4KB, 0, MEM_DIRECT|MEM_READONLY);

   log_it(2, "Map zero to %08X\n", map0);
   // reload ss from ds. It was SELZERO, not SEL32DATA before.
   __asm {
      mov  ax, ds
      mov  ss, ax
      nop
   }
   mt_swunlock();
   // notify about success
   sys_notifyexec(SECB_PAE, 0);
   return 0;
}

u32t _std pag_query(void *addr) {
   u32t va = (u32t)addr;
   // on EFI entire 4Gb should be mirrored
   if (hlp_hosttype()==QSHT_EFI) return PGEA_WRITE; else
   // 1st page is beyond of DS segment in plain mode
   if (!in_pagemode) return va<PAGESIZE?PGEA_NOTPRESENT:PGEA_WRITE; else {
   // else decoding PAE page tables
      u32t  cr3v = getcr3();
      u64t *pdpt = (u64t*)(cr3v&~(1<<CR3PAE_PDPSHL-1)),
            *pde = (u64t*)(pdpt[va>>PD1G_ADDRSHL]&~(u64t)PAGEMASK) +
                   ((va&PD1G_MASK)>>PD2M_ADDRSHL),
              pv = *pde;

      if ((pv&PT_PRESENT)==0) return PGEA_NOTPRESENT; else
      if (pv&PD_BIGPAGE) return pv&PT_WRITE?PGEA_WRITE:PGEA_READ; else {
         u64t  paddr = pv & ((u64t)1<<PT64_MAXADDR)-PAGESIZE;
         u64t    *pt = (u64t*)paddr + ((va&PD2M_MASK)>>PT_ADDRSHL), ptv;
         // ????
         if (paddr>=_4GBLL) return PGEA_UNKNOWN;
         ptv = *pt;
         if ((ptv&PT_PRESENT)==0) return PGEA_NOTPRESENT; else
            return ptv&PT_WRITE?PGEA_WRITE:PGEA_READ;
      }
   }
}
