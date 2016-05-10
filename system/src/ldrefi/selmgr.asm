;
; QSINIT
; EFI loader - set/get selector descriptor
; -------------------------------------------
; code was just copied from PMODE to make mostly the same reaction
;
                include inc/qstypes.inc                         ;
                include inc/segdef.inc                          ;

_DATA           segment
                extrn   _gdt_size:word
                extrn   _gdt_lowest:word
                extrn   _gdt_pos:dword
_DATA           ends

_TEXT           segment
                assume  cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT
;-----------------------------------------------------------------------------
; test for valid selector in BX
testsel:
                cmp     bx,_gdt_size                            ; selector BX out of range?
                jnc     @@selerr                                ;
                mov     edi,_gdt_pos                            ; base of GDT
                and     bl,0F8h                                 ; mask offset table index and RPL
                movzx   ebx,bx                                  ; EBX = selector index
                test    byte ptr [edi+ebx+6],10h                ; is descriptor used?
                jz      @@selerr                                ;
                clc                                             ;
                ret                                             ;
@@selerr:
                stc                                             ;
                ret                                             ;
;-----------------------------------------------------------------------------
; test access bits in CX
testaccess:                                                     
                test    ch,20h                                  ; test MUST BE 0 bit in CH
                jnz     @@selerr                                ; if not 0, error
                test    cl,80h                                  ; test present bit
                jz      @@selerr                                ; if 0, error
                test    cl,10h                                  ;
                jz      @@testselok                             ; system? skip tests

                test    cl,8                                    ; if code, more tests needed
                jz      @@testselok                             ; if data, skip code tests

                test    cl,6                                    ; test conforming and readable bits
                jz      @@selerr                                ; if both 0, error
                jpe     @@selerr                                ; if equal, error
@@testselok:
                clc                                             ;
                ret                                             ;

; get descriptor
; ---------------------------------------------------------------
; in bx - selector, edi - buffer for descriptor
                public  getseldesc
getseldesc      proc    near                                    
                push    edi                                     ;
                call    testsel                                 ; test for valid selector BX
                jc      @@gdesc_err
                push    esi
                lea     esi,[edi+ebx]                           ; ESI -> descriptor in GDT
                mov     edi,[esp+4]                             ; get EDI from stack
                cld                                             ;
                pushfd                                          ;
                cli                                             ;
                movsd                                           ; copy descriptor
                movsd                                           ;
                popfd                                           ;
                pop     esi                                     ;
@@gdesc_err:
                pop     edi                                     ;
                ret                                             ;
getseldesc      endp

; set descriptor
; ---------------------------------------------------------------
; in bx - selector, edi - descriptor
                public  setseldesc
setseldesc      proc    near                                    
                push    edi                                     ;
                call    testsel                                 ; test selector
                jc      @@sdesc_err                             ;
                push    esi                                     ;
                push    ecx                                     ;
                mov     esi,[esp+8]                             ; ESI = EDI from stack
                mov     cx,[esi+5]                              ; get access rights
                call    testaccess                              ; test access bits in CX
                jc      @@sdesc_accerr                          ;
                add     edi,ebx                                 ; adjust EDI to descriptor in GDT
                cld                                             ;
                pushfd                                          ;
                cli                                             ;
                movsd                                           ; copy descriptor
                lodsd                                           ;
                or      eax,100000h                             ; set descriptor AVL bit
                stosd                                           ;
                popfd                                           ;
@@sdesc_accerr:
                pop     ecx                                     ;
                pop     esi                                     ;
@@sdesc_err:
                pop     edi                                     ;
                ret
setseldesc      endp

; free descriptor
; ---------------------------------------------------------------
; in bx - selector
                public  selfree
selfree         proc    near
                push    edi
                push    ebx
                call    testsel                                 ; test for valid selector BX
                jc      @@sfree_err                             ;
                pushfd                                          ;
                cli                                             ;
                and     byte ptr [edi+ebx+6],0EFh               ; mark descriptor as free
                mov     ebx,[esp]                               ; org. sel value
                xor     eax,eax                                 ;
                mov     di,fs                                   ;
                cmp     di,bx                                   ; check FS/GS
                jnz     @@sfree_nofs                            ; for cleared
                mov     fs,ax                                   ; selector
@@sfree_nofs:
                mov     di,gs                                   ;
                cmp     di,bx                                   ;
                jnz     @@sfree_nogs                            ;
                mov     gs,ax                                   ;
@@sfree_nogs:
                popfd                                           ; rest IF & CF
@@sfree_err:
                pop     ebx                                     ;
                pop     edi                                     ;
                ret
selfree         endp

; allocate descriptors
;----------------------------------------------------------------
;u16t hlp_selalloc(u32t count);
                public  _hlp_selalloc
_hlp_selalloc   proc    near
@@count         =  4                                            ;
                mov     ecx,[esp+@@count]                       ;
                push    ebx                                     ;
                pushfd                                          ;
                cli                                             ;
                jecxz   @@salloc_err                            ;
                mov     edx,_gdt_pos                            ; base of GDT
                movzx   eax,_gdt_size                           ;
                dec     eax                                     ; EAX = last selector index
                and     al,0F8h                                 ;
                mov     ebx,ecx                                 ; number of selectors to alloc
@@salloc_loop:
                test    byte ptr [edx+eax+6],10h                ; is descriptor used?
                jnz     @@salloc_used                           ;
                dec     ebx                                     ; found free, dec counter
                jnz     @@salloc_free                           ; continue if need to find more
                mov     ebx,eax                                 ; found all of requested
@@salloc_setuploop:
                mov     dword ptr [edx+ebx],0                   ; set entire new descriptor
                mov     dword ptr [edx+ebx+4],109200h           ;
                add     ebx,8                                   ; increment selector index
                dec     ecx                                     ; dec # of descriptors
                jnz     @@salloc_setuploop                      ; loop if more to mark
                jmp     @@salloc_exit                           ; return ok, with eax
@@salloc_used:
                mov     ebx,ecx                                 ; reset number of selectors
@@salloc_free:
                sub     eax,8                                   ; dec current selector counter
                cmp     ax,_gdt_lowest                          ; more descriptors to go?
                jae     @@salloc_loop                           ; if yes, loop
@@salloc_err:
                xor     eax,eax                                 ;
@@salloc_exit:
                popfd                                           ;
                pop     ebx                                     ;
                ret     4                                       ;
_hlp_selalloc   endp                                            ;

_TEXT           ends
                end
