;
; QSINIT
; help calls
;

                include inc/segdef.inc
                include inc/seldesc.inc
                include inc/qstypes.inc
                include inc/serial.inc
                include inc/debug.inc
                include inc/dpmi.inc
                include inc/vbedata.inc
                include inc/cpudef.inc

                extrn   _syscr3  :dword                         ;
                extrn   _syscr4  :dword                         ;
                extrn   _systr   :word                          ;
                extrn   _IODelay :word                          ;
                extrn   _pinfo_gs:word                          ; process context

_DATA           segment                                         ;
cpuid_supported db      0                                       ;
msr_supported   db      2                                       ; bit 2 = init me
_DATA           ends                                            ;

_TEXT           segment
                assume  cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT
                .686p

;----------------------------------------------------------------
;void *__stdcall memset(void *dst, int c, unsigned long length);
                public  _memset                                 ;
_memset         proc    near                                    ;
@@dst           =  4                                            ;
@@char          =  8                                            ;
@@length        = 12                                            ;
                mov     ecx, [esp+@@length]                     ;
                jecxz   @@set0                                  ;
                push    edi                                     ;
                mov     edi, [esp+4+@@dst]                      ;
                xor     eax, eax                                ;
                mov     al, [esp+4+@@char]                      ;
                cld                                             ;
                push    ecx                                     ;
                shr     ecx,2                                   ;
                jz      @@setb                                  ;
                mov     ah,al                                   ;
                push    ax                                      ;
                bswap   eax                                     ;
                pop     ax                                      ;
            rep stosd                                           ;
@@setb:
                pop     ecx                                     ;
                and     ecx,3                                   ;
            rep stosb                                           ;
                pop     edi                                     ;
@@set0:                                                         ;
                mov     eax, [esp+@@dst]                        ;
                ret     12                                      ;
_memset         endp

;----------------------------------------------------------------
;u16t* __stdcall memsetw (u16t *dst, u16t value, u32t length);
                public  _memsetw                                ;
_memsetw        proc    near                                    ;
@@dst           =  4                                            ;
@@char          =  8                                            ;
@@length        = 12                                            ;
                mov     ecx, [esp+@@length]                     ;
                jecxz   @@setw0                                 ;
                push    edi                                     ;
                mov     edi, [esp+4+@@dst]                      ;
                mov     ax, [esp+4+@@char]                      ;
                cld                                             ;
            rep stosw                                           ;
                pop     edi                                     ;
@@setw0:
                mov     eax, [esp+@@dst]                        ;
                ret     12                                      ;
_memsetw        endp

;----------------------------------------------------------------
;u32t* __stdcall memsetd (u32t *dst, u32t value, u32t length);
                public  _memsetd                                ;
_memsetd        proc    near                                    ;
@@dst           =  4                                            ;
@@char          =  8                                            ;
@@length        = 12                                            ;
                mov     ecx, [esp+@@length]                     ;
                jecxz   @@setd0                                 ;
                push    edi                                     ;
                mov     edi, [esp+4+@@dst]                      ;
                mov     eax, [esp+4+@@char]                     ;
                cld                                             ;
            rep stosd                                           ;
                pop     edi                                     ;
@@setd0:
                mov     eax, [esp+@@dst]                        ;
                ret     12                                      ;
_memsetd        endp

;----------------------------------------------------------------
;void* __stdcall memcpy(void *dst, void *src, u32t length);
                public  _memcpy                                 ;
_memcpy         proc    near                                    ;
@@dst           =  4                                            ;
@@src           =  8                                            ;
@@length        = 12                                            ;
                mov     ecx, [esp+@@length]                     ;
                jecxz   @@cpy0                                  ;
                push    edi                                     ;
                mov     edi, [esp+4+@@dst]                      ;
                push    esi                                     ;
                mov     esi, [esp+8+@@src]                      ;
                cld                                             ;
                push    ecx                                     ;
                shr     ecx,2                                   ;
                jz      @@cpyb                                  ;
            rep movsd                                           ;
@@cpyb:
                pop     ecx                                     ;
                and     ecx,3                                   ;
            rep movsb                                           ;
                pop     esi                                     ;
                pop     edi                                     ;
@@cpy0:                                                         ;
                mov     eax, [esp+@@dst]                        ;
                ret     12                                      ;
_memcpy         endp


;----------------------------------------------------------------
;void* __stdcall memmove(void *dst, void *src, u32t length);
                public  _memmove                                ;
