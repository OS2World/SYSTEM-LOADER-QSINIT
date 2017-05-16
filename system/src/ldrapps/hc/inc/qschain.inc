;
; chain structures
;
;cc #ifndef QSINIT_CHAININC
;cc #define QSINIT_CHAININC
;cc #include "qcl/qslist.h"
;cc #include "seldesc.h"
;cc #include "qsmodint.h"

EXIT_STACK_SIZE = 128

exit_stack_s    struc
es_sign         dd      ?
es_od           dd      ?
es_ebp          dd      ?
es_userdata     dd      ?
es_address      dd      ?
es_retpoint     dd      ?
exit_stack_s    ends

ordinal_data_s  struc
;ct module*
od_handle       dd      ?
od_ordinal      dd      ?
;ct ptr_list
od_entry        dd      ?
;ct ptr_list
od_exit         dd      ?
;ct void*
od_replace      dd      ?
;ct ptr_list
od_replist      dd      ?
;ct u8t*
od_thunk        dd      ?
od_address      dd      ?
;cr struct exit_stack_s        od_exitstack[EXIT_STACK_SIZE];
od_exitstack    db      EXIT_STACK_SIZE * size exit_stack_s dup (?)
ordinal_data_s  ends

;cc typedef struct ordinal_data_s ordinal_data;

mod_chaininfo_s struc
mc_entry        dd      ?
;ct struct pushad_s *
mc_regs         dd      ?
;ct ordinal_data*
mc_orddata      dd      ?
;ct void*
mc_replace      dd      ?
mc_userdata     dd      ?
mod_chaininfo_s ends

;cc typedef struct mod_chaininfo_s mod_chaininfo;

;cc #endif // QSINIT_CHAININC
