
                .486p
                include inc/vbedata.inc
                include inc/graphdef.inc

_TEXT           segment para public USE32 'CODE'
                public  _con_setvgapal
_con_setvgapal  proc    near
                push    esi
                mov     esi,[esp+8]
                mov     ecx,768
                mov     edx,3DAh
@@setpal_wretr1:
                in      al,dx
                test    al,8
                jnz     @@setpal_wretr1
@@setpal_wretr2:
                in      al,dx
                test    al,8
                jz      @@setpal_wretr2
                mov     edx,3C8h
                xor     eax,eax
                out     dx,al
                inc     edx
                cld
            rep outsb
                pop     esi
                ret     4
_con_setvgapal  endp

                public  _con_getvgapal
_con_getvgapal  proc    near
                push    edi
                mov     edi,[esp+8]
                mov     ecx,768
                mov     edx,3C7h
                xor     eax,eax
                out     dx,al
                inc     dx
                inc     dx
                cld
            rep insb
                pop     edi
                ret     4
_con_getvgapal  endp

; 1. array of letters - 4 bytes width
; 2. width of letters <= 32
;void _std con_drawchar(con_drawinfo *cdi, u32t color, u32t bgcolor,
;                       u32t *chdata, u8t* mempos, u32t pitch, int cursor);
;
                public  _con_drawchar
_con_drawchar   proc    near
@@cursor        = dword ptr [ebp+20h]                           ;
@@bytes_in_line = dword ptr [ebp+1Ch]                           ;
@@mempos        = dword ptr [ebp+18h]                           ;
@@chdata        = dword ptr [ebp+14h]                           ;
@@bgcolor       = dword ptr [ebp+10h]                           ;
@@color         = dword ptr [ebp+0Ch]                           ;
@@cdi           = dword ptr [ebp+08h]                           ;

                push    ebp                                     ;
                mov     ebp,esp                                 ;
                push    ebx                                     ;
                push    esi                                     ;
                push    edi                                     ;

                mov     esi,@@chdata                            ;
                mov     edi,@@mempos                            ;
                mov     ebx,@@cdi                               ;
                mov     dh,[ebx].cdi_FontY                      ;
                xor     ecx,ecx                                 ; cursor color
                cld                                             ;
                test    @@cursor,-1                             ;
                jz      @@dch_line_loop                         ; ecx=0 if no
                mov     ecx,[ebx].cdi_CurColor                  ; cursor here
@@dch_line_loop:
                lodsd                                           ; font bits (one line)
                dec     dh                                      ;
                push    esi                                     ;
                mov     esi,eax                                 ;
                mov     al,[ebx].cdi_FontMult                   ;
@@dch_mult_loop:
                mov     dl,[ebx].cdi_FontX                      ;
                push    edi                                     ;
                push    eax                                     ;
                push    esi                                     ;
@@dch_point_loop:
                shl     esi,1                                   ;
                jc      @@dch_pix_txcol                         ;
                jecxz   @@dch_pix_bgcol                         ;
                xor     eax,eax                                 ; cursor here
                mov     al,dh                                   ; rare case, can
                bt      [ebx].cdi_CurMask,eax                   ; use slow code
                jnc     @@dch_pix_bgcol                         ;
                mov     eax,ecx                                 ;
                jmp     @@dch_putpix                            ;
@@dch_pix_txcol:
                mov     eax,@@color                             ; text
                jmp     @@dch_putpix                            ;
@@dch_pix_bgcol:
                mov     eax,@@bgcolor                           ; background
@@dch_putpix:
                push    ecx                                     ;
                movzx   ecx,[ebx].cdi_FontMult                  ;
                mov     bl,byte ptr [ebx].cdi_vbpps             ;
                test    bl,4                                    ;
                jnz     @@dch_putpix4                           ;
                test    bl,2                                    ;
                jz      @@dch_putpix1                           ;
                test    bl,1                                    ;
                jz      @@dch_putpix2                           ;
                bswap   eax                                     ; slow, but
                mov     ch,ah                                   ; 24-bit is slow
                bswap   eax                                     ; anyway
