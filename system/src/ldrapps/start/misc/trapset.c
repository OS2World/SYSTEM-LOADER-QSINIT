//
// QSINIT "start" module
// exceptions setup/service functions
//
#include "stdlib.h"
#include "seldesc.h"
#include "qsxcpt.h"
#include "qslog.h"
#include "qsinit.h"
#include "qsutil.h"
#include "qssys.h"
#include "efnlist.h"
#include "qecall.h"
#include "vio.h"

#define TRAP_STACK        6144   // stack size for traps 8, 12 & 13

struct tss_s     *tss_data = 0,  // tss struct
                    *tss08 = 0, 
                    *tss12 = 0,
                    *tss13 = 0;
u16t              main_tss = 0,  // tss selector
                trap08_tss = 0,
                trap12_tss = 0,
                trap13_tss = 0;
u32t             no_tgates = 0;

void _std sys_settr  (u16t sel);
void _std except_init(void);
void      traptask_8(void);
void      traptask_12(void);
void      traptask_13(void);

void _std sys_exfunc2(volatile sys_xcpt* except) {
   sys_exfunc3(except);
   log_it(2,"Exception %d occured in file %s, line %d\n", except->reserved[1],
      (char*)except->reserved[2], except->reserved[3]);
}

/** allocate TSS selector.
    @param   tssdata       TSS data (must not cross page boundary, as Intel says)
    @param   limit         TSS limit (use 0 for default)
    @return  tss selector or 0 on error. */
u16t _std sys_tssalloc(void *tssdata, u16t limit) {
   struct desctab_s  sd;
   u16t sel = hlp_selalloc(2);
   u32t  pa = (u32t)tssdata - hlp_segtoflat(0);
   if (!sel) return 0;
   if (!limit) limit = sizeof(struct tss_s) - 1;
   
   sd.d_limit   = limit;
   sd.d_loaddr  = pa;
   sd.d_hiaddr  = pa>>16;
   sd.d_access  = D_TSS032;
   sd.d_attr    = 0;
   sd.d_extaddr = pa>>24;

   if (!sys_seldesc(sel,&sd) || !hlp_selsetup(sel+SEL_INCR, pa, limit, 
      QSEL_16BIT|QSEL_BYTES)) 
   {
      log_it(2,"tssalloc(%X,%u) -> %X %8b\n", tssdata, limit, pa, &sd);

      hlp_selfree(sel+SEL_INCR);
      hlp_selfree(sel);
      return 0;
   }
   return sel;
}

/** free TSS selector.
    Function will fail if TSS is current task or linked to current task.
    @param   sel           TSS selector
    @return success flag (1/0) */
int _std sys_tssfree(u16t sel) {
   u32t  diff = hlp_segtoflat(0);
   int  svint = sys_intstate(0),
           rc = 0;
   u16t   ctr = get_taskreg(),
         wsel = ctr;
   /* walk over linked tasks ... (but who can create this list??? ;) */
   while (wsel!=sel) {
      struct tss_s *tss;
      u32t   base = hlp_selbase(wsel,0);
      // invalid selector? -> end of list
      if (base==FFFF) { rc = 1; break; }

      tss  = (struct tss_s*)(base - diff);
      wsel = tss->tss_backlink;
      // walked to the first entry again
      if (wsel==ctr) { rc = 1; break; }
   }
   sys_intstate(svint);
   if (rc) {
      u32t lim1, lim2;
      // selector for r/w access to TSS
      if (hlp_selbase(sel+SEL_INCR,&lim2) == hlp_selbase(sel,&lim1))
         if (lim1==lim2) hlp_selfree(sel+SEL_INCR);
      hlp_selfree(sel);
   }
   return rc;
}

#ifdef __WATCOMC__
// direct jump to exception handling asm code
void trap_handle(struct tss_s *xd);
#pragma aux trap_handle "_*" parm [eax] aborts;
void trap_handle_64(struct tss_s *ts, struct xcpt64_data *xd);
#pragma aux trap_handle_64 "_*" parm [eax] [edx] aborts;
#endif

