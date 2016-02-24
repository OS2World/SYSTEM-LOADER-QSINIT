;
; QSINIT
; debug/log output
;

                include inc/segdef.inc
                include inc/qstypes.inc
                include inc/basemac.inc
                include inc/serial.inc
                include inc/qsinit.inc

                .486

                extrn   _ComPortAddr:word                       ;
                extrn   _BaudRate:dword                         ;
                extrn   _logrmbuf:dword                         ;
                extrn   _logrmpos:word                          ;

                extrn   _strlen:near                            ;
                extrn   _vio_charout:near                       ;
                extrn   _tm_getdate:near                        ;

_TEXT           segment
                assume  cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT

                public  _hlp_seroutchar                         ;
_hlp_seroutchar proc    near                                    ;
@@outchar       =  4                                            ;
                mov     dx,cs:_ComPortAddr                      ;
                or      dx,dx                                   ;
                jz      @@outch_exit                            ;
                add     dx,COM_LSR                              ;
@@outch_state:
                in      al,dx                                   ;
                test    al,20h                                  ;
                jz      @@outch_state                           ;
                mov     dx,cs:_ComPortAddr                      ;
                mov     al,[esp+@@outchar]                      ;
                out     dx,al                                   ;
ifdef INITDEBUG
                ret     4                                       ;
@@outch_exit:                                                   ;
                jmp     _vio_charout                            ;
else
@@outch_exit:
                ret     4                                       ;
endif
_hlp_seroutchar endp                                            ;
;----------------------------------------------------------------
                public  _hlp_seroutstr                          ;
_hlp_seroutstr  proc    near                                    ;
@@str           =  4                                            ;
                push    esi                                     ;
                mov     esi,[esp+4+@@str]                       ;
                xor     eax,eax                                 ;
                cld                                             ;
@@outstr_loop:
                lodsb                                           ;
                or      al,al                                   ;
                jz      @@outstr_bye                            ;
                cmp     al,10                                   ; check for correct
                jnz     @@outstr_norm                           ; \r\n sequence.
                cmp     ah,13                                   ; if \r is absent - add one
                jz      @@outstr_norm                           ;
                push    13                                      ;
                call    _hlp_seroutchar                         ;
                mov     al,10                                   ;
@@outstr_norm:
                mov     ah,al
                push    eax                                     ;
                call    _hlp_seroutchar                         ;
                jmp     @@outstr_loop                           ;
@@outstr_bye:
                pop     esi
                ret     4                                       ;
_hlp_seroutstr  endp                                            ;

;----------------------------------------------------------------
; u16t _std hlp_seroutinfo(u32t *baudrate)
                public  _hlp_seroutinfo                         ;
_hlp_seroutinfo proc    near                                    ;
@@baudrate      =  4                                            ;
                movzx   eax,_ComPortAddr                        ; return current baud
                mov     ecx,[esp+@@baudrate]                    ; rate in any case
                jecxz   @@serinfo_nobaud                        ; (required for
                mov     edx,_BaudRate                           ;  DBCARD command)
                mov     [ecx],edx                               ;
@@serinfo_nobaud:
                ret     4                                       ;
_hlp_seroutinfo endp                                            ;

; set debug port baud rate (from BaudRate variable)
;----------------------------------------------------------------
; void setbaudrate(void);
                public  _setbaudrate
_setbaudrate    proc    near                                    ;
                push    ebx                                     ;
                mov     ecx,_BaudRate                           ; ECX = new baud rate
                mov     ax,CLOCK_RATEL                          ;
                shl     eax,16                                  ;
                mov     ax,CLOCK_RATEH                          ;
                xor     edx,edx                                 ; EDX:EAX = clock rate
                div     ecx                                     ;
                mov     bx,ax                                   ; BX = clock rate / baud rate

                mov     dx,_ComPortAddr                         ;
                or      dx,dx                                   ; IF port not found
                jz      @@setbr_exit                            ; THEN exit
                add     dx,COM_LCR                              ; DX -> LCR
                in      al,dx                                   ; AL = current value of LCR
                or      al,LC_DLAB                              ; Turn on DLAB
                out     dx,al                                   ;

                add     dx,COM_DLM-COM_LCR                      ; DX -> MSB of baud latch
                mov     al,bh                                   ; AL = divisor latch MSB
                out     dx,al                                   ;
                dec     dx                                      ; DX -> LSB of baud latch
                mov     al,bl                                   ; AL = divisor latch LSB
                out     dx,al                                   ; Set LSB of baud latch

                add     dx,COM_LCR-COM_DLL                      ; DX -> LCR
                mov     al,3                                    ; AL = same mode as in main
                out     dx,al                                   ;
