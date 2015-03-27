;
; QSINIT
; common PM process
;

                .386p
                include inc/qstypes.inc
                include inc/segdef.inc
                include inc/basemac.inc
                include inc/dpmi.inc
                include inc/seldesc.inc
                include inc/debug.inc
; ---------------------------------------------------------------
_BSS16          segment                                         ;
                public  _pminitres                              ;
_pminitres      db      ?                                       ;
                extrn   _rm16code:word                          ;
_BSS16          ends                                            ;
; ---------------------------------------------------------------
LOADER          segment                                         ;
                extrn   _prepare32:near                         ;
LOADER          ends                                            ;
; ---------------------------------------------------------------
_TEXT           segment                                         ;
                extrn   _init32:near                            ;
                extrn   _exit_prepare:near                      ;
                extrn   vio_charout:near                        ;
                extrn   vio_init:near                           ; text mode prn init/done
                extrn   vio_done:near                           ;
                extrn   rmstop:near                             ;
                public  init32call                              ;
                public  _exit_pm32                              ;
                public  _exit_pm32s                             ;
                assume  cs:_TEXT, ds:DGROUP, es:DGROUP, ss:DGROUP
init32call      label   near                                    ;
                call    vio_init                                ;
                call    _init32                                 ;
; return from init32() here                                     ;
_exit_pm32a:
                push    eax                                     ; rc code
                jmp     _exit_pm32s                             ;
_exit_pm32:
                pop     eax                                     ; drop return addr
_exit_pm32s:
                call    _exit_prepare                           ; call exitlist
                mov     al,13                                   ;
                call    vio_charout                             ; print eol
                mov     al,10                                   ;
                call    vio_charout                             ;
                call    vio_done                                ;
                pop     ebx                                     ; rc
ifdef INITDEBUG
                dbg32print <2>,<"exiting pm, rc=%d",10>,<ebx>   ;
endif
; ---------------------------------------------------------------
                or      ebx,ebx                                 ;
                jnz     rmstop                                  ;
                mov     bl,9                                    ;
                jmp     rmstop                                  ;
_TEXT           ends                                            ;
; ---------------------------------------------------------------
;
; 16 bit pm code
;

TEXT16          segment                                         ;
                assume  cs:TEXT16, ds:G16, es:nothing, ss:G16   ;
                public  pm16set                                 ;
; here we start in 16 bits PM (going from start.asm)
pm16set:
                mov     _pminitres,al                           ; entry point
ifdef INITDEBUG
                dbg16print <"hi!",10>                           ;
endif
                xor     eax,eax                                 ;
                mov     ax,offset _prepare32                    ; jump to 32-bit code
                movzx   ecx,_rm16code                           ; with flat DS/ES, but
                shl     ecx,PARASHIFT                           ; 16-bit current stack
                add     ecx,eax                                 ; selector
                mov     ax,SEL32CODE                            ;
                push    eax                                     ;
                push    ecx                                     ;
                mov     ax,SEL32DATA                            ;
                mov     es,ax                                   ; set flat ds & es
                mov     ds,ax                                   ;
                db      66h                                     ;
                retf                                            ;
TEXT16          ends                                            ;
                end
