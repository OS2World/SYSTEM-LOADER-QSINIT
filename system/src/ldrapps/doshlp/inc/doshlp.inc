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

OS2DUMP_BUFFER_SIZE    = 80000h                                 ; 512k

;
; 32-bit helpers.
; note, that SSToDS() must be used for stack args
;
;cc typedef unsigned long (_cdecl *EFDCall32)(unsigned long, ...);
EFDC32_MaxIndex        = 0                                      ; () - max suported index
EFDC32_SetThAffinity   = 1                                      ; (u64t *mask)
EFDC32_CurProcNum      = 2                                      ; () = 0..x - current processor

; info struct for bootos2, it must fill all parameters, marked with ">".
; note: signature field added for hd4disk only, was no way before to
; check loader type (QS or OS/4) in loaded OS/2 system.
;
; note, that structure is shared with AOSLDR and some fields are just not used here
;
; struct size (EFDStructSize) is valid only if EXPF_FLAGSEX flag is set
;
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
ClockMod        dw      ?                                       ; > clock modulation value
PushKey         dw      ?                                       ; > push key code (or 0)
DBCSSeg         dw      ?                                       ; > OS2DBCS segment
DBCSFontSeg     dw      ?                                       ; > OS2DBCS.FNT segment
DBCSFontSize    dd      ?                                       ; > OS2DBCS.FNT size
MemPagesLo      dd      ?                                       ; > # of memory pages below 4Gb (except 1st Mb)
MemPagesHi      dd      ?                                       ; > # of memory pages above 4Gb
SavedInt10h     dd      ?                                       ; BIOS int10h vector
FontTabOfs      dw      ?                                       ; FONT_TABLE offset in OEMHLP
FBDActive       db      ?                                       ; >
FBDBits         db      ?                                       ; >
FBDMemLin       dd      ?                                       ; FBD* are not used in QSINIT
FBDMemPhys      dd      ?                                       ; >
FBDLineLen      dd      ?                                       ; >
FBDModeX        dw      ?                                       ; >
FBDModeY        dw      ?                                       ; >
FBDMaskR        dd      ?                                       ; >
FBDMaskG        dd      ?                                       ; >
FBDMaskB        dd      ?                                       ; >
RMEFList        dd      ?                                       ; > not used in QSINIT
LogMapSize      dd      ?                                       ; > log area map size, in 64kb
CpuFBits        dd      ?                                       ; > sys_isavail() result
CfgData         dd      ?                                       ; > config.sys text (ofs from LogBufPAddr)
CfgDataLen      dw      ?                                       ; > config.sys text length
GSICPktOfs      dw      ?                                       ; offset of GSIC data packet
BiosTabOfs      dw      ?                                       ; offset of BIOS tables array
VBiosPhys       dd      ?                                       ; > not used in QSINIT
;EFDCall32
PFCall32        dd      ?                                       ; > 32-bit api entry point
PFEPtr          dd      ?                                       ; > internal, for Call32
DumpBufLin      dd      ?                                       ; buffer for OS2DUMP (lin.addr)
DumpBufPhys     dd      ?                                       ; buffer for OS2DUMP (phys.addr)
FlagsEx         dd      ?                                       ; more flags
EFDStructSize   dw      ?                                       ; this struct size
DumpHubOrder    dw      ?                                       ; > order of disk search for OS2DUMP
PCILastBus      db      ?                                       ; > PCI last bus
NCpuRun         db      ?                                       ; # of initialized CPUs
BPrintFOfs      dw      ?                                       ; offset of _bprintf16
ExportFixupData ends

; ExportFixupData.Flags bits (loader information & state)
EXPF_PCI        equ     1                                       ; PCI present
EXPF_DHINIT     equ     2                                       ; DHInit was done
EXPF_DISCARDED  equ     4                                       ; Boot part was discarded
EXPF_TWODSKBOOT equ     8                                       ; Two disk boot
EXPF_FULLCABLE  equ    10h                                      ; COM port full cable option active
EXPF_COMINITED  equ    20h                                      ; COM port inited at least once
EXPF_EFIHOST    equ    40h                                      ; (not used in QSINIT)
EXPF_DBCSI10H   equ    80h                                      ; DBCS int 10h call done
EXPF_MODE3      equ   100h                                      ; set 80x25 mode was called after EXPF_DISCARDED
EXPF_PFLIST     equ   200h                                      ; preload files available (RMEFList is valid)
EXPF_CFGTEXT    equ   400h                                      ; config.sys text is valid
EXPF_ACPIRESET  equ   800h                                      ; use ACPI reset call
EXPF_CALL32     equ  1000h                                      ; PFCall32 supported
EXPF_FLAGSEX    equ  2000h                                      ; extra flags present (as well as struct size)

