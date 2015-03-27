;
; QSINIT
; EFI loader - misc functions
;
                include inc/qstypes.inc                         ;
                include inc/segdef.inc                          ;
                include inc/efnlist.inc 

                extrn   _call64:near
                extrn   _call64l:near

_DATA           segment
_DATA           ends

_TEXT           segment
                assume  cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT

;-----------------------------------------------------------------------------
; u32t _std tm_getdate(void);
; return time/date in dos format. flag CF is set for 1 second overflow.
; 64bit part returns 33 bits, just shift it here to make 32 + CF.
                public  _tm_getdate
_tm_getdate     proc    near                                    ;
                push    0                                       ;
                push    0                                       ;
                push    EFN_TMGETDATE                           ;
                call    _call64l                                ;
                add     esp,12                                  ;
                shrd    eax,edx,1                               ;
                ret                                             ;
_tm_getdate     endp

_TEXT           ends
                end
