;
; QSINIT
; 32-bit part unpack & fixup code
; ---------------------------------------------------------------
; note: CS/DS/ES in this code is FLAT
;       SS - 16-bit stack, so all DATA16/BSS16 variable
;            addressing here going with SS prefix!
;
                include inc/qstypes.inc                         ;
                include inc/segdef.inc                          ;
                include inc/qsbinfmt.inc                        ;
                include inc/dpmi.inc                            ;

                .486

                Parent    EQU  0
                Freq      EQU  Parent + 5640
                LeftC     EQU  Freq   + 5640     ; 11280
                RightC    EQU  LeftC  + 2832     ; 14112
                ii        EQU  RightC + 2832
                Len       EQU  ii     + 4
      Input_Bit_Count     EQU  Len    + 4
                CurBuf    EQU  Input_Bit_Count + 4
      Input_Bit_Buffer    EQU  CurBuf + 4
                InLength  EQU  Input_Bit_Buffer + 4
                SourcePtr EQU  InLength + 4
                DestinPtr EQU  SourcePtr + 4
                Buffer    EQU  DestinPtr + 4


_BSS16          segment                                         ;
                extrn   _rm16code:word                          ;
                extrn   _highbase:dword                         ;
                extrn   _highstack:dword                        ;
                extrn   _bin_header:mkbin_info                  ;
                extrn   bssend:word                             ;
                public  _stack16_pos                            ;
_stack16_pos    dw      ?                                       ;
                public  _rm_regs                                ;
_rm_regs        rmcallregs_s <?>                                ; real mode call regs
_BSS16          ends                                            ;

TEXT16          segment                                         ;
                extrn   panic_initpm:near                       ;
TEXT16          ends                                            ;

DATA16          segment
                public  _unpbuffer
_unpbuffer      dd      Buffer + 8001h + 4
CopyMin         dd      0,16,80,320,1024,4096,16384,0
CopyBits        dd      4,6,8,10,12,14,14,0
                extrn   bin32_seg:word
DATA16          ends

LOADER          segment
                assume  cs:nothing, ds:nothing, es:nothing, ss:G16

                TWICEMAX EQU  1409
                MAXCHAR  EQU   704
                SUCCMAX  EQU   705
                MAXFREQ  EQU  2000

updateFreq      proc    near                                    ;
@@ll0:
                mov     ecx,dword ptr Freq[esi+ecx*4]           ;
                add     ecx,dword ptr Freq[esi+eax*4]           ;
                mov     eax,dword ptr Parent[esi+eax*4]         ;
                mov     dword ptr Freq[esi+eax*4],ecx           ;
                cmp     eax,1                                   ;
                je      @@ll4                                   ;
                mov     ecx,dword ptr Parent[esi+eax*4]         ;
                cmp     eax,dword ptr LeftC[esi+ecx*4]          ;
                jne     @@ll3                                   ;
                mov     ecx,dword ptr RightC[esi+ecx*4]         ;
                jmp     @@ll4                                   ;
@@ll3:
                mov     ecx,dword ptr LeftC[esi+ecx*4]          ;
@@ll4:          
                cmp     eax,1                                   ;
                jnz     @@ll0                                   ;
                cmp     dword ptr [esi+Freq+4],MAXFREQ          ;
                jnz     @@ll8                                   ;
@@ll7:          
                shr     dword ptr Freq[esi+eax*4],1             ;
                inc     eax                                     ;
                cmp     eax,TWICEMAX                            ;
                jle     @@ll7                                   ;
@@ll8:          
                ret                                             ;
updateFreq      endp                                            ;

;----------------------------------------------------------------
; u32t _std decompress(u32t DataLen, u8t* Src, u8t *Dst, u8t *Buffer);
                public  _decompress
_decompress     proc    near                                    ;
@@Length        =  8                                            ;
@@Source        = 12                                            ;
@@Destin        = 16                                            ;
@@Buffer        = 20                                            ;
                                                                ;
                push    ebp                                     ;
                mov     ebp,esp                                 ;
                                                                ;
                mov     esi, _unpbuffer                         ; copy all parameters into
                mov     eax, [ebp+@@Length]                     ; our buffer & use ebp as
                mov     [esi+InLength], eax                     ; usual storage
                mov     eax, [ebp+@@Source]                     ;
                mov     [esi+SourcePtr], eax                    ;
                mov     eax, [ebp+@@Destin]                     ;
                mov     [esi+DestinPtr], eax                    ;

                xor     ebp,ebp                                 ;
                                                                ;
                mov     [esi+Input_Bit_Count],ebp               ;
                mov     [esi+Input_Bit_Buffer],ebp              ;
                mov     eax,[esi+DestinPtr]                     ;
                mov     [esi+CurBuf],eax                        ;

                mov     ecx,2                                   ;
