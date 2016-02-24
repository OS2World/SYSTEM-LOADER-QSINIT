;
; QSINIT
; help calls, 16-bit part
; --------------------------------------------------------------------
; 16 & 32 function must be separated, because wasm make wrong 16-bit
; offsets when import 16-bit data

                NOSTORSAVE  = 1
                include inc/segdef.inc
                include inc/seldesc.inc
                include inc/qstypes.inc
                include inc/serial.inc
                include inc/debug.inc
                include inc/basemac.inc
                include inc/vbedata.inc
                include inc/cpudef.inc

DATA16          segment                                         ;
                extrn   _logbufseg:word                         ;
                extrn   _DiskBufRM_Seg:word                     ;
                extrn   _syslapic:dword                         ;
                extrn   _apic_tmr_vnum:byte                     ;
                public  _ComPortAddr, _BaudRate, _state_rec_len ;
                public  _logrmbuf, _logrmpos, _page0_fptr       ;
                public  rm_timer_ofs, rm_spur_ofs               ;
                public  apic_tmr_rcnt, apic_tmr_reoi            ;
                align   4                                       ;
_logrmbuf       dd      0                                       ;
_BaudRate       dd      BD_115200                               ; current dbport baud rate
_ComPortAddr    dw      0                                       ; and address
_logrmpos       dw      0                                       ;

ifdef INITDEBUG
vio_out_proc    dw      offset printchar_rm                     ; vio prn char for
                dw      offset printchar_pm                     ; RM and PM
endif
rm_timer_ofs    dw      offset rm_timer                         ;
rm_spur_ofs     dw      offset rm_spurious                      ;
apic_tmr_rcnt   dw      0                                       ; counters
apic_tmr_reoi   dw      0                                       ;

mini_gdt        desctab_s <0,0,0,0,0,0>                         ;
                desctab_s <0FFFFh,0,0,D_DATA0,D_DBIG or D_GRAN4K or D_AVAIL or 0Fh,0>
mini_gdt_ptr    dw      $-mini_gdt-1, offset mini_gdt, 0, 0     ;

; in non-paged mode FLAT DS have no 0 page.
; in paged mode, zero page mapped as r/o, and separately as r/w.
; This is page 0 access actual far32 pointer, this value updated
; by START module when it turns on PAE.
_page0_fptr     dd      0                                       ;
                dd      SELZERO                                 ;
_state_rec_len  dw      size tss_s                              ;
DATA16          ends                                            ;

_BSS16          segment
                public  _storage_w                              ;
_storage_w      db      STO_BUF_LEN * STOINIT_ESIZE dup(?)      ;
_storage_w_end  label   near                                    ;
                public  _mfsd_openname, _mfsd_fsize             ;
_mfsd_openname  db      MFSD_NAME_LEN dup(?)                    ;
_mfsd_fsize     dd      ?                                       ;
_BSS16          ends

                extrn   enablea20:near                          ;

;================================================================
;
; warning! many of functions below does not save registers because
;          called from protected mode only!


TEXT16          segment
                assume  cs:G16, ds:G16, es:nothing, ss:G16

                public  _getkeyflags
_getkeyflags    proc    far                                     ;
                mov     ah,02h                                  ; read keyboard status
                int     16h                                     ;
                push    ax                                      ;
                mov     ah,12h                                  ;
                int     16h                                     ;
                pop     cx                                      ;
                mov     al,cl                                   ;
                ret                                             ;
_getkeyflags    endp

; set keyboard rate and delay
;----------------------------------------------------------------
                public  _setkeymode                             ;
_setkeymode     proc    far                                     ;
                mov     bp,sp                                   ;
                mov     ax,0305h                                ;
                mov     bx,[bp+4]                               ;
                int     16h                                     ;
                ret     4                                       ;
_setkeymode     endp                                            ;

; set 80x25, 80x43 or 80x50 text mode
;----------------------------------------------------------------
                public  _settextmode                            ;
