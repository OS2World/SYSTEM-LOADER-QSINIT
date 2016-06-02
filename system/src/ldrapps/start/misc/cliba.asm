;
; QSINIT "start" module
; subset of C library functions
;
                .586p


_DATA           segment dword public USE32 'DATA'
                public aboutstr_local
aboutstr_local  label near
                include vercheck.inc
_DATA           ends

CODE32          segment dword public USE32 'CODE'
                assume cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT

;----------------------------------------------------------------
;int  __stdcall memcmp(const void *s1, const void *s2, u32t length);
                public  _memcmp
_memcmp         proc    near
@@s1            =  4                                            ;
@@s2            =  8                                            ;
@@length        = 12                                            ;
                push    edi                                     ;
                mov     edi,[esp+4+@@s1]                        ;
                push    esi                                     ;
                mov     esi,[esp+8+@@s2]                        ;
                mov     ecx,[esp+8+@@length]                    ;
                xor     eax,eax                                 ;
           repe cmpsb                                           ;
                jz      @@memcmp_ok                             ;
                sbb     eax, eax                                ;
                sbb     eax, 0FFFFFFFFh                         ;
@@memcmp_ok:
                pop     esi                                     ;
                pop     edi                                     ;
                ret     12                                      ;
_memcmp         endp                                            ;

;----------------------------------------------------------------
;u32t __stdcall bcmp(const void *s1, const void *s2, u32t length);
;
; set result in CF flag as bonus
;
                public  _bcmp
_bcmp           proc    near
@@s1            =  4                                            ;
@@s2            =  8                                            ;
@@length        = 12                                            ;
                push    edi                                     ;
                mov     edi,[esp+4+@@s1]                        ;
                push    esi                                     ;
                mov     esi,[esp+8+@@s2]                        ;
                mov     eax,esi                                 ;
                mov     ecx,[esp+8+@@length]                    ;
           repe cmpsb                                           ;
                jnz     @@bcmp_diff                             ;
                mov     esi,eax                                 ;
@@bcmp_diff:
                sub     eax,esi                                 ;
                pop     esi                                     ;
                neg     eax                                     ;
                pop     edi                                     ;
                ret     12                                      ;
_bcmp           endp                                            ;

;----------------------------------------------------------------
;char* __stdcall strrchr(char *src, int c);
                public  _strrchr
_strrchr        proc    near                                    ;
@@src           =  4                                            ;
@@cc            =  8                                            ;
                xor     eax,eax                                 ;
                push    esi                                     ;
                xor     edx,edx                                 ;
                mov     ecx, [esp+4+@@cc]                       ;
                mov     esi, [esp+4+@@src]                      ;
@@strrchr_loop:
                lodsb                                           ;
                test    al,al                                   ;
                jz      @@strrchr_end                           ;
                cmp     al,cl                                   ;
                jnz     @@strrchr_loop                          ;
                lea     edx,[esi-1]                             ;
                jmp     @@strrchr_loop                          ;
@@strrchr_end:
                mov     eax,edx                                 ; result
                pop     esi                                     ;
                ret     8                                       ;
_strrchr        endp                                            ;

;----------------------------------------------------------------
;char*  __stdcall strstr (const char *str, const char *substr);
                public  _strstr
_strstr         proc    near                                    ;
@@str           =  4                                            ;
@@substr        =  8                                            ;
                push    ebx                                     ;
                push    esi                                     ;
                push    edi                                     ;
                xor     al,al                                   ;
                mov     edi,[esp+12+@@substr]                   ;
                or      ecx,-1                                  ;
          repne scasb                                           ;
                not     ecx                                     ;
                dec     ecx                                     ;
                jz      @@strstr_notfound                       ;
                mov     edx,ecx                                 ;
                mov     edi,[esp+12+@@str]                      ;
                mov     ebx,edi                                 ;
                or      ecx,-1                                  ;
          repne scasb                                           ;
                not     ecx                                     ;
                sub     ecx,edx                                 ;
                jbe     @@strstr_notfound                       ;
                mov     edi,ebx                                 ;
