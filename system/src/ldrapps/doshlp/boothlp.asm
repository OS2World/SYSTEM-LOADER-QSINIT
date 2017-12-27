;
; QSINIT
; doshlp code - discardable part
;

                .586p
                include segdef.inc
                include doshlp.inc
                include ldrparam.inc
                include inc/qstypes.inc
                include inc/basemac.inc
                include inc/seldesc.inc
                include inc/fixups.inc
                include inc/iopic.inc
                include inc/timer.inc
                include inc/debug.inc
                include inc/cpudef.inc
                include inc/filetab.inc

                MSR_IA32_MTRR_DEF_TYPE = 02FFh                  ;
                MTRRS_MTRRON           = 0800h                  ;

DefDiscDosHlp   macro   func                                    ;
                DHP_&func label word                            ;
                dw      DH&func                                 ;
                dw      BOOTHLP_CODESEL                         ;
                endm                                            ;

DefDosHlp       macro   func                                    ;
                DHP_&func label word                            ;
                extrn   DH&func:near                            ;
                dw      DH&func                                 ;
                dw      DOSHLP_CODESEL                          ;
                endm                                            ;

BOOTHLP_DATA    segment
                public  DosHlpFunctions                         ;
DosHlpFunctions label   near                                    ;
                dw              DOSHLPVERSION                   ; Version number - used to verify an os2ldr and os2krnl match
                DefDiscDosHlp   Init                            ; DosHlp Initialization
                DefDosHlp       Reboot                          ; reboot control
                DefDosHlp       NMI                             ; NMI Enabling/Disabling
                DefDiscDosHlp   SizeMem                         ; available memory recognition
                DefDiscDosHlp   Config                          ; Get hardware configuration
                DefDiscDosHlp   BaseDDList                      ; Return a pointer to the base (virtual) device driver list
                DefDiscDosHlp   GetDriveParms                   ; Get Disk Parameters
                DefDosHlp       InitSystemDump                  ; stand alone dump
                DefDosHlp       SystemDump                      ; stand alone dump
                DefDiscDosHlp   ReadSectors                     ; Real mode read sectors
                DefDosHlp       SerInit                         ; Serial Init
                DefDosHlp       SetBaudRate                     ; Serial Set baudrate
                DefDosHlp       SerIn                           ; Serial Input
                DefDosHlp       SerOut                          ; Serial Output
                DefDosHlp       ToneOn                          ; Speaker Control
                DefDosHlp       ToneOff                         ; Speaker Control
                DefDosHlp       GetMask                         ; Mode switch control
                DefDosHlp       SetMask                         ; Mode switch control
                DefDosHlp       SetRealMask                     ; Mode switch control
                DefDosHlp       SetProtMask                     ; Mode switch control
                DefDosHlp       SetDosEnv                       ; Mode switch control
                DefDosHlp       Int10                           ; Character output
                DefDosHlp       ProtGetMessage                  ; For protected mode usage
                DefDiscDosHlp   RealGetMessage                  ; For real mode usage
                DefDiscDosHlp   RegisterTmrDD                   ;
                DefDosHlp       Tmr16QueryTime                  ;
                DefDosHlp       EnableWatchdogNMI               ;
                DefDosHlp       DisableWatchdogNMI              ;
                DefDiscDosHlp   InstallIRET                     ;
                DefDiscDosHlp   Discard                         ;
; ---------------------------------------------------------------
; 32-bit Doshlps
; ---------------------------------------------------------------
                DefDiscDosHlp   InitInterrupts                  ;
                DefDosHlp       SetIRQMask                      ;
                DefDosHlp       SendEOI                         ;
                DefDosHlp       Tmr32QueryTime                  ;
                DefDosHlp       TmrSetRollover                  ;
                DefDiscDosHlp   InitNPX                         ;
                DefDosHlp       ClrBusyNPX                      ;
                DefDosHlp       AckIntNPX                       ;
                DefDosHlp       WaitNPX                         ;
                DefDosHlp       ValidNPXSwitch                  ;
                DefDosHlp       VNPXReset                       ;
                DefDosHlp       VNPXClrBusy                     ;
                DefDosHlp       WhyNMI                          ;
                DefDosHlp       AckNMI                          ;
                DefDosHlp       ResetWatchdogNMI                ;
                DefDosHlp       DisableCache                    ;
                DefDosHlp       FindParity                      ;
                DefDosHlp       EnableCache                     ;
                DefDosHlp       GetErrorLogPtr                  ;
                DefDosHlp       WriteErrorLog                   ;
                DefDosHlp       ReadErrorLog                    ;
                DefDosHlp       ResetParity                     ;
                DefDosHlp       IODelayCalibrate                ;
                DefDiscDosHlp   IODelayFixup                    ;
                DefDiscDosHlp   SetIRQVector                    ;
                DefDiscDosHlp   UnSetIRQVector                  ;
                DefDosHlp       GetIRR                          ;
                DefDosHlp       GetISR                          ;
                DefDosHlp       GetMask32                       ;
                DefDosHlp       SetMask32                       ;
                DefDosHlp       ProtModeRTCRegs                 ;
                DefDosHlp       RealModeRTCRegs                 ;
                DefDosHlp       GetDHPICVars                    ;

BaseDDFileList  label   near
                db      "RESOURCE.SYS",0,1                      ;
                db      "CLOCK01.SYS" ,0,1                      ;
                db      "SCREEN01.SYS",0,1                      ;
                db      "KBDBASE.SYS" ,0,1                      ;
                db      0                                       ;

BaseVDDFileList label   near
                db      "VBIOS.SYS" ,0                          ;
                db      "VDMA.SYS"  ,0                          ;
                db      "VPIC.SYS"  ,0                          ;
                db      "VFLPY.SYS" ,0                          ;
                db      "VTIMER.SYS",0                          ;
                db      "VLPT.SYS"  ,0                          ;
                db      "VKBD.SYS"  ,0                          ;
                db      "VDSK.SYS"  ,0                          ;
                db      "VCMOS.SYS" ,0                          ;
                db      "VNPX.SYS"  ,0                          ;
                db      0                                       ;

StrEOL          db      13,10,0                                 ;
                extrn   DosHlpResEnd:near                       ;
                extrn   RouterDiff:dword                        ;

                public  HardwareInterruptTable                  ;
HardwareInterruptTable label byte                               ; Interrupt table ref
                InterruptTable  < 8, 50H,  8H>                  ; * 0...7 from 50h
                InterruptTable  < 8, 70H, 70H>                  ; * 8..15 from 70h
                InterruptTable  < 0,  0 ,  0 >                  ; * end of table

                public  HardwareInterruptFlags                  ;
HardwareInterruptFlags  label  word                             ;
                dw      0                                       ; 0
                dw      0                                       ; 1
                dw      IRQF_FSYS                               ; slave PIC
                dw      IRQF_FSHARING                           ; 3
                dw      IRQF_FSHARING                           ; 4
                dw      IRQF_FSHARING                           ; 5
                dw      IRQF_FSHARING                           ; 6
                dw      IRQF_FSHARING                           ; 7
                dw      0                                       ; 8
                dw      IRQF_FSHARING                           ; 9
                dw      IRQF_FSHARING                           ; 10
                dw      IRQF_FSHARING                           ; 11
                dw      0                                       ; 12
                dw      0                                       ; 13
                dw      IRQF_FSHARING                           ; 14
                dw      IRQF_FSHARING                           ; 15

                align   2
                public  FixUpTable                              ;
