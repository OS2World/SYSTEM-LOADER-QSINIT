;
; QSINIT
; debug/log output
;

                include inc/segdef.inc
                include inc/qstypes.inc
                include inc/basemac.inc
                include inc/serial.inc
                include inc/debug.inc
                include inc/qsinit.inc

                .486

;DATA16          segment                                         ;
                extrn   _ComPortAddr:word                       ;
                extrn   _BaudRate:dword                         ;
                extrn   _logrmbuf:dword                         ;
                extrn   _logrmpos:word                          ;
;DATA16          ends                                            ;

                extrn   _strlen:near                            ;
                extrn   vio_charout:near                        ;

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
                pop     edx                                     ;
                pop     eax                                     ;
                push    edx                                     ;
                jmp     vio_charout                             ;
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
                movzx   eax,_ComPortAddr                        ;
                or      eax,eax                                 ;
                jz      @@serinfo_nobaud                        ;
                mov     ecx,[esp+@@baudrate]                    ;
                jecxz   @@serinfo_nobaud                        ;
                mov     edx,_BaudRate                           ;
                mov     [ecx],edx                               ;
@@serinfo_nobaud:
                ret     4                                       ;
_hlp_seroutinfo endp                                            ;

;----------------------------------------------------------------
; void log_buffer(int level, const char* msg);
                public  _log_buffer                             ;
_log_buffer     proc    near                                    ;
@@lb_loglevel   =  4                                            ;
@@lb_logmsg     =  8                                            ;
                push    [esp+@@lb_logmsg]                       ;
                call    _strlen                                 ;
                mov     ecx,LOGBUF_SIZE-3                       ;
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
                mov     eax,[esp+@@lb_loglevel+4]               ; log flags
                inc     ecx                                     ;
                push    esi                                     ;
                mov     esi,[esp+@@lb_logmsg+8]                 ;
                stosb                                           ; copying message
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

