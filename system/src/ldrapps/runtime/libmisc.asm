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

; both bsr32 & bsf32 are too small to care about size and too
; annoying in trace output when linked dynamically.
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

CODE32          ends

                end
