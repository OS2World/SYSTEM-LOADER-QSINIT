;
; QSINIT
; function export table
;
                include inc/qstypes.inc
                include inc/segdef.inc                          ; segments
                include inc/basemac.inc
                include inc/qsinit.inc

                extrn   _exit_pm32      :near
                extrn   _exit_pm32s     :near
                extrn   _hlp_seroutchar :near
                extrn   _hlp_seroutstr  :near
                extrn   _log_printf     :near
                extrn   _hlp_memalloc   :near
                extrn   _hlp_memfree    :near
                extrn   _hlp_memrealloc :near
                extrn   _hlp_memqconst  :near
                extrn   _hlp_memgetsize :near
                extrn   _hlp_fopen      :near
                extrn   _hlp_fread      :near
                extrn   _hlp_fclose     :near
                extrn   _hlp_freadfull  :near
                extrn   _f_mount        :near
                extrn   _f_open         :near
                extrn   _f_read         :near
                extrn   _f_lseek        :near
                extrn   _f_close        :near
                extrn   _f_opendir      :near
                extrn   _f_readdir      :near
                extrn   _f_stat         :near
                extrn   _f_write        :near
                extrn   _f_getfree      :near
                extrn   _f_truncate     :near
                extrn   _f_sync         :near
                extrn   _f_unlink       :near
                extrn   _f_mkdir        :near
                extrn   _f_chmod        :near
                extrn   _f_utime        :near
                extrn   _f_rename       :near
                extrn   _snprintf       :near
                extrn   __snprint       :near
                extrn   _memset         :near
                extrn   _strncmp        :near
                extrn   _strnicmp       :near
                extrn   _strlen         :near
                extrn   _strncpy        :near
                extrn   _strcpy         :near
                extrn   _strchr         :near
                extrn   _memcpy         :near
                extrn   _toupper        :near
                extrn   _memchrnb       :near
                extrn   _memchrw        :near
                extrn   _memchrnw       :near
                extrn   _memchrd        :near
                extrn   _memchrnd       :near
                extrn   _vio_charout    :near
                extrn   _vio_strout     :near
                extrn   _vio_clearscr   :near
                extrn   _vio_setpos     :near
                extrn   _vio_setshape   :near
                extrn   _vio_setcolor   :near
                extrn   _zip_open       :near
                extrn   _zip_close      :near
                extrn   _zip_nextfile   :near
                extrn   _zip_readfile   :near
                extrn   _crc32          :near
                extrn   _zip_isok       :near
                extrn   _zip_unpacked   :near
                extrn   _BootBPB        :byte
                extrn   _hlp_selalloc   :near
                extrn   _hlp_selfree    :near
                extrn   _hlp_selsetup   :near
                extrn   _hlp_selbase    :near
                extrn   _mod_load       :near
                extrn   _mod_unpackobj  :near
                extrn   _mod_getfuncptr :near
                extrn   _mod_exec       :near
                extrn   _mod_free       :near
                extrn   _key_read       :near
                extrn   _key_pressed    :near
                extrn   _exit_restart   :near
                extrn   _hlp_segtoflat  :near
                extrn   _hlp_rmcall     :near
                extrn   _memmove        :near
                extrn   _key_speed      :near
                extrn   _key_status     :near
                extrn   _tolower        :near
                extrn   _memchr         :near
                extrn   _vio_setmode    :near
                extrn   _vio_resetmode  :near
                extrn   _usleep         :near
                extrn   _key_wait       :near
                extrn   _hlp_memreserve :near
                extrn   _hlp_memprint   :near
                extrn   _exit_prepare   :near
                extrn   _mod_context    :near
                extrn   _memsetw        :near
                extrn   _memsetd        :near
                extrn   __longjmp       :near
                extrn   __setjmp        :near
                extrn   _vio_getmode    :near
                extrn   _str2long       :near
                extrn   _tm_counter     :near
                extrn   __itoa          :near
                extrn   __itoa64        :near
                extrn   __utoa          :near
                extrn   __utoa64        :near
                extrn   __prt_common    :near
                extrn   _hlp_seroutinfo :near
                extrn   _int12mem       :near
                extrn   _key_push       :near
                extrn   _vio_getshape   :near
                extrn   _hlp_diskcount  :near
                extrn   _hlp_disksize   :near
                extrn   _hlp_diskread   :near
                extrn   _hlp_diskwrite  :near
                extrn   _hlp_boottype   :near
                extrn   _vio_getpos     :near
                extrn   _aboutstr       :near
                extrn   _hlp_memavail   :near
                extrn   _hlp_int15mem   :near
                extrn   _log_flush      :near
                extrn   _mod_query      :near
                extrn   _exit_poweroff  :near
                extrn   _hlp_getcpuid   :near
                extrn   _sys_errbrk     :near
                extrn   _sto_init       :near
                extrn   _hlp_querybios  :near
                extrn   _hlp_readmsr    :near
                extrn   _hlp_writemsr   :near
                extrn   _hlp_diskmode   :near
                extrn   _f_getlabel     :near
                extrn   _f_setlabel     :near
                extrn   _f_closedir     :near
                extrn   _tm_calibrate   :near
                extrn   _vio_getmodefast:near
                extrn   _key_waithlt    :near
                extrn   _hlp_runcache   :near
                extrn   _hlp_fddline    :near
                extrn   _vio_defshape   :near
                extrn   _key_getspeed   :near
                extrn   _hlp_seroutset  :near
                extrn   _vio_intensity  :near
                extrn   _sys_setcr3     :near
                extrn   _sys_tmirq32    :near
                extrn   _sys_tmirq64    :near
                extrn   _sys_settr      :near
                extrn   _mod_apidirect  :near
                extrn   _hlp_diskadd    :near
                extrn   _hlp_diskremove :near
                extrn   _log_vprintf    :near
                extrn   __I8D           :near
                extrn   __U8D           :near
                extrn   __U8RS          :near
                extrn   __U8LS          :near
                extrn   __I8RS          :near
                extrn   __I8LS          :near
                extrn   __U8M           :near
                extrn   __I8M           :near
                extrn   _wcslen         :near
                extrn   _mod_secondary  :dword
                extrn   _mod_list       :dword
                extrn   _mod_ilist      :dword
                extrn   _minifsd_ptr    :word
                extrn   _IODelay        :word
                extrn   _page0_fptr     :dword
                extrn   _tm_getdate     :near
                extrn   _vio_beepactive :near
                extrn   _tm_setdate     :near
                extrn   _vio_beep       :near
                extrn   _sys_intstate   :near
                extrn   _sys_getint     :near
                extrn   _sys_setint     :near
                extrn   _sys_intgate    :near
                extrn   _sys_seldesc    :near
                extrn   _hlp_hosttype   :near
                extrn   _call64         :near
                extrn   _vio_setmodeex  :near
                extrn   _vio_writebuf   :near
                extrn   _vio_readbuf    :near
                extrn   _memsetq        :near
                extrn   _memchrq        :near
                extrn   _memchrnq       :near
                extrn   _vio_ttylines   :near
                extrn   _hlp_memallocsig:near
                extrn   _exit_reboot    :near
                extrn   _hlp_disksize64 :near
                extrn   _sys_setxcpt64  :near
                extrn   _sys_selquery   :near
                extrn   _hlp_setcpinfo  :near
                extrn   _hlp_diskstruct :near
                extrn   _exit_restirq   :near
                extrn   _hlp_rmcallreg  :near
                extrn   _sys_rmtstat    :near
                extrn   _mt_yield       :near
                extrn   _hlp_tscread    :near
                extrn   _hlp_tscin55ms  :near
                extrn   _exit_inprocess :near
                extrn   _mt_exechooks   :near
                extrn   _mt_swlock      :near
                extrn   _mt_swunlock    :near
                extrn   _mt_safedadd    :near
                extrn   _mt_safedand    :near
                extrn   _mt_safedor     :near
                extrn   _mt_safedxor    :near
                extrn   _mt_cmpxchgd    :near
                extrn   _clock          :near
                extrn   _sys_clock      :near
                extrn   _hlp_blistadd   :near
                extrn   _hlp_blistdel   :near

