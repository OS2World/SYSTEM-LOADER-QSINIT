;
; QSINIT
; doshlp code - ugly collection of ancient stuff ;)
;
                .586p
                include segdef.inc
                include doshlp.inc
                include ldrparam.inc
                include devhlp.inc
                include inc/qstypes.inc
                include inc/basemac.inc
                include inc/lowports.inc
                include inc/seldesc.inc
                include inc/iopic.inc
                include inc/cmos.inc
                include inc/timer.inc
                include inc/fixups.inc
                include inc/debug.inc
                include inc/serial.inc

LOGTMP_SIZE     equ     128

MSR_IA32_CLOCKMODULATION        equ     019Ah
MSR_AMD_PSTATE_LIMIT            equ     0C0010061h
MSR_AMD_PSTATE_CONTROL          equ     0C0010062h
MSR_AMD_PSTATE_STATUS           equ     0C0010063h

PUBLIC_INFO     segment
                extrn   PublicInfo:LoaderInfoData
                extrn   External:ExportFixupData
PUBLIC_INFO     ends

RESPART_END     segment                                         ;
                public  DosHlpResEnd                            ; end of resident
DosHlpResEnd    label   near                                    ; part label
RESPART_END     ends                                            ;

LASTSEG         segment                                         ;
                public  EndOfDosHlp                             ; end of entire doshlp
EndOfDosHlp     label   near                                    ; label
LASTSEG         ends                                            ;

OEMHLP_DATA     segment
                extrn   DEVHLP:dword                            ;
OEMHLP_DATA     ends   

DOSHLP_DATA     segment
                public  TimerRollover,Timer
FPU_CtrlWord    dw      0                                       ; FPU control word
                align   4
; rollover count (initialized in clock driver)
TimerRollover   qw_rec  <0,0>                                   ; current,next rollover count
; Timer value (updated at every Tmr rollover in Clock driver).
Timer           qw_rec  <0,0>                                   ;

                public  RM_PICMask,PM_PICMask,RM_RTCRegs,PM_RTCRegs
                public  SystemIRQMasks
SystemIRQMasks  dd      0
RM_PICMask      dd      0                                       ; RM pic masks
PM_PICMask      dd      -1                                      ; PM pic masks
PM_RTCRegs      db      CMOS_REG_A                              ;
                db      2Bh                                     ; REG_A Prot
                db      CMOS_REG_B or NMI_MASK                  ;
                db      52h                                     ; REG_B Prot
RM_RTCRegs      db      CMOS_REG_A                              ;
                db      26h                                     ; REG_A Real
                db      CMOS_REG_B or NMI_MASK                  ;
                db      2                                       ; REG_B Real
generic_msg     label   word                                    ;
                dw      generic_msg_end - generic_msg - 2       ; generic error
                db      'OS/2 error! <SYS00000>',13,10          ; message
generic_msg_end label   word                                    ;
                db      0                                       ;
panic_msg       label   word                                    ;
                db      'OEMHLP panic: function is not '        ; panic message
                db      'implemented!',13,10,'$'                ;
panic_msg_end   label   word                                    ;
                db      0                                       ;
                align   4
BaudRateTable   dd      BD_150                                  ;
                dd      BD_300                                  ;
                dd      BD_600                                  ;
                dd      BD_1200                                 ;
                dd      BD_2400                                 ;
                dd      BD_4800                                 ;
                dd      BD_9600                                 ;
                dd      BD_19200                                ;
                dd      BD_38400                                ;
                dd      BD_57600                                ;
                dd      BD_115200                               ;
BaudRateTBSize  equ     ($ - BaudRateTable)/4                   ;

LogTmpBuf       db      LOGTMP_SIZE dup(0)                      ; temp buffer for log
LogTmpBufPos    dw      0                                       ;
LastSerOutChar  db      0                                       ;
DOSHLP_DATA     ends

DOSHLP_CODE     segment
                assume  cs:DGROUP,ds:nothing,es:nothing,ss:nothing

                public  get_dgroup
get_dgroup      proc    near                                    ;
                smsw    ax                                      ;
                test    al,1                                    ;
                jz      @@s_ds_rm                               ;
                mov     ax,DOSHLP_DATASEL                       ;
                ret                                             ;
@@s_ds_rm:
                mov     ax,cs                                   ;
                ret                                             ;
get_dgroup      endp                                            ;

                public  DHInitSystemDump                        ;
DHInitSystemDump proc   far                                     ;
                db      9Ah                                     ;
                dd      0h                                      ;
                ret                                             ;
DHInitSystemDump endp                                           ;

                public  DHSystemDump                            ;
DHSystemDump    proc   far                                      ;
                db      9Ah                                     ;
                dd      3                                       ;
                ret                                             ;
DHSystemDump    endp                                            ;

; ---------------------------------------------------------------
; reboot                                                        ;
; ---------------------------------------------------------------
                public  DHReboot                                ;
DHReboot        proc    far                                     ;
                assume  cs:DGROUP,ds:DGROUP,es:nothing,ss:nothing
                call    get_dgroup                              ;
                mov     ds,ax
                cli                                             ; disable ints
                mov     al,0Bh                                  ; is anyone still need this clock
                out     CMOS_ADDR,al                            ; un-setup?
                in      al,CMOS_DATA                            ;
                and     al,0BFh                                 ; clear "enable periodic interrupt"
                push    ax                                      ;
                mov     al,0Bh                                  ;
                out     CMOS_ADDR,al                            ;
                pop     ax                                      ;
                out     CMOS_DATA,al                            ; status mask without periodic ints
                mov     al,0Ah                                  ;
                out     CMOS_ADDR,al                            ;
                in      al,CMOS_DATA                            ;
                and     al,0F0h                                 ; clear rate selection field
                or      al,6                                    ; set rate to 1024 Hz
                push    ax                                      ;
                mov     al,0Ah                                  ;
                out     CMOS_ADDR,al                            ;
                pop     ax                                      ;
                out     CMOS_DATA,al                            ;
                mov     al,0Fh                                  ; prevent garbage writes
                out     CMOS_ADDR,al                            ;
                in      al,CMOS_DATA                            ; finishing last access

                mov     ax,ROM_DATASEG                          ; same both in RM and PM
                mov     es,ax                                   ;
                mov     si,72h                                  ; set warm/cold start indicator
                mov     word ptr es:[si],1234h                  ;
                mov     al,0FEh                                 ; set magic value for reboot...
                out     KBD_STATUS_PORT,AL                      ;
@@dhrb_1:
                mov     cx,5000h                                ; wait a bit
@@dhrb_2:
                loop    @@dhrb_2                                ;
                mov     eax,cr0                                 ;
                test    al,CR0_PE                               ;
                jnz     @@dhrb_3                                ; go to protected mode
                call    dword ptr ds:[KAT.GotoProt]             ;
@@dhrb_3:
                mov     es,ds:KAT.IDTSel                        ;
                mov     cx,20*8                                 ; invalidate exception handlers
                xor     di,di                                   ;
                xor     ax,ax                                   ;
            rep stosb                                           ;
                mov     ds,ax                                   ; produce triple exception ;)
                mov     [di],al                                 ;
                hlt                                             ; hlt and try again ;)
                jmp     @@dhrb_1                                ;
DHReboot        endp                                            ;

;
; "Unimplemented" panic call
; ---------------------------------------------------------------
                public  DHInternalError                         ;
DHInternalError proc    far                                     ;
                SaveReg <ds,di,si,dx>                           ;
                mov     si,DOSHLP_DATASEL                       ;
                mov     ds,si                                   ;
                mov     si,offset panic_msg                     ;
                mov     di,panic_msg_end - panic_msg            ;
                Dev_Hlp InternalError                           ;
                RestReg <dx,si,di,ds>                           ;
                ret                                             ;
DHInternalError endp                                            ;

