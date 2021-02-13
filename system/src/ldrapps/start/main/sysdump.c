#include "qssys.h"
#include "qslog.h"
#include "seldesc.h"
#include "stdlib.h"
#include "syslocal.h"

void get_gdt(struct lidt64_s *gdt);
void get_idt(struct lidt64_s *idt);
#ifdef __WATCOMC__
#pragma aux get_gdt = "sgdt [eax]" parm [eax] modify exact [];
#pragma aux get_idt = "sidt [eax]" parm [eax] modify exact [];
#endif

/// dump GDT to log
void _std sys_gdtdump(printf_function pfn) {
   struct lidt64_s    gdt;
   u32t      sels, ii, lp;
   struct desctab_s   *gd;

   if (pfn==0) pfn = log_printf;

   memset(&gdt, 0, sizeof(struct lidt64_s));
   get_gdt(&gdt);

   gd   = (struct desctab_s *)gdt.lidt64_base;
   sels = (gdt.lidt64_limit & 0xFFFF) >> 3;
   if (gdt.lidt64_base>=_4GBLL) {
      pfn("GDT is above 4Gb\n");
      return;
   }
   
   pfn("== GDT contents ==\n");
   for (ii=0, lp=0; ii<=sels; ii++, gd++) {
      char  outs[128], *cp = outs;
      cp += sprintf(cp, "%04X : ", ii<<3);

      if ((gd->d_access & D_PRES)==0) {
         // do not annoy with empty GDT entries
         if (ii<=lp+1 || ii==sels) strcpy(cp, "not present"); 
            else continue;
      } else {
         int good = 0, edn = 0;
         if (gd->d_attr & D_LONG) {
            if (gd->d_attr & D_DBIG) strcpy(cp, "invalid"); else {
               cp += sprintf(cp, "Code64 %c %2s  ", gd->d_access&D_ACCESSED?'A':' ',
                  gd->d_access&D_CONFORM?"CN":"  ");
               good = 2;
            }
         } else
         if (gd->d_access & D_SEG) {
            int  code = gd->d_access & D_CODE;

            cp += sprintf(cp, "%-7s", code?"CodeE":"DataR");
            if (gd->d_access&D_RX) cp[-2]=code?'R':'W';

            edn = gd->d_access&D_EXPDN;
            cp += sprintf(cp, "%c %2s   ", gd->d_access&D_ACCESSED?'A':' ',
               edn?(code?"CN":"ED"):"  ");
            // conforming bit for code
            if (code) edn = 0;

            good = 1;
         } else {
            static const char *syssel[] = {
               0, "TSS16 (Avl)", "LDT", "TSS16 (Busy)", "CallG16", "TaskG", "IntG16", "TrapG16",
               0, "TSS32 (Avl)", 0,     "TSS32 (Busy)", "CallG32", 0,       "IntG32", "TrapG32",
            };
            const char *tx = syssel[gd->d_access & D_TYPEMASK];
         
            if (!tx) sprintf(cp, "invalid type (%02X)", gd->d_access); else {
               cp += sprintf(cp, "%-14s", tx);
               good = 1;
            }
         }
         if (good==1) {
            u32t limit = gd->d_limit | (u32t)(gd->d_attr & D_EXTLIMIT)<<16,
                  rlim = _lsl_(ii<<3), rbase,
                  base = gd->d_loaddr | (u32t)gd->d_hiaddr<<16 | (u32t)gd->d_extaddr<<24;
            if (edn) {
               rbase = rlim+1+base;
               rlim  = (gd->d_attr&D_DBIG? FFFF : 0xFFFF) - (rlim+1);
            } else
               rbase = base;

            cp += sprintf(cp, "%08X L:%08X %c %c (%08X..%08X) ", base, limit, 
               gd->d_attr&D_GRAN4K?'G':' ', gd->d_attr&D_DBIG?'B':' ', rbase, rbase+rlim);
         }
         if (good) cp += sprintf(cp, " DPL:%d", gd->d_access>>5 & 3);
         lp = ii;
      }
      strcat(cp, "\n");
      pfn(outs);
   }
}

