;
; QSINIT
; timer & io delay calibrate
;
                include inc/timer.inc
                include inc/lowports.inc
                include inc/iopic.inc
                include inc/cmos.inc
                include inc/segdef.inc

_BSS            segment                                         ;
                extrn   _NextBeepEnd:dword                      ;
lastgt_tick     dd      ?                                       ; tick of last query time call
lastgt_value    dd      ?                                       ; last query time value
lastgt_overflow dd      ?                                       ; one second flag
_BSS            ends

_TEXT           segment
                assume  cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT

                extrn   _usleep:near

;----------------------------------------------------------------
;void _std vio_beep(u16t freq, u32t ms);
                public  _vio_beep
_vio_beep       proc near
@@freq          =  4                                            ;
@@ms            =  8                                            ;
                mov     ecx,[esp+@@ms]                          ;
                jecxz   @@vbeep_exit                            ;
                xor     edx,edx                                 ;
                lea     eax,[ecx+27]                            ; round to 55 ms
                mov     ecx,55                                  ;
                div     ecx                                     ;
                or      eax,eax                                 ;
                jz      @@vbeep_exit                            ; too short
                push    esi                                     ;
                movzx   esi,word ptr [esp+4+@@freq]             ;
                cmp     esi,20                                  ; freq < 20?
                jc      @@vbeep_end                             ;

                pushf                                           ;
                push    eax                                     ;
                cli                                             ;
                xor     edx,edx                                 ;
                cmp     _NextBeepEnd,edx                        ;
                jz      @@vbeep_nosound                         ;
                in      al,SPEAKER_PORT                         ; turn off speaker
                and     al,SPEAKER_MASK_OFF                     ;
                out     SPEAKER_PORT,al                         ;
@@vbeep_nosound:
                mov     al,SC_CNT2 or RW_LSBMSB or CM_MODE3     ; timer 2/LSB/MSB/binary
                out     PORT_CW,al                              ;
                mov     eax,TIMERFREQ                           ;
                div     esi                                     ; divide to frequency
                and     al,0FEh                                 ; mask odd value
                out     PORT_CNT2,al                            ; write to timer

                xor     edx,edx                                 ;
                push    edx                                     ;
                call    _usleep                                 ;
                mov     al,ah                                   ;
                out     PORT_CNT2,al                            ;
                push    edx                                     ;
                call    _usleep                                 ;

                in      al,SPEAKER_PORT                         ; read port
                push    edx                                     ;
                call    _usleep                                 ;
                or      al,SPEAKER_MASK                         ; turn on speaker
                out     SPEAKER_PORT,al                         ;

                pop     eax                                     ;
                add     eax,ss:[BIOS_TICS]                      ; update sound
                mov     _NextBeepEnd,eax                        ; end time
                popf                                            ;
@@vbeep_end:
                pop     esi                                     ;
@@vbeep_exit:
                ret     8                                       ;
_vio_beep       endp                                            ;

;----------------------------------------------------------------
; read cmos
; in : al - addr
; out: al - value
cmos2bin        proc    near                                    ;
                out     CMOS_ADDR,al                            ;
                jmp     $+2                                     ;
                in      al,CMOS_DATA                            ;
                xor     ah,ah                                   ;
                shl     ax,4                                    ;
                shr     al,4                                    ;
                aad                                             ;
                ret                                             ;
cmos2bin        endp                                            ;

;----------------------------------------------------------------
; write cmos
; in : al - addr
;      ah - value
bin2cmos        proc    near                                    ;
                out     CMOS_ADDR,al                            ;
                mov     al,ah                                   ;
                aam                                             ;
                shl     al,4                                    ;
                shr     ax,4                                    ;
                out     CMOS_DATA,al                            ;
                ret                                             ;
bin2cmos        endp                                            ;

;----------------------------------------------------------------
; u32t _std tm_getdate(void);
;
                public  _tm_getdate                             ;
_tm_getdate     proc    near                                    ;
                mov     edx,ss:[BIOS_TICS]                      ; update cmos time
                lea     eax,[edx-17]                            ; at least once per
                cmp     lastgt_tick,eax                         ; second
                jc      @@tmgtme_query                          ;
                bt      lastgt_overflow,0                       ; set CF
                mov     eax,lastgt_value                        ;
                ret                                             ;
