;
; QSINIT "start" module
; low level system support functions
;
                .586p

                include inc/cpudef.inc
                include inc/pci.inc

                MSR_IA32_MTRR_DEF_TYPE = 02FFh
                MTRRS_MTRRON           = 0800h

CODE32          segment dword public USE32 'CODE'
                assume cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT

;----------------------------------------------------------------
;void _std hlp_mtrrmstart(u32t *state);
;----------------------------------------------------------------
; 1.Disable interrupts.
; 2.Enter the no-fill cache mode. (Set the CD flag in control register CR0 to 1
;   and the NW flag to 0.)
; 3.Flush all caches using the WBINVD instructions. Note on a processor that
;   supports self-snooping, CPUID feature flag bit 27, this step is unnecessary.
; 4.If the PGE flag is set in control register CR4,
;   flush all TLBs by clearing that flag.
; 5.If the PGE flag is clear in control register CR4, flush all TLBs by
;   executing a MOV from control register CR3 to another register and then
;   a MOV from that register back to CR3.
; 6.Disable all range registers (by clearing the E flag in register MTRRdefType).
;   If only variable ranges are being modified, software may clear the valid
;   bits for the affected register pairs instead.
; 7.Update the MTRRs.
; 8.Enable all range registers (by setting the E flag in register MTRRdefType).
;   If only variable-range registers were modified and their individual valid
;   bits were cleared, then set the valid bits for the affected ranges instead.
; 9.Flush all caches and all TLBs a second time. (The TLB flush is required for
;   Pentium 4, Intel Xeon, and P6 family processors. Executing the WBINVD
;   instruction is not needed when using Pentium 4, Intel Xeon, and P6 family
;   processors, but it may be needed in future systems.)
;10.Enter the normal cache mode to re-enable caching. (Set the CD and NW
;   flags in control register CR0 to 0.)
;11.Set PGE flag in control register CR4, if cleared in Step 6 (above).
;12.Enable interrupts.

                public  _hlp_mtrrmstart
_hlp_mtrrmstart proc  near
@@pstate        =  4                                            ;
                push    edi                                     ;
                mov     edi,[esp+4+@@pstate]                    ;
                push    ebx                                     ;
                mov     eax,1                                   ;
                cpuid                                           ; for snoop flag
                pushfd                                          ;
                cli                                             ;
                pop     eax                                     ;
                mov     ecx,cr0                                 ;
                and     ecx,CPU_CR0_NW or CPU_CR0_CD            ; save CD & NW
                test    eax,CPU_EFLAGS_IF                       ; save
                setnz   cl                                      ; interrupts flag
                mov     eax,cr0                                 ;
                and     eax,not CPU_CR0_NW                      ;
                or      eax,CPU_CR0_CD                          ; no-fill cache
                mov     cr0,eax                                 ; mode

                and     edx,CPUID_FI2_SSNOOP                    ;
                jnz     @@mtrst_ssnoop                          ; flush cache
                wbinvd                                          ;
@@mtrst_ssnoop:
                add     ecx,edx                                 ; save ssnoop
                and     eax,CPU_CR0_PG                          ; PG is set?
                lea     ecx,[ecx+eax+2]                         ; save in state
                jz      @@mtrst_nopages                         ;
                mov     eax,cr4                                 ;
                mov     edx,eax                                 ;
                and     edx,CPU_CR4_PGE                         ; check PGE flag
                lea     ecx,[ecx+edx]                           ; and save it
                jz      @@mtrst_nofpge                          ;
                sub     eax,edx                                 ;
                mov     cr4,eax                                 ; clear PGE flag
                jmp     @@mtrst_nopages                         ;
@@mtrst_nofpge:
                mov     edx,cr3                                 ; flush TLBs
                mov     cr3,edx                                 ;
@@mtrst_nopages:
                mov     ebx,ecx                                 ;
                mov     ecx,MSR_IA32_MTRR_DEF_TYPE              ; disable all
                rdmsr                                           ; MTRRs
                push    eax                                     ;
                and     eax,not MTRRS_MTRRON                    ;
                xor     edx,edx                                 ;
                wrmsr                                           ;
                pop     eax                                     ; save mtrr
                and     eax,MTRRS_MTRRON                        ; state
                add     ebx,eax                                 ;
                mov     [edi],ebx                               ;
                pop     ebx                                     ;
                pop     edi                                     ;
                ret     4                                       ;
