;
; QSINIT "start" module
; Watcom C++ runtime trick code
;
                .486p

                extrn  __wcpp_4_module_dtor_:near
                extrn  _exit_with_popup:near

CODE32          segment byte public USE32 'CODE'
                assume cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT
                extrn  _exit:near

                public  __EnterWVIDEO
                public  __exit_with_msg
                public  exit
__EnterWVIDEO:
                xor     eax,eax                                 ; return 0
                ret                                             ;
__exit_with_msg:
                pop     eax                                     ; pop ret addr and use
                call    _exit_with_popup                        ; args of original call
exit:
                jmp     _exit                                   ; one parameter

CODE32          ends

YI              segment word public USE32 'DATA'                ; call static classes
                db      0, 32                                   ; destructors in common
                dd      offset __wcpp_4_module_dtor_            ; C++ library
YI              ends                                            ;

                end
