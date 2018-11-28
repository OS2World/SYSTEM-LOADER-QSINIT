;
; QSINIT
; le/lx exe module initializaion
;
                .486p

                MINIMAL_STACK = 4096

CODE32          segment byte public USE32 'CODE'                ;
                extrn   _main:near                              ;
                extrn   _strlen:near                            ;
                extrn   _vio_strout:near                        ;
                extrn   __parse_cmdline:near                    ;
                extrn   __rt_process_exit_list:near             ;
                assume  cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT      ;
start:
                mov     eax, [esp+4]                            ;
                mov     __Module, eax                           ;
                mov     eax, [esp+12]                           ;
                mov     __EnvPtr, eax                           ;
                mov     eax, [esp+16]                           ;
                mov     __CmdLine, eax                          ;
                mov     exit_esp, esp                           ;
                push    eax                                     ;
                call    _strlen                                 ;
                inc     eax                                     ;
                add     eax, __CmdLine                          ;
                mov     __CmdArgs, eax                          ;
                push    eax                                     ;
                call    _strlen                                 ; args len
                add     eax,16                                  ; 15 + 1 for \0
                and     eax,not 15                              ; round to 16 
                lea     ecx, [esp - MINIMAL_STACK]              ; check here:
                sub     ecx, eax                                ; stack - argv is
                cmp     ecx, offset __stack_start               ; > MINIMAL_STACK
                jc      args_error                              ;
                sub     esp, eax                                ;
                push    esp                                     ;
                push    __CmdLine                               ;
                call    __parse_cmdline                         ;
                push    offset __argv                           ;
                push    eax                                     ;
                call    _main                                   ;
                add     esp, 8                                  ;

                call    __rt_process_exit_list                  ;
                mov     esp, cs:exit_esp                        ;
                ret
;----------------------------------------------------------------
; _exit() for the main thread

                public  __rt__exit                              ;
__rt__exit:
                mov     eax, [esp+4]                            ;
                mov     esp, cs:exit_esp                        ;
                ret                                             ;
args_error:
                push    offset argerr_msg                       ; use vio_ instead of
                call    _vio_strout                             ; printf() here to prevent
                push    -1                                      ; of linking START
                call    __rt__exit                              ; module to all apps.
;----------------------------------------------------------------
next_random:
                mov     eax,8088405h                            ;
                mul     __RandSeed                              ;
                inc     eax                                     ;
                mov     __RandSeed,eax                          ;
                ret                                             ;

                public  _random                                 ;
_random         label   near                                    ;
                mov     ecx, [esp+4]                            ;
                call    next_random                             ;
                mul     ecx                                     ; Random * Range / 1 0000 0000h
                mov     eax,edx                                 ; 0 <= eax < Range
                ret     4                                       ;
CODE32          ends                                            ;

_DATA           segment dword public USE32 'DATA'
                public  __Module
                public  __CmdLine
                public  __CmdArgs
; this is module* struct, referenced in qsmod.h. DO NOT modify contents, this
; is a system data actually ;)
__Module        dd      0
; Original envptr, we do not modify this value.
; Runtime environment & exec functions use and modify process_context->envptr
; pointer instead of it. This value remain unchanged until process exit.
__EnvPtr        dd      0
; command line and arguments, located in _EnvPtr memory block
__CmdLine       dd      0
__CmdArgs       dd      0
; esp saved value
exit_esp        dd      0

                public  __RandSeed, __IsDll                     ;
__RandSeed      dd      1                                       ;
__IsDll         dd      0                                       ;
                extrn   __argv:dword                            ;
argerr_msg      db      "stack too small!",10,13,0              ;
_DATA           ends

STACK32         segment dword public USE32 'STACK'
                public  __stack_start
__stack_start   label   near
STACK32         ends

                end start
