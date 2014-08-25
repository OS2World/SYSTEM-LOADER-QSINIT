;
; QSINIT
; 16bit tiny print subset
; note: used both in qsinit & doshlp binary
;
; void __cdecl printf16(char far *fmt,...);
;
;   %x       - print argument as a hex word (16 bit)
;   %d       - print argument as decimal word (16 bit)
;   %c       - print argument as ascii character
;   %b[nn]   - print argument as hex byte (far 16 bit, nn bytes)
;   %s[nn]   - print argument as string (far 16 bit, nn chars)
;   %ld      - print argument as decimal dword (32 bit)
;   %lx      - print argument as a hex dword (32 bit)
;
;   \n \t - supported in DOSHLP_BUILD only.
;

                include inc/qstypes.inc
                include inc/segdef.inc                          ; segments
; ----------------------------------
ifdef DOSHLP_BUILD
DOSHLP_CODE     segment
                extrn   DHSerOut:near
print_char macro                                                ;
                push    cs                                      ;
                call    DHSerOut                                ;
endm
else ; -----------------------------
TEXT16          segment
                extrn   _seroutchar_w:near
print_char macro                                                ;
                push    ax                                      ;
                call    _seroutchar_w                           ;
endm
endif
; ----------------------------------
                assume  cs:nothing, ds:nothing, es:nothing, ss:nothing

                public  _printf16
_printf16       proc    far                                     ;
                push    bp                                      ;
                mov     bp,sp                                   ;
                pushf                                           ;
                pushad                                          ;
                push    ds                                      ;
                push    gs                                      ;
                lds     si,[bp+6]                               ; fmt string
                mov     di,10                                   ; parameters
                xor     ebx,ebx                                 ; 3 high bytes must be cleared!
@@dhp_charloop_ah:
                xor     ah,ah                                   ; clean prev. char
@@dhp_charloop:
                lodsb                                           ;
                or      al,al                                   ;
                jz      @@dhp_exit                              ;
                cmp     al,'%'                                  ;
                jnz     @@dhp_fmtprn                            ;
@@dhp_fmtarg:
                lodsb                                           ;
                or      al,al                                   ;
                jz      @@dhp_exit                              ;
                cmp     al,'s'                                  ;
                jz      @@dhp_string                            ;
                cmp     al,'x'                                  ;
                jz      @@dhp_hex4                              ;
                cmp     al,'d'                                  ;
                jz      @@dhp_short                             ;
                cmp     al,'c'                                  ;
                jz      @@dhp_char                              ;
                cmp     al,'b'                                  ;
                jz      @@dhp_binary                            ;
                cmp     al,'l'                                  ;
                jnz     @@dhp_fmtprn                            ;
                lodsb                                           ;
                or      al,al                                   ; %lx %ld
                jz      @@dhp_exit                              ;
                cmp     al,'x'                                  ;
                jz      @@dhp_hex8                              ;
                cmp     al,'d'                                  ;
                jnz     @@dhp_charloop_ah                       ;
@@dhp_dword:
                mov     ecx,[bp][di]                            ; print %ld
                add     di,4                                    ;
                jmp     @@dhp_unsigned                          ;
@@dhp_short:
                xor     ecx,ecx                                 ;
                mov     cx,[bp][di]                             ; print %d
                add     di,2                                    ;
@@dhp_unsigned:
                mov     ebx,1000000000                          ;
                push    si                                      ;
                xor     si,si                                   ;
@@dhp_uns_loop:
                mov     eax,ecx                                 ;
                xor     edx,edx                                 ;
                div     ebx                                     ;
                mov     ecx,edx                                 ;
                or      si,ax                                   ;
                jz      @@dhp_uns_hi                            ;
@@dhp_uns_zero:
                add     al,'0'                                  ;
                print_char                                      ;
                inc     si                                      ; chars printed flag
@@dhp_uns_hi:
                mov     eax,0CCCCCCCDh                          ; div 10
                mul     ebx                                     ;
                shr     edx,3                                   ;
                mov     ebx,edx                                 ;
                jnz     @@dhp_uns_loop                          ;
                xor     al,al                                   ;
                or      si,si                                   ; no chars printed?
                jz      @@dhp_uns_zero                          ; print at least one
                pop     si                                      ;
                jmp     @@dhp_charloop_ah                       ;
@@dhp_exit:
                pop     gs                                      ;
                pop     ds                                      ;
                popad                                           ;
                popf                                            ;
                mov     sp,bp                                   ;
                pop     bp                                      ;
                ret                                             ;
ifdef DOSHLP_BUILD
@@dhp_escape:
                mov     dl,[si]                                 ; \n \t
                or      dl,dl                                   ; if not - out
                jz      @@dhp_fmt_noeol                         ; both \ and symbol
                cmp     dl,'n'                                  ;
                jz      @@dhp_escape_n                          ;
                cmp     dl,'t'                                  ;
                jnz     @@dhp_fmt_noeol                         ;
                mov     al,9                                    ;
                print_char                                      ;
                inc     si                                      ;
                jmp     @@dhp_charloop_ah                       ;
