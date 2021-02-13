//
// QSINIT
// partition management dll
//
#include "stdlib.h"
#include "qsutil.h"
#include "qsmod.h"
#include "qssys.h"
#include "qsio.h"
#include "qstask.h"
#include "memmgr.h"
#include "qsshell.h"
#include "pscan.h"
#include "qcl/qsmt.h"

const char  *selfname = MODNAME_DMGR;
static qshandle  cmux = 0;
// free partition list data
void scan_free(void);
void set_hooks(void);
void remove_hooks(void);
void lvm_buildcrc(void);

void scan_lock(void) { if (cmux) mt_muxcapture(cmux); }
void scan_unlock(void) { if (cmux) mt_muxrelease(cmux); }

// mutex creation
static void _std on_mtmode(sys_eventinfo *info) {
   if (mt_muxcreate(0, "__ptmgr_mux__", &cmux) || io_setstate(cmux, IOFS_DETACHED, 1))
      log_it(0, "mutex init error!\n");
}

extern unsigned __stdcall _wc_static_init();
extern unsigned __stdcall _wc_static_fini();

unsigned __cdecl LibMain( unsigned hmod, unsigned termination ) {
   if (!termination) {
      if (mod_query(selfname,MODQ_LOADED|MODQ_NOINCR)) {
         log_printf("%s already loaded!\n",selfname);
         return 0;
      }
      // init static variables & classes
      _wc_static_init();
      // catch MT mode
      if (!mt_active()) sys_notifyevent(SECB_MTMODE|SECB_GLOBAL, on_mtmode);
         else on_mtmode(0);
      cmd_shelladd("GPT"   ,shl_gpt);
      cmd_shelladd("LVM"   ,shl_lvm);
      cmd_shelladd("DMGR"  ,shl_dmgr);
      cmd_shelladd("MOUNT" ,shl_mount);
      cmd_shelladd("UMOUNT",shl_umount);
      cmd_shelladd("FORMAT",shl_format);
      log_printf("%s is loaded!\n",selfname);
      lvm_buildcrc();
      set_hooks();
   } else {
      if (!cmux) sys_notifyevent(0, on_mtmode);
      cmd_shellrmv("FORMAT",shl_format);
      cmd_shellrmv("UMOUNT",shl_umount);
      cmd_shellrmv("MOUNT" ,shl_mount);
      cmd_shellrmv("DMGR"  ,shl_dmgr);
      cmd_shellrmv("LVM"   ,shl_lvm);
      cmd_shellrmv("GPT"   ,shl_gpt);
      remove_hooks();
      scan_free();
      cache_free();
      gptcfg_free();
      if (cmux)
         if (mt_closehandle(cmux)) log_it(2, "mutex fini error!\n");
      log_printf("%s unloaded!\n",selfname);
      // fini static variables & classes
      _wc_static_fini();
   }
   return 1;
}