@@strstr_nextcomp:                                              ;
                mov     esi,[esp+12+@@substr]                   ;
                lodsb                                           ;
          repne scasb                                           ;
                jnz     @@strstr_notfound                       ;
                mov     eax,ecx                                 ;
                mov     ebx,edi                                 ;
                mov     ecx,edx                                 ;
                dec     ecx                                     ;
           repe cmpsb                                           ;
                mov     ecx,eax                                 ;
                mov     edi,ebx                                 ;
                jnz     @@strstr_nextcomp                       ;
                lea     eax,[edi-1]                             ;
                jmp     @@strstr_end                            ;
@@strstr_notfound:
                xor     eax,eax                                 ;
@@strstr_end:
                pop     edi                                     ;
                pop     esi                                     ;
                pop     ebx                                     ;
                ret     8                                       ;
_strstr         endp                                            ;

; search/patch binary data
;----------------------------------------------------------------
;int __stdcall patch_binary(u8t *addr, u32t len, u32t times, void *str,
;              u32t slen, u32t poffs, void*pstr, u32t plen, u32t *stoplen);
                public  _patch_binary
_patch_binary   proc    near

@@psb_addr      = dword ptr [ebp+8 ]                            ;
@@psb_len       = dword ptr [ebp+12]                            ;
@@psb_times     = dword ptr [ebp+16]                            ;
@@psb_str       = dword ptr [ebp+20]                            ;
@@psb_slen      = dword ptr [ebp+24]                            ;
@@psb_poffs     = dword ptr [ebp+28]                            ;
@@psb_pstr      = dword ptr [ebp+32]                            ;
@@psb_plen      = dword ptr [ebp+36]                            ;
@@psb_reslen    = dword ptr [ebp+40]                            ;

                push    ebp                                     ;
                mov     ebp,esp                                 ;
                push    edi                                     ;
                push    esi                                     ;
                cld                                             ;
                mov     esi,@@psb_str                           ; data to search
                or      esi,esi                                 ;
                jz      @@psl_end                               ;
                mov     edi,@@psb_addr                          ; where to search
                mov     ecx,@@psb_len                           ; and how long
@@psl_cycle:
                mov     esi,@@psb_str                           ; data to search
                mov     al,[esi]                                ;
          repnz scasb                                           ; searching...
                jecxz   @@psl_end                               ;
                push    ecx                                     ;
                push    edi                                     ;
                dec     edi                                     ;
                mov     edx,@@psb_slen                          ;
@@psl_comp:
                lodsb                                           ; comparing data
                or      edx,edx                                 ;
                jnz     @@psl_binstr                            ;
                or      al,al                                   ;
                jz      @@psl_replace                           ;
@@psl_binstr:
                cmp     [edi],al                                ;
                jnz     @@psl_replace                           ;
                inc     edi                                     ;
                dec     ecx                                     ;
                or      edx,edx                                 ;
                jz      @@psl_comp                              ;
                dec     edx                                     ;
                jnz     @@psl_comp                              ;
@@psl_replace:
                pop     edi                                     ; zf - success here
                pop     ecx                                     ;
                jnz     @@psl_cycle                             ; try again
                push    edi                                     ;
                dec     edi                                     ; patching
                add     edi,@@psb_poffs                         ;
                push    ecx                                     ;
                mov     ecx,@@psb_plen                          ;
                jecxz   @@psl_noreplace                         ;
                mov     esi,@@psb_pstr                          ; data to write
            rep movsb                                           ; copying
@@psl_noreplace:
                pop     ecx                                     ;
                pop     edi                                     ;
                dec     @@psb_times                             ; times--;
                jnz     @@psl_cycle                             ;
@@psl_end:
                mov     eax,@@psb_times                         ; return unreplaced
                mov     edi,@@psb_reslen                        ;
                or      edi,edi                                 ;
                jz      @@psl_nosave                            ; stoplen is 0?
                mov     [edi],ecx                               ; no, save remaining length
@@psl_nosave:
                pop     esi                                     ;
                pop     edi                                     ;
                leave                                           ;
                ret     36                                      ;
_patch_binary   endp                                            ;


;----------------------------------------------------------------
;int  __stdcall bsf32(u32t value);
                public  _bsf32
_bsf32          proc    near
@@bsfd_value    =  4                                            ;
                bsf     eax,dword ptr [esp+@@bsfd_value]        ;
                jnz     @@bsfd_done                             ;
                or      eax,-1                                  ;
@@bsfd_done:
                ret     4                                       ;
_bsf32          endp                                            ;

