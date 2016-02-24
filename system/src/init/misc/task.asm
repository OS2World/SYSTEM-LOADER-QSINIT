;
; QSINIT
; timer setup & misc - BIOS build only
;
                include inc/segdef.inc
                include inc/cpudef.inc

                extrn   _page0_fptr   :dword                    ;
                extrn   _syslapic     :dword                    ;
                extrn   _apic_tmr_vnum:byte                     ;
                extrn   _rm16code     :word                     ;
                extrn   _apic_tmr_vorg:dword                    ;
                extrn   _apic_spr_org :dword                    ;
                extrn   _apic_spr_vorg:dword                    ;
                extrn   _apic_spr_vnum:byte                     ;
                extrn   rm_timer_ofs  :word                     ; rm timer int offset
                extrn   rm_spur_ofs   :word                     ; rm spurious int offset

_TEXT           segment
                assume  cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT
                .686p

;----------------------------------------------------------------
; u32t _std sys_tmirq32(u32t lapicaddr, u8t tmrint, u8t sprint);
                public  _sys_tmirq32
_sys_tmirq32    proc    near                                    ;
                movzx   ecx,byte ptr [esp+8]                    ; tmrint
                movzx   edx,byte ptr [esp+12]                   ; sprint
                test    cl,0F0h                                 ;
                jz      @@setapic_exiterr                       ; must be above 16
                test    dl,0F0h                                 ;
                jz      @@setapic_exiterr                       ;
                mov     eax,[esp+4]                             ; lapicaddr
                or      eax,eax                                 ;
                jz      @@setapic_exiterr                       ;
               
                push    gs                                      ;
                pushfd                                          ;
                push    edi                                     ;
                push    esi                                     ;
                cli                                             ;
                mov     edi,eax                                 ; lapicaddr

                lgs     esi,_page0_fptr                         ; writable page 0
                mov     eax,gs:[esi+ecx*4]                      ;
                mov     _apic_tmr_vorg,eax                      ; save old RM vectors
                mov     eax,gs:[esi+edx*4]                      ;
                mov     _apic_spr_vorg,eax                      ;
                mov     _apic_tmr_vnum,cl                       ;
                mov     _apic_spr_vnum,dl                       ;
                mov     _syslapic,edi                           ;
                mov     ax,_rm16code                            ;
                shl     eax,16                                  ; set new RM vectors
                mov     ax,rm_spur_ofs                          ;
                mov     gs:[esi+edx*4],eax                      ;
                mov     ax,rm_timer_ofs                         ;
                mov     gs:[esi+ecx*4],eax                      ;

                mov     eax,[edi+APIC_SPURIOUS*4]               ; save spurious
                mov     _apic_spr_org,eax                       ; register

                pop     esi                                     ;
                pop     edi                                     ;
                popfd                                           ;
                pop     gs                                      ;
                or      al,1                                    ; clears ZF
@@setapic_exiterr:
                setnz   al                                      ;
@@setapic_exit:
                movzx   eax,al                                  ;
                ret     12                                      ;
_sys_tmirq32    endp                                            ;

_TEXT           ends
                end
