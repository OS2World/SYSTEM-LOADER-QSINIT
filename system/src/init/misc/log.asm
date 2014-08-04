;
; QSINIT
; debug/log output
;
                NOSTORSAVE  = 1
                NOSERIALDEF = 1
                include inc/segdef.inc
                include inc/qstypes.inc
                include inc/basemac.inc
                include inc/serial.inc
                include inc/debug.inc
                include inc/qsinit.inc

                .486

_TEXT           segment
                extrn   vio_charout16:near
                extrn   vio_charout:near
_TEXT           ends

_DATA           segment                                         ;

                extrn   _logbufseg:word                         ;
                extrn   _ZeroAddress:dword                      ; phys 0 address
                extrn   pm16data:word                           ;
                public  _ComPortAddr                            ;
                public  _BaudRate                               ;
                public  _logrmbuf, _logrmpos                    ;
                align   4                                       ;
_logrmbuf       dd      0                                       ;
_BaudRate       dd      BD_115200                               ; current dbport baud rate
_ComPortAddr    dw      0                                       ;
_logrmpos       dw      0                                       ;

ifdef INITDEBUG
vio_out_proc    dw      offset printchar_rm                     ; vio prn char for
                dw      offset vio_charout16                    ; RM and PM
endif
_DATA           ends                                            ;

_BSS            segment
                public  _storage_w                              ;
_storage_w      db      STO_BUF_LEN * STOINIT_ESIZE dup(?)      ;
_storage_w_end  label   near                                    ;
_BSS            ends

;================================================================
_TEXT           segment
                assume  cs:_TEXT,ds:DGROUP,es:nothing,ss:DGROUP

                public  _setbaudrate
_setbaudrate    proc    near                                    ;
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

;                add     dx,COM_FCR                              ;
;                xor     al,al                                   ;
;                out     dx,al                                   ; disable FIFO
@@setbr_exit:
                retf                                            ;
_setbaudrate    endp                                            ;

                public  _earlyserinit                           ;
_earlyserinit   proc    far                                     ;
                mov     dx,_ComPortAddr                         ;
                or      dx,dx                                   ;
                jz      @@esi_exit                              ;
;                inc     dx                                      ; disable all interrupts
;                xor     al,al                                   ;
;                out     dx,al                                   ;
;                dec     dx                                      ;
                                                                ;
                push    dx                                      ;
                push    cs                                      ;
                call    _setbaudrate                            ;
                pop     dx                                      ;
                add     dx,COM_MCR                              ;
                in      al,dx                                   ;
                or      al,3                                    ; RTS/DSR set
                out     dx,al                                   ;
@@esi_exit:
                ret                                             ;
_earlyserinit   endp                                            ;
;
; char appending to last line in realmode log buffer
; zero assumed at _logbufseg pos (end of string char)
; if char prior to zero is \n - new log line will be started
;
                public  _seroutchar_w
_seroutchar_w   proc    near                                    ;
                push    dx                                      ;
                push    ax                                      ;
                mov     dx,cs:_ComPortAddr                      ; port addr present?
                or      dx,dx                                   ;
                jz      @@serout16_exit                         ;
                add     dx,COM_LSR                              ; status port
@@serout16_wait:
                in      al,dx                                   ; check status
                test    al,020h                                 ;
                jz      @@serout16_wait                         ; wait
                mov     dx,cs:_ComPortAddr                      ;
                mov     al,[esp+6]                              ; character from stack
                out     dx,al                                   ;

                cmp     al,13                                   ; skip \r
                jz      @@serout16_exit                         ;
                push    di                                      ;
                mov     di,cs:_logrmpos                         ;
                cmp     di,LOGBUF_SIZE-5                        ; rm buffer is full?
                jnc     @@serout16_logskip                      ;
                push    es                                      ;
                mov     es,cs:_logbufseg                        ;
                cld                                             ;
                or      di,di                                   ; 1st line in buffer?
                jz      @@serout16_firstline                    ;
                cmp     byte ptr es:[di-1],10                   ; last char is '\n'?
                jnz     @@serout16_logadd                       ;
                inc     di                                      ; skip 0
@@serout16_firstline:
                mov     byte ptr es:[di],32h                    ; flags + level
                inc     di                                      ;
@@serout16_logadd:
                stosb                                           ;
                mov     byte ptr es:[di],0                      ; end of string
                mov     cs:_logrmpos,di                         ;
                pop     es                                      ;
@@serout16_logskip:
                pop     di                                      ;
@@serout16_exit:
ifdef INITDEBUG                                                 ;
                smsw    dx                                      ; additional output
                and     dx,1                                    ; to screen while 16bit
                xchg    dx,bx                                   ; init active
                shl     bx,1                                    ;
                mov     bx, cs:vio_out_proc[bx]                 ;
                xchg    dx,bx                                   ;
                call    dx                                      ;
endif
                pop     ax                                      ;
                pop     dx                                      ;
                ret     2                                       ;