nextord macro ordinal                                           ; set next ordinal
                dw      ordinal                                 ; number
                dw      0FFFFh                                  ;
endm

next_is_offset macro                                            ;
                dd      0FFFFFFFFh                              ;
endm

next_offsets macro cnt                                          ; must be <16
                db      0 - cnt                                 ;
                db      0FFh                                    ;
                dw      0FFFFh                                  ;
endm

; export table. up to 254 exports.
;
_DATA           segment
;----------------------------------------------------------------
                ALIGN 4                                         ;
                public  _exptable_data                          ;
_exptable_data:
                nextord <1>                                     ;
                dd      offset _exit_pm32                       ;
                dd      offset _hlp_rmcallreg                   ;
                dd      offset _hlp_rmcall                      ;
                dd      offset _int12mem                        ;
                next_is_offset                                  ;
                dd      offset _aboutstr                        ; *
                dd      offset _hlp_querybios                   ;
                dd      offset _exit_pm32s                      ;
                dd      offset _sys_setcr3                      ;
                dd      offset _sys_settr                       ; #9
;----------------------------------------------------------------
;                nextord <10>                                    ;
                next_offsets <2>
                dd      offset _hlp_seroutchar                  ; * #10
                dd      offset _hlp_seroutstr                   ; * not chunked
                dd      offset _log_printf                      ;
                dd      offset _log_flush                       ;
                dd      offset _hlp_seroutinfo                  ;
                dd      offset _sys_errbrk                      ;
                dd      offset _sto_init                        ;
                dd      offset _hlp_seroutset                   ;
                dd      offset _log_vprintf                     ;
                dd      offset _hlp_tscread                     ; #19
