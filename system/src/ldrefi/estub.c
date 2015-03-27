//
// QSINIT
// EFI loader - some stubs for missing code
//

#include "clib.h"
#include "qslog.h"
#include "qsutil.h"
#include "qsint.h"
#include "qsinit.h"
#define STORAGE_INTERNAL
#include "qsstor.h"
#include "ioint13.h"
#include "qecall.h"

/* this variables located in 16-bit portion of QSINIT for some reasons,
   so need to duplicate it here */
u64t           page0_fptr;
u32t             BaudRate = 115200;
u16t          ComPortAddr = 0;
stoinit_entry   storage_w[STO_BUF_LEN];
struct Disk_BPB   BootBPB; // empty BPB (for now)

int _std hlp_fddline(u32t disk) {
   return -1;
}

u32t __cdecl hlp_rmcall(u32t rmfunc, u32t dwcopy, ...) {
   log_printf("Unsupported RM call of %04X:%04X!\n", rmfunc>>16, rmfunc&0xFFFF);
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