;
; DHNMI - enable/disable NMI
; ---------------------------------------------------------------
; Entry: al = 0   - enable nmi
;        al = 80h - disable nmi
;        ah = 0   - do not reset NMI status bits
;        ah = 1   - reset NMI status bits
; ---------------------------------------------------------------
                public  DHNMI                                   ;
DHNMI           proc    far                                     ;
                assume  cs:DGROUP,ds:nothing,es:nothing,ss:nothing
                push    dx                                      ;
                push    ds                                      ;
                cmp     al,DISABLE_NMI_REQ                      ;
                pushf                                           ;
                cli                                             ;
                mov     al,DISABLE_NMI                          ;
                je      @@dhnmi_disable                         ; disable?
                or      ah,ah                                   ;
                jz      @@dhnmi_enable                          ; no reset requested
                out     NMI_PORT,al                             ;
                in      al,NMI_STATUS_PORT                      ; fetch AT status bits
                or      al,NMI_IOCHK or NMI_FAILSAFE            ;
                out     NMI_STATUS_PORT,al                      ; disable and clear io_chk and fs_tmr
                and     al,not (NMI_IOCHK or NMI_FAILSAFE)      ;
                out     NMI_STATUS_PORT,al                      ; and now renable
@@dhnmi_enable:
                mov     al,ENABLE_NMI                           ;
@@dhnmi_disable:
                out     NMI_PORT,al                             ; setup itself
                call    DHDelay                                 ;
                call    DHDelay                                 ;
                in      al,NMI_PORT+1                           ;
                popf                                            ;
                clc                                             ; return success
                pop     ds                                      ;
                pop     dx                                      ;
                ret                                             ;
DHNMI           endp

                assume  cs:DGROUP,ds:nothing,es:nothing,ss:nothing
                public  DHDelay
DHDelay         proc    near
                IODelay16
                ret
DHDelay         endp
; ---------------------------------------------------------------
; actually this call can be used in Merlin hybernate code, so
; start to panic here ;)
                public  DHSetDosEnv                             ;
DHSetDosEnv     label   far                                     ;
                jmp     near ptr DHInternalError                ;
; ---------------------------------------------------------------
                public  DHEnableWatchdogNMI                     ;
                public  DHDisableWatchdogNMI                    ;
DHEnableWatchdogNMI  label far                                  ;
DHDisableWatchdogNMI label far                                  ;
                retf                                            ;
; ---------------------------------------------------------------
                public  DHSetProtMask                           ;
DHSetProtMask   proc    far                                     ;
                assume  cs:DGROUP,ds:nothing,es:nothing,ss:nothing
                SaveReg <ds,di,eax,cx>                          ;
                call    get_dgroup                              ;
                mov     ds,ax                                   ;
                assume  ds:DOSHLP_DATA                          ;
                pushf                                           ;
                cli                                             ;
                lea     di,PM_RTCRegs                           ;
                call    SetRTCRegs                              ; PM RTC regs
                mov     eax,PM_PICMask                          ;
                call    SetMask                                 ; PM PIC mask
                popf                                            ;
                RestReg <cx,eax,di,ds>                          ;
                assume  ds:nothing                              ;
                ret                                             ;
DHSetProtMask   endp                                            ;
; ---------------------------------------------------------------
                public  DHSerIn                                 ;
DHSerIn         proc    far                                     ;
                assume  cs:DGROUP,ds:nothing,es:nothing,ss:nothing
                push    ds                                      ;
                push    dx                                      ;
                push    cs                                      ;
                pop     ds                                      ; READ ONLY Data
                assume  ds:DOSHLP_DATA                          ;
                xor     ax,ax                                   ; no char

                test    PublicInfo.DebugTarget,DEBUG_TARGET_COM ; check debug target
                jz      @@dhsin_exit                            ;
                mov     dx,PublicInfo.DebugPort                 ;
                or      dx,dx                                   ; port present?
                jz      @@dhsin_exit                            ;
                add     dx,COM_LSR                              ; data ready?
                in      al,dx                                   ;
                test    al,1                                    ;
                jz      @@dhsin_exit                            ; no, exit
                mov     dx,PublicInfo.DebugPort                 ;
                in      al,dx                                   ;
                or      dx,dx                                   ; clear zero flag
@@dhsin_exit:
                pop     dx                                      ;
                pop     ds                                      ;
                ret                                             ;
DHSerIn         endp                                            ;

; ---------------------------------------------------------------
                public  DHSerOut                                ;
DHSerOut        proc    far                                     ;
                SaveReg <ds,eax,edx>                            ;
                mov     dx,ax                                   ;
                call    get_dgroup                              ;
                mov     ds,ax                                   ;
                mov     ax,dx                                   ;
                cmp     External.LogBufSize,0                   ; log present?
                jz      @@dhso_outbyte                          ;
                call    LogOutChar                              ; make a visit
@@dhso_outbyte:
                test    PublicInfo.DebugTarget,DEBUG_TARGET_COM ; serial?
                jz      @@dhso_nocom                            ;
                mov     dx,PublicInfo.DebugPort                 ;
                or      dx,dx                                   ;
                jz      @@dhso_exit                             ; THEN send char to bit bucket
                cmp     al, 0Ah                                 ; \n ?
                jnz     @@dhso_prepare                          ;
                cmp     LastSerOutChar,0Dh                      ; was \r before \n?
                jz      @@dhso_prepare                          ;
                mov     al,0Dh                                  ; no? then print it ;)
                call    @@dhso_start                            ;
                mov     al,0Ah                                  ;
@@dhso_prepare:
                mov     LastSerOutChar,al                       ;
                push    offset @@dhso_exit                      ;
@@dhso_start:
                xchg    al,ah                                   ; ah = character
                add     dx,COM_LSR                              ; wait for empty transmit register
@@dhso_waitout:
; ---------------------------------------------------------------
; For HW flow control at full wire cable
; Wire must connected as:
; RTS1         - CTS2
; CTS1         - RTS2
; DTR1         - DSR2 + RING2 *RING is optional *this link is optional, not use by term/osldr
; DSR1 + RING1 - DTR2         *RING is optional *this link is optional, not use by term/osldr
; RX1          - TX2
; TX1          - RX2
; GRD1         - GRD2
; Info GRD1    - Info GRD2 (for 25 pin only)
; Logic  - Wait CTS cx times, then do out
; ---------------------------------------------------------------
                test    External.Flags,EXPF_FULLCABLE           ; HW flow control?
                jz      @@dhso_ready                            ;
                shl     edx,16                                  ; save dx
                shl     eax,16                                  ; save ax
                mov     dx,PublicInfo.DebugPort                 ;
                add     dx,COM_MCR                              ;
                mov     al,3                                    ;
                out     dx,al                                   ; DTR & RTS to high
                mov     dx,PublicInfo.DebugPort                 ;
                add     dx,COM_MSR                              ;
                push    cx                                      ;
                mov     cx,1000h                                ; how many wait
@@dhso_waitcts:
                in      al,dx                                   ;
                test    al,010h                                 ; check CTS
                jnz     @@dhso_waittr                           ;
                loop    @@dhso_waitcts                          ;
@@dhso_waittr:
                pop     cx                                      ;
                shr     edx,16                                  ; restore dx
                shr     eax,16                                  ; restore ax
@@dhso_ready:
                in      al,dx                                   ;
                test    al,020h                                 ;
                jz      @@dhso_waitout                          ;
                xchg    al,ah                                   ; AL = character

                mov     dx,PublicInfo.DebugPort                 ;
                out     dx,al                                   ;
                retn                                            ; for near call above
@@dhso_nocom:
                test    PublicInfo.DebugTarget,DEBUG_TARGET_VIO ; print to screen?
                jz      @@dhso_exit                             ;
                call    VideoOut                                ;
@@dhso_exit:
                RestReg <edx,eax,ds>                            ;
                ret                                             ;
DHSerOut        endp                                            ;