_seroutchar_w   endp

                public  printchar_rm
printchar_rm    proc    near
                assume  ds:nothing, ss:nothing

                SaveReg <ds,es>                                 ; preserve registers
                pusha                                           ;
                mov     bx, 0B800h                              ; text memory
                mov     ds, bx                                  ;
                mov     bx, 40h                                 ; bios data 40h
                mov     es, bx                                  ;

                mov     cx, es:[50h]                            ; cursor pos
                xor     bx, bx                                  ;
                cmp     al, 0Ah                                 ; EOL?
                je      @@eol                                   ;
                cmp     al, 0Dh                                 ; CR?
                jnz     @@nocr                                  ;
                xor     cl, cl                                  ;
                jmp     @@savepos                               ;
@@nocr:
                mov     bl, ch                                  ; offset
                imul    bx, 160                                 ;
                xor     ch, ch                                  ;
                shl     cx, 1                                   ;
                add     bx, cx                                  ; printing
                mov     ah, 0Ch                                 ; color
                mov     [bx], ax                                ;
                mov     cx, es:[50h]                            ; update cursor pos
                inc     cl                                      ; ch - row
                cmp     cl, 80                                  ;
                jc      @@savepos                               ;
@@eol:
                xor     cl, cl                                  ; new line
                movzx   bx, byte ptr es:[84h]                   ; number of lines
                cmp     ch, bl                                  ;
                jnz     @@noscroll                              ;
                mov     es:[50h], cx                            ; save cursor pos
                push    ds                                      ;
                pop     es                                      ;
                mov     si, 160                                 ; scrolling ;)
                xor     di, di                                  ;
                imul    cx, bx, 80                              ; (lines-1) * 80
            rep movsw                                           ;
                mov     cl, 80h                                 ;
                mov     ax, 0720h                               ; cleaning last line
            rep stosw                                           ;
                jmp     @@exit                                  ;
@@noscroll:
                inc     ch                                      ; новая строка
@@savepos:
                mov     es:[50h],cx                             ; сохраняем позицию курсора
@@exit:
                popa                                            ;
                RestReg <es,ds>                                 ;
                ret                                             ;
printchar_rm    endp

;
; get dgroup segment/selector (dual mode rm/pm proc)
;----------------------------------------------------------------
; OUT : AX - value
get_dgroup      proc    near
                smsw    ax                                      ;
                test    al,1                                    ;
                jz      @@getds_rm                              ;
                mov     ax,pm16data                             ;
                ret                                             ;
@@getds_rm:
                mov     ax,cs                                   ;
                ret                                             ;
get_dgroup      endp

;
; save dword into storage (dual mode rm/pm proc)
;----------------------------------------------------------------
; IN : EAX - dword, SI - name, save all except flags
                public  sto_save_w
sto_save_w      proc    near
                pushad                                          ;
                SaveReg <ds,es>                                 ;
                push    eax                                     ; save value
                call    get_dgroup                              ; ds=es=dgroup
                mov     ds,ax                                   ;
                mov     es,ax                                   ;
                mov     bx,offset _storage_w                    ;
                xor     bp,bp                                   ; empty entry
@@svs_loop:
                lea     di,[bx+9]                               ;
                cmp     byte ptr [di],0                         ; name empty?
                jz      @@svs_empty                             ;
                push    si                                      ;
                mov     cx,11                                   ;
           repe cmpsb                                           ;
                pop     si                                      ;
                jnz     @@svs_nextloop                          ;
                jmp     @@svs_ready                             ;
@@svs_empty:
                mov     bp,bx                                   ; remember it
@@svs_nextloop:
                add     bx,STOINIT_ESIZE                        ;
                cmp     bx,offset _storage_w_end                ;
                jnz     @@svs_loop                              ; next entry
                mov     bx,bp                                   ; try to get empty
@@svs_ready:
                pop     eax                                     ;
                or      bx,bx                                   ; entry founded?
                jz      @@svs_failed                            ;
                mov     di,bx                                   ; store data &
                stosd                                           ; name
                mov     al,4                                    ;
                stosb                                           ; lo size byte
                xor     eax,eax                                 ;
                stosd                                           ; hi size + flag
                mov     cx,11                                   ;
            rep movsb                                           ;
@@svs_failed:
                RestReg <es,ds>                                 ;
                popad                                           ;
                ret                                             ;
sto_save_w      endp

_TEXT           ends
;================================================================

CODE32          segment
                assume  cs:CODE32,ds:DGROUP,es:DGROUP,ss:DGROUP

                extrn   _strlen:near                            ;

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
                sub     eax,_ZeroAddress                        ;
                mov     edx,ecx                                 ;
                shl     edx,1                                   ;
                add     dx, offset @@errbrk_dr0                 ;
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
                mov     dr2,eax                                 ;
                ret                                             ;
_sys_errbrk     endp

CODE32          ends
                end

