                .686p
                .xmm3
                include inc/cpudef.inc
                include inc/efnlist.inc
                include inc/qsinit.inc
                include inc/qspdata.inc

                extrn   _mt_exitthread_int:near
                extrn   _se_newsession:near
                extrn   _mt_switchfiber:near
                extrn   _mod_start_cb:near
                extrn   _free:near
                extrn   _mt_exechooks:mt_proc_cb_s
                extrn   _pt_current:dword

_TEXT           segment dword public USE32 'CODE'

                public  _mt_exitthreada, _mt_exitthread         ;
                public  _pure_retn, _mt_launch, _fiberexit_apc  ;
                public  _regs32to64, _regs64to32                ;
_mt_exitthreada label   near                                    ;
                push    eax                                     ;
                push    0                                       ; ret addr
_mt_exitthread  label   near                                    ;
                mov     cx,cs                                   ;
                add     cx,SEL_INCR                             ;
                mov     ds,cx                                   ; guarantee
                mov     es,cx                                   ; ds/es
                cld                                             ;
                sti                                             ;

                mov     eax,_pt_current                         ; onexit callback
                xor     ecx,ecx                                 ;
                xchg    ecx,[eax].tiCbExit                      ; call it only once
                jecxz   @@exth_nocb                             ;
                call    ecx                                     ;
                jmp     _mt_exitthread                          ; restore seg regs again
@@exth_nocb:
                jmp     _mt_exitthread_int                      ;

_pure_retn      label   near                                    ;
                ret

_fiberexit_apc  label   near                                    ;
                push    1                                       ;
                push    0                                       ;
                call    _mt_switchfiber                         ; should never return
                mov     eax,255                                 ;
                jmp     _mt_exitthreada                         ;

; u32t _std mt_gettid(void);
;----------------------------------------------------------------
                public  _mt_gettid                              ;
_mt_gettid      proc    near                                    ;
                pushfd                                          ;
                cli                                             ;
                xor     eax,eax                                 ;
                or      eax,_pt_current                         ;
                jz      @@getid_nomt                            ;
                mov     eax,[eax].tiTID                         ;
                popfd                                           ;
                ret                                             ;
@@getid_nomt:
                inc     eax                                     ;
                popfd                                           ;
                ret                                             ;
_mt_gettid      endp                                            ;

; u32t _std mt_getfiber(void);
;----------------------------------------------------------------
                public  _mt_getfiber                            ;
_mt_getfiber    proc    near                                    ;
                pushfd                                          ;
                cli                                             ;
                xor     eax,eax                                 ;
                or      eax,_pt_current                         ;
                jz      @@getfb_nomt                            ;
                mov     eax,[eax].tiFiberIndex                  ;
@@getfb_nomt:
                popfd                                           ;
                ret                                             ;
_mt_getfiber    endp                                            ;

;================================================================

_mt_launch      proc    near
                pushad                                          ;
                call    _mod_start_cb                           ;
                popad                                           ;
                jecxz   @@mtl_notse                             ; not a session
                push    0                                       ;
                push    ebx                                     ;
                push    edx                                     ;
                push    eax                                     ;
                call    _se_newsession                          ; create session call
                or      ebx,ebx                                 ;
                jz      @@mtl_notse                             ;
                push    ebx                                     ; free session title
                call    _free                                   ; string
@@mtl_notse:
                sti                                             ;
                cld                                             ;
                xor     eax,eax                                 ;
                xor     ebx,ebx                                 ;
                xor     ecx,ecx                                 ;
                xor     edx,edx                                 ;
                xor     esi,esi                                 ;
                xor     edi,edi                                 ;
                xor     ebp,ebp                                 ;
                ret                                             ;
_mt_launch      endp

_regs64to32     proc    near                                    ;
                pushfd                                          ;
                cli                                             ;
                mov     edx,[esp+8]                             ; dst
                mov     ecx,[esp+12]                            ; src
                cld                                             ;
                push    edi                                     ;
                lea     edi,[edx].tss_eip                       ;
                mov     eax,dword ptr [ecx].x64_rip             ;
                stosd                                           ;
                mov     eax,dword ptr [ecx].x64_rflags          ;
                stosd                                           ;
                mov     eax,dword ptr [ecx].x64_rax             ;
                stosd                                           ;
                mov     eax,dword ptr [ecx].x64_rcx             ;
                stosd                                           ;
                mov     eax,dword ptr [ecx].x64_rdx             ;
                stosd                                           ;
                mov     eax,dword ptr [ecx].x64_rbx             ;
                stosd                                           ;
                mov     eax,dword ptr [ecx].x64_rsp             ;
                stosd                                           ;
                mov     eax,dword ptr [ecx].x64_rbp             ;
                stosd                                           ;
                mov     eax,dword ptr [ecx].x64_rsi             ;
                stosd                                           ;
                mov     eax,dword ptr [ecx].x64_rdi             ;
                stosd                                           ;
                mov     eax,dword ptr [ecx].x64_es              ;
                stosd                                           ;
                mov     eax,dword ptr [ecx].x64_cs              ;
                stosd                                           ;
                mov     eax,dword ptr [ecx].x64_ss              ;
                stosd                                           ;
                mov     eax,dword ptr [ecx].x64_ds              ;
                stosd                                           ;
                mov     eax,dword ptr [ecx].x64_fs              ;
                stosd                                           ;
                mov     eax,dword ptr [ecx].x64_gs              ;
                stosd                                           ;
                pop     edi                                     ;
                popfd                                           ;
                ret     8                                       ;
