;
; QSINIT
; text mode output
;
                include inc/segdef.inc
                include inc/basemac.inc
                include inc/qsinit.inc
                include inc/qstypes.inc

_DATA           segment
                extrn   _B8000:dword
                extrn   __0400:dword
                public  _text_col, _max_x, _max_y
_pagesize       dw      0
_max_x          dw      80
_max_y          dw      25
cursor_x        dw      0
cursor_y        dw      0
_text_col       dw      07h
crt_port        dw      3D4h
shape           dw      0
tabstr          db      "    ",0
_DATA           ends


_TEXT           segment
                assume  cs:DGROUP,ds:DGROUP,es:DGROUP,ss:DGROUP
;
; initialize text output
;----------------------------------------------------------------
;
                public  vio_init
vio_init        proc    near                                    ;
                SaveReg <ax,bx,dx,gs>                           ; preserve registers
                mov     bx, SELZERO                             ; gs:FLAT
                mov     gs, bx                                  ;
                mov     bx, gs:[463h]                           ; Controller index register
                mov     crt_port, bx                            ;
                mov     dl, gs:[484h]                           ;
                inc     dl                                      ; lines number
                mov     byte ptr _max_y, dl                     ;
                mov     ax, gs:[44Ah]                           ; columns number
                mov     _max_x, ax                              ;
                mul     dl                                      ;
                shl     ax, 1                                   ;
                mov     _pagesize, ax                           ;
                mov     ax, gs:[450h]                           ; current position for page 0
                mov     byte ptr cursor_x, al                   ;
                mov     byte ptr cursor_y, ah                   ;
                mov     ax, gs:[460h]                           ; cursor shape
                mov     shape, ax                               ;
                RestReg <gs,dx,bx,ax>                           ;
                ret                                             ;
vio_init        endp

                public  vio_charout16
vio_charout16   proc    near                                    ;
                push    ds                                      ;
                push    es                                      ;
                mov     dx,SEL32DATA                            ;
                mov     es,dx                                   ; setup ds, es
                mov     ds,dx                                   ; but leave original ss
                CallF32w SEL32CODE, vio_charoutf                ; (it must be flat or
                pop     es                                      ;  QS init ss)
                pop     ds                                      ;
                ret                                             ;
vio_charout16   endp

_TEXT           ends

CODE32          segment
                assume  cs:CODE32,ds:DGROUP,es:DGROUP,ss:DGROUP

;
; fini text output
;----------------------------------------------------------------
; return current line/col in ax
                public  vio_done
vio_done        proc    near
                push    ebx                                     ;
                mov     ebx, __0400                             ;
                mov     al, byte ptr cursor_x                   ; update cursor pos
                mov     ah, byte ptr cursor_y                   ;
                mov     [ebx+50h], ax                           ;
                pop     ebx                                     ;
                ret                                             ;
vio_done        endp

;
; print char
;----------------------------------------------------------------
; IN : AL = char, text_col = color, cursor_x & cursor_y
; OUT: EAX= 1 if eol was done

                public  _vio_charout
                public  vio_charout

vio_charoutf    label   far                                     ;
                push    eax                                     ;
                call    vio_charout                             ;
                pop     eax                                     ;
                retf                                            ;
_vio_charout    label   near                                    ;
                mov     eax, [esp]                              ; get return address
                add     esp, 4                                  ;
                xchg    eax, [esp]                              ; swap arg <-> ret addr.
vio_charout     proc    near                                    ;
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
                cmp     _text_col, 100h                         ; colored output?
                jae     @@vcr_nocolor                           ;
                mov     dh, byte ptr _text_col                  ;
                mov     [eax+1], dh                             ;
@@vcr_nocolor:          
                mov     [eax], dl                               ;
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
vio_charout     endp                                            ;
;
; print string
;----------------------------------------------------------------
; IN: eax = string address (terminated by 0)
                public  vio_strout                              ;
                public  _vio_strout                             ;

_vio_strout     label   near                                    ;
                mov     eax, [esp]                              ; get return address
                add     esp, 4                                  ;
                xchg    eax, [esp]                              ; swap arg <-> ret addr.
vio_strout      proc    near                                    ;
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
vio_strout      endp                                            ;


; update mode info
;----------------------------------------------------------------
                public  _vio_updateinfo
_vio_updateinfo proc    near                                    ;
                SaveReg <eax,ebx>                               ;
                mov     ebx, __0400                             ;
                mov     dl, [ebx+84h]                           ;
                inc     dl                                      ; lines number
                mov     byte ptr _max_y, dl                     ;
                mov     ax, [ebx+4Ah]                           ; columns number
                mov     _max_x, ax                              ;
                mul     dl                                      ;
                shl     ax, 1                                   ;
                mov     _pagesize, ax                           ;
                mov     ax, [ebx+50h]                           ; current position for page 0
                mov     byte ptr cursor_x, al                   ;
                mov     byte ptr cursor_y, ah                   ;
                mov     ax, [ebx+60h]                           ; cursor shape
                mov     shape, ax                               ;
                RestReg <ebx,eax>                               ;
                ret                                             ;
