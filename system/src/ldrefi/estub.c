//
// QSINIT
// EFI loader - some stubs for missing code
//

#include "clib.h"
#include "dpmi.h"
#include "qslog.h"
#include "qsutil.h"
#include "qsint.h"
#include "qsinit.h"
#define STORAGE_INTERNAL
#include "qsstor.h"
#include "ioint13.h"
#include "qecall.h"
#include "seldesc.h"
#include "../ldrapps/hc/qssys.h"

/* this variables located in 16-bit portion of QSINIT for some reasons,
   so need to duplicate it here */
u64t           page0_fptr;
u32t             BaudRate = 115200;
u16t          ComPortAddr = 0;
stoinit_entry   storage_w[STO_BUF_LEN];
struct Disk_BPB   BootBPB; // empty BPB (for now)
extern u32t         sel64;

void get_idt(struct lidt64_s *idt);
#ifdef __WATCOMC__
#pragma aux get_idt = "sidt [eax]" parm [eax];
#endif

int _std sys_intstate(int on);

void memmove64(u64t dst, u64t src, u64t length, int usecli) {
   call64(EFN_MEMMOVE, 7, 4, dst, src, length, usecli);
}

int _std hlp_fddline(u32t disk) {
   return -1;
}

u32t __cdecl hlp_rmcall(u32t rmfunc, u32t dwcopy, ...) {
   log_printf("Unsupported RM call of %04X:%04X!\n", rmfunc>>16, rmfunc&0xFFFF);
   return 0;
}

u32t __cdecl hlp_rmcallreg(int intnum, rmcallregs_t *regs, u32t dwcopy, ...) {
   log_printf("Unsupported RM call int %i/%04X:%04X\n", intnum, regs->r_cs, regs->r_ip);
   return 0;
}

u32t _std hlp_querybios(u32t index) {
   switch (index) {
      // simulate "APM not present" error code
      case QBIO_APM: return 0x10086;
      /* query PCI BIOS (B101h), return bx<<16|cl<<8| al or 0 if no PCI.
         PCI scan code in START take in mind EFI host and make full scan of
         all 256 buses, instead of reading last bus from here */
      case QBIO_PCI: return 0;
   }
   return 0;
}

u32t _std exit_poweroff(int suspend) {
   if (!suspend) call64(EFN_EXITPOWEROFF, 0, 1, EFI_SHUTDOWN);
   return 0;
}

void _std make_reboot(int warm) {
   call64(EFN_EXITPOWEROFF, 0, 1, warm?EFI_RESETWARM:EFI_RESETCOLD);
}

u32t _std sys_getint(u8t vector, u64t *addr) {
   struct lidt64_s    idt;
   u32t    vectoroffs, rc;
   int           irqstate;
   struct gate64_s  *id, buf;

   if (!addr) return 0;

   memset(&idt, 0, sizeof(struct lidt64_s));
   get_idt(&idt);

   vectoroffs = vector*sizeof(struct gate64_s);
   // idt limit < 256???
   if (vectoroffs + sizeof(struct gate64_s) - 1 > idt.lidt64_limit) return 0;

   if (idt.lidt64_base + vectoroffs >= _4GBLL) {
      // IDT above 4Gb? who can made this? still try to support
      memmove64((u32t)&buf, idt.lidt64_base + vectoroffs, sizeof(struct gate64_s), 1);
      id = &buf;
   } else
      id = (struct gate64_s *)idt.lidt64_base + vector;

   irqstate = sys_intstate(0);
   rc       = id->g64_access&3;
   *addr    = (u64t)id->g64_ofs32<<32 | (u64t)id->g64_ofs16<<16 | id->g64_ofs0;
   sys_intstate(irqstate);
   return rc;
}

int _std sys_setint(u8t vector, u64t *addr, u32t type) {
   struct lidt64_s  idt;
   u32t      vectoroffs;
   int         irqstate;
   u64t              av;
   struct gate64_s   id;

   if (type==SINT_TASKGATE || type>SINT_TRAPGATE || !addr) return 0;
   // force it to int gate here (BIOS host makes real detection, nevertheless)
   if (type==0) type = SINT_INTGATE;

   memset(&idt, 0, sizeof(struct lidt64_s));
   get_idt(&idt);

   vectoroffs = vector*sizeof(struct gate64_s);
   // idt limit < 256???
   if (vectoroffs + sizeof(struct gate64_s) - 1 > idt.lidt64_limit) return 0;

   av = *addr;
   id.g64_sel      = sel64;
   id.g64_ofs0     = av;
   id.g64_ofs16    = av>>16;
   id.g64_ofs32    = av>>32;
   id.g64_reserved = 0;
   id.g64_parms    = 0;
   id.g64_access   = D_PRES | D_INTGATE32+type-SINT_INTGATE;

   if (idt.lidt64_base + vectoroffs >= _4GBLL) {
      // IDT above 4Gb? who can made this? still try to support
      memmove64(idt.lidt64_base + vectoroffs, (u32t)&id, sizeof(struct gate64_s), 1);
   } else {
      struct gate64_s *idp = (struct gate64_s *)idt.lidt64_base + vector;
      // cli and copy
      int irqstate = sys_intstate(0);
      memcpy(idp, &id, sizeof(struct gate64_s));
      sys_intstate(irqstate);
   }
   return 1;
}

u32t _std sys_tmirq32(u32t lapicaddr, u8t tmrint, u8t sprint) {
   return 0;
}