;----------------------------------------------------------------
;                nextord <20>                                    ;
                dd      offset _hlp_memalloc                    ; #20
                dd      offset _hlp_memfree                     ;
                dd      offset _hlp_memrealloc                  ;
                dd      offset _hlp_memavail                    ;
                dd      offset _hlp_memqconst                   ;
                dd      0                                       ;
                dd      offset _hlp_memprint                    ;
                dd      offset _hlp_memgetsize                  ;
                dd      offset _hlp_memreserve                  ;
                dd      offset _hlp_memallocsig                 ; #29
;----------------------------------------------------------------
;                nextord <30>                                    ;
                dd      0                                       ; hlp_finit was here, #30
                dd      offset _hlp_fopen                       ;
                dd      offset _hlp_fread                       ;
                dd      offset _hlp_fclose                      ;
                dd      offset _hlp_freadfull                   ;
;----------------------------------------------------------------
                nextord <40>                                    ;
                dd      offset _f_mount                         ;
                dd      offset _f_open                          ;
                dd      offset _f_read                          ;
                dd      offset _f_lseek                         ;
                dd      offset _f_close                         ;
                dd      offset _f_opendir                       ;
                dd      offset _f_readdir                       ;
                dd      offset _f_stat                          ;
                dd      offset _f_write                         ;
                dd      offset _f_getfree                       ;
                dd      offset _f_truncate                      ;
                dd      offset _f_sync                          ;
                dd      offset _f_unlink                        ;
                dd      offset _f_mkdir                         ;
                dd      offset _f_chmod                         ;
                dd      offset _f_utime                         ;
                dd      offset _f_rename                        ;
                dd      offset _f_closedir                      ;
                dd      0                                       ;
                dd      0                                       ;
                dd      0                                       ;
                dd      0                                       ;
                dd      offset _f_getlabel                      ;
                dd      offset _f_setlabel                      ;
                dd      offset _hlp_setcpinfo                   ;