ll1:
                mov     eax,ecx                                 ;
                mov     dword ptr Freq[esi+ecx*4],1             ;
                shr     eax,1                                   ;
                mov     Parent[esi+ecx*4],eax                   ;
                inc     ecx                                     ;
                cmp     ecx,TWICEMAX                            ;
                jle     ll1                                     ;
                mov     ecx,1                                   ;
ll2:
                mov     eax,ecx                                 ;
                shl     eax,1                                   ;
                mov     LeftC[esi+ecx*4],eax                    ;
                inc     eax                                     ;
                mov     RightC[esi+ecx*4],eax                   ;
                inc     ecx                                     ;
                cmp     ecx,MAXCHAR                             ;
                jle     ll2                                     ;
                                                                ;
                jmp     l__25                                   ;
ll3:
                cmp     ebx,100h                                ;
                je      l__34                                   ;
                jnc     l__6                                    ;
                mov     edi,[esi+CurBuf]                        ;
                mov     [edi],bl                                ;
                inc     dword ptr [esi+CurBuf]                  ;
                mov     ds:[ebp+esi+Buffer],bl                  ;
                inc     ebp                                     ;
                cmp     ebp,8000h                               ;
                jnz     l__25                                   ;
                xor     ebp,ebp                                 ;
                jmp     l__25                                   ;
l__6:           
                sub     ebx,101h                                ;
                mov     ecx,ebx                                 ;
                and     ecx,3Fh                                 ;
                add     ecx,3                                   ;
                mov     [esi+Len],ecx                           ;
                                                                ;
                shr     ebx,6                                   ;
                add     ecx,CopyMin[ebx*4]                      ;
                mov     edx,CopyBits[ebx*4]                     ;
                dec     edx                                     ;
                xor     ebx,ebx                                 ;
                push    ecx                                     ;
                xor     ecx,ecx                                 ;
                mov     edi,[esi+Input_Bit_Buffer]              ;
                jmp     l__13                                   ;
l__7:           
                cmp     dword ptr [esi+Input_Bit_Count],0       ;
                jnz     l__9                                    ;
                mov     edi,[esi+SourcePtr]                     ;
                mov     edi,[edi]                               ;
                add     dword ptr [esi+SourcePtr],4             ;
                mov     dword ptr [esi+Input_Bit_Count],20h     ;
l__9:           
                dec     dword ptr [esi+Input_Bit_Count]         ;
                xor     eax,eax                                 ;
                shl     edi,1                                   ;
                adc     eax,eax                                 ;
                shl     eax,cl                                  ;
                or      ebx,eax                                 ;
                inc     ecx                                     ;
l__13:          
                cmp     edx,ecx                                 ;
                jge     l__7                                    ;
                mov     [esi+Input_Bit_Buffer],edi              ;
                pop     ecx                                     ;
                add     ecx,ebx                                 ;
                mov     eax,ebp                                 ;
                mov     ebx,ebp                                 ;
                sub     ebx,ecx                                 ;
                test    ebx,ebx                                 ;
                jge     l__16                                   ;
                add     ebx,8000h                               ;
l__16:          
                mov     ecx,[esi+Len]                           ;
                dec     ecx                                     ;
                mov     dword ptr [esi+ii],0                    ;
                mov     edi,[esi+CurBuf]                        ;
                jmp     l__22                                   ;
l__17:          
                mov     dl,[esi+ebx+offset Buffer]              ;
                mov     [edi],dl                                ;
                inc     edi                                     ;
                mov     [esi+eax+offset Buffer],dl              ;
                inc     eax                                     ;
                cmp     eax,8000h                               ;
                jnz     l__19                                   ;
                xor     eax,eax                                 ;
l__19:          
                inc     ebx                                     ;
                cmp     ebx,8000h                               ;
                jne     l__21                                   ;
                xor     ebx,ebx                                 ;
l__21:          
                inc     dword ptr [esi+ii]                      ;
l__22:          
                cmp     ecx,[esi+ii]                            ;
                jge     l__17                                   ;
                mov     [esi+CurBuf],edi                        ;
                lea     ebp,[ebp+ecx+1]                         ;
                cmp     ebp,8000h                               ;
                jl      l__25                                   ;
                sub     ebp,8000h                               ;
l__25:          
                mov     ebx,1                                   ;
                mov     edi,[esi+Input_Bit_Buffer]              ;
l__26:          
                cmp     dword ptr [esi+Input_Bit_Count],0       ;
                jnz     l__28                                   ;
                mov     edi,[esi+SourcePtr]                     ;
                mov     edi,[edi]                               ;
                add     dword ptr [esi+SourcePtr],4             ;
                mov     dword ptr [esi+Input_Bit_Count],20h     ;