;
; allocate selector for physical video memory
; ---------------------------------------------------------------
; OUT: bx - selector or 0
;
VideoMemSelAlloc proc   near                                    ;
                assume  cs:nothing,ds:DOSHLP_DATA,es:nothing,ss:nothing
                SaveReg <esi,edi,ax>                            ;
                push    ds                                      ; es was saved in VideoOut
                pop     es                                      ;
                mov     di,offset PublicInfo.VMemGDTSel         ;
                mov     cx,1                                    ;
                call    dword ptr KAT.AllocGDT                  ;
                mov     bx,0                                    ;
                jc      @@vmsa_ready                            ;
                mov     si,PublicInfo.VMemGDTSel                ;
                                                                ;
                mov     eax,0B8000h                             ;
                mov     ecx,80*50*2                             ;
                mov     dh,6                                    ;
                call    dword ptr KAT.PhysToGDT                 ;
                mov     bx,si                                   ;
@@vmsa_ready:
                mov     PublicInfo.VMemGDTSel,bx                ;
                RestReg <ax,edi,esi>                            ;
                ret                                             ;
VideoMemSelAlloc endp

;
; symbol to screen (single code for RM/PM)
; ---------------------------------------------------------------
; IN: al - char to output
;
                assume  cs:nothing,ds:DOSHLP_DATA,es:nothing,ss:nothing
VideoOut        proc    near                                    ;
                cmp     al,0Dh                                  ; \r
                je      @@vout_exit                             ; yes, skip it!
                SaveReg <es,ebx,ecx,edi>                        ; ds, edx, eax was saved above
                xor     edi,edi                                 ; vmem offset in segment
                mov     ebx,cr0                                 ;
                test    bl,CR0_PE                               ; PM?
                mov     bx,0B800h                               ;
                jz      @@vout_real_mode                        ;
                mov     ecx,cr3                                 ; paging mode active?
                or      ecx,ecx                                 ;
                jnz     @@vout_pgenable                         ;
                mov     edi,0B8000h                             ; no, calculating from physical addr
                add     edi,External.LAddrDelta                 ; by adding non-zero FLAT base offset
                mov     bx,PublicInfo.FlatDS                    ;
                jmp     @@vout_real_mode                        ;
@@vout_pgenable:
                test    PublicInfo.BootFlags,BOOTFLAG_PGINIT    ; kernel api ready?
                jz      @@vout_leave                            ;
                mov     bx,PublicInfo.VMemGDTSel                ; paging mode active - trying to
                or      bx,bx                                   ; allocate selector
                jnz     @@vout_real_mode                        ;
                call    VideoMemSelAlloc                        ;
                or      bx,bx                                   ;
                jz      @@vout_leave                            ; failed, exiting
@@vout_real_mode:
                assume  ds:nothing
                mov     ds,bx                                   ; sel/seg for output
                mov     bx,40h                                  ; sel/seg 40h (yes, kernel create this one)
                mov     es,bx                                   ;

                mov     cx,es:[50h]                             ; cursor pos
                xor     ebx,ebx                                 ;
                cmp     al,0Ah                                  ; \n
                je      @@vout_eol                              ;

                mov     bl,ch                                   ; offset
                imul    ebx,160                                 ;
                xor     ch,ch                                   ;
                shl     cx,1                                    ;
                add     bx,cx                                   ; painting with
                mov     ah,0Eh                                  ; YELLOW! :)
                mov     [edi+ebx],ax                            ;
                mov     cx,es:[50h]                             ; update cursor column
                inc     cl                                      ; ch - row
                cmp     cl,80                                   ;
                jc      @@vout_savepos                          ;
@@vout_eol:
                xor     cl,cl                                   ; new string
                movzx   bx,byte ptr es:[84h]                    ; bottom line?
                cmp     ch,bl                                   ;
                jnz     @@vout_noscroll                         ;
                mov     es:[50h],cx                             ; save cursor position
                push    ds                                      ;
                pop     es                                      ;
                push    esi                                     ;
                lea     esi,[edi+160]                           ; scrolling screen ;)
                imul    ecx,ebx, 80                             ;
            rep movs    word ptr es:[edi],word ptr ds:[esi]     ; db 67h ;)
                mov     ecx,80                                  ;
                mov     eax,0720h                               ; clearing bottom line
            rep stos    word ptr es:[edi]                       ;
                pop     esi                                     ;
                jmp     @@vout_leave                            ;
@@vout_noscroll:
                inc     ch                                      ; new line
@@vout_savepos:
                mov     es:[50h],cx                             ; save cursor position
@@vout_leave:
                RestReg <edi,ecx,ebx,es>                        ;
@@vout_exit:
                ret                                             ;
VideoOut        endp                                            ;

;
; Print symbol to log
; ---------------------------------------------------------------
; IN : ax - char
;     
LogOutChar      proc    near                                    ;
                assume  cs:nothing,ds:DOSHLP_DATA,es:nothing,ss:nothing
                push    edi                                     ;
                push    edx                                     ;
                mov     edx,cr0                                 ;
                test    dl,CR0_PE                               ;
                jz      @@log_real                              ;
                test    edx,CR0_PG                              ; paging mode enabled?
                jz      @@log_p_usephys                         ; if no - use phys,
                mov     edi,External.LogBufVAddr                ; else - linear addr
                or      edi,edi                                 ; required
                jz      @@log_real                              ;
@@log_p_usephys:
                cmp     LogTmpBufPos,0                          ; check temp buffer
                jz      @@log_p_start                           ; 
                push    ax                                      ; yes, data present
                push    si                                      ;
                push    cx                                      ;
                mov     cx,LogTmpBufPos                         ; save counter and
                mov     LogTmpBufPos,0                          ; loop self cx times ;)
                mov     si,offset LogTmpBuf                     ;
                cld                                             ;
@@log_p_loop:
                lodsb                                           ;
                call    LogOutChar                              ;
                loop    @@log_p_loop                            ;
                pop     cx                                      ;
                pop     si                                      ;
                pop     ax                                      ; continue with current al
@@log_p_start:
                test    edx,CR0_PG                              ; paging mode enabled?
                jnz     @@log_p_pgenable                        ;
                mov     edi,External.LogBufPAddr                ; no, use physical addr to
                add     edi,External.LAddrDelta                 ; calculate log buffer offset
@@log_p_pgenable:
                push    es                                      ;
                mov     es,ds:PublicInfo.FlatDS                 ; es = FLAT
                push    ecx                                     ;
                mov     ecx,edi                                 ;
                mov     edx,External.LogBufSize                 ; get log`s full size
                shl     edx,16                                  ;
                LockLogBuffer <External.LogBufLock>             ; lock log buffer
                add     edi,External.LogBufWrite                ; 
                stos    byte ptr es:[edi]                       ; writing single byte
                sub     edi,ecx                                 ;
                pop     ecx                                     ;
                cmp     edi,edx                                 ; check buffer overflow
                jnz     @@log_p_noround                         ;
                xor     edi,edi                                 ; yes? fix it
@@log_p_noround:
                mov     External.LogBufWrite,edi                ; save write pos
                cmp     edi,External.LogBufRead                 ;
                jnz     @@log_p_norround                        ; we`re overwriting unreaded data?
                inc     edi                                     ; yes? fix read pos
                cmp     edi,edx                                 ; and check it for overflow too
                jnz     @@log_p_noralign                        ;
                xor     edi,edi                                 ;
@@log_p_noralign:
                mov     External.LogBufRead,edi                 ; saving read pos
@@log_p_norround:
                UnlockLogBuffer <External.LogBufLock>           ; unlock log buffer
                pop     es                                      ;
                jmp     @@log_exit                              ; ************************************
@@log_real:
                mov     edi,External.LogBufPAddr                ; real mode / pm without
                or      edi,edi                                 ; vaddr output to buffer
                jz      @@log_exit                              ; phys addr present?
                mov     di,LogTmpBufPos                         ; saving to temp buffer
                mov     [LogTmpBuf+di],al                       ;
                inc     di                                      ;
                cmp     di, LOGTMP_SIZE                         ; temp buffer overflow?
                jnc     @@log_exit                              ;
