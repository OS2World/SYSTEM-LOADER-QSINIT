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
BDA_VCrtAddr    dw      ?                 ;
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
BDA_VMisc       dw      ?                 ;
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

;cc #endif // QS_BASE_BIOSDATA_INC
