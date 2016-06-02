//
// QSINIT
// partition management dll
//
#include "stdlib.h"
#include "qsutil.h"
#include "qslog.h"
#include "qsmod.h"
#include "qssys.h"
#include "memmgr.h"
#include "qsshell.h"
#include "pscan.h"

int        lib_ready = 0; // lib is ready
const char *selfname = MODNAME_DMGR;
// free partition list data
void scan_free(void);
void set_hooks(void);
void remove_hooks(void);
void lvm_buildcrc(void);

void _std on_exit(sys_eventinfo *info) {
   if (!lib_ready) return;
   cmd_shellrmv("FORMAT",shl_format);
   cmd_shellrmv("UMOUNT",shl_umount);
   cmd_shellrmv("MOUNT" ,shl_mount);
   cmd_shellrmv("DMGR"  ,shl_dmgr);
   cmd_shellrmv("LVM"   ,shl_lvm);
   cmd_shellrmv("GPT"   ,shl_gpt);
   remove_hooks();
   scan_free();
   cache_free();
   lib_ready = 0;
}

unsigned __cdecl LibMain( unsigned hmod, unsigned termination ) {
   if (!termination) {
      if (mod_query(selfname,MODQ_LOADED|MODQ_NOINCR)) {
         log_printf("%s already loaded!\n",selfname);
         return 0;
      }
      sys_notifyevent(SECB_QSEXIT|SECB_GLOBAL, on_exit);
      cmd_shelladd("GPT"   ,shl_gpt);
      cmd_shelladd("LVM"   ,shl_lvm);
      cmd_shelladd("DMGR"  ,shl_dmgr);
      cmd_shelladd("MOUNT" ,shl_mount);
      cmd_shelladd("UMOUNT",shl_umount);
      cmd_shelladd("FORMAT",shl_format);
      lib_ready = 1;
      log_printf("%s is loaded!\n",selfname);
      lvm_buildcrc();
      set_hooks();
   } else {
      sys_notifyevent(0, on_exit);
      on_exit(0);
      log_printf("%s unloaded!\n",selfname);
   }
   return 1;
}
