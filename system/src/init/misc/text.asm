;
; QSINIT
; text mode output
;----------------------------------------------------------------
; note, that code here should also support VESA text modes even it
; cannot set them. CONSOLE module uses this.
;
                include inc/segdef.inc
                include inc/basemac.inc
                include inc/qsinit.inc
                include inc/qstypes.inc

                extrn   _page0_fptr:dword

_B8000          equ     0B8000h

_DATA           segment
                public  _text_col, _max_x, _max_y               ;
                public  _cvio_ttylines                          ;
_cvio_ttylines  dd      0                                       ;
_pagesize       dw      0                                       ;
_max_x          dw      80                                      ;
_max_y          dw      25                                      ;
cursor_x        dw      0                                       ;
cursor_y        dw      0                                       ;
_text_col       dw      07h                                     ;
crt_port        dw      3D4h                                    ;
shape           dw      0                                       ;
tabstr          db      "    ",0                                ;
_DATA           ends

_TEXT           segment
                assume  cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT      ;

; initialize text output
;----------------------------------------------------------------
;
                public  vio_init
vio_init        proc    near                                    ;
                push    gs                                      ;
                mov     ax, SELZERO                             ;
                mov     gs, ax                                  ;
                mov     ax, gs:[463h]                           ; Controller index register
                mov     crt_port, ax                            ;
                mov     dl, gs:[484h]                           ;
                inc     dl                                      ; lines number
                mov     byte ptr _max_y, dl                     ;
                mov     ax, gs:[44Ah]                           ; columns number
                mov     _max_x, ax                              ;
                mul     dl                                      ;
                shl     ax, 1                                   ;
                mov     _pagesize, ax                           ;
                mov     ax, gs:[450h]                           ; current position for page 0
                pop     gs                                      ;
                mov     byte ptr cursor_x, al                   ;
                mov     byte ptr cursor_y, ah                   ;
                push    3                                       ; VIO_SHAPE_LINE
                call    _cvio_setshape                          ; set cursor shape
                ret                                             ;
vio_init        endp

;
; fini text output
;----------------------------------------------------------------
; return current line/col in ax
                public  vio_done
vio_done        proc    near
                mov     al, byte ptr cursor_x                   ; update cursor pos
                mov     ah, byte ptr cursor_y                   ;

                push    ebx                                     ;
                push    es                                      ;
                les     ebx,_page0_fptr                         ;
                mov     es:[ebx+450h], ax                       ;
                pop     es                                      ;
                pop     ebx                                     ;
                ret                                             ;
vio_done        endp

;
; print char
;----------------------------------------------------------------
; IN : AL = char, text_col = color, cursor_x & cursor_y
; OUT: EAX= 1 if eol was done

                public  _cvio_charout
_cvio_charout   proc    near                                    ;
                mov     eax, [esp]                              ; get return address
                add     esp, 4                                  ;
                xchg    eax, [esp]                              ; swap arg <-> ret addr.
vio_charout     label   near                                    ;
                cmp     al, 9                                   ;
                jz      @@vcr_tab                               ;
                push    edx                                     ;
                push    0                                       ; result for eax
                cmp     al, 10                                  ;
                jz      @@vcr_lf                                ;
                cmp     al, 13                                  ;
                jz      @@vcr_cr                                ;

                push    eax                                     ;
                movzx   eax, cursor_y                           ;
                mul     _max_x                                  ;
                add     ax, cursor_x                            ;
                shl     eax, 1                                  ;
                add     eax, _B8000                             ;
                pop     edx                                     ;
                mov     dh, byte ptr _text_col                  ;
                mov     [eax], dx                               ;
                inc     cursor_x                                ;
                mov     dx, _max_x                              ;
                cmp     cursor_x, dx                            ;
                jc      @@vcr_isbottom                          ;
@@vcr_lf:       
                inc     cursor_y                                ;
                inc     dword ptr [esp]                         ; eol counter
@@vcr_cr:        
                mov     cursor_x, 0                             ;
@@vcr_isbottom:          
                mov     ax, _max_y                              ;
                cmp     cursor_y, ax                            ;
                jae     @@vcr_scroll                            ;
@@vcr_exit:
                call    setpos                                  ;
                pop     eax                                     ;
                pop     edx                                     ;
                add     _cvio_ttylines, eax                     ;
                ret                                             ;
@@vcr_tab:
                mov     eax, offset tabstr                      ; print 4 spaces
                call    vio_strout                              ;
                ret                                             ;