@@dhp_escape_n:
                inc     si                                      ;
                mov     al,13                                   ;
                print_char                                      ;
                mov     al,10                                   ;
                print_char                                      ;
                jmp     @@dhp_charloop_ah                       ;
endif
@@dhp_char:
                mov     al,[bp][di]                             ; print %c
                add     di,2                                    ;
@@dhp_fmtprn:
ifdef DOSHLP_BUILD
                cmp     al,'\'                                  ;
                jz      @@dhp_escape                            ;
endif
                cmp     al,10                                   ; print char
                jnz     @@dhp_fmt_noeol                         ;
                cmp     ah,13                                   ;
                jz      @@dhp_fmt_noeol                         ;
                mov     al,13                                   ;
                dec     si                                      ;
@@dhp_fmt_noeol:
                mov     ah,al                                   ;
                print_char                                      ;
                jmp     @@dhp_charloop                          ;
@@dhp_string:
                call    @@dhp_getint                            ; print %s
                push    si                                      ;
                lgs     si,[bp][di]                             ;
                add     di,4                                    ;
@@dhp_stringloop:
                lods    byte ptr gs:[si]                        ;
                or      al,al                                   ;
                jz      @@dhp_stringend                         ;
                print_char                                      ;
                loop    @@dhp_stringloop                        ;
@@dhp_stringend:
                pop     si                                      ;
                jmp     @@dhp_charloop_ah                       ;
@@dhp_hex8:
                add     al,2                                    ; print %x, %lx
@@dhp_hex4:
                sub     al,'x'-2                                ;
                mov     cl,al                                   ; number of bytes
                xor     ch,ch                                   ;
                add     di,cx                                   ;
                push    di                                      ;
@@dhp_hexloop:
                dec     di                                      ;
                mov     dl,[bp][di]                             ;
                xor     dh,dh                                   ;
                stc                                             ;
@@dhp_hexloopchar:
                rcl     dx,4                                    ;
                and     dh,0Fh                                  ;
                mov     bl,dh                                   ;
                mov     al,cs:dhp_ABCDEF[bx]                    ;
                print_char                                      ;
                cmp     dl,80h                                  ;
                clc                                             ;
                jnz     @@dhp_hexloopchar                       ;
                loop    @@dhp_hexloop                           ;
                pop     di                                      ;
                jmp     @@dhp_charloop_ah                       ;
@@dhp_binary:
                call    @@dhp_getint                            ; print %b
                push    si                                      ;
                lgs     si,[bp][di]                             ;
                add     di,4                                    ;
                xor     dh,dh                                   ;
                or      ax,ax                                   ;
                jz      @@dhp_binloop                           ;
                neg     cx                                      ; make 1 from -1
@@dhp_binloop:
                lods    byte ptr gs:[si]                        ;
                mov     dl,al                                   ;
                xor     dh,dh                                   ;
                stc                                             ;
@@dhp_binloopbyte:
                rcl     dx,4                                    ;
                and     dh,0Fh                                  ;
                mov     bl,dh                                   ;
                mov     al,cs:dhp_ABCDEF[bx]                    ;
                print_char                                      ;
                cmp     dl,80h                                  ;
                clc                                             ;
                jnz     @@dhp_binloopbyte                       ;
                mov     al,' '                                  ;
                print_char                                      ;
                loop    @@dhp_binloop                           ;
                pop     si                                      ;
                jmp     @@dhp_charloop_ah                       ;
; get int in cx, or FFFFh. return ax=0 if cx<>-1
@@dhp_getint:
                xor     cx,cx                                   ;
                xor     ah,ah                                   ;
@@dhp_getint_loop:
                lodsb                                           ;
                or      al,al                                   ;
                jz      @@dhp_getint_end                        ;
                sub     al,'0'                                  ;
                jc      @@dhp_getint_end                        ;
                cmp     al,10                                   ;
                jnc     @@dhp_getint_end                        ;
                imul    cx,cx,10                                ;
                add     cx,ax                                   ;
                jmp     @@dhp_getint_loop                       ;
@@dhp_getint_end:
                dec     si                                      ;
                or      cx,cx                                   ; return -1
                setz    al                                      ; if cx was zero
                neg     ax                                      ;
                or      cx,ax                                   ;
                retn                                            ;
dhp_ABCDEF      label   byte                                    ;
ifdef DOSHLP_BUILD
                db      '0123456789ABCDEF'                      ;
else
                public  _alphabet
_alphabet       label   near
                db      '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'  ; used in utoa
endif
_printf16       endp

ifdef DOSHLP_BUILD
DOSHLP_CODE     ends
else
TEXT16          ends
endif
                end
