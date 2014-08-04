;
; QSINIT
; page unpack code
;
                include inc/basemac.inc 
                include inc/qstypes.inc

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

_DATA           segment dword public USE32 'DATA'
                public M3BufferSize
M3BufferSize    dd  Buffer + 1000h + 4
CopyMin         dd  0,16,80,320,1024,4096,16384,0
CopyBits        dd  4,6,8,10,12,14,14,0

_DATA           ends

CODE32          segment para public USE32 'CODE'
                assume cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT

                TWICEMAX EQU  1409
                MAXCHAR  EQU   704
                SUCCMAX  EQU   705
                MAXFREQ  EQU  2000

_Update_Freq    proc    near                                    ;
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
_Update_Freq    endp                                            ;

                public _DecompressM3
_DecompressM3   proc    near                                    ;
@@Length        =  8                                            ;
@@Source        = 12                                            ;
@@Destin        = 16                                            ;
@@Buffer        = 20                                            ;
                                                                ;
                push    ebp                                     ;
                mov     ebp,esp                                 ;
                SaveReg <ebx,edi,esi>                           ;
                                                                ;
                mov     esi, [ebp+@@Buffer]                     ; copy all parameters into
                mov     eax, [ebp+@@Length]                     ; our buffer & use ebp as
                mov     [esi+InLength], eax                     ; usual storage
                mov     eax, [ebp+@@Source]                     ;
                mov     [esi+SourcePtr], eax                    ;
                mov     eax, [ebp+@@Destin]                     ;
                mov     [esi+DestinPtr], eax                    ;
                                                                ;
                xor     ebp,ebp                                 ;
                                                                ;
                mov     [esi+Input_Bit_Count],ebp               ;
                mov     [esi+Input_Bit_Buffer],ebp              ;
                mov     eax,[esi+DestinPtr]                     ;
                mov     [esi+CurBuf],eax                        ;
                                                                ;
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
                jmp     near ptr l__25                          ;
ll3:
                cmp     ebx,100h                                ;
                je      near ptr l__34                          ;
                jnc     l__6                                    ;
                mov     edi,[esi+CurBuf]                        ;
                mov     [edi],bl                                ;
                inc     dword ptr [esi+CurBuf]                  ;
                mov     byte ptr ds:[esi+ebp+Buffer],bl         ;
                inc     ebp                                     ;
                cmp     ebp,8000h                               ;
                jnz     near ptr l__25                          ;
                xor     ebp,ebp                                 ;
                jmp     near ptr l__25                          ;
l__6:           
                sub     ebx,101h                                ;
                mov     ecx,ebx                                 ;
                and     ecx,3Fh                                 ;
                add     ecx,3                                   ;
                mov     [esi+Len],ecx                           ;
                                                                ;
                shr     ebx,6                                   ;
                add     ecx,CopyMin[ebx*4]                      ;
                                                                ;
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
                je      near ptr l_l16                          ;
                push    ebx                                     ;
                mov     eax,edi                                 ;
                mov     ecx,LeftC[esi+edx*4]                    ;
                cmp     ecx,edi                                 ;
                jne     l_l3                                    ;
                mov     ecx,RightC[esi+edx*4]                   ;
l_l3:           
                call    _Update_Freq                            ;
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
                call    _Update_Freq                            ;
l_l15:          
                mov     edi,Parent[esi+edi*4]                   ;
                mov     edx,Parent[esi+edi*4]                   ;
                cmp     edx,1                                   ;
                jnz     near ptr l_l4                           ;
                pop     ebx                                     ;
l_l16:          
                jmp     near ptr ll3                            ;
l__34:          
                mov     eax,[esi+CurBuf]                        ;
                sub     eax,[esi+DestinPtr]                     ;
                                                                ;
                RestReg <esi,edi,ebx>                           ;
                pop     ebp                                     ;
                ret     16                                      ;
_DecompressM3   endp

; ITERDATA2 decompression
;----------------------------------------------------------------
                public  _DecompressM2                           ;
_DecompressM2   proc    near                                    ;
@@DataLen       =  8                                            ; data length
@@Src           = 12                                            ; source data
@@Dst           = 16                                            ; dest. page
@@DstEnd        = -4                                            ; dst data end ptr

                push    ebp                                     ;
                mov     ebp,esp                                 ;
                sub     esp,4                                   ;
                SaveReg <ebx,edi,esi>                           ;
                mov     edi,[ebp+@@Dst]                         ;
                mov     esi,[ebp+@@Src]                         ;
                mov     ecx,[ebp+@@DataLen]                     ;
                lea     edx,[edi+PAGESIZE]                      ;
                mov     [ebp+@@DstEnd],edx                      ;
                xor     ecx,ecx                                 ;
                align   16
@@d2_loop:
                cmp     [ebp+@@DstEnd],edi                      ; main loop
                jc      @@d2_errexit                            ;
                xor     eax,eax                                 ;
                xor     ecx,ecx                                 ;
                lodsb                                           ;
                mov     bl,al                                   ;
                and     ebx,3                                   ;
                jmp     d2_case[ebx*4]                          ;

