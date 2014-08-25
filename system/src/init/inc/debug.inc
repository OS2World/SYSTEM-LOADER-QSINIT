;
; QSINIT
; debug print from asm code
; ----------------------------------------
; note: 16 & 32 bit printf format are different!!!
; * 16 bit referenced in prn16.asm 
; * 32 bit in print.c 
;

TEXT16          segment
                extrn   _printf16:near                          ; 
ifndef NOSTORSAVE                                               ;
                extrn   sto_save_w:near                         ; 
endif                                                           ;
TEXT16          ends

_TEXT           segment                                         ;
                extrn   _log_it:near                            ;
_TEXT           ends                                            ;

;
; save named dword value into storage (qsstor.h)
; dual mode (rm/pm), save all except flags
;
save_dword      macro   storage_key,value                       ;
                local   KeyStr, Done
                ifdifi  <value>,<eax>                           ;
                    push    eax                                 ;
                    mov     eax,value                           ;
                endif                                           ;
                push    si                                      ;
                mov     si,offset KeyStr                        ;
                push    offset Done                             ;
                jmp     sto_save_w                              ;
KeyStr:
                db      &storage_key,0                          ; format string
Done:
                pop     si
                ifdifi  <value>,<eax>                           ;
                    pop     eax                                 ;
                endif                                           ;
                endm

;
; WARNING! This macros assume tiny model (cs==ss)
;
dbg16print      macro   FmtStr,args                             ; do not need to save regs here
                local   FmtString, StackPtr, BeginPar           ; because _printf16 preserve all
ifdef INITDEBUG                                                 ;
                _BSS16  segment
StackPtr:       
                dd      0                                       ;
                _BSS16  ends
                DATA16  segment
FmtString:      
                db      &FmtStr,0                               ; format string
                DATA16  ends

                mov     dword ptr ss:[StackPtr],esp             ; saving stack
                irp y,<args>                                    ; pushing args
                    push  y                                     ;
                endm                                            ;
                push    cs                                      ;
                push    offset FmtString                        ;
                push    cs                                      ;
                call    _printf16                               ;
                mov     esp,dword ptr ss:[StackPtr]             ; restore stack
endif ; INITDEBUG
                endm 

dbg32print      macro   level,FmtStr,args                       ;
                local   FmtString, StackPtr, BeginPar           ;
ifdef DEBUG                                                     ;
                pushf                                           ;
                pushad                                          ;
                push    ds                                      ;
                push    es                                      ;
                push    SEL32DATA                               ;
                mov     es,[esp]                                ;
                pop     ds                                      ; FLAT DS/ES
                mov     dword ptr ss:[StackPtr],esp             ; saving stack
                jmp     BeginPar                                ;
StackPtr:       
                dd      0                                       ;
FmtString:      
                db      &FmtStr,0                               ; format string
BeginPar:
                irp y,<args>                                    ; pushing args
                    push  y                                     ;
                endm                                            ;
                push    offset FmtString                        ;
                push    level                                   ;
                call    _log_it                                 ;
                mov     esp,dword ptr ss:[StackPtr]             ; restore stack
                pop     es                                      ;
                pop     ds                                      ;
                popad                                           ;
                popf                                            ;
endif ; INITDEBUG
                endm
