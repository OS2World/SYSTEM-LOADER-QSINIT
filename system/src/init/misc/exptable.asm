;
; QSINIT
; function export table
;
                include inc/qstypes.inc
                include inc/segdef.inc                          ; segments
                include inc/basemac.inc
                include inc/qsinit.inc

CODE32          segment
                extrn   _exit_pm32      :near
                extrn   _exit_pm32s     :near
                extrn   rmcall32        :near
                extrn   _hlp_seroutchar :near
                extrn   _hlp_seroutstr  :near
                extrn   _log_printf     :near
                extrn   _hlp_memalloc   :near
                extrn   _hlp_memfree    :near
                extrn   _hlp_memrealloc :near
                extrn   _hlp_memqconst  :near
                extrn   _hlp_memgetsize :near
                extrn   _hlp_finit      :near
                extrn   _hlp_fopen      :near
                extrn   _hlp_fread      :near
                extrn   _hlp_fclose     :near
                extrn   _hlp_freadfull  :near
                extrn   _hlp_fdone      :near
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
                extrn   _unpack_zip     :near
                extrn   _physmem        :word
                extrn   _physmem_entries:word
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
                extrn   _mod_listadd    :near
                extrn   _mod_listdel    :near
                extrn   _mod_listlink   :near
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
                extrn   _mod_listflags  :near
                extrn   _usleep         :near
                extrn   _key_wait       :near
                extrn   _hlp_memreserve :near
                extrn   _hlp_memprint   :near
                extrn   _exit_prepare   :near
                extrn   _exit_handler   :near
                extrn   _mod_context    :near
                extrn   _hlp_chdir      :near
                extrn   _hlp_chdisk     :near
                extrn   _hlp_curdisk    :near
                extrn   _hlp_curdir     :near
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
                extrn   _int15mem       :near
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
                extrn   _sys_settr      :near
                extrn   _mod_apidirect  :near
                extrn   _mod_findexport :near
                extrn   _hlp_diskadd    :near
                extrn   _hlp_diskremove :near
                extrn   _log_vprintf    :near
                extrn   _NextBeepEnd    :near
                extrn   __I8D           :near
                extrn   __U8D           :near
                extrn   __U8RS          :near
                extrn   __U8LS          :near
                extrn   __I8RS          :near
                extrn   __I8LS          :near
                extrn   __U8M           :near
                extrn   __I8M           :near
                extrn   _wcslen         :near

CODE32          ends
DATA32          segment
                extrn   _mod_secondary :dword
                extrn   _mod_list      :dword
                extrn   _mod_ilist     :dword
DATA32          ends
BSS             segment
                extrn   _minifsd_ptr   :word
                extrn   _IODelay       :word                    ;
BSS             ends

nextord macro ordinal                                           ; set next ordinal
                db      ordinal                                 ; number
                db      0FFh                                    ;
endm

next_is_offset macro                                            ;
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
                dw      offset _exit_pm32                       ;
                dw      offset rmcall32                         ;
                dw      offset _hlp_rmcall                      ;
                dw      offset _int12mem                        ;
                next_is_offset                                  ;
                dw      offset _aboutstr                        ;
                dw      offset _hlp_querybios                   ;
                dw      offset _exit_pm32s                      ;
                dw      offset _sys_setcr3                      ;
                dw      offset _sys_settr                       ; #9
;----------------------------------------------------------------
;                nextord <10>                                    ;
                dw      offset _hlp_seroutchar                  ; #10
                dw      offset _hlp_seroutstr                   ;
                dw      offset _log_printf                      ;
                dw      offset _log_flush                       ;
                dw      offset _hlp_seroutinfo                  ;
                dw      offset _sys_errbrk                      ;
                dw      offset _sto_init                        ;
                dw      offset _hlp_seroutset                   ;
                dw      offset _log_vprintf                     ;
