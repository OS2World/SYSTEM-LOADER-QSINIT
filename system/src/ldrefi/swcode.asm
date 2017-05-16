                include inc/qstypes.inc                         ;
                include inc/segdef.inc                          ;
                include inc/qspdata.inc                         ;
                include inc/cpudef.inc                          ;
                include inc/efnlist.inc                         ;
                .686p

_DATA           segment
                public  _aboutstr                               ;
                db      10,10                                   ;
_aboutstr       label   near                                    ;
                include version.inc                             ;
                db      10,0,10                                 ;
                extrn   _ofs64:fword                            ;
                extrn   _sel64:dword                            ;
                extrn   _syscr4:dword                           ;
                extrn   _ret64offset:dword                      ;
                extrn   _logrmpos:word                          ;
                extrn   xcptret:dword                           ;
xcpt64handler   dd      0                                       ;
tm64handler     dd      0                                       ;
                extrn   _mt_exechooks:mt_proc_cb_s              ;
_DATA           ends

_TEXT           segment
                assume  cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT

                extrn   _exit_prepare:near
                extrn   _init32:near
                extrn   _init_physmem:near
                extrn   _log_flush:near
                extrn   _mt_yield:near

                public  _exit_pm32, _exit_pm32s, _exit_pm32a
                public  init32call

init32call      label   near
                call    _init_physmem                           ;
                call    _init32                                 ;
; return from init32() here                                     ;
_exit_pm32a:
                push    eax                                     ; rc code
                jmp     _exit_pm32s                             ;
_exit_pm32:
                pop     eax                                     ; drop return addr
_exit_pm32s:
                call    _exit_prepare                           ;
                pop     eax                                     ; rc

                push    _sel64                                  ;
                push    _ret64offset                            ;
                retf

;/** call 64-bit function.
;    @param function  Function index
;    @param arg8mask  Bit mask for arguments. Use 1 for 8 bit value, 0 for 1/2/4-bit
;    @param argcnt    Number of arguments
;    @return function result (from rax, returned in edx:eax) */
;----------------------------------------------------------------
;u64t _std call64l(int function, u32t arg8mask, u32t argcnt, ...);

                public  _call64, _call64l, _ret64
_call64         proc    near
_call64l        label   near
@@function      =  8                                            ;
@@arg8mask      = 12                                            ;
@@argcnt        = 16                                            ;
@@arglist       = 20                                            ;
                push    ebp                                     ;
                mov     ebp,esp                                 ;
                push    ebx                                     ;
                push    esi                                     ;
                push    edi                                     ;
                push    ds                                      ;
                push    es                                      ;
                push    fs                                      ;
                push    gs                                      ;

                mov     eax,cr0                                 ; EFI BIOSes saves cr0, then
                test    eax,CPU_CR0_TS                          ; going to #NP and then restores cr0
                jz      @@c64_no_ts                             ; (to repeat this in next call)
                mov     ecx,_mt_exechooks.mtcb_cfpt             ;
                jecxz   @@c64_reset_ts                          ; no FPU onwer?
                mov     ecx,_mt_exechooks.mtcb_rmcall           ;
                jecxz   @@c64_reset_ts                          ;
           lock inc     _mt_exechooks.mtcb_glock                ;
                call    ecx                                     ;
           lock dec     _mt_exechooks.mtcb_glock                ;
@@c64_reset_ts:
                clts                                            ;
@@c64_no_ts:
                mov     eax,32                                  ;
                mov     ecx,[ebp+@@argcnt]                      ; align stack
                lea     edx,[ecx*8]                             ; to 16
                cmp     edx,eax                                 ;
                cmovc   edx,eax                                 ;
                sub     esp,edx                                 ;
                and     esp,0FFFFFFF0h                          ;
                jecxz   @@c64_call                              ;

                mov     edx,[ebp+@@arg8mask]                    ;
                lea     esi,[ebp+@@arglist]                     ;
                mov     edi,esp                                 ;
                xor     eax,eax                                 ;
                cld                                             ;
@@c64_argloop:
                movsd                                           ;
                shr     edx,1                                   ;
                jc      @@c64_arg8bit                           ;
                stosd                                           ;
                loop    @@c64_argloop                           ;
                jmp     @@c64_call
@@c64_arg8bit:
                movsd                                           ;
                loop    @@c64_argloop                           ;
@@c64_call:
                mov     eax,[ebp+@@function]                    ;
                cli                                             ;
                jmp     fword ptr _ofs64                        ;
_ret64:
                lea     esp,[ebp-28]                            ;
                mov     ebx,cr4                                 ; just to be safe
                mov     esi,_syscr4                             ;
                or      esi,ebx                                 ; restore our`s
                cmp     esi,ebx                                 ; cr4 bits
                jz      @@c64_skipcr4                           ;
                mov     cr4,esi                                 ;
@@c64_skipcr4:
                sti                                             ;
                pop     gs                                      ;
                pop     fs                                      ;
                pop     es                                      ;
                pop     ds                                      ;
                pop     edi                                     ;
                pop     esi                                     ;
                pop     ebx                                     ;
                pop     ebp                                     ;

                test    _logrmpos,-1                            ;
                jz      @@c64_nolog                             ;
                push    edx                                     ; flush log buffer
                push    eax                                     ;
                call    _log_flush                              ;
                pop     eax                                     ;
                pop     edx                                     ;
@@c64_nolog:
                call    _mt_yield                               ;
                ret                                             ;
_call64         endp

; Note, what this can be #NM exception from EFI INTERRUPT HANDLER!
; These dumb EFI BIOS coders use SSE commands in any case and TS bit will
; guide us here. And, when EFI leaves interrupt - it restores CR0 with TS=1,
; in addition!
                public  xcpt64entry
xcpt64entry     proc    near
                mov     ax,ss                                   ; esp points to temp buffer
                mov     ds,ax                                   ; allocated by 64-bit part
                mov     es,ax                                   ;

;                mov     eax,cr4                                 ;
;                mov     ecx,_syscr4                             ;
;                or      ecx,eax                                 ;
;                cmp     ecx,eax                                 ;
;                jz      @@xc64_skipcr4                          ;
;                mov     cr4,ecx                                 ;
;@@xc64_skipcr4:
                cmp     dword ptr [esp].x64_number,256          ; interrupts are disabled
                cmovnz  ecx,xcpt64handler                       ; here
                jnz     @@xc64_common                           ;
                mov     ecx,tm64handler                         ;
@@xc64_common:
                jecxz   @@xc64_nocall                           ;
                push    esp                                     ; addr of struct
                call    ecx                                     ;
@@xc64_nocall:
                cli                                             ;
                clts                                            ;
                push    _sel64                                  ;
                push    xcptret                                 ;
                retf
xcpt64entry     endp

                public  _sys_setxcpt64                          ;
_sys_setxcpt64  proc    near                                    ;
                mov     eax,[esp+4]                             ;
                xchg    eax,xcpt64handler                       ;
                ret     4                                       ;
_sys_setxcpt64  endp                                            ;

                public  _sys_tmirq64                            ;
_sys_tmirq64    proc    near                                    ;
                mov     eax,[esp+4]                             ;
                xchg    eax,tm64handler                         ;
                ret     4                                       ;
_sys_tmirq64    endp                                            ;

_TEXT           ends
                end