d2_case         label   near                                    ;
                dd      offset @@d2_seq0                        ;
                dd      offset @@d2_seq1                        ;
                dd      offset @@d2_seq2                        ;
                dd      offset @@d2_seq3                        ;
;----------------------------------------------------------------
                align   16
@@d2_seq3:
                mov     bl,al                                   ; case 3
                lodsw                                           ;
                mov     cl,bl                                   ;
                and     cl,3Ch                                  ;
                jz      @@d2_seq3_3                             ; skip memcpy
                shr     ecx,3                                   ; 
                jnc     @@d2_seq3_1                             ; or do it ;)
                movsb                                           ;
@@d2_seq3_1:
                shr     ecx,1                                   ;
                jnc     @@d2_seq3_2                             ;
                movsw                                           ;
@@d2_seq3_2:
            rep movsd                                           ;
@@d2_seq3_3:
                mov     bh,al                                   ;
                shl     bx,2                                    ;
                and     bh,3Fh                                  ;
                shr     eax,4                                   ;
                mov     cl,bh                                   ;
                mov     edx,esi                                 ; copy from previously
                mov     esi,edi                                 ; unpacked data
                sub     esi,eax                                 ;
                jc      @@d2_errexit                            ;
                cmp     eax,4                                   ;
                jc      @@d2_seq3_4                             ;
                mov     al,cl                                   ;
                and     al,3                                    ;
                shr     ecx,2                                   ;
            rep movsd                                           ;
                mov     cl,al                                   ;
@@d2_seq3_4:
                shr     cl,1                                    ;
                jnc     @@d2_seq3_5                             ;
                movsb                                           ;
@@d2_seq3_5:
            rep movsw                                           ;
                mov     esi,edx                                 ;
                jmp     @@d2_loop                               ;
;----------------------------------------------------------------
                align   16
@@d2_seq2:
                mov     ah,[esi]                                ; case 2
                inc     esi                                     ;
                mov     bl,3                                    ;
                shr     eax,2                                   ;
                mov     cl,al                                   ;
                and     cl,bl                                   ;
                add     cl,bl                                   ;
                mov     edx,esi                                 ;
                mov     esi,edi                                 ;
                shr     eax,2                                   ;
                sub     esi,eax                                 ;
                jc      @@d2_errexit                            ;
                shr     ecx,1                                   ;
                jnc     @@d2_seq2_1                             ;
                movsb                                           ;
@@d2_seq2_1:
            rep movsw                                           ;
                mov     esi,edx                                 ;
                jmp     @@d2_loop                               ;

                align   16
@@d2_seq1:
                mov     cl,al                                   ; case 1
                lodsb                                           ;
                mov     ah,ch                                   ;
                shl     cl,1                                    ;
                rcl     ax,1                                    ;
                shr     ecx,3                                   ;
                mov     dx,cx                                   ;
                mov     bl,3                                    ;
                and     cl,bl                                   ;
            rep movsb                                           ;
                shr     dl,2                                    ;
                add     dl,bl                                   ;
                mov     cl,dl                                   ;
                mov     edx,esi                                 ;
                mov     esi,edi                                 ;
                sub     esi,eax                                 ;
                jc      @@d2_errexit                            ;
                cmp     al,1                                    ;
                jbe     @@d2_seq1_1                             ;
                shr     cx,1                                    ;
            rep movsw                                           ;
                adc     cl,ch                                   ;
@@d2_seq1_1:
            rep movsb                                           ;
                mov     esi,edx                                 ;
                jmp     @@d2_loop                               ;
;----------------------------------------------------------------
                align   16
@@d2_seq0:
                mov     cl,al                                   ; case 0
                jcxz    @@d2_seq0_3                             ;
                shr     ecx,3                                   ;
                jnc     @@d2_seq0_1                             ;
                movsb                                           ;
@@d2_seq0_1:
                shr     ecx,1                                   ;
                jnc     @@d2_seq0_2                             ;
                movsw                                           ;
@@d2_seq0_2:
            rep movsd                                           ;
                jmp     @@d2_loop                               ;
@@d2_seq0_3:
                lodsw                                           ;
                mov     cl,al                                   ;
                jcxz    @@d2_zeroexit                           ;
                mov     al,ah                                   ; fill dst cl times
                push    ax                                      ;
                rol     eax,10h                                 ;
                pop     ax                                      ;
                shr     ecx,1                                   ;
                jnc     @@d2_seq0_4                             ;
                stosb                                           ;
@@d2_seq0_4:
                shr     ecx,1                                   ;
                jnc     @@d2_seq0_5                             ;
                stosw                                           ;
@@d2_seq0_5:
            rep stosd                                           ;
                jmp     @@d2_loop                               ;
@@d2_errexit:
                xor     eax,eax                                 ;
                jmp     @@d2_exit                               ;
@@d2_zeroexit:
                mov     eax,edi                                 ; bytes written
                sub     eax,[ebp+@@Dst]                         ;
@@d2_exit:
                RestReg <esi,edi,ebx>                           ;
                mov     esp,ebp                                 ;
                pop     ebp                                     ;
                ret     12                                      ;
_DecompressM2   endp

CODE32          ends
                end
