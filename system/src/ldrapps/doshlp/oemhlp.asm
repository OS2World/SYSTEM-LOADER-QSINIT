;
; QSINIT
; oemhlp "driver" code
;

                ; 0 - use saved device list, supplied by bootos2.exe
                ; 1 - walk over PCI config space & search
                oemhlp_walk  = 1

                include segdef.inc                              ;
                include doshlp.inc                              ;
                include ldrparam.inc                            ;
                include devhlp.inc                              ;
                include inc/oempci.inc                          ;
                include inc/pci.inc                             ;
                include inc/ioint13.inc                         ;
                include inc/qsconst.inc                         ;
                include inc/qstypes.inc                         ;
                include inc/devsym.inc                          ; ioctl reference
                include inc/debug.inc                           ;

OEMHLP_DATA     segment                                         ;
;
; OEMHLP$ driver command dispatch table
; ---------------------------------------------------------------
OEMHLPTable     label   word                                    ;
                dw      FN_ERR                                  ; 0  - Init for device
                dw      FN_ERR                                  ; 1  - Media Check
                dw      FN_ERR                                  ; 2  - Build BPB
                dw      FN_ERR                                  ; 3  - Reserved
                dw      FN_LOGREAD                              ; 4  - Read (log)
                dw      FN_NONDESRD                             ; 5  - Non-Destruct Read
                dw      FN_OK                                   ; 6  - Input Status
                dw      FN_OK                                   ; 7  - Input Flush
                dw      FN_LOGWRITE                             ; 8  - Write (log)
                dw      FN_ERR                                  ; 9  - Write w/Verify
                dw      FN_OK                                   ; A  - Output Status
                dw      FN_OK                                   ; B  - Output Flush
                dw      FN_ERR                                  ; C  - IOCtl Write
                dw      FN_OK                                   ; D  - Device Open
                dw      FN_OK                                   ; E  - Device Close
                dw      FN_ERR                                  ; F  - Removable Media
                dw      FN_IOCTL                                ; 10 - Generic Ioctl
                dw      FN_ERR                                  ; 11 - Reset Media
                dw      FN_ERR                                  ; 12 - Get Logical Drive Map
                dw      FN_ERR                                  ; 13 - Set Logical Drive Map
                dw      FN_ERR                                  ; 14 - DeInstall
                dw      FN_ERR                                  ; 15 - Reserved
                dw      FN_ERR                                  ; 16 - HDD
                dw      FN_ERR                                  ; 17 - HDD
                dw      FN_ERR                                  ; 18 - Reserved
                dw      FN_ERR                                  ; 19 - Reserved
                dw      FN_ERR                                  ; 1A - Reserved
TableSize       EQU     ($-offset OEMHLPTable)/2                ;
; ---------------------------------------------------------------
pINIT           dw      DEV_INIT                                ; 1B - Device Init
                dw      BOOTHLP_CODESEL                         ;
;
; IOCTL table
; ---------------------------------------------------------------
IOCTLTable      label   word                                    ;
                dw      DOS_INFO                                ; 0 - return DOS info
                dw      GET_MACH_INFO                           ; 1 - return machine info
                dw      GET_VIDEO_INFO                          ; 2 - return DCC info
                dw      GET_FONTS                               ; 3 - return fonts
                dw      FN_ERR                                  ; 4 - EISA (unsupported)
                dw      BIOS_INFO                               ; 5 - return BIOS info
                dw      GET_VIDEO_INFO                          ; 6 - return misc state
                dw      GET_VIDEO_INFO                          ; 7 - return adapter types
                dw      GET_VIDEO_INFO                          ; 8 - return SVGA info
                dw      GET_MEMORY_INFO                         ; 9 - return memory info
                dw      GET_VIDEO_INFO                          ; A - return DMQS selector
                dw      FN_ERR                                  ; B - PCI - will be replaced by PciCall
                dw      GET_DBCS_INFO                           ; C - return DBCS unique information
                dw      GET_MACH_ID                             ; D - return machine ID byte
                dw      QUERY_DISK_INFO                         ; E - return disk info
                dw      GET_GSIC_INFO                           ; F - return Enhanced SpeedStep data
                dw      GET_DEVHLP_ADDR                         ;10 - return DevHlp address
                dw      GET_MEM_INFO_EX                         ;11 - return extended memory info
                dw      FN_ERR                                  ;12 - get video frame buffer
                dw      LOG_WRITE                               ;13 - write to the log/debug port
                dw      GET_BIOS_TABLE                          ;14 - get bios table pointer
IOCTLTable_Size equ     $-offset IOCTLTable                     ;

;
; Table of PCI bios calls supported by oemhlp
; ---------------------------------------------------------------
PCITable        label   word                                    ;
                dw      PCIGetBiosInfo                          ;
                dw      PCIFindDevice                           ;
                dw      PCIFindClassCode                        ;
                dw      PCIReadConfig                           ;
                dw      PCIWriteConfig                          ;
PCITable_Size   equ     $-offset PCITable                       ;


OEMHLP_CHECK_VIDEO      equ     2                               ; OEMHLP IOCTL functions
OEMHLP_MISC_INFO        equ     6                               ;
OEMHLP_VIDEO_ADAPTER    equ     7                               ;
OEMHLP_SVGA_INFO        equ     8                               ;
OEMHLP_DMQS_INFO        equ     10                              ;

OEMHLP_DBCS_HWSCROLL    equ     0                               ; return video H/W scroll capability
OEMHLP_DBCS_FONTGDTS    equ     1                               ; return GDTs for DBCS fonts
OEMHLP_DBCS_INT10VECTOR equ     2                               ; return original INT 10h vector

;
; OEMHLP_GETOEMADAPTIONINFO data packet
; ---------------------------------------------------------------
; note: version patched in DHStubInit at hardcoded offset!
ADAPT_Packet    struc                                           ;
OEM             db      'QSINIT OS/2 loader', 2 dup (0)         ; Name of OEM
REV             db      '20.45',5 dup (0)                       ; Internal revision
ADAPT_Packet    ends                                            ;

;
; OEMHLP_GETMACHINEINFO data packet
; ---------------------------------------------------------------
MACH_Packet     struc                                           ;
MachOEM         db      20 dup (?)                              ; name of manufacturer
MachModel       db      10 dup (?)                              ; model number
MachROM         db      10 dup (?)                              ; ROM version number
MACH_Packet     ends                                            ;

;
; OEMHLP_GETROMBIOSINFO data packet
; ---------------------------------------------------------------
BIOS_Packet     struc
BiosModel       dw      ?                                       ; machine model byte
BiosSubModel    dw      ?                                       ; machine submodel byte
BiosRevision    dw      ?                                       ; BIOS revision level
BiosIsAbios     dw      ?                                       ; ABIOS present?
BIOS_Packet     ends

;
; OEMHLP_GETBIOSTABLE data packet
; ---------------------------------------------------------------
BIOSTAB_Packet  struc                                           ;
BiosTabAddr     dd      ?                                       ; phys.addr
BiosTabLen      dd      ?                                       ; length
BiosTabStatus   dw      ?                                       ; status (bit 0 - crc error)
BIOSTAB_Packet  ends                                            ;

;
; OEMHLP_GETMEMINFO data packet
; ---------------------------------------------------------------
MEMORY_Packet   struc                                           ;
LowMemKB        dw      ?                                       ;
HighMemKB       dd      ?                                       ;
MEMORY_Packet   ends                                            ;

;
; OEMHLP_GETMEMINFOEX data packet
; ---------------------------------------------------------------
MEMORYEX_Packet struc                                           ;
MiLowPages      dd      ?                                       ;
MiHighPages     dd      ?                                       ;
MiAvailPages    dd      ?                                       ;
MEMORYEX_Packet ends                                            ;

                public  AdaptInfo, GSIC_Data, BIOSTAB_Data
AdaptInfo       label   near
ADAPT_Data      ADAPT_Packet    <>                              ;
BIOS_Data       BIOS_Packet     <>                              ;
MEMORY_Data     MEMORY_Packet   <>                              ;
MEMORYEX_Data   MEMORYEX_Packet <>                              ;
MACH_Data       MACH_Packet     <>                              ;
GSIC_Data       GSIC_Packet     <>                              ;

BIOSTAB_Size    equ     8                                       ;
BIOSTAB_Data    db      BIOSTAB_Size * sizeof BIOSTAB_Packet dup (0)

; ---------------------------------------------------------------
; OEMHLP_CHECK_VIDEO, OEMHLP_MISC_INFO or OEMHLP_VIDEO_ADAPTER
; data packet structure
; ---------------------------------------------------------------

NO_ADAPTER_SET  equ     -1                                      ;

SVGAInfo_s      struc                                           ; matches OEMSVGAINFO (bvhsvga)
AdapterType1    dw      ?                                       ;
ChipType1       dw      ?                                       ;
Memory1         dd      ?                                       ; size of video RAM
SVGAInfo_s      ends                                            ;

