;
; QSINIT "start" module
; init/fini segments
;
                .486p

; put it here to allow eliminate it by linker
CODE32          segment byte public USE32 'CODE'
                extrn   _mem_free:near
                public  _free
_free           label   near
                jmp     _mem_free
CODE32          ends

                end
