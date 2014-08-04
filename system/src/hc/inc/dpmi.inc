;
; QSINIT
; dpmi structs
;

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
