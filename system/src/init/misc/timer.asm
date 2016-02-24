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

                .586

_BSS16          segment
                public  _IODelay, _OrgInt50h, _NextBeepEnd
                public  _countsIn55ms, _rtdsc_present
                align   4
_rtdsc_present  db      ?
                db      ?                                       ; reserved for align
_IODelay        dw      ?                                       ; i/o delay value
_OrgInt50h      dd      ?                                       ; old irq0 vector
_NextBeepEnd    dd      ?                                       ; next end of beep, ticks
rdtscprev       dq      ?                                       ;
_countsIn55ms   dq      ?                                       ;
_BSS16          ends

TEXT16          segment
                assume  cs:G16, ds:G16, es:nothing, ss:G16

timer_setmode   proc    near
                call    shortwait                               ;
                out     PORT_CW,al                              ; program count mode of timer 0
                xor     al,al                                   ;
                out     PORT_CNT0,al                            ; set count to 64k
                nop                                             ;
                out     PORT_CNT0,al                            ;
                ret
timer_setmode   endp

;
; note: this routine is a copy of original IBM code. The reason for this is
;       a high sensitivity of several network cards (Intel basically) to the
;       IODELAY value.
;
                public  _calibrate_delay
_calibrate_delay proc   far
                pushf                                           ;
                cli                                             ; disable interrupts
                xor     eax,eax                                 ;
                mov     dword ptr rdtscprev,eax                 ; zero rdtsc
                mov     dword ptr rdtscprev+4,eax               ; counters
                mov     dword ptr _countsIn55ms,eax             ;
                mov     dword ptr _countsIn55ms+4,eax           ;

                mov     al,SC_READBACK or RB_NOLATCHCNT or CM_CNT0
                out     PORT_CW,al                              ; save current timer mode
                in      al,PORT_CNT0                            ;
                push    ax                                      ;
ifdef INITDEBUG
                xor     ah,ah                                   ;
                dbg16print <"pit0 status: %x",10>,<ax>          ;
endif
                mov     al,SC_CNT0 or RW_LSBMSB or CM_MODE2     ; setup largest value
                call    timer_setmode                           ;

                call    shortwait                               ; Snapshot value 
                call    shortwait                               ;
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
                pop     di                                      ; pop saved state
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

                mov     dx,di                                   ; check original timer
                and     dl,STATUS_CNTMODE                       ; mode and restore it to
                cmp     dl,CM_MODE3                             ; mode 3 (who knows? ;))
                jnz     @@dllp3                                 ;

                pushf                                           ; restore mode 3
                cli                                             ;
                mov     al,SC_CNT0 or RW_LSBMSB or CM_MODE3     ;
                call    timer_setmode                           ;
                popf
@@dllp3:
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
                jz      @@irq0_rtdsc                            ;
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
@@irq0_rtdsc:
                test    cs:_rtdsc_present,1                     ; timer calc
                jz      @@irq0_quit                             ;
                push    eax                                     ;
                push    ecx                                     ;
                push    edx                                     ;
                rdtsc                                           ;
                mov     ecx,eax                                 ;
                xchg    ecx,dword ptr cs:rdtscprev              ; calc diff between
                jecxz   @@irq0_rtdsc_zero                       ; rdtscprev and
                sub     eax,ecx                                 ; current value
@@irq0_rtdsc_nz:
                mov     ecx,edx                                 ;
                xchg    ecx,dword ptr cs:rdtscprev+4            ; and save current
                sbb     edx,ecx                                 ; as rdtscprev
                mov     dword ptr cs:_countsIn55ms,eax          ;
                mov     dword ptr cs:_countsIn55ms+4,edx        ;
                jmp     @@irq0_rtdsc_exit
@@irq0_rtdsc_zero:
                or      ecx,dword ptr cs:rdtscprev+4            ; rdtscprev==0?
                jnz     @@irq0_rtdsc_nz                         ; if not - continue calc,
                mov     dword ptr cs:rdtscprev+4,edx            ; else just save new rdtscprev
@@irq0_rtdsc_exit:
                pop     edx                                     ;
                pop     ecx                                     ;
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
                mov     di,PIC1_IRQ_NEW shl 2                   ; get int 50h
                mov     eax,es:[di]                             ;
                mov     cs:_OrgInt50h,eax                       ;
                mov     ax,cs                                   ; and set own
                shl     eax,16                                  ;
                mov     ax,offset irq0_handler                  ;
                stosd                                           ;
                popf                                            ;
                ret                                             ;
settimerirq     endp

;----------------------------------------------------------------
; remove irq0_handler
                public  _rmvtimerirq                            ;
_rmvtimerirq    proc    far                                     ;
                pushf                                           ;
                cli                                             ;
                xor     eax,eax                                 ;
                mov     es,ax                                   ;
                mov     ecx,cs:_OrgInt50h                       ; restore org
                mov     es:[PIC1_IRQ_NEW shl 2],ecx             ; int 8 ptr
                xchg    cs:_NextBeepEnd,eax                     ;
                or      eax,eax                                 ;
                jz      @@rmvti_nosound                         ;
                in      al,SPEAKER_PORT                         ; turn off speaker
                and     al,SPEAKER_MASK_OFF                     ;
                out     SPEAKER_PORT,al                         ;
@@rmvti_nosound:
                popf                                            ;
                ret                                             ;
_rmvtimerirq    endp                                            ;

TEXT16          ends
                end