/// dump IDT to log
void _std sys_idtdump(printf_function pfn) {
   struct lidt64_s  idt;
   u32t    irqs, ii, lp;

   if (pfn==0) pfn = log_printf;

   memset(&idt, 0, sizeof(struct lidt64_s));
   get_idt(&idt);

   pfn("== IDT contents ==\n");

   if (!sys_is64mode()) {
      struct gate_s   *id = (struct gate_s *)idt.lidt64_base;
      irqs = (idt.lidt64_limit & 0xFFFF) >> 3;

      // dump 32-bit IDT
      for (ii=0, lp=0; ii<=irqs; ii++, id++) {
         char  outs[128], *cp = outs;
         cp += sprintf(cp, "%3d : ", ii);
      
         if ((id->g_access & D_PRES)==0) {
            // do not annoy with empty IDT entries
            if (ii<=lp+1 || ii==irqs) strcpy(cp, "not present"); 
               else continue;
         } else {
            int good = 0,
               gtype = id->g_access & 7;
      
            switch (gtype) {
               case D_TASKGATE:
                  if ((id->g_access&(0x10|D_32))==0) {
                     cp += sprintf(cp, "TaskG    %04X         ", id->g_handler>>16);
                     good = 1;
                  }
                  break;
               case D_INTGATE :
               case D_TRAPGATE:
                  if ((id->g_access&0x10)==0 && (id->g_parms&~D_WCMASK)==0) {
                     static const char *gtstr[] = {"Int16 ", "Trap16", "Int32 ", "Trap32"};
      
                     cp += sprintf(cp, "%s   %04X:%08X", gtstr[(id->g_access&1) + 
                        (id->g_access&D_32?2:0)], id->g_handler>>16,
                           id->g_handler&0xFFFF|(u32t)id->g_extoffset<<16);
                     good = 1;
                  }
                  break;
            }
            if (good) {
               cp += sprintf(cp, "  DPL:%d", id->g_access>>5 & 3);
            } else {
               sprintf(cp, "invalid type (%02X %02X)", id->g_access, id->g_parms);
            }
            lp = ii;
         }
         strcat(cp, "\n");
         pfn( outs);
      }
   } else {
      struct gate64_s *id = (struct gate64_s *)idt.lidt64_base;
      irqs = (idt.lidt64_limit & 0xFFFF) >> 4;
   
      if (idt.lidt64_base>=_4GBLL) {
         pfn("IDT is above 4Gb\n");
         return;
      }
      // dump 64-bit IDT
      for (ii=0, lp=0; ii<=irqs; ii++, id++) {
         char  outs[128], *cp = outs;
         cp += sprintf(cp, "%3d : ", ii);
      
         if ((id->g64_access&D_PRES)==0) {
            // do not annoy with empty IDT entries
            if (ii<=lp+1 || ii==irqs) strcpy(cp, "not present"); 
               else continue;
         } else {
            int good = 0;

            switch (id->g64_access&0xF) {
               case D_INTGATE32 :
               case D_TRAPGATE32:
                  if ((id->g64_access&0x10)==0 && (id->g64_parms&~D_WCMASK)==0) {
                     static const char *gtstr[] = {"Int64 ", "Trap64"};
                     cp += sprintf(cp, "%s   %04X:%012LX", gtstr[id->g64_access&1], id->g64_sel,
                        (u64t)id->g64_ofs32<<32 | (u64t)id->g64_ofs16<<16 | id->g64_ofs0);
                     good = 1;
                  }
                  break;
            }
            if (good) {
               cp += sprintf(cp, "  DPL:%d  IST:%d", id->g64_access>>5 & 3, id->g64_parms&7);
            } else {
               sprintf(cp, "invalid type (%02X %02X)", id->g64_access, id->g64_parms);
            }
            lp = ii;
         }
         strcat(cp, "\n");
         pfn(outs);
      }
   }
}