_settextmode    proc    far                                     ;
                mov     bp,sp                                   ;
                mov     ah,12h                                  ;
                mov     al,[bp+4]                               ;
                mov     bl,30h                                  ; 350/400 lines
                int     10h                                     ;
                mov     ax,0003h                                ;
                int     10h                                     ;
                mov     ax,1112h                                ; font height
                add     al,[bp+5]                               ;
                xor     bl,bl                                   ;
                int     10h                                     ;
                mov     ax,1003h                                ; blink off
                xor     bl,bl                                   ;
                int     10h                                     ;
                ret     4                                       ;
_settextmode    endp                                            ;

; is screen in text mode? (common or vesa)
;----------------------------------------------------------------
                public  _istextmode                             ;
_istextmode     proc    far                                     ;
                mov     ax,4F03h                                ;
                int     10h                                     ;
                or      ah,ah                                   ;
                jnz     @@ismode_old                            ;
                and     bx,1FFFh                                ; zero flags
                cmp     bx,100h                                 ;
                jc      @@ismode_old                            ;
                mov     cx,bx                                   ;
                mov     es,_DiskBufRM_Seg                       ;
                xor     di,di                                   ;
                mov     ax,4F01h                                ; check vesa mode
                int     10h                                     ;
                cmp     ax,004Fh                                ;
                jnz     @@ismode_old                            ;
                mov     ax,es:[di].ModeAttributes               ;
                and     ax,VSMI_BIOSSUPP or VSMI_GRAPHMODE      ;
                cmp     ax,VSMI_BIOSSUPP                        ;
                setz    al                                      ;
                ret                                             ;
@@ismode_old:
                mov     ah,0Fh                                  ;
                int     10h                                     ;
                cmp     al,4                                    ; <=3?
                setc    al                                      ;
                ret                                             ;
_istextmode     endp                                            ;

; reset mode to 80x25 or clear screen if mode was not changed.
; this check used to prevent screen blinking on unnessesary mode switch
;----------------------------------------------------------------
                public  _checkresetmode                         ;
_checkresetmode proc    far                                     ;
                push    fs                                      ; warning! arg usage below!
                mov     ax,4F03h                                ;
                int     10h                                     ;
                or      ah,ah                                   ;
                jnz     @@crmode_old                            ;
                cmp     bx,100h                                 ;
                jnc     @@crmode_reset                          ;
@@crmode_old:
                mov     ah,0Fh                                  ;
                int     10h                                     ;
                cmp     al,3                                    ;
                jnz     @@crmode_reset                          ;

                xor     ax,ax                                   ;
                mov     fs,ax                                   ;
ifndef INITDEBUG
                mov     al,fs:[484h]                            ; 25 lines?
                cmp     al,24                                   ; do not check in
                jnz     @@crmode_reset                          ; verbose build!
endif
                mov     ax,fs:[44Ah]                            ;
                cmp     ax,80                                   ; 80 cols?
                jz      @@crmode_ok                             ;
@@crmode_reset:
                mov     ax,1202h                                ;
                mov     bl,30h                                  ;
                int     10h                                     ;
                mov     ax,0003h                                ;
                int     10h                                     ;
                xor     bl,bl                                   ;
                mov     ax,1114h                                ; reset font after
                int     10h                                     ; EGA/VGA text mode
                jmp     @@crmode_exit                           ;
@@crmode_ok:
                mov     bp,sp                                   ; need clear?
                mov     ax,[bp+6]                               ;
                or      ax,ax                                   ;
                jz      @@crmode_exit                           ;
                mov     ax,40h                                  ; clear screen
                mov     es,ax                                   ;
                mov     dh,es:[84h]                             ;
                mov     dl,es:[4Ah]                             ;
                dec     dl                                      ;
                xor     cx,cx                                   ;
                mov     ax,0600h                                ;
                mov     bh,07h                                  ;
                int     10h                                     ;
@@crmode_exit:
                mov     ax,1003h                                ; set intensity
                xor     bl,bl                                   ; (EGA BIOS)
                int     10h                                     ;
                pop     fs                                      ;
                ret     4                                       ;
_checkresetmode endp                                            ;

; test something
;----------------------------------------------------------------
; in : [sp+4] = function index
; out: dx:ax  = -1 - invalid index, else function result
                public  _biostest