static void _std xcpt64(struct xcpt64_data *xd) {
   struct desctab_s sd;
   struct tss_s    t32;
   int  is64 = 1;
   /* interrupts disabled here by common 64-bit exception handler -
      until the end of trap screen output, or, at least, until 1st EFI`s
      console call in native console mode */
   if (sys_selquery(xd->x64_cs,&sd))
      if ((sd.d_attr&D_LONG)==0) is64 = 0;
   // 32-bit struct still used for 64-bit printing too
   t32.tss_cs  = xd->x64_cs;
   t32.tss_ds  = xd->x64_ds;
   t32.tss_es  = xd->x64_es;
   t32.tss_fs  = xd->x64_fs;
   t32.tss_gs  = xd->x64_gs;
   t32.tss_ss  = xd->x64_ss;
   t32.tss_eax = xd->x64_rax;
   t32.tss_ebx = xd->x64_rbx;
   t32.tss_ecx = xd->x64_rcx;
   t32.tss_edx = xd->x64_rdx;
   t32.tss_esi = xd->x64_rsi;
   t32.tss_edi = xd->x64_rdi;
   t32.tss_ebp = xd->x64_rbp;
   t32.tss_eip = xd->x64_rip;
   t32.tss_esp = xd->x64_rsp;
   t32.tss_cr3 = xd->x64_code;
   t32.tss_eflags    = xd->x64_rflags;
   t32.tss_reservdbl = xd->x64_number;
   // task gate selector (flag to prevent task switching)
   t32.tss_backlink  = 0;
   // will never return from both functions
   if (is64) trap_handle_64(&t32,xd); else trap_handle(&t32);
}

static void setup_eficeptions(int stage) {
   if (stage==0) {
      sys_setxcpt64(xcpt64);
      if (!call64(EFN_XCPTON, 0, 0)) log_it(2,"no exceptions!\n");
   }
}

void setup_exceptions(int stage) {
   if (hlp_hosttype()==QSHT_EFI) {
      setup_eficeptions(stage);
      return;
   } else
   if (hlp_hosttype()!=QSHT_BIOS) return;

   if (stage==0) {
      except_init();
   } else 
   if (stage==1 && !no_tgates) {
      u32t step = Round256(sizeof(struct tss_s));
      // create tasks for common code and 8, 12, 13 exceptions
      tss_data = hlp_memallocsig(step * 4 + TRAP_STACK * 4, "tss", QSMA_READONLY|QSMA_RETERR);
      if (tss_data) {
         u16t selzero = get_flatss(),
              selcs   = get_flatcs();
         tss08 = (struct tss_s *)((u8t*)tss_data + step);
         tss12 = (struct tss_s *)((u8t*)tss08 + step);
         tss13 = (struct tss_s *)((u8t*)tss12 + step);
         // this stack is used in exception handling
         tss_data->tss_esp0  = (u32t)tss_data + step * 4 + TRAP_STACK;
         tss_data->tss_ss0   = selzero;
         tss_data->tss_iomap = sizeof(struct tss_s);
         // task stacks
         tss08->tss_esp   = tss_data->tss_esp0 + TRAP_STACK;
         tss08->tss_ss    = selzero;
         tss08->tss_eip   = (u32t)&traptask_8;
         tss08->tss_cs    = selcs;
         tss08->tss_esp2  = (u32t)tss08;
         tss08->tss_iomap = sizeof(struct tss_s);
      
         tss12->tss_esp   = tss08->tss_esp + TRAP_STACK;
         tss12->tss_ss    = selzero;
         tss12->tss_eip   = (u32t)&traptask_12;
         tss12->tss_cs    = selcs;
         tss12->tss_esp2  = (u32t)tss12;
         tss12->tss_iomap = sizeof(struct tss_s);
      
         tss13->tss_esp   = tss12->tss_esp + TRAP_STACK;
         tss13->tss_ss    = selzero;
         tss13->tss_eip   = (u32t)&traptask_13;
         tss13->tss_cs    = selcs;
         tss13->tss_esp2  = (u32t)tss13;
         tss13->tss_iomap = sizeof(struct tss_s);
      
         main_tss   = sys_tssalloc(tss_data, 0);
         trap08_tss = sys_tssalloc(tss08, 0);
         trap12_tss = sys_tssalloc(tss12, 0);
         trap13_tss = sys_tssalloc(tss13, 0);

         if (trap08_tss && trap12_tss && trap13_tss && main_tss) {
            int rc = 0;
            rc += sys_intgate( 8, trap08_tss);
            rc += sys_intgate(12, trap12_tss);
            rc += sys_intgate(13, trap13_tss);
            sys_settr(main_tss);
            if (rc==3) log_it(2,"task gates on\n");
         }
      }
   }
}

// update cr3 in tss segments, called just before page mode on 
void sys_tsscr3(u32t cr3) {
   if (tss_data) tss_data->tss_cr3 = cr3;
   if (tss08) tss08->tss_cr3 = cr3;
   if (tss12) tss12->tss_cr3 = cr3;
   if (tss13) tss13->tss_cr3 = cr3;
}