_memmove        proc    near                                    ;
@@dst           =  4                                            ;
@@src           =  8                                            ;
@@length        = 12                                            ;
                push    esi                                     ;
                mov     esi, [esp+4+@@src]                      ;
                push    edi                                     ;
                mov     edi, [esp+8+@@dst]                      ;
                mov     ecx, [esp+8+@@length]                   ;
                test    ecx,ecx                                 ;
                jz      @@move0                                 ;
                cmp     esi,edi                                 ;
                jae     @@move_forward                          ;
                std                                             ; move backward
                add     esi, ecx                                ;
                add     edi, ecx                                ;
                mov     eax, ecx                                ;
                and     ecx, 3                                  ;
                dec     esi                                     ;
                dec     edi                                     ;
                rep     movsb                                   ;
                mov     ecx, eax                                ;
                shr     ecx, 2                                  ;
                jz      @@move0                                 ;
                sub     esi, 3                                  ;
                sub     edi, 3                                  ;
                rep     movsd                                   ;
                jmp     @@move0                                 ;
@@move_forward:
                cld                                             ; move forward
@@move_align:
                test    edi, 3                                  ; make sure data is aligned
                jz      @@move_aligned                          ;
                movsb                                           ;
                dec     ecx                                     ;
                jz      @@move0                                 ;
                jmp     @@move_align                            ;
@@move_aligned:
                mov     eax, ecx                                ;
                shr     ecx, 2                                  ;
                and     al, 3                                   ;
                rep     movsd                                   ;
                mov     cl, al                                  ;
                rep     movsb                                   ;
@@move0:
                pop     edi                                     ;
                pop     esi                                     ;
                cld                                             ;
                mov     eax, [esp+@@dst]                        ;
                ret     12                                      ;
_memmove        endp                                            ;

;----------------------------------------------------------------
;int strncmp(char *s1, char *s2, unsigned long n);
                public _strncmp
_strncmp        proc near
@@str1          =  4                                            ;
@@str2          =  8                                            ;
@@len           = 12                                            ;
                push    esi                                     ;
                mov     ecx, [esp+4+@@len]                      ;
                push    edi                                     ;
                mov     esi, [esp+8+@@str1]                     ;
                push    ds                                      ;
                mov     edi, esi                                ;
                pop     es                                      ;
                xor     eax, eax                                ;
          repne scasb                                           ;
                sub     edi, esi                                ;
                mov     ecx, edi                                ;
                mov     edi, [esp+8+@@str2]                     ;
           repe cmpsb                                           ;
                jz      @@rcok                                  ;
                sbb     eax, eax                                ;
                sbb     eax, 0FFFFFFFFh                         ;
@@rcok:
                pop     edi                                     ;
                pop     esi                                     ;
                ret     12                                      ;
_strncmp        endp                                            ;

;----------------------------------------------------------------
;int strnicmp(char *s1, char *s2, unsigned long n);
                public _strnicmp
_strnicmp       proc near
@@str1          =  4                                            ;
@@str2          =  8                                            ;
@@len           = 12                                            ;

                push    esi                                     ;
                mov     ecx, [esp+4+@@len]                      ;
                push    edi                                     ;
                mov     esi, [esp+8+@@str1]                     ;
                push    ds                                      ;
                mov     edi, esi                                ;
                pop     es                                      ;
                xor     eax, eax                                ;
          repne scasb                                           ;
                sub     edi, esi                                ;
                mov     ecx, edi                                ;
                mov     edi, [esp+8+@@str2]                     ;
@@nic_cmp_loop:
                lodsb                                           ;
                call    @@nic_upper_al                          ;
                mov     ah, al                                  ;
                mov     al, es:[edi]                            ;
                inc     edi                                     ;
                call    @@nic_upper_al                          ;
                cmp     ah, al                                  ;
                loope   @@nic_cmp_loop                          ;
                mov     eax, 0                                  ;
                jz      @@nic_rcok                              ;
                sbb     eax, eax                                ;
                sbb     eax, 0FFFFFFFFh                         ;
@@nic_rcok:
                pop     edi                                     ;
                pop     esi                                     ;
                ret     12                                      ;
@@nic_upper_al:
                cmp     al, 61h                                 ;
                jc      @@nic_upper_ok                          ;
                cmp     al, 7Bh                                 ;
                jnc     @@nic_upper_ok                          ;
                sub     al, 20h                                 ;
@@nic_upper_ok:
                ret                                             ;
_strnicmp       endp                                            ;

;----------------------------------------------------------------
;u32t  __stdcall strlen(char *s);
                public  _strlen
_strlen         proc near
@@str         =  4                                              ;
                push    edi                                     ;
                mov     edi,dword ptr [esp+4+@@str]             ;
                xor     ecx,ecx                                 ;
                dec     ecx                                     ;
                xor     al, al                                  ;
          repne scasb                                           ;
                not     ecx                                     ;
                dec     ecx                                     ;
                mov     eax,ecx                                 ;
                pop     edi                                     ;
                ret     4                                       ;
