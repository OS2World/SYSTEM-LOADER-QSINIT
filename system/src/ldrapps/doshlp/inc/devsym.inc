
SrHead          struc
ReqLen          db      ?                                       ; length in bytes
ReqUnit         db      ?                                       ; device unit number
ReqFunc         db      ?                                       ; type of request
ReqStat         dw      ?                                       ; status word
                dd      ?                                       ; reserved
DevLink         dd      ?                                       ; link field for
                                                                ; chain requests
SrHead          ends

; Status word masks
STERR           equ     8000h                                   ; bit 15 - Error
STINTER         equ     0400h                                   ; bit 10 - Interim character
STBUI           equ     0200h                                   ; bit  9 - Busy
STDON           equ     0100h                                   ; bit  8 - Done
STECODE         equ     00FFh                                   ; error code
WRECODE         equ     0

; Device Driver Request Packet Defintions
; ---------------------------------------------------------------

PktMax          equ     18
PktHeadSize     equ     13

Packet          struc
PktLen          db      ?                                       ; length in bytes of packet
PktUnit         db      ?                                       ; subunit number of block device
PktCmd          db      ?                                       ; command code
PktStatus       dw      ?                                       ; status word
PktFlag         db      ?                                       ; disk driver internal flags
PktFlagHigh     db      ?                                       ; high byte
                db      2 dup(?)                                ; reserved
PktDevLink      dd      ?                                       ; device multiple-request link
InitcUnit       db      ?                                       ; 
InitEcode       dw      ?                                       ;
InitEdata       dw      ?                                       ;
Init_pBPB       dw      ?                                       ;
                dw      ?                                       ;
                dw      ?                                       ;
                dw      ?                                       ;
                dw      ?                                       ;
                dw      ?                                       ;
                db      ?                                       ;
Packet          ends

fPktInt13RP     equ     01H
fPktCallOutDone equ     02H
fPktDiskIOTchd  equ     04H

PktData1        struc
PacketHead1     db      PktHeadSize dup(?)       
InitcUnit       db      ?
InitpEnd        dd      ?
InitParms1      dd      ?
Initdrv1        db      ?
res1            db      ?
InitSysiData1   db      ?
PktData1        ends

PktData2        struc
PacketHead2     db      PktHeadSize dup(?)       
InitUnit        db      ?
InitEcode       dw      ?
InitEdata       dw      ?
InitParms2      dd      ?
res2            db      ?
Initdrv22       db      ?
res3            db      ?
InitSysiData2   db      ?
PktData2        ends

InitDevHlp      equ     InitpEnd
InitpBPB        equ     InitParms1

LENInit         equ     10
LENInitRes      equ     (LENInit + 2)
.errnz          (PktMax LT LENInit)
.errnz          (PktMax LT LENInitRes)

DCS_Addr        equ     dword ptr PktData+3
VCS_Addr        equ     dword ptr PktData+7
LENGetDevSpprt  equ     11

MedChkmedia     equ     byte ptr PktData
MedChkflag      equ     byte ptr PktData+1
MedChkpVID      equ     dword ptr PktData+2
LENMedChk       equ     6
.errnz          (PktMax LT LENMedChk)

BldBPBmedia     equ     byte ptr PktData                        ; media byte
BldBPBbuffer    equ     dword ptr PktData+1                     ; scratch buffer
BldBPBpBPB      equ     dword ptr PktData+5                     ; returned pointer to BPB
LENBldBPB       equ     9
.errnz          (PktMax LT LENBldBPB)

PktData3        struc
PacketHead3     db      PktHeadSize dup(?)       
InitcUnit3      db      ?
InitpEnd3       dd      ?
IOcount         dw      ?
IOstart         dw      ?
Initdrv3        db      ?
res33           db      ?
InitSysiData    db      ?
PktData3        ends

IOmedia         equ     InitcUnit                               ; media byte
IOpData         equ     InitpEnd                                ; transfer address
IOPhysRBA       equ     InitParms1                              ; physical starting sector

IOSFN           equ     word ptr PktData+11                     ; for device only
LENIO           equ     13
PktAdvise       equ     word ptr PktData+13                     ; for >= v12 only
.errnz          (PktMax LT LENIO)

;ReadBypassAdj   equ     CMDInputBypass-CMDINPUT
;WriteBypassAdj  equ     CMDOutputBypass-CMDOUTPUT

NDRbyte         equ     byte ptr PktData                        ; byte read from device
LENNDR          equ     1
.errnz          (PktMax LT LENNDR)