_biostest       proc    far
                mov     bp,sp                                   ;
                mov     bx,[bp+4]                               ;
                cmp     bx,biostestsize                         ;
                jnc     @@bt_err                                ;
                shl     bx,1                                    ;
                jmp     word ptr @@bt_offsets[bx]               ;
@@bt_err:
                mov     ax,-1                                   ;
                mov     dx,ax                                   ;
                ret                                             ;
@@bt_apm:
                mov     ax,5300h                                ; APM presence
                xor     bx,bx                                   ;
                int     15h                                     ; ret
                setc    dl                                      ; 0x100xx on err
                jnc     @@bt_apm_1                              ;
                mov     al,ah                                   ;
                xor     ah,ah                                   ;
@@bt_apm_1:
                xor     dh,dh                                   ; 0x00ver on ok
                ret                                             ;
@@bt_pci:
                mov     ax,0B101h                               ; PCI presence
                xor     edi,edi                                 ;
                int     1Ah                                     ;
                jc      @@bt_pci_fail                           ;
                cmp     edx,020494350h                          ;
                jnz     @@bt_pci_fail                           ;
                mov     ah,cl                                   ;
                mov     dx,bx                                   ;
                ret                                             ;
@@bt_pci_fail:
                xor     ax,ax                                   ;
                xor     dx,dx                                   ;
                ret                                             ;
@@bt_offsets:
                dw      offset  @@bt_apm
                dw      offset  @@bt_pci
@@bt_tabend:
biostestsize    equ     (offset @@bt_tabend - offset @@bt_offsets) shr 1
_biostest       endp

; apm poweroff/suspend
;----------------------------------------------------------------
; in : [sp+4] = 02 - suspend, 03 - power off
; out: exit
                public  _poweroffpc
_poweroffpc     proc    far
                mov     bp,sp                                   ;
                mov     ax,5300h                                ;
                xor     bx,bx                                   ;
                int     15h                                     ;
                dbg16print <"5300: %x %x %x",10>,<cx,bx,ax>
                jc      @@hlt_loop                              ;
                push    ax                                      ;

                mov     ax,5301h                                ;
                xor     bx,bx                                   ;
                int     15h                                     ;
                jnc     @@hlt_connected                         ;
                cmp     ah,2                                    ; connected?
                jnz     @@hlt_loop                              ;
@@hlt_connected:
                pop     cx                                      ;
                cmp     cx,100h                                 ;
                mov     bx,-1                                   ; FFFF for APM 1.0
                jz      @@hlt_ver100                            ;
                mov     ax,530Eh                                ; set org version
                xor     bx,bx                                   ;
                int     15h                                     ;
                dbg16print <"530E: %x",10>,<ax>
                mov     bx,1                                    ;
@@hlt_ver100:
                mov     ax,5308h                                ; enable APM
                mov     cx,1                                    ;
                int     15h                                     ;
                dbg16print <"5308: %x",10>,<ax>
                mov     ax,530Fh                                ; engage APM
                mov     bx,1                                    ;
                mov     cx,bx                                   ;
                int     15h                                     ;
                dbg16print <"530F: %x",10>,<ax>
                mov     ax,530Dh                                ; enable device APM
                mov     bx,1                                    ;
                mov     cx,bx                                   ;
                int     15h                                     ;
                dbg16print <"530D: %x",10>,<ax>

                mov     ax,5307h                                ;
                mov     cx,[bp+4]                               ;
                mov     bx,1                                    ;
                int     15h                                     ;
                dbg16print <"5307: %x",10>,<ax>

                mov     ax,5304h                                ; disconnect
                xor     bx,bx                                   ;
                int     15                                      ;
@@hlt_loop:
                mov     sp,bp                                   ;
                ret                                             ;
_poweroffpc     endp

;----------------------------------------------------------------
;void* __stdcall memmove16(void *dst, const void *src, u32t length);
                public  _memmove16                              ;
