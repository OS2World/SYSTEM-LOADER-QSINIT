#include "stdlib.h"
#include "console.h"
#include "conint.h"
#include "qsmodext.h"
#include "qsinit_ord.h"
#include "qsutil.h"
#include "vio.h"

static int hooks_on = 0;

void hook_modeset(void);

// fast emu
int _std hook_modeinfo(mod_chaininfo *mc) {
   hook_modeset();
   return 1;
}

int _std hook_modesetex(u32t cols, u32t lines) {
   return con_setmode(cols, lines, 0);
}

void con_insthooks(void) {
   if (!hooks_on) {
      u32t qsinit = mod_query(MODNAME_QSINIT, MODQ_NOINCR);
      if (qsinit) {
         mod_apichain(qsinit, ORD_QSINIT_vio_setmode, APICN_ONEXIT|APICN_FIRSTPOS, hook_modeinfo);
         mod_apichain(qsinit, ORD_QSINIT_vio_resetmode, APICN_ONEXIT|APICN_FIRSTPOS, hook_modeinfo);
         mod_apichain(qsinit, ORD_QSINIT_vio_setmodeex, APICN_REPLACE, hook_modesetex);
      }
      hooks_on = 1;
   }
}

void con_removehooks(void) {
   if (hooks_on) {
      u32t qsinit = mod_query(MODNAME_QSINIT, MODQ_NOINCR);
      if (qsinit) {
         mod_apiunchain(qsinit, ORD_QSINIT_vio_setmode, APICN_ONEXIT, hook_modeinfo);
         mod_apiunchain(qsinit, ORD_QSINIT_vio_resetmode, APICN_ONEXIT, hook_modeinfo);
         mod_apiunchain(qsinit, ORD_QSINIT_vio_setmodeex, APICN_REPLACE, hook_modesetex);
      }
      hooks_on = 0;
   }
}
