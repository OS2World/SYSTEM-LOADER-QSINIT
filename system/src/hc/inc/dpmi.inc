;
; QSINIT
; dpmi structs
;

;cc #ifndef QS_BASE_DPMIDATA
;cc #define QS_BASE_DPMIDATA

rmcallregs_s    struc
r_edi           dd      ?
r_esi           dd      ?
r_ebp           dd      ?
r_reserved      dd      ?
r_ebx           dd      ?
r_edx           dd      ?
r_ecx           dd      ?
r_eax           dd      ?
r_flags         dw      ?
r_es            dw      ?
r_ds            dw      ?
r_fs            dw      ?
r_gs            dw      ?
r_ip            dw      ?
r_cs            dw      ?
r_sp            dw      ?
r_ss            dw      ?
rmcallregs_s    ends

;cc typedef struct rmcallregs_s     rmcallregs_t;

;/ bh bit for restore IRQs in DPMI 0x301/0x302 functions
FN30X_PICRESET  = 2
;/ bh bit for removing RM timer INT in DPMI 0x301/0x302 functions
FN30X_TIMEROFF  = 4

;cc #endif // QS_BASE_DPMIDATA
