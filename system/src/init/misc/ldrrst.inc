;
; QSINIT
; loader restart
; --------------------------------------------------------------
; placed to separate segment at the end of code to remove it from
; 1st page, because this function used as mixed code/data.

TEXT16          segment
                public  _LdrRstCode
                public  _LdrRstCS
                public  _dd_bootdisk
                public  _dd_bootflags
                public  _dd_bpbofs
                public  _dd_bpbseg
                public  _dd_rootofs
                public  _dd_rootseg
                public  _dd_firstsector
_LdrRstCode     proc    near                                    ;
                assume  cs:nothing, ds:nothing, es:nothing, ss:nothing
                mov     ax, 0E2Ah                               ;
                int     10h                                     ;

                mov     eax,cr4                                 ; wipe QSINIT paging
                and     ax,not (CPU_CR4_PAE or CPU_CR4_PGE)     ; to not confuse
                mov     cr4,eax                                 ; feature users ;)
                xor     eax,eax                                 ;
                mov     cr3,eax                                 ;

                db      0B8h                                    ;
LdrRstSS        label   near                                    ;
                dw      0                                       ;
                cli                                             ;
                db      0BCh                                    ;
LdrRstSP        label   near                                    ;
                dw      0                                       ;
                mov     ss, ax                                  ;
                db      0BAh                                    ;
_dd_bootdisk    db      0                                       ; dx
_dd_bootflags   db      0                                       ; dh
                db      0BEh                                    ;
_dd_bpbofs      label   near                                    ; si
                dw      0                                       ;
                db      0B8h                                    ;
_dd_bpbseg      label   near                                    ; ds
                dw      0                                       ;
                mov     ds, ax                                  ;
                db      0B8h                                    ;
_dd_rootseg     label   near                                    ; es
                dw      0                                       ;
                mov     es, ax                                  ;
                db      0BFh                                    ;
_dd_rootofs     label   near                                    ; di
                dw      0                                       ;
                db      0BBh                                    ;
_dd_firstsector label   near                                    ; bx
                dw      0                                       ;
                                                                ;
                sti                                             ;
                db      0EAh                                    ;
                dw      0                                       ;
_LdrRstCS       label   near                                    ;
                dw      0                                       ;
_LdrRstCode     endp
TEXT16          ends
