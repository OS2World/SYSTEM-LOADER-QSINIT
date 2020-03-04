;
; QSINIT
; real mode calls from 32 bit code
;
                include inc/segdef.inc
                include inc/qstypes.inc
                include inc/filetab.inc                         ;
                include inc/dpmi.inc                            ;
                include inc/debug.inc

                .486

                extrn   _rm_regs:rmcallregs_s
                extrn   _filetable:filetable_s                  ;
                extrn   _stack16_pos:word                       ;
                extrn   _rm16code:word                          ;
                extrn   _qs16base:dword                         ;
                extrn   sector_io:far                           ;
                extrn   panic_initpm_np:near                    ;
                extrn   _mfsd_openname:byte                     ;
                extrn   _mfsd_fsize:dword                       ;
                extrn   _rmpart_size:word                       ;
                extrn   _mt_swlock:near                         ;
                extrn   _mt_swunlock:near                       ;

RMC_IRET        = 40000000h                                     ; call to iret frame
RMC_EXITCALL    = 80000000h                                     ; exit from QSINIT

_BSS            segment                                         ;
save_edi        dd      ?                                       ;
save_esi        dd      ?                                       ; regs storage
save_ebp        dd      ?                                       ;
save_ebx        dd      ?                                       ;
save_ret        dd      ?                                       ;
save_ret2       dd      ?                                       ;
_BSS            ends                                            ;

_DATA           segment                                         ;
rmcall32ax      dw      0301h                                   ; dpmi func for rmcall
rmcall32bx      dw      0                                       ; flags in bx
                public  _cvio_blink, _mfs_rmcmode               ;
_cvio_blink     db      0                                       ;
_mfs_rmcmode    dw      (FN30X_LOCKI16H or FN30X_LOCKI10H) shl 8 ;
_DATA           ends                                            ;

_TEXT           segment
                assume  cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT

                extrn   _log_flush:near                         ;
                extrn   _key_filter:near                        ;
;----------------------------------------------------------------
; call rm: edx - addr, ecx - number of words in stack
;
rmcall32:
                mov     save_edi,edi                            ;
                mov     edi,offset _rm_regs                     ; do not use 32bit offset
                xor     eax,eax                                 ; zero fs, gs
                mov     dword ptr [edi+rmcallregs_s.r_fs],eax   ;
                mov     ax,_rm16code                            ;
                mov     [edi+rmcallregs_s.r_ds],ax              ; set ds to 9100h
                mov     [edi+rmcallregs_s.r_es],ax              ;
                mov     [edi+rmcallregs_s.r_ss],ax              ; set ss:sp to own stack,
                mov     ax,_stack16_pos                         ; use saved pointer
                mov     [edi+rmcallregs_s.r_sp],ax              ; the reason is a tiny model
                                                                ; of 16 bit code (ss=ds)
                mov     dword ptr [edi+rmcallregs_s.r_ip],edx   ; cs:ip
;----------------------------------------------------------------
; call rm: ecx - number of words in stack
;          edi - rmcallregs_s, orig. edi saved in save_edi
rmcall32regs:
                mov     save_esi,esi                            ;
                mov     save_ebp,ebp                            ;
                mov     save_ebx,ebx                            ;
                pushf                                           ;
                pop     _rm_regs.r_flags                        ; save flags
                pop     save_ret                                ; return addr

                xor     bx,bx                                   ;
                xchg    bx,rmcall32bx                           ; dpmi call
                mov     ax,0301h                                ; restore defaults
                xchg    ax,rmcall32ax                           ;
                int     31h                                     ;
                mov     eax,[edi+rmcallregs_s.r_edx]            ; restore dword result
                shl     eax,16                                  ; (dx:ax)
                mov     ax,word ptr [edi+rmcallregs_s.r_eax]    ;

                mov     dx,[edi+rmcallregs_s.r_flags]           ; combine flags
                pushf                                           ;
                pop     cx                                      ;
                and     dx,08D5h                                ; use OF, SF, ZF, AF, PF, CF
                and     cx,not 08D5h                            ; from real mode and
                or      dx,cx                                   ; preserve other
                push    dx                                      ;
                popf                                            ;

                mov     edi,save_edi                            ;
                mov     esi,save_esi                            ;
                mov     ebp,save_ebp                            ;
                mov     ebx,save_ebx                            ;