_strlen         endp                                            ;

;----------------------------------------------------------------
;char* __stdcall strchr(char *src, int c);
                public  _strchr
_strchr         proc near
@@src           =  4                                            ;
@@cc            =  8                                            ;
                xor     eax,eax                                 ;
                push    esi                                     ;
                mov     ecx, [esp+4+@@cc]                       ;
                mov     esi, [esp+4+@@src]                      ;
@@strchr_loop:
                lodsb                                           ;
                test    al,al                                   ;
                jz      @@strchr_fail                           ;
                cmp     al,cl                                   ;
                jnz     @@strchr_loop                           ;
                lea     eax,[esi-1]                             ;
@@strchr_fail:
                pop     esi                                     ;
                ret     8                                       ;
_strchr         endp                                            ;

;----------------------------------------------------------------
                public _strncpy
_strncpy        proc near
@@str1          =  4                                            ;
@@str2          =  8                                            ;
@@len           = 12                                            ;
                push    esi                                     ;
                mov     ecx, [esp+4+@@len]                      ;
                push    edi                                     ;
                mov     esi, [esp+8+@@str2]                     ;
                mov     edi, [esp+8+@@str1]                     ;
@@loop:
                lodsb                                           ;
                stosb                                           ;
                test    al,al                                   ;
                loopne  @@loop                                  ;
                xor     eax,eax                                 ;
                shr     ecx,1                                   ;
                rep     stosw                                   ;
                adc     cx,cx                                   ;
                rep     stosb                                   ;
                pop     edi                                     ;
                mov     eax, [esp+4+@@str1]                     ;
                pop     esi                                     ;
                ret     12                                      ;
_strncpy        endp

;----------------------------------------------------------------
;char* __stdcall strcpy(char *dst, char *src);
                public _strcpy
_strcpy         proc near
@@str1          =  4                                            ;
@@str2          =  8                                            ;
                push    esi                                     ;
                push    edi                                     ;
                mov     esi, [esp+8+@@str2]                     ;
                mov     edi, [esp+8+@@str1]                     ;
@@loop_cpy:
                lodsb                                           ;
                stosb                                           ;
                test    al,al                                   ;
                jnz     @@loop_cpy                              ;
                pop     edi                                     ;
                mov     eax, [esp+4+@@str1]                     ;
                pop     esi                                     ;
                ret     8                                       ;
_strcpy         endp

;----------------------------------------------------------------
;u8t* __stdcall memchr(u8t*mem, u8t chr, u32t buflen);
                public  _memchr
_memchr         proc    near
@@mem           =  4                                            ;
@@chr           =  8                                            ;
@@buflen        = 12                                            ;
                push    edi                                     ;
                mov     edi,[esp+4+@@mem]                       ;
                mov     eax,[esp+4+@@chr]                       ;
                mov     ecx,[esp+4+@@buflen]                    ;
                repne   scasb                                   ;
                mov     eax,ecx                                 ;
                jnz     @@bye0                                  ;
                lea     eax,[edi-1]                             ;
@@bye0:                                                         ;
                pop     edi                                     ;
                ret     12                                      ;
_memchr         endp                                            ;

;----------------------------------------------------------------
;u8t* __stdcall memchrnb(u8t*mem, u8t chr, u32t buflen);
                public  _memchrnb
_memchrnb       proc    near
@@mem           =  4                                            ;
@@chr           =  8                                            ;
@@buflen        = 12                                            ;
                push    edi                                     ;
                mov     edi,[esp+4+@@mem]                       ;
                mov     eax,[esp+4+@@chr]                       ;
                mov     ecx,[esp+4+@@buflen]                    ;
                repe    scasb                                   ;
                mov     eax,ecx                                 ;
                jz      @@bye1                                  ;
                lea     eax,[edi-1]                             ;
@@bye1:                                                         ;
                pop     edi                                     ;
                ret     12                                      ;
_memchrnb       endp                                            ;

;----------------------------------------------------------------
;u16t* __stdcall memchrw(u16t*mem, u16t chr, u32t buflen);
                public  _memchrw
_memchrw        proc    near
@@mem           =  4                                            ;
@@chr           =  8                                            ;
@@buflen        = 12                                            ;
                push    edi                                     ;
                mov     edi,[esp+4+@@mem]                       ;
                mov     eax,[esp+4+@@chr]                       ;
                mov     ecx,[esp+4+@@buflen]                    ;
                repne   scasw                                   ;
                mov     eax,ecx                                 ;
                jnz     @@bye2                                  ;
                lea     eax,[edi-2]                             ;
@@bye2:                                                         ;
                pop     edi                                     ;
                ret     12                                      ;
_memchrw        endp                                            ;