FixUpTable      label   word                                    ;
                dw      offset DHFixup_KJmp                     ;
                dw      offset DHFixup_LdIRQi                   ;
                dw      offset DHFixup_SetDSd                   ;
                dw      offset DHFixup_SetDSw                   ;
                dw      offset DHFixup_PushPS                   ;
                dw      offset DHFixup_PushHS                   ;
                dw      offset DHFixup_PushSS                   ;
                dw      offset DHFixup_SetDSd                   ;
FixUpTableEnd   label   word                                    ;

                public  readbuf_ptr                             ;
readbuf_ptr     dw      0                                       ;
gdt_limit       dw      0                                       ;
gdt_base        dd      0                                       ;
BOOTHLP_DATA    ends

DOSHLP_CODE     segment                                         ;
                extrn   int13check:near                         ;
                extrn   GetRTCRegs:near                         ;
                extrn   GenericMessage:near                     ;
                extrn   DHInternalError:near                    ;
                extrn   _printf16:near                          ;
DOSHLP_CODE     ends                                            ;

OEMHLP_CODE     segment
                extrn   OEMHLPStrategy:near                     ;
OEMHLP_CODE     ends

OEMHLP_DATA     segment
                extrn   AdaptInfo:byte                          ;
                extrn   PCIConfigAL:byte                        ;
                extrn   PCIConfigBX:word                        ;
                extrn   PCIConfigCL:byte                        ;
OEMHLP_DATA     ends

DOSHLP32_CODE   segment                                         ;
                extrn   DHIRQ0Router:near                       ;
                extrn   DHIRQ0Jump:near                         ;
                extrn   CommonIRQWarp:near                      ;
                extrn   CommonIRQRouter:near                    ;
DOSHLP32_CODE   ends                                            ;

LASTSEG         segment                                         ;
                extrn   EndOfDosHlp:near                        ;
LASTSEG         ends                                            ;

_BSS            segment
                public  DHConfigAX,DHConfigBX,DHConfigCH        ;
                public  DHConfigCL,DHConfigDH,DHConfigDL        ;
DHConfigAX      dw      ?                                       ; system config data
DHConfigBX      dw      ?                                       ;
DHConfigCH      db      ?                                       ;
DHConfigCL      db      ?                                       ;
DHConfigDH      db      ?                                       ;
DHConfigDL      db      ?                                       ;
                public  Io32FixupTable                          ;
Io32FixupTable  dd      FIXUP_STORE_TABLE_SIZE dup (?)          ;
                public  MsrSetupTable                           ;
MsrSetupTable   db      MSR_SETUP_TABLE_SIZE * size msr_setup_entry dup (?)

irq_router      dd      ?                                       ; FLAT pointer to IRQ routers
airqi           dd      ?                                       ; kernel airqi
ksi             krnl_start_info <>                              ; start info struct
                extrn   _BootBPB:dword                          ;
                extrn   _driveparams:byte                       ;
                extrn   _dd_bootdisk:byte                       ;
                extrn   dd_bootflags:byte                       ;
_BSS            ends

DOSHLP_DATA     segment
                extrn   External:ExportFixupData                ;
                extrn   PublicInfo:LoaderInfoData               ;
                extrn   KAT:KernelAccess                        ;
                extrn   RM_PICMask:dword                        ;
                extrn   RM_RTCRegs:dword                        ;
                extrn   TimerRollover:qw_rec                    ;
                extrn   Timer:qw_rec                            ;
                extrn   SystemIRQMasks:dword                    ;

                public  FLAT_PSDSel,FLAT_HSSel,FLAT_SSSel
FLAT_PSDSel     dw      ?                                       ; FLAT PSD SS
FLAT_HSSel      dw      ?                                       ; FLAT interrupt SS
FLAT_SSSel      dw      ?                                       ; FLAT SS

DOSHLP_DATA     ends

DOSHLP32_DATA   segment
                extrn KAT:KernelAccess                          ;
DOSHLP32_DATA   ends

_TEXT           segment
                extrn   _VesaDetect:near                        ;
_TEXT           ends

_DATA           segment
                extrn   _VesaInfo:near                          ;
                extrn   _useAFio:byte                           ;
                extrn   _i13ext_table:byte                      ;
_DATA           ends

BOOTHLP_CODE    segment
                extrn   GetVideoInfo:near                       ;
                extrn   DHReadSectors:far                       ;
;
; DHBaseDDList - Returns the list of Base Device Drivers
; ---------------------------------------------------------------
; Exit:   ES:DI -> list of base device drivers
;         ES:SI -> list of base virtual device drivers
                assume  cs:DGROUP,ds:nothing,es:nothing,ss:nothing
                public  DHBaseDDList
DHBaseDDList    proc    far
                push    BOOTHLP_DATASEL                         ;
                pop     es                                      ;
                mov     di, offset BaseDDFileList               ;
                mov     si, offset BaseVDDFileList              ;
                ret                                             ;
DHBaseDDList    endp
;
; ---------------------------------------------------------------
;
                assume  cs:DGROUP,ds:nothing,es:nothing,ss:nothing
                public  DHDiscard
DHDiscard       proc    far
                push    ds                                      ;
                push    DOSHLP_DATASEL                          ;
                pop     ds                                      ;
                assume  ds:DOSHLP_DATA                          ;
                or      External.Flags,EXPF_DISCARDED           ;
                assume  ds:nothing                              ;
                pop     ds                                      ;
                ret                                             ;
DHDiscard       endp
;
; ---------------------------------------------------------------
                public  DHInitNPX                               ;
DHInitNPX       proc    near                                    ;
                xor     ax, ax                                  ; 387 forewer ;)
                ret                                             ;
DHInitNPX       endp                                            ;
;
; ---------------------------------------------------------------
                public  DHSizeMem
DHSizeMem       proc    far
                assume  cs:BOOTHLP_CODE,ds:DGROUP,es:nothing,ss:nothing
                push    ds                                      ;
                mov     ax,DOSHLP_CODESEL                       ;
                mov     ds,ax                                   ;
                mov     edx,External.ExtendMem                  ;
                mov     bx,External.HighMem                     ;
                mov     ax,External.LowMem                      ;
                pop     ds                                      ;
                ret                                             ;
DHSizeMem       endp
;
; ---------------------------------------------------------------
                Public  DHConfig
DHConfig        proc    far                                     ;
                assume  cs:DGROUP,ds:nothing,es:nothing,ss:nothing
                mov     ax,cs:DHConfigAX                        ; Get data saved
                mov     bx,cs:DHConfigBX                        ; at init stage
                mov     ch,cs:DHConfigCH                        ;
                mov     cl,cs:DHConfigCL                        ;
                mov     dh,cs:DHConfigDH                        ;
                mov     dl,cs:DHConfigDL                        ;
                ret                                             ;