; restore all static vars before flushing rm log, because we can be
; recursive from log_flush()
                push    [save_ret]                              ;
                pushf                                           ;
                push    eax                                     ;
                call    _log_flush                              ;
                pop     eax                                     ;
; unlock it and reschedule
                call    _mt_swunlock                            ;
                popf                                            ;
                ret                                             ; return to caller

; convert edx to seg:ofs, but check for 64k limit
make_rmaddr     label   near                                    ;
                sub     edx,_qs16base                           ;
                push    ebx                                     ;
                mov     ebx,0FFFF0000h                          ;
                add     ebx,edx                                 ;
                jnc     @@vbox5_fix                             ; YES! now i used "into" ;)
                into                                            ; but stupid VBox 5.1.x signal ALWAYS!
@@vbox5_fix:
                mov     bx,_rm16code                            ; it will make trap 4 on wrong addr
                shl     ebx,16                                  ; (the easiest way to _throw_ here)
                or      edx,ebx                                 ;
                pop     ebx                                     ;
                ret                                             ;

;u16t _std mfs_open(void);
                public  _mfs_open                               ;
_mfs_open       label   near                                    ;
                mov     edx,offset _mfsd_fsize                  ;
                call    make_rmaddr                             ;
                push    edx                                     ;
                mov     edx,offset _mfsd_openname               ;
                call    make_rmaddr                             ;
                push    edx                                     ;
                mov     ecx,4                                   ;
                mov     edx,_filetable.ft_muOpen                ; function ptr
                call    _mt_swlock                              ;
                mov     ax,_mfs_rmcmode                         ; disable int10/16h
                mov     rmcall32bx,ax                           ;
                call    rmcall32                                ;
                add     esp,8                                   ;
                ret

;u16t _std strm_open(void);
                public  _strm_open                              ;
_strm_open      label   near                                    ;
                mov     edx,offset _mfsd_openname               ;
                call    make_rmaddr                             ;
                push    edx                                     ;
                mov     ecx,2                                   ;
                mov     edx,_filetable.ft_resofs                ; function ptr
                call    _mt_swlock                              ;
                mov     ax,_mfs_rmcmode                         ; disable int10/16h
                mov     rmcall32bx,ax                           ;
                call    rmcall32                                ;
                add     esp,4                                   ;
                ret

;u32t __cdecl mfs_read(u32t offset, u32t buf, u32t readsize);
                public  _mfs_read                               ;
_mfs_read       label   near                                    ;
                shl     dword ptr [esp+8],16 - PARASHIFT        ; make it 16:16
                mov     word ptr [esp+8],0                      ;
                mov     ecx,6                                   ; number of dw to copy
                mov     edx,_filetable.ft_muRead                ; function ptr
_mfs_common:
                call    _mt_swlock                              ;
                mov     ax,_mfs_rmcmode                         ; disable int10/16h
                mov     rmcall32bx,ax                           ;
                jmp     rmcall32                                ;

;u16t __cdecl strm_read(u32t buf, u16t readsize);
                public  _strm_read                              ;
_strm_read      label   near                                    ;
                shl     dword ptr [esp+4],16 - PARASHIFT        ; make it 16:16
                mov     word ptr [esp+4],0                      ;
                mov     ecx,3                                   ; number of dw to copy
                mov     edx,_filetable.ft_reslen                ; function ptr
                jmp     _mfs_common                             ;

;void __cdecl mfs_close(void);
                public  _mfs_close                              ;
_mfs_close      label   near                                    ;
                xor     ecx,ecx                                 ;
                mov     edx,_filetable.ft_muClose               ;
                jmp     _mfs_common                             ;

;void __cdecl mfs_term(void);
                public  _mfs_term                               ;
_mfs_term       label   near                                    ;
                xor     ecx,ecx                                 ;
                mov     edx,_filetable.ft_muTerminate           ;
                jmp     _mfs_common                             ;

;u32t __cdecl hlp_rmcall(u32t rmfunc, u32t dwcopy, ...);
                public  _hlp_rmcall                             ;