;----------------------------------------------------------------
;u16t* __stdcall memchrnw(u16t*mem, u16t chr, u32t buflen);
                public  _memchrnw
_memchrnw       proc    near
@@mem           =  4                                            ;
@@chr           =  8                                            ;
@@buflen        = 12                                            ;
                push    edi                                     ;
                mov     edi,[esp+4+@@mem]                       ;
                mov     eax,[esp+4+@@chr]                       ;
                mov     ecx,[esp+4+@@buflen]                    ;
                repe    scasw                                   ;
                mov     eax,ecx                                 ;
                jz      @@bye3                                  ;
                lea     eax,[edi-2]                             ;
@@bye3:                                                         ;
                pop     edi                                     ;
                ret     12                                      ;
_memchrnw       endp                                            ;

;----------------------------------------------------------------
;u32t* __stdcall memchrd(u32t*mem, u32t chr, u32t buflen);
                public  _memchrd
_memchrd        proc    near
@@mem           =  4                                            ;
@@chr           =  8                                            ;
@@buflen        = 12                                            ;
                push    edi                                     ;
                mov     edi,[esp+4+@@mem]                       ;
                mov     eax,[esp+4+@@chr]                       ;
                mov     ecx,[esp+4+@@buflen]                    ;
                repne   scasd                                   ;
                mov     eax,ecx                                 ;
                jnz     @@bye4                                  ;
                lea     eax,[edi-4]                             ;
@@bye4:                                                         ;
                pop     edi                                     ;
                ret     12                                      ;
_memchrd        endp                                            ;

;----------------------------------------------------------------
;u32t* __stdcall memchrnd(u32t*mem, u32t chr, u32t buflen);
                public  _memchrnd
_memchrnd       proc    near
@@mem           =  4                                            ;
@@chr           =  8                                            ;
@@buflen        = 12                                            ;
                push    edi                                     ;
                mov     edi,[esp+4+@@mem]                       ;
                mov     eax,[esp+4+@@chr]                       ;
                mov     ecx,[esp+4+@@buflen]                    ;
                repe    scasd                                   ;
                mov     eax,ecx                                 ;
                jz      @@bye5                                  ;
                lea     eax,[edi-4]                             ;
@@bye5:                                                         ;
                pop     edi                                     ;
                ret     12                                      ;
_memchrnd       endp                                            ;

;----------------------------------------------------------------
;u32t     __stdcall wcslen(const wchar_t *str);
                public  _wcslen
_wcslen         proc near
@@str         =  4                                              ;
                push    edi                                     ;
                mov     edi,dword ptr [esp+4+@@str]             ;
                xor     ecx,ecx                                 ;
                dec     ecx                                     ;
                xor     ax, ax                                  ;
          repne scasw                                           ;
                not     ecx                                     ;
                dec     ecx                                     ;
                mov     eax,ecx                                 ;
                pop     edi                                     ;
                ret     4                                       ;
_wcslen         endp                                            ;

;----------------------------------------------------------------
;u16t hlp_selalloc(u32t count);
                public  _hlp_selalloc
_hlp_selalloc   proc    near
@@count         =  4                                            ;
                mov     ecx,[esp+@@count]                       ;
                xor     eax,eax                                 ;
                int     31h                                     ;
                jnc     @@selal_ok                              ;
                xor     eax,eax                                 ;
@@selal_ok:
                ret     4                                       ;
_hlp_selalloc   endp                                            ;

;----------------------------------------------------------------
;u32t hlp_selfree(u32t selector);
                public  _hlp_selfree
_hlp_selfree    proc    near
@@selector      =  4                                            ;
                push    ebx                                     ;
                mov     ebx,[esp+4+@@selector]                  ;
                xor     eax,eax                                 ;
                cmp     bx,SEL32DATA                            ; check for system
                jbe     @@selfree_ok                            ; selector
                inc     eax                                     ;
                int     31h                                     ; free it
                sbb     eax,eax                                 ;
                inc     eax                                     ; set bool rc
@@selfree_ok:
                pop     ebx                                     ;
                ret     4                                       ;
_hlp_selfree    endp                                            ;


;----------------------------------------------------------------
;u32t  hlp_selsetup(u16t selector, u32t base, u32t limit, u32 type);
                public  _hlp_selsetup
_hlp_selsetup   proc    near
@@selector      =  4                                            ;
@@base          =  8                                            ;
@@limit         = 12                                            ;
@@type          = 16                                            ;
                push    edi                                     ;
                push    ebx                                     ;
                lea     edi,[esp-8]                             ; descriptor buffer
                mov     esp,edi                                 ; in edi
                mov     ebx,[esp+16+@@selector]                 ;
                xor     eax,eax                                 ;
                cmp     bx,SEL32DATA                            ; check for system
                jbe     @@selsetup_ok                           ; selector
                mov     edx,[esp+16+@@type]                     ;
                shr     edx,1                                   ; code selector?
                jc      @@selsetup_code                         ;
                mov     [edi].d_access,D_DATA0                  ;
                jmp     short @@selsetup_02                     ;