_regs64to32     endp                                            ;

_regs32to64     proc    near
                pushfd
                cli
                mov     edx,[esp+8]                             ; dst
                mov     ecx,[esp+12]                            ; src
                cld                                             ;
                push    esi                                     ;
                lea     esi,[ecx].tss_eip                       ;
                lodsd                                           ;
                mov     dword ptr [edx].x64_rip,eax             ;
                lodsd                                           ;
                mov     dword ptr [edx].x64_rflags,eax          ;
                lodsd                                           ;
                mov     dword ptr [edx].x64_rax,eax             ;
                lodsd                                           ;
                mov     dword ptr [edx].x64_rcx,eax             ;
                lodsd                                           ;
                mov     dword ptr [edx].x64_rdx,eax             ;
                lodsd                                           ;
                mov     dword ptr [edx].x64_rbx,eax             ;
                lodsd                                           ;
                mov     dword ptr [edx].x64_rsp,eax             ;
                lodsd                                           ;
                mov     dword ptr [edx].x64_rbp,eax             ;
                lodsd                                           ;
                mov     dword ptr [edx].x64_rsi,eax             ;
                lodsd                                           ;
                mov     dword ptr [edx].x64_rdi,eax             ;
                lodsd                                           ;
                mov     word ptr [edx].x64_es,ax                ;
                lodsd                                           ;
                mov     word ptr [edx].x64_cs,ax                ;
                lodsd                                           ;
                mov     word ptr [edx].x64_ss,ax                ;
                lodsd                                           ;
                mov     word ptr [edx].x64_ds,ax                ;
                lodsd                                           ;
                mov     word ptr [edx].x64_fs,ax                ;
                lodsd                                           ;
                mov     word ptr [edx].x64_gs,ax                ;
                pop     esi                                     ;
                popfd                                           ;
                ret     8                                       ;
_regs32to64     endp                                            ;

;----------------------------------------------------------------
;void _std swapregs32(struct tss_s *saveregs, struct tss_s *setregs);
;----------------------------------------------------------------
; this is manual switching point. Because it designed as C function,
; we can ignore eax, ecx, edx and flags here.
;
                public  _swapregs32                             ;
_swapregs32     proc    near                                    ;
                pushfd                                          ;
                cli                                             ;
                mov     ecx,[esp+8]                             ; save
                jecxz   @@swp32_nosave                          ;
                std                                             ;
                mov     edx,edi                                 ;
                xor     eax,eax                                 ;
                lea     edi,[ecx].tss_gs                        ;
                mov     ax,gs                                   ;
                stosd                                           ;
                mov     ax,fs                                   ;
                stosd                                           ;
                mov     ax,ds                                   ;
                stosd                                           ;
                mov     ax,ss                                   ;
                stosd                                           ;
                mov     ax,cs                                   ;
                stosd                                           ;
                mov     ax,es                                   ;
                stosd                                           ;
                mov     eax,edx                                 ; edi
                stosd                                           ;
                mov     eax,esi                                 ;
                stosd                                           ;
                mov     eax,ebp                                 ;
                stosd                                           ;
                mov     eax,esp                                 ;
                stosd                                           ;
                mov     eax,ebx                                 ;
                stosd                                           ;
                xor     eax,eax                                 ;
                stosd                                           ;
                stosd                                           ;
                stosd                                           ;
                mov     eax,[esp]                               ; flags
                stosd                                           ;
                mov     eax,offset @@swp32_ret                  ;
                stosd                                           ;
; context saved, stack above esp is previous fiber now!
@@swp32_nosave:
                mov     edx,[esp+12]                            ; set

                mov     gs,[edx].tss_gs                         ;
                mov     fs,[edx].tss_fs                         ;
                mov     es,[edx].tss_es                         ;
                mov     edi,[edx].tss_edi                       ;
                mov     esi,[edx].tss_esi                       ;
                mov     ebp,[edx].tss_ebp                       ;
                mov     ebx,[edx].tss_ebx                       ;
                mov     eax,[edx].tss_eax                       ;
                mov     ecx,[edx].tss_ecx                       ;

                mov     ss,[edx].tss_ss                         ; new stack
                mov     esp,[edx].tss_esp                       ;
                push    [edx].tss_eflags                        ;
                push    dword ptr [edx].tss_cs                  ;
                push    [edx].tss_eip                           ;
                push    [edx].tss_edx                           ;
                mov     ds,[edx].tss_ds                         ;
                pop     edx                                     ;
                mov     _mt_exechooks.mtcb_glock,0              ; !!!!!!!
                iretd                                           ;
@@swp32_ret:
                popfd
                ret     8                                       ;
_swapregs32     endp

_TEXT           ends                                            ;
                end
