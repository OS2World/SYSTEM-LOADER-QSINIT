#include "stdlib.h"
#include "qslog.h"
#include "qsmodext.h"
#include "qsshell.h"

unsigned __cdecl LibMain(unsigned hmod, unsigned termination) {
   if (!termination) {
      if (mod_query(mod_getname(hmod,0), MODQ_LOADED|MODQ_NOINCR)) {
         log_printf("%s already loaded!\n", mod_getname(hmod,0));
         return 0;
      }
      cmd_shelladd("MEM" , shl_mem);
      cmd_shelladd("MTRR", shl_mtrr);
      cmd_shelladd("LOG" , shl_log);
      cmd_shelladd("PCI" , shl_pci);
   } else {
      cmd_shellrmv("PCI" , shl_pci);
      cmd_shellrmv("LOG" , shl_log);
      cmd_shellrmv("MTRR", shl_mtrr);
      cmd_shellrmv("MEM" , shl_mem);
   }
   return 1;
}
