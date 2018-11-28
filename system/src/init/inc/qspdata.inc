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
;cc #include "qsmodint.h"
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
piParentTID     dd      ?          ;/< parent tid (or 0 for detached)
piThreads       dd      ?          ;/< number of active threads
piExitCode      dd      ?          ;/< process exit code
;ct process_context*
piContext       dd      ?          ;/< process context
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
PFLM_NOPWAIT    equ     4          ;/< this is session start, no parent waiting
PFLM_LWAIT      equ     8          ;/< wait for launcher`s mt_waitobject() call
PFLM_CHAINEXIT  equ    10h         ;/< replacing by another module in progress (2 same PIDs active)

THRD_RUNNING    equ     0          ;/< thread is active
THRD_SUSPENDED  equ     1          ;/< suspended thread
THRD_FINISHED   equ     2          ;/< thread is exited already
THRD_WAITING    equ     3          ;/< thread is waiting for tiWaitReason
THRD_STATEMAX   equ     3

THWR_CHILDEXEC  equ     0          ;/< child process exit
THWR_TIDMAIN    equ     1          ;/< main thread waits for other active threads
THWR_TIDEXIT    equ     2          ;/< thread waits other to exit
THWR_WAITOBJ    equ     3          ;/< thread waits in mt_waitobject() func
THWR_WAITLNCH   equ     4          ;/< child waits for mt_waitobject() call by launcher

;cc // thread info
mt_threadinfo   struc
tiSign          dd      ?          ;/< header signature
tiPID           dd      ?
tiTID           dd      ?
tiSession       dd      ?
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
;ct void*
tiFirstMutex    dd      ?          ;/< first owned mutex (mutex_data*)
;ct void*
tiSigFunc       dd      ?          ;/< signal handler
;ct u32t*
tiSigQueue      dd      ?          ;/< signal queue
tiSigFiber      dd      ?          ;/< current existing signal fiber

mt_threadinfo   ends

TFLM_MAIN       equ     1          ;/< main thread (exit will cause process termination)
TFLM_EMBLIST    equ     2          ;/< list embedded into thread info memoryblock
TFLM_SYSTEM     equ     4          ;/< system thread (kill disabled)
TFLM_NOSCHED    equ     8          ;/< skip it in scheduler enumeration
TFLM_TLP        equ    10h         ;/< scheduler internal hint bit
TFLM_NOFPU      equ    20h         ;/< ignore FPU context saving
TFLM_NOTRACE    equ    40h         ;/< trace output disabled for this thread

mt_fiberstate   struc
fiSign          dd      ?          ;/< header signature
fiType          dw      ?          ;/< fiber type
fiFPUMode       dw      ?          ;/< FPU usage mode
;ct void*
fiUserData      dd      ?          ;/< fiber data ptr
;ct void*
fiStack         dd      ?          ;/< stack pointer to free it
fiStackSize     dd      ?          ;/< size of custom stack
fiXcptTop       dd      ?          ;/< exception stack top
;ct void*
fiFPURegs       dd      ?          ;/< FPU state buffer (can be 0)
fiRegs          tss_s  <?>
mt_fiberstate   ends

FIBT_MAIN       equ     0          ;/< fiber is common executing stream
FIBT_APC        equ     1          ;/< fiber is async procedure
FIBT_AVAIL      equ     2          ;/< empty slot

FIBF_EMPTY      equ     0          ;/< FPU state buffer is empty or unallocated
FIBF_FIB0       equ     1          ;/< use fiber 0 buffer (not used, actually)
FIBF_OK         equ     2          ;/< FPU state buffer is in use

;cc typedef mt_prcdata* (_std *pmt_new  )(process_context *pq, void *mtdata);
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
;* callback from switcher code when TS bit it set.
;  does not called duaring interrupt.
;ct pmt_yield
mtcb_rmcall     dd      ?
;/ !!!
mtcb_glock      dd      ?
mtcb_llcaller   dd      ?          ;/< last mt_swlock() caller addr
;ct u32t*
mtcb_pxcpttop   dd      ?
;ct process_context*
mtcb_ctxmem     dd      ?
;ct mt_thrdata*
mtcb_cth        dd      ?          ;/< current thread
mtcb_sesno      dd      ?          ;/< current session no
;ct mt_thrdata*
mtcb_cfpt       dd      ?          ;/< current FPU owner thread
mtcb_cfpf       dd      ?          ;/< current FPU thread`s fiber index
mt_proc_cb_s    ends

;cc typedef struct mt_proc_cb_s  mt_proc_cb;

MAX_TID_BITS    equ     12         ;/< max 4096 threads per process (2^12)

PREALLOC_THLIST equ     32

TLS_FAKE_PTR    equ    0FFFFFFFFh  ;///< value for used, but unallocated slot

;cc mt_thrdata* _std mt_gettcb(void);

;cc #ifdef __cplusplus
;cc }
;cc #endif
;cc #endif // QSINIT_PROCINT
