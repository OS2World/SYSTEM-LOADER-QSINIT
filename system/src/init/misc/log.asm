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
                extrn   _mod_secondary:dword                    ;

                extrn   _strlen:near                            ;
                extrn   _vio_charout:near                       ;
                extrn   _tm_getdate:near                        ;
                extrn   _mt_swlock:near                         ;
                extrn   _mt_swunlock:near                       ;
                extrn   _checkbaudrate:near                     ;

_TEXT           segment
                assume  cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT

;----------------------------------------------------------------
; void  _std  hlp_serialout(u16t port, u8t chr);
                public  _hlp_serialout                          ;
_hlp_serialout  proc    near                                    ;
                mov     dx,[esp+4]                              ;
                or      dx,dx                                   ;
                jz      @@soch_exit                             ;
                add     dx,COM_LSR                              ;
@@soch_state:
                in      al,dx                                   ;
                test    al,20h                                  ;
                jz      @@soch_state                            ;
                mov     dx,[esp+4]                              ;
                mov     al,[esp+8]                              ;
                out     dx,al                                   ;
@@soch_exit:
                ret     8                                       ;
_hlp_serialout  endp                                            ;

;----------------------------------------------------------------
; void  _std  hlp_seroutchar(u32t symbol);
                public  _hlp_seroutchar                         ;
_hlp_seroutchar proc    near                                    ;
@@outchar       =  4                                            ;
                mov     dx,cs:_ComPortAddr                      ;
                or      dx,dx                                   ;
                jz      @@outch_exit                            ;

                mov     eax,_mod_secondary                      ; check dbport_lock
                or      eax,eax                                 ; field in mod_secondary
                jz      @@outch_ok                              ;
                test    dword ptr [eax+4],-1                    ; !!!
                jnz     @@outch_exit                            ;
@@outch_ok:
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
                call    _mt_swlock                              ;
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
                mov     ah,al                                   ;
                push    eax                                     ;
                call    _hlp_seroutchar                         ;
                jmp     @@outstr_loop                           ;
@@outstr_bye:
                call    _mt_swunlock                            ;
                pop     esi                                     ;
                ret     4                                       ;
_hlp_seroutstr  endp                                            ;

;----------------------------------------------------------------
; u16t _std hlp_seroutinfo(u32t *baudrate)
                public  _hlp_seroutinfo                         ;
_hlp_seroutinfo proc    near                                    ;
@@baudrate      =  8                                            ;
                pushfd                                          ;
                cli                                             ;
                movzx   eax,_ComPortAddr                        ; return current baud
                mov     ecx,[esp+@@baudrate]                    ; rate in any case
                jecxz   @@serinfo_nobaud                        ; (required for
                mov     edx,_BaudRate                           ;  DBCARD command)
                mov     [ecx],edx                               ;
@@serinfo_nobaud:
                popfd                                           ;
                ret     4                                       ;
_hlp_seroutinfo endp                                            ;

; set debug port baud rate (from BaudRate variable)
;----------------------------------------------------------------
; int _std hlp_serialrate(u16t port, u32t baudrate);

                public  _hlp_serialrate
_hlp_serialrate proc    near                                    ;
                xor     eax,eax                                 ;
                or      ax,[esp+4]                              ; port is non-zero
                jz      @@setbr_exit                            ;
                push    [esp+8]                                 ;
                call    _checkbaudrate                          ; validate in rate table
                or      eax,eax                                 ;
                jz      @@setbr_exit                            ;

                call    _mt_swlock                              ;
                mov     ecx,[esp+8]                             ; new baud rate
                mov     ax,CLOCK_RATEL                          ;
                shl     eax,16                                  ;
                mov     ax,CLOCK_RATEH                          ;
                xor     edx,edx                                 ; edx:eax = clock rate
                div     ecx                                     ;
                mov     cx,ax                                   ; cx = clock rate / baud rate

                mov     dx,[esp+4]                              ;
                add     dx,COM_LCR                              ; dx -> LCR (+3)
                in      al,dx                                   ; al = current value of LCR
                or      al,LC_DLAB                              ; Turn on DLAB
                out     dx,al                                   ;

                add     dx,COM_DLM-COM_LCR                      ; dx -> MSB of baud latch (+1)
                mov     al,ch                                   ; al = divisor latch MSB
                out     dx,al                                   ;
                dec     dx                                      ; dx -> LSB of baud latch
                mov     al,cl                                   ; al = divisor latch LSB
                out     dx,al                                   ; Set LSB of baud latch

                add     dx,COM_LCR-COM_DLL                      ; dx -> LCR (+3)
                mov     al,3                                    ; al = same mode as in main
                out     dx,al                                   ;
                dec     dx                                      ; dx -> FCR (+2)
                mov     al,0C7h                                 ; clear & enable FIFO, 14 chars
                out     dx,al                                   ;
                and     eax,1                                   ; result code
                call    _mt_swunlock                            ;
@@setbr_exit:
                ret     8                                       ;
_hlp_serialrate endp                                            ;

;----------------------------------------------------------------
; u8t   _std  hlp_serialin(u16t port);
                public  _hlp_serialin                           ;
_hlp_serialin   proc    near                                    ;
                xor     eax,eax                                 ; no char
                mov     dx,[esp+4]                              ;
                or      dx,dx                                   ; port present?
                jz      @@in_ch_exit                            ;
                add     dx,COM_LSR                              ; data ready?
                in      al,dx                                   ;
                test    al,1                                    ;
                setnz   al                                      ; al = 0
                jz      @@in_ch_exit                            ; exit
                mov     dx,[esp+4]                              ;
                in      al,dx                                   ;
@@in_ch_exit:
                ret     4                                       ;
_hlp_serialin   endp                                            ;

; init debug com port
;----------------------------------------------------------------
; void _std serial_init(u16t port);
                public  _serial_init                            ;
_serial_init    proc    near                                    ;
                mov     dx,[esp+4]                              ;
                or      dx,dx                                   ;
                jz      @@esi_exit                              ;
                push    edx                                     ;
                push    _BaudRate                               ;
                push    edx                                     ;
                call    _hlp_serialrate                         ;
                pop     edx                                     ;
                add     dx,COM_MCR                              ;
                in      al,dx                                   ;
                or      al,3                                    ; RTS/DSR set
                out     dx,al                                   ;
                add     dx,COM_IEN-COM_MCR                      ;
                xor     al,al                                   ; disable interrupts
                out     dx,al                                   ;
@@esi_exit:
                ret     4                                       ;
_serial_init    endp                                            ;

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
_sys_errbrk     proc    near                                    ;
@@ebrk_type     = 20                                            ;
@@ebrk_size     = 16                                            ;
@@ebrk_index    = 12                                            ;
@@ebrk_address  =  8                                            ;
                pushfd                                          ;
                cli                                             ;
                mov     ecx,[esp+@@ebrk_index]                  ; index
                and     ecx,3                                   ;
                shl     ecx,1                                   ;
                mov     eax,dr7                                 ; disable bp
                btr     eax,ecx                                 ;
                mov     dr7,eax                                 ;
                mov     eax,[esp+@@ebrk_address]                ; set address
;                sub     eax,_ZeroAddress                        ;
                mov     edx,ecx                                 ;
                shl     edx,1                                   ;
                add     edx, offset @@errbrk_dr0                ;
                call    edx                                     ;
                mov     dl,[esp+@@ebrk_size]                    ; size
                dec     dl                                      ;
                and     dl,3                                    ;
                shl     dl,2                                    ;
                mov     al,[esp+@@ebrk_type]                    ; type
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
                popfd                                           ; enable ints
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

