;
; QSINIT OS/2 "ramdisk" basedev
; driver specific code
;
                include segdef.inc

DEV_CHAR_DEV    equ     8000h
DEV_30          equ     0800h
DEVLEV_3        equ     0180h

DEV_IOCTL2      equ     0001h                                   ; for CMDShutdown
DEV_16MB        equ     0002h                                   ;
DEV_ADAPTER_DD  equ     0008h                                   ;
DEV_INITCOMPLETE equ    0010h                                   ;

MAX_IO_USED     equ     16

                public  _ProtCS, _ProtDS
                extrn   _strategy:near
                extrn   DOS32FLATDS:ABS

DDHEADER        segment
                dd      -1
                dw      DEV_CHAR_DEV or DEVLEV_3 or DEV_30
                dw      offset strategy_stub                    ; strategy entry point
                dw      0                                       ; IDC entry point
                db      'HD4DISK$'                              ; device driver name
_ProtCS         dw      0                                       ; prot CS
_ProtDS         dw      0                                       ; prot DS
                dw      0,0
                dd      DEV_ADAPTER_DD or DEV_16MB or DEV_INITCOMPLETE or DEV_IOCTL2
DDHEADER        ends

CODEEND         segment
CodeSegEnd      label   near
CODEEND         ends

DATAEND         segment
DataSegEnd      label   near
DATAEND         ends

_DATA           segment
                public  _CodeSegLen, _DataSegLen, _FlatDS, _pIORBHead
_CodeSegLen     dw      offset CodeSegEnd
_DataSegLen     dw      offset DataSegEnd
_FlatDS         dw      DOS32FLATDS
_pIORBHead      dd      0
_DATA           ends

_TEXT           segment

strategy_stub   proc    far
                pushad
                push    ds
                push    es
                push    bx
                call    _strategy
                pop     ds
                popad
                ret
strategy_stub   endp


                public  _put_IORB
_put_IORB       proc    near
                push    bp
                mov     bp,sp
                push    es
                push    di
                les     di,[bp+4]
                mov     ecx,[bp+4]
@@putIORB_loop:
                mov     eax,_pIORBHead
                mov     es:[di+18h],eax
           lock cmpxchg _pIORBHead,ecx
                jnz     @@putIORB_loop

                pop     di
                pop     es
                pop     bp
                ret     4
_put_IORB       endp


                public  _notify_hook
_notify_hook    proc    far
                pushad
                push    ds
                push    es
                xor     eax,eax
                xchg    _pIORBHead,eax
                or      eax,eax
                jz      @@nh_exit
                mov     bp,sp
                push    eax
                push    eax
@@nh_loop:
                les     di,[bp-8]
                mov     eax,es:[di+18h]
                mov     [bp-4],eax
                call    far ptr es:[di+1Ch]

                mov     eax,[bp-4]
                mov     [bp-8],eax
                or      eax,eax
                jnz     @@nh_loop
                mov     sp,bp
@@nh_exit:
                pop     es
                pop     ds
                popad
		        ret
_notify_hook    endp


                public  _memcpy                                 ;
; void memcpy(void far *dst, const void far *src, u16t length);
_memcpy         proc    near                                    ;
@@dst           =  2                                            ;
@@src           =  6                                            ;
@@length        = 10                                            ;
                movzx   esp,sp                                  ;
                push    cx                                      ;
                mov     cx,[esp+2+@@length]                     ;
                jcxz    @@cpy0                                  ;
                push    di                                      ;
                push    es                                      ;
                les     di,[esp+6+@@dst]                        ;
                push    si                                      ;
                push    ds                                      ;
                lds     si,[esp+10+@@src]                       ;
                cld                                             ;
                push    cx                                      ;
                shr     cx,2                                    ;
                jz      @@cpyb                                  ;
            rep movsd                                           ;
@@cpyb:
                pop     cx                                      ;
                and     cx,3                                    ;
            rep movsb                                           ;
                pop     ds                                      ;
                pop     si                                      ;
                pop     es                                      ;
                pop     di                                      ;
@@cpy0:
                pop     cx                                      ;
                ret     10                                      ;
_memcpy         endp
_TEXT           ends

                end