l__28:
                dec     dword ptr [esi+Input_Bit_Count]         ;
                shl     edi,1                                   ;
                jnc     l__31                                   ;
                mov     ebx,RightC[esi+ebx*4]                   ;
                jmp     l__32                                   ;
l__31:          
                mov     ebx,LeftC[esi+ebx*4]                    ;
l__32:          
                cmp     ebx,MAXCHAR                             ;
                jle     l__26                                   ;
                sub     ebx,SUCCMAX                             ;
                mov     [esi+Input_Bit_Buffer],edi              ;
                lea     edi,[ebx+SUCCMAX]                       ;
                inc     dword ptr Freq[esi+edi*4]               ;
                mov     edx,Parent[esi+edi*4]                   ;
                cmp     edx,1                                   ;
                je      l_l16                                   ;
                push    ebx                                     ;
                mov     eax,edi                                 ;
                mov     ecx,LeftC[esi+edx*4]                    ;
                cmp     ecx,edi                                 ;
                jne     l_l3                                    ;
                mov     ecx,RightC[esi+edx*4]                   ;
l_l3:           
                call    updateFreq                              ;
l_l4:           
                mov     ecx,Parent[esi+edx*4]                   ;
                mov     ebx,LeftC[esi+ecx*4]                    ;
                cmp     ebx,edx                                 ;
                jne     l_l6                                    ;
                mov     ebx,RightC[esi+ecx*4]                   ;
l_l6:           
                mov     eax,Freq[esi+edi*4]                     ;
                cmp     eax,Freq[esi+ebx*4]                     ;
                jle     l_l15                                   ;
                mov     Parent[esi+ebx*4],edx                   ;
                mov     Parent[esi+edi*4],ecx                   ;
                cmp     edx,LeftC[esi+ecx*4]                    ;
                jne     l_l10                                   ;
                mov     RightC[esi+ecx*4],edi                   ;
                jmp     l_l11                                   ;
l_l10:          
                mov     LeftC[esi+ecx*4],edi                    ;
l_l11:          
                cmp     edi,LeftC[esi+edx*4]                    ;
                jne     l_l13                                   ;
                mov     LeftC[esi+edx*4],ebx                    ;
                mov     ecx,RightC[esi+edx*4]                   ;
                jmp     l_l14                                   ;
l_l13:          
                mov     RightC[esi+edx*4],ebx                   ;
                mov     ecx,LeftC[esi+edx*4]                    ;
l_l14:          
                mov     edi,ebx                                 ;
                mov     eax,ebx                                 ;
                call    updateFreq                              ;
l_l15:          
                mov     edi,Parent[esi+edi*4]                   ;
                mov     edx,Parent[esi+edi*4]                   ;
                cmp     edx,1                                   ;
                jnz     l_l4                                    ;
                pop     ebx                                     ;
l_l16:          
                jmp     ll3                                     ;
l__34:          
                mov     eax,[esi+CurBuf]                        ;
                sub     eax,[esi+DestinPtr]                     ;

                pop     ebp                                     ;
                ret     12                                      ;
_decompress     endp

;----------------------------------------------------------------
; panic from 32-bit part of 16-bit object
; eax = error code for RM message

                public  rmstop_preinit                          ;
rmstop_preinit  proc    near                                    ; exit to rm
                xor     ebp,ebp                                 ;
                mov     bp,offset _rm_regs                      ; offset in DATA16

                mov     [ebp+rmcallregs_s.r_eax],eax            ; error code

                mov     ax,_rm16code                            ;
                mov     [ebp+rmcallregs_s.r_ds],ax              ; set ds to 9100h
                mov     [ebp+rmcallregs_s.r_ss],ax              ; set ss:sp to own stack,
                shl     eax,16                                  ;
                mov     ax,offset panic_initpm                  ;
                mov     dword ptr [ebp+rmcallregs_s.r_ip],eax   ; cs:ip

                mov     ax,sp                                   ; share own 16-bit stack
                sub     ax,100h                                 ;
                mov     [ebp+rmcallregs_s.r_sp],ax              ;
                pushf                                           ;
                pop     _rm_regs.r_flags                        ; save flags

                mov     edi,ebp                                 ;
                mov     ax,ss                                   ; es:[edi] - registers
                mov     es,ax                                   ;
                mov     bh,FN30X_PICRESET or FN30X_TIMEROFF     ; dpmi call
                mov     ax,301h                                 ;
                int     31h                                     ;
                ret                                             ; return to caller
rmstop_preinit  endp

;----------------------------------------------------------------
; unpack and launch 32-bit part of code
;----------------------------------------------------------------
; DS/ES -> FLAT
; CS    -> FLAT
; SS    -> 16-bit data
;
                public  _prepare32