@@setbr_exit:
                pop     ebx                                     ;
                ret                                             ;
_setbaudrate    endp                                            ;

; init debug com port
;----------------------------------------------------------------
; void earlyserinit(void);
                public  _earlyserinit                           ;
_earlyserinit   proc    near                                    ;
                mov     dx,_ComPortAddr                         ;
                or      dx,dx                                   ;
                jz      @@esi_exit                              ;
                push    edx                                     ;
                call    _setbaudrate                            ;
                pop     edx                                     ;
                add     dx,COM_MCR                              ;
                in      al,dx                                   ;
                or      al,3                                    ; RTS/DSR set
                out     dx,al                                   ;
@@esi_exit:
                ret                                             ;
_earlyserinit   endp                                            ;

;----------------------------------------------------------------
; void log_buffer(int level, const char* msg);
                public  _log_buffer                             ;
_log_buffer     proc    near                                    ;
@@lb_loglevel   =  4                                            ;
@@lb_logmsg     =  8                                            ;
                push    [esp+@@lb_logmsg]                       ;
                call    _strlen                                 ;
                mov     ecx,LOGBUF_SIZE-3-4                     ;
                movzx   edx,_logrmpos                           ;
                sub     ecx,edx                                 ;
                jc      @@lb_putfailed                          ;
                sub     ecx,eax                                 ; no space for
                jc      @@lb_putfailed                          ; new string
                push    edi                                     ;
                mov     edi,edx                                 ;
                mov     ecx,eax                                 ; str length
                add     edi,_logrmbuf                           ;
                or      edx,edx                                 ; 1st line?
                jz      @@logbuf_start                          ; no, skip final zero
                inc     edi                                     ; from previous line
@@logbuf_start:
                push    ecx                                     ; get current time
                call    _tm_getdate                             ;
                pop     ecx                                     ;
                mov     edx,[esp+@@lb_loglevel+4]               ; log flags
                jnc     @@logbuf_oddsecond                      ;
                or      dl,4                                    ; LOGIF_SECOND
@@logbuf_oddsecond:
                inc     ecx                                     ;
                push    esi                                     ;
                mov     esi,[esp+@@lb_logmsg+8]                 ;
                mov     [edi],dl                                ; copying message
                inc     edi                                     ;
                stosd                                           ;
            rep movsb                                           ;
                sub     edi,_logrmbuf                           ;
                dec     edi                                     ; last 0 is not
                mov     _logrmpos,di                            ; included
                pop     esi                                     ;
                pop     edi                                     ;
@@lb_putfailed:
                ret     8                                       ;
_log_buffer     endp                                            ;


;----------------------------------------------------------------
; void _std sys_errbrk(void *access, u8t index, u8t size, u8t type);
                public  _sys_errbrk                             ;
_sys_errbrk     proc    near
                mov     ecx,[esp+8]                             ; index
                and     ecx,3                                   ;
                shl     ecx,1                                   ;
                mov     eax,dr7                                 ; disable bp
                btr     eax,ecx                                 ;
                mov     dr7,eax                                 ;
                mov     eax,[esp+4]                             ; set address
;                sub     eax,_ZeroAddress                        ;
                mov     edx,ecx                                 ;
                shl     edx,1                                   ;
                add     edx, offset @@errbrk_dr0                ;
                call    edx                                     ;
                mov     dl,[esp+12]                             ; size
                dec     dl                                      ;
                and     dl,3                                    ;
                shl     dl,2                                    ;
                mov     al,[esp+16]                             ; type
                and     eax,3                                   ;
                or      al,dl                                   ;
                shl     eax,16                                  ; dr7 bits
                shl     eax,cl                                  ;
                shl     eax,cl                                  ;
                mov     al,3                                    ;
                shl     al,cl                                   ;

                mov     edx,0Fh                                 ; mask for len
                shl     edx,cl                                  ; and r/w
                shl     edx,cl                                  ;
                shl     edx,16                                  ;
                not     edx                                     ;

                mov     ecx,dr7                                 ; enable bp
                and     ecx,edx                                 ;
                or      ecx,eax                                 ;
                mov     dr7,ecx                                 ;
                ret     16
@@errbrk_dr0:
                mov     dr0,eax                                 ; 4 bytes len
                ret                                             ; used above
@@errbrk_dr1:
                mov     dr1,eax                                 ;
                ret                                             ;
@@errbrk_dr2:
                mov     dr2,eax                                 ;
                ret                                             ;
@@errbrk_dr3:
                mov     dr3,eax                                 ;
                ret                                             ;
_sys_errbrk     endp

_TEXT           ends
                end