@@vcr_scroll:
                SaveReg <ecx,esi,edi>                           ; scroll screen
                movzx   ecx, _pagesize                          ;
                movzx   esi, _max_x                             ;
                shl     esi, 1                                  ;
                sub     ecx, esi                                ;
                mov     eax, ecx                                ; save for the future
                shr     ecx, 2                                  ;
                mov     edi, _B8000                             ;
                add     esi, edi                                ;
                cld                                             ;
                rep     movsd                                   ;
                                                                ;
                mov     edi, eax                                ;
                add     edi, _B8000                             ;
                movzx   ecx, _max_x                             ;
                mov     ah, byte ptr _text_col                  ;
                mov     al, 0                                   ;
                rep     stosw                                   ;
                                                                ;
                mov     ax, cursor_y                            ;
                dec     ax                                      ;
                mov     cursor_y, ax                            ;
                RestReg <edi,esi,ecx>                           ;
                jmp     @@vcr_exit                              ;
_cvio_charout   endp                                            ;
;
; print string
;----------------------------------------------------------------
; IN: eax = string address (terminated by 0)
                public  _cvio_strout                            ;
_cvio_strout    proc    near                                    ;
                mov     eax, [esp]                              ; get return address
                add     esp, 4                                  ;
                xchg    eax, [esp]                              ; swap arg <-> ret addr.
vio_strout      label   near                                    ;
                push    esi                                     ;
                push    0                                       ; rc
                cld                                             ;
                mov     esi, eax                                ;
@@vst_loop:
                lodsb                                           ;
                or      al, al                                  ;
                jz      @@vst_eol                               ;
                call    vio_charout                             ;
                add     [esp], eax                              ;
                jmp     @@vst_loop                              ;
@@vst_eol:
                pop     eax                                     ;
                pop     esi                                     ;
                ret                                             ;
_cvio_strout    endp                                            ;


; update mode info
;----------------------------------------------------------------
                public  _vio_updateinfo
_vio_updateinfo proc    near                                    ;
                SaveReg <eax,ebx,gs>                            ;
                lgs     ebx, _page0_fptr                        ;
                mov     dl, gs:[ebx+484h]                       ;
                inc     dl                                      ; lines number
                mov     byte ptr _max_y, dl                     ;
                mov     ax, gs:[ebx+44Ah]                       ; columns number
                mov     _max_x, ax                              ;
                mul     dl                                      ;
                shl     ax, 1                                   ;
                mov     _pagesize, ax                           ;
                mov     ax, gs:[ebx+450h]                       ; current position for page 0
                mov     byte ptr cursor_x, al                   ;
                mov     byte ptr cursor_y, ah                   ;
                RestReg <gs,ebx,eax>                            ;
                ret                                             ;
_vio_updateinfo endp

;
; set current color
;----------------------------------------------------------------
;
                public  _cvio_setcolor                          ;
_cvio_setcolor  proc    near                                    ;
                mov     ax, [esp+4]                             ;
                mov     _text_col, ax                           ;
                ret     4                                       ;
_cvio_setcolor  endp                                            ;

;
; get current color
;----------------------------------------------------------------
;
                public  _cvio_getcolor                          ;
_cvio_getcolor  proc    near                                    ;
                movzx   eax, _text_col                          ;
                ret                                             ;
_cvio_getcolor  endp                                            ;

;
; clear screen
;----------------------------------------------------------------
;
                public  _cvio_clearscr                          ;
_cvio_clearscr  proc    near                                    ;
                SaveReg <eax,ecx,edi>                           ;
                mov     ch,byte ptr _text_col                   ;
                xor     cl,cl                                   ;
                shrd    eax,ecx,16                              ;
                mov     ax,cx                                   ;

                movzx   ecx,_pagesize                           ;
                mov     edi,_B8000                              ;
                shr     ecx,1                                   ;
                cld                                             ;
            rep stosw                                           ;
                mov     cursor_x,cx                             ;
                mov     cursor_y,cx                             ;
                call    setpos                                  ;
                RestReg <edi,ecx,eax>                           ;
                ret                                             ;
_cvio_clearscr  endp                                            ;
;
; set cursor position
;----------------------------------------------------------------
; IN:  AH = Y, AL = X
                public  _cvio_setpos                            ;
_cvio_setpos    proc    near                                    ;
                mov     eax, [esp]                              ; get return address
                xchg    eax, [esp+8]                            ; exchange with variable
                mov     ah, [esp+4]                             ;
                add     esp, 8                                  ; fix stack ptr
vio_setpos      label   near                                    ;
                mov     byte ptr cursor_x, al                   ;
                mov     al,ah                                   ;
                xchg    byte ptr cursor_y, ah                   ;
                sub     al,ah                                   ; update output line
                movsx   eax,al                                  ; counter
                add     _cvio_ttylines,eax                      ;
                call    setpos                                  ;
                ret                                             ;