;----------------------------------------------------------------
;int  __stdcall bsf64(u64t value);
                public  _bsf64
_bsf64          proc    near
@@bsfq_value    =  4                                            ;
                bsf     eax,dword ptr [esp+@@bsfq_value]        ;
                jnz     @@bsfq_done                             ;
                bsf     eax,dword ptr [esp+@@bsfq_value+4]      ;
                jz      @@bsfq_0                                ;
                add     al,32                                   ;
                jmp     @@bsfq_done                             ;
@@bsfq_0:
                or      eax,-1                                  ;
@@bsfq_done:
                ret     8                                       ;
_bsf64          endp                                            ;

;----------------------------------------------------------------
;int  __stdcall bsr32(u32t value);
                public  _bsr32
_bsr32          proc    near
@@bsrd_value    =  4                                            ;
                bsr     eax,dword ptr [esp+@@bsrd_value]        ;
                jnz     @@bsrd_done                             ;
                or      eax,-1                                  ;
@@bsrd_done:
                ret     4                                       ;
_bsr32          endp                                            ;

;----------------------------------------------------------------
;int  __stdcall bsr64(u64t value);
                public  _bsr64
_bsr64          proc    near
@@bsrq_value    =  4                                            ;
                bsr     eax,dword ptr [esp+@@bsrq_value+4]      ;
                lea     eax,[eax+32]                            ;
                jnz     @@bsrq_done                             ;
                bsr     eax,dword ptr [esp+@@bsrq_value]        ;
                jnz     @@bsrq_done                             ;
                or      eax,-1                                  ;
@@bsrq_done:
                ret     8                                       ;
_bsr64          endp                                            ;

;----------------------------------------------------------------
;u32t hlp_copytoflat(void* Destination, u32t Offset, u32t Sel, u32t Length);
                public  _hlp_copytoflat                         ;
_hlp_copytoflat proc    near                                    ;
@@Length        = [ebp+1Ch]                                     ;
@@Sel           = [ebp+18h]                                     ;
@@Offset        = [ebp+14h]                                     ;
@@Destination   = [ebp+10h]                                     ;
                push    esi                                     ;
                push    edi                                     ;
                push    ebp                                     ;
                mov     ebp,esp                                 ;
                mov     esi,@@Offset                            ; 
                mov     ax,ds                                   ; is it FLAT?
                cmp     ax,word ptr @@Sel                       ; skip all checks
                mov     eax,@@Length                            ; (lsl will lie because
                jz      @@cpf_lenok                             ; segment can be up-down)

                lsl     ecx,word ptr @@Sel                      ;
                jnz     @@cpf_exit0                             ;
                sub     ecx,esi                                 ; offset start behind
                jc      @@cpf_exit0                             ; seg limit?
                inc     ecx                                     ;
                cmp     ecx,eax                                 ; check for too long
                jnc     @@cpf_lenok                             ; request
                mov     eax,ecx                                 ;
@@cpf_lenok:
                cld                                             ;
                mov     ecx,eax                                 ;
                shr     ecx,2                                   ;
                mov     edi,@@Destination                       ;
                push    gs                                      ;
                mov     gs,@@Sel                                ;
            rep movs    dword ptr es:[edi],dword ptr gs:[esi]   ; copy data
                mov     ecx,eax                                 ;
                and     ecx,3                                   ;
            rep movs    byte ptr es:[edi],byte ptr gs:[esi]     ;
                pop     gs                                      ;
                jmp     @@cpf_exit                              ;
@@cpf_exit0:
                xor     eax,eax                                 ; 0 bytes copied
@@cpf_exit:
                mov     esp,ebp                                 ;
                pop     ebp                                     ;
                pop     edi                                     ;
                pop     esi                                     ;
                ret     16                                      ;
_hlp_copytoflat endp                                            ;

;----------------------------------------------------------------
;u8t* __stdcall memrchr(u8t*mem, u8t chr, u32t buflen);
                public  _memrchr
_memrchr        proc    near
@@mem           =  4                                            ;
@@chr           =  8                                            ;
@@buflen        = 12                                            ;
                push    edi                                     ;
                mov     edi,[esp+4+@@mem]                       ;
                mov     ecx,[esp+4+@@buflen]                    ;
                mov     eax,[esp+4+@@chr]                       ;
                lea     edi,[edi+ecx-1]                         ;
                pushf                                           ;
                std                                             ;
          repne scasb                                           ;
                mov     eax,ecx                                 ;
                jnz     @@bye0                                  ;
                lea     eax,[edi+1]                             ;
