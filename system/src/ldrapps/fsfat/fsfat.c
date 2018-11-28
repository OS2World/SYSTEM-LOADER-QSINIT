#include "fsfat.h"
#include "qsmodext.h"

vol_data *_extvol = 0;

unsigned __cdecl LibMain(unsigned hmod, unsigned termination) {
   if (!termination) {
      if (mod_query(mod_getname(hmod,0), MODQ_LOADED|MODQ_NOINCR)) {
         log_printf("%s already loaded!\n", mod_getname(hmod,0));
         return 0;
      }
      _extvol = (vol_data*)sto_data(STOKEY_VOLDATA);
      if (!_extvol) {
         log_printf("missing extvol value!\n");
         return 0;
      }
      // register file system
      if (!register_fatio()) return 0;
      // set system flag on self to prevent unload
      ((module*)hmod)->flags |= MOD_SYSTEM;
   }
   return 1;
}
