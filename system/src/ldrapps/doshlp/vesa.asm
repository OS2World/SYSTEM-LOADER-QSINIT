;
; QSINIT
; vesa detection
;

                include inc/segdef.inc                          ; segment declarations
                include inc/vesainfo.inc                        ; loader vesa info table
                include inc/vbedata.inc                         ; vesa mode info struct

MODEBUF         equ     200h

                public  _VesaDetect

_DATA           segment
                public  _VesaInfo
                public  _VesaMask
                public  _VesaNoLFB
_VesaInfo       VesaInfoBuf <>
                ; mode mask from LOGOMASK ini parameter
                ; M640x480x8+M640x480x15+M640x480x16+M640x480x24+M640x480x32
_VesaMask       dw      0
ModeCheckTable8 dw      408h
ModeCheckTable  dw      60Fh,610h,618h,620h
_VesaNoLFB      dw      0
_M3Seg          dw      0
_DATA           ends

_TEXT           segment
_VesaDetect     proc    near
                assume  cs:nothing, ds:_DATA, es:nothing, ss:nothing
                pushad                                          ;
                mov     _M3Seg,ax                               ; buffer seg
                push    es                                      ;
                mov     es,ax                                   ; copying mask from
                mov     ax,es:[0]                               ; buffer, it was saved
                mov     _VesaMask,ax                            ; here by bootos2.exe
                mov     ax,es:[2]                               ;
                mov     _VesaNoLFB,ax                           ;
                push    gs                                      ;
                xor     ax,ax                                   ;
                mov     _VesaInfo.SuppotedModes,ax              ; initing fields
                mov     _VesaInfo.ModeNum,ax                    ;
                dec     ax                                      ;
                mov     word ptr _VesaInfo.win4write,ax         ;
                mov     ax,1130h                                ; read font 8x16 address
                mov     bh,6                                    ;
                int     10h                                     ;
                xor     eax,eax                                 ;
                mov     ax,es                                   ;
                sub     ax,0A000h                               ;
                jc      near ptr @@err_novesa                   ; addr <  A0000h
                cmp     ax,6000h                                ;
                jnc     near ptr @@err_novesa                   ; addr > 100000h
                movzx   ebp,bp                                  ;
                shl     eax,4                                   ;
                add     eax,ebp                                 ; calculate physical
                mov     _VesaInfo.FontAddr,eax                  ; save offset from A0000h to make it linear later
                mov     es,_M3Seg                               ;
                xor     di,di                                   ;
                mov     word ptr es:[di],0                      ; make sure vesa 1.x call
                mov     ax,4F00h                                ;
                int     10h                                     ;
                cmp     ax,004Fh                                ;
                jnz     near ptr @@err_novesa                   ; get info failed
                                                                            
                lgs     si,es:[di+0Eh]                          ; mode list pointer
@@mode_loop:                                                    
                lods    word ptr gs:[si]                        ;
                cmp     ax,-1                                   ;
                jz      @@finished                              ;
                                                                
                mov     cx,ax                                   ;
                push    cx                                      ;
                call    GetModeInfo                             ;
                pop     cx                                      ;
                jc      @@mode_loop                             ; error getting mode info
                                                                
                mov     ax,es:[di].ModeAttributes               ;
                test    ax,VSMI_SUPPORTED                       ; skip any unsupported
                jz      @@mode_loop                             ;
                test    ax,VSMI_GRAPHMODE                       ; text
                jz      @@mode_loop                             ;
                test    ax,VSMI_NOBANK                          ; and non-banking mode
                jnz     @@mode_loop                             ;
                cmp     cx,VM_640x480x8                         ;
@@mode8bit:
                mov     dl,M640x480x8                           ;
                jnz     @@next_check                            ;
@@newmode:
                or      byte ptr _VesaInfo.SuppotedModes,dl     ; vesa mode by number
                jmp     @@mode_loop                             ;
@@next_check:
                                                                ; OEM mode and
                test    ax,VSMI_OPTINFO                         ; optional info present
                jz      @@mode_loop                             ;
                cmp     es:[di].XResolution,640                 ; check size
                jnz     @@mode_loop                             ;
                cmp     es:[di].YResolution,480                 ;
                jnz     @@mode_loop                             ;
                cmp     es:[di].NumberOfPlanes,1                ; one plane
                jnz     @@mode_loop                             ;
                mov     dh,es:[di].MemoryModel                  ; memory model
                mov     dl,es:[di].BitsPerPixel                 ; bpp
                mov     bx,8                                    ;
@@bpp_check:
                sub     bx,2                                    ; search mode in table
                cmp     dx,ModeCheckTable[bx]                   ;
                jz      @@bpp_ok                                ;
                or      bx,bx                                   ;
                jz      @@mode_loop                             ;
                jmp     @@bpp_check                             ;
