//
// QSINIT "start" module
// "main" function
//
#include "stdlib.h"
#include "qsutil.h"
#include "memmgr.h"
#include "internal.h"
#include "qsshell.h"
#include "qsxcpt.h"
#include "vio.h"

unsigned __stdcall _wc_static_init();
void setup_exceptions(int stage);
void setup_loader(void);
void setup_shell(void);
void setup_log(void);
void setup_cache(void);
void setup_memory(void);
void setup_fileio(void);
void setup_storage(void);
void setup_hardware(void);
void check_version(void);
int  get_ini_parms(void);
void setup_fio(void);
void done_ini(void);
void mem_init(void);

void _std mod_main(void) {
   // init static classes and variables
   _wc_static_init();
   // init global exception handler
   setup_exceptions(0);
   // init memory manager
   mem_init();
   /* init log & storage
      - must be called before mod_secondary export */
   setup_log();
   setup_storage();
   // file i/o, uses storage at least, and provides system file i/o for everybody
   setup_fio();
   // export minor LE/LX loader code, missing in QSINIT
   setup_loader();
   // setup std file i/o
   setup_fileio();
   // get some parameters from ini and copy ini file to 1:
   get_ini_parms();
   /* advanced setup of system memory
      - must be after memInit() & setup_loader() (mod_secondary for QSINIT) */
   setup_memory();
   // setup some hardware (must be after setup_exceptions(), at least)
   setup_hardware();
   // call some internals
   setup_cache();
   // install shell commands
   setup_shell();
   // check version (if we`re still alive)
   check_version();
   // left shift was pressed (safe mode)
   if (hlp_insafemode()) {
      cmd_state cst;
      log_printf("skipping start.cmd\n");
      key_waithlt(0);
      cmd_exec("1:\\safemode.cmd",0);
   } else {
      if (hlp_hosttype()==QSHT_BIOS) setup_exceptions(1);
      key_speed(0,0);
      cmd_exec("1:\\start.cmd",0);
   }
   // clearing ini file cache
   done_ini();
   /* WARNING! START module is never released by QSINIT, so exported code
      CAN be called after this point! */
   log_printf("exiting start\n");
}

/* empty LibMain, because it called too early to init START things.
   _wc_static_init calling from mod_main() for us,
   _wc_static_fini is never calling */
unsigned __stdcall LibMain(unsigned hmod, unsigned termination) {
   return 1;
}