@@bye0:
                popf                                            ;
                pop     edi                                     ;
                ret     12                                      ;
_memrchr        endp                                            ;

;----------------------------------------------------------------
;u8t* __stdcall memrchrnb(u8t*mem, u8t chr, u32t buflen);
                public  _memrchrnb     
_memrchrnb      proc    near
@@mem           =  4                                            ;
@@chr           =  8                                            ;
@@buflen        = 12                                            ;
                push    edi                                     ;
                mov     edi,[esp+4+@@mem]                       ;
                mov     eax,[esp+4+@@chr]                       ;
                mov     ecx,[esp+4+@@buflen]                    ;
                lea     edi,[edi+ecx-1]                         ;
                pushf                                           ;
                std                                             ;
           repe scasb                                           ;
                mov     eax,ecx                                 ;
                jz      @@bye1                                  ;
                lea     eax,[edi+1]                             ;
@@bye1:
                popf                                            ;
                pop     edi                                     ;
                ret     12                                      ;
_memrchrnb      endp                                            ;

;----------------------------------------------------------------
;u16t* __stdcall memrchrw(u16t*mem, u16t chr, u32t buflen);
                public  _memrchrw
_memrchrw       proc    near
@@mem           =  4                                            ;
@@chr           =  8                                            ;
@@buflen        = 12                                            ;
                push    edi                                     ;
                mov     edi,[esp+4+@@mem]                       ;
                mov     eax,[esp+4+@@chr]                       ;
                mov     ecx,[esp+4+@@buflen]                    ;
                lea     edi,[edi+ecx*2-2]                       ;
                pushf                                           ;
                std                                             ;
          repne scasw                                           ;
                mov     eax,ecx                                 ;
                jnz     @@bye2                                  ;
                lea     eax,[edi+2]                             ;
@@bye2:
                popf                                            ;
                pop     edi                                     ;
                ret     12                                      ;
_memrchrw       endp                                            ;

;----------------------------------------------------------------
;u16t* __stdcall memrchrnw(u16t*mem, u16t chr, u32t buflen);
                public  _memrchrnw
_memrchrnw      proc    near
@@mem           =  4                                            ;
@@chr           =  8                                            ;
@@buflen        = 12                                            ;
                push    edi                                     ;
                mov     edi,[esp+4+@@mem]                       ;
                mov     eax,[esp+4+@@chr]                       ;
                mov     ecx,[esp+4+@@buflen]                    ;
                lea     edi,[edi+ecx*2-2]                       ;
                pushf                                           ;
                std                                             ;
           repe scasw                                           ;
                mov     eax,ecx                                 ;
                jz      @@bye3                                  ;
                lea     eax,[edi+2]                             ;
@@bye3:
                popf                                            ;
                pop     edi                                     ;
                ret     12                                      ;
_memrchrnw      endp                                            ;

;----------------------------------------------------------------
;u32t* __stdcall memrchrd(u32t*mem, u32t chr, u32t buflen);
                public  _memrchrd
_memrchrd       proc    near
@@mem           =  4                                            ;
@@chr           =  8                                            ;
@@buflen        = 12                                            ;
                push    edi                                     ;
                mov     edi,[esp+4+@@mem]                       ;
                mov     eax,[esp+4+@@chr]                       ;
                mov     ecx,[esp+4+@@buflen]                    ;
                lea     edi,[edi+ecx*4-4]                       ;
                pushf                                           ;
                std                                             ;
          repne scasd                                           ;
                mov     eax,ecx                                 ;
                jnz     @@bye4                                  ;
                lea     eax,[edi+4]                             ;
@@bye4:
                popf                                            ;
                pop     edi                                     ;
                ret     12                                      ;
_memrchrd       endp                                            ;

;----------------------------------------------------------------
;u32t* __stdcall memrchrnd(u32t*mem, u32t chr, u32t buflen);
                public  _memrchrnd