;----------------------------------------------------------------
                nextord <20>                                    ;
                dw      offset _hlp_memalloc                    ;
                dw      offset _hlp_memfree                     ;
                dw      offset _hlp_memrealloc                  ;
                dw      offset _hlp_memavail                    ;
                dw      offset _hlp_memqconst                   ;
                dw      0                                       ;
                dw      offset _hlp_memprint                    ;
                dw      offset _hlp_memgetsize                  ;
                dw      offset _hlp_memreserve                  ;
;----------------------------------------------------------------
                nextord <30>                                    ;
                dw      offset _hlp_finit                       ;
                dw      offset _hlp_fopen                       ;
                dw      offset _hlp_fread                       ;
                dw      offset _hlp_fclose                      ;
                dw      offset _hlp_freadfull                   ;
                dw      offset _hlp_fdone                       ;
;----------------------------------------------------------------
                nextord <40>                                    ;
                dw      offset _f_mount                         ;
                dw      offset _f_open                          ;
                dw      offset _f_read                          ;
                dw      offset _f_lseek                         ;
                dw      offset _f_close                         ;
                dw      offset _f_opendir                       ;
                dw      offset _f_readdir                       ;
                dw      offset _f_stat                          ;
                dw      offset _f_write                         ;
                dw      offset _f_getfree                       ;
                dw      offset _f_truncate                      ;
                dw      offset _f_sync                          ;
                dw      offset _f_unlink                        ;
                dw      offset _f_mkdir                         ;
                dw      offset _f_chmod                         ;
                dw      offset _f_utime                         ;
                dw      offset _f_rename                        ;
                dw      offset _f_closedir                      ;
                dw      offset _hlp_chdir                       ;
                dw      offset _hlp_chdisk                      ;
                dw      offset _hlp_curdisk                     ;
                dw      offset _hlp_curdir                      ;
                dw      offset _f_getlabel                      ;
                dw      offset _f_setlabel                      ;
;----------------------------------------------------------------
                nextord <70>                                    ;
                dw      offset _snprintf                        ;
                dw      0                                       ;
                dw      offset _memset                          ;
                dw      offset _strncmp                         ;
                dw      offset _strnicmp                        ;
                dw      offset _strlen                          ;
                dw      offset _strncpy                         ;
                dw      offset _strcpy                          ;
                dw      offset _strchr                          ;
                dw      offset _memcpy                          ;
                dw      offset _toupper                         ;
                dw      offset _memchrnb                        ;
                dw      offset _memchrw                         ;
                dw      offset _memchrnw                        ;
                dw      offset _memchrd                         ;
                dw      offset _memchrnd                        ;
                dw      offset _memmove                         ;
                dw      offset _tolower                         ;
                dw      offset _memchr                          ;
                dw      offset __snprint                        ;
                dw      offset _usleep                          ;
                dw      offset _memsetw                         ;
                dw      offset _memsetd                         ;
                dw      offset __longjmp                        ;
                dw      offset __setjmp                         ;
                dw      offset _str2long                        ;
                dw      offset __itoa                           ;
                dw      offset __itoa64                         ;
                dw      offset __utoa                           ;
                dw      offset __utoa64                         ; #99
;----------------------------------------------------------------
;                nextord <100>                                   ;
                dw      offset _vio_charout                     ; #100
                dw      offset _vio_strout                      ;
                dw      offset _vio_clearscr                    ;
                dw      offset _vio_setpos                      ;
                dw      offset _vio_setshape                    ;
                dw      offset _vio_setcolor                    ;
                dw      offset _vio_setmode                     ;
                dw      offset _vio_resetmode                   ;
                dw      offset _vio_getmode                     ;
                dw      0                                       ;
;----------------------------------------------------------------
;                nextord <110>                                   ;
                dw      offset _zip_open                        ; #110
                dw      offset _zip_close                       ;
                dw      offset _zip_nextfile                    ;
                dw      offset _zip_readfile                    ;
                dw      offset _crc32                           ;
                dw      offset _zip_isok                        ;
                dw      offset _zip_unpacked                    ;
                dw      offset _unpack_zip                      ;
