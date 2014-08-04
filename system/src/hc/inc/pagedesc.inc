;cc #ifndef QS_BASE_PAGEDESC
;cc #define QS_BASE_PAGEDESC

;
; Pages & other
; note: missing flags has the same values in PT_*
;

; CR3 32-bit fields
CR3_PWT         equ     00000008h   ; write-through
CR3_CDIS        equ     00000010h   ; cache disable
CR3_ADDRSHL     equ     12

CR3PAE_PDPSHL   equ     5           ; page-directory-pointer addr shift

PD4M_HIADDRSHL  equ     13          
PD4M_HIADDRMASK equ     003FE000h   ; bits 32..x of page physical address
PD4M_ADDRSHL    equ     22
PD4M_ADDRMASK   equ    0FFC00000h   ; bits 22..31 of page physical address
PD4M_MASK       equ     003FFFFFh

PD2M_ADDRSHL    equ     21
PD2M_MASK       equ     001FFFFFh

PDPAE_XDSHL     equ     63          ; execute-disable bit shift

PD_PAT          equ     00001000h
PD_BIGPAGE      equ     00000080h

PT_PRESENT      equ     00000001h
PT_WRITE        equ     00000002h
PT_R3ACCESS     equ     00000004h
PT_PWT          equ     00000008h
PT_CDIS         equ     00000010h
PT_ACCESSED     equ     00000020h
PT_DIRTY        equ     00000040h
PT_PAT          equ     00000080h
PT_GLOBAL       equ     00000100h

PT_ADDRMASK     equ    0FFFFF000h
PT_ADDRSHL      equ     12
PT_ADDRINCR     equ     (1 shl PT_ADDRSHL)

;cc #endif // QS_BASE_PAGEDESC