@@selsetup_code:
                mov     [edi].d_access,D_CODE0                  ;
@@selsetup_02:
                xor     cl,cl                                   ;
                shr     edx,1                                   ; 16 bit?
                jc      @@selsetup_03                           ;
                or      cl,D_DBIG                               ;
@@selsetup_03:
                shr     edx,1                                   ; limit in bytes?
                jc      @@selsetup_04                           ;
                or      cl,D_GRAN4K                             ;
@@selsetup_04:
                mov     eax,[esp+16+@@base]                     ;
                shr     edx,1                                   ; base is physical?
                jnc     @@selsetup_05                           ;
;                sub     eax,_ZeroAddress                        ;
@@selsetup_05:
                shr     edx,1                                   ;
                jnc     @@selsetup_06                           ; R3 segment
                add     [edi].d_access,D_DPL3                   ;
@@selsetup_06:
                mov     [edi].d_loaddr,ax                       ;
                bswap   eax                                     ;
                mov     [edi].d_extaddr,al                      ;
                mov     [edi].d_hiaddr,ah                       ;
                mov     eax,[esp+16+@@limit]                    ;
                mov     [edi].d_limit,ax                        ;
                shr     eax,16                                  ; setup limit
                and     al,0Fh                                  ;
                or      al,cl                                   ;
                mov     [edi].d_attr,al                         ;
                mov     ax,0Ch                                  ; set code selector desc
;                push    eax                                     ; debug print
;                push    edx                                     ;
;                mov     eax,[edi]                               ;
;                mov     edx,[edi+4]                             ;
;                dbg32print <2>,<"sel %4X desc: %08X %08X",10>,<eax,edx,ebx>
;                pop     edx                                     ;
;                pop     eax                                     ;
                push    ds                                      ;
                pop     es                                      ;
                int     31h                                     ;
                setnc   al                                      ;
                movzx   eax,al                                  ;
@@selsetup_ok:
                add     esp,8                                   ;
                pop     ebx                                     ;
                pop     edi                                     ;
                ret     16                                      ;
_hlp_selsetup   endp


; u32t hlp_selbase(u32t Sel, u32t *Limit);
;----------------------------------------------------------------
; signal error in 'C' flag as bonus (used in mod_context below)
;
                public  _hlp_selbase                            ;
_hlp_selbase    proc    near                                    ;
@@selq_limit    = [ebp+18h]                                     ;
@@selq_sel      = [ebp+14h]                                     ;
                push    esi                                     ;
                push    edi                                     ;
                push    ebx                                     ;
                push    ebp                                     ;
                mov     ebp,esp                                 ;
                sub     esp,8                                   ; buffer for selector
                mov     edi,esp                                 ; info
                mov     bx,@@selq_sel                           ;
                mov     ax,000Bh                                ;
                int     31h                                     ;
                jc      @@selq_exit0                            ;
                mov     eax,dword ptr [edi+desctab_s.d_loaddr-1]; calculate base address
                mov     al,[edi+desctab_s.d_extaddr]            ;
                ror     eax,8                                   ;
                mov     esi,@@selq_limit                        ; limit arg is 0?
                or      esi,esi                                 ; clear CF for result ;)
                jz      @@selq_exit                             ;
                lsl     ecx,word ptr @@selq_sel                 ;
                mov     [esi],ecx                               ; saving limit here
                jmp     @@selq_exit                             ;
@@selq_exit0:
                mov     eax,-1                                  ; error, no selector
@@selq_exit:
                mov     esp,ebp                                 ;
                pop     ebp                                     ;
                pop     ebx                                     ;
                pop     edi                                     ;
                pop     esi                                     ;
                ret     8                                       ;
_hlp_selbase    endp                                            ;

;----------------------------------------------------------------
;u32t launch32(module *md,u32t env,u32t cmdline);
                public  _launch32