ScreenDDName    db      "SCREEN$ "                              ; name of PDD we communicate with
SVGAInfo        SVGAInfo_s <NO_ADAPTER_SET,0,0100000h>          ;
UNKNOWN_DISPLAY EQU     -1                                      ;
VGA_COLOR       EQU     8                                       ;
DisplayCode     db      UNKNOWN_DISPLAY                         ;
DisplayMisc     db      0                                       ; Misc. state info

CGA_BIT         equ     2                                       ;
VGA_BIT         equ     8                                       ;
MAX_FONTS       equ     6                                       ;
FONT_SIZE       equ     4*MAX_FONTS

                public  FONT_TABLE                              ;
FONT_TABLE      dd      0,0,0,0,0,0,0                           ;
;
; DBCS data GDT selectors list (6 words)
; ---------------------------------------------------------------
DBCS_SelCnt     dw      0                                       ;
DBCS_SegSize    dw      0                                       ; last segment size
DBCS_Sel        dw      0,0,0,0                                 ;
; ---------------------------------------------------------------

                public  DEVHLP                                  ;
                public  PCIConfigBX,PCIConfigAL,PCIConfigCL
DEVHLP          dd      ?                                       ; Device Helper service routine
Machine_ID      dw      0                                       ; unsupported ID for ThinkPads
PCIConfigBX     dw      0                                       ;
PCIConfigAL     db      0                                       ;
PCIConfigCL     db      0                                       ;
                extrn _edparmtable:byte                         ; BIOS disk parameter tables
                extrn _eddparmtable:byte                        ;
OEMHLP_DATA     ends
; -----------------------------------------------------------------
DOSHLP_CODE     segment                                         ;
                extrn get_dgroup:near                           ;
                extrn DHSerOut:near                             ;
DOSHLP_CODE     ends                                            ;
; -----------------------------------------------------------------
DOSHLP_DATA     segment                                         ;
                extrn External:ExportFixupData                  ;
                extrn PublicInfo:LoaderInfoData                 ;
                extrn KAT:KernelAccess                          ;
DOSHLP_DATA     ends                                            ;
; -----------------------------------------------------------------
LASTSEG         segment                                         ;
                extrn EndOfDosHlp:near                          ; end of entire doshlp
LASTSEG         ends                                            ;
; -----------------------------------------------------------------
_BSS            segment
                extrn DHConfigAX:word                           ; system config data
                extrn DHConfigBX:word                           ;
                extrn DHConfigCH:byte                           ;
                extrn DHConfigCL:byte                           ;
                extrn DHConfigDH:byte                           ;
                extrn DHConfigDL:byte                           ;
_BSS            ends
; -----------------------------------------------------------------
OEMHLP_CODE     segment
                assume  ds:DGROUP, es:nothing, ss:nothing       ;
;
; OEMHLP Strategy Entry Point
; -----------------------------------------------------------------
                public  OEMHLPStrategy                          ;
OEMHLPStrategy  proc    far                                     ;
                push    ds                                      ;
                push    esi                                     ;
                call    get_dgroup                              ;
                mov     ds,ax                                   ;
                mov     al,es:[bx].ReqFunc                      ; check command code
                cmp     al,TableSize                            ;
                jc      @@oemstrat_process                      ;
                cmp     al,CMDInitBase                          ; init?
                jnz     @@oemstrat_error                        ;
                cmp     pINIT,offset FN_ERR                     ; init done?
                jnz     @@oemstrat_init                         ;
@@oemstrat_error:
                mov     ax,STERR + ERROR_I24_BAD_COMMAND        ; Unknown Cmd + Error Bit
                jmp     @@oemstrat_exit                         ;
@@oemstrat_init:
                call    dword ptr [pINIT]                       ; process init
                jmp     @@oemstrat_exit                         ;
@@oemstrat_process:
                xor     ah,ah                                   ;
                mov     si,ax                                   ; command table offset
                shl     si,1                                    ;
                push    es                                      ;
                push    bx                                      ;
                push    dx                                      ;

                test    byte ptr External.FlagsEx,EXPFX_OHTRACE ; trace on?
                jz      @@oemstrat_t1skip                       ;
                dbg16print <"OEMHLP strat %x",10>,<ax>          ;
@@oemstrat_t1skip:
                call    OEMHLPTable[si]                         ; process request

                test    byte ptr cs:External.FlagsEx,EXPFX_OHTRACE ; trace on?
                jz      @@oemstrat_t2skip                       ;
                dbg16print <"OEMHLP strat rc %x",10>,<ax>       ;
@@oemstrat_t2skip:
                pop     dx                                      ;
                pop     bx                                      ;
                pop     es                                      ;
@@oemstrat_exit:
                mov     es:[bx].ReqStat,ax                      ; rc
                or      es:[bx].ReqStat,STDON                   ; set "done" flag
                pop     esi                                     ;
                pop     ds                                      ;
                ret                                             ;
OEMHLPStrategy  endp
; ---------------------------------------------------------------
                public  FN_ERR                                  ;
FN_ERR          proc    near                                    ;
                mov     ax,STERR + ERROR_I24_BAD_COMMAND        ; Unknown Cmd + Error Bit
                ret                                             ;
FN_ERR          endp                                            ;
; ---------------------------------------------------------------
                public  FN_OK                                   ;
FN_OK           proc    near                                    ;
                xor     ax,ax                                   ; no error
                ret                                             ;
FN_OK           endp                                            ;

;
; OEM Generic IOCTL Routine
; ---------------------------------------------------------------
; In : es:bx = request packet address, ds = OEMHLP_DATA
; Out: ax = error code
                public  FN_IOCTL
FN_IOCTL        proc    near                                    ;
                cmp     es:[bx].GIOCategory,90h                 ; deny ACPI calls
                jz      FN_ERR                                  ;
                xor     eax,eax                                 ;
                mov     al,es:[bx].GIOFunction                  ; command code
                lea     esi,[eax*2]                             ;
                cmp     si,IOCTLTable_Size                      ;
                jnc     FN_ERR                                  ;

                cmp     al,11                                   ;
                jz      @@fiocd_skip                            ;
                test    byte ptr External.FlagsEx,EXPFX_OHTRACE ; trace on?
                jz      @@fiocd_skip                            ;
                dbg16print <"OEMHLP ioctl %x",10>,<ax>          ;
                push    di                                      ;
                call    IOCTLTable[si]                          ;
                pop     di                                      ;
                dbg16print <"OEMHLP ioctl rc %x",10>,<ax>       ;
                ret                                             ;
@@fiocd_skip:
                push    di                                      ;
                call    IOCTLTable[si]                          ;
                pop     di                                      ;
                ret                                             ;
FN_IOCTL        endp
;
; Copy data packet
; ---------------------------------------------------------------
; In: si = src, cx = count, es:bx = IOCTL data packet
CopyDataPacket  proc   near
                push    di                                      ;
                mov     ax,word ptr es:[bx].GIODataPack+2       ; check cx bytes
                mov     di,word ptr es:[bx].GIODataPack         ; data access.
                VerifyAccessW                                   ;
                mov     ax,STERR + ERROR_I24_INVALID_PARAMETER  ;
                jc      @@ExitCDP                               ;
                les     di,es:[bx].GIODataPack                  ;
                cld                                             ;
            rep movsb                                           ; copying packet
                xor     ax,ax                                   ;
@@ExitCDP:
                pop     di                                      ;
                ret                                             ;
CopyDataPacket  endp
; ---------------------------------------------------------------
                public  DOS_INFO
DOS_INFO        proc    near                                    ;
                mov     cx,size ADAPT_Packet                    ;
                mov     si,offset ADAPT_Data                    ;
                call    CopyDataPacket                          ;
DOS_INFO        endp
; ---------------------------------------------------------------
                public  BIOS_INFO
BIOS_INFO       Proc    Near                                    ;
                mov     cx,size BIOS_Packet                     ;
                mov     si,offset BIOS_Data                     ;
                call    CopyDataPacket                          ;
                ret                                             ;
BIOS_INFO       endp
; ---------------------------------------------------------------
                public  GET_DEVHLP_ADDR
GET_DEVHLP_ADDR proc    near                                    ;
                mov     cx,4                                    ;
                mov     si,offset DEVHLP                        ; DevHlp address
                call    CopyDataPacket                          ;
                ret                                             ;
GET_DEVHLP_ADDR endp
; ---------------------------------------------------------------
                public  GET_MACH_INFO
GET_MACH_INFO   proc    near                                    ;
                mov     cx,size MACH_Packet                     ;
                mov     si,offset MACH_Data                     ;
                call    CopyDataPacket                          ;
                ret                                             ;
GET_MACH_INFO   endp
; ---------------------------------------------------------------
                public  GET_MEMORY_INFO