_memmove16      proc    near                                    ;
@@mmv16_dst     =  4                                            ;
@@mmv16_src     =  6                                            ;
@@mmv16_length  =  8                                            ;
                push    bp                                      ;
                mov     bp,sp                                   ;
                push    es                                      ;
                push    cx                                      ;
                push    si                                      ;
                push    ds                                      ;
                pop     es                                      ;
                mov     si,[bp+@@mmv16_src]                     ;
                push    di                                      ;
                mov     di,[bp+@@mmv16_dst]                     ;
                mov     cx,[bp+@@mmv16_length]                  ;
                test    cx,cx                                   ;
                jz      @@mmv16_move0                           ;
                cmp     si,di                                   ;
                jae     @@mmv16_forward                         ;
                std                                             ; move backward
                add     si,cx                                   ;
                add     di,cx                                   ;
                mov     ax,cx                                   ;
                and     cx,3                                    ;
                dec     si                                      ;
                dec     di                                      ;
            rep movsb                                           ;
                mov     cx,ax                                   ;
                shr     cx,2                                    ;
                jz      @@mmv16_move0                           ;
                sub     si,3                                    ;
                sub     di,3                                    ;
            rep movsd                                           ;
                jmp     @@mmv16_move0                           ;
@@mmv16_forward:
                cld                                             ; move forward
@@mmv16_align:
                test    di,3                                    ; make sure data is aligned
                jz      @@mmv16_aligned                         ;
                movsb                                           ;
                dec     cx                                      ;
                jz      @@mmv16_move0                           ;
                jmp     @@mmv16_align                           ;
@@mmv16_aligned:
                mov     ax,cx                                   ;
                shr     cx,2                                    ;
                and     al,3                                    ;
            rep movsd                                           ;
                mov     cl, al                                  ;
            rep movsb                                           ;
@@mmv16_move0:
                pop     di                                      ;
                pop     si                                      ;
                cld                                             ;
                pop     cx                                      ;
                pop     es                                      ;
                mov     ax,[bp+@@mmv16_dst]                     ;
                mov     sp,bp                                   ;
                pop     bp                                      ;
                ret     6                                       ;
_memmove16      endp

;
; char appending to last line in realmode log buffer
; zero assumed at _logbufseg pos (end of string char)
; if char prior to zero is \n - new log line will be started
;
                public  _seroutchar_w
_seroutchar_w   proc    near                                    ;
                push    dx                                      ;
                push    ax                                      ;
                mov     dx,cs:_ComPortAddr                      ; port addr present?
                or      dx,dx                                   ;
                jz      @@serout16_exit                         ;
                add     dx,COM_LSR                              ; status port
@@serout16_wait:
                in      al,dx                                   ; check status
                test    al,020h                                 ;
                jz      @@serout16_wait                         ; wait
                mov     dx,cs:_ComPortAddr                      ;
                mov     al,[esp+6]                              ; character from stack
                out     dx,al                                   ;

                cmp     al,13                                   ; skip \r
                jz      @@serout16_exit                         ;
                push    di                                      ;
                mov     di,cs:_logrmpos                         ;
                cmp     di,LOGBUF_SIZE-5-4                      ; rm buffer is full?
                jnc     @@serout16_logskip                      ;
                push    es                                      ;
                mov     es,cs:_logbufseg                        ;
                cld                                             ;
                or      di,di                                   ; 1st line in buffer?
                jz      @@serout16_firstline                    ;
                cmp     byte ptr es:[di-1],10                   ; last char is '\n'?
                jnz     @@serout16_logadd                       ;
                inc     di                                      ; skip 0
@@serout16_firstline:
                xchg    ah,al                                   ;
                mov     al,32h                                  ; flags + level
                stosb                                           ;
                xor     al,al                                   ; save zero time
                stosb                                           ; (we have troubles
                stosb                                           ; with current time
                stosb                                           ; on EFI host only)
                stosb                                           ;
                xchg    ah,al                                   ;
@@serout16_logadd:
                stosb                                           ;
                mov     byte ptr es:[di],0                      ; end of string
                mov     cs:_logrmpos,di                         ;
                pop     es                                      ;
@@serout16_logskip:
                pop     di                                      ;
