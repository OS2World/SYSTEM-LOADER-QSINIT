#include "seldesc.h"
#include "pagedesc.h"
#include "stdlib.h"
#include "cpudef.h"
#include "qssys.h"
#include "qshm.h"
#include "internal.h"
#include "qecall.h"

static const char *pattype[8] = { "UC", "WC", "???", "???", "WT",
   "WP", "WB", "UC-" };

static void fillinfo(u32t lowdd, char *tgt, u64t patmsr, int big) {
   // "W U A D G UC-"
   // "R S A D G WT"
   memset(tgt, ' ', 14);
   tgt[0] = lowdd&PT_WRITE ?'W':'R';
   tgt[2] = lowdd&PT_R3ACCESS ?'U':'S';

   if (lowdd&PT_ACCESSED) tgt[4] = 'A';
   if (lowdd&PT_DIRTY)    tgt[6] = 'D';
   if (big && (lowdd&PT_BIGPAGE) || !big)
      if (lowdd&PT_GLOBAL) tgt[8] = 'G';

   if (patmsr==FFFF64) {
      if (lowdd&PT_PWT)  { tgt[10] = 'W'; tgt[11] = 'T'; }
      if (lowdd&PT_CDIS) { tgt[10] = 'C'; tgt[11] = 'D'; }
   } else {
      u32t idx = (lowdd&PT_PWT?1:0) + (lowdd&PT_CDIS?2:0);
      if (!big) idx+=lowdd&PT_PAT4K?4:0; else
      if ((lowdd&PT_BIGPAGE)) idx+=lowdd&PT_PATBIG?4:0;

      strcpy(tgt+10, pattype[(u32t)(patmsr>>idx)&7]);
   }
   tgt[14] = 0;
}

static void dump_pagedir(u64t *pd, u64t vbase, u64t patmsr, int is64) {
   u32t ii, lnp = 0;

   for (ii=0; ii<512; ii++) {
      u64t *pde = pd + ii,
             pv = *pde,
          paddr = pv & ((u64t)1<<PT64_MAXADDR)-PAGESIZE,
           addr = vbase + ii*2*_1MB;
      char astr[48], va_str[32];

      snprintf(va_str, 32, is64?"%012LX:":"%08LX:", addr);

      if (pv&PT_PRESENT) {
         fillinfo(pv, astr, patmsr, 1);

         if (pv&PD_BIGPAGE) {
            log_it(2, "%s big page (%012LX) %s\n", va_str, paddr&~(u64t)((1<<PD2M_ADDRSHL)-1),
               astr);
         } else
         if (paddr>=_4GBLL) {
            log_it(2, "%s page table above 4Gb! (%012LX) %s\n", va_str, paddr, astr);
         } else {
            u32t pti;
            u64t *pt = (u64t*)paddr;
            log_it(2, "%s page tab (%012LX) %s\n", va_str, paddr, astr);
            for (pti=0; pti<512; pti++) {
               u64t ptv = pt[pti];
               paddr    = ptv & ((u64t)1<<PT64_MAXADDR)-PAGESIZE;
               snprintf(va_str, 32, is64?"%012LX:":"%08LX:", addr+pti*PAGESIZE);

               if (ptv&PT_PRESENT) {
                  fillinfo(ptv, astr, patmsr, 0);
                  log_it(2, "%s          (%012LX) %s\n", va_str, paddr, astr);
               } else
                  log_it(2, "%s np\n", va_str);
            }
         }
         lnp = 0;
      } else
      if (!lnp) { log_it(2, "%s not present\n", va_str, addr); lnp = 1; }

   }
}

// it is not locked!
void _std pag_printall(void) {
   u32t  ii, pi, cr0v, cr3v, cr4v;
   u64t  patmsr;
   log_it(2,"<====== Page table ======>\n");
   cr0v = getcr0();
   cr3v = getcr3();
   cr4v = getcr4();
   if ((cr0v&CPU_CR0_PG)==0) return;
   if ((cr4v&CPU_CR4_PAE)==0) return;

   if (sys_isavail(SFEA_PAT)) {
      hlp_readmsr(MSR_IA32_PAT, (u32t*)&patmsr, (u32t*)&patmsr + 1);
      log_it(2,"PAT (0..7): %8b\n", &patmsr);
   } else
      patmsr = FFFF64;

   if (sys_is64mode()) {
      u64t  cr3x, *pmlt;
      u32t  pmli, lnp = 0;
      log_it(2,"64-bit mode!\n");
      cr3x = call64l(EFN_HLPSETREG, 2, 2, SETREG64_CR3, FFFF64);
      if (cr3x==FFFF64) { log_it(2,"Version mismatch!\n"); return; }
      if (cr3x>=_4GBLL) { log_it(2,"Page table above 4GB!\n"); return; }

      pmlt = (u64t*)((u32t)cr3x & PT_ADDRMASK);

      for (pmli=0; pmli<512; pmli++) {
         u64t   pv = pmlt[pmli],
            staddr = (u64t)pmli<<39;
         char astr[48];

         if (pv&PT_PRESENT) {
            u64t pdpt = pv & ((u64t)1<<PT64_MAXADDR)-PAGESIZE;
            fillinfo(pv, astr, patmsr, 1);
            lnp = 0;

            log_it(2, "PML4E[%d]: %012LX %s\n", pmli, pdpt, astr);

            if (pdpt>=_4GBLL) continue;

            for (ii=0; ii<512; ii++, staddr+=_1GB) {
               u64t  paddr;
               pv    = ((u64t*)pdpt)[ii];
               paddr = pv & ((u64t)1<<PT64_MAXADDR)-PAGESIZE;
               fillinfo(pv, astr, patmsr, 1);

               if (pv&PT_PRESENT) {
                  lnp = 0;
                  if (pv&PD_BIGPAGE) {
                     log_it(2, "%012LX: big page (%012LX) %s\n", staddr,
                        paddr&~(u64t)((1<<PD1G_ADDRSHL)-1), astr);
                  } else
                  if (paddr>=_4GBLL) {
                     log_it(2, "%012LX: PDPTE above 4Gb! (%012LX) %s\n", staddr,
                        paddr, astr);
                  } else {
                     log_it(2, "%012LX: PDPTE (%012LX) %s\n", staddr, paddr, astr);
                     dump_pagedir((u64t*)paddr, staddr, patmsr, 1);
                  }
               } else
               if (!lnp) { log_it(2, "%012LX: not present\n", staddr); lnp = 1; }
            }
         } else
         if (!lnp) { log_it(2, "%012LX: not present\n", staddr); lnp = 1; }
      }
   } else {
      u64t *pdpt = (u64t*)(cr3v&~(1<<CR3PAE_PDPSHL-1));

      for (ii=0; ii<4; ii++) {
         u32t lnp = 0;
         log_it(2, "PDPTE[%d]: %010LX\n", ii, pdpt[ii]);
         dump_pagedir((u64t*)(pdpt[ii]&~(u64t)PAGEMASK), ii*_1GB, patmsr, 0);
      }
   }
}
