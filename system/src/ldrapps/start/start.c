//
// QSINIT "start" module
// "main" function
//
#include "qsutil.h"
#include "memmgr.h"
#include "syslocal.h"
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
void setup_sessions(void);
void setup_tasklist(void);
void check_codepage(void);
void check_version(void);
int  get_ini_parms(void);
int  unpack_ldi(void);
void mount_vol0(void);
void setup_fio(void);
void mem_init(void);

void _std mod_main(void) {
   // init static classes and variables
   _wc_static_init();
   // query QSINIT module handle
   mh_qsinit = (u32t)mod_context()->self;
   // init global exception handler
   setup_exceptions(0);
   // init memory manager
   mem_init();
   /* init log & storage
      - must be called before mod_secondary export */
   setup_log();
   setup_storage();
   // setup vio to common way
   setup_sessions();
   // file i/o, uses storage at least, and provides system file i/o for everybody
   setup_fio();
   // export minor LE/LX loader code, missing in QSINIT
   setup_loader();
   // setup std file i/o
   setup_fileio();
   // get some parameters from ini and copy ini file to 1:
   get_ini_parms();
   if (!unpack_ldi()) exit_pm32(QERR_NOEXTDATA);
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
   // try to mount boot volume
   mount_vol0();
   // install ctrl-esc hotkey
   setup_tasklist();
   // left shift was pressed (safe mode)
   if (hlp_insafemode()) {
      cmd_state cst;
      log_printf("skipping start.cmd\n");
      cmd_exec("b:\\safemode.cmd",0);
   } else {
      if (hlp_hosttype()==QSHT_BIOS) setup_exceptions(1);
      key_speed(0,0);
      // check for codepage, inherited via RESTART
      check_codepage();
      cmd_exec("b:\\start.cmd",0);
   }
   /* WARNING! START module is never released by QSINIT, so exported code
      CAN be called after this point! */
   log_printf("exiting start\n");
}

/* empty LibMain, because it called too early to init START things.
   _wc_static_init called by mod_main() for us,
   _wc_static_fini is never called */
unsigned __stdcall LibMain(unsigned hmod, unsigned termination) {
   return 1;
}