DHConfig        endp
;
; ---------------------------------------------------------------
                public  DHIODelayFixup
DHIODelayFixup  proc    far
                assume  cs:DGROUP,ds:nothing,es:nothing,ss:nothing
                SaveReg <ds,cx,si,edi>                          ; preserve registers
                push    es                                      ; FLAT selector
                push    BOOTHLP_DATASEL                         ;
                pop     ds                                      ;
                push    DOSHLP_DATASEL                          ;
                pop     es                                      ;
                mov     es:External.IODelay, ax                 ;
                mov     si,offset IOFixB                        ;
@@iodfix_loop:
                cmp     si,offset IOFixE                        ; fixuping IODelay
                jz      @@iodfix_table32                        ; both in resident and
                mov     di,[si]                                 ; discardable parts
                add     si,2                                    ;
                cmp     di,offset DosHlpResEnd                  ;
                jc      @@iodfix_res                            ;
                mov     [di],ax                                 ; discardable
                add     di,2                                    ;
                jmp     @@iodfix_loop                           ;
@@iodfix_res:
                stosw                                           ; resident
                jmp     @@iodfix_loop                           ;
@@iodfix_table32:
                lea     si,Io32FixupTable                       ;
                mov     cx,es:External.Io32FixupCnt             ;
                pop     es                                      ; es:FLAT
                jcxz    @@iodfix_exit                           ;
@@iodfix_loop32:
                mov     edi,[si]                                ;
                stos    word ptr [edi]                          ;
                add     si,4                                    ;
                loop    @@iodfix_loop32                         ;
@@iodfix_exit:
                RestReg <edi,si,cx,ds>                          ;
                ret                                             ;
DHIODelayFixup  endp
;
; ---------------------------------------------------------------
                public  DHGetDriveParms
DHGetDriveParms proc    far
                assume  cs:DGROUP,ds:nothing,es:nothing,ss:nothing
                push    dx                                      ;
                push    cs                                      ;
                pop     ds                                      ;
                test    dl,80h                                  ; hard disk?
                jz      @@dhdp_copy                             ;
                and     dl,1                                    ;
                add     dl,2                                    ; drives 0,1,80h,81h
@@dhdp_copy:
                mov     si,offset _driveparams                  ;
                xor     dh,dh                                   ; index*10
                shl     dl,1                                    ; (sizeof driveparams)
                add     si,dx                                   ;
                shl     dl,2                                    ;
                add     si,dx                                   ;
                pop     dx                                      ;
                ret                                             ;
DHGetDriveParms endp
;
; DHInit - Initialize DosHlp
; ---------------------------------------------------------------
                public  DHInit
DHInit          proc    far
                push    cx                                      ;
                push    ds                                      ;
                push    si                                      ;
                push    ds                                      ;
                mov     ax,DOSHLP_SEG                           ;
                mov     es,ax                                   ;
                mov     ds,ax                                   ;
                assume  ds:DOSHLP_DATA,es:DOSHLP_DATA           ;
                push    eax                                     ;
                CallF16 DOSHLP_CODESEL,DHGetMask                ; get mask
                mov     RM_PICMask, eax                         ;
                lea     di, RM_RTCRegs                          ;
                CallF16 DOSHLP_CODESEL,GetRTCRegs               ;
                pop     eax                                     ;
                pop     ds                                      ;
                mov     di,offset KAT                           ;
                mov     cx,size KernelAccess/2                  ;
                cld                                             ;
                rep     movsw                                   ;
                mov     ax,MSG_SYSINIT_BANNER                   ;
                push    cs                                      ;
                call    near ptr DHRealGetMessage               ; get message
                xor     ax,ax                                   ;
                test    es:External.Flags,EXPF_PCI              ;
                setnz   al                                      ; set PCI bus bit, the only
                shl     al,4                                    ; one actual for us ;)
                or      es:External.Flags,EXPF_DHINIT           ;
                mov     di,si                                   ; message offset
                mov     si,ds                                   ;
                cmp     si,DOSHLP_SEG                           ;
                jz      @@dhi_resmsg                            ;
                mov     si,BOOTHLP_DATASEL                      ;
@@dhi_resmsg:
                mov     es,si                                   ; replace segment to selector
                pop     si                                      ;
                pop     ds                                      ;
                assume  ds:nothing,es:nothing                   ;
                pop     cx                                      ;
                ret                                             ;
DHInit          endp
;
; ---------------------------------------------------------------
                public  DHRegisterTmrDD
DHRegisterTmrDD proc    far
                assume  cs:DGROUP,ds:nothing,es:nothing,ss:nothing
                mov     di,DOSHLP_DATASEL                       ;
                mov     bx,offset TimerRollover                 ;
                mov     cx,offset Timer                         ;
                mov     eax,TIMERFREQ                           ;
                ret                                             ;
DHRegisterTmrDD endp
;
; ---------------------------------------------------------------
DHIRET          proc    near                                    ;
                iretd                                           ;
DHIRET          endp                                            ;
;
; DHInstallIRET - install iret handlers for hardware interrupts
; ---------------------------------------------------------------
                public  DHInstallIRET
DHInstallIRET   proc    far                                     ;
                push    ds                                      ;
                push    fs                                      ;
                pusha                                           ;
                mov     ax,DOSHLP_DATASEL                       ;
                mov     ds,ax                                   ;
                assume  ds:DOSHLP_DATA                          ;
                mov     ax,KAT.IDTSel                           ;
                mov     fs,ax                                   ; get access to IDT
                mov     ax,cs                                   ;
                mov     ds,ax                                   ;
                assume  ds:DGROUP                               ;
                lea     si, HardwareInterruptTable              ;
                shl     eax,16                                  ;
                mov     ax,offset DHIRET                        ; handler pointer
                xor     bx,bx                                   ;
@@dhir_loop:
                movzx   cx,[si].ITCount                         ;
                jcxz    @@dhir_exit                             ; last block?
                movzx   di,[si].ITHVector                       ;
                shl     di,3                                    ; offset in IDT
@@dhir_picloop:
                mov     fs:[di].g_handler,eax                   ; IDT setup
                mov     fs:[di].g_extoffset,bx                  ;
                mov     fs:[di].g_access,D_INT032               ;
                add     di,size gate_s                          ;
                loop    @@dhir_picloop                          ; next IRQ
                add     si,size InterruptTable                  ;
                jmp     @@dhir_loop                             ; next block
@@dhir_exit:
                popa                                            ;
                pop     fs                                      ;
                pop     ds                                      ;
                ret                                             ;
DHInstallIRET   endp
;
; Return message string (real mode)
; ---------------------------------------------------------------
; IN:  ax - message index
; OUT: ds:si - message string
;
                public  DHRealGetMessage                        ;
DHRealGetMessage proc far                                       ;
                assume  cs:nothing,ds:DOSHLP_DATA,es:nothing,ss:nothing
                ;dbg16print <"DHRealGetMessage: %x",10>,<ax>     ;
                push    cx                                      ;
                mov     si,DOSHLP_SEG                           ;
                mov     ds,si                                   ;
                mov     cx,ax                                   ;
                mov     si,External.ResMsgOfs                   ;
                cld