;----------------------------------------------------------------
                nextord <70>                                    ;
                dd      offset _snprintf                        ;
                dd      0                                       ; printf was here
                dd      offset _memset                          ;
                dd      offset _strncmp                         ;
                dd      offset _strnicmp                        ;
                dd      offset _strlen                          ;
                dd      offset _strncpy                         ;
                dd      offset _strcpy                          ;
                dd      offset _strchr                          ;
                dd      offset _memcpy                          ;
                dd      offset _toupper                         ;
                dd      offset _memchrnb                        ;
                dd      offset _memchrw                         ;
                dd      offset _memchrnw                        ;
                dd      offset _memchrd                         ;
                dd      offset _memchrnd                        ;
                dd      offset _memmove                         ;
                dd      offset _tolower                         ;
                dd      offset _memchr                          ;
                dd      offset __snprint                        ;
                dd      offset _usleep                          ;
                dd      offset _memsetw                         ;
                dd      offset _memsetd                         ;
                dd      offset __longjmp                        ;
                dd      offset __setjmp                         ;
                dd      offset _str2long                        ;
                dd      offset __itoa                           ;
                dd      offset __itoa64                         ;
                dd      offset __utoa                           ;
                dd      offset __utoa64                         ; #99
;----------------------------------------------------------------
;                nextord <100>                                   ;
                dd      offset _vio_charout                     ; #100
                dd      offset _vio_strout                      ;
                dd      offset _vio_clearscr                    ;
                dd      offset _vio_setpos                      ;
                dd      offset _vio_setshape                    ;
                dd      offset _vio_setcolor                    ;
                dd      offset _vio_setmode                     ;
                dd      offset _vio_resetmode                   ;
                dd      offset _vio_getmode                     ;
                dd      offset _vio_beep                        ;
;----------------------------------------------------------------
;                nextord <110>                                   ;
                dd      offset _zip_open                        ; #110
                dd      offset _zip_close                       ;
                dd      offset _zip_nextfile                    ;
                dd      offset _zip_readfile                    ;
                dd      offset _crc32                           ;
                dd      offset _zip_isok                        ;
                dd      offset _zip_unpacked                    ;
;----------------------------------------------------------------
                nextord <120>                                   ;
                dd      offset _hlp_int15mem                    ;
                dd      offset _exit_poweroff                   ;
                dd      offset _exit_restirq                    ;
                dd      0                                       ;
                next_is_offset                                  ;
                dd      offset _BootBPB                         ; *
                dd      offset _hlp_runcache                    ;
                dd      offset _exit_reboot                     ;
                dd      offset _exit_restart                    ;
                next_offsets <3>                                ;
                dd      offset _minifsd_ptr                     ; *
                dd      offset _IODelay                         ; *
                dd      offset _page0_fptr                      ; *
                dd      offset _exit_prepare                    ;
                dd      0                                       ;
                dd      offset _hlp_boottype                    ;
                dd      offset _hlp_getcpuid                    ;
                dd      offset _hlp_readmsr                     ;
                dd      offset _hlp_writemsr                    ;
                dd      offset _sys_intstate                    ;
                dd      offset _hlp_hosttype                    ;
                dd      offset _call64                          ; #139
;----------------------------------------------------------------
;                nextord <140>                                   ;
                dd      offset _hlp_selalloc                    ; #140
                dd      offset _hlp_selfree                     ;
                dd      offset _hlp_selsetup                    ;
                dd      offset _hlp_selbase                     ;
                dd      0                                       ;
                dd      offset _hlp_segtoflat                   ;
                dd      offset _sys_getint                      ;
                dd      offset _sys_setint                      ;
                dd      offset _sys_intgate                     ;
                dd      offset _sys_seldesc                     ;
                dd      offset _sys_selquery                    ;
                dd      offset _sys_setxcpt64                   ;
                dd      offset _sys_tmirq32                     ;
                dd      offset _sys_tmirq64                     ;
                dd      offset _sys_rmtstat                     ;
                dd      offset _mt_yield                        ;
                dd      offset _mt_swlock                       ;
                dd      offset _exit_inprocess                  ;
                next_is_offset                                  ;
                dd      offset _mt_exechooks                    ;
                dd      offset _mt_swunlock                     ; #159