_hlp_mtrrmstart endp                                            ;

;----------------------------------------------------------------
;void _std hlp_mtrrmend(u32t state);
;
; state bits (merged in one dword ;)
;  bit 0                - interrupts flag
;  bit 1                - state validation bit (set to 1)
;  bit CPU_CR4_PGE      - actual value
;  bit MTRRS_MTRRON     - mtrr was on
;  bit CPUID_FI2_SSNOOP - actual values
;  bit CPU_CR0_NW       -
;  bit CPU_CR0_CD       -
;  bit CPU_CR0_PG       - PGE must be ignored if PG is 0
;
;  warning! whose bit accessed from syshlp.c code

                public  _hlp_mtrrmend
_hlp_mtrrmend   proc  near
@@pstate        =  4                                            ;
                push    edi                                     ;
                mov     edi,[esp+4+@@pstate]                    ;
                push    ebx                                     ;
                mov     ebx,[edi]                               ; call with zeroed
                or      ebx,ebx                                 ; state info?
                jz      @@mtren_exit                            ;
                test    ebx,MTRRS_MTRRON                        ;
                jz      @@mtren_skipmtrr                        ;
                mov     ecx,MSR_IA32_MTRR_DEF_TYPE              ; restore MTRR state
                rdmsr                                           ;
                or      eax,MTRRS_MTRRON                        ;
                xor     edx,edx                                 ;
                wrmsr                                           ;
@@mtren_skipmtrr:
                test    ebx,CPUID_FI2_SSNOOP                    ;
                jnz     @@mtren_ssnoop                          ; flush cache
                wbinvd                                          ;
@@mtren_ssnoop:
                mov     eax,cr3                                 ; flush TLBs
                mov     cr3,eax                                 ;

                mov     eax,cr0                                 ;
                mov     ecx,ebx                                 ;
                and     ecx,CPU_CR0_NW or CPU_CR0_CD            ; restore orginal
                and     eax,not (CPU_CR0_NW or CPU_CR0_CD)      ; cache flags
                or      eax,ecx                                 ;
                mov     cr0,eax                                 ;

                test    ebx,CPU_CR0_PG                          ;
                jz      @@mtren_nopages                         ;
                mov     ecx,ebx                                 ; restore PGE
                and     ecx,CPU_CR4_PGE                         ; flag
                mov     eax,cr4                                 ;
                or      eax,ecx                                 ;
                mov     cr4,eax                                 ;
@@mtren_nopages:
                test    ebx,1                                   ; restore IF
                jz      @@mtren_exit                            ;
                sti                                             ;
@@mtren_exit:
                mov     dword ptr [edi],0                       ; clear state
                pop     ebx                                     ;
                pop     edi                                     ;
                ret     4                                       ;
_hlp_mtrrmend   endp                                            ;

                public  _hlp_mtrrapply
_hlp_mtrrapply  proc    near
@@mtra_buffer   =  4                                            ;
                sub     esp,4                                   ;
                push    esp                                     ;
                call    _hlp_mtrrmstart                         ; drop MSRON restoration
                and     dword ptr [esp],not MTRRS_MTRRON        ; by _hlp_mtrrmend
                push    ebx                                     ;
                push    esi                                     ;
                push    edi                                     ;
                mov     ebx,32                                  ;
                mov     esi,[esp+16+@@mtra_buffer]              ;
                xor     ecx,ecx                                 ;
                cld                                             ;
@@mtra_loop:
                lodsw                                           ; write fixed and
                mov     cx,ax                                   ; variable range
                jecxz   @@mtra_done                             ; MTRR msrs here,
                lodsd                                           ; the last one must be
                mov     edx,eax                                 ; IA32_MTRR_DEF_TYPE
                lodsd                                           ; with MTRR on bit set
                xchg    edx,eax                                 ;
                wrmsr                                           ;
                dec     ebx                                     ;
                jnz     @@mtra_loop                             ;