@@serout16_exit:
ifdef INITDEBUG                                                 ;
                smsw    dx                                      ; additional output
                and     dx,1                                    ; to screen while 16bit
                xchg    dx,bx                                   ; init active
                shl     bx,1                                    ;
                mov     bx, cs:vio_out_proc[bx]                 ;
                xchg    dx,bx                                   ;
                call    dx                                      ;
endif
                pop     ax                                      ;
                pop     dx                                      ;
                ret     2                                       ;
_seroutchar_w   endp

ifdef INITDEBUG                                                 ;
printchar_pm    proc    near                                    ;
                ret                                             ;
printchar_pm    endp                                            ;

                public  printchar_rm
printchar_rm    proc    near
                assume  ds:nothing, ss:nothing

                SaveReg <ds,es>                                 ; preserve registers
                pusha                                           ;
                mov     bx, 0B800h                              ; text memory
                mov     ds, bx                                  ;
                mov     bx, 40h                                 ; bios data 40h
                mov     es, bx                                  ;

                mov     cx, es:[50h]                            ; cursor pos
                xor     bx, bx                                  ;
                cmp     al, 0Ah                                 ; EOL?
                je      @@eol                                   ;
                cmp     al, 0Dh                                 ; CR?
                jnz     @@nocr                                  ;
                xor     cl, cl                                  ;
                jmp     @@savepos                               ;
@@nocr:
                mov     bl, ch                                  ; offset
                imul    bx, 160                                 ;
                xor     ch, ch                                  ;
                shl     cx, 1                                   ;
                add     bx, cx                                  ; printing
                mov     ah, 0Ch                                 ; color
                mov     [bx], ax                                ;
                mov     cx, es:[50h]                            ; update cursor pos
                inc     cl                                      ; ch - row
                cmp     cl, 80                                  ;
                jc      @@savepos                               ;
@@eol:
                xor     cl, cl                                  ; new line
                movzx   bx, byte ptr es:[84h]                   ; number of lines
                cmp     ch, bl                                  ;
                jnz     @@noscroll                              ;
                mov     es:[50h], cx                            ; save cursor pos
                push    ds                                      ;
                pop     es                                      ;
                mov     si, 160                                 ; scrolling ;)
                xor     di, di                                  ;
                imul    cx, bx, 80                              ; (lines-1) * 80
            rep movsw                                           ;
                mov     cl, 80h                                 ;
                mov     ax, 0720h                               ; cleaning last line
            rep stosw                                           ;
                jmp     @@exit                                  ;
@@noscroll:
                inc     ch                                      ; новая строка
@@savepos:
                mov     es:[50h],cx                             ; сохраняем позицию курсора
@@exit:
                popa                                            ;
                RestReg <es,ds>                                 ;
                ret                                             ;
printchar_rm    endp
endif

;
; get dgroup segment/selector (dual mode rm/pm proc)
;----------------------------------------------------------------
; OUT : AX - value
get_dgroup      proc    near
                smsw    ax                                      ;
                test    al,1                                    ;
                jz      @@getds_rm                              ;
                mov     ax,SELDATA                              ;
                ret                                             ;
@@getds_rm:
                mov     ax,cs                                   ;
                ret                                             ;
get_dgroup      endp

;
; save dword into storage (dual mode rm/pm proc)
;----------------------------------------------------------------
; IN : EAX - dword, SI - name, save all except flags
                public  sto_save_w
sto_save_w      proc    near
                pushad                                          ;
                SaveReg <ds,es>                                 ;
                push    eax                                     ; save value
                call    get_dgroup                              ; ds=es=dgroup
                mov     ds,ax                                   ;
                mov     es,ax                                   ;
                mov     bx,offset _storage_w                    ;
                xor     bp,bp                                   ; empty entry
@@svs_loop:
                lea     di,[bx+9]                               ;
                cmp     byte ptr [di],0                         ; name empty?
                jz      @@svs_empty                             ;
                push    si                                      ;
                mov     cx,11                                   ;
           repe cmpsb                                           ;
                pop     si                                      ;
                jnz     @@svs_nextloop                          ;
                jmp     @@svs_ready                             ;
