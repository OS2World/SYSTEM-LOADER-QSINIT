;
; QSINIT
; common PM process
;

                .386p
                include inc/qstypes.inc
                include inc/segdef.inc
                include inc/basemac.inc
                include inc/dpmi.inc
                include inc/seldesc.inc
                NOSELDATA = 1
                include inc/debug.inc

; ---------------------------------------------------------------
_DATA           segment                                         ;
                align   4                                       ;
                public  _B8000                                  ;
                public  __0400                                  ;
                public  pm16data                                ;
_B8000          dd      0B8000h                                 ; B800 address
__0400          dd      000400h                                 ; 40 address
init32_addr     dd      offset _init32                          ; 48bit address of init32()
SELCODE32       dw      0                                       ;
pm16code        dw      0                                       ;
pm16data        dw      0                                       ;
_DATA           ends                                            ;
; ---------------------------------------------------------------
_BSS            segment                                         ;
                public  _rm_regs                                ;
                public  desc32                                  ;
                public  _pminitres                              ;
                public  SELDATA32                               ;
_rm_regs        rmcallregs_s <?>                                ; real mode call regs
SELDATA32       dw      ?                                       ; DGROUP based flat ds
desc32          desctab_s <>                                    ; descriptor data
_pminitres      db      ?                                       ;
                extrn save32_esp:dword                          ;
                extrn _rm16code:word                            ;
_BSS            ends                                            ;
; ---------------------------------------------------------------
CODE32          segment                                         ;
                extrn   _init32:near                            ;
                extrn   _exit_prepare:near                      ;
                extrn   vio_charout:near                        ;
                extrn   vio_done:near                           ;
                extrn   rmcall32:near                           ;
                extrn   rmstop:near                             ;
                public  _exit_pm32                              ;
                public  _exit_pm32s                             ;
                assume cs:CODE32,ds:DGROUP,es:DGROUP,ss:DGROUP  ;
; return from init32() here                                     ;
_exit_pm32a:
                push    eax                                     ; rc code
                jmp     _exit_pm32s                             ;
_exit_pm32:
                pop     eax                                     ; drop return addr
_exit_pm32s:
                call    _exit_prepare                           ; call exitlist
                mov     al,13                                   ;
                call    vio_charout                             ; print eol
                mov     al,10                                   ;
                call    vio_charout                             ;
                call    vio_done                                ;
                pop     ebx                                     ; rc
ifdef INITDEBUG
                dbg32print <2>,<"exiting pm, rc=%d",10>,<ebx>   ;
endif
; ---------------------------------------------------------------
                or      ebx,ebx                                 ;
                jnz     rmstop                                  ;
                mov     bl,9                                    ;
                jmp     rmstop                                  ;
CODE32          ends                                            ;
; ---------------------------------------------------------------
;
; 16 bit pm code
;

_TEXT           segment                                         ;
                assume cs:_TEXT,ds:DGROUP,es:nothing,ss:DGROUP

                extrn   vio_init:near                           ; text mode prn init/done

                public  pm16set                                 ; entry point
pm16set:                                                        ;
                mov     pm16code,cs                             ; save seg regs
                mov     pm16data,ss                             ; ss==ds
                mov     _pminitres,al                           ;
                movzx   edx,_rm16code                           ;
                shl     edx,PARASHIFT                           ; phys addr of flat
                sub     _B8000,edx                              ; B800 address
                sub     __0400,edx                              ; 40:0 address
                mov     SELCODE32,SEL32CODE                     ;
                mov     SELDATA32,SEL32DATA                     ;
                call    vio_init                                ;
ifdef INITDEBUG
                dbg16print <"hi!",10>                           ;
endif
                push    offset _exit_pm32a                      ; 32bit retn
                mov     ax,SELDATA32                            ;
                mov     es,ax                                   ; setup ds, es and ss
                mov     ds,ax                                   ;
                mov     ss,ax                                   ;
                movzx   esp,sp                                  ;
                jmp     fword ptr cs:init32_addr                ; far jmp to 32bit
_TEXT           ends                                            ;
                end
