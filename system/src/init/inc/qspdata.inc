;
; QSINIT
; process/thread internal data
;

;cc #ifndef QSINIT_PROCINT
;cc #define QSINIT_PROCINT

;cc #ifdef __cplusplus
;cc extern "C" {
;cc #endif
;cc #define MODULE_INTERNAL
;cc #include "qsmod.h"

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
piThreads       dd      ?          ;/< number of active threads
piExitCode      dd      ?          ;/< process exit code

piPrio          dd      ?          ;/< 
;ct module*
piModule        dd      ?          ;/< exe file module
;ct mt_thrdata*
piList          dd      ?
piListAlloc     dd      ?          ;/< allocated size of piList

piMiscFlags     dd      ?
;ct struct mt_procinfo*
piNext          dd      ?          ;/< mtlib internal
;ct struct mt_procinfo*
piFirstChild    dd      ?          ;/< mtlib internal
mt_procinfo     ends

PFLM_SYSTEM     equ     1          ;/< system process, should never free it
PFLM_EMBLIST    equ     2          ;/< list embedded into process info block

THRD_RUNNING    equ     0          ;/< thread is active
THRD_SUSPENDED  equ     1          ;/< suspended thread
THRD_EXITING    equ     2          ;/< thread in exiting state
THRD_WAITING    equ     3          ;/< thread is waiting for tiWaitReason

THWR_CHILDEXEC  equ     0          ;/< child process exit

;cc // thread info
mt_threadinfo   struc
tiSign          dd      ?          ;/< header signature
tiPID           dd      ?
tiTID           dd      ?
;ct mt_prcdata*
tiParent        dd      ?
tiFibers        dd      ?          ;/< number of fibers
tiExitCode      dd      ?          ;/< thread exit code

tiTime          dq      ?          ;/< total execution time (ms)
tiLastTime      dq      ?          ;/< last executed (tics)

tiState         dw      ?
tiWaitReason    dw      ?
;ct void*
tiUserData      dd      ?          ;/< user data ptr
;ct mt_fibdata*
tiList          dd      ?          ;/< thread fibers
tiListAlloc     dd      ?          ;/< allocated size of tiList (1 by default)

tiMiscFlags     dd      ?

mt_threadinfo   ends

TFLM_MAIN       equ     1          ;/< main thread (exit will cause process termination)
TFLM_EMBLIST    equ     2          ;/< list embedded into thread info memoryblock


mt_fiberstate   struc
fiSign          dd      ?          ;/< header signature
fiType          dd      ?          ;/< fiber flags
;ct void*
fiRegisters     dd      ?          ;/< registers (block bounded to this struct)
;ct void*
fiUserData      dd      ?          ;/< fiber data ptr

mt_fiberstate   ends

FIBT_MAIN       equ     0          ;/< fiber is common exectuin stream
FIBT_APC        equ     1          ;/< fiber is async procedure


;cc typedef mt_prcdata* (_std *pmt_new )(process_context *pq);
;cc typedef u32t        (_std *pmt_exec)(process_context *pq);
;cc typedef void        (_std *pmt_exit)(process_context *pq);
;cc typedef struct {
;cc    pmt_new      init;
;cc    pmt_exec     exec;
;cc    pmt_exit     fini;
;cc } mt_proc_cbext;
;cc
;cc void _std mt_setextcb(mt_proc_cbext *ecb);
;cc 

;cc #ifdef __cplusplus
;cc }
;cc #endif
;cc #endif // QSINIT_PROCINT