_cvio_setpos    endp                                            ;
;
; set hardware cursor position
;----------------------------------------------------------------
; IN:  cursor_x, cursor_y
setpos          proc    near                                    ;
                SaveReg <eax,ebx,edx,gs>                        ;
                mov     ax, cursor_y                            ;
                mul     _max_x                                  ;
                add     ax, cursor_x                            ;
                mov     bl, al                                  ;
                mov     dx, crt_port                            ;
                mov     al, 0Eh                                 ;
                out     dx, ax                                  ;
                inc     al                                      ;
                mov     ah, bl                                  ;
                out     dx, ax                                  ;
                lgs     edx, _page0_fptr                        ; update BIOS pos
                mov     al, byte ptr cursor_x                   ;
                mov     ah, byte ptr cursor_y                   ;
                mov     gs:[edx+450h], ax                       ;
                RestReg <gs,edx,ebx,eax>                        ;
                ret                                             ;
setpos          endp                                            ;
;
; set cursor shape
;----------------------------------------------------------------
; IN:   AH = start line,  AL = end line
;       if bit D5 <AH> = 1 then cursor hidden
;       bit D5 D6 <AL> -> cursor disp
setshape        proc    near                                    ;
                SaveReg <eax,ebx,edx,gs>                        ;
                lgs     ebx, _page0_fptr                        ; save in bios data
                mov     gs:[ebx+460h], ax                       ;
                mov     bl, al                                  ;
                mov     al, 0Ah                                 ;
                mov     dx, crt_port                            ;
                out     dx, ax                                  ;
                inc     al                                      ;
                mov     ah, bl                                  ;
                out     dx, ax                                  ;
                RestReg <gs,edx,ebx,eax>                        ;
                ret                                             ;
setshape        endp

;
; get cursor shape
;----------------------------------------------------------------
; OUT:  ax = VIO_SHAPE_* constant (0..4)
                public  _cvio_getshape                          ;
_cvio_getshape  proc    near                                    ;
                movzx   eax, shape                              ; cursor shape
                ret                                             ;
_cvio_getshape  endp

;
; set one of default cursor shapes
;----------------------------------------------------------------
                public  _cvio_setshape                          ;
_cvio_setshape  proc    near                                    ;
                mov     dl, [esp+4]                             ;
                mov     al,dl                                   ;
                cbw                                             ;
                mov     shape, ax                               ; save current shape
                sub     dl, 4                                   ;
                jz      @@vdshape_hide                          ;
                jnc     @@vdshape_exit                          ; > VIO_SHAPE_NONE
                push    gs                                      ;
                lgs     ecx, _page0_fptr                        ;
                mov     al, gs:[ecx+485h]                       ;
                pop     gs                                      ;
                cmp     al, 11h                                 ;
                jc      @@vdshape_fix                           ; fix wrong value
                mov     al, 10h                                 ;
@@vdshape_fix:
                mov     ah, al                                  ; bottom line
                add     ah, 2                                   ; -2 for 14 & 16
                shr     ah, 3                                   ; -1 for 8
                sub     al, ah                                  ;
                mov     ah, al                                  ;
                inc     dl                                      ; 
                jz      @@vdshape_set                           ; VIO_SHAPE_LINE
                dec     ah                                      ;
                inc     dl                                      ;
                jz      @@vdshape_set                           ; VIO_SHAPE_WIDE
                inc     ah                                      ;
                shr     ah, 1                                   ;
                inc     dl                                      ;
                jz      @@vdshape_set                           ; VIO_SHAPE_HALF
                xor     ah, ah                                  ;
                jmp     @@vdshape_set                           ; VIO_SHAPE_FULL
@@vdshape_hide:
                mov     shape, 4                                ; fix possible wrong values
                mov     ax, 2000h                               ; hide cursor
@@vdshape_set:
                call    setshape                                ;
@@vdshape_exit:
                ret     4                                       ;
_cvio_setshape  endp

;
; get hardware cursor position
;----------------------------------------------------------------
; IN:  *line, *col
                public  _cvio_getpos                            ;
_cvio_getpos    proc    near                                    ;
                SaveReg <eax,ecx>                               ;
                xor     eax, eax                                ;
                mov     ecx, [esp+16]                           ;
                jecxz   @@vgp_nox                               ;
                mov     ax, cursor_x                            ;
                mov     [ecx], eax                              ;
@@vgp_nox:
                mov     ecx, [esp+12]                           ;
                jecxz   @@vgp_noy                               ;
                mov     ax, cursor_y                            ;
                mov     [ecx], eax                              ;
@@vgp_noy:
                RestReg <ecx,eax>                               ;
                ret     8
_cvio_getpos    endp

_TEXT           ends
                end