GET_MEMORY_INFO proc    near                                    ;
                mov     cx,size MEMORY_Packet                   ;
                mov     si,offset MEMORY_Data                   ;
                call    CopyDataPacket                          ;
                ret                                             ;
GET_MEMORY_INFO endp                                            ;
; ---------------------------------------------------------------
                public  GET_MEM_INFO_EX
GET_MEM_INFO_EX proc    near                                    ;
                mov     cx,size MEMORYEX_Packet                 ;
                mov     si,offset MEMORYEX_Data                 ;
                call    CopyDataPacket                          ;
                ret                                             ;
GET_MEM_INFO_EX endp                                            ;
; ---------------------------------------------------------------
                public  GET_MACH_ID
GET_MACH_ID     proc    near                                    ;
                mov     cx,2                                    ;
                mov     si,offset Machine_ID                    ;
                call    CopyDataPacket                          ;
                ret                                             ;
GET_MACH_ID     endp

;
; query video info generic IOCTL routine
; ---------------------------------------------------------------
                public  GET_VIDEO_INFO
GET_VIDEO_INFO  proc    near                                    ;
                test    External.FlagsEx,EXPFX_FORCEDUMP        ;
                jz      @@gvi_0                                 ;
                mov     al,DHGETDOSV_VECTORSDF                  ; force system dump
                Dev_Hlp GetDOSVar                               ;
                or      bx,bx                                   ;
                jz      @@gvi_0                                 ;
                mov     es,ax                                   ;
                jmp     dword ptr es:[bx]                       ;
@@gvi_0:
                push    cx                                      ;
                push    di                                      ;
                mov     di,word ptr es:[bx].GIODataPack         ;
                mov     al,es:[bx].GIOFunction                  ;
                mov     cx,1                                    ; video info packet
                cmp     al,OEMHLP_SVGA_INFO                     ;
                jnz     @@gvi_1                                 ;
                mov     cx,size SVGAInfo_s                      ;
@@gvi_1:
                cmp     al,OEMHLP_DMQS_INFO                     ;
                jnz     @@gvi_2                                 ;
                add     cx,3                                    ; dword parameter
@@gvi_2:                                                        ;
                mov     ax,word ptr es:[bx].GIODataPack+2       ; verify access
                VerifyAccessW                                   ;
                mov     ax,STERR+ERROR_I24_INVALID_PARAMETER    ;
                jc      @@gvi_exit                              ; exit with error

                mov     al,es:[bx].GIOFunction                  ;
                push    es                                      ;
                les     di,es:[bx].GIODataPack                  ;
                cmp     al,OEMHLP_CHECK_VIDEO                   ;
                jnz     @@gvi_3                                 ;
                mov     al,DisplayCode                          ; return display code
                jmp     @@gvi_video                             ;
@@gvi_3:
                cmp     al,OEMHLP_MISC_INFO                     ;
                jnz     @@gvi_4                                 ;
                mov     al,DisplayMisc                          ; return misc. info.
                jmp     @@gvi_video                             ;
@@gvi_4:
                cmp     al,OEMHLP_VIDEO_ADAPTER                 ;
                jne     @@gvi_5                                 ;
                mov     al,CGA_BIT or VGA_BIT                   ; return adapter type
                jmp     @@gvi_video                             ;
@@gvi_5:
                cmp     al,OEMHLP_SVGA_INFO                     ;
                jne     @@gvi_dmqs                              ;

                mov     si,offset SVGAInfo                      ; ds:si - SVGAInfo
                cmp     [si].AdapterType1,NO_ADAPTER_SET        ;
                jnz     @@gvi_screendd_called                   ; make call to SCREEN$
                pop     es                                      ; es:bx - request packet
                mov     di,offset ScreenDDName                  ; ds:di - driver name
                push    ds                                      ; preserve important registers.
                push    si                                      ;
                xor     ax,ax                                   ;
                or      ax,STERR                                ; set error
                call    dword ptr KAT.IOCTLWorker               ; call IOCTL and set STERR bit in ax if error
                pop     si                                      ;
                test    ax,STERR                                ;
                jnz     @@gvi_unrec                             ; error?
                mov     di,si                                   ;
                lds     si,es:[bx].GIODataPack                  ; ds:si - data pack is source
                pop     es                                      ; load es with saved ds
                mov     cx,size SVGAInfo_s                      ;
            rep movsb                                           ;
                xor     ax,ax                                   ; return ok
@@gvi_exit:
                pop     di                                      ;
                pop     cx                                      ;
                ret                                             ;
@@gvi_unrec:
                pop     ds                                      ;
                mov     ds:[si].AdapterType1,0                  ; unrecognized adapter
                les     di,es:[bx].GIODataPack                  ;
                push    es                                      ; fix the stack frame.
@@gvi_screendd_called:
                mov     cx,size SVGAInfo_s                      ; es:di - packet
            rep movsb                                           ; copying from SVGAInfo
                jmp     @@gvi_return                            ; to data packet
@@gvi_dmqs:
                cmp     al,OEMHLP_DMQS_INFO                     ;
                jnz     @@gvi_return                            ;
                xor     ax,ax                                   ;
                stosw                                           ; return 0 in DMQS ptr
                stosw                                           ;
                pop     es                                      ; restore stack frame
                jmp     @@gvi_exit                              ;
@@gvi_video:
                mov     es:[di],al                              ; return 1 byte info
@@gvi_return:
                pop     es                                      ; restore stack frame
                xor     ax,ax                                   ; success
                jmp     @@gvi_exit                              ;
GET_VIDEO_INFO  endp

;
; query font IOCTL routine
; ---------------------------------------------------------------
GET_FONTS       proc    near                                    ;
                push    cx                                      ;
                push    di                                      ;
                mov     ax,word ptr es:[bx].GIODataPack+2       ; check access
                mov     di,word ptr es:[bx].GIODataPack         ;
                mov     cx,FONT_SIZE                            ;
                VerifyAccessW                                   ;
                mov     ax,STERR + ERROR_I24_INVALID_PARAMETER  ;
                jc      @@qfnt_err                              ;
                les     di,es:[bx].GIODataPack                  ;
                mov     cx,MAX_FONTS*2                          ; maximum size of font packet
                mov     si,offset FONT_TABLE                    ; adaptation data area
                cld                                             ; set forward copy direction
@@qfnt_loop:
                cmp     word ptr es:[di],-1                     ; check for end of font packet
                jz      @@qfnt_exit                             ;
                movsw                                           ; copy font information
                loop    @@qfnt_loop                             ;
@@qfnt_exit:
                xor     ax,ax                                   ;
@@qfnt_err:
                pop     di                                      ;
                pop     cx                                      ;
                ret                                             ;
GET_FONTS       endp

;
; DBCS services IOCTL routine
; ---------------------------------------------------------------
GET_DBCS_INFO   proc    near                                    ;
                push    cx                                      ;
                push    di                                      ;
                cmp     word ptr es:[bx].GIOParaLen,4           ;
                mov     ax,STERR + ERROR_I24_INVALID_PARAMETER  ;
                jnz     @@dbcsi_ret                             ;
                mov     ax,word ptr es:[bx].GIOParaPack+2       ; check access
                mov     di,word ptr es:[bx].GIOParaPack         ;
                mov     cx,word ptr es:[bx].GIOParaLen          ;
                VerifyAccessR                                   ;
                mov     ax,STERR + ERROR_I24_INVALID_PARAMETER  ;
                jc      @@dbcsi_ret                             ;
                mov     ax,word ptr es:[bx].GIODataPack+2       ;
                mov     di,word ptr es:[bx].GIODataPack         ;
                mov     cx,word ptr es:[bx].GIODataLen          ;
                VerifyAccessW                                   ;
                mov     ax,STERR + ERROR_I24_INVALID_PARAMETER  ;
                jc      @@dbcsi_ret                             ;
                push    es                                      ;
                les     di,es:[bx].GIOParaPack                  ;
                mov     ax,word ptr es:[di]                     ; Fn #

                test    byte ptr External.FlagsEx,EXPFX_OHTRACE ; trace on?
                jz      @@dbcsi_skiptrace                       ;
                dbg16print <"OEMHLP dbcs info Fn %x",10>,<ax>   ;
@@dbcsi_skiptrace:
                pop     es                                      ;
                les     di,es:[bx].GIODataPack                  ;
                or      ax,ax                                   ;
                jnz     @@dbcsi_01                              ; OEMHLP_DBCS_HWSCROLL
                mov     es:[di],al                              ; just 0
                jmp     @@dbcsi_noerr                           ;
@@dbcsi_01:
                cmp     ax,OEMHLP_DBCS_FONTGDTS                 ;
                jnz     @@dbcsi_02                              ;
                call    DBCS_FONT_GDTS                          ;
                jmp     @@dbcsi_noerr                           ;