@@log_r_nocopy:
                mov     LogTmpBufPos,di                         ; update temp buffer pos
@@log_exit:
                pop     edx                                     ;
                pop     edi                                     ;
                ret                                             ;
LogOutChar      endp                                            ;

;
; initialize debug kernel serial port
; ---------------------------------------------------------------
                public  DHSerInit                               ;
DHSerInit       proc    far                                     ;
                assume  cs:DGROUP,ds:nothing,es:nothing,ss:nothing
                push    edx                                     ;
                test    cs:PublicInfo.DebugTarget,DEBUG_TARGET_COM
                jz      @@dhsi_exit                             ;
                mov     dx,cs:PublicInfo.DebugPort              ;
                or      dx,dx                                   ;
                jz      @@dhsi_exit                             ; save regs for
                SaveReg <eax,ecx,bx>                            ; SetBaudRate
                add     dx,COM_MCR                              ;
                in      al,dx                                   ;
                or      al,3                                    ;
                out     dx,al                                   ;
                call    SetBaudRate                             ;
                call    get_dgroup                              ;
                push    es                                      ; flag about
                mov     es,ax                                   ; com port init
                or      es:External.Flags,EXPF_COMINITED        ;
                RestReg <es,bx,ecx,eax>                         ;
                stc                                             ;
@@dhsi_exit:
                pop     edx                                     ;
                cmc                                             ; error code
                ret                                             ;
DHSerInit       endp                                            ;

;
; set the baud rate on the debugging port
; ---------------------------------------------------------------
; Actually, this function called only by ".B rate [port]" kernel
; debugger command, so this code is a garbage ;)
; IN: BX  = port (0 = default, 1 = COM1, 2 = COM2, n = I/O address)
;     EAX = baud rate
;
                public  DHSetBaudRate                           ;
DHSetBaudRate   proc    far                                     ;
                assume  cs:nothing,ds:nothing,es:DOSHLP_DATA,ss:nothing
                SaveReg <es,di,eax,ecx,edx,bx>                  ;
                push    ax                                      ;
                call    get_dgroup                              ;
                mov     es,ax                                   ;
                pop     ax                                      ;
                test    es:PublicInfo.DebugTarget,DEBUG_TARGET_COM
                jz      @@sbr_exit                              ;
                mov     di,offset BaudRateTable                 ;
                mov     cx,BaudRateTBSize                       ;
                cld                                             ; check baud rate table
          repne scasd                                           ;
                jz      @@sbr_port                              ;
                stc                                             ;
                jmp     @@sbr_exit                              ;
@@sbr_port:
                mov     es:External.BaudRate,eax                ;
                or      bx,bx                                   ;
                jnz     @@sbr_com1                              ;
                mov     bx,es:PublicInfo.DebugPort              ;
                jmp     @@sbr_setup                             ;
@@sbr_com1:
                cmp     bx,1                                    ;
                jne     @@sbr_com2                              ;
                mov     bx,COM1_PORT                            ;
@@sbr_com2:
                cmp     bx,2                                    ;
                jne     @@sbr_setup                             ;
                mov     bx,COM2_PORT                            ;
@@sbr_setup:
                mov     es:PublicInfo.DebugPort,bx              ;
                push    cs                                      ;
                call    near ptr DHSerInit                      ;
@@sbr_exit:
                RestReg <bx,edx,ecx,eax,di,es>                  ;
                ret                                             ;
DHSetBaudRate   endp                                            ;

; ---------------------------------------------------------------
; eax, bx, ecx, edx must be saved in parent
SetBaudRate     proc    near                                    ;
                assume  cs:DGROUP,ds:nothing,es:nothing,ss:nothing
                mov     ecx,cs:External.BaudRate                ; new baud rate
                mov     ax,CLOCK_RATEL                          ;
                shl     eax,16                                  ;
                mov     ax,CLOCK_RATEH                          ;
                xor     edx,edx                                 ; edx:eax = clock rate
                div     ecx                                     ;
                mov     bx,ax                                   ; clock rate / baud rate

                mov     dx,cs:PublicInfo.DebugPort              ;
                or      dx,dx                                   ;
                jz      @@sbr_noport                            ;
                add     dx,COM_LCR                              ;
                in      al,dx                                   ; current value of LCR
                or      al,LC_DLAB                              ; turn on DLAB
                out     dx,al                                   ;

                add     dx,COM_DLM-COM_LCR                      ;
                mov     al,bh                                   ; divisor latch MSB
                out     dx,al                                   ;
                dec     dx                                      ;
                mov     al,bl                                   ; divisor latch LSB
                out     dx,al                                   ;

                add     dx,COM_LCR-COM_DLL                      ;
                mov     al,3                                    ;
                out     dx,al                                   ;
@@sbr_noport:
                ret                                             ;
SetBaudRate     endp                                            ;

; ---------------------------------------------------------------
                assume  cs:nothing,ds:nothing,es:nothing,ss:nothing
                public  DHSetMask
DHSetMask       proc    far                                     ;
                call    SetMask                                 ;
                ret                                             ;
DHSetMask       endp                                            ;
; ---------------------------------------------------------------
SetMask         proc    near                                    ;
                push    eax                                     ;
                pushf                                           ;
                cli                                             ; disable interrupts
                out     PIC1_PORT1,al                           ; setup PIC1
                xchg    al,ah                                   ;
                out     PIC2_PORT1,al                           ; setup PIC2
                popf                                            ;
                pop     eax                                     ;
                ret                                             ;
SetMask         endp                                            ;
; ---------------------------------------------------------------
                public  DHGetMask                               ;
DHGetMask       proc    far                                     ;
                call    GetMask                                 ;
                ret                                             ;
DHGetMask       endp                                            ;
; ---------------------------------------------------------------
GetMask         proc    near                                    ;
                pushf                                           ; disable interrupts
                cli                                             ;
                xor     eax,eax                                 ;
                in      al,PIC2_PORT1                           ; read PIC2
                xchg    al,ah                                   ;
                in      al,PIC1_PORT1                           ; read PIC1
                popf                                            ;
                ret                                             ;
GetMask         endp                                            ;
; ---------------------------------------------------------------
                public  DHSetRealMask                           ;
DHSetRealMask   proc    far                                     ;
                assume  cs:DGROUP,ds:nothing,es:nothing,ss:nothing
                SaveReg <ds,di,eax>                             ;
                call    get_dgroup                              ;
                mov     ds,ax                                   ;
                assume  ds:DOSHLP_DATA                          ;
                pushf                                           ;
                cli                                             ;
                lea     di,PM_RTCRegs                           ; saving PM RTC regs
                push    cs                                      ;
                call    GetRTCRegs                              ;
                lea     di,RM_RTCRegs                           ; loading RM RTC regs
                call    SetRTCRegs                              ;
                call    GetMask                                 ; save PM pic mask
                mov     PM_PICMask,eax                          ;
                or      eax,SystemIRQMasks                      ;
                or      eax,0000DE79h                           ; disable IRQs 1,2,7,8,13
                not     eax                                     ; (eax) = OS2 DD's owned IRQ masks
                or      eax,RM_PICMask                          ;
                and     ax,0BFFFh                               ; irq14 always on ;)
                call    SetMask                                 ; set RM pic mask
                popf                                            ;
                RestReg <eax,di,ds>                             ;
                assume  ds:nothing                              ;
                ret                                             ;
DHSetRealMask   endp                                            ;
; ---------------------------------------------------------------
                public  DHInt10
DHInt10         proc    far                                     ;
                cmp     ax,3                                    ; 80x25?
                jnz     @@dhi10_set                             ;
                push    ax
                mov     ah,0Fh                                  ; get current video mode
                int     10h                                     ;
                and     al,0F7h                                 ; mask for 07 and 0F modes
                cmp     al,7                                    ;
                pop     ax                                      ; is this a monochrome mode?
                jnz     @@dhi10_set                             ;
                xor     ah,ah                                   ; yes, set mono 80x25