@@dhgmr_loop:
                lodsw                                           ; msg id
                or      ax,ax                                   ;
                jz      @@dhgmr_loopend                         ; end of list?
                cmp     ax,cx                                   ;
                jz      @@dhgmr_done                            ; found?
                lodsw                                           ;
                add     si,ax                                   ; msg size
                inc     si                                      ; term. nul
                jmp     @@dhgmr_loop                            ;
@@dhgmr_done:
                mov     ax,cx                                   ;
                pop     cx                                      ;
                ret                                             ;
@@dhgmr_loopend:
                mov     ax,cs                                   ; second pass?
                mov     si,ds                                   ;
                cmp     ax,si                                   ;
                jz      @@dhgmr_notfound                        ; no message
                mov     si,External.DisMsgOfs                   ;
                mov     ds,ax                                   ; go to second
                jmp     @@dhgmr_loop                            ; pass
@@dhgmr_notfound:
                mov     ax,cx                                   ;
                CallF16 DOSHLP_SEG,GenericMessage               ; SYSXXXXX!!
                pop     cx
                ret
DHRealGetMessage endp                                           ;

;
; stub for disk read code from qsinit
; ---------------------------------------------------------------
                public  panic_initpm
panic_initpm    label   near
                mov     ax,MSG_SYSINIT_BOOT_ERROR
; ---------------------------------------------------------------
ErrExit         label   near
                assume  cs:DGROUP,ds:DGROUP,es:nothing,ss:nothing
                push    ax                                      ;
                push    cs                                      ;
                pop     ds                                      ;
                mov     si,offset StrEOL                        ; eol
                call    ErrPrt                                  ;
                pop     ax                                      ;
                assume  ds:nothing                              ;
                push    cs                                      ;
                call    near ptr DHRealGetMessage               ; message address in DS:SI
                inc     si                                      ; skip message length
                inc     si                                      ;
                call    ErrPrt                                  ; print it
                mov     ax,MSG_SYSINIT_INSER_DK                 ;
                push    cs                                      ;
                call    near ptr DHRealGetMessage               ;
                inc     si                                      ;
                inc     si                                      ;
                call    ErrPrt                                  ;
@@ex_halt:
                sti                                             ; waiting for CAD
                hlt                                             ;
                jmp     @@ex_halt                               ;
; ---------------------------------------------------------------
ErrPrt          proc    near                                    ;
                cld                                             ;
@@ep_loop:
                lodsb                                           ;
                or      al,al                                   ;
                jz      @@ep_err                                ;
                mov     ah,14                                   ;
                mov     bx,7                                    ;
                int     10h                                     ;
                jmp     @@ep_loop                               ;
@@ep_err:
                ret                                             ;
ErrPrt          endp

;
; init1 call
; ---------------------------------------------------------------
; This is first initialization call of doshlp.
; Doshlp is not splitted at this time and messages is not loaded.
; Procedure checks some hardware parameters and saves it into
; both parts (doshlp and boothlp).
;
                public  GetSysConfig
GetSysConfig    proc    far
                assume  cs:DGROUP,ds:DGROUP,es:nothing,ss:nothing
                int     11h                                     ; hardware config
                mov     bx,ax                                   ;
                mov     ax,cs                                   ;
                mov     ds,ax                                   ;
                xor     ax,ax                                   ;
                test    bl,1                                    ; no disks?
                jz      @@gsc_noflop                            ;
                mov     al,bl                                   ;
                shr     al,6                                    ; number of floppies
                inc     al                                      ;
@@gsc_noflop:
                or      al,080h                                 ; FPU present
                mov     ah,bh                                   ;
                and     ah,0CEh                                 ;
                shr     ah,1                                    ;
                mov     DHConfigAX,ax                           ; COM + LPTs
                mov     DHConfigCL,1                            ; always color

                mov     ax,0B101h                               ; is PCI BIOS present?
                push    ds                                      ;
                int     1Ah                                     ;
                pop     ds                                      ;
                jc      @@gsc_nopci                             ; no, don't set flags
                cmp     edx,020494350h                          ;
                jne     @@gsc_nopci                             ;
                or      External.Flags, EXPF_PCI                ; set PCI flag
                mov     PCIConfigAL,al                          ; and save
                mov     PCIConfigBX,bx                          ; parameters
                mov     PCIConfigCL,cl                          ;
@@gsc_nopci:
                mov     ah,0C0h                                 ; get machine model
                push    ds                                      ;
                int     15h                                     ;
                pop     ds                                      ;
                jc      @@gsc_nomachdata                        ;

                test    byte ptr es:[bx+5],4                    ; byte 5 bit 2 tells if ebda exists ?
                jz      @@gsc_noxbda                            ; xBDALen already initialised to 0
                push    es                                      ;
                mov     ax,ROM_DATASEG                          ; BIOS Data area segment
                mov     es,ax                                   ;
                mov     ax,es:[0Eh]                             ; EXT BIOS segment at 40:0e
                mov     es,ax                                   ;
                mov     al,es:[0]                               ; number of Kbytes in EBDA
                xor     ah,ah                                   ;
                shl     ax,10                                   ; number of bytes in EBDA
                pop     es                                      ;
                mov     External.xBDALen,ax                     ;
@@gsc_noxbda:
                mov     ah,es:[bx+2]                            ; machine model
                mov     al,es:[bx+3]                            ; type
                mov     ch,es:[bx+4]                            ; and BIOS level
                jmp     @@gsc_fill                              ;
@@gsc_nomachdata:
                mov     ax,0F000h                               ; Int 15H did not work - get the machine model from the ROM
                mov     es,ax                                   ;
                mov     ah,es:0FFFEh                            ; pick up model byte
                xor     al,al                                   ; set type to 0
                xor     ch,ch                                   ; set BIOS level to 0
@@gsc_fill:
                mov     DHConfigBX,ax                           ; store model & submodel for DHConfig
                mov     DHConfigCH,ch                           ; save BIOS revision level

                mov     eax,1                                   ; CPUID
                cpuid                                           ;
                sub     ah, 2                                   ; OS/2 processor code
                and     ah, 07h                                 ; mask all but family bits
                or      ah, 10h                                 ; FPU present
                mov     DHConfigDH,ah                           ;

                mov     ah,8                                    ; get number of hard disks
                mov     dl,80h                                  ;
                int     13h                                     ;
                jnc     @@gsc_hddok                             ;
                mov     dl,0                                    ; no hard disks.