_memrchrnd      proc    near
@@mem           =  4                                            ;
@@chr           =  8                                            ;
@@buflen        = 12                                            ;
                push    edi                                     ;
                mov     edi,[esp+4+@@mem]                       ;
                mov     eax,[esp+4+@@chr]                       ;
                mov     ecx,[esp+4+@@buflen]                    ;
                lea     edi,[edi+ecx*4-4]                       ;
                pushf                                           ;
                std                                             ;
           repe scasd                                           ;
                mov     eax,ecx                                 ;
                jz      @@bye5                                  ;
                lea     eax,[edi+4]                             ;
@@bye5:
                popf                                            ;
                pop     edi                                     ;
                ret     12                                      ;
_memrchrnd      endp                                            ;

;----------------------------------------------------------------
;u64t* __stdcall memrchrq(u64t*mem, u64t chr, u32t buflen);
                public  _memrchrq
_memrchrq       proc    near
@@mem           =  4                                            ;
@@chr           =  8                                            ;
@@buflen        = 16                                            ;
                push    ebx                                     ;
                push    esi                                     ;
                push    edi                                     ;
                mov     esi,[esp+12+@@mem]                      ;
                mov     edx,[esp+12+@@chr]                      ;
                mov     edi,[esp+12+@@chr+4]                    ;
                mov     ecx,[esp+12+@@buflen]                   ;
                pushf                                           ;
                std                                             ;
                lea     esi,[esi+ecx*8-4]                       ;
@@mchq_loop:
                dec     ecx                                     ;
                js      @@mchq_notfound                         ;
                lodsd                                           ;
                mov     ebx,eax                                 ;
                lodsd                                           ;
                cmp     ebx,edi                                 ;
                jnz     @@mchq_loop                             ;
                cmp     eax,edx                                 ;
                jnz     @@mchq_loop                             ;
                lea     eax,[esi+8]                             ;
@@mchq_exit:
                popf                                            ;
                pop     edi                                     ;
                pop     esi                                     ;
                pop     ebx                                     ;
                ret     16                                      ;
@@mchq_notfound:
                xor     eax,eax                                 ;
                jmp     @@mchq_exit                             ;
_memrchrq       endp                                            ;

;----------------------------------------------------------------
;u64t* __stdcall memrchrnq(u64t*mem, u64t chr, u32t buflen);
                public  _memrchrnq
_memrchrnq      proc    near
@@mem           =  4                                            ;
@@chr           =  8                                            ;
@@buflen        = 16                                            ;
                push    ebx                                     ;
                push    esi                                     ;
                push    edi                                     ;
                mov     esi,[esp+12+@@mem]                      ;
                mov     edx,[esp+12+@@chr]                      ;
                mov     edi,[esp+12+@@chr+4]                    ;
                mov     ecx,[esp+12+@@buflen]                   ;
                pushf                                           ;
                std                                             ;
                lea     esi,[esi+ecx*8-4]                       ;
@@mchnq_loop:
                dec     ecx                                     ;
                js      @@mchnq_notfound                        ;
                lodsd                                           ;
                mov     ebx,eax                                 ;
                lodsd                                           ;
                cmp     ebx,edi                                 ;
                jz      @@mchnq_loop                            ;
                cmp     eax,edx                                 ;
                jz      @@mchnq_loop                            ;
                lea     eax,[esi+8]                             ;
@@mchnq_exit:
                pop     edi                                     ;
                pop     esi                                     ;
                pop     ebx                                     ;
                ret     16                                      ;
@@mchnq_notfound:
                xor     eax,eax                                 ;
                jmp     @@mchnq_exit                            ;
_memrchrnq      endp                                            ;

;----------------------------------------------------------------
;void   __stdcall memxchg(void *m1, void *m2, u32t length);
                public  _memxchg                                ;
_memxchg        proc    near                                    ;
@@m1            =  4                                            ;
@@m2            =  8                                            ;
@@length        = 12                                            ;
                mov     ecx, [esp+@@length]                     ;
                jecxz   @@mxchg_exit                            ;
                push    edi                                     ;
                mov     edi, [esp+4+@@m1]                       ;
                push    esi                                     ;
                mov     esi, [esp+8+@@m2]                       ;
                cld                                             ;
                push    ecx                                     ;
                shr     ecx,2                                   ;
                jz      @@mxchg_loop1s                          ;
@@mxchg_loop4:
                lodsd                                           ;
                mov     edx,[edi]                               ;
                dec     ecx                                     ;
                stosd                                           ;
                mov     [esi-4],edx                             ;
                jnz     @@mxchg_loop4                           ;
