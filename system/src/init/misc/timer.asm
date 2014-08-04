;
; QSINIT
; timer & io delay calibrate
;
                include inc/segdef.inc
                include inc/timer.inc
                include inc/lowports.inc
                include inc/iopic.inc
                include inc/cmos.inc
                include inc/debug.inc

_BSS            segment
                public  _IODelay
                public  _OrgInt50h
                public  _NextBeepEnd
                extrn   _ZeroAddress:dword                      ; phys 0 address
_IODelay        dw      ?                                       ; i/o delay value
_OrgInt50h      dd      ?                                       ; old irq0 vector
_NextBeepEnd    dd      ?                                       ; next end of beep, ticks
_BSS            ends

_TEXT           segment
                assume  cs:DGROUP,ds:DGROUP,es:nothing,ss:DGROUP
;
; note: this routine is a copy of original IBM code. The reason for this is
;       a high sensitivity of several network cards (Intel basically) to the
;       IODELAY value.
;
                public  _calibrate_delay
_calibrate_delay proc   far
                pushf                                           ;
                cli                                             ; disable interrupts
                xor     bx,bx                                   ;

                mov     al,SC_CNT0 or RW_LSBMSB or CM_MODE2     ; setup largest value
                out     PORT_CW,al                              ; program count mode of timer 0
                call    shortwait                               ;
                mov     ax,bx                                   ;
                out     PORT_CNT0,al                            ; program timer 0 count
                call    shortwait                               ;
                mov     al,ah                                   ;
                out     PORT_CNT0,al                            ;

                call    shortwait                               ; Snapshot value
                mov     al,SC_CNT0 or RW_LATCHCNT               ; Latch PIT Ctr 0 command.
                out     PORT_CW,al                              ;
                call    shortwait                               ;
                in      al,PORT_CNT0                            ; Read PIT Ctr 0, LSByte.
                call    shortwait                               ;
                mov     cl,al                                   ;
                in      al,PORT_CNT0                            ; Read PIT Ctr 0, MSByte.
                mov     ch,al                                   ;
                mov     si,0CE3Ah                               ;
                ALIGN 4                                         ;
@@dllp:
                dec     si                                      ;
                jnz     @@dllp                                  ;
                                                                
                mov     si,0CE3Ah                               ;
                ALIGN 4                                         ;
@@dllp1:                                                        
                dec     si                                      ;
                jnz     @@dllp1                                 ;

                mov     al,SC_CNT0 or RW_LATCHCNT               ; Snapshot - use for error correction
                out     PORT_CW,al                              ; Latch PIT Ctr 0 command.
                call    shortwait                               ;
                in      al,PORT_CNT0                            ; Read PIT Ctr 0, LSByte.
                call    shortwait                               ;
                mov     dl,al                                   ;
                in      al,PORT_CNT0                            ; Read PIT Ctr 0, MSByte.
                mov     dh,al                                   ;
                popf                                            ;
                xor     bx,bx                                   ;
                sub     bx,cx                                   ; bx - snapshot time
                sub     cx,dx                                   ; cx - loop consumption + snapshot time
                and     bx,bx                                   ;
                jz      @@latchdf4                              ;
                sub     cx,bx                                   ;
                jbe     @@latchdf0                              ;
                mov     ax,0CE3Ah                               ;
                mul     bx                                      ;
                mov     si,80Eh                                 ;
                div     si                                      ;
                cmp     ax,cx                                   ;
                ja      @@latchdff                              ;
@@latchdf0:
                add     cx,bx                                   ;
                add     cx,bx                                   ;
@@latchdf4:
                shl     cx,1                                    ;
                inc     cx                                      ;
                mov     ax,0FF68h                               ;
                mov     dx,1                                    ;
                jmp     @@latche05                              ;
@@latchdff:
                mov     ax,0F618h                               ;
                mov     dx,0                                    ;
@@latche05:
                cmp     cx,dx                                   ;
                jbe     @@latche14                              ;
                idiv    cx                                      ;
                or      dx,dx                                   ;
                jz      @@latche17                              ;
                add     ax,1                                    ;
                jnb     @@latche17                              ;
@@latche14:
                mov     ax,0FFFFh                               ;
@@latche17:
                mov     _IODelay,ax                             ; save value for later use
                ret                                             ;
_calibrate_delay endp                                           ;

shortwait       proc    near
                push    ax                                      ;
                mov     ax,0FFh                                 ;
@@loop:         
                dec     ax                                      ;
                or      ax,ax                                   ;
                jnz     @@loop                                  ;
                pop     ax                                      ;
                ret                                             ;
shortwait       endp                                            ;

;----------------------------------------------------------------
; timer irq handler (irq 0, int 50h)
;
                align   4
irq0_handler    proc    near                                    ;
                pushf                                           ;
                cli                                             ;

                cmp     dword ptr cs:_NextBeepEnd,0             ; sound playing?
                jz      @@irq0_quit                             ;
                push    eax                                     ;
                push    es                                      ;
                xor     ax,ax                                   ;
                mov     es,ax                                   ;
                mov     eax,es:BIOS_TICS                        ;
                pop     es                                      ;
                cmp     eax,cs:_NextBeepEnd                     ;
                jc      @@irq0_soundon                          ;
                xor     eax,eax                                 ;
                mov     cs:_NextBeepEnd,eax                     ;
                in      al,SPEAKER_PORT                         ; turn off speaker
                and     al,SPEAKER_MASK_OFF                     ;
                out     SPEAKER_PORT,al                         ;
@@irq0_soundon:
                pop     eax                                     ;
@@irq0_quit:
                popf                                            ;
                jmp     dword ptr cs:_OrgInt50h                 ;
irq0_handler    endp

;----------------------------------------------------------------
; install irq0_handler
                public  settimerirq
settimerirq     proc    near                                    ;
                pushf                                           ;
                cli                                             ;
                xor     eax,eax                                 ;
                mov     cs:_NextBeepEnd,eax                     ; zero it again ;)
                mov     es,ax                                   ;
                mov     edi,PIC1_IRQ_NEW shl 2                  ; get int 50h
                mov     eax,es:[edi]                            ;
                mov     cs:_OrgInt50h,eax                       ;
                mov     ax,cs                                   ; and set own
                shl     eax,16                                  ;
                mov     ax,offset irq0_handler                  ;
                stosd                                           ;
                popf                                            ;
                ret                                             ;
settimerirq     endp

_TEXT           ends


CODE32          segment
                assume  cs:CODE32,ds:DGROUP,es:DGROUP,ss:DGROUP
;----------------------------------------------------------------
; remove irq0_handler
                public  _rmvtimerirq                            ;
_rmvtimerirq    proc    near                                    ;
                pushf                                           ;
                cli                                             ;
                mov     ecx,PIC1_IRQ_NEW shl 2                  ;
                add     ecx,_ZeroAddress                        ; restore org
                mov     eax,_OrgInt50h                          ; int 8 ptr
                mov     [ecx],eax                               ;
                xor     eax,eax                                 ;
                xchg    _NextBeepEnd,eax                        ;
                or      eax,eax                                 ;
                jz      @@rmvti_nosound                         ;
                in      al,SPEAKER_PORT                         ; turn off speaker
                and     al,SPEAKER_MASK_OFF                     ;
                out     SPEAKER_PORT,al                         ;
@@rmvti_nosound:
                popf                                            ;
                ret                                             ;
_rmvtimerirq    endp                                            ;

CODE32          ends
                end
