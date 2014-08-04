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

PUBLIC_INFO     segment
                dw      DOSHLP_SIGN                             ; check bytes
                dw      offset External                         ;
                public  PublicInfo                              ;
                public  External                                ;
PublicInfo      LoaderInfoData <size LoaderInfoData>            ; must be at 100:4
; interface with bootos2.exe
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
PUBLIC_INFO     ends

_DATA           segment
                public  _swcode, _paeppd
_swcode         dd      0
_paeppd         dd      0
_DATA           ends

                end
