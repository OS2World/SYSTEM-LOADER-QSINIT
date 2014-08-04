;
; QSINIT
; OS/4 loader interface part
;

OS4MAGIC        equ 342F534Fh         ; OS/4 string (placed at 100:0)

LDRINFO_OFS     equ 4                 ; offset of LoaderInfoData from 100:0

LoaderInfoData  struc
StructSize        dw    ?             ; size of struct
BootKernel        db    14 dup (?)    ; kernel file name
BootFlags         dd    ?             ; flags
FlatCS            dw    ?             ;
FlatDS            dw    ?             ;
DevHlpFunc        dd    ?             ; DevHlp ptr (ready after OEMHLP init)
DebugTarget       dw    ?             ; debug target (see ldr_target.h)
DebugPort         dw    ?             ; debug port
DosHlpFlatBase    dd    ?             ; doshlp FLAT adress
LdrHighFlatBase   dd    ?             ; boothlp FLAT adress (discardable loader part)
VMemFlatBase      dd    ?             ; 0 here (was A0000 mapped to kernel linear)
VesaInfoOffset    dw    ?             ; vesa info offset
PrintFOffset      dw    ?             ; offset of DHPrintf function in DOSHLP segment
ConfigExt         dd    ?             ; config file .Ext instead of .SYS
VMemGDTSel        dw    ?             ; set by loader OR by OS/4 kernel after going to PM
LoaderInfoData  ends

; BootFlags values

BOOTFLAG_LOG       equ 00000001h      ; log on
BOOTFLAG_CHS       equ 00000002h      ; int13 ext. off
BOOTFLAG_NOREV     equ 00000004h      ; no revision
BOOTFLAG_LFB       equ 00000008h      ; LFB allowed in logo
BOOTFLAG_CFGEXT    equ 00000010h      ; config.sys .EXT replacement is active
BOOTFLAG_VESAFNT   equ 00000020h      ; vesa font copy present
BOOTFLAG_PRELOAD   equ 00000040h      ; preload flag is set
BOOTFLAG_NOLOGO    equ 00000080h      ; no logo mode active
BOOTFLAG_DBCS_ON   equ 00000100h      ; DBCS fonts loaded (not used in QSINIT)
BOOTFLAG_PGINIT    equ 00000200h      ; pginit was done (set by OS/4 kernel)
BOOTFLAG_EDISK     equ 00800000h      ; boot from emulated disk (QSINIT only)
BOOTFLAG_NOAF      equ 01000000h      ; do NOT align disk i/o to 4k (QSINIT only)
BOOTFLAG_WARPSYS   equ 02000000h      ; warp kernel (<= Merlin FP12, QSINIT only)
BOOTFLAG_SMP       equ 04000000h      ; SMP kernel (QSINIT only)

; DebugTarget values
DEBUG_TARGET_NONE  equ 0000h          ; output is absent
DEBUG_TARGET_COM   equ 0001h          ; com port
DEBUG_TARGET_VIO   equ 0002h          ; vio output
DEBUG_TARGET_OTHER equ 0004h          ; external output (not used)
DEBUG_TARGET_MASK  equ 0007h          ; mask for all output
DEBUG_CTRLC_ON     equ 0008h          ; kernel debuffer must check for Ctrl-C

DEBUG_VERBOSE_LOG  equ 8000h          ; VERBOSE option (for OS/4 kernel logging)