@@svs_empty:
                mov     bp,bx                                   ; remember it
@@svs_nextloop:
                add     bx,STOINIT_ESIZE                        ;
                cmp     bx,offset _storage_w_end                ;
                jnz     @@svs_loop                              ; next entry
                mov     bx,bp                                   ; try to get empty
@@svs_ready:
                pop     eax                                     ;
                or      bx,bx                                   ; entry founded?
                jz      @@svs_failed                            ;
                mov     di,bx                                   ; store data &
                stosd                                           ; name
                mov     al,4                                    ;
                stosb                                           ; lo size byte
                xor     eax,eax                                 ;
                stosd                                           ; hi size + flag
                mov     cx,11                                   ;
            rep movsb                                           ;
@@svs_failed:
                RestReg <es,ds>                                 ;
                popad                                           ;
                ret                                             ;
sto_save_w      endp

;
; set FS to big real selector
;----------------------------------------------------------------
; actually, this is crazy thing - we enabling A20 inside interrupt :)
;
; but there is only one bugaboo, who can disable it - IBM`s PXE
; loader, which calls int 15h functions for memory cache i/o
;
rmflat_fs       proc    near
                pushad                                          ;
                call    enablea20                               ;
                xor     eax,eax                                 ;
                or      ax,word ptr cs:mini_gdt_ptr+4           ; prepare GDT address
                jnz     @@rmf_gdtready                          ;
                mov     ax,cs                                   ; we`re always above
                shl     eax,4                                   ; 1000h, so use 3rd
                add     dword ptr cs:mini_gdt_ptr+2,eax         ; word as a flag
@@rmf_gdtready:
                lgdt    fword ptr cs:mini_gdt_ptr               ; set PM
                mov     eax,cr0                                 ;
                or      al,CPU_CR0_PE                           ;
                and     eax,not (CPU_CR0_PG or CPU_CR0_AM)      ;
                mov     cr0,eax                                 ;
                jmp     $+2                                     ;
                mov     bx,8                                    ; loads FS
                mov     fs,bx                                   ;
                and     al,not CPU_CR0_PE                       ; and exit back
                mov     cr0,eax                                 ;
                popad                                           ;
                ret                                             ;
rmflat_fs       endp

;
; APIC timer RM vector ;)
;----------------------------------------------------------------
; install it actually, because no way to disable interrupt at 100%.
; Accepted interrupt can wait in IRR while we`re switching to RM, for
; example - and masking do nothing with it. Next it will crash VBox
; where high RM vectors are zero and stops timer on common BIOS with
; single iret without EOI.
;
                assume  ds:nothing, es:nothing, ss:nothing      ;
rm_timer        proc    near                                    ;
                push    eax                                     ;
                cli                                             ;
                mov     eax,cs:_syslapic                        ;
                or      eax,eax                                 ;
                jz      @@rmt_irq_noapic                        ;
                push    fs                                      ;
                call    rmflat_fs                               ;
                push    ebx                                     ;
                push    ecx                                     ;
                movzx   ebx,cs:_apic_tmr_vnum                   ;
                mov     ecx,ebx                                 ;
                and     cl,1Fh                                  ; check ISR bit
                shr     ebx,1                                   ; before sending
                and     bl,0F0h                                 ;
                bt      dword ptr fs:[eax+APIC_ISR_TAB*4+ebx],ecx ; EOI
                jnc     @@rmt_irq_notours                       ;
                xor     ebx,ebx                                 ;
                inc     cs:apic_tmr_reoi                        ; eoi counter
                mov     fs:[eax+APIC_EOI*4],ebx                 ;
@@rmt_irq_notours:
                inc     cs:apic_tmr_rcnt                        ; total counter
                pop     ecx                                     ;
                pop     ebx                                     ;
                pop     fs                                      ;
@@rmt_irq_noapic:
                pop     eax                                     ;
                iret                                            ;
rm_timer        endp                                            ;

rm_spurious     proc   near
                iret
rm_spurious     endp

TEXT16          ends
                end
