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
#include "memmgr.h"       // QSMEMOWNER_* constants

typedef  _std (*pf_mtpexitcb)(mt_thrdata *td, int stage);

void  _std usleep      (u32t usec);
void  _std mt_yield    (void);
u64t  _std hlp_tscread (void);
int   _std sys_intstate(int on);

u16t  _std key_read_nw (u32t *status);

int   _std tolower     (int cc);
u32t  _std strlen      (const char *str);
u32t  _std wcslen      (const u16t *str);
u16t       ff_convert  (u16t chr, unsigned int dir);
u32t       env_length  (const char *env);

void       mem_procexit(process_context *pq);
u32t       hlp_fopen2  (const char *name, int allow_pxe);
// serial port init, has no lock inside!
u32t  _std serial_init (u16t port);
void       init_host   (void);

/// secondary function table, from "start" module
extern mod_addfunc *mod_secondary; 
extern pf_mtpexitcb    mt_pexitcb;
extern volatile 
       mt_proc_cb    mt_exechooks;
extern vol_data*           extvol;
/// disk buffer address
extern u32t             DiskBufPM;
/// boot partition disk i/o is available
extern u8t           bootio_avail,
                         pxemicro,
                         streamio,
                         safeMode,
                      mfsd_noterm;

extern u16t           ComPortAddr;
extern u32t              BaudRate;

extern u32t         cvio_ttylines;
extern void             *init_env;

extern u16t                cp_num;
extern u32t              qs16base; // base addr of 16-bit part

#ifndef EFI_BUILD
/// check/reset BIOS Ctrl-Break flag in 040:0071h
int   _std check_cbreak(void);
/// boot flags
extern u8t           dd_bootflags,
                      dd_bootdisk;
extern u32t        transition_ptr;  ///< ecx value at loader start
#endif

/// get current thread data
mt_thrdata* _std mt_curthread(void);

/* - - - - - - - - - - - - - - - - - */
typedef struct {
   u32t                  sign;
   u32t                 crc32;
   u32t                length;
   u32t                  type;
} transition_data;

#define TRND_ENV                1         ///< block with env. data
#define TRND_CMDHIST            2         ///< block with shell history list
#define TRND_CODEPAGE           3         ///< codepage selected
#define TRND_LAST      0x80000000         ///< last block flag
#define TRND_SIGN      0x444E5254

#define next_trblock(td) ((transition_data*)((u8t*)(td+1) + Round16(td->length)))

#endif // QSINIT_LOCAL_HEADER
