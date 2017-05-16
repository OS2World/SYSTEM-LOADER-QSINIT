//
// QSINIT
// internal loader part defines (not available for other code)
//
#ifndef QSINIT_LOCAL_HEADER
#define QSINIT_LOCAL_HEADER

#include "qsmodint.h"
#include "qsbase.h"
#include "qsinit.h"
#include "qspdata.h"
#include "qsstor.h"
#include "qserr.h"
#include "qsint.h"


void  _std usleep      (u32t usec);
void  _std mt_yield    (void);
u64t  _std hlp_tscread (void);
int   _std sys_intstate(int on);

int   _std tolower     (int cc);
u32t  _std strlen      (const char *str);
u32t  _std wcslen      (const u16t *str);
u16t       ff_convert  (u16t chr, unsigned int dir);

void       mem_procexit(process_context *pq);
u32t       hlp_fopen2  (const char *name, int allow_pxe);

/// secondary function table, from "start" module
extern mod_addfunc *mod_secondary; 
extern volatile 
       mt_proc_cb    mt_exechooks;
extern vol_data*           extvol;
/// disk buffer address
extern u32t             DiskBufPM;
/// boot partition disk i/o is available
extern u8t           bootio_avail,
                         pxemicro,
                         streamio,
                         safeMode;

extern u16t           ComPortAddr;
extern u32t              BaudRate;

#ifndef EFI_BUILD
/// boot flags
extern u8t           dd_bootflags,
                      dd_bootdisk;
#endif

/// get current thread data
mt_thrdata* _std mt_curthread(void);

#endif // QSINIT_LOCAL_HEADER
