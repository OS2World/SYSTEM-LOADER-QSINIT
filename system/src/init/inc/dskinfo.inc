;
; QSINIT
; internal disk info structs
;

;cc #ifndef QSINIT_DISKINFO
;cc #define QSINIT_DISKINFO

;cc #ifdef EFIX64
;cc typedef u32t diskio_func;
;cc typedef u32t extptr_type;
;cc #else
;cc typedef u32t (_std *diskio_func)(u32t disk, u64t sector, u32t count, void *data);
;cc typedef void *extptr_type;
;cc #endif

qs_diskinfo     struc
qd_sectors      dq      ?
qd_cyls         dd      ?
qd_heads        dw      ?         ; ext. info value, used in api
qd_spt          dw      ?         ;
qd_chsheads     dw      ?         ; ah=08h value, used in actual CHS i/o
qd_chsspt       dw      ?         ;
;ct diskio_func
qd_extread      dd      ?
;ct diskio_func
qd_extwrite     dd      ?
;ct extptr_type
qd_extptr       dd      ?         ; MUST be 0 or qs_extdisk compatible pointer
qd_sectorsize   dw      ?
qd_sectorshift  db      ?
qd_biosdisk     db      ?
qd_mediatype    db      ?         ; 1/2 - floppy, 3 - hdd
qd_flags        db      ?         ; HDDF_*
qs_diskinfo     ends

;cc #ifndef EFIX64
;cc /** add disk to system HDD list.
;cc     @param  qdi        Disk information struct. All parameters up to
;cc                        qd_biosdisk must be filled.
;cc     @return disk number or -1 on error. */
;cc s32t _std hlp_diskadd(struct qs_diskinfo *qdi);

;cc /** query/update internal disk info.
;cc     Function is for internal use only!
;cc     @param  disk       Disk number.
;cc     @param  qdi        Disk information struct, can be 0 to query only.
;cc     @return current disk info structure or 0 if one of args is invalid */
;cc struct qs_diskinfo *_std hlp_diskstruct(u32t disk, struct qs_diskinfo *qdi);

;cc /** remove disk, added by hlp_diskadd call.
;cc     @param  disk       Disk number.
;cc     @return success flag (1/0) */
;cc int  _std hlp_diskremove(u32t disk);
;cc #endif

HDDF_LBAON      equ     1                    ;//< Use LBA access (up to 2Tb)
HDDF_PRESENT    equ     2                    ;//< Device present
HDDF_LBAPRESENT equ     4                    ;//< LBA possible on device
HDDF_HOTSWAP    equ     8                    ;//< emulated device

;cc #endif // QSINIT_DISKINFO