@@gsc_hddok:
                mov     DHConfigDL,dl                           ;

                mov     ax,offset EndOfDosHlp                   ; disk read buffer
                add     ax,SECTMASK                             ; for one sector
                and     ax,not SECTMASK                         ;
                mov     readbuf_ptr,ax                          ;

                push    cs                                      ; (re)init debug port
                call    DHSerInit                               ;

                call    IODelayInit                             ; init IODelay
                call    int13check                              ; get drive parameters
                call    GetVideoInfo                            ; get fonts

                test    PublicInfo.BootFlags,BOOTFLAG_EDISK     ; emulated disk?
                jnz     @@gsc_skipchs                           ;
                test    dd_bootflags,BF_NOMFSHVOLIO or BF_RIPL  ; no disk i/o?
                jnz     @@gsc_skipchs                           ;

                mov     bl,_dd_bootdisk                         ; select CHS/LBA
                btr     bx,7                                    ; for disk i/o
                jnc     @@gsc_skipchs                           ; floppy?
                xor     bh,bh                                   ;
                and     _i13ext_table[bx],0FEh                  ; drop flag
                test    PublicInfo.BootFlags,BOOTFLAG_CHS       ; CHS?
                jnz     @@gsc_skipchs                           ;
                test    _i13ext_table[bx],4                     ; i13x supported?
                jz      @@gsc_skipchs                           ;
                or      _i13ext_table[bx],1                     ; yes, enable
@@gsc_skipchs:
                test    PublicInfo.BootFlags,BOOTFLAG_NOAF      ;
                setz    _useAFio                                ;
                test    PublicInfo.BootFlags,BOOTFLAG_NOLOGO    ;
                jnz     @@gsc_skipvesa                          ;
                mov     ax,External.Buf32k                      ;
                call    _VesaDetect                             ; detecting vesa
@@gsc_skipvesa:
                mov     PublicInfo.VesaInfoOffset,offset _VesaInfo
                mov     PublicInfo.PrintFOffset,offset _printf16
                ret                                             ;
GetSysConfig    endp
;
; store initial IODelay values (it filled by zero by default)
; ---------------------------------------------------------------
                public  IODelayInit
IODelayInit     proc    near                                    ;
                assume  cs:DGROUP,ds:DGROUP,es:nothing,ss:nothing
                mov     ax,cs                                   ;
                mov     ds,ax                                   ;
                mov     es,ax                                   ;

                mov     si, offset IOFixB                       ;
                mov     ax, External.IODelay                    ;
@@iodinit_loop:
                cmp     si, offset IOFixE                       ;
                jz      @@iodinit_done                          ;
                mov     di,[si]                                 ;
                stosw                                           ;
                add     si,2                                    ;
                jmp     @@iodinit_loop                          ;
@@iodinit_done:
                ret                                             ;
IODelayInit     endp                                            ;

PrepareOS2DUMP  proc    near                                    ;
                db      9Ah                                     ;
                dd      6                                       ;
                ret                                             ;
PrepareOS2DUMP  endp                                            ;

PrepareOS2DBCS  proc    near                                    ;
                db      9Ah                                     ;
                dd      0                                       ;
                ret                                             ;
PrepareOS2DBCS  endp                                            ;

SetDBCSFont     proc    near                                    ;
                push    1                                       ;
                push    External.DbcsFontSeg                    ;
SetDBCSFontCmd  label   near
                db      9Ah                                     ;
                dd      3                                       ;
                pop     eax                                     ; +4
                ret                                             ;
SetDBCSFont     endp                                            ;

;
; init2 routine
; ---------------------------------------------------------------
; Finalizing init and going into kernel.
; Loader already divided here, but high part was placed in disk
; buffer and need to be moved to the actual location.
;
                public  DHStubInit                              ;