@@dbcsi_02:
                cmp     ax,OEMHLP_DBCS_INT10VECTOR              ;
                jnz     @@dbcsi_noerr                           ;
                call    DBCS_INT10VEC                           ;
                jc      @@dbcsi_ret                             ;
@@dbcsi_noerr:
                xor     ax, ax                                  ;
@@dbcsi_ret:
                pop     di                                      ;
                pop     cx                                      ;
                ret                                             ;
GET_DBCS_INFO   endp                                            ;

DBCS_FONT_GDTS  proc    near                                    ;
@@dbcs_fntaddr  = dword ptr [bp-4]                              ;
@@dbcs_fntlen   = dword ptr [bp-8]                              ;
                movzx   eax,External.DbcsFontSeg                ;
                shl     eax,4                                   ;
                jz      @@dbcs_fnsel_ret                        ;
                cmp     DBCS_SelCnt,0                           ;
                jnz     @@dbcs_fnsel_ret                        ;
                push    di                                      ;
                push    es                                      ;
                push    bp                                      ;
                mov     bp,sp                                   ;
                push    eax                                     ;
                push    External.DBCSFontSize                   ;
@@dbcs_fnsel_loop:
                push    ds                                      ;
                pop     es                                      ;
                mov     di,offset DBCS_Sel                      ;
                mov     ax,DBCS_SelCnt                          ;
                shl     ax,1                                    ;
                add     di,ax                                   ;
                mov     cx,1                                    ;
                Dev_Hlp AllocGDTSelector                        ;
                mov     si,es:[di]                              ;
                mov     eax,@@dbcs_fntaddr                      ;
                mov     ecx,@@dbcs_fntlen                       ;
                cmp     ecx,0F000h                              ;
                jbe     @@dbcs_fnsel_last                       ;
                mov     ecx,0F000h                              ;
@@dbcs_fnsel_last:
                mov     dh,1                                    ;
                Dev_Hlp PhysToGDTSel                            ;
                inc     DBCS_SelCnt                             ;
                mov     DBCS_SegSize,cx                         ;
                add     @@dbcs_fntaddr,0F000h                   ;
                sub     @@dbcs_fntlen,0F000h                    ;
                jbe     @@dbcs_fnsel_ready                      ;
                cmp     DBCS_SelCnt,4                           ;
                jc      @@dbcs_fnsel_loop                       ;
@@dbcs_fnsel_ready:
                add     sp,8                                    ;
                pop     bp                                      ;
                pop     es                                      ;
                pop     di                                      ;
@@dbcs_fnsel_ret:
                cld                                             ;
                mov     si,offset DBCS_SelCnt                   ;
                mov     cx,6                                    ;
            rep movsw                                           ;
                ret                                             ;
DBCS_FONT_GDTS  endp                                            ;

DBCS_INT10VEC   proc    near                                    ; update int 10h vector
                test    External.Flags,EXPF_DBCSI10H            ; in bios interrupt table
                cld                                             ; to value, supplied by OS2DBCS
                jz      @@dbcs_i10_set                          ;
                xor     ax,ax                                   ;
                stosw                                           ;
                stosw                                           ;
                jmp     @@dbcs_i10_ret                          ;
@@dbcs_i10_set:
                mov     eax,External.SavedInt10h                ; ptr for caller
                stosd                                           ;
                xor     ax,ax                                   ; 4 bytes at phys 0040h
                mov     bx,40h                                  ;
                mov     cx,4                                    ;
                mov     dh,4                                    ;
                Dev_Hlp PhysToUVirt                             ;
                mov     ax,STERR + ERROR_I24_INVALID_PARAMETER  ;
                jc      @@dbcs_i10_ret                          ;
                mov     eax,External.SavedInt10h                ; restore BIOS int 10h
                mov     es:[bx],eax                             ; vector, saved by OS2DBCS
                mov     ax,es                                   ;
                mov     dh,2                                    ;
                Dev_Hlp PhysToUVirt                             ; flag it to prevent
                or      External.Flags,EXPF_DBCSI10H            ; second call
@@dbcs_i10_ret:
                ret                                             ;
DBCS_INT10VEC   endp                                            ;

;
; read log from user space (copy OEMHLP$ boot.log)
; ---------------------------------------------------------------
FN_LOGREAD      proc    near                                    ;
                xor     ax,ax                                   ;
                push    esi                                     ;
                pusha                                           ;
                push    es                                      ;
                push    ebx                                     ;
                mov     cx,word ptr es:[bx].NumSectorsRW        ; bytes to read
                mov     ax,word ptr es:[bx].XferAddrRW+2        ;
                mov     bx,word ptr es:[bx].XferAddrRW          ; map buffer
                mov     dh,1                                    ;
                Dev_Hlp PhysToVirt                              ; result in es:di
                jnc     @@RDCopy                                ;
                jmp     @@RDNothingNoLock                       ;
@@RDNothing:
                UnlockLogBuffer External.LogBufLock             ; exiting with unlock
@@RDNothingNoLock:
                pop     ebx                                     ;
                pop     es                                      ;
                popa                                            ;
                pop     esi                                     ;
                mov     word ptr es:[bx].NumSectorsRW,ax        ; 0 bytes was readed
                ret                                             ;
@@RDCopy:
                LockLogBuffer External.LogBufLock               ; !!!lock log, no any printf
                mov     esi,External.LogBufRead                 ; calls until unlock!!!
                cmp     esi,External.LogBufWrite                ; r/w pos comparation
                je      @@RDNothing                             ;
                push    gs                                      ;
                push    ecx                                     ;
                mov     bp,cx                                   ; output buffer size
                mov     ecx,External.LogBufSize                 ; log size
                shl     ecx,16                                  ;
                mov     ebx,External.LogBufVAddr                ; log buffer address
                xor     dx,dx                                   ;
                mov     gs,PublicInfo.FlatDS                    ; kernel FLAT
@@RDCopyLoop:
                mov     al,Byte Ptr gs:[esi+ebx]                ; copying data
                stosb                                           ;
                inc     esi                                     ;
                inc     dx                                      ;
                cmp     esi,ecx                                 ;
                jnz     @@RDCopyNoFix                           ;
                xor     esi,esi                                 ; cycled buffer
@@RDCopyNoFix:
                cmp     esi,External.LogBufWrite                ; reach write pos?
                je      @@RDCopyDone                            ;
                cmp     dx,bp                                   ; reach end of buffer?
                je      @@RDCopyDone                            ;
                jmp     @@RDCopyLoop                            ;
@@RDCopyDone:
                pop     ecx                                     ;
                pop     gs                                      ;
                assume  gs:nothing                              ;
                mov     External.LogBufRead,esi                 ; save read pos
                UnlockLogBuffer External.LogBufLock             ; unlock log
                pop     ebx                                     ;
                pop     es                                      ;
                mov     word ptr es:[bx].NumSectorsRW,dx        ; actual readed
                popa                                            ;
                pop     esi                                     ;
                xor     ax,ax                                   ; rc=0
                ret                                             ;
FN_LOGREAD      endp                                            ;

;
; write text to log from user space (echo Hi!>OEMHLP$)
; ---------------------------------------------------------------
FN_LOGWRITE     proc    near                                    ;
                xor     ax,ax                                   ;
                pusha                                           ;
                push    es                                      ;
                push    ebx
                mov     cx,word ptr es:[bx].NumSectorsRW        ; number of bytes to write
                mov     si,cx                                   ;
                mov     ax,word ptr es:[bx].XferAddrRW+2        ; map buffer
                mov     bx,word ptr es:[bx].XferAddrRW          ;
                mov     dh,1                                    ; result in es:di
                Dev_Hlp PhysToVirt                              ;
                jnc     @@WRPrint                               ;
                pop     ebx                                     ;
                pop     es                                      ;
                popa                                            ;
                mov     word ptr es:[bx].NumSectorsRW,ax        ; 0 bytes written
                ret                                             ;
@@WRPrint:
                test    byte ptr External.FlagsEx,EXPFX_OHTRAP  ; trap enabled?
                jz      @@WRPrint2                              ;
                cmp     si,4                                    ;
                jc      @@WRPrint2                              ; int 3 on "1111" string
                cmp     dword ptr es:[di], 31313131h            ;
                jnz     @@WRPrint2                              ;
                int     3                                       ;
@@WRPrint2:
                mov     dx,si                                   ; cx - input buffer size
                mov     si,di                                   ;
@@WRLoop:
                jcxz    @@WREnd                                 ; loop serial out
                lods    byte ptr es:[si]                        ;
                push    cs                                      ;
                call    DHSerOut                                ;
                loop    @@WRLoop                                ;
@@WREnd:
                pop     ebx                                     ; restore packet address
                pop     es                                      ;
                mov     word ptr es:[bx].NumSectorsRW,dx        ; all data was written
                popa                                            ;
                xor     ax,ax                                   ; rc=0
                ret                                             ;
FN_LOGWRITE     endp