; DumpHubOrder bits
DUMPHAB_AHCI    equ     1                                       ;
DUMPHAB_BIOS    equ     2                                       ;

EXPFX_NEWDUMP   equ     1                                       ; new OS2DUMP loaded
EXPFX_FORCEDUMP equ     2                                       ; force system dump in SCREEN01.SYS init
EXPFX_DUMPSRCH  equ     4                                       ; enable volume search logic in dump
EXPFX_OHTRACE   equ     8                                       ; trace OEMHLP$ calls
EXPFX_OHTRPCI   equ    10h                                      ; trace OEMHLP$ PCI calls
EXPFX_OHTSHIFT  equ     3                                       ; bit shift for the trace value above
EXPFX_OHTRAP    equ    20h                                      ; enable test OEMHLP$ trap on "1111" string
EXPFX_VDMBUF    equ    40h                                      ; not used in QSINIT

; sys_isavail() values (stored in ExportFixupData.CpuFBits above)
;cc #ifndef SFEA_INVM
SFEA_PAE        equ   00000001h                                 ; PAE supported              
SFEA_PGE        equ   00000002h                                 ; PGE supported              
SFEA_PAT        equ   00000004h                                 ; PAT supported              
SFEA_CMOV       equ   00000008h                                 ; CMOV instructions (>=P6)   
SFEA_MTRR       equ   00000010h                                 ; MTRR present               
SFEA_MSR        equ   00000020h                                 ; MSRs present               
SFEA_X64        equ   00000040h                                 ; AMD64 present              
SFEA_CMODT      equ   00000080h                                 ; clock modulation supported 
SFEA_LAPIC      equ   00000100h                                 ; Local APIC available       
SFEA_FXSAVE     equ   00000200h                                 ; FXSAVE available           
SFEA_XSAVE      equ   00000400h                                 ; XSAVE available            
SFEA_SSE1       equ   00000800h                                 ; SSE available              
SFEA_SSE2       equ   00001000h                                 ; SSE2 available             
SFEA_INVM       equ   08000000h                                 ; running in virtual machine 
SFEA_INTEL      equ   10000000h                                 ; CPU is Intel               
SFEA_AMD        equ   20000000h                                 ; CPU is AMD                 
;cc #endif // SFEA_INVM

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

VerifyAccessR   macro  function                                 ;
                mov     dx, 27h                                 ; DevHlp_VerifyAccess
                call    [DEVHLP]                                ;
endm                                                            ;

VerifyAccessW   macro  function                                 ;
                mov     dx, 127h                                ; DevHlp_VerifyAccess
                call    [DEVHLP]                                ;
endm                                                            ;


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

; always returned as zero-filled in QSINIT
GSIC_Packet     struc                                           ;
gsic_ver        dd      ?                                       ;
gsic_info       dd      ?                                       ;
gsic_cmd        db      ?                                       ;
gsic_cmdport    dw      ?                                       ;
gsic_ep2        db      ?                                       ;
gsic_eventport  dw      ?                                       ;
gsic_flhi       dw      ?                                       ;
gsic_fl1        db      ?                                       ;
gsic_fl2        db      ?                                       ;
gsic_reserved   db      8 dup (?)                               ;
GSIC_Packet     ends                                            ;

; get offset of bprintf16(), return zero if loader is too old (no function)
;cc #define get_bprint(efd) \
;cc    (((efd)->Flags & EXPF_FLAGSEX) && \
;cc       (efd)->EFDStructSize>FIELDOFFSET(struct ExportFixupData, BPrintFOfs) \
;cc          ? (efd)->BPrintFOfs : 0)

; types for printf16() and bprintf16() (16-bit compiler only)
; read \system\src\init\misc\prn16.asm for details
;cc #if defined(__WATCOMC__) && !defined(__386__)
;cc typedef void far __cdecl (*DHSeroutPrnFn)(char far *fmt,...);
;
;cc typedef void far __cdecl (*DHCharoutFn)(char);
;cc typedef void far __cdecl (*DHPrnFn)(DHCharoutFn charout, char far *fmt,...);
;cc #endif

;cc #endif // QS_DOSHLP_HEADER
