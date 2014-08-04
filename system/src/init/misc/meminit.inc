;
; QSINIT
; query physical memory info
;

                MIN_ACPI_INFO_SIZE = 20                         ;
                MEM_USABLE         = 1                          ;
                ACPI_MEMTABLE_SIZE = 10                         ; number of entries for int 15-E820

                SMAP_CONST           = 534D4150h                ; "SMAP"

AcpiInfo        struc
BaseAddrLow     dd      ?
BaseAddrHigh    dd      ?
LengthLow       dd      ?
LengthHigh      dd      ?
AcpiMemType     dd      ?
AcpiInfo        ends

_BSS            segment
_AcpiMemCnt     dw      ?                                       ;
_Int1588Mem     dw      ?                                       ;
_Int15E8Mem     dw      ?                                       ;
                public  _AcpiInfoBuf, _AcpiMemCnt               ;
                public  _Int1588Mem, _Int15E8Mem                ;
_AcpiInfoBuf    db (ACPI_MEMTABLE_SIZE*SIZE AcpiInfo) dup (?)   ; int 15-E820 memory info
_BSS            ends

_TEXT           segment
init_memory     proc    near
                assume  cs:dgroup,ds:dgroup,es:dgroup,ss:dgroup

                push    ds
                pop     es

                xor     ebx,ebx
                mov     _Int1588Mem,bx                          ; zero both old way results
                mov     _Int15E8Mem,bx                          ;
                mov     _AcpiMemCnt,bx                          ;
                mov     edx, SMAP_CONST                         ;
                lea     di,_AcpiInfoBuf                         ; 
@@FindMemRange:
                mov     eax,0E820h                              ; int 15-E820 read cycle 
                mov     ecx,MIN_ACPI_INFO_SIZE                  ; read by one value
                int     15h                                     ;
                jc      @@AcpiListEnd                           ;
                cmp     AcpiInfo.AcpiMemType[di], MEM_USABLE    ;
                jnz     @@memi1                                 ; non-free range, skip it
                add     di,MIN_ACPI_INFO_SIZE                   ;
                inc     _AcpiMemCnt                             ;
                cmp     _AcpiMemCnt,ACPI_MEMTABLE_SIZE          ;
                jnc     @@AcpiListEnd                           ; >10 entries - exiting away ;)
@@memi1:
                or      ebx,ebx                                 ; 
                jnz     @@FindMemRange                          ; loop until error
@@AcpiListEnd:
                xor     ebx,ebx                                 ;
                cmp     _AcpiMemCnt,bx                          ; found something?
                jnz     @@skip_15E8                             ;
                mov     ah,88h                                  ; get 1-64Mb memory via int 15-88
                int     15h                                     ; 
                cmp     ax,0FFFCh                               ; truncate value to 64Mb
                jb      @@memi2                                 ;
                mov     ax,0FFFCh                               ;
@@memi2:
                mov     _Int1588Mem,ax                          ;

                mov     ax,0E801h                               ; int 15-E801 read
                int     15h                                     ; 16b-64Mb memory
                jc      @@skip_15E8                             ; supported?

                cmp     _Int1588Mem,03C01h                      ; int 15-88 return <15Mb?
                jb      @@verify_15E8                           ; compare with 15E8
                                                                ; int 15-88 return > 15Mb
                cmp     ax,03C00h                               ; int 15-E8 return <=15Mb
                jg      @@skip_15E8                             ; no? - skip it
                mov     _Int1588Mem,ax                          ;
                jmp     @@15E8_ok                               ;
@@verify_15E8:
                cmp     _Int1588Mem,ax                          ; ax = # of 1K pages found?
                jne     @@skip_15E8                             ; don't use bx if they mismatched
@@15E8_ok:
                mov     _Int15E8Mem,bx                          ; 
@@skip_15E8:
                ret
init_memory     endp

; do not save nothing, called via DPMI only, saving result to disk i/o buffer
                public  _getfullSMAP                            ;
                assume  cs:_TEXT,ds:nothing,es:nothing,ss:DGROUP;
_getfullSMAP    proc    far                                     ;
                xor     ebx, ebx                                ;
                mov     es, cs:_DiskBufRM_Seg                   ;
                xor     di, di                                  ;
                mov     edx, SMAP_CONST                         ;
@@gfsmap_loop:
                mov     eax, 0E820h                             ; 
                mov     ecx, MIN_ACPI_INFO_SIZE                 ; read by one value
                int     15h                                     ;
                jc      @@gfsmap_end                            ;
                add     di, MIN_ACPI_INFO_SIZE                  ;
                or      ebx, ebx                                ; 
                jnz     @@gfsmap_loop                           ; loop until error
@@gfsmap_end:
                xor     ax, ax                                  ; zero last entry
                mov     cx, MIN_ACPI_INFO_SIZE shr 1            ;
            rep stosw                                           ;
                ret                                             ;
_getfullSMAP    endp                                            ;

_TEXT           ends

