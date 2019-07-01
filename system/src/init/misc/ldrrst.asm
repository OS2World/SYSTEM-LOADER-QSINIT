;
; QSINIT
; loader restart
; --------------------------------------------------------------
;
                include inc/segdef.inc                          ;
                include inc/filetab.inc                         ;
                include inc/cpudef.inc                          ;

; just a temporary space to place moveton`s PXE micro filetable
; during restart (it is abandoned after file open)
                FILETAB_SEG = 3500h

DATA16          segment                                         ;
                extrn   _filetable:filetable_s                  ;
DATA16          ends                                            ;

TEXT16          segment
                public  _LdrRstCode
                public  _LdrRstCS, LdrRstSS, LdrRstSP
                public  _dd_bootdisk, _dd_bootflags
                public  _dd_bpbofs, _dd_bpbseg
                public  _dd_rootofs, _dd_rootseg
                public  _dd_firstsector
_LdrRstCode     proc    near                                    ;
                assume  cs:nothing, ds:nothing, es:nothing, ss:nothing
                mov     eax,cr4                                 ; wipe QSINIT paging
                and     ax,not (CPU_CR4_PAE or CPU_CR4_PGE)     ; to not confuse
                mov     cr4,eax                                 ; feature users ;)
                xor     eax,eax                                 ;
                mov     cr3,eax                                 ;

                mov     al,_dd_bootflags                        ; fix moveton`s PXE
                test    al,BF_MICROFSD                          ; broken filetable
                jz      @@rstldt_common                         ; (use own cached copy)
                mov     si,offset _filetable                    ;
                cmp     [si].ft_cfiles,6                        ;
                jnz     @@rstldt_common                         ;
                
                xor     edi,edi                                 ;
                or      edi,[si].ft_muOpen                      ; open should be zero
                jnz     @@rstldt_common                         ;

                push    FILETAB_SEG                             ;
                pop     es                                      ;
                mov     cx,size filetable_s                     ;
                cld                                             ;
            rep movsb                                           ;
                xor     di,di                                   ;
                jmp     @@rstldt_run                            ;
@@rstldt_common:
                db      0B8h                                    ;
_dd_rootseg     dw      0                                       ; es
                mov     es, ax                                  ;
                db      0BFh                                    ;
_dd_rootofs     dw      0                                       ; di
@@rstldt_run:
                db      0B8h                                    ;
LdrRstSS        dw      0                                       ;
                cli                                             ;
                db      0BCh                                    ;
LdrRstSP        dw      0                                       ;
                mov     ss, ax                                  ;
                db      0BAh                                    ;
_dd_bootdisk    db      0                                       ; dx
_dd_bootflags   db      0                                       ; dh
                db      0BEh                                    ;
_dd_bpbofs      dw      0                                       ; si
                db      0B8h                                    ;
_dd_bpbseg      dw      0                                       ; ds
                mov     ds, ax                                  ;
                db      0BBh                                    ;
_dd_firstsector dw      0                                       ; bx
                                                                ;
                sti                                             ;
                db      0EAh                                    ;
                dw      0                                       ;
_LdrRstCS       dw      0                                       ;
_LdrRstCode     endp

TEXT16          ends
                end