_hlp_rmcall     label   near                                    ;
                call    _mt_swlock                              ; yes, twice! because
                call    _mt_swlock                              ; we have static save_ret2
                pop     save_ret2                               ; usage AFTER rmcall32
                pop     edx                                     ;
                cmp     edx,0A0000h                             ; value < A0000?
                jnc     @@hrmc_ext                              ;
                mov     ecx,_qs16base                           ; hack, used for compat.
                cmp     edx,ecx                                 ; with QSINITs <rev 281
                jc      @@hrmc_cvt                              ;
                movzx   eax,_rmpart_size                        ; if addr < A0000 is in
                add     ecx,eax                                 ; the our`s 16-bit obj
                cmp     edx,ecx                                 ; boundaries - we just
                jnc     @@hrmc_cvt                              ; set for it our CS
                sub     edx,_qs16base                           ;
                mov     cx,_rm16code                            ;
                shl     ecx,16                                  ;
                or      edx,ecx                                 ;
                jmp     @@hrmc_ext                              ;
@@hrmc_cvt:
                bswap   edx                                     ; make it far ptr in
                shr     dx,4                                    ; simple way
                bswap   edx                                     ;
@@hrmc_ext:
                pop     ecx                                     ;
                call    rmcall_setflags                         ; read flags
                call    rmcall32                                ;
                lea     esp,[esp-8]                             ; do not touch flags
                push    save_ret2                               ;
                pushfd                                          ;
                call    _mt_swunlock                            ;
                popfd                                           ;
                ret                                             ;

rmcall_setflags label   near
                test    ecx,RMC_EXITCALL                        ;
                jz      @@hrmc_f1                               ; set "PIC reset"
                and     ecx,not RMC_EXITCALL                    ; flag for DPMI call
                mov     rmcall32bx,(FN30X_PICRESET or FN30X_TIMEROFF) shl 8 ;
@@hrmc_f1:
                test    ecx,RMC_IRET                            ;
                jz      @@hrmc_f2                               ; far call with
                and     ecx,not RMC_IRET                        ; iret frame
                mov     rmcall32ax,0302h                        ;
@@hrmc_f2:
                retn

; u32t  __cdecl hlp_rmcallreg(int intnum, rmcallregs_t *regs, u32t dwcopy, ...);
                public  _hlp_rmcallreg
_hlp_rmcallreg  label   near
                call    _mt_swlock                              ; yes, twice!
                call    _mt_swlock                              ;
                pop     save_ret2                               ;
                pop     edx                                     ; intnum
                mov     save_edi,edi                            ;
                pop     edi                                     ; regs
                pop     ecx                                     ; dwcopy
                call    rmcall_setflags                         ; read flags
                or      edx,edx                                 ;
                js      @@hrmc_reg_far                          ;
                mov     byte ptr rmcall32bx,dl                  ; int #
                mov     rmcall32ax,0300h                        ;
@@hrmc_reg_far:
                call    rmcall32regs                            ;
                lea     esp,[esp-12]                            ; do not touch flags
                push    save_ret2                               ;
                pushfd                                          ;
                call    _mt_swunlock                            ;
                popfd                                           ;
                ret                                             ;

;u8t __stdcall raw_io(struct qs_diskinfo *di, u64t start, u32t sectors, u8t write);
                public  _raw_io                                 ;
_raw_io         label   near                                    ;
                pop     eax                                     ; return addr
                call    _mt_swlock                              ;
                mov     edx,offset _rm_regs                     ;
                pop     ecx                                     ;
                sub     ecx,_qs16base                           ; pop esi (must be in DGROUP!)
                mov     [edx+rmcallregs_s.r_esi],ecx            ;
                pop     [edx+rmcallregs_s.r_eax]                ; pop edx:eax
                pop     [edx+rmcallregs_s.r_edx]                ; start sector
                pop     [edx+rmcallregs_s.r_ecx]                ; number of sectors
                pop     [edx+rmcallregs_s.r_ebx]                ; write flag
                push    eax                                     ; return addr
                mov     edx,offset sector_io                    ;
                call    make_rmaddr                             ; setup addr
                xor     ecx,ecx                                 ;
                call    rmcall32                                ;
                setc    al                                      ; set al to 1 if failed
                retn                                            ;

;----------------------------------------------------------------
; call r/m interrupt vector (common code)
; in: dl = int num
;  rm ax = ax                rm cx = cx                rm esi = 0
;  rm bx = high part of eax  rm dx = high part of ecx  rm edi = 0
rmint_common    label   near
                call    _mt_swlock                              ;
                mov     rmcall32ax,0300h                        ; change rmcall
                xor     dh,dh                                   ; func to int
                mov     rmcall32bx,dx                           ; # in dl
                mov     edx,offset _rm_regs                     ;
                mov     word ptr [edx+rmcallregs_s.r_eax],ax    ;
                shr     eax,16                                  ;
                mov     word ptr [edx+rmcallregs_s.r_ebx],ax    ;
                mov     word ptr [edx+rmcallregs_s.r_ecx],cx    ;
                shr     ecx,16                                  ;
                mov     word ptr [edx+rmcallregs_s.r_edx],cx    ;
                xor     ecx,ecx                                 ;
                mov     [edx+rmcallregs_s.r_esi],ecx            ;
                mov     [edx+rmcallregs_s.r_edi],ecx            ;
                call    rmcall32                                ;
                retn

;u16t key_read_int(void);
                public  _key_read_int                           ;
_key_read_int   label   near                                    ; read keyboard
                mov     dl,16h                                  ; int 16h
                mov     ah,10h                                  ; enhanced key read
                call    rmint_common                            ;
                movzx   eax,ax                                  ;
                jmp     _key_filter                             ;

;u8t  key_pressed(void);
                public  _ckey_pressed                           ;
_ckey_pressed   label   near                                    ; is key pressed?
                mov     dl,16h                                  ; int 16h
                mov     ah,11h                                  ; enhanced key check
                call    rmint_common                            ;
                setnz   al                                      ;
                retn                                            ;

;u8t  _std key_push(u16t code);
                public  _ckey_push                              ;
_ckey_push      label   near                                    ; push key press
                mov     dl,16h                                  ;
                mov     ah,05h                                  ; int 16h, ah=05h
                mov     cx,[esp+4]                              ; scan<<8|key
                call    rmint_common                            ;
                or      al,al                                   ;
                setz    al                                      ; 1 on success
                retn    4                                       ;

;void _std cvio_intensity(u8t value);
                public  _cvio_intensity                         ;
_cvio_intensity label   near                                    ;
                mov     dl,10h                                  ; int 10h
                mov     ah,[esp+8]                              ; ax=1003h
                or      ah,ah                                   ;
                setz    ah                                      ; value for bl
                mov     _cvio_blink,ah                          ;
                bswap   eax                                     ;
                mov     ax,1003h                                ;
                call    rmint_common                            ;
                retn    4                                       ;

;u16t int12mem(void);
                public  _int12mem                               ;
_int12mem       label   near                                    ; set keyboard rate
                mov     dl,12h                                  ; int 12h
                call    rmint_common                            ;
                retn                                            ;

;int _std _hlp_fddline(u32t disk);
                public  _hlp_fddline                            ;
_hlp_fddline    label   near                                    ; fdd change line test
                mov     dl,13h                                  ; int 13h
                mov     ah,16h                                  ; int 13h, ah=16h
                mov     ecx,[esp+4]                             ;
                xor     cl,80h                                  ; reverse FDD bit
                js      @@fddc_noch                             ;
                cmp     ecx,100h                                ; <=7F in al, so will
                jnc     @@fddc_dsknerr                          ; be -1 on exit
                shl     ecx,16                                  ; put to dl
                call    rmint_common                            ;
                setc    al                                      ;
                jnc     @@fddc_noch                             ;
                cmp     ah,al                                   ; ah==0?
                jc      @@fddc_noch                             ;
@@fddc_dsknerr:
                neg     al                                      ;
@@fddc_noch:
                movsx   eax,al                                  ;
                retn    4                                       ;

                public  rmstop                                  ; exit to rm
rmstop          label   near                                    ;
                mov     edx,offset _rm_regs                     ; _mt_swlock already
                mov     [edx+rmcallregs_s.r_eax], ebx           ; called in exit_pm32
                mov     [edx+rmcallregs_s.r_edx], eax           ;
                xor     ecx,ecx                                 ;
                mov     edx,offset panic_initpm_np              ; setup addr
                mov     rmcall32bx,(FN30X_PICRESET or FN30X_TIMEROFF) shl 8 ; reset all
                call    make_rmaddr                             ;
                call    rmcall32                                ; never return

_TEXT           ends
                end