_launch32       proc    near
@@md            =  4                                            ;
@@env           =  8                                            ;
@@cmdline       = 12                                            ;
                push    ds                                      ;
                pop     es                                      ;
                push    ebx                                     ;
                push    esi                                     ;
                push    edi                                     ;
                push    ebp                                     ;

                mov     esi,[esp+16+@@md]                       ;
                mov     ecx,[esp+16+@@env]                      ;
                mov     edx,[esp+16+@@cmdline]                  ;

                xor     eax,eax                                 ;
                xor     ebx,ebx                                 ;
                xor     edi,edi                                 ;

                mov     ebp,esp                                 ; switching to the
                mov     esp,[esi+12]                            ; app stack
                push    ebp                                     ;
                mov     ebp,[esi+8]                             ;
                push    ebp                                     ; module start point

                push    edx                                     ; command line ptr
                push    ecx                                     ; environment ptr
                push    0                                       ; 0
                push    esi                                     ; module handle

                xor     esi,esi                                 ; zero registers
                xor     ebp,ebp                                 ;
                xor     ecx,ecx                                 ;
                xor     edx,edx                                 ;

                mov     gs,_pinfo_gs                            ; process context

                call    [esp+16]                                ; exec module
                add     esp,20                                  ;
                pop     esp                                     ; cleanup after exit
                push    SEL32DATA                               ;
                mov     es,[esp]                                ;
                pop     ds                                      ;
                cld                                             ;
                sti                                             ;
                xor     cx,cx                                   ;
                mov     fs,cx                                   ;
                mov     gs,_pinfo_gs                            ;

                pop     ebp                                     ;
                pop     edi                                     ;
                pop     esi                                     ;
                pop     ebx                                     ;
                ret     12                                      ;
_launch32       endp

;----------------------------------------------------------------
;u32t dll32init(module *md, u32t term);
                public  _dll32init
_dll32init      proc    near
@@md            =  4                                            ;
@@term          =  8                                            ;
                push    ds                                      ;
                pop     es                                      ;
                push    ebx                                     ;
                push    esi                                     ;
                push    edi                                     ;
                push    ebp                                     ;

                mov     esi,[esp+16+@@md]                       ;
                mov     edx,[esp+16+@@term]                     ;
                push    [esi+8]                                 ; dll start point
                push    edx                                     ; term flag
                push    esi                                     ; module handle

                xor     eax,eax                                 ; zero registers
                xor     ebx,ebx                                 ;
                xor     edi,edi                                 ;
                xor     ecx,ecx                                 ;
                xor     ebp,ebp                                 ;
                xor     esi,esi                                 ;
                xor     edx,edx                                 ;

                mov     gs,_pinfo_gs                            ;

                call    [esp+8]                                 ; exec module
                add     esp,12                                  ;
                push    SEL32DATA                               ;
                mov     es,[esp]                                ;
                pop     ds                                      ;
                cld                                             ;
                sti                                             ;
                xor     cx,cx                                   ;
                mov     fs,cx                                   ;
                mov     gs,_pinfo_gs                            ;

                pop     ebp
                pop     edi
                pop     esi
                pop     ebx
                ret     8
_dll32init      endp

; void __stdcall usleep(u32t usec);
;----------------------------------------------------------------
; preserve all registers because used from asm code too
                public  _usleep
_usleep         proc    near
@@usec          =  4                                            ;
                push    edx                                     ;
                mov     edx,[esp+4+@@usec]                      ;
                inc     edx                                     ; add 1 mks ;)
                push    ecx                                     ;
@@iodelay_cloop:
                movzx   ecx,_IODelay                            ;
                shl     ecx,1                                   ;
                align   4                                       ;
@@iodelay_loop:
                dec     ecx                                     ;
                jnz     @@iodelay_loop                          ;
                dec     edx                                     ;
                jnz     @@iodelay_cloop                         ;
                pop     ecx                                     ;
                pop     edx                                     ;
                ret     4                                       ;
_usleep         endp

;----------------------------------------------------------------
; process_context* _std mod_context(void);
                public  _mod_context                            ;
_mod_context    proc    near                                    ;
                sub     esp,4                                   ; limit buffer
                push    esp                                     ;
                push    gs                                      ;
                call    _hlp_selbase                            ;
                pop     ecx                                     ;
                jc      @@mdqctx_error                          ; no selector
                cmp     ecx,3                                   ; limit <3?
                jc      @@mdqctx_error                          ;
                inc     ecx                                     ;
;                add     eax,_ZeroAddress                        ;
                cmp     ecx,[eax]                               ; limit must be equal
                jnz     @@mdqctx_error                          ; to size field
                ret                                             ;
@@mdqctx_error:
                xor     eax,eax                                 ;
                ret                                             ;
_mod_context    endp

;----------------------------------------------------------------
; int __stdcall _setjmp(jmp_buf env);
                public  __setjmp