_vio_updateinfo endp

;
; clear screen
;----------------------------------------------------------------
;
                public  _vio_setcolor                           ;
_vio_setcolor   proc    near                                    ;
                mov     ax, [esp+4]                             ;
                mov     _text_col, ax                           ;
                ret     4                                       ;
_vio_setcolor   endp                                            ;

;
; clear screen
;----------------------------------------------------------------
;
                public  _vio_clearscr                           ;
_vio_clearscr   proc    near                                    ;
                SaveReg <eax,ecx,edi>                           ;
                mov     ax, 0700h                               ;
                cmp     _text_col, 100h                         ;
                jae     @@clearscr100                           ;
                mov     ah, byte ptr _text_col                  ;
@@clearscr100:                                                  ;
                mov     cx, ax                                  ;
                shl     eax, 16                                 ;
                mov     ax, cx                                  ;
                                                                ;
                movzx   ecx, _pagesize                          ;
                mov     edi, _B8000                             ;
                shr     ecx, 2                                  ;
                cld                                             ;
                rep     stosd                                   ;
                mov     cursor_x, cx                            ;
                mov     cursor_y, cx                            ;
                call    setpos                                  ;
                RestReg <edi,ecx,eax>                           ;
                ret                                             ;
_vio_clearscr   endp                                            ;
;
; set cursor position
;----------------------------------------------------------------
; IN:  AH = Y, AL = X
                public  _vio_setpos                             ;
                public  vio_setpos                              ;
_vio_setpos     label   near                                    ;
                mov     eax, [esp]                              ; get return address
                xchg    eax, [esp+8]                            ; exchange with variable
                mov     ah, [esp+4]                             ;
                add     esp, 8                                  ; fix stack ptr
vio_setpos      proc    near                                    ;
                mov     byte ptr cursor_y, ah                   ;
                mov     byte ptr cursor_x, al                   ;
                call    setpos                                  ;
                ret                                             ;
vio_setpos      endp                                            ;
;
; set hardware cursor position
;----------------------------------------------------------------
; IN:  cursor_x, cursor_y
setpos          proc    near                                    ;
                SaveReg <eax,ebx,edx>                           ;
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
                mov     edx, __0400                             ; update BIOS pos
                mov     al, byte ptr cursor_x                   ;
                mov     ah, byte ptr cursor_y                   ;
                mov     [edx+50h], ax                           ;
                RestReg <edx,ebx,eax>                           ;
                ret                                             ;
setpos          endp                                            ;
;
; set cursor shape
;----------------------------------------------------------------
; IN:   AH = start line,  AL = end line
;       if bit D5 <AH> = 1 then cursor hidden
;       bit D5 D6 <AL> -> cursor disp
                public  _vio_setshape
                public  vio_setshape
_vio_setshape   label   near                                    ;
                mov     eax, [esp]                              ; get return address
                xchg    eax, [esp+8]                            ; exchange with variable
                mov     ah, [esp+4]                             ;
                add     esp, 8                                  ; fix stack ptr
vio_setshape    proc    near                                    ;
                mov     shape, ax                               ; save current shape
                SaveReg <eax,ebx,edx>                           ;
                mov     ebx, __0400                             ; save in bios data
                mov     [ebx+60h], ax                           ;
                mov     bl, al                                  ;
                mov     al, 0Ah                                 ;
                mov     dx, crt_port                            ;
                out     dx, ax                                  ;
                inc     al                                      ;
                mov     ah, bl                                  ;
                out     dx, ax                                  ;
                RestReg <edx,ebx,eax>                           ;
                ret                                             ;
vio_setshape    endp

;
; get cursor shape
;----------------------------------------------------------------
; OUT:  AH = start line,  AL = end line
                public  _vio_getshape
_vio_getshape   proc    near                                    ;
                movzx   eax, shape                              ; cursor shape
                ret                                             ;
_vio_getshape   endp

;
; set one of default cursor shapes
;----------------------------------------------------------------
                public  _vio_defshape
_vio_defshape   proc    near                                    ;
                mov     dl, [esp+4]                             ;
                sub     dl, 4                                   ;
                jz      @@vdshape_hide                          ;
                jnc     @@vdshape_exit                          ; > VIO_SHAPE_NONE
                mov     ecx, __0400                             ;
                mov     al, [ecx+85h]                           ;
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
                mov     ax, 2000h                               ; hide cursor
@@vdshape_set:
                call    vio_setshape                            ;
@@vdshape_exit:
                ret     4                                       ;
_vio_defshape   endp

;
; get hardware cursor position
;----------------------------------------------------------------
; IN:  *line, *col
                public  _vio_getpos
_vio_getpos     proc    near                                    ;
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
_vio_getpos     endp

CODE32          ends
                end