@@mtra_done:
                pop     edi                                     ;
                pop     esi                                     ;
                pop     ebx                                     ;
                push    esp                                     ;
                call    _hlp_mtrrmend                           ;
                add     esp,4                                   ;
                ret     4                                       ;
_hlp_mtrrapply  endp

;----------------------------------------------------------------
; u32t _std hlp_pciread(u16t bus, u8t slot, u8t func, u8t offset, u8t size);
                public  _hlp_pciread
_hlp_pciread    proc    near
@@pcir_bus      =  4                                            ;
@@pcir_slot     =  8                                            ;
@@pcir_func     =  12                                           ;
@@pcir_ofs      =  16                                           ;
@@pcir_size     =  20                                           ;
                mov     ax,8000h                                ;
                or      ax,[esp+@@pcir_bus]                     ;
                shl     eax,16                                  ;
                mov     dl,[esp+@@pcir_slot]                    ;
                shl     dl,3                                    ;
                mov     ah,dl                                   ;
                mov     dl,[esp+@@pcir_func]                    ;
                and     dl,7                                    ;
                or      ah,dl                                   ;
                mov     al,[esp+@@pcir_ofs]                     ;
                mov     dx,PCI_ADDR_PORT                        ;
                pushfd                                          ; to make safe out/in pair
                cli                                             ;
                out     dx,eax                                  ;
                add     dl,4                                    ; 0xCFC
                mov     ah,[esp+@@pcir_size+4]                  ;
                shr     ah,1                                    ;
                jc      @@pcir_rdb                              ;
                shr     ah,1                                    ;
                jc      @@pcir_rdw                              ;
                in      eax,dx                                  ;
                popfd                                           ;
                ret     20                                      ;
@@pcir_rdw:
                and     eax,2                                   ;
                add     dl,al                                   ;
                in      ax,dx                                   ;
                popfd                                           ;
                ret     20                                      ;
@@pcir_rdb:
                and     eax,3                                   ;
                add     dl,al                                   ;
                in      al,dx                                   ;
                popfd                                           ;
                ret     20                                      ;
_hlp_pciread    endp

;----------------------------------------------------------------
;void _std hlp_pciwrite(u16t bus,u8t slot,u8t func,u8t ofs,u8t size,u32t value);
                public  _hlp_pciwrite
_hlp_pciwrite   proc    near
@@pcir_bus      =  4                                            ;
@@pcir_slot     =  8                                            ;
@@pcir_func     =  12                                           ;
@@pcir_ofs      =  16                                           ;
@@pcir_size     =  20                                           ;
@@pcir_value    =  24                                           ;
                mov     ax,8000h                                ;
                or      ax,[esp+@@pcir_bus]                     ;
                shl     eax,16                                  ;
                mov     dl,[esp+@@pcir_slot]                    ;
                shl     dl,3                                    ;
                mov     ah,dl                                   ;
                mov     dl,[esp+@@pcir_func]                    ;
                and     dl,7                                    ;
                or      ah,dl                                   ;
                mov     al,[esp+@@pcir_ofs]                     ;
                mov     dx,PCI_ADDR_PORT                        ;
                pushfd                                          ; to make safe out/in pair
                cli                                             ;
                out     dx,eax                                  ;
                add     dl,4                                    ; 0xCFC
                mov     cl,al                                   ;
                mov     ch,[esp+@@pcir_size+4]                  ;
                mov     eax,[esp+@@pcir_value+4]                ;
                shr     ch,1                                    ;
                jc      @@pciw_wdb                              ;
                shr     ch,1                                    ;
                jc      @@pciw_wdw                              ;
                out     dx,eax                                  ;
                popfd                                           ;
                ret     24                                      ;
@@pciw_wdw:
                and     cl,2                                    ;
                add     dl,cl                                   ;
                out     dx,ax                                   ;
                popfd                                           ;
                ret     24                                      ;
@@pciw_wdb:
                and     cl,3                                    ;
                add     dl,cl                                   ;
                out     dx,al                                   ;
                popfd                                           ;
                ret     24                                      ;
_hlp_pciwrite   endp

CODE32          ends

                end