DHStubInit      proc    far                                     ; assume can`t help us here
                assume  cs:nothing,ds:DOSHLP_DATA,es:nothing,ss:nothing
                push    DOSHLP_SEG                              ;
                pop     ds                                      ; point DS to doshlp

                mov     al,External.KernelVersion               ;
                or      al,al                                   ;
                jz      @@boothlp_no_version                    ;
                mov     dl,10                                   ;
                xor     ah,ah                                   ;
                div     dl                                      ;
                add     ax,3030h                                ; update oemhlp
                mov     word ptr [AdaptInfo+23],ax              ; version string

                cmp     al,34h                                  ; >= 4.0
                jnc     @@boothlp_no_version                    ; 10 for Warp3
                dec     word ptr cs:DosHlpFunctions             ;
@@boothlp_no_version:
                mov     ax,External.DumpSeg                     ; init system dump
                or      ax,ax                                   ;
                jz      @@boothlp_no_os2dump                    ;
                mov     word ptr DHInitSystemDump+3,ax          ; segment
                mov     word ptr DHSystemDump+3,ax              ;
                mov     word ptr cs:PrepareOS2DUMP+3,ax         ; and dump itself
                call    PrepareOS2DUMP                          ;
@@boothlp_no_os2dump:
                mov     ax,External.DBCSSeg                     ;
                or      ax,ax                                   ;
                jz      @@boothlp_no_os2dbcs                    ;
                mov     word ptr cs:PrepareOS2DBCS+3,ax         ;
                mov     word ptr cs:SetDBCSFontCmd+3,ax         ;
                push    ax                                      ;
                call    PrepareOS2DBCS                          ;
                call    SetDBCSFont                             ;
                pop     fs                                      ;
                mov     eax,fs:[6]                              ;
                mov     External.SavedInt10h,eax                ;
@@boothlp_no_os2dbcs:
                fninit                                          ; init FPU
                fninit                                          ;

                mov     eax,cr4                                 ; wipe QSINIT paging
                and     eax,not (CPU_CR4_PAE or CPU_CR4_PGE)    ; to not confuse
                mov     cr4,eax                                 ; feature users ;)
                xor     eax,eax                                 ;
                mov     cr3,eax                                 ;

                test    External.Flags,EXPF_COMINITED           ; need to
                jnz     @@boothlp_com_ready                     ; reinit COM port
                test    PublicInfo.DebugPort,0FFFFh             ; again?
                jz      @@boothlp_com_ready                     ;
                CallF16 DOSHLP_CODESEL,DHSerInit                ;
@@boothlp_com_ready:
                mov     cx,External.PushKey                     ; push key
                jcxz    @@boothlp_nokeypush                     ;
                mov     ah,5                                    ;
                push    ds                                      ;
                int     16h                                     ;
                pop     ds                                      ;
@@boothlp_nokeypush:
                mov     es,External.DisPartSeg                  ;
                movzx   ecx,External.DisPartLen                 ;
                mov     di,cs                                   ; change stack to
                mov     ss,di                                   ; this segment
                lea     esp,[ecx-PAGESIZE]                      ;
                xor     si,si                                   ; replace qsinit to
                xor     di,di                                   ; boothlp (this code)
                cld                                             ;
            rep movs    byte ptr es:[di],byte ptr cs:[si]       ; no return possible below this point!
                push    es                                      ;
                push    offset @@boothlp_9x00                   ; jump to final
                retf                                            ; location
@@boothlp_9x00:
                mov     ax,es                                   ;
                mov     ss,ax                                   ; update ss too
                push    ds                                      ;
                pop     es                                      ;
                mov     ds,ax                                   ; ss=cs=ds here
                assume  ds:_BSS,es:DOSHLP_DATA                  ; es=doshlp

                mov     DHP_Init+2,ax                           ; setup segments
                mov     DHP_SizeMem+2,ax                        ; for RM doshlp calls
                mov     DHP_Config+2,ax                         ;
                mov     DHP_GetDriveParms+2,ax                  ;
                mov     DHP_ReadSectors+2,ax                    ;
                mov     DHP_RealGetMessage+2,ax                 ;

                mov     word ptr ksi.ki_pArenaInfo,sp           ; start of arena list
                mov     word ptr ksi.ki_pArenaInfo+2,ax         ; arena segment
                mov     word ptr ksi.ki_pBPB,offset _BootBPB    ;
                mov     word ptr ksi.ki_pBPB+2,ds               ; bpb

                mov     ax,word ptr _dd_bootdisk                ; boot disk & flags
                mov     ksi.ki_Drive,al                         ;
                mov     ksi.ki_BootFlags,ah                     ;

                mov     ksi.ki_DosHlpSeg,es                     ; doshlp segment
                mov     ax,es:External.DisPartOfs               ;
                mov     ksi.ki_DosHlpLen,ax                     ;
                mov     word ptr ksi.ki_DosHlpAddr,offset DosHlpFunctions
                mov     word ptr ksi.ki_DosHlpAddr+2,ds         ;
                mov     word ptr ksi.ki_OEMDDStrat,offset OEMHLPStrategy
                mov     word ptr ksi.ki_OEMDDStrat+2,es         ;

                xor     cx,cx                                   ;
                mov     fs,cx                                   ;
                mov     gs,cx                                   ; anyone still
                test    es:External.Flags,EXPF_TWODSKBOOT       ; really need this? ;)
                setnz   cl                                      ;
                mov     bx,offset ksi                           ;

                call    es:External.OS2Init                     ; YES!
                mov     ax,MSG_SYSINIT_SYS_STOPPED              ;
                jmp     ErrExit                                 ; should never occur
DHStubInit      endp

BOOTHLP_CODE    ends


BOOTHLP32_CODE  segment
                assume  cs:nothing,ds:nothing,es:nothing,ss:nothing
;
; Initialize interrupt hardware, and system data
; ---------------------------------------------------------------
; IN:  DS - FLAT data, EAX - offset of airqi, BX - PSD stack selector,
;      CX - interrupt stack selector, DX - 32 bit code stack selector
; OUT: AX - IRQ for NPX, BX - number of IRQs in system
;
; note: this call supports Warp kernel, but does not support Warp Server
; SMP (it requires another one IRQ router and its irqi size == 16 as in Aurora)
;
                public  DHInitInterrupts
DHInitInterrupts proc   near                                    ;
                SaveReg <es,fs,gs>                              ;
                SaveReg <esi,edi,ecx,edx,ebp>                   ; we can destroy all regs ;)
                mov     si,DOSHLP_DATASEL                       ; 108 -> es
                mov     es,si                                   ;
                assume  es:DGROUP                               ;
                mov     si,BOOTHLP_DATASEL                      ; 118 -> gs
                mov     gs,si                                   ;
                assume  gs:_DATA                                ;
                mov     gs:airqi,eax                            ;
                mov     es:PublicInfo.FlatCS,cs                 ; saving selectors
                mov     es:PublicInfo.FlatDS,ds                 ;

                mov     ebp,size irqi                           ;
                test    es:PublicInfo.BootFlags,BOOTFLAG_WARPSYS ; Warp kernel?
                jz      @@dhii_w45call                          ;
                xor     cx,cx                                   ; invalid values
                xor     dx,dx                                   ; on Warp
                mov     ebp,size irqi3                          ;
                test    es:PublicInfo.BootFlags,BOOTFLAG_SMP    ;
                jnz     @@dhii_w45call                          ;
                xor     bx,bx                                   ;
@@dhii_w45call:
                mov     es:FLAT_HSSel,cx                        ;
                mov     es:FLAT_SSSel,dx                        ;
                mov     es:FLAT_PSDSel,bx                       ;
@@dhii_callfix:
                call    DHFixup                                 ;
                lea     ebx,HardwareInterruptFlags              ; filling IDT
                lea     esi,HardwareInterruptTable              ; <Count, pmVector, rmVector>
                mov     edx,gs:airqi                            ;
                mov     eax,es:PublicInfo.DosHlpFlatBase        ;
                add     eax,offset DHIRQ0Router                 ; first router
                mov     gs:irq_router,eax                       ;
                mov     fs,es:KAT.IDTSel                        ; IDT selector
@@dhii_tableloop:
                movzx   ecx,byte ptr gs:[esi].ITCount           ;
                or      ecx,ecx                                 ;
                jz      @@dhii_tableend                         ;
                movzx   edi,byte ptr gs:[esi].ITHVector         ;
                shl     edi,3                                   ; offset in IDT
@@dhii_irqloop:
                mov     eax,gs:irq_router                       ; IRQ router base
                mov     word ptr fs:[edi].g_handler,ax          ; setup IDT entry
                shr     eax,16                                  ;
                mov     fs:[edi].g_extoffset,ax                 ;
                mov     ax,es:PublicInfo.FlatCS                 ;
                mov     word ptr fs:[edi].g_handler+2,ax        ;
                mov     fs:[edi].g_access,D_INT032              ;
                add     edi,size gate_s                         ; next entry
                add     gs:irq_router,IRQR_SIZE                 ;

                mov     ax,gs:[ebx]                             ; get flags
                mov     [edx].irqi_usFlags,ax                   ;
                test    [edx].irqi_usFlags,IRQF_FSYS            ; system IRQ?
                jz      @@dhii_nonsys                           ;
                movzx   eax,[edx].irqi_usIRQNum                 ; collect and set mask
                bts     es:SystemIRQMasks, eax                  ;
                push    ebx                                     ;
                mov     ebx,es:PublicInfo.DosHlpFlatBase        ;
                add     ebx,offset DHSetIRQMask                 ;
                call    ebx                                     ;
                pop     ebx                                     ;
@@dhii_nonsys:
                inc     ebx                                     ; next HardwareInterruptFlags entry
                inc     ebx                                     ;
                add     edx,ebp                                 ; next entry
                dec     ecx                                     ;
                jnz     @@dhii_irqloop                          ;
                add     esi,size InterruptTable                 ; next table entry
                jmp     @@dhii_tableloop                        ;
@@dhii_tableend:
                mov     eax,es:PublicInfo.BootFlags             ;
                and     eax,BOOTFLAG_WARPSYS or BOOTFLAG_SMP    ; check for pure
                cmp     eax,BOOTFLAG_WARPSYS                    ; Warp kernel type
                mov     bx,10h                                  ; (non-SMP)
                jz      @@dhii_noHighIrqs                       ;
                mov     cx,20h                                  ; 32 high irqs
@@dhii_highloop:
                mov     [edx].irqi_usFlags, IRQF_FSHARING       ; share all high IRQs
                add     edx,ebp                                 ;
                loop    @@dhii_highloop                         ;
                test    es:PublicInfo.BootFlags,BOOTFLAG_WARPSYS ; Warp kernel?
                jnz     @@dhii_noPSD_patch                      ;

                sgdt    fword ptr gs:gdt_limit                  ; patch PSD stack sel to
                mov     edi,gs:gdt_base                         ; fix error in ACPI.PSD stack
                mov     ax,gs:gdt_limit                         ; allocation code
                movzx   ecx,es:FLAT_PSDSel                      ; else we get #8 in PSD_INSTALL
                cmp     ax,cx                                   ;
                jc      @@dhii_noPSD_patch                      ; invalid PSDSel?

                mov     al,[edi+ecx].d_access                   ; check seg type
                and     al,D_EXPDN or D_PRES or D_W             ; exp. down writable
                cmp     al,D_EXPDN or D_PRES or D_W             ;
                jnz     @@dhii_noPSD_patch                      ;
                mov     al,[edi+ecx].d_attr                     ;
                and     al,D_EXTLIMIT                           ;
                shl     eax,16                                  ; limit must be equal
                mov     ax,[edi+ecx].d_limit                    ; to MPDATAFLAT
                inc     eax                                     ;
                shl     eax,PAGESHIFT                           ;
                cmp     eax,MPDATAFLAT                          ; wasm make signed shr :(
                jnz     @@dhii_noPSD_patch                      ;
                mov     ax,[edi+ecx].d_loaddr                   ;
                or      al,[edi+ecx].d_hiaddr                   ; base must be 0
                or      al,[edi+ecx].d_extaddr                  ;
                or      ax,ax                                   ;
                jnz     @@dhii_noPSD_patch                      ;
                mov     [edi+ecx].d_limit,ax                    ; patch to 0xF0000000+page
@@dhii_noPSD_patch:
                mov     bx,30h                                  ; total irqs
@@dhii_noHighIrqs:
                mov     ax,IRQ_NPX                              ; NPX

                test    es:PublicInfo.BootFlags,BOOTFLAG_WARPSYS ; Warp kernel?
                jz      @@dhii_w45router
                movzx   ecx,bx                                  ; replace common router
                mov     esi,offset DHIRQ0Jump + 1               ; for Warp kernel
                add     esi,es:PublicInfo.DosHlpFlatBase        ;
                mov     edx,gs:RouterDiff                       ;
@@dhii_rpatchloop:
                add     [esi],edx                               ;
                add     esi,IRQR_SIZE                           ;
                loop    @@dhii_rpatchloop                       ;
@@dhii_w45router:
                assume  es:nothing,gs:nothing                   ;
                RestReg <ebp,edx,ecx,edi,esi>                   ;
                RestReg <gs,fs,es>                              ;
                ret                                             ;
DHInitInterrupts endp
;
; Setup IRQ vector in IDT
;----------------------------------------------------------------
; IN: ECX = irq, EDX = vector

                public  DHSetIRQVector
DHSetIRQVector  proc    near                                    ;
                SaveReg <ebx,ecx,esi,edi,fs,gs>                 ;

                mov     ax,DOSHLP_DATASEL                       ;
                mov     fs,ax                                   ;
                assume  fs:DOSHLP_DATA                          ;
                mov     gs,fs:KAT.IDTSel                        ; IDT data
                mov     edi,edx                                 ; vector setup
                shl     edi,3                                   ;
                mov     eax,5                                   ; error code
                test    byte ptr gs:[edi].g_access,D_PRES       ;
                jnz     @@dhsv_exit                             ; selector present?

                mov     esi,fs:PublicInfo.DosHlpFlatBase        ;
                imul    eax,ecx,IRQR_SIZE                       ;
                lea     eax,[esi+eax+offset DHIRQ0Router]       ; offset to IRQ handler

                mov     word ptr gs:[edi].g_handler,ax          ;
                shr     eax,10h                                 ; change IDT entry
                mov     gs:[edi].g_extoffset,ax                 ;
                mov     ax,fs:PublicInfo.FlatCS                 ;
                mov     word ptr gs:[edi].g_handler+2,ax        ;
                mov     byte ptr gs:[edi].g_access,D_INT032     ;
                xor     eax,eax                                 ;
@@dhsv_exit:
                RestReg <gs,fs,edi,esi,ecx,ebx>                 ;
                assume  fs:nothing                              ;
                ret                                             ;
DHSetIRQVector  endp
;
; Remove IRQ vector
;----------------------------------------------------------------
; IN: ECX = irq, EDX = vector
;
                public  DHUnSetIRQVector
DHUnSetIRQVector proc   near                                    ;
                SaveReg <ebx,ecx,esi,edi,fs,gs>                 ;

                mov     si,DOSHLP_DATASEL                       ;
                mov     fs,si                                   ;
                assume  fs:DOSHLP_DATA                          ;
                mov     gs,fs:KAT.IDTSel                        ;
                mov     edi,edx                                 ;
                shl     edi,3                                   ; offset in IDT
                mov     bl,gs:[edi].g_access                    ;
                test    bl,D_PRES                               ;
                jz      @@dhusv_exit                            ; selector present?
                cmp     bl,D_INT032                             ;
                jnz     @@dhusv_changed                         ; 32 bit?
                mov     bx,word ptr gs:[edi].g_handler+2        ;
                cmp     bx,fs:PublicInfo.FlatCS                 ; check vector sel
                jnz     @@dhusv_changed                         ;

                mov     esi,fs:PublicInfo.DosHlpFlatBase        ;
                imul    eax,ecx,IRQR_SIZE                       ;
                lea     eax,[esi+eax+offset DHIRQ0Router]       ; offset to IRQ handler

                mov     bx,gs:[edi].g_extoffset                 ;
                shl     ebx,10h                                 ;
                mov     bx,word ptr gs:[edi].g_handler          ; check vector ofs
                cmp     eax,ebx                                 ;
                jnz     @@dhusv_changed                         ; the same?
                xor     eax,eax                                 ;
                mov     gs:[edi],eax                            ; yes, clear IDT entry
                mov     gs:[edi+4],eax                          ;
                jmp     @@dhusv_exit                            ; rc = 0
@@dhusv_changed:
                mov     eax,5                                   ; rc = 5
@@dhusv_exit:
                RestReg <gs,fs,edi,esi,ecx,ebx>                 ;
                assume  fs:nothing                              ;
                ret                                             ;
DHUnSetIRQVector endp

;
; Fixup kernel jump points
;----------------------------------------------------------------
; IN: ds - FLAT, es - doshlp data (108), gs - boothlp data (118)
;
                public  DHFixup
DHFixup         proc    near
                assume  ds:nothing,es:nothing,ss:nothing
                assume  fs:DGROUP,gs:BOOTHLP_DATA

                push    es                                      ; free es for stos
                pop     fs                                      ; fs saved in caller
                mov     esi,offset CommonFixB                   ;
@@dhfix_make:
                cmp     esi,offset CommonFixE                   ; while more fixups to do
                jz      @@dhfix_end                             ;
                movzx   ebx,gs:[esi].fx_data                    ; fixup data
                xor     edi,edi                                 ;
                les     di,gs:[esi].fx_addr                     ; address
                mov     dl,gs:[esi].fx_misc                     ; misc byte

                xor     ecx,ecx                                 ; func offset
                mov     cl,gs:[esi].fx_type                     ;
                mov     cx,gs:FixUpTable[ecx*2]                 ;
                add     ecx,fs:PublicInfo.LdrHighFlatBase       ;
                call    ecx                                     ;
                add     esi,size FixUpStruc                     ;
                jmp     @@dhfix_make                            ;
@@dhfix_end:
                push    fs                                      ;
                pop     es                                      ;
                ret

; KJMPTYPE type fixup
                public  DHFixup_KJmp
DHFixup_KJmp    label   near
                add     ebx,fs:PublicInfo.DosHlpFlatBase        ;
                xor     eax,eax                                 ;
                mov     al,dl                                   ; field offset in KAT
                add     ax,offset KAT                           ;
                mov     eax,fs:[eax]                            ;
                sub     eax,ebx                                 ;
                stosd                                           ; fix address
                ret                                             ;

; LDIRQBTYPE type fixup
                public  DHFixup_LdIRQi                          ;
DHFixup_LdIRQi  label   near                                    ;
                mov     eax,gs:airqi                            ;
                add     eax,ebx                                 ; fix operrand
                stosd                                           ;
                ret                                             ;

; CMPFLATDSTYPE type fixup
                public  DHFixup_SetDSw                          ;
DHFixup_SetDSw  label   near                                    ;
                mov     ax,fs:PublicInfo.FlatDS                 ;
                stosw                                           ;
                ret                                             ;

; PUSHFLATDSTYPE, SETFLATDSTYPE type fixup
                public  DHFixup_SetDSd                          ;
DHFixup_SetDSd  label   near                                    ;
                movzx   eax,fs:PublicInfo.FlatDS                ;
                stosd                                           ;
                ret                                             ;

; PUSHFLAT_PSSEL type fixup
                public DHFixup_PushPS                           ;
DHFixup_PushPS  label   near                                    ;
                mov     ax,fs:FLAT_PSDSel                       ;
                stosw                                           ;
                ret                                             ;

; PUSHFLAT_HSSEL  type fixup
                public DHFixup_PushHS                           ;
DHFixup_PushHS  label   near                                    ;
                mov     ax,fs:FLAT_HSSel                        ;
                stosw                                           ;
                ret                                             ;

; PUSHFLAT_SSSEL type fixup
                public  DHFixup_PushSS                          ;
DHFixup_PushSS  label   near                                    ;
                mov     ax,fs:FLAT_SSSel                        ;
                stosw                                           ;
                ret                                             ;

                assume  fs:nothing,gs:nothing
DHFixup         endp

;
; Setup MTRR`s MSR registers on secondary CPUs
;----------------------------------------------------------------
; IN: ESI = doshlp flat, EDI = boothlp flat
;
                public  DHMsr_Setup
