
                include inc/segdef.inc
                include inc/qspdata.inc
                include inc/qsvdata.inc

                extrn   _cvio_setmode  :near
                extrn   _cvio_setmodeex:near
                extrn   _cvio_resetmode:near
                extrn   _cvio_savestate:near
                extrn   _cvio_reststate:near
                extrn   _cvio_getmode  :near
                extrn   _cvio_getmodefast:near
                extrn   _cvio_intensity:near
                extrn   _cvio_setshape :near
                extrn   _cvio_getshape :near
                extrn   _cvio_defshape :near
                extrn   _cvio_setcolor :near
                extrn   _cvio_clearscr :near
                extrn   _cvio_setpos   :near
                extrn   _cvio_getpos   :near
                extrn   _cvio_charout  :near
                extrn   _cvio_strout   :near
                extrn   _cvio_writebuf :near
                extrn   _cvio_readbuf  :near
                extrn   _ckey_read     :near
                extrn   _ckey_pressed  :near
                extrn   _ckey_wait     :near
                extrn   _ckey_status   :near
                extrn   _ckey_push     :near

_DATA           segment

                extrn   _mt_exechooks  :mt_proc_cb_s
                extrn   _cvio_ttylines :dword

                public  vh_common
vh_common       label   near                                    ;
                db      'cvio',0,0,0,0                          ;
                dd      0                                       ;
                dd      0                                       ;
                dd      _cvio_savestate                         ;
                dd      _cvio_reststate                         ;
                dd      _cvio_setmode                           ;
                dd      _cvio_setmodeex                         ;
                dd      _cvio_resetmode                         ;
                dd      _cvio_getmode                           ;
                dd      _cvio_getmodefast                       ;
                dd      _cvio_intensity                         ;
                dd      _cvio_setshape                          ;
                dd      _cvio_getshape                          ;
                dd      _cvio_defshape                          ;
                dd      _cvio_setcolor                          ;
                dd      _cvio_clearscr                          ;
                dd      _cvio_setpos                            ;
                dd      _cvio_getpos                            ;
                dd      getlinesptr                             ;
                dd      _cvio_charout                           ;
                dd      _cvio_strout                            ;
                dd      _cvio_writebuf                          ;
                dd      _cvio_readbuf                           ;
                dd      _ckey_read                              ;
                dd      _ckey_pressed                           ;
                dd      _ckey_wait                              ;
                dd      _ckey_status                            ;
vh_common_kpush dd      _ckey_push                              ;

if offset vh_kpush NE vh_common_kpush - vh_common               ; check is this rec equal
                .err                                            ; to vio_handler_s?
endif                                                           ;

_DATA           ends

vhroute         macro   fnname,vhofs                            ;
                public  fnname                                  ;
fnname          label   near                                    ;
                mov     al,offset vhofs                         ;
                jmp     route_main                              ;
                endm                                            ;

_TEXT           segment                                         ;
                assume  cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT      ;

getlinesptr     proc    near                                    ;
                mov     eax,offset _cvio_ttylines               ;
                ret                                             ;
getlinesptr     endp                                            ;

                vhroute <_vio_setmode   >,<vh_setmode  >        ;
                vhroute <_vio_setmodeex >,<vh_setmodeex>        ;
                vhroute <_vio_resetmode >,<vh_resetmode>        ;
                vhroute <_vio_getmode   >,<vh_getmode  >        ;
                vhroute <_vio_getmodefast>,<vh_getmfast>        ;
                vhroute <_vio_intensity >,<vh_intensity>        ;
                vhroute <_vio_setshape  >,<vh_setshape >        ;
                vhroute <_vio_getshape  >,<vh_getshape >        ;
                vhroute <_vio_defshape  >,<vh_defshape >        ;
                vhroute <_vio_setcolor  >,<vh_setcolor >        ;
                vhroute <_vio_clearscr  >,<vh_clearscr >        ;
                vhroute <_vio_setpos    >,<vh_setpos   >        ;
                vhroute <_vio_getpos    >,<vh_getpos   >        ;
                vhroute <_vio_ttylines  >,<vh_ttylines >        ;
                vhroute <_vio_charout   >,<vh_charout  >        ;
                vhroute <_vio_strout    >,<vh_strout   >        ;
                vhroute <_vio_writebuf  >,<vh_writebuf >        ;
                vhroute <_vio_readbuf   >,<vh_readbuf  >        ;
                vhroute <_key_read      >,<vh_kread    >        ;
                vhroute <_key_pressed   >,<vh_kpressed >        ;
                vhroute <_key_wait      >,<vh_kwait    >        ;
                vhroute <_key_status    >,<vh_kstatus  >        ;
                vhroute <_key_push      >,<vh_kpush    >        ;

route_main      label   near                                    ;
                xor     ecx,ecx                                 ;
                movzx   eax,al                                  ;
                or      ecx,_mt_exechooks.mtcb_cvh              ;
                jnz     @@rm_common                             ;
@@rm_default:
                mov     ecx,offset vh_common                    ;
@@rm_common:
                mov     ecx,[ecx][eax]                          ;
                jecxz   @@rm_default                            ;
                jmp     ecx                                     ;

_TEXT           ends                                            ;
                end