@@dch_putpix3:
                stosw                                           ;
                mov     es:[edi],ch                             ;
                inc     edi                                     ;
                dec     cl                                      ;
                jnz     @@dch_putpix3                           ;
                jmp     @@dch_pix_next                          ;
@@dch_putpix1:
            rep stosb
                jmp     @@dch_pix_next                          ;
@@dch_putpix4:
            rep stosd                                           ;
                jmp     @@dch_pix_next                          ;
@@dch_putpix2:
            rep stosw                                           ;
@@dch_pix_next:
                pop     ecx                                     ;
                dec     dl                                      ; next font dot
                mov     ebx,@@cdi                               ;
                jnz     @@dch_point_loop                        ;
                pop     esi                                     ; next screen line
                pop     eax                                     ;
                pop     edi                                     ;
                add     edi,@@bytes_in_line                     ;
                dec     al                                      ;
                jnz     @@dch_mult_loop                         ;
                pop     esi                                     ; next font line
                or      dh,dh                                   ;
                jnz     @@dch_line_loop                         ;

                pop     edi                                     ;
                pop     esi                                     ;
                pop     ebx                                     ;
                mov     esp,ebp                                 ;
                pop     ebp                                     ;
                ret     28                                      ;
_con_drawchar   endp

;void _std con_fillblock(void *Dest, u32t d_X, u32t d_Y, u32t LineLen_D, 
;                        u32t Color, u32t vbpps);
                public  _con_fillblock
_con_fillblock  proc    near
@@vbpps         = dword ptr [ebp+1Ch]                           ;
@@Color         = dword ptr [ebp+18h]                           ;
@@LineLen_D     = dword ptr [ebp+14h]                           ;
@@d_Y           = dword ptr [ebp+10h]                           ;
@@d_X           = dword ptr [ebp+0Ch]                           ;
@@Dest          = dword ptr [ebp+08h]                           ;

                mov     eax,[esp+08h] ; if (d_X||d_Y) {         ;
                or      eax,[esp+0Ch]                           ;
                jz      @@fbf_quit                              ;
                push    ebp                                     ;
                mov     ebp,esp                                 ;
                
                push    edi                                     ;
                push    ebx                                     ;
                mov     eax,@@Color                             ;
                mov     edi,@@Dest                              ;
@@fbf_loop1:
                mov     ecx,@@d_X                               ;
                push    edi                                     ;
                mov     edx,@@vbpps                             ;
                test    edx,4                                   ;
                jnz     @@fbf_loop2_4                           ;
                test    edx,2                                   ;
                jz      @@fbf_loop2_1                           ;
                test    edx,1                                   ;
                jz      @@fbf_loop2_2                           ;
                mov     ebx,eax                                 ;
                shr     ebx,8                                   ;
@@fbf_loop2_3:
                or      ecx,ecx                                 ;
                jz      @@fbf_quitloop2                         ;
                stosb                                           ;
                mov     [edi],bl                                ;
                inc     edi                                     ;
                mov     [edi],bh                                ;
                inc     edi                                     ;
                dec     ecx                                     ;
                jmp     @@fbf_loop2_3                           ;
@@fbf_loop2_1:
            rep stosb                                           ;
@@fbf_loop2_4:
            rep stosd                                           ;
@@fbf_loop2_2:
            rep stosw                                           ;
@@fbf_quitloop2:
                pop     edi                                     ;
                add     edi,@@LineLen_D                         ;
                dec     @@d_Y                                   ;
                jnz     @@fbf_loop1                             ;
                
                pop     ebx                                     ;
                pop     edi                                     ;
                ;mov     esp,ebp
                pop     ebp                                     ;
@@fbf_quit:
                ret     24                                      ;
_con_fillblock  endp

_TEXT           ends

                end