@@bpp_ok:
                mov     _VesaInfo.Mode15[bx],cx                 ;
                shr     bx,1                                    ; update mode number & info bit mask
                mov     cl,bl                                   ;
                inc     cl                                      ;
                inc     bh                                      ;
                shl     bh,cl                                   ;
                or      byte ptr _VesaInfo.SuppotedModes,bh     ;
                jmp     @@mode_loop                             ;
@@mode_failed:
                mov     dx,-1                                   ; drop failed mode from list
                btc     dx,si                                   ;
                and     _VesaInfo.SuppotedModes,dx              ;
@@finished:                                                     
                mov     dx,_VesaInfo.SuppotedModes              ;
                and     dx,_VesaMask                            ; disable mode by mask from ini
                jz      near ptr @@err_novesa                   ; no any suitable 640x480 mode
                                                                
                xor     si,si                                   ;
                cmp     dx,M640x480x8                           ; 8 bit mode default number
                mov     cx,VM_640x480x8                         ;
                jz      @@save_mode                             ;
                
                bsr     si,dx                                   ; get mode number with 
                mov     bx,si                                   ; highest supported colors
                dec     bx                                      ;
                shl     bx,1                                    ;
                mov     cx,_VesaInfo.Mode15[bx]                 ;
@@save_mode:
                mov     _VesaInfo.ModeNum,cx                    ;
                call    GetModeInfo                             ;
                jc      @@mode_failed                           ;
                shl     si,1                                    ;
                movzx   ax,byte ptr ModeCheckTable8[si]         ; update ColorBits field
                mov     _VesaInfo.ColorBits,ax                  ;
                mov     ax,word ptr es:[di].WinAAttributes      ;
                test    al,1                                    ;
                jz      @@mode_failed                           ; win A is not supported ;)
                test    al,4                                    ; win A writeable?
                jz      @@next_46                               ;
                mov     _VesaInfo.win4write,0                   ;
@@next_46:
                test    al,2                                    ;
                jz      @@next_47                               ;
                mov     _VesaInfo.win4read,0                    ; win A readable
@@next_47:                                                      
                cmp     word ptr _VesaInfo.win4write,0          ; win A suitable for all
                jz      @@next_49                               ;
                test    ah,4                                    ; win B writeable?
                jz      @@next_48                               ;
                mov     _VesaInfo.win4write,1                   ;
@@next_48:                                                      
                test    ah,2                                    ;
                jz      @@next_49                               ;
                mov     _VesaInfo.win4read,1                    ; win B readable
@@next_49:                                                      
                xor     eax,eax                                 ;
                mov     ax,es:[di].Granularity                  ;
                mov     _VesaInfo.win_gran,eax                  ; save mode info vars
                mov     ax,es:[di].WinSize                      ;
                shl     eax,10                                  ;
                mov     _VesaInfo.win_size,eax                  ;
                movzx   eax,es:[di].WinASegment                 ;
                or      ax,ax                                   ;
                jz      @@next50                                ;
                sub     ax,0A000h                               ;
                shl     eax,4                                   ;
@@next50:
                mov     _VesaInfo.WinAAddr,eax                  ; win A linear offset from A0000
                movzx   eax,es:[di].WinBSegment                 ;
                or      ax,ax                                   ;
                jz      @@next51                                ;
                sub     ax,0A000h                               ;
                shl     eax,4                                   ;
@@next51:                                                       
                mov     _VesaInfo.WinBAddr,eax                  ; win B linear offset from A0000
                movzx   eax,es:[di].BytesPerScanline            ;
                mov     _VesaInfo.LineLen,eax                   ;
                                                                
                mov     ax,es:[di].ModeAttributes               ;
                test    ax,VSMI_LINEAR                          ; check for LFB
                jz      @@err_novesa                            ;
                cmp     byte ptr _VesaNoLFB, 0                  ;
                jnz     @@err_novesa                            ; LFB disabled from ini?
                mov     eax,es:[di].PhysBasePtr                 ;
                or      eax,eax                                 ; frame address = 0?
                jz      @@err_novesa                            ;
                mov     _VesaInfo.LFBAddr,eax                   ;
                or      _VesaInfo.ModeNum,4000h                 ; switch mode to LFB
@@err_novesa:
                pop     gs                                      ;
                pop     es                                      ;
                popad                                           ;
                ret                                             ;
_VesaDetect     endp

GetModeInfo     proc    near
                mov     es,_M3Seg                               ; make sure, we do not 
                mov     di,MODEBUF                              ; overwrite mode list
                mov     ax,4F01h
                int     10h
                cmp     ax,004Fh
                clc
                jz      @@err_nomode
                stc
@@err_nomode:
                ret
GetModeInfo     endp

_TEXT           ends
                end