;----------------------------------------------------------------
;                nextord <160>                                   ;
                dd      offset _mod_load                        ; #160
                dd      offset _mod_unpackobj                   ;
                dd      offset _mod_getfuncptr                  ;
                dd      offset _mod_exec                        ;
                dd      offset _mod_free                        ;
                next_offsets <3>                                ;
                dd      offset _mod_secondary                   ; *
                dd      offset _mod_list                        ; *
                dd      offset _mod_ilist                       ; *
                nextord <173>                                   ; 168..172 removed
                dd      offset _mod_context                     ;
                dd      offset _mod_query                       ;
                dd      offset _mod_apidirect                   ;
;----------------------------------------------------------------
                nextord <180>                                   ;
                dd      offset _key_read                        ;
                dd      offset _key_pressed                     ;
                dd      offset _key_speed                       ;
                dd      offset _key_status                      ;
                dd      offset _key_wait                        ;
                dd      offset _key_push                        ;
                dd      offset _key_waithlt                     ;
                dd      offset _key_getspeed                    ;
;----------------------------------------------------------------
                nextord <190>                                   ;
                dd      offset _tm_counter                      ;
                dd      offset _tm_getdate                      ;
                dd      offset _vio_beepactive                  ;
                dd      offset _tm_setdate                      ;
                dd      offset _tm_calibrate                    ;
                dd      offset _clock                           ;
                dd      offset _hlp_tscin55ms                   ;
                dd      offset _sys_clock                       ;
;----------------------------------------------------------------
                nextord <200>                                   ;
                dd      offset _vio_getshape                    ;
                dd      offset _vio_getpos                      ;
                dd      offset _vio_getmodefast                 ;
                dd      offset _vio_defshape                    ;
                dd      offset _vio_intensity                   ;
                dd      offset _vio_setmodeex                   ;
                dd      offset _vio_writebuf                    ;
                dd      offset _vio_readbuf                     ;
                next_is_offset                                  ;
                dd      offset _vio_ttylines                    ; *
;----------------------------------------------------------------
                nextord <210>                                   ;
                dd      offset _hlp_diskcount                   ;
                dd      offset _hlp_disksize                    ;
                dd      offset _hlp_diskread                    ;
                dd      offset _hlp_diskwrite                   ;
                dd      offset _hlp_diskmode                    ;
                dd      offset _hlp_fddline                     ;
                dd      offset _hlp_diskadd                     ;
                dd      offset _hlp_diskremove                  ;
                dd      0                                       ; mount/unmount
                dd      0                                       ; was here
                dd      0                                       ;
                dd      offset _hlp_disksize64                  ;
                dd      offset _hlp_diskstruct                  ;
;----------------------------------------------------------------
                nextord <230>                                   ;
                next_offsets <8>                                ;
                dd      offset __U8D                            ; * all __int64
                dd      offset __I8D                            ; * ops are not
                dd      offset __U8RS                           ; * chunked
                dd      offset __U8LS                           ; *
                dd      offset __I8RS                           ; *
                dd      offset __I8LS                           ; *
                dd      offset __U8M                            ; *
                dd      offset __I8M                            ; *
                dd      offset __prt_common                     ;
                dd      offset _wcslen                          ;
                dd      offset _memsetq                         ;
                dd      offset _memchrq                         ;
                dd      offset _memchrnq                        ;
                next_offsets <7>                                ;
                dd      offset _mt_safedadd                     ; *
                dd      offset _mt_safedand                     ; *
                dd      offset _mt_safedor                      ; *
                dd      offset _mt_safedxor                     ; *
                dd      offset _mt_cmpxchgd                     ; *
                dd      offset _hlp_blistadd                    ; *
                dd      offset _hlp_blistdel                    ; *
;----------------------------------------------------------------
                nextord <0>                                     ;
_DATA           ends                                            ;
;----------------------------------------------------------------
                end
