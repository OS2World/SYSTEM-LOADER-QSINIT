;
; QSINIT
; DOSHLP binary public header (100:0004)
;
                .486p
                include segdef.inc
                include doshlp.inc
                include ldrparam.inc

                extrn   GetSysConfig:near
                extrn   DHStubInit:near
                extrn   MsrSetupTable:byte
                extrn   DosHlpFunctions:byte
                extrn   DosHlpResEnd:byte
                extrn   _dd_bootdisk:byte
                extrn   EndOfDosHlp:byte
                extrn   Io32FixupTable:byte
                extrn   _BootBPB:byte
                extrn   _driveparams:byte
                extrn   _edparmtable:byte
                extrn   _eddparmtable:byte
                extrn   _i13ext_table:byte
                extrn   GSIC_Data:byte
                extrn   FONT_TABLE:dword
                extrn   BIOSTAB_Data:byte
                extrn   ExtCall32:near
                extrn   _bprintf16:near

PUBLIC_INFO     segment
                dw      DOSHLP_SIGN                             ; check bytes
                dw      offset External                         ;
                public  PublicInfo                              ;
                public  External                                ;
PublicInfo      LoaderInfoData <size LoaderInfoData>            ; must be at 100:4
; interface with bootos2.exe
; but also a public structure for several users (like HD4DISK) ...
                public  External                                ; too long init line to
External        label   near                                    ; wasm it, write separately
e@InfoSign      dd      0                                       ;
e@Init1Ofs      dw      offset GetSysConfig                     ;
e@Init2Ofs      dw      offset DHStubInit                       ;
e@DumpSeg       dw      0                                       ;
e@DisPartOfs    dw      offset DosHlpResEnd                     ;
e@DosHlpTabOfs  dw      offset DosHlpFunctions                  ;
e@BootDisk      dw      offset _dd_bootdisk                     ;
e@DosHlpSize    dw      offset EndOfDosHlp                      ;
e@LowMem        dw      0                                       ;
e@HighMem       dw      0                                       ;
e@ExtendMem     dd      0                                       ;
e@xBDALen       dw      0                                       ;
e@Flags         dw      0                                       ;
e@IODelay       dw      0                                       ;
e@Io32FixupOfs  dw      offset Io32FixupTable                   ;
e@Io32FixupCnt  dw      0                                       ;
e@BootBPBOfs    dw      offset _BootBPB                         ;
e@MsrTableOfs   dw      offset MsrSetupTable                    ;
e@MsrTableCnt   dw      0                                       ;
e@LogBufLock    db      0                                       ;
e@KernelVer     db      0                                       ;
e@LogBufPAddr   dd      0                                       ;
e@LogBufVAddr   dd      0                                       ;
e@LogBufSize    dd      0                                       ;
e@LogBufWrite   dd      0                                       ;
e@LogBufRead    dd      0                                       ;
e@LAddrDelta    dd      0                                       ;
e@ResMsgOfs     dw      0                                       ;
e@DisMsgOfs     dw      0                                       ;
e@DisPartSeg    dw      0                                       ;
e@DisPartLen    dw      0                                       ;
e@OS2Init       dd      0                                       ;
e@Buf32k        dw      0                                       ;
e@BaudRate      dd      0                                       ;
e@PCICount      dw      0                                       ;
e@PCIBusList    dw      0                                       ;
e@PCIClassList  dw      0                                       ;
e@PCIVdrList    dw      0                                       ;
e@HD4Page       dd      0                                       ;
e@DPOfs         dw      offset _driveparams                     ;
e@EDTabOfs      dw      offset _edparmtable                     ;
e@EDDTabOfs     dw      offset _eddparmtable                    ;
e@DFTabOfs      dw      offset _i13ext_table                    ;
e@PSwcode       dw      offset _swcode                          ;
e@Ppaeppd       dw      offset _paeppd                          ;
e@ClockMod      dw      0                                       ;
e@PushKey       dw      0                                       ;
e@DBCSSeg       dw      0                                       ;
e@DBCSFontSeg   dw      0                                       ;
e@DBCSFontSize  dd      0                                       ;
e@MemPagesLo    dd      0                                       ;
e@MemPagesHi    dd      0                                       ;
e@SavedInt10h   dd      0                                       ;
e@FontTabOfs    dw      offset FONT_TABLE                       ;
e@FBDActive     db      0                                       ; not used
e@FBDBits       db      0                                       ; not used
e@FBDMemLin     dd      0                                       ; not used
e@FBDMemPhys    dd      0                                       ; not used
e@FBDLineLen    dd      0                                       ; not used
e@FBDModeX      dw      0                                       ; not used
e@FBDModeY      dw      0                                       ; not used
e@FBDMaskR      dd      0                                       ; not used
e@FBDMaskG      dd      0                                       ; not used
e@FBDMaskB      dd      0                                       ; not used
e@RMEFList      dd      0FFFFFFFFh                              ; not used
e@LogMapSize    dd      0                                       ;
e@CpuFBits      dd      0                                       ;
e@CfgData       dd      0                                       ;
e@CfgDataLen    dw      0                                       ;
e@GSICPktOfs    dw      offset GSIC_Data                        ;
e@BiosTabOfs    dw      offset BIOSTAB_Data                     ;
e@VBiosPhys     dd      0                                       ; not used
e@PFCall32      dd      offset ExtCall32                        ;
e@PFEPtr        dd      0                                       ;
e@DumpBufLin    dd      0                                       ;
e@DumpBufPhys   dd      0                                       ;
e@FlagsEx       dd      0                                       ;
e@EFDStructSize dw      ExtFini - External                      ;
e@DumpHubOrder  dw      0                                       ;
e@PCILastBus    db      0                                       ;
e@NCpuRun       db      1                                       ;
e@BPrintFOfs    dw      offset _bprintf16                       ;
ExtFini         label   near                                    ;
PUBLIC_INFO     ends

if size ExportFixupData NE ExtFini - External                   ; check is this rec equal
                .err                                            ; to original struct?
endif                                                           ;

_DATA           segment
                public  _swcode, _paeppd
_swcode         dd      0
_paeppd         dd      0
_DATA           ends

                end