@@dhi10_set:
                int     10h                                     ;
                ret                                             ;
DHInt10         endp                                            ;
; ---------------------------------------------------------------
                public  DHTmr16QueryTime
DHTmr16QueryTime proc   far                                     ;
                push    ds                                      ;
                mov     dx,DOSHLP_DATASEL                       ;
                mov     ds,dx                                   ;
                assume  ds:DGROUP                               ;
                pushf                                           ;
                cli                                             ;
                mov     al,OCW3_READ_ISR                        ; setup PIC for ISR read
                out     PIC1_PORT0,al                           ;
                call    DHDelay                                 ;
                in      al,PIC1_PORT0                           ; check for servicing IRQ0
                call    DHDelay                                 ;
                mov     ah,al                                   ;
                mov     al,OCW3_READ_IRR                        ; setup PIC for IRR read
                out     PIC1_PORT0,al                           ;
                call    DHDelay                                 ;
                in      al,PIC1_PORT0                           ; check for pending IRQ0
                or      ah,al                                   ;
                mov     al,SC_CNT0 or RW_LATCHCNT               ;
                out     PORT_CW,al                              ;
                call    DHDelay                                 ;
                in      al,PORT_CNT0                            ;
                call    DHDelay                                 ;
                movzx   edx,al                                  ;
                in      al,PORT_CNT0                            ;
                mov     dh,al                                   ;
                in      al,PIC1_PORT0                           ;
                neg     dx                                      ;
                and     ax, 101h                                ;
                jz      @@tqt16_nopending                       ; no IRQ0 pending?
                add     edx,TimerRollover.qw_low                ;
                or      ah,ah                                   ;
                jnz     @@tqt16_nopending                       ; jump if IRR0
                mov     edx,TimerRollover.qw_low                ;
@@tqt16_nopending:
                add     edx,Timer.qw_low                        ; add current time
                mov     eax,edx                                 ;
                mov     edx,Timer.qw_high                       ;
                adc     edx,0                                   ;
                jno     @@tqt16_exit                            ;
                mov     eax,-1                                  ; overflow? store
                mov     edx,7FFFFFFFh                           ; max __int64 value
@@tqt16_exit:
                popf                                            ;
                pop     ds                                      ;
                assume  ds:nothing                              ;
                ret                                             ;
DHTmr16QueryTime endp                                           ;
; ---------------------------------------------------------------
                public DHToneOff                                ;
DHToneOff       proc   far                                      ;
                pushf                                           ;
                push    ax                                      ;
                in      al,SPEAKER_PORT                         ; turn off speaker
                and     al,SPEAKER_MASK_OFF                     ;
                out     SPEAKER_PORT,al                         ;
                pop     ax                                      ;
                popf                                            ;
                ret                                             ;
DHToneOff       endp                                            ;

;
; Start speaker sound (real/pm)
; ---------------------------------------------------------------
; IN:  bx - frequency
;
                public  DHToneOn                                ;
DHToneOn        proc    far                                     ;
                push    ax                                      ;
                push    cx                                      ;
                push    dx                                      ;
                mov     al,10110110b                            ; timer 2/LSB/MSB/binary
                out     PORT_CW,al                              ;
                mov     dx,TIMERFREQ_HI                         ;
                mov     ax,TIMERFREQ_LO                         ;
                div     bx                                      ; divide to frequency
                out     PORT_CNT2,al                            ; write to timer
                call    DHDelay                                 ;
                mov     al,ah                                   ;
                out     PORT_CNT2,al                            ;
                call    DHDelay                                 ;

                in      al,SPEAKER_PORT                         ; read port
                call    DHDelay                                 ;
                or      al,SPEAKER_MASK                         ; turn on speaker
                out     SPEAKER_PORT,al                         ;
                pop     dx                                      ;
                pop     cx                                      ;
                pop     ax                                      ;
                ret                                             ;
DHToneOn        endp                                            ;

;
; Return message string (protected mode)
; ---------------------------------------------------------------
; IN:  ax - message index
; OUT: ds:si - message string
;
                public  DHProtGetMessage                        ;
DHProtGetMessage proc   far                                     ;
                assume  cs:nothing,ds:DOSHLP_DATA,es:nothing,ss:nothing
                ;dbg16print <"DHProtGetMessage: %x",10>,<ax>     ;
                push    cx                                      ;
                mov     si,DOSHLP_DATASEL                       ;
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
                mov     ax,BOOTHLP_DATASEL                      ; second pass?
                mov     si,ds                                   ;
                cmp     ax,si                                   ;
                jz      @@dhgmr_notfound                        ; no message
                test    External.Flags,EXPF_DISCARDED           ;
                jnz     @@dhgmr_notfound                        ; high part discarded?
                mov     si,External.DisMsgOfs                   ;
                mov     ds,ax                                   ; go to second
                jmp     @@dhgmr_loop                            ; pass
@@dhgmr_notfound:
                mov     ax,cx                                   ;
                pop     cx                                      ;
                jmp     GenericMessage                          ; SYSXXXXX!!
DHProtGetMessage endp                                           ;

;
; Return generic SYSXXXXX message (both real and PM mode)
; ---------------------------------------------------------------
; IN:  ax - message index
; OUT: ds:si - message string
;
                public  GenericMessage
GenericMessage  proc    far
                SaveReg <es,cx,dx,di>                           ;
                mov     si,ax                                   ;
                call    get_dgroup                              ;
                mov     ds,ax                                   ;
                mov     es,ax                                   ;

                mov     di,offset generic_msg+18                ;
                mov     cx,5                                    ;
                mov     al,'0'                                  ;
            rep stosb                                           ; store 00000

                mov     di,offset generic_msg+18+4              ;
                std                                             ;
                mov     ax,si                                   ; msgid -> XXXXX
                mov     cx,10                                   ; conversion
@@genmsg_int:
                xor     dx,dx                                   ;
                div     cx                                      ;
                xchg    al,dl                                   ;
                add     al,'0'                                  ;
                stosb                                           ;
                xchg    al,dl                                   ;
                or      ax,ax                                   ; msgid finished
                jnz     @@genmsg_int                            ;
                cld                                             ;
                mov     ax,si                                   ;
                mov     si,offset generic_msg                   ;
                RestReg <di,dx,cx,es>                           ;
                ret                                             ;
GenericMessage  endp

;
; ---------------------------------------------------------------
                public  DHIODelayCalibrate
DHIODelayCalibrate proc far                                     ;
                assume  cs:nothing,ds:nothing,es:nothing,ss:nothing
                mov     ax,cs:External.IODelay                  ;
                ret                                             ;
DHIODelayCalibrate endp                                         ;
; ---------------------------------------------------------------

;
; Write CMOS reg
; ---------------------------------------------------------------
; In: AL = register, AH = data
WriteCMOS16     proc    near                                    ;
                out     CMOS_ADDR,al                            ;
                xchg    ah,al                                   ;
                out     CMOS_DATA,al                            ;
                ret                                             ;
WriteCMOS16     endp                                            ;
;
; Read CMOS reg
; ---------------------------------------------------------------
; In : AL = register
; Out: AL = data
ReadCMOS16      proc    near                                    ;
                out     CMOS_ADDR,al                            ;
                in      al,CMOS_DATA                            ;
                ret                                             ;
ReadCMOS16      endp                                            ;
;
; SetRTCRegs - Set Real Time Clock Regs
; ---------------------------------------------------------------
SetRTCRegs      proc    near                                    ;
                push    cx                                      ;
                pushf                                           ;
                cli                                             ;
                mov     ax,[di+2]                               ; Restore RTC Reg B
                call    WriteCMOS16                             ;
                mov     ax,[di]                                 ; Restore RTC Reg A
                call    WriteCMOS16                             ;
                mov     cx,4                                    ;
