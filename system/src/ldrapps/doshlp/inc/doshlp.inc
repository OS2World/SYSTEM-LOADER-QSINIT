;
; QSINIT
; doshlp code - common defines
;
;cc #ifndef QS_DOSHLP_HEADER
;cc #define QS_DOSHLP_HEADER

DOSHLP_CODESEL         =  100h
DOSHLP_DATASEL         =  108h
DOSHLP_SEG             =  100h
BOOTHLP_CODESEL        =  110h
BOOTHLP_DATASEL        =  118h
ROM_DATASEG            =   40h

DOSHLP_SIGN            = 5351h

DOSHLPVERSION          =   11

INT13E_TABLESIZE       =   16                                   ; number of ext.info entries

FIXUP_STORE_TABLE_SIZE =   30h
MSR_SETUP_TABLE_SIZE   =   20h

MPDATAFLAT             = 0FF800000h                             ; MPDATA FLAT address

EXPDATA_SIGN           = 74645351h                              ; QSdt

; info struct for bootos2, it must fill all parameters, marked with ">".
; note: signature field added for hd4disk only, was no way before to
; check loader type (QS or OS/4) in loaded OS/2 system.
ExportFixupData struc
InfoSign        dd      ?                                       ; signature
Init1Ofs        dw      ?                                       ; offset of init1 call
Init2Ofs        dw      ?                                       ; offset of init2 call
DumpSeg         dw      ?                                       ; > os2dump segment
DisPartOfs      dw      ?                                       ; discardable part(boothlp) start here
DosHlpTableOfs  dw      ?                                       ; offset of doshlp table
BootDisk        dw      ?                                       ; > offset of boot disk var
DosHlpSize      dw      ?                                       ; doshlp size (without 512 buffer at the end!!!)
LowMem          dw      ?                                       ; > memory size info
HighMem         dw      ?                                       ; >
ExtendMem       dd      ?                                       ; >
xBDALen         dw      ?                                       ; EBDA data size
Flags           dw      ?                                       ; exported flags
IODelay         dw      ?                                       ; > IODelay value
Io32FixupOfs    dw      ?                                       ; > kernel IODelay fixups
Io32FixupCnt    dw      ?                                       ; >
BootBPBOfs      dw      ?                                       ; > offset of BootBPB
MsrTableOfs     dw      ?                                       ; > offset of MsrSetupTable
MsrTableCnt     dw      ?                                       ; # of MsrSetupTable entries
LogBufLock      db      ?                                       ; log access mutex
KernelVersion   db      ?                                       ; > version: 30,40,45 or 0 (init2 only)
LogBufPAddr     dd      ?                                       ; > log physical address
LogBufVAddr     dd      ?                                       ; log FLAT address, available later
LogBufSize      dd      ?                                       ; > log size in 64kb
LogBufWrite     dd      ?                                       ; log write pos
LogBufRead      dd      ?                                       ; log read pos
LAddrDelta      dd      ?                                       ; > addr delta for non-paging mode
ResMsgOfs       dw      ?                                       ; > res msg ofs in doshlp
DisMsgOfs       dw      ?                                       ; > dis msg ofs in boothlp
DisPartSeg      dw      ?                                       ; > boothlp segment (9xxx)
DisPartLen      dw      ?                                       ; > boothlp full length (with arenas, so aligned to page)
OS2Init         dd      ?                                       ; > OS2 kernel entry point
Buf32k          dw      ?                                       ; > free buffer (init1 only!!!)
BaudRate        dd      ?                                       ; > dbport baud rate
PCICount        dw      ?                                       ; > number of entries in PCI arrays
PCIBusList      dw      ?                                       ; > list of bus/slot/func entries
PCIClassList    dw      ?                                       ; > list of class info entries
PCIVendorList   dw      ?                                       ; > list of vendor entries
HD4Page         dd      ?                                       ; > start of hd4disk (# of phys.page)
DPOfs           dw      ?                                       ; offset of dp table
EDTabOfs        dw      ?                                       ; offset of ed table
EDDTabOfs       dw      ?                                       ; offset of edd table
DFTabOfs        dw      ?                                       ; offset of disk flags
Pswcode         dw      ?                                       ; offset of swcode variable
Ppaeppd         dw      ?                                       ; offset of paeppd variable
ExportFixupData ends

; ExportFixupData.Flags bits
EXPF_PCI        equ     1                                       ; PCI present
EXPF_DHINIT     equ     2                                       ; DHInit was done
EXPF_DISCARDED  equ     4                                       ; Boot part was discarded
EXPF_TWODSKBOOT equ     8                                       ; Two disk boot
EXPF_FULLCABLE  equ    10h                                      ; COM port full cable option active
EXPF_COMINITED  equ    20h                                      ; COM port inited at least once

KernelAccess    struc                                           ;
; AllocGDT call:                                                ;
;   es:di = pointer to array to store selectors                 ;
;   cx    = number of selectors requested                       ;
AllocGDT        dd      ?                                       ;
; PhysToGDT call:                                               ;
;   eax   = physical address                                    ;
;   ecx   = segment size, not limit, <= 64Kb                    ;
;   dh    = type:                                               ;
;             0: 16-bit readable code                           ;
;             1: 16-bit writable data                           ;
;             3: 16-bit readable IOPL code                      ;
;             4: 16-bit writable IOPL data                      ;
;             5: 16-bit readable ring 0 code                    ;
;             6: 16-bit writable ring 0 data                    ;
;           80h: add to any of the above for 32 bit addressing  ;
;   si    = selector for setup                                  ;
PhysToGDT       dd      ?                                       ;
GotoReal        dd      ?                                       ;
GotoProt        dd      ?                                       ;
SetSSBase       dd      ?                                       ;
IRQRouter       dd      ?                                       ;
VirtToLin       dd      ?                                       ;
IDTSel          dw      ?                                       ;
NMICount        dd      ?                                       ;
IOCTLWorker     dd      ?                                       ;
SetSSBase1      dd      ?                                       ;
KernelAccess    ends                                            ;

MSG_SYSINIT_BANNER              equ     1721                    ;
MSG_NMI                         equ     1944                    ;
MSG_SYSINIT_BOOT_ERROR          equ     2025                    ;
MSG_SYSINIT_INSER_DK            equ     2027                    ;
MSG_SYSINIT_SYS_STOPPED         equ     2065                    ;

DHVA_ACCESS_READ                equ     0                       ;
DHVA_ACCESS_WRITE               equ     1                       ;
KPG_RING0_CODE                  equ     5                       ;
KPG_RING0_DATA                  equ     6                       ;

ERROR_I24_WRITE_PROTECT         equ     0                       ;
ERROR_I24_BAD_UNIT              equ     1                       ;
ERROR_I24_NOT_READY             equ     2                       ;
ERROR_I24_BAD_COMMAND           equ     3                       ;
ERROR_I24_CRC                   equ     4                       ;
ERROR_I24_BAD_LENGTH            equ     5                       ;
ERROR_I24_SEEK                  equ     6                       ;
ERROR_I24_NOT_DOS_DISK          equ     7                       ;
ERROR_I24_SECTOR_NOT_FOUND      equ     8                       ;
ERROR_I24_OUT_OF_PAPER          equ     9                       ;
ERROR_I24_WRITE_FAULT           equ     10                      ;
ERROR_I24_READ_FAULT            equ     11                      ;
ERROR_I24_GEN_FAILURE           equ     12                      ;
ERROR_I24_DISK_CHANGE           equ     13                      ;
ERROR_I24_WRONG_DISK            equ     15                      ;
ERROR_I24_UNCERTAIN_MEDIA       equ     16                      ;
ERROR_I24_CHAR_CALL_INTERRUPTED equ     17                      ;
ERROR_I24_NO_MONITOR_SUPPORT    equ     18                      ;
ERROR_I24_INVALID_PARAMETER     equ     19                      ;
ERROR_I24_DEVICE_IN_USE         equ     20                      ;
ERROR_I24_QUIET_INIT_FAIL       equ     21                      ;

ISF_STACK_PAD   equ     8                                       ;

irqi            struc
irqi_pdirqHead  dd      ?                                       ; head of shared DD chain (0 = not use)
irqi_usIRQNum   db      ?                                       ; IRQ number
irqi_vector     db      ?                                       ; Trap number
irqi_usFlags    dw      ?                                       ; IRQ Flags
irqi_PSDHdl     dd      ?                                       ;
irqi_spur_count db      ?                                       ;
pad1            db      ?                                       ; reserved
pad2            dw      ?                                       ;
irqi            ends

irqi3           struc
irqi3_pdirqHead dd      ?                                       ; Head of shared DD chain (0 = not set)
irqi3_usIRQNum  dw      ?                                       ; IRQ number
irqi3_usFlags   dw      ?                                       ; IRQ Flags
irqi3           ends

IRQF_FSHARED    equ     1                                       ;
IRQF_FSYS       equ     2                                       ;
IRQF_FSHARING   equ     4                                       ;
IRQF_FNPX       equ     8                                       ;
IRQF_FVDM       equ    10h                                      ;
IRQF_FIPI       equ    20h                                      ;

InterruptTable  struc                                           ;
ITCount         db      ?                                       ;
ITHVector       db      ?                                       ;
ITMappedVector  db      ?                                       ;
InterruptTable  ends                                            ;

IRQR_SIZE       equ     8                                       ; IRQ router size

PCI_FNNUMBER    equ     0Bh                                     ; PCI access oemhlp func

CMDInitBase     equ     27                                      ;

krnl_start_info struc                                           ;
ki_pArenaInfo   dd      ?                                       ;
ki_pBPB         dd      ?                                       ;
ki_Drive        db      ?                                       ;
ki_BootFlags    db      ?                                       ;
ki_DosHlpSeg    dw      ?                                       ;
ki_DosHlpLen    dw      ?                                       ;
ki_DosHlpAddr   dd      ?                                       ;
ki_reserved     dw      ?                                       ;
ki_OEMDDStrat   dd      ?                                       ;
krnl_start_info ends                                            ;

LockLogBuffer macro lockvar
local LockReady, LockLoop, WaitLoop
                push    ax
LockLoop:
                mov     ax, 100h
           lock cmpxchg lockvar, ah
                jz      LockReady
WaitLoop:
                dec     ax
                jnz     WaitLoop
                jmp     LockLoop
LockReady:
                pop     ax
endm

UnlockLogBuffer macro lockvar
                push    ax
                xor     ax,ax
                xchg    lockvar, al
                pop     ax
endm

Dev_Hlp         macro  function
                mov     dl, DevHlp_&function
                call    [DEVHLP]
endm

msr_setup_entry struc                                           ;
mse_regnum      dw      ?                                       ;
mse_ddlow       dd      ?                                       ;
mse_ddhigh      dd      ?                                       ;
msr_setup_entry ends                                            ;

HD4DiskInfo     struc
h4_sign         dd      ?                                       ;
h4_version      dd      ?                                       ;
h4_cyls         dd      ?                                       ;
h4_heads        dd      ?                                       ;
h4_spt          dd      ?                                       ;
h4_pages        dd      ?                                       ;
h4_tabofs       dw      ?                                       ; ofs in this page
h4_tabsize      dw      ?                                       ; # of entries
h4_reserved     dd      ?                                       ;
HD4DiskInfo     ends

HD4Entry        struc                                           ;
hde_1stpage     dd      ?                                       ;
hde_sectors     dd      ?                                       ;
HD4Entry        ends                                            ;

;cc typedef struct HD4DiskInfo       HD4_Header;
;cc typedef struct HD4Entry        HD4_TabEntry;

;cc #endif // QS_DOSHLP_HEADER