LENStatus       equ     0
.errnz          (PktMax LT LENStatus)

LENFlush        equ     0
.errnz          (PktMax LT LENFlush)

OCSFN           equ     word ptr PktData
OCLastRef       equ     word ptr PktData+2
LENOpnCls       equ     4
.errnz          (PktMax LT LENOpnCls)

LENRemMed       equ     0
.errnz          (PktMax LT LENRemMed)

CStpSKG         equ     byte ptr PktData+0                      ; screen/keyboard number
LenStp          equ     1                                       ; size of request additions
.errnz          (PktMax LT LenStp)

CMDGenIOCTL16   struc
PacketGIO       db      PktHeadSize dup(?)       
GIOCategory     db      ?                                       ; category code
GIOFunction     db      ?                                       ; function code
GIOParaPack     dd      ?                                       ; pointer to parameter packet
GIODataPack     dd      ?                                       ; pointer to data packet
GIOSFN          dw      ?                                       ; (used by spooler?)
GIOParaLen      dw      ?                                       ; length of parameter packet
GIODataLen      dw      ?                                       ; length of data packet
CMDGenIOCTL16   ends

LENGenIOCTL     equ     12                                      ; length w/o new parameters
.errnz          (PktMax LT LENGenIOCTL)

LENGenIOCTL2    equ     16                                      ; length w/ new parameters
.errnz          (PktMax LT LENGenIOCTL2)

LENDeIn         equ     0
.errnz          (PktMax LT LENDeIn)

LENResMed       equ     0
.errnz          (PktMax LT LENResMed)

Add_On_NewDD    equ     dword ptr PktData
Add_On_SrvRout  equ     dword ptr PktData
LenAddOn        equ     4

Logical_Drive   equ     byte ptr PktData                        ; logical drive number
LenGetSetLog    equ     1

LENShutdown     equ     5
SDFuncCode      equ     byte ptr PktData
SDReserveLow    equ     word ptr PktData+1
SDReserveHigh   equ     word ptr PktData+3

STDDERROR       equ     4000h
ERROR_DEVICEDRIVER_DEFINED_BASE EQU 0FE00h

StaticPackets   equ     128

ReqPktSize      equ     32
.ERRNZ  (SIZE Packet - 31)

;
; PhysDiskTbl - Physical Disk Entry Structure
; --------------------------------------------------------------
diskentry       struc
ddheader        dd      0                                       ; pointer to device header
ereserved       db      0                                       ; reserved
reldisknum      db      0                                       ; disk # for device driver
devpid          dw      0                                       ; pid
deveflags       dw      0                                       ; flags for the physical disk
logunitbitmap   dd      0                                       ; logical unit bit map
diskentry       ends


LENSaveRestore  equ     1
SRFuncCode      equ     byte ptr PktData
SRFC_Save       equ     0
SRFC_Restore    equ     1

CMDGenIOCTLK    struc
PacketGIOK      db      PktHeadSize dup(?)       
GIOCategoryK    db      ?                                       ; category code
IOpData         dd      ?                                       ; pointer to data packet
GIOFunctionK    db      ?                                       ; function code
GIOParaPackK    dd      ?                                       ; pointer to parameter packet
GIOSFNK         dw      ?                                       ; (used by spooler?)
GIOParaLenK     dw      ?                                       ; length of parameter packet
GIODataLenK     dw      ?                                       ; length of data packet
CMDGenIOCTLK    ends

PktRW           struc
PacketHeadRW    db      PktHeadSize dup(?)       
MediaRW         db      ?                                       ;
XferAddrRW      dd      ?                                       ; 1
NumSectorsRW    dw      ?                                       ; 5
RBA_RW          dd      ?                                       ; 7
SFN_RW          dw      ?                                       ;
PktRW           ends

PktIO           struc
HeadIO          SrHead  <?>
InitcUnit3      db      ?
InitpEnd3       dd      ?
IOcount         dw      ?
IOstart         dw      ?
Initdrv3        db      ?
res33           db      ?
InitSysiData    db      ?
PktIO           ends

;
; Packet for DHAttachDD
; ---------------------------------------------------------------
IDCPacket       struc
IDCRealOff      dw      ?                                       ; reserved
IDCRealCS       dw      ?                                       ;
IDCRealDS       dw      ?                                       ;
IDCProtOff      dw      ?                                       ;
IDCProtCS       dw      ?                                       ;
IDCProtDS       dw      ?                                       ;
IDCPacket       ends