;
; same as FN_LOGWRITE, but via ioctl
; ---------------------------------------------------------------
LOG_WRITE       proc    near                                    ;
                mov     ax,word ptr es:[bx].GIOParaPack+2       ; check write size
                mov     di,word ptr es:[bx].GIOParaPack         ;
                mov     cx,2                                    ;
                VerifyAccessR
                jc      @@logwr_fail                            ;
                push    es                                      ;
                les     di,es:[bx].GIOParaPack                  ;
                mov     cx,word ptr es:[di]                     ; size to write
                pop     es                                      ;
                jcxz    @@logwr_done                            ;
                mov     ax,word ptr es:[bx].GIODataPack+2       ;
                mov     di,word ptr es:[bx].GIODataPack         ; access to string
                mov     si,cx                                   ;
                VerifyAccessR                                   ;
                jc      @@logwr_fail                            ;
                mov     cx,si                                   ;
                les     si,es:[bx].GIODataPack                  ;
@@logwr_loop:
                lods    byte ptr es:[si]                        ;
                push    cs                                      ;
                call    DHSerOut                                ;
                loop    @@logwr_loop                            ;
@@logwr_done:
                xor     ax,ax                                   ;
                ret                                             ;
@@logwr_fail:
                mov     ax,STERR + ERROR_I24_INVALID_PARAMETER  ;
                ret                                             ;
LOG_WRITE       endp                                            ;

;
; get BIOS table pointer ioctl
; ---------------------------------------------------------------
GET_BIOS_TABLE  proc    near                                    ;
                mov     ax,word ptr es:[bx].GIOParaPack+2       ; check index value
                mov     di,word ptr es:[bx].GIOParaPack         ;
                mov     cx,2                                    ;
                VerifyAccessR                                   ;
                jc      @@btab_fail                             ;
                push    es                                      ;
                les     di,es:[bx].GIOParaPack                  ;
                mov     si,word ptr es:[di]                     ; table number
                pop     es                                      ;
                cmp     si,BIOSTAB_Size                         ;
                jnc     @@btab_fail                             ;
                imul    si,size BIOSTAB_Packet                  ;
                add     si,offset BIOSTAB_Data                  ;
                test    [si].BiosTabAddr,-1                     ; addr is 0?
                jz      @@btab_notab                            ; return error
                mov     cx,size BIOSTAB_Packet                  ;
                call    CopyDataPacket                          ;
                ret                                             ;
@@btab_notab:
                mov     ax,STERR + ERROR_I24_READ_FAULT         ;
                ret                                             ;
@@btab_fail:
                mov     ax,STERR + ERROR_I24_INVALID_PARAMETER  ;
                ret                                             ;
GET_BIOS_TABLE  endp                                            ;

;
; stub
; ---------------------------------------------------------------
FN_NONDESRD     proc    near                                    ;
                mov     ax,STBUI+STDON                          ; Set BUSY & DONE bits
                ret                                             ;
FN_NONDESRD     endp                                            ;

;
; PCI function select
; ---------------------------------------------------------------
PciCall         proc    near                                    ;
                push    di                                      ;
                push    ecx                                     ;
                push    ebx                                     ;
                push    fs                                      ;

                mov     ax,word ptr es:[bx].GIOParaPack+2       ;
                mov     di,word ptr es:[bx].GIOParaPack         ;
                mov     cx,PCI_PBI_PARM_SIZE                    ; verify packet
                VerifyAccessR                                   ;
                jc      @@pcc_err                               ;
                mov     fs,ax                                   ; read subfn
                xor     eax,eax                                 ; check subfn number
                mov     al,fs:[di].pbi_bPCIFunc                 ;
                shl     ax,1                                    ;
                cmp     ax,PCITable_Size                        ;
                cmc
                jc      @@pcc_err                               ; invalid subfn
                call    PCITable[eax]                           ;
@@pcc_exit:
                test    byte ptr External.FlagsEx,EXPFX_OHTRPCI ; PCI trace on?
                jz      @@pcc_notrace                           ;
                dbg16print <"Leaving PciCall: rc %x",10>,<ax>   ;
@@pcc_notrace:
                pop     fs                                      ;
                pop     ebx                                     ;
                pop     ecx                                     ;
                pop     di                                      ;
                ret                                             ;
@@pcc_err:
                mov     ax,STERR+ERROR_I24_INVALID_PARAMETER    ;
                jmp     @@pcc_exit                              ;
PciCall         endp                                            ;

; ---------------------------------------------------------------
@@selReqPack   = word ptr [bp - 2]
@@offReqPack   = word ptr [bp - 4]
@@selParmPack  = word ptr [bp - 6]
@@offParmPack  = word ptr [bp - 8]
@@selDataPack  = word ptr [bp - 10]
@@offDataPack  = word ptr [bp - 12]
; ---------------------------------------------------------------
VrfyPCIData     macro   DataSize
                mov     @@selReqPack,es                         ;
                mov     @@offReqPack,bx                         ;
                mov     ax,word ptr es:[bx].GIOParaPack+2       ;
                mov     di,word ptr es:[bx].GIOParaPack         ;
                mov     @@selParmPack,ax                        ;
                mov     @@offParmPack,di                        ;
                mov     ax,word ptr es:[bx].GIODataPack+2       ;
                mov     di,word ptr es:[bx].GIODataPack         ;
                mov     @@selDataPack,ax                        ;
                mov     @@offDataPack,di                        ;
                mov     cx,DataSize                             ;
                VerifyAccessW                                   ;
                endm

; verify parm packet accessibility
VrfyPCIParm     macro   ParmSize
                mov     ax,@@selParmPack                        ;
                mov     di,@@offParmPack                        ;
                mov     cx,ParmSize                             ;
                VerifyAccessR                                   ;
                endm
;
; PCI Bios Presence Check
; ---------------------------------------------------------------
; function return parameters, saved on init
PCIGetBiosInfo  proc    near                                    ;
                enter   12,0                                    ;
                VrfyPCIData PCI_PBI_DATA_SIZE                   ;
                jc      PCIFuncErr                              ;
                test    byte ptr External.FlagsEx,EXPFX_OHTRPCI ; PCI trace on?
                jz      @@pcgbi_notrace                         ;
                dbg16print <"GetBiosInfo",10>                   ;
@@pcgbi_notrace:
                mov     es,@@selDataPack                        ; fill data packet
                mov     bx,@@offDataPack                        ;
                mov     es:[bx].pbi_bReturn,0                   ;
                mov     ax,PCIConfigBX                          ;
                mov     byte ptr es:[bx].pbi_bMajorRev,ah       ;
                mov     byte ptr es:[bx].pbi_bMinorRev,al       ;
                mov     al,PCIConfigCL                          ;
                mov     byte ptr es:[bx].pbi_bLastBus,al        ;
                mov     al,PCIConfigAL                          ;
                mov     byte ptr es:[bx].pbi_bHWMech,al         ;
                xor     ax,ax                                   ; error code
                mov     es,@@selReqPack                         ;
                leave                                           ;
                ret                                             ;
PCIGetBiosInfo  endp                                            ;

if oemhlp_walk GT 0
; Searching for dword in PCI config space
; ---------------------------------------------------------------
; IN : eax - dword, di - mask for low 16 bits, dl - index
;      cl - offset in device space
; OUT: CF=1 if no entry, else ch=bus/cl=devfun, al = bios error code
;      esi - destroyd
PCIWalk         proc    near                                    ;
                push    bx                                      ;
                pushf                                           ;
                cli                                             ;
                mov     esi,eax                                 ; dword, we search for
                mov     ebx,80000000h                           ; starting from 0.0.0
                or      bl,cl                                   ;
                movzx   cx,dl                                   ; index
@@pciwlk_loop:
                mov     eax,ebx                                 ;
                mov     dx,PCI_ADDR_PORT                        ;
                out     dx,eax                                  ;
                add     dl,4                                    ; 0xCFC
                in      eax,dx                                  ;
                and     ax,di                                   ;
                cmp     eax,esi                                 ;
                jnz     @@pciwlk_next                           ;
                jcxz    @@pciwlk_found                          ; index==0?
                dec     cx                                      ;
@@pciwlk_next:
                test    bh,7                                    ;
                jnz     @@pciwlk_notFn0                         ;

                mov     dx,PCI_ADDR_PORT                        ;
                mov     eax,ebx                                 ;
                mov     al,PCI_HEADER_TYPE and not 3            ;
                out     dx,eax                                  ;
                add     dl,4 + (PCI_HEADER_TYPE and 3)          ; 0xCFC
                in      al,dx                                   ;
                cmp     al,0FFh                                 ;
                jz      @@pciwlk_notpresent                     ;
                test    al,80h                                  ; multifunction device?
                jnz     @@pciwlk_notFn0                         ;
@@pciwlk_notpresent:
                or      bh,7                                    ;