__setjmp        proc    near
@@env           =  4                                            ;
                mov     eax,[esp+@@env]                         ;
                mov     [eax].r_ebx,ebx                         ;
                mov     [eax].r_edi,edi                         ;
                mov     [eax].r_esi,esi                         ;
                mov     [eax].r_ebp,ebp                         ;
                mov     [eax].r_ss,ss                           ;
                mov     [eax].r_cs,cs                           ;
                mov     [eax].r_ds,ds                           ;
                mov     [eax].r_es,es                           ;
                mov     [eax].r_fs,fs                           ;
                mov     [eax].r_gs,gs                           ;
                mov     [eax].r_ecx,ecx                         ;
                mov     [eax].r_edx,edx                         ;
                mov     [eax].r_eax,esp                         ;
                push    [esp]                                   ;
                pop     [eax].r_reserved                        ; return point
                pushf                                           ;
                pop     [eax].r_flags                           ; flags
                xor     eax,eax                                 ;
                ret     4                                       ;
__setjmp        endp

;----------------------------------------------------------------
; void  __stdcall _longjmp (jmp_buf env, int return_value);
                public  __longjmp
__longjmp       proc    near
@@env           =  4                                            ;
@@return_value  =  8                                            ;
                mov     eax,[esp+@@env]                         ;
                mov     ecx,[esp+@@return_value]                ;
                or      ecx,ecx                                 ;
                jnz     @@ljmp_nonzero                          ;
                inc     ecx                                     ;
@@ljmp_nonzero:
                mov     ds,[eax].r_ds                           ;
                mov     ebx,[eax].r_ebx                         ;
                mov     edx,[eax].r_edx                         ;
                mov     edi,[eax].r_edi                         ;
                mov     esi,[eax].r_esi                         ;
                mov     ebp,[eax].r_ebp                         ;
                mov     es,[eax].r_es                           ;
                mov     fs,[eax].r_fs                           ;
                mov     gs,[eax].r_gs                           ;

                mov     ss,[eax].r_ss                           ;
                mov     esp,[eax].r_eax                         ;
                add     esp,8                                   ; pop __setjmp

                push    [eax].r_flags                           ; flags
                popf                                            ;
                push    [eax].r_ecx                             ; ecx
                push    [eax].r_reserved                        ; eip
                movzx   eax,[eax].r_cs                          ;
                xchg    eax,[esp+4]                             ; cs
                xchg    eax,ecx                                 ;
                retf                                            ;
__longjmp       endp

;----------------------------------------------------------------
; long  __stdcall str2long (char *str);
                public  _str2long
_str2long       proc    near
@@strp          =  4                                            ;
                push    esi                                     ;
                mov     esi,[esp+4+@@strp]                      ;
                push    ebx                                     ;
                xor     ecx,ecx                                 ;
                xor     eax,eax                                 ;
                xor     bh,bh                                   ;
@@s2int_space:
                lodsb                                           ;
                cmp     al,' '                                  ;
                jz      @@s2int_space                           ;
                cmp     al,'0'                                  ;
                jnz     @@s2int_neg                             ;
                mov     bl,[esi]                                ;
                and     bl,not 20h                              ;
                cmp     bl,'X'                                  ;
                jnz     @@s2int_dec                             ;
                inc     esi                                     ;
@@s2int_hex:                                                    ;
                lodsb                                           ;
                cmp     al,'A'                                  ;
                jnc     @@s2int_hexAF                           ;
                sub     al,'0'                                  ;
                jc      @@s2int_exit                            ;
                cmp     al,10                                   ;
                jnc     @@s2int_exit                            ;
@@s2int_hexadd:
                shl     ecx,4                                   ;
                add     cl,al                                   ;
                jmp     @@s2int_hex                             ;
@@s2int_hexAF:
                and     al,not 20h                              ;
                sub     al,'A'-10                               ;
                cmp     al,16                                   ;
                jnc     @@s2int_exit                            ;
                jmp     @@s2int_hexadd                          ;
@@s2int_neg:
                cmp     al,'-'                                  ;
                jnz     @@s2int_dec                             ;
                inc     bh                                      ; negative value
                lodsb                                           ;
@@s2int_dec:
                cmp     al,'0'                                  ;
                jc      @@s2int_decdone                         ;
                cmp     al,'9'                                  ;
                ja      @@s2int_decdone                         ;
                imul    ecx,10                                  ;
                lea     ecx,[ecx+eax-'0']                       ;
                lodsb                                           ;
                jmp     @@s2int_dec                             ;
@@s2int_decdone:
                or      bh,bh                                   ;
                jz      @@s2int_exit                            ;
                neg     ecx                                     ; '-'
@@s2int_exit:
                mov     eax,ecx                                 ;
                pop     ebx                                     ;
                pop     esi                                     ;
                ret     4                                       ;
_str2long       endp         

;----------------------------------------------------------------
; int _std hlp_getcpuid(u32t index, u32t *data);
                public  _hlp_getcpuid