@@mxchg_loop1s:
                pop     ecx                                     ;
                and     ecx,3                                   ;
@@mxchg_loop1:
                lodsb                                           ;
                mov     dl,[edi]                                ;
                dec     ecx                                     ;
                stosb                                           ;
                mov     [esi-1],dl                              ;
                jnz     @@mxchg_loop1                           ;

                pop     esi                                     ;
                pop     edi                                     ;
@@mxchg_exit:
                ret     12                                      ;
_memxchg        endp

;----------------------------------------------------------------
;u16t   __stdcall bswap16(u16t value);
                public  _bswap16                                ;
_bswap16        proc    near                                    ;
                movzx   eax,word ptr [esp+4]                    ;
                xchg    ah,al                                   ;
                ret     4                                       ;
_bswap16        endp

;----------------------------------------------------------------
;u32t   __stdcall bswap32(u32t value);
                public  _bswap32                                ;
_bswap32        proc    near                                    ;
                mov     eax,[esp+4]                             ;
                bswap   eax                                     ;
                ret     4                                       ;
_bswap32        endp

;----------------------------------------------------------------
;u64t   __stdcall bswap64(u64t value);
                public  _bswap64                                ;
_bswap64        proc    near                                    ;
                mov     edx,[esp+4]                             ;
                mov     eax,[esp+8]                             ;
                bswap   edx                                     ;
                bswap   eax                                     ;
                ret     8                                       ;
_bswap64        endp

;----------------------------------------------------------------
;wchar_t *__stdcall wcscpy(wchar_t *dst, const wchar_t *src);
                public _wcscpy
_wcscpy         proc near
@@str1          =  4                                            ;
@@str2          =  8                                            ;
                push    esi                                     ;
                push    edi                                     ;
                mov     esi, [esp+8+@@str2]                     ;
                mov     edi, [esp+8+@@str1]                     ;
@@loop_cpy:
                lodsw                                           ;
                stosw                                           ;
                test    ax,ax                                   ;
                jnz     @@loop_cpy                              ;
                pop     edi                                     ;
                mov     eax, [esp+4+@@str1]                     ;
                pop     esi                                     ;
                ret     8                                       ;
_wcscpy         endp

;----------------------------------------------------------------
;wchar_t *__stdcall wcsncpy(wchar_t *dst, const wchar_t *src, u32t n);
                public _wcsncpy
_wcsncpy        proc near
@@str1          =  4                                            ;
@@str2          =  8                                            ;
@@len           = 12                                            ;
                push    esi                                     ;
                mov     ecx, [esp+4+@@len]                      ;
                push    edi                                     ;
                mov     esi, [esp+8+@@str2]                     ;
                mov     edi, [esp+8+@@str1]                     ;
@@loop:
                lodsw                                           ;
                stosw                                           ;
                test    ax,ax                                   ;
                loopne  @@loop                                  ;

                xor     eax,eax                                 ;
                shr     ecx,1                                   ;
            rep stosd                                           ;
                adc     ecx,ecx                                 ;
            rep stosw                                           ;
                pop     edi                                     ;
                mov     eax, [esp+4+@@str1]                     ;
                pop     esi                                     ;
                ret     12                                      ;
_wcsncpy        endp

;----------------------------------------------------------------
;u64t  __stdcall mt_safeqadd(u64t *src, u64t value);
;u64t  __stdcall mt_safeqand(u64t *src, u64t value);
;u64t  __stdcall mt_safeqor (u64t *src, u64t value);
;u64t  __stdcall mt_safeqxor(u64t *src, u64t value);
                public  _mt_safeqadd
                public  _mt_safeqand, _mt_safeqor, _mt_safeqxor
_mt_safeqadd    proc    near                                    ;
                mov     ecx,[esp+4]                             ;
                mov     eax,[esp+8]                             ;
                mov     edx,[esp+12]                            ;
                pushfd                                          ;
                cli                                             ;
                add     eax,[ecx]                               ;
                adc     edx,[ecx+4]                             ;
@@mtsq_exit:
                mov     [ecx],eax                               ;
                mov     [ecx+4],edx                             ;
                popfd                                           ;
                ret     12                                      ;