@@srtc_loop:
                mov     al,CMOS_REG_C                           ; read reg C 4 times to clear
                call    ReadCMOS16                              ; any pending interrupts
                loop    @@srtc_loop                             ;
                mov     al,CMOS_REG_D                           ; Set RTC addr ptr to RTC Reg D
                call    ReadCMOS16                              ;
                popf                                            ;
                pop     cx                                      ;
                ret                                             ;
SetRTCRegs      endp
;
; GetRTCRegs - Reads Real Time Clock Regs A,B
; ---------------------------------------------------------------
                public  GetRTCRegs
GetRTCRegs      proc    near
                pushf                                           ;
;               cli                                             ;
                mov     al,CMOS_REG_A                           ; save reg A
                call    ReadCMOS16                              ;
                and     al,07Fh                                 ; drop NMI flag
                mov     [di+1],al                               ;
                mov     al,CMOS_REG_B or NMI_MASK               ; save reg B
                call    ReadCMOS16                              ;
                mov     [di+3],al                               ;
                mov     al,CMOS_REG_D                           ;
                call    ReadCMOS16                              ;
                popf                                            ;
                retf                                            ; retf here!
GetRTCRegs      endp

DOSHLP_CODE     ends
; ===============================================================

DOSHLP32_CODE   segment
                assume  cs:DOSHLP32_CODE,ds:nothing,es:nothing,ss:nothing

                public  DH32_Delay
DH32_Delay      proc    near
                IODelay32
                ret
DH32_Delay      endp
; ---------------------------------------------------------------
                public  DHWhyNMI
DHWhyNMI        proc    near                                    ; do not return any
                push    ds                                      ; EISA messages
                push    si                                      ;
                push    ax                                      ;
                mov     ax,MSG_NMI                              ;
                push    cs                                      ;
                call    near ptr DHProtGetMessage               ; get message
                movzx   edx,si                                  ;
                pop     ax                                      ;
                mov     si,ds                                   ;
                mov     fs,si                                   ;
                pop     si                                      ;
                pop     ds                                      ;
                ret                                             ;
DHWhyNMI        endp
; ---------------------------------------------------------------
                public  DHAckNMI
                public  DHWriteErrorLog
                public  DHVNPXClrBusy
DHWriteErrorLog label   near                                    ;
DHAckNMI        label   near                                    ;
DHVNPXClrBusy   label   near                                    ;
                ret
; ---------------------------------------------------------------
                public DHGetErrorLogPtr                         ;
DHGetErrorLogPtr proc  near                                     ;
                mov     ax,-1                                   ;
                ret                                             ;
DHGetErrorLogPtr endp                                           ;
; ---------------------------------------------------------------
                public  DHReadErrorLog                          ;
DHReadErrorLog  proc    near                                    ;
                mov     edx,-1                                  ;
                ret                                             ;
DHReadErrorLog  endp                                            ;
; ---------------------------------------------------------------
                public  DHResetParity                           ;
                public  DHDisableCache                          ;
                public  DHEnableCache                           ;
DHResetParity   label   near                                    ;
DHDisableCache  label   near                                    ;
DHEnableCache   label   near                                    ;
                stc                                             ;
                ret                                             ;
; ---------------------------------------------------------------
                public  DHFindParity                            ;
DHFindParity    proc    near                                    ;
                clc                                             ;
                ret                                             ;
DHFindParity    endp                                            ;
; ---------------------------------------------------------------
                public  DHResetWatchdogNMI                      ;
                public  DHValidNPXSwitch                        ; WARNING! original doshlp
DHResetWatchdogNMI label  near                                  ; return AX in DHValidNPXSwitch
DHValidNPXSwitch   label  near                                  ;
                xor     eax,eax                                 ;
                ret                                             ;
; ---------------------------------------------------------------
                public  DHTmr32QueryTime                        ;
DHTmr32QueryTime proc   near                                    ;
                push    ds                                      ;
                mov     dx,DOSHLP_DATASEL                       ;
                mov     ds,dx                                   ;
                assume  ds:DGROUP                               ;
                pushfd                                          ;
                cli                                             ;
                mov     al,OCW3_READ_ISR                        ; setup PIC for ISR read
                out     PIC1_PORT0,al                           ;
                call    DH32_Delay                              ;
                in      al,PIC1_PORT0                           ; check for servicing IRQ0
                call    DH32_Delay                              ;
                mov     ah,al                                   ;
                mov     al,OCW3_READ_IRR                        ; setup PIC for IRR read
                out     PIC1_PORT0,al                           ;
                call    DH32_Delay                              ;
                in      al,PIC1_PORT0                           ; check for pending IRQ0
                or      ah,al                                   ;
                mov     al,SC_CNT0 or RW_LATCHCNT               ;
                out     PORT_CW,al                              ;
                call    DH32_Delay                              ;
                in      al,PORT_CNT0                            ;
                call    DH32_Delay                              ;
                movzx   edx,al                                  ;
                in      al,PORT_CNT0                            ;
                mov     dh,al                                   ;
                in      al,PIC1_PORT0                           ;
                neg     dx                                      ;
                and     ax, 101h                                ;
                jz      @@tqt32_nopending                       ; no IRQ0 pending?
                add     edx,TimerRollover.qw_low                ;
                or      ah,ah                                   ;
                jnz     @@tqt32_nopending                       ; jump if IRR0
                mov     edx,TimerRollover.qw_low                ;
@@tqt32_nopending:
                add     edx,Timer.qw_low                        ; add current time
                mov     eax,edx                                 ;
                mov     edx,Timer.qw_high                       ;
                adc     edx,0                                   ;
                jno     @@tqt32_exit                            ;
                mov     eax,-1                                  ; overflow? store
                mov     edx,7FFFFFFFh                           ; max __int64 value
@@tqt32_exit:
                popfd                                           ;
                pop     ds                                      ;
                assume  ds:nothing                              ;
                ret                                             ;
DHTmr32QueryTime endp                                           ;
; ---------------------------------------------------------------
                public  DHSetMask32
DHSetMask32     proc    near                                    ;
                pushfd                                          ; save and disable interrupts
                cli                                             ;
                out     PIC1_PORT1,al                           ; update mask in PIC1
                xchg    al,ah                                   ;
                out     PIC2_PORT1,al                           ; update mask in 2nd PIC
                xchg    al,ah                                   ;
                popfd                                           ; restore interrupt flag
                ret                                             ;
DHSetMask32     endp                                            ;
; ---------------------------------------------------------------
                public  DHGetMask32
DHGetMask32     proc    near                                    ;
                pushfd                                          ; save and disable interrupts
                cli                                             ;
                mov     eax,-1                                  ; mask IRQ>16
                in      al,PIC2_PORT1                           ; get mask 8-15
                xchg    al,ah                                   ;
                in      al,PIC1_PORT1                           ; get mask 0-7
                popfd                                           ; restore interrupt flag
                ret                                             ;
DHGetMask32     endp                                            ;
; ---------------------------------------------------------------
FPUSW_BUSY      equ     1000000000000000b
FPUSW_ERROR     equ     0000000010000000b
FPUCW_INVOP     equ     0000000000000001b
FPUCW_INVOPOFF  equ     1111111111111110b

                public  DHVNPXReset
DHVNPXReset     proc    near                                    ;
                cmp     ecx,NPX_REAL_PORT                       ;
                jz      @@dhnpxr_doit                           ; check port number
                stc                                             ;
                ret                                             ;
@@dhnpxr_doit:                                                  ;
                push    ds                                      ;
                push    eax                                     ;
                fninit                                          ; reset FPU
                fninit                                          ;
                fnstsw  ax                                      ;
                or      ax,FPUSW_BUSY+FPUCW_INVOP+FPUSW_ERROR   ; set status word
                fstsw   ax                                      ;
                push    DOSHLP_DATASEL                          ;
                pop     ds                                      ;
                assume  ds:DOSHLP_DATA                          ;
                fnstcw  FPU_CtrlWord                            ;
                and     FPU_CtrlWord,FPUCW_INVOPOFF             ; reset mask bit for inv. ops
                fldcw   FPU_CtrlWord                            ;
                clc                                             ;
                pop     eax                                     ;
                pop     ds                                      ;
                assume  ds:nothing                              ;
                ret                                             ;
