;
; QSINIT
; EFI loader - export table for 64-bit code
;
                include inc/qstypes.inc                         ;
                include inc/segdef.inc                          ;

                extrn   init32call:near
                extrn   _qd_array:byte
                extrn   _ret64:near
                extrn   _mfsd_openname:byte
                extrn   xcpt64entry:near

_DATA           segment

                public  _minifsd_ptr                            ;
; ----> must be in the same order and place,
;       exported to bootos2.exe <--------------------------------
_minifsd_ptr    dd      0                                       ; boot flags
minifsd_flags   dw      0                                       ;
_DiskBufRM_Seg  dw      0                                       ;
; here must be _filetable, but only bootos2.exe can read it, so
; leave it as it is for a while
; ---------------------------------------------------------------

; this table defined in qsexp.h in 64-bit code!
; entry point in BIN_HEADER points directly to it
                public  exp64_tab
                align   4
exp64_tab       label   near
                dd      exp64_tab_end - exp64_tab
                dd      offset init32call
_phmembase      dd      0
_highbase       dd      0
_highlen        dd      0
_logrmbuf       dd      0
                dd      offset _qd_array
_DiskBufPM      dd      0
_gdt_pos        dd      0
_pbin_header    dd      0
_ofs64          dd      0
_sel64          dd      0
                dd      offset _ret64
_flat32cs       dd      0
                dd      offset _mfsd_openname
_ret64offset    dd      0
_cvio_ttylines  dd      0
                dd      offset xcpt64entry
xcptret         dd      0

_logrmpos       dw      0
_qd_fdds        db      0
_qd_hdds        db      0
_gdt_size       dw      0
_gdt_lowest     dw      0
_IODelay        dw      1000
_safeMode       db      0
_reserved_db    db      0
_qs_bootlen     dq      0
_qs_bootstart   dq      0
_qd_bootdisk    dd      0
_countsIn55ms   dq      0
_acpitable      dd      0
_int12msize     dd      0
_smbios         dd      0
_smbios3        dd      0
_mpstab         dd      0
exp64_tab_end   label   near

                public  _phmembase, _highbase
                public  _logrmbuf, _logrmpos, _DiskBufPM, _qd_fdds
                public  _qd_hdds, _gdt_pos, _gdt_size, _gdt_lowest
                public  _pbin_header, _IODelay, _sel64, _ofs64, _smbios
                public  _flat32cs, _ret64offset, _highlen, _safeMode
                public  _cvio_ttylines, xcptret, _qs_bootlen, _qs_bootstart
                public  _qd_bootdisk, _countsIn55ms, _acpitable, _int12msize
                public  _smbios3, _mpstab
_DATA           ends
                end