_mt_safeqand    label   near                                    ;
                mov     ecx,[esp+4]                             ;
                mov     eax,[esp+8]                             ;
                mov     edx,[esp+12]                            ;
                pushfd                                          ;
                cli                                             ;
                and     eax,[ecx]                               ;
                and     edx,[ecx+4]                             ;
                jmp     @@mtsq_exit                             ;
_mt_safeqor     label   near                                    ;
                mov     ecx,[esp+4]                             ;
                mov     eax,[esp+8]                             ;
                mov     edx,[esp+12]                            ;
                pushfd                                          ;
                cli                                             ;
                or      eax,[ecx]                               ;
                or      edx,[ecx+4]                             ;
                jmp     @@mtsq_exit                             ;
_mt_safeqxor    label   near                                    ;
                mov     ecx,[esp+4]                             ;
                mov     eax,[esp+8]                             ;
                mov     edx,[esp+12]                            ;
                pushfd                                          ;
                cli                                             ;
                xor     eax,[ecx]                               ;
                xor     edx,[ecx+4]                             ;
                jmp     @@mtsq_exit                             ;
_mt_safeqadd    endp

;----------------------------------------------------------------
;u64t  __stdcall mt_cmpxchgq(u64t *src, u64t value, u64t cmpvalue);
                public  _mt_cmpxchgq                            ;
_mt_cmpxchgq    proc    near                                    ;
                push    edi                                     ;
                push    ebx                                     ;

                mov     edi,[esp+12]                            ;
                mov     ecx,[esp+16]                            ;
                mov     ebx,[esp+20]                            ;
                mov     eax,[esp+24]                            ;
                mov     edx,[esp+28]                            ;
           lock cmpxchg8b [edi]                                 ;
                pop     ebx                                     ;
                pop     edi                                     ;
                ret     20                                      ;
_mt_cmpxchgq    endp                                            ;

;----------------------------------------------------------------
;void* __stdcall memcpy0(void *dst, void *src, u32t length);
                public  _memcpy0                                ;
_memcpy0        proc    near                                    ;
@@dst           =  4                                            ;
@@src           =  8                                            ;
@@length        = 12                                            ;
                push    esi                                     ;
                mov     esi, [esp+4+@@src]                      ;
                push    edi                                     ;
                mov     edi, [esp+8+@@dst]                      ;
                mov     ecx, [esp+8+@@length]                   ;
                push    es                                      ;
                mov     ax,ss                                   ; ss is pure FLAT
                mov     es,ax                                   ; in any mode
                test    ecx,ecx                                 ;
                jz      @@move0                                 ;
                cmp     esi,edi                                 ;
                jnc     @@move_forward                          ;
                lea     eax,[esi+ecx]                           ;
                cmp     edi,eax                                 ;
                jnc     @@move_forward                          ;
                std                                             ; move backward
                add     esi, ecx                                ;
                add     edi, ecx                                ;
                mov     eax, ecx                                ;
                and     ecx, 3                                  ;
                dec     esi                                     ;
                dec     edi                                     ;
            rep movs    byte ptr es:[esi], byte ptr ss:[esi]    ;
                mov     ecx, eax                                ;
                shr     ecx, 2                                  ;
                jz      @@move0                                 ;
                sub     esi, 3                                  ;
                sub     edi, 3                                  ;
            rep movs    dword ptr es:[esi], dword ptr ss:[esi]  ;
                jmp     @@move0                                 ;
@@move_forward:
                cld                                             ; move forward
@@move_align:
                test    edi, 3                                  ; make sure data is aligned
                jz      @@move_aligned                          ;
                movs    byte ptr es:[esi], byte ptr ss:[esi]    ;
                dec     ecx                                     ;
                jz      @@move0                                 ;
                jmp     @@move_align                            ;
@@move_aligned:
                mov     eax, ecx                                ;
                shr     ecx, 2                                  ;
                and     al, 3                                   ;
            rep movs    dword ptr es:[esi], dword ptr ss:[esi]  ;
                mov     cl, al                                  ;
            rep movs    byte ptr es:[esi], byte ptr ss:[esi]    ;
@@move0:
                pop     es                                      ;
                pop     edi                                     ;
                pop     esi                                     ;
                cld                                             ;
                mov     eax, [esp+@@dst]                        ;
                ret     12                                      ;
_memcpy0        endp


CODE32          ends

                end