_prepare32      proc    near                                    ;
                movzx   esi,bin32_seg                           ;
                shl     esi,PARASHIFT                           ;
                mov     edi,_highbase                           ;
                mov     ecx,_bin_header.pmPacked                ;
                cmp     ecx,_bin_header.pmTotal                 ;
                jz      @@p32_copy                              ;

                push    edi                                     ;
                push    esi                                     ;
                push    ecx                                     ;

                call    _decompress                             ; unpack
                cmp     eax,_bin_header.pmTotal                 ;
                jz      @@p32_fxapply                           ;
                mov     eax,7                                   ; error exit to RM
                jmp     rmstop_preinit                          ;
@@p32_copy:
                cld                                             ; just copy
            rep movsb                                           ;
@@p32_fxapply:
                mov     ebx,_highbase                           ;
                mov     ecx,_bin_header.pmTotal                 ;
                lea     esi,[ecx+ebx-1]                         ;
                sub     ecx,_bin_header.pmStored                ; ecx = fixups size
                jecxz   @@p32_nofxmove                          ;
                mov     edi,_bin_header.pmSize                  ;
                lea     edi,[edi+ebx-1]                         ; move fixups above
                add     edi,ecx                                 ; BSS data
                std                                             ;
            rep movsb                                           ;
                cld                                             ;
@@p32_nofxmove:
                mov     edi,_bin_header.pmStored                ; zero BSS
                mov     ecx,_bin_header.pmSize                  ; (if exist one)
                sub     ecx,edi                                 ;
                add     edi,ebx                                 ;
                jecxz   @@p32_nobss                             ;
                xor     al,al                                   ;
            rep stosb                                           ;
@@p32_nobss:
                mov     esi,_bin_header.pmSize                  ;
                add     esi,ebx                                 ;
                mov     ecx,_bin_header.fxSelCnt                ;
                jecxz   @@p32_nofx_sel                          ;
@@p32_loopfx_sel:
                lodsd                                           ; apply
                lea     edx,[eax+ebx]                           ; selector fixups
                lodsb                                           ;
                dec     al                                      ;
                shl     al,3                                    ;
                cbw                                             ;
                add     ax,SEL32CODE                            ;
                mov     [edx],ax                                ;
                loop    @@p32_loopfx_sel                        ;
@@p32_nofx_sel:
                mov     ecx,_bin_header.fxOfsCnt                ;
                jecxz   @@p32_nofx_ofs                          ;
                movzx   ebp,_rm16code                           ; apply
                shl     ebp,PARASHIFT                           ; offset fixups
@@p32_loopfx_ofs:
                lodsd                                           ; tgt
                mov     edx,eax                                 ;
                lodsd                                           ; src
                test    byte ptr [esi],FXRELO_S16               ;
                jz      @@p32_loopfx_ofs_s32                    ; fixup location is
                add     edx,ebp                                 ; 16-bit code?
                jmp     @@p32_loopfx_ofs_target                 ;
@@p32_loopfx_ofs_s32:
                add     edx,ebx                                 ;
@@p32_loopfx_ofs_target:
                test    byte ptr [esi],FXRELO_T16               ;
                jnz     @@p32_loopfx_ofs16                      ;
                add     eax,ebx                                 ;
                jmp     @@p32_loopfx_apply                      ;
@@p32_loopfx_ofs16:
                add     eax,ebp                                 ;
@@p32_loopfx_apply:
                inc     esi                                     ;
                mov     [edx],eax                               ;
                loop    @@p32_loopfx_ofs                        ;
@@p32_nofx_ofs:
                mov     ecx,_bin_header.fxRelCnt                ; apply
                jecxz   @@p32_nofx_rel                          ; relative fixups
@@p32_loopfx_rel:
                lodsd                                           ;
                lea     edx,[eax+ebx]                           ;
                lodsd                                           ;
                add     eax,ebp                                 ;
                sub     eax,edx                                 ;
                sub     eax,4                                   ;
                mov     [edx],eax                               ;
                loop    @@p32_loopfx_rel
@@p32_nofx_rel:
                add     ebx,_bin_header.pmEntry                 ; this must be init32call label

                mov     _stack16_pos,sp                         ;
                mov     eax,_highstack                          ; save it because ss:_highstack
                mov     cx,SELZERO                              ; actually here
                mov     ss,cx                                   ;
                xchg    esp,eax                                 ; replace stack to QSM_STACK32

                sub     ax, offset bssend                       ; high part of prev esp
                push    eax                                     ; MUST be clean, if not - this
                                                                ; will seen in init32 print()
                jmp     ebx                                     ;
                ret
_prepare32      endp

LOADER          ends
                end