;----------------------------------------------------------------
                nextord <120>                                   ;
                dw      offset _int15mem                        ;
                dw      offset _exit_poweroff                   ;
                dw      offset _physmem                         ;
                dw      offset _physmem_entries                 ;
                next_is_offset                                  ;
                dw      offset _BootBPB                         ;
                dw      offset _hlp_runcache                    ;
                dw      0                                       ;
                dw      offset _exit_restart                    ;
                next_is_offset                                  ;
                dw      offset _minifsd_ptr                     ;
                next_is_offset                                  ;
                dw      offset _IODelay                         ;
                next_is_offset                                  ;
                dw      offset _NextBeepEnd                     ;
                dw      offset _exit_prepare                    ;
                dw      offset _exit_handler                    ;
                dw      offset _hlp_boottype                    ;
                dw      offset _hlp_getcpuid                    ;
                dw      offset _hlp_readmsr                     ;
                dw      offset _hlp_writemsr                    ;
;----------------------------------------------------------------
                nextord <140>                                   ;
                dw      offset _hlp_selalloc                    ;
                dw      offset _hlp_selfree                     ;
                dw      offset _hlp_selsetup                    ;
                dw      offset _hlp_selbase                     ;
                dw      0                                       ;
                dw      offset _hlp_segtoflat                   ;
;----------------------------------------------------------------
                nextord <160>                                   ;
                dw      offset _mod_load                        ;
                dw      offset _mod_unpackobj                   ;
                dw      offset _mod_getfuncptr                  ;
                dw      offset _mod_exec                        ;
                dw      offset _mod_free                        ;
                next_is_offset                                  ;
                dw      offset _mod_secondary                   ;
                next_is_offset                                  ;
                dw      offset _mod_list                        ;
                next_is_offset                                  ;
                dw      offset _mod_ilist                       ;
                dw      offset _mod_listadd                     ;
                dw      offset _mod_listdel                     ;
                dw      offset _mod_listlink                    ;
                dw      0                                       ;
                dw      offset _mod_listflags                   ;
                dw      offset _mod_context                     ;
                dw      offset _mod_query                       ;
                dw      offset _mod_apidirect                   ;
                dw      offset _mod_findexport                  ;
;----------------------------------------------------------------
                nextord <180>                                   ;
                dw      offset _key_read                        ;
                dw      offset _key_pressed                     ;
                dw      offset _key_speed                       ;
                dw      offset _key_status                      ;
                dw      offset _key_wait                        ;
                dw      offset _key_push                        ;
                dw      offset _key_waithlt                     ;
                dw      offset _key_getspeed                    ;
;----------------------------------------------------------------
                nextord <190>                                   ;
                dw      offset _tm_counter                      ;
                dw      0                                       ;
                dw      0                                       ;
                dw      0                                       ;
                dw      offset _tm_calibrate                    ;
;----------------------------------------------------------------
                nextord <200>                                   ;
                dw      offset _vio_getshape                    ;
                dw      offset _vio_getpos                      ;
                dw      offset _vio_getmodefast                 ;
                dw      offset _vio_defshape                    ;
                dw      offset _vio_intensity                   ;
;----------------------------------------------------------------
                nextord <210>                                   ;
                dw      offset _hlp_diskcount                   ;
                dw      offset _hlp_disksize                    ;
                dw      offset _hlp_diskread                    ;
                dw      offset _hlp_diskwrite                   ;
                dw      offset _hlp_diskmode                    ;
                dw      offset _hlp_fddline                     ;
                dw      offset _hlp_diskadd                     ;
                dw      offset _hlp_diskremove                  ;
;----------------------------------------------------------------
                nextord <230>                                   ;
                dw      offset __U8D                            ;
                dw      offset __I8D                            ;
                dw      offset __U8RS                           ;
                dw      offset __U8LS                           ;
                dw      offset __I8RS                           ;
                dw      offset __I8LS                           ;
                dw      offset __U8M                            ;
                dw      offset __I8M                            ;
                dw      offset __prt_common                     ;
                dw      offset _wcslen                          ;
;----------------------------------------------------------------
                nextord <0>                                     ;
_DATA           ends                                            ;
;----------------------------------------------------------------
                end
