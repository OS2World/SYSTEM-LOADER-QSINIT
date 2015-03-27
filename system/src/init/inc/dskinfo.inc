;
; QSINIT
; internal disk info structs
;

;cc #ifndef QSINIT_DISKINFO
;cc #define QSINIT_DISKINFO

;cc #ifdef EFIX64
;cc typedef u32t diskio_func;
;cc #else
;cc typedef u32t (_std *diskio_func)(u32t disk, u64t sector, u32t count, void *data);
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
qd_sectorsize   dw      ?
qd_sectorshift  db      ?
qd_biosdisk     db      ?
qd_mediatype    db      ?         ; 1/2 - floppy, 3 - hdd
qd_flags        db      ?         ; HDDF_*
qs_diskinfo     ends


; include ioint13.h to get access to this area
;cc #ifdef QS_BASE_IOINT13

;cc /** add disk to system HDD list.
;cc     @param  qdi        Disk information struct. All parameters up to
;cc                        qd_biosdisk must be filled.
;cc     @return disk number or -1 on error. */
;cc s32t _std hlp_diskadd(struct qs_diskinfo *qdi);

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
