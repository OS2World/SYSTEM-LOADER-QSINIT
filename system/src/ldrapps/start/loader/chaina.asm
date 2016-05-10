;
; QSINIT
; chain (hook) calls
;
                include inc/basemac.inc 
                include inc/qstypes.inc
                include inc/seldesc.inc
                include inc/qschain.inc

                .486

CODE32          segment para public USE32 'CODE'
                assume cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT

                extrn   _thunk_call :near                       ;
                extrn   _thunk_panic:near                       ;
                extrn   _mt_swlock:near                         ;
                extrn   _mt_swunlock:near                       ;
                extrn   _sys_exfunc4:near                       ;
                extrn   _log_printf :near                       ;

EXIT_SIGN       = 6B6F6F48h                                     ; Hook

; call interception thunk
; --------------------------------------------------------------
; entry:  eax - ordinal_data*
;       stack:  parameters
;               caller eip
;               eax         <- esp
;
                public  _chain_entry
_chain_entry    label   near
                call    _mt_swlock                              ;
                cmp     [eax].od_entry,0                        ;
                push    [eax].od_replace                        ; @@mc_replace
                push    0                                       ; @@mc_userdata
                jz      @@chain_noentry                         ;
                pushad                                          ;
                mov     ecx,[esp+size pushad_s]                 ; update eax &
                mov     [esp].pa_eax,ecx                        ; esp to real
                add     [esp].pa_esp,12                         ; values
                mov     ebp,esp                                 ;
                sub     esp,size mod_chaininfo_s                ;
                mov     edi,eax                                 ; ordinal_data*
                mov     ecx,[eax].od_replace                    ;

                mov     [esp].mc_entry,1                        ;
                mov     [esp].mc_regs,ebp                       ; call entry
                mov     [esp].mc_orddata,edi                    ; list processor
                mov     [esp].mc_replace,ecx                    ;
                mov     [esp].mc_userdata,0                     ;
                push    esp                                     ;
                call    _thunk_call                             ;
                mov     eax,[ebp-size mod_chaininfo_s].mc_replace
                mov     ecx,[ebp-size mod_chaininfo_s].mc_userdata
                mov     esp,ebp                                 ;
                mov     [esp+size pushad_s],ecx                 ;
                mov     [esp+size pushad_s+4],eax               ;
                mov     [ebp].pa_eax,edi                        ; preserve eax &
                sub     [ebp].pa_esp,12                         ; saves in stack
                popad
@@chain_noentry:
                cmp     [eax].od_exit,0                         ;
                jnz     @@chain_withexit                        ;
@@chain_skipexit:
                cmp     dword ptr [esp+4],0                     ;
                jnz     @@chain_call_mc                         ;
                add     esp,8                                   ;
                mov     eax,[eax].od_address                    ; original addr
                jmp     @@chain_call                            ;
@@chain_call_mc:
                pop     eax                                     ; skip userdata
                pop     eax                                     ; mc_replace addr
@@chain_call:
                xchg    eax,[esp]                               ; and eax
                call    _mt_swunlock                            ;
                ret                                             ; goto function
@@chain_withexit:
                push    edi                                     ;
                push    ecx                                     ;
                mov     ecx,EXIT_STACK_SIZE                     ;
                lea     edi,[eax].od_exitstack                  ;
@@chain_srchloop:
                cmp     dword ptr [edi],0                       ; searching for
                jz      @@chain_srchdone                        ; empty stack
                add     edi,size exit_stack_s                   ; entry 
                loop    @@chain_srchloop                        ;
                pop     ecx                                     ; not found?
                pop     edi                                     ; panic to log
                pushad                                          ; and call
                push    0                                       ; without exit
                push    eax                                     ; chain
                call    _thunk_panic                            ;
                popad                                           ;
                jmp     @@chain_skipexit                        ;
@@chain_srchdone:
                mov     [edi].es_sign,EXIT_SIGN                 ;
                mov     [edi].es_od,eax                         ;
                mov     [edi].es_ebp,ebp                        ;
                mov     ebp,edi                                 ;
                pop     ecx                                     ;
                pop     edi                                     ;
                pop     [ebp].es_userdata                       ;
                mov     eax,[eax].od_address                    ;
                cmp     dword ptr [esp],0                       ; @@mc_replace==0?
                jz      @@chain_call2_rep                       ;
                mov     eax,[esp]                               ;
@@chain_call2_rep:
                mov     [ebp].es_address,eax                    ;
                pop     eax                                     ; skip @@mc_replace
                pop     eax                                     ; restore eax
                pop     [ebp].es_retpoint                       ;
                push    offset @@chain_calldone                 ;
                call    _mt_swunlock                            ;
                jmp     near ptr [ebp].es_address               ;
@@chain_calldone:
                push    ebp                                     ;
                pushfd                                          ;
                cmp     [ebp].es_sign,EXIT_SIGN                 ;
                jnz     @@chain_realpanic                       ;
                pushad                                          ;
                mov     edi,esp                                 ;
                sub     esp,size mod_chaininfo_s                ;
                mov     [esp].mc_entry,0                        ;
                mov     [esp].mc_regs,edi                       ;
                mov     eax,[ebp].es_od                         ;
                mov     [esp].mc_orddata,eax                    ;
                mov     eax,[ebp].es_address                    ;
                mov     [esp].mc_replace,eax                    ;
                mov     eax,[ebp].es_userdata                   ;
                mov     [esp].mc_userdata,eax                   ;
                mov     eax,[ebp].es_ebp                        ; user`s ebp
                mov     [edi].pa_ebp,eax                        ;
                mov     eax,[ebp].es_retpoint                   ; ret eip
                mov     [edi+size pushad_s+4],eax               ;
                mov     [ebp].es_sign,0                         ;
                call    _mt_swlock                              ;
                push    esp                                     ;
                call    _thunk_call                             ;
                mov     esp,edi                                 ;
                call    _mt_swunlock                            ;
                popad                                           ;
                popfd                                           ;
                ret                                             ;

@@fname         db      'chaina.asm',0                          ;
@@chain_realpanic:
;  really bad - we`re do not know point to return here
                pushad                                          ;
                push    1                                       ;
                push    0                                       ;
                call    _thunk_panic                            ;
                popad                                           ;
                popfd                                           ;
                pop     ebp                                     ;
; generate exception with all registers, flags & esp preserved
                push    144                                     ; line 
                push    offset @@fname                          ; filename
                push    0FFFDh                                  ; xcpt_hookerr
                call    _sys_exfunc4                            ; _throw_ it

CODE32          ends
                end
