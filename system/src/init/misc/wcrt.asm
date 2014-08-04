;
; QSINIT
; watcom functions for 64-bit int arithmetic. 
; compiler depends on this even without runtime libs.
;
                .386p

                include inc/segdef.inc

CODE32          segment
                assume  cs:CODE32,ds:DGROUP,es:DGROUP,ss:DGROUP

                public  __I8D
__I8D           proc    near
                or      edx,edx
                js      @@i8d_2
                or      ecx,ecx
                js      @@i8d_1
                call    __U8D
                ret
@@i8d_1:
                neg     ecx
                neg     ebx
                sbb     ecx,0
                call    __U8D
                neg     edx
                neg     eax
                sbb     edx,0
                ret
@@i8d_2:
                neg     edx
                neg     eax
                sbb     edx,0
                or      ecx,ecx
                jns     @@i8d_3
                neg     ecx
                neg     ebx
                sbb     ecx,0
                call    __U8D
                neg     ecx
                neg     ebx
                sbb     ecx,0
                ret
@@i8d_3:
                call    __U8D
                neg     ecx
                neg     ebx
                sbb     ecx,0
                neg     edx
                neg     eax
                sbb     edx,0
                ret
__I8D           endp

                public  __U8D
__U8D           proc    near
                or      ecx,ecx
                jne     @@u8d_3
                dec     ebx
                je      @@u8d_2
                inc     ebx
                cmp     ebx,edx
                ja      @@u8d_1
                mov     ecx,eax
                mov     eax,edx
                sub     edx,edx
                div     ebx
                xchg    eax,ecx
@@u8d_1:
                div     ebx
                mov     ebx,edx
                mov     edx,ecx
                sub     ecx,ecx
@@u8d_2:
                ret
@@u8d_3:
                cmp     ecx,edx
                jb      @@u8d_5
                jne     @@u8d_4
                cmp     ebx,eax
                ja      @@u8d_4
                sub     eax,ebx
                mov     ebx,eax
                sub     ecx,ecx
                sub     edx,edx
                mov     eax,1
                ret
@@u8d_4:
                sub     ecx,ecx
                sub     ebx,ebx
                xchg    eax,ebx
                xchg    edx,ecx
                ret
@@u8d_5:
                push    ebp
                push    esi
                push    edi
                sub     esi,esi
                mov     edi,esi
                mov     ebp,esi
@@u8d_6:
                add     ebx,ebx
                adc     ecx,ecx
                jb      @@u8d_9
                inc     ebp
                cmp     ecx,edx
                jb      @@u8d_6
                ja      @@u8d_7
                cmp     ebx,eax
                jbe     @@u8d_6
@@u8d_7:
                clc
@@u8d_8:
                adc     esi,esi
                adc     edi,edi
                dec     ebp
                js      @@u8d_12
@@u8d_9:
                rcr     ecx,1
                rcr     ebx,1
                sub     eax,ebx
                sbb     edx,ecx
                cmc
                jb      @@u8d_8
@@u8d_10:
                add     esi,esi
                adc     edi,edi
                dec     ebp
                js      @@u8d_11
                shr     ecx,1
                rcr     ebx,1
                add     eax,ebx
                adc     edx,ecx
                jae     @@u8d_10
                jmp     @@u8d_8
@@u8d_11:
                add     eax,ebx
                adc     edx,ecx
@@u8d_12:
                mov     ebx,eax
                mov     ecx,edx
                mov     eax,esi
                mov     edx,edi
                pop     edi
                pop     esi
                pop     ebp
                ret
__U8D           endp

                public  __U8RS
__U8RS          proc    near
                mov     ecx,ebx
                and     cl,3Fh
                test    cl,20h
                jne     @@u8rs_1
                shrd    eax,edx,cl
                shr     edx,cl
                ret
@@u8rs_1:
                mov     eax,edx
                sub     ecx,20h
                xor     edx,edx
                shr     eax,cl
                ret
__U8RS          endp

                public  __I8RS
__I8RS          proc    near
                mov     ecx,ebx
                and     cl,3Fh
                test    cl,20h
                jne     @@i8rs_1
                shrd    eax,edx,cl
                sar     edx,cl
                ret
@@i8rs_1:
                mov     eax,edx
                sub     cl,20h
                sar     edx,1Fh
                sar     eax,cl
                ret
__I8RS          endp

                public  __U8LS
                public  __I8LS
__U8LS          proc    near
__I8LS          label   near
                mov     ecx,ebx
                and     cl,3Fh
                test    cl,20h
                jne     @@u8ls_1
                shld    edx,eax,cl
                shl     eax,cl
                ret
@@u8ls_1:
                mov     edx,eax
                sub     cl,20h
                xor     eax,eax
                shl     edx,cl
                ret
__U8LS          endp

                public  __U8M
                public  __I8M
__U8M           proc    near
__I8M           label   near
                test    edx,edx
                jne     @@u8m_1
                test    ecx,ecx
                jne     @@u8m_1
                mul     ebx
                ret
@@u8m_1:
                push    eax
                push    edx
                mul     ecx
                mov     ecx,eax
                pop     eax
                mul     ebx
                add     ecx,eax
                pop     eax
                mul     ebx
                add     edx,ecx
                ret
__U8M           endp

CODE32          ends
                end
