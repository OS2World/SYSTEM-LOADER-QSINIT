;cc #ifndef QS_BASE_IOINT13
;cc #define QS_BASE_IOINT13

;
; Fixed disk parameter table
;

; note - this is BPB for OS/2 boot, it has a strange size of 0x1F and
;        must be preserved. Common BPB defined in parttab.inc
Disk_BPB        struc
BPB_BytesPerSec dw      ?                 ; physical sector size
BPB_SecPerClus  db      ?                 ; sectors per cluster
BPB_ResSectors  dw      ?                 ; number of reserved sectors
BPB_FATCopies   db      ?                 ; number of FAT copies
BPB_RootEntries dw      ?                 ; number of root dir entries
BPB_TotalSec    dw      ?                 ; total number of sectors
BPB_MediaByte   db      ?                 ; media descriptor byte
BPB_SecPerFAT   dw      ?                 ; number of sectors in one FAT copy
BPB_SecPerTrack dw      ?                 ; sectors per track
BPB_Heads       dw      ?                 ; number of heads
BPB_HiddenSec   dd      ?                 ; number of hidden sectors
BPB_TotalSecBig dd      ?                 ; total sectors (if BPB_TotalSectors = 0)
BPB_BootDisk    db      ?                 ; boot disk
BPB_BootLetter  db      ?                 ; boot drive letter ( 80h+(x-'C') or 0 )
                db      4 dup(?)          ;
Disk_BPB        ends


FDParmTable     struc
FDMaxCyl        dw      ?                       ; Maximum number of cylinders
FDMaxHd         db      ?                       ; Maximum number of heads
FDAUnUse        dw      ?                       ; Unused on an AT
FDWritePComp    dw      ?                       ; Starting write precompensation cyl
FDBUnUse        db      ?                       ; Unused on an AT
FDControl       db      ?                       ; Control byte
FDCUnUse        db      3 dup (?)               ; Unused on an AT
FDLandZon       dw      ?                       ; Landing zone
FDSecTrk        db      ?                       ; Number of sectors/track
FDRes           db      ?                       ; Reserved
FDParmTable     ends

; Rom Bios Drive Parameter Table
Drive_Table     struc
DiskSPECIFY_1   db      ?                       ; Specify bytes
DiskSPECIFY_2   db      ?
MOTOR_TIMEOUT   db      ?                       ; Motor timeout
SECSIZE         db      ?                       ; Sector size
EOT             db      ?                       ; EOT - Should be either 15 or 9
GAPLENGTH       db      ?                       ; Gap length
DTL             db      ?                       ;
FMTGapLength    db      ?                       ; Gap length for format
FMTFillByte     db      ?                       ; Fill byte for format
HeadSTTL        db      ?                       ; Head settle time.
Motor_Start     db      ?                       ; Motor start time
Drive_Table     ends

; packet for int 13h, ah=42h ext. read
ExtReadPacket   struc                           ;
ER_PktSize      db      ?                       ; packet size (10h)
ER_Reserved     db      ?                       ; resrved, 0
ER_SectorCount  dw      ?                       ; number of sectors
ER_BufferAddr   dd      ?                       ; buffer
ER_StartSect    dq      ?                       ; start sector
ER_PhysAddr     dq      ?                       ; flat address (not used)
ExtReadPacket   ends                            ;

; disk parameter table (int 13h, ah=48h)
EDParmTable     struc
ED_BufferSize   dw      ?                       ; buffer size (1Eh)
ED_Flags        dw      ?                       ; information flags
ED_Cylinders    dd      ?                       ; number of cylinders
ED_Heads        dd      ?                       ; number of heads
ED_SectOnTrack  dd      ?                       ; number of sectors per track
ED_Sectors      dq      ?                       ; total number of sectors
ED_SectorSize   dw      ?                       ; bytes per sector
ED_EDDTable     dd      ?                       ; EDD table address
EDParmTable     ends

; enhanced disk drive parameter table
EDDParmTable    struc
EDD_IoBase      dw      ?                       ; physical I/O port
EDD_IoCtrl      dw      ?                       ; control port
EDD_Flags       db      ?                       ; drive flags
EDD_Reserved    db      ?                       ; proprientary info
EDD_Irq         db      ?                       ; irq (4 low bits)
EDD_MultiSec    db      ?                       ; sectors for milti-sec transfer
EDD_DMAInfo     db      ?                       ; DMA control
EDD_PIOInfo     db      ?                       ; PIO control
EDD_Options     dw      ?                       ; drive options
EDD_Reserved2   dw      ?                       ; reserved
EDD_Revision    db      ?                       ; extension revision
EDD_Checksum    db      ?                       ; check sum
EDDParmTable    ends

; Rates for the various media in the drives.
RATE500         equ     0                       ; 500 Kb/sec
RATE300         equ     1                       ; 300 Kb/sec
RATE250         equ     2                       ; 250 Kb/sec
RATE1K          equ     3                       ; 1000 Kb/sec
RATEMAX         equ     RATE250                 ; Maximum rate index that we support

DelayLoad       equ     35                      ; 35 milliseconds to load head

; Locations in ROM data area of fixed disk parameter tables on an AT
FDTAB_0         equ     104h                    ; First fixed disk  - Int 41H vector
FDTAB_1         equ     118h                    ; Second fixed disk - Int 46H vector

; Status codes
FD_SDIO         =       01000000b               ; Transfer direction (0 -> controller)
FD_SRQM         =       10000000b               ; Controller ready for next data

; Commands to floppy disk controller
FD_CRESET       =       007h                    ; Recalibrate drive
FD_CSENSE       =       008h                    ; Sense interrupt status
FD_CSEEK        =       00Fh                    ; Seek to another track
FD_CREAD        =       046h                    ; MFM read, skip deleted data
FD_CWRITE       =       045h                    ; MFM write, don`t skip deleted data
FD_CSPEC        =       003h                    ; Special - step rate, head load/unload
FD_CFORMAT      =       04Dh                    ; MFM format track

; Status bits
FX_BUSY         =       10000000b               ; Status port busy bit
FX_READY        =       01000000b               ; Status port ready bit
FX_DRQ          =       00001000b               ; Data Request bit
FX_ERROR        =       00000001b               ; Error status

; Floppy drive I/O ports
FD_PSEL         =       03F2h                   ; Controls drive select and motors
FD_PDAT         =       03F5h                   ; Data transfer to/from controller
FD_PSTAT        =       03F4h                   ; Controller status
FD_PCTRL        =       03F7h                   ; Diskette control register

PDMA            =       0                       ; Base of ports for DMA control
PDMAX           =       7Fh                     ; Address extension regs for DMA
DMA_MSKON       =       04h                     ; Mask on = 1 in bit 2
DMA_MSKOFF      =       00h                     ; (Mask off = 0 in bit 2)
FD_DMA          =       2                       ; floppy disk DMA channel

;  DMA controller fields: block mode, addr increment, no autoinitialize
DMA_READ        =       44h                     ; DMA read command
DMA_WRITE       =       48h                     ; DMA write command
DMA_VERIFY      =       40h                     ; DMA verify command
DORmask         =       0Ch                     ; Not reset, enable DMA & interrupt

;cc #endif // QS_BASE_IOINT13
