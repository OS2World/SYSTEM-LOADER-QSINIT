                .686p
                .xmm3
                include inc/cpudef.inc
                include inc/efnlist.inc
                include inc/qsinit.inc
                include inc/qspdata.inc

                extrn   _mt_exitthread_int:near
                extrn   _mt_exechooks:mt_proc_cb_s
                extrn   _pt_current:dword

_TEXT           segment dword public USE32 'CODE'

                public  _mt_exitthreada, _mt_exitthread         ;
                public  _pure_retn, _mt_launch                  ;
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
                jmp     _mt_exitthread_int                      ;

_pure_retn      label   near                                    ;
                ret

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

; u64t _std mt_tlsget(u32t index);
;----------------------------------------------------------------
                public  _mt_tlsget                              ;
_mt_tlsget      proc    near                                    ;
                pushfd                                          ;
                cli                                             ;
                xor     eax,eax                                 ;
                or      eax,_pt_current                         ; no MT?
                jz      @@gettls_err                            ;
                mov     ecx,[eax].tiParent                      ;
                mov     edx,[esp+8]                             ; index
                cmp     edx,[ecx].piTLSSize                     ;
                jnc     @@gettls_err                            ;
                mov     eax,[eax].tiTLSArray                    ;
                mov     ecx,[eax+edx*4]                         ;
                jecxz   @@gettls_err                            ; no ptr?
                mov     eax,[ecx]                               ;
                mov     edx,[ecx+4]                             ;
                popfd                                           ;
                ret     4                                       ;
@@gettls_err:
                xor     edx,edx                                 ; zero result
                xor     eax,eax                                 ;
                popfd                                           ;
                ret     4                                       ;
_mt_tlsget      endp                                            ;

; qserr _std mt_tlsset(u32t index, u64t value);
;----------------------------------------------------------------
                public  _mt_tlsset                              ;
_mt_tlsset      proc    near                                    ;
@@tlsset_index  =  8                                            ;
@@tlsset_value  =  12                                           ;
                pushfd                                          ;
                cli                                             ;
                xor     eax,eax                                 ;
                or      eax,_pt_current                         ;
                jz      @@settls_err1                           ;
                mov     ecx,[eax].tiParent                      ;
                mov     edx,[esp+@@tlsset_index]                ; index
                cmp     edx,[ecx].piTLSSize                     ;
                jnc     @@settls_err2                           ;
                mov     eax,[eax].tiTLSArray                    ;
                mov     ecx,[eax+edx*4]                         ;
                jecxz   @@settls_err2                           ; no ptr?
                mov     eax,[esp+@@tlsset_value]                ;
                mov     edx,[esp+@@tlsset_value+4]              ;
                mov     [ecx],eax                               ;
                mov     [ecx+4],edx                             ;
                xor     eax,eax                                 ; no error
@@settls_exit:
                popfd                                           ;
                ret     12                                      ;
@@settls_err2:
                mov     eax,50009h                              ; bad TLS slot err
                jmp     @@settls_exit                           ;
@@settls_err1:
                mov     eax,50001h                              ; MT disabled err
                jmp     @@settls_exit                           ;
_mt_tlsset      endp                                            ;

; qserr _std mt_tlsaddr(u32t index, u64t **slotaddr);
;----------------------------------------------------------------
                public  _mt_tlsaddr                             ;
_mt_tlsaddr     proc    near                                    ;
@@tlsaddr_index =  8                                            ;
@@tlsaddr_pret  =  12                                           ;
                pushfd                                          ;
                cli                                             ;
                xor     eax,eax                                 ;
                or      eax,_pt_current                         ;
                jz      @@addrtls_err1                          ;
                mov     ecx,[eax].tiParent                      ;
                mov     edx,[esp+@@tlsaddr_index]               ; index
                cmp     edx,[ecx].piTLSSize                     ;
                jnc     @@addrtls_err2                          ;
                mov     eax,[eax].tiTLSArray                    ;
                mov     ecx,[eax+edx*4]                         ;
                jecxz   @@addrtls_err2                          ; no ptr?
                mov     edx,[esp+@@tlsaddr_pret]                ;
                xor     eax,eax                                 ; no error
                mov     [edx],ecx                               ;
@@addrtls_exit:
                popfd                                           ;
                ret     8                                       ;
@@addrtls_err2:
                mov     eax,50009h                              ; bad TLS slot err
                jmp     @@addrtls_exit                          ;
@@addrtls_err1:
                mov     eax,50001h                              ; MT disabled err
                jmp     @@addrtls_exit                          ;
_mt_tlsaddr     endp                                            ;


;================================================================

_mt_launch      proc    near
                push    edx                                     ; command line ptr
                push    ecx                                     ; environment ptr
                push    0                                       ; 0
                push    eax                                     ; module handle
                push    offset _mt_exitthreada                  ; ret addr
                push    ebx                                     ; startaddr
                xor     eax,eax                                 ;
                xor     ebx,ebx                                 ;
                xor     ecx,ecx                                 ;
                xor     edx,edx                                 ;
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
