//
// QSINIT "extcmd" module
// Optional system API and shell commands, loaded on call.
// 
#include "stdlib.h"
#include "qslog.h"
#include "qsmodext.h"
#include "qsshell.h"

void exi_register_vh_tty(void);
void exi_register_extcmd(void);

unsigned __cdecl LibMain(unsigned hmod, unsigned termination) {
   if (!termination) {
      if (mod_query(mod_getname(hmod,0), MODQ_LOADED|MODQ_NOINCR)) {
         log_printf("%s already loaded!\n", mod_getname(hmod,0));
         return 0;
      }
      cmd_shelladd("MEM"   , shl_mem);
      cmd_shelladd("MTRR"  , shl_mtrr);
      cmd_shelladd("LOG"   , shl_log);
      cmd_shelladd("PCI"   , shl_pci);
      cmd_shelladd("PS"    , shl_ps);
      cmd_shelladd("MD5"   , shl_md5);
      cmd_shelladd("SETINI", shl_setini);
      cmd_shelladd("SETKEY", shl_setkey);

      // register serial port vio handler
      exi_register_vh_tty();
      // and qs_extcmd class
      exi_register_extcmd();
      // set system flag on self to prevent unload
      ((module*)hmod)->flags |= MOD_SYSTEM;
   }
   return 1;
}