DHMsr_Setup     proc    near
                mov     eax,1                                   ;
                cpuid                                           ; for snoop flag
                pushfd                                          ;
                cli                                             ;
                pop     eax                                     ;
                mov     ecx,cr0                                 ;
                and     ecx,CPU_CR0_NW or CPU_CR0_CD            ; save CD & NW
                test    eax,CPU_EFLAGS_IF                       ; save
                setnz   cl                                      ; interrupts flag
                mov     eax,cr0                                 ;
                and     eax,not CPU_CR0_NW                      ;
                or      eax,CPU_CR0_CD                          ; no-fill cache
                mov     cr0,eax                                 ; mode

                and     edx,CPUID_FI2_SSNOOP                    ;
                jnz     @@mtrst_ssnoop                          ; flush cache
                wbinvd                                          ;
@@mtrst_ssnoop:
                add     ecx,edx                                 ; save ssnoop
                and     eax,CPU_CR0_PG                          ; PG is set?
                lea     ecx,[ecx+eax+2]                         ; save in state
                jz      @@mtrst_nopages                         ;
                mov     eax,cr4                                 ;
                mov     edx,eax                                 ;
                and     edx,CPU_CR4_PGE                         ; check PGE flag
                lea     ecx,[ecx+edx]                           ; and save it
                jz      @@mtrst_nofpge                          ;
                sub     eax,edx                                 ;
                mov     cr4,eax                                 ; clear PGE flag
                jmp     @@mtrst_nopages                         ;