@@pciwlk_notFn0:
                inc     bh                                      ; next devfn
                jnz     @@pciwlk_loop                           ;
                shld    eax,ebx,16                              ;
                cmp     al,PCIConfigCL                          ;
                jz      @@pciwlk_notfound                       ;
                add     ebx,10000h                              ; next bus
                jmp     @@pciwlk_loop                           ;
@@pciwlk_found:
                shr     ebx,8                                   ;
                mov     cx,bx                                   ;
                popf                                            ;
                xor     al,al                                   ; CF=0
                pop     bx                                      ;
                ret                                             ;
@@pciwlk_notfound:
                mov     al,PCI_BIOS_ERR_NODEV                   ;
                popf                                            ;
                pop     bx                                      ;
                stc                                             ;
                ret                                             ;
PCIWalk         endp                                            ;

else ; oemhlp_walk > 0

; Search dword in PCI array
; ---------------------------------------------------------------
; IN : eax - dword, di - array, dl - index
; OUT: CF=1 if no entry, else cx - position, al - bios error code
PCISearchList   proc    near                                    ;
                push    ds                                      ;
                pop     es                                      ;
                mov     cx,External.PCICount                    ;
                cld                                             ;
@@pcisl_loop:
          repne scasd                                           ;
                jnz     @@pcisl_nodev                           ;
                or      dl,dl                                   ;
                jz      @@pcisl_found                           ;
                dec     dl                                      ;
                jmp     @@pcisl_loop                            ;
@@pcisl_nodev:
                mov     al,PCI_BIOS_ERR_NODEV                   ;
                stc                                             ;
                ret                                             ;
@@pcisl_found:
                sub     cx,External.PCICount                    ;
                not     cx                                      ;
                xor     al,al                                   ;
                ret                                             ;
PCISearchList   endp                                            ;

endif ; oemhlp_walk > 0

;
; PCI - Find Class Code
; PCI - Find Device
; ---------------------------------------------------------------
; if oemhlp_walk == 0
;   we searching in dword array of vendors or class code and get index
;   of found entry, then read bus/slot/func from the same index of
;   PCIBusList array
; else
;   we just walking over PCI space until the last bus
;
PCIFindClassCode proc    near                                   ;
                enter   12,0                                    ;
                VrfyPCIData PCI_PFCC_DATA_SIZE                  ;
                jc      PCIFuncErr                              ;
                VrfyPCIParm PCI_PFCC_PARM_SIZE                  ; verify access
                jc      PCIFuncErr                              ;
                mov     fs,@@selParmPack                        ;
                mov     bx,@@offParmPack                        ;
                mov     dl,fs:[bx].pfcc_bIndex                  ;
                mov     eax,fs:[bx].pfcc_ulClassCode            ;

                test    byte ptr External.FlagsEx,EXPFX_OHTRPCI ; PCI trace on?
                jz      pci_find_notrace1                       ;
                xor     dh,dh                                   ;
                dbg16print <"FindClassCode: %lx %d",10>,<dx,eax> ;
pci_find_notrace1:
if oemhlp_walk GT 0
                mov     di,0FF00h                               ; read dword at 8
                shl     eax,8                                   ; and ignore low byte
                mov     cl,PCI_REVISION_ID                      ;
else
                mov     di,External.PCIClassList                ;
endif
pci_find_common:
if oemhlp_walk GT 0
                call    PCIWalk                                 ;
else
                call    PCISearchList                           ;
endif
                mov     es,@@selDataPack                        ; fill packet
                mov     bx,@@offDataPack                        ;
                mov     byte ptr es:[bx].pfcc_bReturn,al        ;
                jc      PCIFuncFail                             ;
if oemhlp_walk GT 0
                xchg    ch,cl                                   ; to be compat with
                mov     ax,cx                                   ; debug printing too
else
                mov     di,External.PCIBusList                  ; get bus/slot/func
                shl     cx,1                                    ; from array
                add     di,cx                                   ; at found index
                mov     ax,[di]                                 ;
endif ; oemhlp_walk > 0
                mov     byte ptr es:[bx].pfcc_bBusNum,al        ;
                mov     byte ptr es:[bx].pfcc_bDevFunc,ah       ;
                ; ------------ debug -----------
                test    byte ptr External.FlagsEx,EXPFX_OHTRPCI ; PCI trace on?
                jz      pci_find_notrace2                       ;
                mov     cx,ax                                   ;
                mov     dx,ax                                   ;
                xor     dh,dh                                   ;
                xchg    ah,al                                   ;
                and     ax,7                                    ;
                shr     cx,11                                   ;
                dbg16print <"Device: %d.%d.%d",10>,<ax,cx,dx>   ;
pci_find_notrace2:
                ; ------------------------------
                xor     ax,ax                                   ; error code
                mov     es,@@selReqPack                         ;
                leave                                           ;
                ret                                             ;
PCIFindDevice   label   near                                    ;
                enter   12,0                                    ;
                VrfyPCIData PCI_PFD_DATA_SIZE                   ;
                jc      PCIFuncErr                              ;
                VrfyPCIParm PCI_PFD_PARM_SIZE                   ; verify access
                jc      PCIFuncErr                              ;

                mov     fs,@@selParmPack                        ; call pci bios
                mov     bx,@@offParmPack                        ;
                mov     dl,fs:[bx].pfd_bIndex                   ;
                mov     eax,dword ptr fs:[bx].pfd_usDeviceID    ;
if oemhlp_walk GT 0
                rol     eax,16                                  ; swap vendor/device words
                mov     di,0FFFFh                               ;
                xor     cl,cl                                   ; PCI_VENDOR_ID
else
                mov     di,External.PCIVendorList               ;
endif
                test    byte ptr External.FlagsEx,EXPFX_OHTRPCI ; PCI trace on?
                jz      pci_find_common                         ; common processing
                xor     dh,dh                                   ;
                dbg16print <"FindDevice: %lx %d",10>,<dx,eax>   ;
                jmp     pci_find_common                         ; -- " -- " -- " --
PCIFindClassCode endp

;
; PCI - Read Configuration Space
; ---------------------------------------------------------------
PCIReadConfig   proc    near                                    ;
                enter   12,0                                    ;
                VrfyPCIData PCI_PRC_DATA_SIZE                   ;
                jc      PCIFuncErr                              ;
                VrfyPCIParm PCI_PRC_PARM_SIZE                   ; verify access
                jc      PCIFuncErr                              ;

                test    byte ptr External.FlagsEx,EXPFX_OHTRPCI ; PCI trace on?
                jz      @@pcir_notrace1                         ;
                dbg16print <"PCIRead",10>                       ;
