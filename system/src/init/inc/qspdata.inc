;
; QSINIT
; process/thread internal data
;

                   include inc/seldesc.inc

;cc #ifndef QSINIT_PROCINT
;cc #define QSINIT_PROCINT

;cc #ifdef __cplusplus
;cc extern "C" {
;cc #endif
;cc #define MODULE_INTERNAL
;cc #include "qsmod.h"
;cc #include "seldesc.h"

PROCINFO_SIGN   equ     72705351h  ; QSpr
THREADINFO_SIGN equ     68745351h  ; QSth
FIBERSTATE_SIGN equ     62665351h  ; QSfb

;cc typedef struct mt_procinfo   mt_prcdata;
;cc typedef struct mt_threadinfo mt_thrdata;
;cc typedef struct mt_fiberstate mt_fibdata;

;cc // process info
mt_procinfo     struc
piSign          dd      ?          ;/< header signature
piPID           dd      ?          ;/< pid
piParentPID     dd      ?          ;/< parent pid
piParentTID     dd      ?          ;/< tid, who waits for this process
piThreads       dd      ?          ;/< number of active threads
piExitCode      dd      ?          ;/< process exit code
;ct process_context*
piContext       dd      ?          ;/< process context

piPrio          dd      ?          ;/<
;ct module*
piModule        dd      ?          ;/< exe file module
;ct mt_thrdata**
piList          dd      ?
piListAlloc     dd      ?          ;/< allocated size of piList

piTLSSize       dd      ?          ;/< size of TLS array

piMiscFlags     dd      ?
;ct struct mt_procinfo*
piNext          dd      ?          ;/< next child of piParent
;ct struct mt_procinfo*
piFirstChild    dd      ?          ;/< first child
;ct struct mt_procinfo*
piParent        dd      ?          ;/< parent process
;ct struct mt_procinfo*
piTmpWalk       dd      ?          ;/< temp var for tree walking
piReserved      db      3 dup (?)  ;/< align to 4
piCurDrive      db      ?
;ct char*
piCurDir        dd      26 dup (?) ;/< a..z, even if now it is a..j
mt_procinfo     ends

PFLM_SYSTEM     equ     1          ;/< system process, should never free it
PFLM_EMBLIST    equ     2          ;/< list embedded into process info block

THRD_RUNNING    equ     0          ;/< thread is active
THRD_SUSPENDED  equ     1          ;/< suspended thread
THRD_FINISHED   equ     2          ;/< thread is exited already
THRD_WAITING    equ     3          ;/< thread is waiting for tiWaitReason
THRD_STATEMAX   equ     3

THWR_CHILDEXEC  equ     0          ;/< child process exit
THWR_TIDMAIN    equ     1          ;/< main thread waits for other active threads
THWR_TIDEXIT    equ     2          ;/< thread waits other to exit
THWR_WAITOBJ    equ     3          ;/< thread waits in mt_waitobject() func

;cc // thread info
mt_threadinfo   struc
tiSign          dd      ?          ;/< header signature
tiPID           dd      ?
tiTID           dd      ?
;ct mt_prcdata*
tiParent        dd      ?
tiFibers        dd      ?          ;/< number of active fibers
tiFiberIndex    dd      ?          ;/< current executing fiber (0..x)
tiExitCode      dd      ?          ;/< thread exit code

tiTime          dq      ?          ;/< total execution time (100 mks)
tiLastTime      dq      ?          ;/< last executed (tics)

;ct void*
tiCbStart       dd      ?          ;/< start callback
;ct void*
tiCbExit        dd      ?          ;/< exit callback
;ct void*
tiCbTerm        dd      ?          ;/< termination callback

;ct u64t**
tiTLSArray      dd      ?          ;/< TLS array

tiState         dw      ?
tiWaitReason    dw      ?
tiWaitHandle    dd      ?          ;/< value depends on tiWaitReason
;ct void*
tiUserData      dd      ?          ;/< user data ptr
;ct mt_fibdata*
tiList          dd      ?          ;/< thread fibers
tiListAlloc     dd      ?          ;/< allocated size of tiList (1 by default)

tiMiscFlags     dd      ?

mt_threadinfo   ends

TFLM_MAIN       equ     1          ;/< main thread (exit will cause process termination)
TFLM_EMBLIST    equ     2          ;/< list embedded into thread info memoryblock
TFLM_SYSTEM     equ     4          ;/< system thread (kill disabled)
TFLM_NOSCHED    equ     8          ;/< skip it in scheduler enumeration

mt_fiberstate   struc
fiSign          dd      ?          ;/< header signature
fiType          dd      ?          ;/< fiber flags
;ct void*
fiUserData      dd      ?          ;/< fiber data ptr
;ct void*
fiStack         dd      ?          ;/< stack pointer to free it
fiStackSize     dd      ?          ;/< size of custom stack
fiXcptTop       dd      ?          ;/< exception stack top
fiRegisters     tss_s  <?>
mt_fiberstate   ends

FIBT_MAIN       equ     0          ;/< fiber is common exectuin stream
FIBT_APC        equ     1          ;/< fiber is async procedure
FIBT_AVAIL      equ     2          ;/< empty slot

;cc typedef mt_prcdata* (_std *pmt_new  )(process_context *pq);
;cc typedef u32t        (_std *pmt_exec )(process_context *pq);
;cc typedef u32t        (_std *pmt_exit )(process_context *pq);
;cc typedef void        (_std *pmt_yield)(void);

;/ shared internal data for MTLIB module
mt_proc_cb_s    struc
;ct pmt_new
mtcb_init       dd      ?
;ct pmt_exec
mtcb_exec       dd      ?
;ct pmt_exit
mtcb_fini       dd      ?
;ct pmt_yield
mtcb_yield      dd      ?
;/ !!!
mtcb_glock      dd      ?
mtcb_llcaller   dd      ?          ;/< last mt_swlock() caller addr
;ct u32t*
mtcb_pxcpttop   dd      ?
;ct process_context*
mtcb_ctxmem     dd      ?
mtcb_ctid       dd      ?          ;/< current thread (pid in mtcb_ctxmem)
mt_proc_cb_s    ends

;cc typedef struct mt_proc_cb_s  mt_proc_cb;

MAX_TID_BITS    equ     12         ;/< max 4096 threads per process (2^12)

PREALLOC_THLIST equ     32

;cc #ifdef __cplusplus
;cc }
;cc #endif
;cc #endif // QSINIT_PROCINT