_hlp_getcpuid   proc    near
                push    edi                                     ;
                test    cpuid_supported,1                       ; cpuid supported?
                jnz     @@gcid_read                             ;
                pushfd                                          ; no? check again ;)
                mov     eax, [esp]                              ;
                xor     dword ptr [esp], CPU_EFLAGS_CPUID       ;
                popfd                                           ;
                pushfd                                          ;
                xor     eax, [esp]                              ;
                setnz   cpuid_supported                         ;
                pop     ecx                                     ;
                jnz     @@gcid_read                             ; ok
                mov     edi, [esp+12]                           ; empty buffer
                stosd                                           ;
                stosd                                           ;
                stosd                                           ;
                stosd                                           ;
                pop     edi                                     ;
                ret     8                                       ;
@@gcid_read:
                mov     eax, [esp+8]                            ; id
                mov     edi, [esp+12]                           ; out buffer
                push    ebx                                     ;
                cpuid                                           ;
                stosd                                           ;
                mov     [edi], ebx                              ;
                pop     ebx                                     ;
                mov     [edi+4], ecx                            ;
                mov     [edi+8], edx                            ;
                pop     edi                                     ;
                mov     eax,1                                   ;
                ret     8
_hlp_getcpuid   endp

; return NZ and eax=1 if supported
msr_avail       proc    near
                test    msr_supported,1                         ; present?
                jnz     @@msrchk_ret                            ;
                test    msr_supported,2                         ; cpuid readed?
                jz      @@msrchk_ret                            ;
                xor     msr_supported,2                         ; drop flag
                sub     esp,16                                  ;
                push    esp                                     ;
                push    1                                       ;
                call    _hlp_getcpuid                           ; cpu id
                add     esp,12                                  ;
                pop     ecx                                     ;
                test    ecx,CPUID_FI2_MSR                       ;
                jz      @@msrchk_ret                            ;
                or      msr_supported,al                        ;
@@msrchk_ret:
                setnz   al                                      ;
                movzx   eax,al                                  ;
                ret                                             ;
msr_avail       endp

;----------------------------------------------------------------
; int _std hlp_readmsr(u32t index, u32t *ddlo, u32t *ddhi);
                public  _hlp_readmsr
_hlp_readmsr    proc    near
                call    msr_avail                               ;
                mov     edx,eax                                 ; edx:eax = 0
                jz      @@msrr_err                              ;
                mov     ecx,[esp+4]                             ;
                rdmsr                                           ; 
@@msrr_err:
                mov     ecx,[esp+8]                             ; write lo dd
                jecxz   @@msrr_no_ddlo                          ;
                mov     [ecx],eax                               ;
@@msrr_no_ddlo:
                mov     ecx,[esp+12]                            ; write hi dd
                jecxz   @@msrr_no_ddhi                          ;
                mov     [ecx],edx                               ;
@@msrr_no_ddhi:
                setnz   al                                      ; result
                movzx   eax,al                                  ;
                ret     12                                      ;
_hlp_readmsr    endp

;----------------------------------------------------------------
; int _std hlp_writemsr(u32t index, u32t ddlo, u32t ddhi);
                public  _hlp_writemsr
_hlp_writemsr   proc    near
                call    msr_avail                               ;
                jz      @@msrr_err                              ;
                mov     eax,[esp+8]                             ;
                mov     edx,[esp+12]                            ;
                mov     ecx,[esp+4]                             ;
                wrmsr                                           ; 
@@msrw_err:
                setnz   al                                      ; result
                movzx   eax,al                                  ;
                ret     12                                      ;
_hlp_writemsr   endp

;----------------------------------------------------------------
; void _std cpuhlt(void);
                public  _cpuhlt
_cpuhlt         proc    near                                    ;
                sti                                             ;
                hlt                                             ;
                ret                                             ;
_cpuhlt         endp

;----------------------------------------------------------------
; void _std sys_setcr3(u32t syscr3, u32t syscr4);
                public  _sys_setcr3
_sys_setcr3     proc    near                                    ;
                pushfd                                          ;
                cli                                             ;
                mov     eax,[esp+8]                             ;
                mov     _syscr3,eax                             ;
                mov     cr3,eax                                 ;
                mov     eax,[esp+12]                            ;
                inc     eax                                     ;
                jz      @@spgr_nocr4                            ;
                dec     eax                                     ;
                mov     _syscr4,eax                             ;
                mov     cr4,eax                                 ;
@@spgr_nocr4:
                popfd                                           ;
                ret     8
_sys_setcr3     endp

;----------------------------------------------------------------
; void _std sys_settr(u16t trsel);
                public  _sys_settr
_sys_settr      proc    near                                    ;
                pushfd                                          ;
                cli                                             ;
                mov     ax,[esp+8]                              ;
                mov     _systr,ax                               ;
                ltr     ax                                      ;
                popfd                                           ;
                ret     4                                       ;
_sys_settr      endp

_TEXT           ends
                end
