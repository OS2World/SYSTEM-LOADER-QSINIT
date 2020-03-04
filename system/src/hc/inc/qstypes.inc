

PAGESIZE        equ     4096                                    ;
PAGESHIFT       equ     12                                      ;
PAGEMASK        equ     4095                                    ;

PARASHIFT       equ     4                                       ;
PARASIZE        equ     (1 SHL PARASHIFT)                       ;
PARAMASK        equ     (PARASIZE - 1)                          ;
SECTSHIFT       equ     9                                       ;
SECTSIZE        equ     (1 SHL SECTSHIFT)                       ;
SECTMASK        equ     (SECTSIZE - 1)                          ;

_1KB            equ     (1   *1024)                             ;
_2KB            equ     (2   *1024)                             ;
_4KB            equ     (4   *_1KB)                             ;
_64KB           equ     (64  *_1KB)                             ;
_1MB            equ     (_1KB*_1KB)                             ;
_2MB            equ     (_2KB*_1KB)                             ;
_16MB           equ     (16  *_1MB)                             ;
_512MB          equ     (512 *_1MB)                             ;
_4GB            equ     (0)                                     ;

qw_rec          struc                                           ;
qw_low          dd      ?                                       ;
qw_high         dd      ?                                       ;
qw_rec          ends                                            ;



;----------------------------------------------------------------
JmpF16          macro   callseg,callofs                         ; jmp to label
                db      0EAh                                    ;
                dw      offset callofs                          ;
                dw      callseg                                 ;
                endm                                            ;
;----------------------------------------------------------------
JmpF16a         macro   callseg,callofs                         ; jmp to abs addr
                db      0EAh                                    ;
                dw      callofs                                 ;
                dw      callseg                                 ;
                endm                                            ;
;----------------------------------------------------------------
CallF16         macro   callseg,callofs                         ;
                db      9Ah                                     ;
                dw      offset callofs                          ;
                dw      callseg                                 ;
                endm                                            ;
;----------------------------------------------------------------
CallF16f        macro   func                                    ; far func
                db      9Ah                                     ;
                dd      func                                    ;
                endm                                            ;
;----------------------------------------------------------------
CallF32         macro   callseg,callofs                         ;
                db      9Ah                                     ;
                dd      offset callofs                          ;
                dw      callseg                                 ;
                endm                                            ;
;----------------------------------------------------------------
CallF32w        macro   callseg,callofs                         ;
                db      66h                                     ;
                db      9Ah                                     ;
                dd      offset callofs                          ;
                dw      callseg                                 ;
                endm                                            ;
;----------------------------------------------------------------
Seg_FS          macro                                           ;
                db      64h                                     ;
                endm                                            ;
;----------------------------------------------------------------
Seg_GS          macro                                           ;
                db      65h                                     ;
                endm                                            ;
;----------------------------------------------------------------
Seg_SS          macro                                           ;
                db      36h                                     ;
                endm                                            ;
;----------------------------------------------------------------
Seg_DS          macro                                           ;
                db      3Eh                                     ;
                endm                                            ;
;----------------------------------------------------------------
Seg_ES          macro                                           ;
                db      26h                                     ;
                endm                                            ;
;----------------------------------------------------------------
Seg_CS          macro                                           ;
                db      2Eh                                     ;
                endm                                            ;
;----------------------------------------------------------------
