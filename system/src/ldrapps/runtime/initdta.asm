;
; QSINIT "start" module
; init/fini segments
;
                .486p

; init/fini watcom c segments boundaries (XIB..XIE and YIB..YIE)
                
XIB             segment word public USE32 'DATA'
                public  __xib_label
__xib_label     label   near
XIB             ends

XI              segment word public USE32 'DATA'
XI              ends

XIE             segment word public USE32 'DATA'
                public  __xie_label
__xie_label     label   near
XIE             ends

YIB             segment word public USE32 'DATA'
                public  __yib_label
__yib_label     label   near
YIB             ends

YI              segment word public USE32 'DATA'
YI              ends

YIE             segment word public USE32 'DATA'
                public  __yie_label
__yie_label     label   near
YIE             ends

DGROUP          GROUP   XIB,XI,XIE,YIB,YI,YIE

                end