@@tmgtme_query:
                xor     eax,eax                                 ;
                xor     ecx,ecx                                 ;
                clc                                             ; clear CF for
                pushf                                           ; result
                cli                                             ;
                mov     al,CMOS_CENT or NMI_MASK                ;
                call    cmos2bin                                ;
                mov     ah,100                                  ;
                imul    ah                                      ;
                mov     cx,ax                                   ;
                mov     al,CMOS_YRS or NMI_MASK                 ;
                call    cmos2bin                                ;
                add     cx,ax                                   ;
                cmp     cx,1980                                 ;
                jc      @@tmgtme_err                            ;
                cmp     cx,2100                                 ;
                jnc     @@tmgtme_err                            ;
                sub     cx,1980                                 ;
                mov     al,CMOS_MON or NMI_MASK                 ;
                call    cmos2bin                                ;
                cmp     al,13                                   ;
                jnc     @@tmgtme_err                            ;
                shl     ecx,4                                   ;
                or      cl,al                                   ;
                mov     al,CMOS_DAY or NMI_MASK                 ;
                call    cmos2bin                                ;
                cmp     al,32                                   ;
                jnc     @@tmgtme_err                            ;
                shl     ecx,5                                   ;
                or      cl,al                                   ;
                mov     al,CMOS_HRS or NMI_MASK                 ;
                call    cmos2bin                                ;
                cmp     al, 24                                  ;
                jnc     @@tmgtme_err                            ;
                shl     ecx,5                                   ;
                or      cl,al                                   ;
                mov     al,CMOS_MIN or NMI_MASK                 ;
                call    cmos2bin                                ;
                cmp     al,60                                   ;
                jnc     @@tmgtme_err                            ;
                shl     ecx,6                                   ;
                or      cl,al                                   ;
                mov     al,CMOS_SEC or NMI_MASK                 ;
                call    cmos2bin                                ;
                cmp     al,60                                   ;
                jnc     @@tmgtme_err                            ;
                cbw                                             ;
                shl     ecx,5                                   ;
                mov     byte ptr lastgt_overflow,al             ;
                ror     ax,1                                    ; store 1 sec
                add     cl,al                                   ; to CF flag in stack
                rol     ah,1                                    ;
                or      [esp],ah                                ;
@@tmgtme_exit:
                cmosreset                                       ;
                mov     lastgt_value,ecx                        ;
                mov     lastgt_tick,edx                         ;
                mov     eax,ecx                                 ;
                popf                                            ;
                ret                                             ;
@@tmgtme_err:
                mov     ecx,40210000h                           ; return 1-1-2012
                jmp     @@tmgtme_exit                           ; on error
_tm_getdate     endp                                            ;

;----------------------------------------------------------------
; void _std tm_setdate(u32t dostime);
;
                public  _tm_setdate
_tm_setdate     proc near
@@dostime       =  4                                            ;
                mov     ecx,[esp+@@dostime]                     ;
                cmosset CMOS_REG_B                              ; disable updates
                mov     al,CMOS_B_DENY                          ;
                out     CMOS_DATA,al                            ;
                pushf                                           ;
@@tmsd_waitupd:
                cli                                             ;
                cmosset CMOS_REG_A                              ;
                in      al,CMOS_DATA                            ;
                sti                                             ;
                test    al,CMOS_A_UIP                           ;
                jnz     @@tmsd_waitupd                          ;
                cli                                             ;
                mov     ah,cl                                   ;
                shl     ah,1                                    ;
                and     ah,3Fh                                  ;
                mov     al,CMOS_SEC or NMI_MASK                 ;
                call    bin2cmos                                ;
                mov     ax,cx                                   ;
                shl     ax,3                                    ;
                and     ah,3Fh                                  ;
                mov     al,CMOS_MIN or NMI_MASK                 ;
                call    bin2cmos                                ;
                mov     ah,ch                                   ;
                shr     ah,3                                    ;
                mov     al,CMOS_HRS or NMI_MASK                 ;
                call    bin2cmos                                ;
                shr     ecx,16                                  ;
                mov     ah,cl                                   ;
                and     ah,1Fh                                  ;
                mov     al,CMOS_DAY or NMI_MASK                 ;
                call    bin2cmos                                ;
                mov     ax,cx                                   ;
                shl     ax,3                                    ;
                and     ah,0Fh                                  ;
                mov     al,CMOS_MON or NMI_MASK                 ;
                call    bin2cmos                                ;
                shr     cx,9                                    ;
                add     cx,1980                                 ;
                mov     ax,cx                                   ;
                mov     cl,100                                  ;
                div     cl                                      ;
                mov     cl,al                                   ; century
                mov     al,CMOS_YRS or NMI_MASK                 ;
                call    bin2cmos                                ;
                mov     ah,cl
                mov     al,CMOS_CENT or NMI_MASK                ;
                call    bin2cmos                                ;
                popf                                            ;
                cmosset <CMOS_REG_B or NMI_MASK>                ; enable updates
                mov     al,CMOS_B_NORM                          ;
                out     CMOS_DATA,al                            ;
                cmosreset                                       ;
                ret     4                                       ;
_tm_setdate     endp


;----------------------------------------------------------------
; u32t _std tm_counter(void);
;
                public  _tm_counter
_tm_counter     proc    near
                mov     eax, dword ptr ss:[BIOS_TICS]
                ret
_tm_counter     endp

_TEXT           ends
                end
