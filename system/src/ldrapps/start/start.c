//
// QSINIT "start" module
// "main" function
//
#include "stdlib.h"
#include "qsutil.h"
#include "memmgr.h"
#include "internal.h"
#include "malloc.h"
#include "qsshell.h"
#include "vio.h"

void setup_exceptions(int stage);
void setup_loader(void);
void setup_shell(void);
void setup_log(void);
void setup_cache(void);
void setup_memory(void);
void setup_fileio(void);
void setup_storage(void);
int  get_ini_parms(void);
void done_ini(void);

void main(int argc, char *argv[]) {
   // init global exception handler
   setup_exceptions(0);
   // init memory manager
   memInit();
   // init log & storage, must be called before mod_secondary export
   setup_log();
   setup_storage();
   // export minor LE/LX loader code, missing in QSINIT
   setup_loader();
   // setup std file i/o 
   setup_fileio();
   // delete self from ramdisk to save space
   unlink(argv[0]);
   // get some parameters from ini and copy ini file to 1:
   get_ini_parms();
   // advanced setup of system memory
   setup_memory();
   // call some internals
   setup_cache();
   // install shell commands
   setup_shell();
   // check left shift state
   if (hlp_insafemode()) {
      cmd_state cst;
      log_printf("skipping start.cmd\n");
      key_waithlt(0);
      cmd_exec("1:\\safemode.cmd",0);
   } else {
      setup_exceptions(1);
      key_speed(0,0);
      cmd_exec("1:\\start.cmd",0);
   }
   // clearing ini file cache
   done_ini();
   /* WARNING! start module does not freed by QSINIT, so exported code CAN be
      called after this point! */
   log_printf("exiting start\n");
}