DHVNPXReset     endp                                            ;
; ---------------------------------------------------------------
                public  DHWaitNPX
DHWaitNPX       proc    near                                    ;
                push    ecx                                     ;
                mov     ecx,80h                                 ;
                loop    $                                       ; small delay
                pop     ecx                                     ;
                ret                                             ;
DHWaitNPX       endp                                            ;
; ---------------------------------------------------------------
; note: SMP kernel call this function from secondary CPU`s init.
;       here we can synchronize MTRRs with CPU0 
;
                public  DHClrBusyNPX
DHClrBusyNPX    proc    near                                    ;
                push    eax                                     ;
                mov     eax,cr0                                 ;
                test    eax,CR0_EM + CR0_TS                     ; check for exceptions
                jnz     @@dhcnpx_nobusy                         ;
                fnstsw  ax                                      ; query status
                test    ax,FPUSW_ERROR                          ;
                jz      @@dhcnpx_nobusy                         ;
                xor     al,al                                   ; port out
                out     NPX_BUSY_PORT,al                        ;
@@dhcnpx_nobusy:
                call    @@dhcnpx_label                          ; get flat doshlp pointer
@@dhcnpx_label:
                pop     eax                                     ;
                sub     eax,offset @@dhcnpx_label               ;
                pushad                                          ;
                mov     esi,eax                                 ; doshlp flat
                xor     edx,edx                                 ;
                movzx   eax,word ptr [External.ClockMod+esi]    ;
                or      eax,eax                                 ;
                jz      @@dhcnpx_no_clockmod                    ;
                test    [External.Flags+esi], EXPF_AMDCPU       ; is it AMD?
                jnz     @@dhcnpx_no_clockmod                    ;
                add     al,10h                                  ; set clock modulation
                mov     ecx,MSR_IA32_CLOCKMODULATION            ; on Intel
                wrmsr                                           ;
@@dhcnpx_no_clockmod:                
                cmp     word ptr [External.MsrTableCnt+esi],dx  ;
                jz      @@dhcnpx_no_msrlist                     ; MSR setup list is empty
                test    [External.Flags+esi],EXPF_DISCARDED     ;
                jnz     @@dhcnpx_no_msrlist                     ; is high part alive?
                mov     edi,esi                                 ;
                sub     edi,[esi+PublicInfo.DosHlpFlatBase]     ; boothlp flat
                add     edi,[esi+PublicInfo.LdrHighFlatBase]    ;
                lea     eax,[edi+DHMsr_Setup]                   ; setup MSRs
                call    eax                                     ;
@@dhcnpx_no_msrlist:
                popad                                           ;
                pop     eax                                     ;
                ret                                             ;
DHClrBusyNPX    endp                                            ;
; ---------------------------------------------------------------
                public  DHAckIntNPX
DHAckIntNPX     proc    near                                    ;
                push    eax                                     ;
                mov     eax,IRQ_NPX                             ; send EOI for FPU
                call    DHSendEOI                               ;
                pop     eax                                     ;
                ret                                             ;
DHAckIntNPX     endp                                            ;
; ---------------------------------------------------------------
                public  DHSendEOI
DHSendEOI       proc    near                                    ;
                push    eax                                     ;
                pushfd                                          ; disable interrupts
                cli                                             ;
                cmp     al,NUM_IRQ                              ; check IRQ number
                jnb     @@dhseoi_exit                           ;
                mov     ah,OCW2_NON_SPECIFIC_EOI                ;
                xchg    ah,al                                   ;
                cmp     ah,NUM_IRQ_PER_PIC                      ;
                jl      @@dhseoi_1                              ;
                out     PIC2_PORT0,al                           ; send EOI to slave
@@dhseoi_1:
                out     PIC1_PORT0,al                           ; send EOI to master
@@dhseoi_exit:
                popfd                                           ; restore interrupt flag
                pop     eax                                     ;
                ret                                             ;
DHSendEOI       endp                                            ;
; ---------------------------------------------------------------
                public  DHGetISR                                ;
DHGetISR        proc    near                                    ;
                pushf                                           ; save flags
                cli                                             ; disable interrupts
                xor     eax,eax                                 ;
                mov     al,OCW3_READ_ISR                        ;
                out     PIC1_PORT0,al                           ;
                out     PIC2_PORT0,al                           ;
                in      al,PIC2_PORT0                           ; check isr on slave pic
                mov     ah,al                                   ; save master pic value
                in      al,PIC1_PORT0                           ; check isr on master pic
                popf                                            ; restore flags
                ret                                             ;
DHGetISR        endp                                            ;
; ---------------------------------------------------------------
                public  DHGetIRR
DHGetIRR        proc    near                                    ;
                pushf                                           ;
                cli                                             ;
                xor     eax,eax                                 ;
                mov     al,OCW3_READ_IRR                        ;
                out     PIC1_PORT0,al                           ;
                out     PIC2_PORT0,al                           ;
                in      al,PIC2_PORT0                           ; check IRR on master pic
                mov     ah,al                                   ; save master pic value
                in      al,PIC1_PORT0                           ; check IRR on slave pic
                popf                                            ;
                ret                                             ;
DHGetIRR        endp                                            ;
;
; Write CMOS reg
; ---------------------------------------------------------------
; In: AL = register, AH = data
WriteCMOS32     proc    near                                    ;
                out     CMOS_ADDR,al                            ;
                xchg    ah,al                                   ;
                out     CMOS_DATA,al                            ;
                ret                                             ;
WriteCMOS32     endp                                            ;
;
; Read CMOS reg
; ---------------------------------------------------------------
; In : AL = register
; Out: AL = data
ReadCMOS32      proc    near                                    ;
                out     CMOS_ADDR,al                            ;
                in      al,CMOS_DATA                            ;
                ret                                             ;
ReadCMOS32      endp                                            ;
; ---------------------------------------------------------------
SetupCMOS       proc    near
                push    ecx                                     ;
                mov     ax,[di+2]                               ; Reg B
                call    WriteCMOS32                             ;
                mov     ax,[di]                                 ; Reg A
                call    WriteCMOS32                             ;
                mov     ecx, 4                                  ;
@@scm_loop:
                mov     al,CMOS_REG_C                           ; Reg C
                call    ReadCMOS32                              ;
                loop    @@scm_loop                              ;
                mov     al,CMOS_REG_D                           ; Reg D
                call    ReadCMOS32                              ;
                pop     ecx                                     ;
                ret                                             ;
SetupCMOS       endp
; ---------------------------------------------------------------
                public  DHProtModeRTCRegs
DHProtModeRTCRegs proc  near                                    ;
                push    ds                                      ;
                push    eax                                     ;
                push    edi                                     ;
                mov     ax,DOSHLP_DATASEL                       ;
                mov     ds,ax                                   ;
                lea     di,PM_RTCRegs                           ;
                call    SetupCMOS                               ;
                pop     edi                                     ;
                pop     eax                                     ;
                pop     ds                                      ;
                ret                                             ;
DHProtModeRTCRegs endp                                          ;
; ---------------------------------------------------------------
                public  DHRealModeRTCRegs
DHRealModeRTCRegs proc  near                                    ;
                push    ds                                      ;
                push    eax                                     ;
                push    edi                                     ;
                mov     ax,DOSHLP_DATASEL                       ;
                mov     ds,ax                                   ;
                lea     di,PM_RTCRegs                           ;
                mov     al,CMOS_REG_A                           ; update reg A
                call    ReadCMOS32                              ;
                and     al,7Fh                                  ; drop NMI flag
                mov     [di+1],al                               ;
                mov     al,CMOS_REG_B or NMI_MASK               ; update reg B
                call    ReadCMOS32                              ;
                mov     [di+3],al                               ;
                mov     al,CMOS_REG_D                           ; read reg D
                call    ReadCMOS32                              ;
                lea     di,RM_RTCRegs                           ;
                call    SetupCMOS                               ;
                pop     edi                                     ;
                pop     eax                                     ;
                pop     ds                                      ;
                ret                                             ;
DHRealModeRTCRegs endp                                          ;
; ---------------------------------------------------------------
                public  DHGetDHPICVars                          ;
DHGetDHPICVars  proc    near                                    ;
                mov     bx,DOSHLP_DATASEL                       ;
                mov     es,bx                                   ;
                mov     ebx, offset SystemIRQMasks              ;
                mov     esi, offset PM_PICMask                  ;
                mov     edi, offset RM_PICMask                  ;
                ret                                             ;
DHGetDHPICVars  endp                                            ;
; ---------------------------------------------------------------
                public  DHSetIRQMask                            ;
DHSetIRQMask    proc    near                                    ;
                push    eax                                     ;
                push    ebx                                     ;
                push    ecx                                     ;
                push    edx                                     ;
                pushfd                                          ; save and disable interrupts
                cli                                             ;
                cmp     al,NUM_IRQ                              ;
                jae     @@dhsim_exit                            ; jump if not slave
                mov     edx,1                                   ;
                mov     cl,al                                   ; irq number
                shl     edx,cl                                  ;
                not     edx                                     ; AND mask
                and     ah,1                                    ; enable/disable bit
                movzx   ebx,ah                                  ;
                shl     ebx,cl                                  ; OR mask
                cmp     al,NUM_IRQ_PER_PIC                      ;
                jae     @@dhsim_pic2                            ;
                in      al,PIC1_PORT1                           ; update mask in PIC1
                and     al,dl                                   ;
                or      al,bl                                   ;
                out     PIC1_PORT1,al                           ;
                jmp     @@dhsim_exit                            ;
@@dhsim_pic2:
                in      al,PIC2_PORT1                           ; update mask in PIC2
                and     al,dh                                   ;
                or      al,bh                                   ;
                out     PIC2_PORT1,al                           ;
                or      ah,ah                                   ; enabling slave IRQ?
                jnz     @@dhsim_exit                            ;
                in      al,PIC1_PORT1                           ; yes, enable it in master PIC
                mov     ah,(1 SHL PIC_SLAVE_IRQ)                ;
                not     ah                                      ;
                and     al,ah                                   ;
                out     PIC1_PORT1,al                           ;
@@dhsim_exit:
                popfd                                           ;
                pop     edx                                     ;
                pop     ecx                                     ;
                pop     ebx                                     ;
                pop     eax                                     ;
                ret                                             ;
DHSetIRQMask    endp                                            ;
; ---------------------------------------------------------------
                public  DHTmrSetRollover
DHTmrSetRollover proc   near                                    ;
                push    eax
                pushfd                                          ; disable ints
                cli                                             ;
                push    ds                                      ;
                mov     ax,DOSHLP_DATASEL                       ;
                mov     ds,ax                                   ;
                assume  DS:DGROUP                               ;
                mov     TimerRollover.qw_high,ecx               ; store next rollover count.
                pop     ds                                      ;
                assume  ds:nothing                              ;
                mov     al,SC_CNT0+RW_LSBMSB+CM_MODE2           ;
                out     PORT_CW,al                              ; set count mode for timer 0
                mov     eax,ecx                                 ;
                call    DH32_Delay                              ;
                out     PORT_CNT0,al                            ; LSB count
                call    DH32_Delay                              ;
                mov     al,ah                                   ;
                out     PORT_CNT0,al                            ; MSB count
                popfd                                           ;
                pop     eax                                     ;
                ret                                             ;
DHTmrSetRollover endp                                           ;
; ---------------------------------------------------------------
; IRQ routers

IrqEntryList    macro   numbers
                irp     x,<numbers>
                align   8                                       ; align all to 8
                public  DHIRQ&x&Router
DHIRQ&x&Router  proc    near
                db      06Ah                                    ; push byte
                db      x                                       ;
                public  DHIRQ&x&Jump                            ;
; near ptr must be here, because DHInitInterrupts patch it to
; Warp router if Warp kernel was loaded...
DHIRQ&x&Jump    label   near                                    ;
                jmp     near ptr CommonIRQRouter                ;
DHIRQ&x&Router  endp
                endm
                endm

                IrqEntryList <0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23>
                IrqEntryList <24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47>

IRQR_REAL_SIZE  equ     DHIRQ1Router-DHIRQ0Router               ;
                .errnz  (IRQR_SIZE - IRQR_REAL_SIZE)            ;
                .errnz  (size irqi - 16)                        ;
                .errnz  (size irqi3 - 8)                        ;

                align   16
                public  CommonIRQRouter                         ;
CommonIRQRouter label   near                                    ;
                sub     esp,ISF_STACK_PAD-4                     ;
                pushad                                          ;
                push    ds                                      ; saving segs
                push    es                                      ;
                push    fs                                      ;
                push    gs                                      ;
                SetFlatDS                                       ; ds/es to FLAT
                mov     ds,eax                                  ;
                mov     es,eax                                  ; another one magic
                xor     ebx,ebx                                 ;
                mov     ax,ss                                   ; ss/esp fix:
                CmpFlatDS CMPFLATDSTYPE                         ;
                jz      @@cirq_jump                             ; check for various
                CmpFlatDS PUSHFLAT_PSSEL                        ; kernel stack
                jz      @@cirq_jump                             ; selectors
                CmpFlatDS PUSHFLAT_HSSEL                        ;
                jz      @@cirq_jump                             ;
                CmpFlatDS PUSHFLAT_SSSEL                        ;
                jz      @@cirq_jump                             ;
                movzx   esp,sp                                  ;
@@cirq_jump:
                mov     bl,[esp+size pushad_s+ISF_STACK_PAD-4+16] ;
                shl     ebx,4                                   ; airq step
                LDIrqB                                          ;
                KJmp    IRQRouter                               ; go to kernel

; Warp kernel router. Warp Server SMP router must be the same,
; but with 16 bytes airq and not implemented now.
                align   16
                public  CommonIRQWarp                           ;
CommonIRQWarp   label   near                                    ;
                sub     esp,ISF_STACK_PAD-4                     ;
                pushad                                          ;
                xor     ebx,ebx                                 ;
                mov     ax,ss                                   ; ss/esp fix:
                CmpFlatDS CMPFLATDSTYPE                         ;
                jz      @@cwirq_jump                            ; check for various
                CmpFlatDS PUSHFLAT_PSSEL                        ; kernel stack
                jz      @@cwirq_jump                            ; selectors
                movzx   esp,sp                                  ;
@@cwirq_jump:                                                   ;
                mov     bl,[esp+size pushad_s+ISF_STACK_PAD-4]  ;
                shl     ebx,3                                   ; airq step
                LDIrqB                                          ;
                KJmp    IRQRouter                               ; go to kernel

DOSHLP32_CODE   ends

DOSHLP32_DATA   segment
                public KAT
KAT             KernelAccess <>
DOSHLP32_DATA   ends

BOOTHLP_CODE    segment
                extrn   DosHlpFunctions:near                    ;
                extrn   GetSysConfig:far                        ;
                extrn   DHStubInit:far                          ;
BOOTHLP_CODE    ends

BOOTHLP_DATA    segment
                public  _dd_bootdisk,dd_bootflags               ;
                public  RouterDiff                              ;
_dd_bootdisk    db      0                                       ; boot disk & flags (accessed as dw!!)
dd_bootflags    db      0                                       ;
RouterDiff      dd      offset CommonIRQWarp - offset CommonIRQRouter
BOOTHLP_DATA    ends

BOOTHLP32_CODE  segment
                extrn   DHMsr_Setup:near                        ;
BOOTHLP32_CODE  ends

_BSS            segment
                extrn   MsrSetupTable:byte                      ;
                extrn   Io32FixupTable:dword                    ;
                extrn   _BootBPB:dword                          ;
_BSS            ends

                end
