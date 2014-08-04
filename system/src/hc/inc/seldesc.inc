;cc #ifndef QS_BASE_SELDESC
;cc #define QS_BASE_SELDESC

;
; Selectors & other
;

TABLE_MASK      equ     004h
RPL_MASK        equ     003h
RPL_CLR         equ     0FFFCh
RPL_RING0       equ     000h
RPL_RING1       equ     001h
RPL_RING2       equ     002h
RPL_RING3       equ     003h
D_PRES          equ     80h
D_NOTPRES       equ     0
D_DPL0          equ     0
D_DPL1          equ     20h
D_DPL2          equ     40h
D_DPL3          equ     60h
D_PRIV          equ     D_DPL3
D_SEG           equ     10h
; ----------------------------------------------------------
D_CTRL          equ     0
D_TSS_BUSY_BIT  equ     2h
D_GATE          equ     4h
D_32            equ     8h
D_TSS           equ     1h
D_LDT           equ     2h
; ----------------------------------------------------------
SEL_INCR        equ     8
; ----------------------------------------------------------
D_TSS_BUSY      equ     (D_TSS+D_TSS_BUSY_BIT)
D_CALLGATE      equ     (D_GATE+0 )
D_TASKGATE      equ     (D_GATE+1 )
D_INTGATE       equ     (D_GATE+2 )
D_TRAPGATE      equ     (D_GATE+3 )
D_TSS32         equ     (D_TSS+D_32)
D_TSSBUSY32     equ     (D_TSS_BUSY+D_32)
D_CALLGATE32    equ     (D_CALLGATE+D_32)
D_INTGATE32     equ     (D_INTGATE+D_32)
D_TRAPGATE32    equ     (D_TRAPGATE+D_32)
D_TYPEMASK      equ     0Fh
D_WCMASK        equ     01Fh
D_MINGATE       equ     D_CALLGATE
D_MAXGATE       equ     D_TRAPGATE32
D_TSSBUSY_CLR   equ     ( NOT D_TSS_BUSY_BIT)
; --------------------------------------------------------------
; d_access
; --------------------------------------------------------------
D_DATA          equ     0
D_CODE          equ     8h
D_CONFORM       equ     4h
D_EXPDN         equ     4h
D_RX            equ     2h
D_X             equ     0
D_W             equ     2h
D_R             equ     0
D_ACCESSED      equ     1h
D_GRAN4K        equ     80h
D_COPER32       equ     40h
D_DBIG          equ     D_COPER32
D_LONG          equ     20h
D_AVAIL         equ     10h
D_EXTLIMIT      equ     0Fh
D_1MEG          equ     100000h
D_GRANMASK      equ     0FFFFFh
; --------------------------------------------------------------
D_DATA0         equ     (D_PRES+D_DPL0+D_SEG+D_DATA+D_W)
D_CODE0         equ     (D_PRES+D_DPL0+D_SEG+D_CODE+D_RX)
D_TRAP0         equ     (D_PRES+D_DPL0+D_CTRL+D_TRAPGATE)
D_INT0          equ     (D_PRES+D_DPL0+D_CTRL+D_INTGATE)
D_TASK0         equ     (D_PRES+D_DPL0+D_CTRL+D_TASKGATE)
D_TSS0          equ     (D_PRES+D_DPL0+D_CTRL+D_TSS)
D_LDT0          equ     (D_PRES+D_DPL0+D_CTRL+D_LDT)
D_TRAP032       equ     (D_PRES+D_DPL0+D_CTRL+D_TRAPGATE32)
D_INT032        equ     (D_PRES+D_DPL0+D_CTRL+D_INTGATE32)
D_TSS032        equ     (D_PRES+D_DPL0+D_CTRL+D_TSS32)
D_DATA1         equ     (D_PRES+D_DPL1+D_SEG+D_DATA+D_W)
D_CODE1         equ     (D_PRES+D_DPL1+D_SEG+D_CODE+D_RX)
D_DATA2         equ     (D_PRES+D_DPL2+D_SEG+D_DATA+D_W)
D_CODE2         equ     (D_PRES+D_DPL2+D_SEG+D_CODE+D_RX)
D_DATA3         equ     (D_PRES+D_DPL3+D_SEG+D_DATA+D_W)
D_CODE3         equ     (D_PRES+D_DPL3+D_SEG+D_CODE+D_RX)
D_INT3          equ     (D_PRES+D_DPL3+D_CTRL+D_INTGATE)
D_TRAP3         equ     (D_PRES+D_DPL3+D_CTRL+D_TRAPGATE)
D_GATE3         equ     (D_PRES+D_DPL3+D_CTRL+D_CALLGATE)
D_INT332        equ     (D_PRES+D_DPL3+D_CTRL+D_INTGATE32)
D_GATE332       equ     (D_PRES+D_DPL3+D_CTRL+D_CALLGATE32)
D_TRAP332       equ     (D_PRES+D_DPL3+D_CTRL+D_TRAPGATE32)
; --------------------------------------------------------------

