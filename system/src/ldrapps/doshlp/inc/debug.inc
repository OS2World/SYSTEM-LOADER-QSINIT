;
; QSINIT
; doshlp code - debug macroses
;

_TEXT           segment
                extrn   _printf16:near                          ; 
_TEXT           ends

ifndef DOSHLP_CODESEL
                include doshlp.inc
endif

; 16 bit print (prn16.asm)
; dual mode (RM/PM), for both parts (resident & discardable),
; cannot print ebp ;)
dbg16print      macro   FmtStr,args                             ; do not need to save regs here
local           FmtString, BeginPar                             ; because _printf16 preserve all
                push    bp                                      ;
                mov     bp,sp                                   ; saving stack ptr in bp
                jmp     BeginPar                                ;
FmtString:      
                db      &FmtStr,0                               ; format string
BeginPar:
                irp y,<args>                                    ; pushing args
                    ifidni <y>,<bp>                             ;
                        push  word ptr [bp]                     ; push original bp
                    else                                        ; from stack (but ebp will fail)
                        push  y                                 ;
                    endif                                       ;
                endm                                            ;
                push    cs                                      ;
                push    offset FmtString                        ;
                CallF16 DOSHLP_CODESEL,_printf16                ;
                mov     sp,bp                                   ; restore stack
                pop     bp                                      ;
                endm

; write char to the top left screen corner (RM only)
im_alive_rm     macro   value                                   ;
                push    es                                      ;
                push    ax                                      ;
                mov     ax,0B800h                               ;
                mov     es,ax                                   ;
                mov     ah,0Ch                                  ;
                mov     al,<value>                              ;
                mov     es:[0],ax                               ;
                pop     ax                                      ;
                pop     es                                      ;
                endm

; qsinit macro, used in disk i/o
save_dword      macro   storage_key,value                       ;
                endm
