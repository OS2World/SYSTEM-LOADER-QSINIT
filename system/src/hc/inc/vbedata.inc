;
; VESA mode info structure
;

VBEInfoBlock            struc
VBESignature            dd     ?
VBEVersion              dw     ?
OemStringPtr            dd     ?
VBECaps                 dd     ?
ModeTablePtr            dd     ?
TotalMemory             dw     ?
OemSoftwareRev          dw     ?
OemVendorNamePtr        dd     ?
OemProductNamePtr       dd     ?
OemProductRevPtr        dd     ?
VBEInfo_Reserved        db     222 dup (?)
VBEInfo_OemData         db     256 dup (?)
VBEInfoBlock            ends

VBEModeInfoBlock        struc
ModeAttributes          dw     ?
WinAAttributes          db     ?
WinBAttributes          db     ?
Granularity             dw     ?
WinSize                 dw     ?
WinASegment             dw     ?
WinBSegment             dw     ?
WinFuncPtr              dd     ?
BytesPerScanline        dw     ?
XResolution             dw     ?
YResolution             dw     ?
XCharSize               db     ?
YCharSize               db     ?
NumberOfPlanes          db     ?
BitsPerPixel            db     ?
NumberOfBanks           db     ?
MemoryModel             db     ?
BankSize                db     ?
NumberOfImagePages      db     ?
Reserved                db     ?
RedMaskSize             db     ?
RedFieldPosition        db     ?
GreenMaskSize           db     ?
GreenFieldPosition      db     ?
BlueMaskSize            db     ?
BlueFieldPosition       db     ?
RsvdMaskSize            db     ?
RsvdFieldPosition       db     ?
DirectColorModeInfo     db     ?
PhysBasePtr             dd     ?
OffScreenMemOffset      dd     ?
OffScreenMemSize        dw     ?
VBEModeInfoBlock        ends

;cc typedef struct VBEModeInfoBlock VBEModeInfo;
;cc typedef struct VBEInfoBlock         VBEInfo;

VSMI_SUPPORTED          equ    0000001h                         ;/< mode supported
VSMI_OPTINFO            equ    0000002h                         ;/< vesa 1.2 info
VSMI_BIOSSUPP           equ    0000004h                         ;/< BIOS support present
VSMI_GRAPHMODE          equ    0000010h                         ;/< graph mode
VSMI_NOBANK             equ    0000040h                         ;/< no bank switching
VSMI_LINEAR             equ    0000080h                         ;/< linear mode support

VSWA_EXISTS             equ    0000001h                         ;/< window exists
VSWA_READABLE           equ    0000002h                         ;/< window readable
VSWA_WRITEABLE          equ    0000004h                         ;/< window writeable

VM_640x480x8            equ    101h