desctab_s       struc
d_limit         dw      ?                                       ; limit  0..15
d_loaddr        dw      ?                                       ; base   0..15
d_hiaddr        db      ?                                       ; base  23..16
d_access        db      ?                                       ; access
d_attr          db      ?                                       ; limit
d_extaddr       db      ?                                       ; base  31..24
desctab_s       ends

desc32_s        struc
d32_low         dd      ?
d32_high        dd      ?
desc32_s        ends

gate_s          struc
g_handler       dd      ?
g_parms         db      ?
g_access        db      ?
g_extoffset     dw      ?
gate_s          ends

g_lowoffset     equ     word ptr g_handler
g_sel           equ     word ptr g_handler + 2
; --------------------------------------------------------------

lidt_s          struc
lidt_limit      dw      ?
lidt_base       dd      ?
lidt_s          ends
; --------------------------------------------------------------
tss_s           struc
tss_backlink    dw      ?
tss_reservdbl   dw      ?
tss_esp0        dd      ?
tss_ss0         dw      ?
tss_reservdss0  dw      ?
tss_esp1        dd      ?
tss_ss1         dw      ?
tss_reservdss1  dw      ?
tss_esp2        dd      ?
tss_ss2         dw      ?
tss_reservdss2  dw      ?
tss_cr3         dd      ?
tss_eip         dd      ?
tss_eflags      dd      ?
tss_eax         dd      ?
tss_ecx         dd      ?
tss_edx         dd      ?
tss_ebx         dd      ?
tss_esp         dd      ?
tss_ebp         dd      ?
tss_esi         dd      ?
tss_edi         dd      ?
tss_es          dw      ?
tss_reservdes   dw      ?
tss_cs          dw      ?
tss_reservdcs   dw      ?
tss_ss          dw      ?
tss_reservdss   dw      ?
tss_ds          dw      ?
tss_reservdds   dw      ?
tss_fs          dw      ?
tss_reservdfs   dw      ?
tss_gs          dw      ?
tss_reservdgs   dw      ?
tss_ldt         dw      ?
tss_reservdldt  dw      ?
tss_tflags      dw      ?
tss_iomap       dw      ?
tss_s           ends

; --------------------------------------------------------------
CR0_PE          equ     00000001h
CR0_MP          equ     00000002h
CR0_EM          equ     00000004h
CR0_TS          equ     00000008h
CR0_ET          equ     00000010h
CR0_NE          equ     00000020h
CR0_WP          equ     00010000h
CR0_AM          equ     00040000h
CR0_NW          equ     20000000h
CR0_CD          equ     40000000h
CR0_PG          equ     80000000h
CR0_RESERVED    equ     1FFAFFC0h

; --------------------------------------------------------------
pushad_s        struc
pa_edi          dd      ?
pa_esi          dd      ?
pa_ebp          dd      ?
pa_esp          dd      ?
pa_ebx          dd      ?
pa_edx          dd      ?
pa_ecx          dd      ?
pa_eax          dd      ?
pushad_s        ends

;cc #endif // QS_BASE_SELDESC
