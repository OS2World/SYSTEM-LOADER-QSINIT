;
; QSINIT runtime
; misc assembler code
;
                .486p

; put it here to allow eliminate it by linker
CODE32          segment byte public USE32 'CODE'
                extrn   _mem_free:near                          ;
                extrn   _call64:near                            ;
                public  _free                                   ;
_free           label   near                                    ;
                jmp     _mem_free                               ;

; both bsr32 & bsf32 are too small to care about size and too annoying
; in trace output when linked dynamically (as well as bswapX)
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
                public  _call64l                                ;
_call64l:
                jmp     _call64                                 ;
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

CODE32          ends

                end
