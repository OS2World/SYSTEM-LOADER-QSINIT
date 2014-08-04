;
; QSINIT
; doshlp code - delay & fixup macroses
;

;----------------------------------------------------------------
IODelay16       macro
                local   fixp,dloop
                push    cx
                db      0B9h
fixp            label   near
                dw      0
                align   4
dloop:
                dec     cx
                jnz     dloop
                pop     cx
                IODELAY_FIXUP segment
                   dw offset fixp
                IODELAY_FIXUP ends
                endm
;----------------------------------------------------------------
IODelay32       macro
                local   fixp,dloop
                push    ecx
                db      0B9h
fixp            label   near
                dd      0
                align   4
dloop:
                dec     ecx
                jnz     dloop
                pop     ecx
                IODELAY_FIXUP segment
                   dw offset fixp
                IODELAY_FIXUP ends
                endm
;----------------------------------------------------------------

KJMPTYPE        equ     0h                                      ; kernel jmp
LDIRQBTYPE      equ     1h                                      ; irq table
PUSHFLATDSTYPE  equ     2h                                      ;
CMPFLATDSTYPE   equ     3h                                      ;
PUSHFLAT_PSSEL  equ     4h                                      ;
PUSHFLAT_HSSEL  equ     5h                                      ;
PUSHFLAT_SSSEL  equ     6h                                      ;
SETFLATDSTYPE   equ     7h                                      ;

;----------------------------------------------------------------
KJmp            macro   fname
                local   dest,retp

                db      0E9H                                    ; near relative jmp
dest            dd      0
retp            label   word

                COMMON_FIXUP segment
                   db KJMPTYPE
                   dw offset dest
                   dw DOSHLP_DATASEL
                   dw offset retp
                   db offset fname - offset AllocGDT
                COMMON_FIXUP ends
                endm
;----------------------------------------------------------------
LDIrqB          macro
                local   irqfix

                db      081h, 0C3h                              ; add ebx,value
irqfix          dd      0

                COMMON_FIXUP segment
                   db LDIRQBTYPE
                   dw offset irqfix
                   dw DOSHLP_DATASEL
                   dw 0
                   db 0
                COMMON_FIXUP ends
                endm
;----------------------------------------------------------------
CmpFlatDS       macro   fixtype
                local   cmpvalue

                db      66h                                     ; cmp ax,value
                db      3Dh                                     ;
cmpvalue        dw      0                                       ;

                COMMON_FIXUP segment
                   db fixtype
                   dw offset cmpvalue
                   dw DOSHLP_DATASEL
                   dw 0
                   db 0
                COMMON_FIXUP ends
                endm
;----------------------------------------------------------------
SetFlatDS       macro
                local   value

                db      0B8h                                    ; mov eax,value
value           dd      0

                COMMON_FIXUP segment
                   db SETFLATDSTYPE
                   dw offset value
                   dw DOSHLP_DATASEL
                   dw 0
                   db 0
                COMMON_FIXUP ends
                endm
;----------------------------------------------------------------
FixUpStruc      struc
fx_type         db      ?                                       ; operation
fx_addr         dd      ?                                       ; address
fx_data         dw      ?                                       ; data
fx_misc         db      ?                                       ; data #2 (KAT offset)
FixUpStruc      ends
