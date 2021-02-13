;cc #ifndef QS_BASE_BIOSDATA_INC
;cc #define QS_BASE_BIOSDATA_INC

;
; BIOS data area
;

BiosDataArea    struc
BDA_Com         dw      4 dup (?)         ;
BDA_Lpt         dw      3 dup (?)         ;
BDA_Xbda        dw      ?                 ;
BDA_Hardware    dw      ?                 ;
BDA_Unused1     db      ?                 ;
BDA_MemSize     dw      ?                 ;
BDA_Unused2     dw      ?                 ;
BDA_KbFlags     dw      ?                 ;
BDA_KbAlt       db      ?                 ;
BDA_KbHead      dw      ?                 ;
BDA_KbTail      dw      ?                 ;
BDA_KbBuf       dw      16 dup (?)        ;
BDA_FdRec       db      ?                 ; floppy recalibration status
BDA_FdMtr       db      ?                 ; floppy motor status
BDA_FdMtrTime   db      ?                 ; floppy motor turn-off timeout
BDA_FdStatus    db      8 dup (?)         ;
BDA_VMode       db      ?                 ;
BDA_VCols       dw      ?                 ;
BDA_VPageSize   dw      ?                 ;
BDA_VPageStart  dw      ?                 ;
BDA_VCurPos     dw      8 dup (?)         ;
BDA_VCurType    dw      ?                 ;
BDA_VPage       db      ?                 ;
BDA_VCrtPort    dw      ?                 ;
BDA_VCrtLast    db      ?                 ; last value written to 3D8h
BDA_VPalLast    db      ?                 ; last value written to 3D9h
BDA_Reset       dd      ?                 ;
BDA_Unused3     db      ?                 ;
BDA_TmCounter   dd      ?                 ; timer ticks since midnight
BDA_TmRollover  db      ?                 ;
BDA_CBreak      db      ?                 ;
BDA_SoftReset   dw      ?                 ;
BDA_HdLastErr   db      ?                 ; fixed disk last operation status
BDA_HdCount     db      ?                 ; # of fixed disks in system
BDA_HdMisc      dw      ?                 ;
BDA_LptTimeout  db      4 dup (?)         ;
BDA_ComTimeout  db      4 dup (?)         ;
BDA_KbBufStart  dw      ?                 ;
BDA_KbBufEnd    dw      ?                 ;
BDA_VRows       db      ?                 ;
BDA_VCharLines  dw      ?                 ; scan-lines in character
BDA_VCtl        db      ?                 ;
BDA_VSwitches   db      ?                 ;
BDA_VModeOpts   db      ?                 ; next mode set options
BDA_VDCCIndex   db      ?                 ;
BDA_FsLastRate  db      ?                 ; floppy last data rate
BDA_HdMisc2     dw      ?                 ;
BDA_HdIntCtrl   db      ?                 ; fixed disk interrupt flag
BDA_FdInfo      db      ?                 ; floppy disk controller information
BDA_FdMedia     db      4 dup (?)         ;
BDA_FdTrack     db      2 dup (?)         ;
BDA_KbFlagsExt  db      ?                 ;
BDA_KbLed       db      ?                 ;
BDA_WaitPtr     dd      ?                 ;
BDA_WaitTimeout dd      ?                 ;
BDA_WaitFlag    db      ?                 ;
BDA_Unused4     db      7 dup (?)         ;
BDA_VSaveTable  dd      ?                 ;
BDA_Unused5     dd      21 dup (?)        ;
BiosDataArea    ends 

StaticFuncTable struc                     ; int 10h 1Bh
VST_Mode0       db      ?                 ; modes 0..7 supported
VST_Mode1       db      ?                 ; modes 8..F supported
VST_Mode2       db      ?                 ; modes 10..13 supported
VST_Unused1     dd      ?                 ;
VST_ScanLines   db      ?                 ; scan lines supported
VST_CBlk        db      ?                 ;
VST_ActiveCblk  db      ?                 ;
VST_MiscFlags   dw      ?                 ;
VST_Unused2     dw      ?                 ;
VST_SaveFlags   db      ?                 ;
VST_Unused3     db      ?                 ;
StaticFuncTable ends                      ;

VideoFuncInfo   struc                     ; int 10h 1Bh
VFI_StaticTab   dd      ?                 ; far ptr to StaticFuncTable
VFI_BDA1        db      30 dup (?)        ; copy of BiosDataArea starting from BDA_VMode
VFI_VRows       db      ?                 ;
VFI_VCharLines  dw      ?                 ; scan-lines in character
VFI_ActiveDCC   db      ?                 ;
VFI_AltDCC      db      ?                 ;
VFI_Colors      dw      ?                 ;
VFI_Pages       db      ?                 ;
VFI_ScanLines   db      ?                 ;
VFI_CBPrimary   db      ?                 ; primary character block
VFI_CBSec       db      ?                 ; secondary character block
VFI_MiscFlags   db      ?                 ;
VFI_NonVGA      db      ?                 ;
VFI_Unused1     db      2 dup (?)         ;
VFI_VideoMem    db      ?                 ; video memory available
VFI_SaveFlags   db      ?                 ;
VFI_DispInfo    db      ?                 ;
VFI_Unused2     db      12 dup (?)        ;
VideoFuncInfo   ends                      ;

;cc #endif // QS_BASE_BIOSDATA_INC
