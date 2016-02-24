//
// QSINIT
// watcom executable static classes init
//

#include "qstypes.h"
#include "clib.h"
#include "vio.h"

#pragma pack(1)
typedef struct {
   u8t      type; /* near=0/far=1 routine indication also used
                     when walking table to flag completed entries */
   u8t      prio; // priority (0-highest 255-lowest)
   u32t      rtn; // routine
} rt_init;
#pragma pack()

void call_init(u32t rtn);
#pragma aux call_init = \
     "pushad   "     \
     "push   ds"     \
     "push   es"     \
     "call   eax"    \
     "pop    es"     \
     "pop    ds"     \
     "popad    "     \
     parm [eax];

extern u8t  _xib_label,_xie_label, // init records
            _yib_label,_yie_label; // fini records

static struct {
   int    done;
   u8t   *bptr, *eptr;
} _wrti[2] = {{-1,&_xib_label,&_xie_label},{-1,&_yib_label,&_yie_label}};

static u32t __stdcall _wc_static_common(int mode) {
   u32t len = (_wrti[mode].eptr - _wrti[mode].bptr)/sizeof(rt_init);
   if (_wrti[mode].done>=0) return _wrti[mode].done;

   if (len) {
      rt_init *rti = (rt_init*)_wrti[mode].bptr;
      u32t    prio = 256;
      int  ii, sel;

      do {
         sel = -1;
         for (ii=0; ii<len; ii++) {
            if (rti[ii].type) { // failed on far calls now
               vio_strout("init/term error!\n");
               return _wrti[mode].done = 0;
            }
            if (rti[ii].prio<prio) {
               prio = rti[ii].prio;
               sel  = ii;
            }
         }
         if (sel>=0 && sel<len) {
            if (rti[sel].rtn) call_init(rti[sel].rtn);
            while (++sel<len)
               if (rti[sel].prio==prio && rti[sel].rtn) call_init(rti[sel].rtn);
         }
      } while (sel>=0);
   }
   return _wrti[mode].done = 1;
}

u32t __stdcall _wc_static_init() {
   return _wc_static_common(0);
}

u32t __stdcall _wc_static_fini() {
   return _wc_static_common(1);
}
