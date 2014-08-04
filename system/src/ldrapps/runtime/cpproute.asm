;
; QSINIT "start" module
; Watcom C++ runtime trick code
;
                .486p

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
                pop     eax                                     ; simulate one parameter
exit:
                jmp     _exit                                   ; one parameter

CODE32          ends

; init/fini watcom c segments boundaries (XIB..XIE and YIB..YIE)

XIB             segment byte public USE32 'DATA'
                public  __xib_label
__xib_label     label   near
XIB             ends

XIE             segment byte public USE32 'DATA'
                public  __xie_label
__xie_label     label   near
XIE             ends

YIB             segment byte public USE32 'DATA'
                public  __yib_label
__yib_label     label   near
YIB             ends

YIE             segment byte public USE32 'DATA'
                public  __yie_label
__yie_label     label   near
YIE             ends

                end
