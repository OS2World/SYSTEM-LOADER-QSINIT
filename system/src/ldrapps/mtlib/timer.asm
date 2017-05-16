                .686p
                .xmm3
                include inc/timer.inc
                include inc/cpudef.inc
                include inc/efnlist.inc
                include inc/seldesc.inc
                include inc/qsinit.inc

                extrn   _timer32cb:near
                extrn   _tmstack32:dword
                extrn   _sys_errbrk:near
                extrn   _timer_reenable:near
                extrn   _next_rdtsc:qword
                extrn   _apic:dword

_DATA           segment dword public USE32 'CODE'
                ; string to find it in memory, actually ;)
                db      "MTLIB COUNTERS",0
_yield_cnt      dd      0
                public  _timer_cnt, _yield_cnt
_timer_cnt      dd      0
_DATA           ends

_TEXT           segment dword public USE32 'CODE'

;
; APIC timer interrupt for 32-bit host.
; ---------------------------------------------------------------
; timer32cb() called to switch process context,
; this code supports returning to outer ring too.
;
                public  _timer32, _spurious32
                assume cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT       ; assume is wrong, at least
_timer32        label   near                                    ; until ss/es setup
                cli                                             ;
                push    eax                                     ;
                push    ebx                                     ;
                push    ds                                      ;
                mov     bx,cs                                   ;
                add     bx,8                                    ;

                mov     ds,bx                                   ;
                mov     eax,_tmstack32                          ;
                sub     eax,size tss_s                          ; tss in stack
                mov     [eax].tss_ecx,ecx                       ;
                mov     [eax].tss_edx,edx                       ;
                mov     [eax].tss_edi,edi                       ;
                mov     [eax].tss_esi,esi                       ;
                mov     [eax].tss_ebp,ebp                       ;
                pop     dword ptr [eax].tss_ds                  ;
                pop     [eax].tss_ebx                           ;
                pop     [eax].tss_eax                           ;
                pop     [eax].tss_eip                           ;
                pop     ecx                                     ; cs
                pop     [eax].tss_eflags                        ;
                mov     [eax].tss_cs,cx                         ;
                mov     [eax].tss_es,es                         ;
                mov     [eax].tss_gs,gs                         ;
                mov     [eax].tss_fs,fs                         ;
                mov     es,bx                                   ;

                test    cl,RPL_MASK                             ; is it upper ring?
                jz      @@tm32_ring0                            ;
                pop     [eax].tss_esp                           ;
                pop     dword ptr [eax].tss_ss                  ;
                jmp     @@tm32_common1                          ;
@@tm32_ring0:
                mov     [eax].tss_ss,ss                         ;
                mov     [eax].tss_esp,esp                       ;
@@tm32_common1:
                mov     cx,ss                                   ;
                cmp     cx,SELZERO                              ; check stack
                jz      @@tm32_good_ss                          ; selector
                cmp     cx,bx                                   ; (leave flat mode
                jz      @@tm32_good_ss                          ; SELZERO as it is)
                mov     ss,bx                                   ;
@@tm32_good_ss:
                mov     esp,eax                                 ; we have own stack!

                push    esp                                     ; struct tss_s *
                call    _timer32cb                              ;

                mov     gs,[esp].tss_gs                         ; restore common
                mov     fs,[esp].tss_fs                         ; registers
                mov     es,[esp].tss_es                         ;
                mov     ebp,[esp].tss_ebp                       ;
                mov     ecx,[esp].tss_ecx                       ;
                mov     edx,[esp].tss_edx                       ;
                mov     edi,[esp].tss_edi                       ;
                mov     esi,[esp].tss_esi                       ;

                movzx   eax,[esp].tss_cs                        ;
                mov     ebx,esp                                 ;
                test    al,RPL_MASK                             ; is it upper ring?
                jz      @@tm32_ring0ret                         ;
                push    dword ptr [ebx].tss_ss                  ; push ss:esp to
                push    [ebx].tss_esp                           ; stack frame
                jmp     @@tm32_ret                              ;
@@tm32_ring0ret:
                cli                                             ; stack replaced
                mov     ss,[ebx].tss_ss                         ; to user one
                mov     esp,[ebx].tss_esp                       ;
@@tm32_ret:
                push    [ebx].tss_eflags                        ;
                push    eax                                     ; cs:eip
                push    [ebx].tss_eip                           ;
                mov     eax,[ebx].tss_eax                       ;

                push    [ebx].tss_ebx                           ;
                mov     ds,[ebx].tss_ds                         ;

                pop     ebx                                     ;
_spurious32     label   near                                    ;
                iretd                                           ;

; void yield(void)
                public  _yield                                  ;
_yield          label   near                                    ;
                pushfd                                          ;
                push    eax                                     ;
                push    edx                                     ; cli also is a flag
                cli                                             ; for timer32cb()

                mov     eax,_apic                               ;
                mov     edx,[eax+APIC_LVT_TMR*4]                ;
                cmp     edx,APIC_DISABLE                        ;
                jnz     @@yie_tmrok                             ;
                call    _timer_reenable                         ;
@@yie_tmrok:
                rdtsc                                           ;
                cmp     edx,dword ptr cs:_next_rdtsc+4          ;
                jc      @@yie_exit                              ; check for ">" ,not
                ja      @@yie_cb32                              ; for ">=", this 
                cmp     eax,dword ptr cs:_next_rdtsc            ; guarantee -1 skip
                jna     @@yie_exit                              ; 
@@yie_cb32:
                call    @@yie_check_ds                          ;

                mov     eax,-1                                  ; prevent recursion, which
                mov     dword ptr _next_rdtsc,eax               ; is real, at least from
                mov     dword ptr _next_rdtsc+4,eax             ; trap screen printing ;)

                pushfd                                          ;
                push    cs                                      ;
                call    _timer32                                ;

                jmp     edx                                     ;
@@yie_check_ds:
                mov     edx,cs                                  ; who knows?
                mov     eax,ds                                  ;
                add     dx,8                                    ;
                cmp     dx,ax                                   ;
                jnz     @@yie_wrong_ds                          ;
                mov     edx,offset @@yie_quit                   ;
                ret                                             ;
@@yie_wrong_ds:
                pop     eax                                     ;
                push    ds                                      ;
                mov     ds,dx                                   ;
                mov     edx,offset @@yie_quit_ds                ;
                jmp     eax
@@yie_exit:
                call    @@yie_check_ds                          ;
                inc     _yield_cnt                              ;
                jmp     edx                                     ;
@@yie_quit_ds:
                pop     ds                                      ;
@@yie_quit:
                pop     edx                                     ;
                pop     eax                                     ;
                popfd                                           ;
                ret                                             ;
_TEXT           ends                                            ;
                end