@@pcir_notrace1:
;
; here we come into deadlock:
; * reading of missing device can cause lock on older PCs
; * returning error will cause trap in resource.sys, produced by
;   Veit`s pcibus.snp snooper
;
; to resolve this we always return FFFFFFFF if bus/slot/func is
; not in list
;
                mov     fs,@@selParmPack                        ; device is in
                mov     bx,@@offParmPack                        ; our list?

                mov     al,fs:[bx].prc_bSize                    ;
                cmp     al,1                                    ;
                jz      @@pcir_sizeok                           ;
                cmp     al,2                                    ;
                jz      @@pcir_sizeok                           ;
                cmp     al,4                                    ;
                jnz     PCIFuncErr                              ; bad size
@@pcir_sizeok:
                call    PCIVerifyArg                            ;
                jc      @@pcir_err                              ; error in cl
                mov     ax,8000h                                ;
                or      al,fs:[bx].prc_bBusNum                  ;
                shl     eax,16                                  ;
                mov     ah,fs:[bx].prc_bDevFunc                 ;
                mov     al,fs:[bx].prc_bConfigReg               ;

                test    byte ptr External.FlagsEx,EXPFX_OHTRPCI ; PCI trace on?
                jz      @@pcir_notrace2                         ;
                dbg16print <"PCIRead: %lx",10>,<eax>            ;
@@pcir_notrace2:
                mov     dx,PCI_ADDR_PORT                        ;
                pushf                                           ; just for
                cli                                             ; safeness
                push    ax                                      ;
                and     al,not 3                                ;
                out     dx,eax                                  ;
                pop     ax                                      ;
                add     dl,4                                    ; 0xCFC
                mov     ah,fs:[bx].prc_bSize                    ;
                cmp     ah,1                                    ;
                jz      @@pcir_rdb                              ;
                cmp     ah,2                                    ;
                jz      @@pcir_rdw                              ;
                in      eax,dx                                  ;
                jmp     @@pcir_done                             ;
@@pcir_rdw:
                and     eax,2                                   ;
                add     dl,al                                   ;
                in      ax,dx                                   ;
                jmp     @@pcir_done                             ;
@@pcir_err:
                mov     cl,fs:[bx].prc_bSize                    ; make FF
                mov     eax,1                                   ; with 1,2,4
                shl     cl,3                                    ; size
                dec     cl                                      ;
                shl     eax,cl                                  ;
                shl     eax,1                                   ;
                dec     eax                                     ;
                jmp     @@pcir_exit                             ;
@@pcir_rdb:
                and     eax,3                                   ;
                add     dl,al                                   ;
                in      al,dx                                   ;
@@pcir_done:
                popf                                            ;
@@pcir_exit:
                mov     es,@@selDataPack                        ; fill packet
                mov     bx,@@offDataPack                        ;
                mov     es:[bx].prc_bReturn,0                   ;
                mov     es:[bx].prc_ulData,eax                  ;

                test    byte ptr External.FlagsEx,EXPFX_OHTRPCI ; PCI trace on?
                jz      @@pcir_notrace3                         ;
                dbg16print <"PCIRead: rc: %lx",10>,<eax>        ;
@@pcir_notrace3:
                xor     ax,ax                                   ; error code
                mov     es,@@selReqPack                         ;
                leave                                           ;
                ret                                             ;
PCIReadConfig   endp                                            ;

;
; PCI - Write Configuration Space
; ---------------------------------------------------------------
PCIWriteConfig  proc    near                                    ;
                enter   12,0                                    ;
                VrfyPCIData PCI_PWC_DATA_SIZE                   ;
                jc      PCIFuncErr                              ;
                VrfyPCIParm PCI_PWC_PARM_SIZE                   ; verify access
                jc      PCIFuncErr                              ;

                test    byte ptr External.FlagsEx,EXPFX_OHTRPCI ; PCI trace on?
                jz      @@pciw_notrace1                         ;
                dbg16print <"PCIWrite",10>                      ;
@@pciw_notrace1:
                mov     fs,@@selParmPack                        ; device is in
                mov     bx,@@offParmPack                        ; out list?
                call    PCIVerifyArg                            ;
                jc      @@pciw_err                              ; error in cl
                mov     ax,8000h                                ;
                or      al,fs:[bx].prc_bBusNum                  ;
                shl     eax,16                                  ;
                mov     ah,fs:[bx].prc_bDevFunc                 ;
                mov     al,fs:[bx].prc_bConfigReg               ;

                test    byte ptr External.FlagsEx,EXPFX_OHTRPCI ; PCI trace on?
                jz      @@pciw_notrace2                         ;
                dbg16print <"PCIWrite: %lx",10>,<eax>           ;
@@pciw_notrace2:
                mov     dx,PCI_ADDR_PORT                        ;
                pushf                                           ; just for
                cli                                             ; safeness
                mov     cl,al                                   ;
                and     al,not 3                                ;
                out     dx,eax                                  ;
                add     dl,4                                    ; 0xCFC
                mov     ch,fs:[bx].pwc_bSize                    ;
                mov     eax,fs:[bx].pwc_ulData                  ;
                cmp     ch,1                                    ;
                jz      @@pciw_wdb                              ;
                cmp     ch,2                                    ;
                jz      @@pciw_wdw                              ;
                cmp     ch,4                                    ;
                jnz     PCIFuncErr                              ; bad size
                out     dx,eax                                  ;
                jmp     @@pciw_done                             ;
@@pciw_wdw:
                and     cl,2                                    ;
                add     dl,cl                                   ;
                out     dx,ax                                   ;
                jmp     @@pciw_done                             ;
@@pciw_wdb:
                and     cl,3                                    ;
                add     dl,cl                                   ;
                out     dx,al                                   ;
@@pciw_done:
                popf                                            ;
                xor     cl,cl                                   ; CF=0
@@pciw_err:
                mov     es,@@selDataPack                        ;
                mov     bx,@@offDataPack                        ;
                mov     es:[bx].pwc_bReturn,cl                  ; fill data packet
                jc      PCIFuncFail                             ;
                xor     ax,ax                                   ; error code
pcierr_exit:
                mov     es,@@selReqPack                         ;
                leave                                           ;
                ret                                             ;
PCIWriteConfig  endp                                            ;

PCIFuncFail     label   near                                    ;
                mov     ax,STERR + ERROR_I24_GEN_FAILURE        ;
                jmp     pcierr_exit                             ;
PCIFuncErr      label   near                                    ;
                mov     ax,STERR + ERROR_I24_INVALID_PARAMETER  ;
                jmp     pcierr_exit                             ;

; Is PCI device present?
; ---------------------------------------------------------------
; function checks list if oemhlp_walk == 0 or just last bus # in walk mode
; in : fs:[bx] - read/write pci packet
; out: CF=1 if not present, cl - BIOS error code
PCIVerifyArg    label   near                                    ;
                mov     al,fs:[bx].prc_bBusNum                  ;
if oemhlp_walk GT 0
                cmp     PCIConfigCL,al                          ; it should be >=
                jc      @@pva_nodev                             ;
else ; oemhlp_walk > 0
                mov     ah,fs:[bx].prc_bDevFunc                 ; search list
                push    ds                                      ; of Bus/Slot/Func
                pop     es                                      ; for required dev.
                mov     di,External.PCIBusList                  ;
                mov     cx,External.PCICount                    ; if not present -
                cld                                             ; simulate BIOS
          repne scasw                                           ; error code
                jnz     @@pva_nodev                             ;
endif ; oemhlp_walk > 0
@@pva_allok:
                xor     cl,cl                                   ; CF=0
                ret                                             ;
@@pva_nodev:
                mov     cl,PCI_BIOS_ERR_NODEV                   ;
                stc                                             ; CF=1
                ret                                             ;

;
; Return Disk Info
; ---------------------------------------------------------------
                public  QUERY_DISK_INFO                         ;
QUERY_DISK_INFO proc    near                                    ;
                push    ds                                      ;
                mov     ax, word ptr es:[bx].GIOParaPack+2      ; check disk number byte
                mov     di, word ptr es:[bx].GIOParaPack        ;
                mov     cx, 1                                   ;
                VerifyAccessR                                   ;
                jc      @@qdi_fail                              ;
                mov     ax, word ptr es:[bx].GIODataPack+2      ; check out table buffer
                mov     di, word ptr es:[bx].GIODataPack        ;
                mov     cx, size EDDParmTable + size EDParmTable
                VerifyAccessW                                   ;
                jc      @@qdi_fail                              ;
                push    ds                                      ;
                lds     si, es:[bx].GIOParaPack                 ;
                movzx   dx, byte ptr [si]                       ; disk number
                pop     ds                                      ;
                cmp     dx, 80h                                 ;
                jb      @@qdi_fail                              ;
                sub     dx, 80h                                 ;
                cmp     dx, INT13E_TABLESIZE                    ; number of supported disks
                jnc     @@qdi_fail                              ;
                lea     si, _eddparmtable                       ;
                mov     ax, size EDDParmTable                   ;
                mul     dl                                      ;
                add     si, ax                                  ;
                push    si                                      ;
                lea     si, _edparmtable                        ;
                mov     ax, size EDParmTable                    ;
                mul     dl                                      ;
                add     si, ax                                  ;
                xor     eax, eax                                ;
                inc     si                                      ;
                inc     si                                      ;
                les     di, es:[bx].GIODataPack                 ;
                stosw                                           ;
                mov     cx, size EDParmTable - 6                ;
            rep movsb                                           ;
                stosd                                           ;
                pop     si                                      ;
                mov     cx, size EDDParmTable                   ;
            rep movsb                                           ;
                xor     ax, ax                                  ;
@@qdi_ret:
                pop     ds                                      ;
                ret                                             ;
@@qdi_fail:
                mov     ax, STERR + ERROR_I24_INVALID_PARAMETER ;
                jmp     @@qdi_ret                               ;
                ret                                             ;
QUERY_DISK_INFO endp                                            ;

;
; Enhanced SpeedStep support
; ---------------------------------------------------------------
; setup code is not implemented in QSINIT, so just return an empty
; packet here
;
                public  GET_GSIC_INFO
GET_GSIC_INFO   proc near
                mov     ax, word ptr es:[bx].GIODataPack+2      ; check buffer access
                mov     di, word ptr es:[bx].GIODataPack        ;
                mov     cx, size GSIC_PACKET                    ;
                VerifyAccessW                                   ;
                jc      @@gsi_fail                              ;
                les     di, es:[bx].GIODataPack                 ;
                push    si                                      ;
                mov     si, offset GSIC_Data                    ;
                mov     cx, offset gsic_reserved                ;
                cld                                             ;
            rep movsb                                           ;
                pop     si                                      ;
                xor     ax,ax                                   ;
                ret                                             ;
@@gsi_fail:
                mov     ax,STERR + ERROR_I24_INVALID_PARAMETER  ;
                ret                                             ;
GET_GSIC_INFO   endp                                            ;

OEMHLP_CODE     ends

; ===============================================================
; discardable segment (110h)
; ===============================================================

BOOTHLP_CODE    segment
                public  DEV_INIT                                ;
DEV_INIT        proc    far                                     ;
                assume  ds:DGROUP, es:nothing, ss:nothing       ;
                                                                ;
                mov     ax,word ptr es:[bx].IOpData+2           ; save DevHlp address
                mov     word ptr [DEVHLP+2],ax                  ;
                mov     word ptr PublicInfo.DevHlpFunc+2,ax     ;
                mov     ax,word ptr es:[bx].IOpData             ;
                mov     word ptr [DEVHLP],ax                    ;
                mov     word ptr PublicInfo.DevHlpFunc,ax       ;

                push    ecx                                     ;
                push    edx                                     ;

                test    External.Flags,EXPF_PCI                 ; PCI present?
                jz      DI20                                    ; enable PCI Call ioctl
                mov     IOCTLTable + (PCI_FNNUMBER shl 1),offset PciCall ;
DI20:
                mov     ax,offset EndOfDosHlp                   ; fake end of segment
                mov     es:[bx].InitEcode,ax                    ;
                mov     es:[bx].InitEdata,ax                    ;

                call    IDENTIFY_MACHINE                        ; Initialize MACH_PACKET

                push    es                                      ;
                push    BOOTHLP_DATASEL                         ; machine/bios type
                pop     es                                      ;
                assume  es:_DATA                                ;
                                                                ;
                xor     ax,ax                                   ; NO! no ABIOS! :)
                mov     BIOS_Data.BiosIsAbios,ax                ;
                mov     Machine_ID,ax                           ;
                                                                ;
                mov     ax,es:DHConfigBX                        ;
                mov     cx,ax                                   ;
                xor     al,al                                   ;
                xchg    al,ah                                   ;
                xor     ch,ch                                   ;
                mov     BIOS_Data.BiosModel,ax                  ;
                mov     BIOS_Data.BiosSubModel,cx               ;
                mov     dl,es:DHConfigCH                        ;
                xor     dh,dh                                   ;
                mov     BIOS_Data.BiosRevision,dx               ;
                assume  es:nothing                              ;
                pop     es                                      ;

                mov     ax,External.LowMem                      ;
                mov     MEMORY_Data.LowMemKB,ax                 ;
                movzx   eax,External.HighMem                    ;
                add     eax,External.ExtendMem                  ;
                mov     MEMORY_Data.HighMemKB,eax               ;
                shr     eax,2                                   ; kb -> pages
                mov     MEMORYEX_Data.MiAvailPages,eax          ;

                mov     eax,External.MemPagesLo                 ;
                mov     MEMORYEX_Data.MiLowPages,eax            ;
                mov     eax,External.MemPagesHi                 ;
                mov     MEMORYEX_Data.MiHighPages,eax           ;

                push    edi                                     ;
                mov     ecx,External.LogMapSize                 ; map log buffer
                jecxz   @@DI_NoLogMap                           ; to linear address
                shl     ecx,16                                  ;
                mov     edi,PublicInfo.DosHlpFlatBase           ;
                add     edi,offset External.LogBufPAddr         ;
                mov     eax, VMDHA_PHYS                         ;
                Dev_Hlp VMAlloc                                 ;
                jc      @@DI_NoLogMap                           ;
                mov     External.LogBufVAddr,eax                ;
@@DI_NoLogMap:
                test    External.FlagsEx,EXPFX_NEWDUMP          ;
                jz      @@DI_NoDump                             ;
; too early allocation of such a big buffer is problematic.
; use VMDHA_USEHIGHMEM for the case without EARLYMEMINIT=TRUE
; and then try without this flag, because it deprecated by EARLYMEMINIT
                mov     esi,VMDHA_USEHIGHMEM                    ;
@@DI_DumpBufLoop:
                mov     ecx,OS2DUMP_BUFFER_SIZE                 ;
                mov     edi,PublicInfo.DosHlpFlatBase           ;
                add     edi,offset External.DumpBufPhys         ;
                mov     eax,VMDHA_FIXED or VMDHA_CONTIG         ;
                or      eax,esi                                 ;
                Dev_Hlp VMAlloc                                 ;
                jnc     @@DI_DumpBufOk                          ;
                xor     si,VMDHA_USEHIGHMEM                     ; try 2nd time
                jz      @@DI_DumpBufLoop                        ;
                jmp     @@DI_DumpBufErr                         ;
@@DI_DumpBufOk:
                mov     External.DumpBufLin,eax                 ;
@@DI_DumpBufErr:
                test    byte ptr External.FlagsEx,EXPFX_OHTRACE ; trace on?
                jz      @@DI_NoDump                             ;
                dbg16print <"DumpBuf: %lx %lx",10>,<External.DumpBufPhys,eax> ;
@@DI_NoDump:
                mov     word ptr [pINIT],offset FN_ERR          ; init done
                pop     edi                                     ;
                pop     edx                                     ;
                pop     ecx                                     ;
                xor     ax,ax                                   ;
                ret                                             ;
DEV_INIT        endp

;
; Find identifying string in ROM
; ---------------------------------------------------------------
                public IDENTIFY_MACHINE                         ;
IDENTIFY_MACHINE proc near                                      ;
                assume  ds:DGROUP, es:nothing, ss:nothing       ;
                push    es                                      ;
                push    bx                                      ;

                mov     ax,0Fh                                  ; 32-bit ROM address
                mov     bx,8000h                                ; (F8000)
                mov     cx,16                                   ; length of ROM area
                mov     dh,0                                    ;
                push    ds                                      ;
                pop     es                                      ;
                assume  es:DGROUP                               ;
                push    ds
                Dev_Hlp PhysToVirt                              ;
                cld                                             ;
                mov     cx,4                                    ;
                mov     di,offset MACH_Data.MachOEM             ; copy BIOS data:
            rep movsb                                           ; manufacturer
                mov     cx,7                                    ;
                mov     di,offset MACH_Data.MachModel           ; model
            rep movsb                                           ;
                mov     cx,5                                    ;
                mov     di,offset MACH_Data.MachROM             ; rom version
            rep movsb                                           ;
                pop     ds
                cmp     dword ptr [di],004D4249h                ; check for IBM sign
                jz      @@iim1                                  ;
                mov     cx,size MACH_Packet                     ; fill packet with zero
                mov     di,offset MACH_Data                     ;
                xor     ax,ax                                   ;
                rep     stosb                                   ;
@@iim1:
                pop     bx                                      ; restore packet location
                pop     es                                      ;
                assume  es:nothing                              ;
                ret                                             ;
IDENTIFY_MACHINE endp                                           ;

;
; GetVideoInfo - initialize display code and font table
; ---------------------------------------------------------------
;
                public  GetVideoInfo                            ;
GetVideoInfo    proc    near                                    ;
                assume  ds:DGROUP, es:nothing, ss:nothing       ;
                push    es                                      ;
                pusha                                           ;
                sub     sp,0040h                                ; reserve space
                mov     di,sp                                   ;
                mov     ax,ss                                   ;
                mov     es,ax                                   ; VGA state info
                mov     ax,1B00h                                ;
                xor     bx,bx                                   ;
                int     10h                                     ;
                mov     bl,VGA_COLOR                            ;
                cmp     al,1Bh                                  ; not supported?
                jnz     @@gvi_noinfo                            ;
                mov     bh,es:[di+2Dh]                          ;
                mov     DisplayMisc,bh                          ;
                cmp     byte ptr es:[di+25h],0                  ;
                jz      @@gvi_noinfo                            ;
                mov     bl,es:[di+25h]                          ;
@@gvi_noinfo:
                add     sp,0040h                                ; restore stack
                mov     DisplayCode,bl                          ;
                mov     bl,8                                    ; get 6 fonts
                mov     si,offset FONT_TABLE                    ;
                mov     bh,2                                    ;
@@gvi_loop:
                mov     ax,1130h                                ; font loop
                xor     bp,bp                                   ;
                xor     cx,cx                                   ;
                xor     dx,dx                                   ;
                int     10h                                     ;
                or      bp,bp                                   ; check data
                jz      @@gvi_lend                              ;
                or      cx,cx                                   ;
                jz      @@gvi_lend                              ;
                or      dx,dx                                   ;
                jz      @@gvi_lend                              ;
                mov     [si],bp                                 ; save pointer
                mov     [si+2],es                               ;
                add     si,4                                    ; next entry
                inc     bh                                      ;
                cmp     bh,bl                                   ;
                jnz     @@gvi_loop                              ;
@@gvi_lend:
                mov     bx,0100h                                ; save something to this
                mov     ax,1130h                                ; location for compatibility.
                int     10h                                     ; because of bug in original code
                mov     [si],bp                                 ; this call was performed anywere,
                mov     [si+2],es                               ; not on japan PCs only

                popa                                            ;
                pop     es                                      ;
                ret                                             ;
GetVideoInfo    endp                                            ;

BOOTHLP_CODE    ends
                end