@@mtrst_nofpge:
                mov     edx,cr3                                 ; flush TLBs
                mov     cr3,edx                                 ;
@@mtrst_nopages:
                mov     ebp,ecx                                 ;
                mov     ecx,MSR_IA32_MTRR_DEF_TYPE              ; disable all
                rdmsr                                           ; MTRRs
                and     eax,not MTRRS_MTRRON                    ;
                xor     edx,edx                                 ;
                wrmsr                                           ;
; cache disabled, MTRR disabled, saved state in ebp
; start to writing MSRs
                push    ds                                      ;
                mov     ds,[PublicInfo.FlatDS+esi]              ;
                mov     bx,[External.MsrTableCnt+esi]           ;
                lea     esi,[edi+MsrSetupTable]                 ;
                xor     ecx,ecx                                 ;
                cld                                             ;
@@dhmsr_loop:
                or      bx,bx                                   ; write fixed and
                jz      @@dhmsr_done                            ; variable range
                lodsw                                           ; MTRR msrs here
                mov     cx,ax                                   ;
                lodsd                                           ; the last one must be
                mov     edx,eax                                 ; IA32_MTRR_DEF_TYPE
                lodsd                                           ; with MTRR on bit set
                xchg    edx,eax                                 ;
                wrmsr                                           ;
                dec     bx                                      ;
                jmp     @@dhmsr_loop                            ;
@@dhmsr_done:
                pop     ds                                      ;
; MRSs done, MTRR must be restored by LAST of MSR writings, else
; we get 8086 until reboot or panorama thread ;)

                test    ebp,CPUID_FI2_SSNOOP                    ;
                jnz     @@mtren_ssnoop                          ; flush cache
                wbinvd                                          ;
@@mtren_ssnoop:
                mov     eax,cr3                                 ; flush TLBs
                mov     cr3,eax                                 ;

                mov     eax,cr0                                 ;
                mov     ecx,ebp                                 ;
                and     ecx,CPU_CR0_NW or CPU_CR0_CD            ; restore orginal
                and     eax,not (CPU_CR0_NW or CPU_CR0_CD)      ; cache flags
                or      eax,ecx                                 ;
                mov     cr0,eax                                 ;

                test    ebp,CPU_CR0_PG                          ;
                jz      @@mtren_nopages                         ;
                mov     ecx,ebp                                 ; restore PGE
                and     ecx,CPU_CR4_PGE                         ; flag
                mov     eax,cr4                                 ;
                or      eax,ecx                                 ;
                mov     cr4,eax                                 ;
@@mtren_nopages:
                test    ebp,1                                   ; restore IF
                jz      @@mtren_exit                            ;
                sti                                             ;
@@mtren_exit:                                                   ;
                ret
DHMsr_Setup     endp


BOOTHLP32_CODE  ends

IODELAY_FIXB    segment
                public  IOFixB
IOFixB          label   near
IODELAY_FIXB    ends

IODELAY_FIXE    segment
                public  IOFixE
IOFixE          label   near
IODELAY_FIXE    ends

COMMON_FIXB     segment
                public  CommonFixB
CommonFixB      label   near
COMMON_FIXB     ends

COMMON_FIXE     segment
                public  CommonFixE
CommonFixE      label   near
COMMON_FIXE     ends

                end
