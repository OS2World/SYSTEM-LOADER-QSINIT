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
_DATA           ends                                            ;

_TEXT           segment
                assume  cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT

                extrn   _log_flush:near                         ;
                extrn   _key_filter:near                        ;
                public  rmcall32                                ;
                ; call rm: edx - addr, ecx - number of words in stack
                ;
rmcall32:
                mov     save_edi,edi                            ;
                mov     save_esi,esi                            ;
                mov     save_ebp,ebp                            ;
                mov     save_ebx,ebx                            ;
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
                pushf                                           ;
                pop     _rm_regs.r_flags                        ; save flags

                pop     save_ret                                ; return addr

                xor     bh,bh                                   ; dpmi call
                mov     ax,rmcall32ax                           ;
                int     31h                                     ;
                mov     eax,_rm_regs.r_edx                      ; restore dword result
                shl     eax,16                                  ; (dx:ax)
                mov     ax,word ptr _rm_regs.r_eax              ;

                mov     dx,_rm_regs.r_flags                     ; combine flags
                pushf                                           ;
                pop     cx                                      ;
                and     dx,08D5h                                ; use OF, SF, ZF, AF, PF, CF
                and     cx,not 08D5h                            ; from real mode and
                or      dx,cx                                   ; preserve other
                push    dx                                      ;
                popf                                            ;
                mov     rmcall32ax, 0301h                       ; restore default rmcall

                mov     edi,save_edi                            ;
                mov     esi,save_esi                            ;
                mov     ebp,save_ebp                            ;
                mov     ebx,save_ebx                            ;

                push    [save_ret]                              ;
                pushf                                           ; restore all static
                push    eax                                     ; vars before
                call    _log_flush                              ; flushing rm log
                pop     eax                                     ;
                popf                                            ;
                ret                                             ; return to caller

; convert edx to seg:ofs but panic if it not in 16-bit segments
make_rmaddr     label   near                                    ;
                sub     edx,_qs16base                           ;
                push    ebx                                     ;
                mov     ebx,0FFFF0000h                          ;
                add     ebx,edx                                 ;
                into                                            ; YES! now i used it ;)
                mov     bx,_rm16code                            ; it will make trap 4 on wrong addr
                shl     ebx,16                                  ;
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
                jmp     rmcall32                                ;

;u16t __cdecl strm_read(u32t buf, u16t readsize);
                public  _strm_read                              ;
_strm_read      label   near                                    ;
                shl     dword ptr [esp+4],16 - PARASHIFT        ; make it 16:16
                mov     word ptr [esp+4],0                      ;
                mov     ecx,3                                   ; number of dw to copy
                mov     edx,_filetable.ft_reslen                ; function ptr
                jmp     rmcall32                                ;

;u32t __cdecl mfs_close(void);
                public  _mfs_close                              ;
_mfs_close      label   near                                    ;
                xor     ecx,ecx                                 ;
                mov     edx,_filetable.ft_muClose               ;
                jmp     rmcall32                                ;

;u32t __cdecl mfs_term(void);
                public  _mfs_term                               ;
_mfs_term       label   near                                    ;
                xor     ecx,ecx                                 ;
                mov     edx,_filetable.ft_muTerminate           ;
                jmp     rmcall32                                ;

;u32t __cdecl hlp_rmcall(u32t rmfunc,u32t dwcopy,...);
                public  _hlp_rmcall                             ;
_hlp_rmcall     label   near                                    ;
                pop     save_ret2                               ;
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
                call    rmcall32                                ;
                lea     esp,[esp-8]                             ; do not touch flags
                jmp     [save_ret2]                             ;

;u8t __stdcall raw_io(struct qs_diskinfo *di, u64t start, u32t sectors, u8t write);
                public  _raw_io                                 ;
_raw_io         label   near                                    ;
                pop     eax                                     ; return addr
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

;u16t key_read(void);
                public  _key_read                               ;
_key_read       label   near                                    ; read keyboard
                mov     rmcall32ax, 0300h                       ; change rmcall func
                push    ebx                                     ;
                mov     edx,offset _rm_regs                     ;
                mov     bh, 10h                                 ; enhanced key read
                mov     [edx+rmcallregs_s.r_eax], ebx           ;
                mov     bl, 16h                                 ; int 16h, ah=0
                xor     ecx,ecx                                 ;
                call    rmcall32                                ;
                pop     ebx                                     ;
                movzx   eax,ax                                  ;
                jmp     _key_filter                             ;

;u8t  key_pressed(void);
                public  _key_pressed                            ;
_key_pressed    label   near                                    ; is key pressed?
                mov     rmcall32ax, 0300h                       ;
                mov     edx,offset _rm_regs                     ;
                push    ebx                                     ;
                mov     bh, 11h                                 ; enhanced key check
                mov     [edx+rmcallregs_s.r_eax], ebx           ;
                mov     bl, 16h                                 ; int 16h, ah=1
                xor     ecx,ecx                                 ;
                call    rmcall32                                ;
                pop     ebx                                     ;
                setnz   al                                      ;
                retn                                            ;

;u8t  _std key_push(u16t code);
                public  _key_push                               ;
_key_push       label   near                                    ; push key press
                mov     rmcall32ax, 0300h                       ;
                mov     cx, [esp+4]                             ; scan<<8|key
                mov     edx,offset _rm_regs                     ;
                push    ebx                                     ;
                mov     bh, 05h                                 ;
                mov     [edx+rmcallregs_s.r_eax], ebx           ;
                mov     [edx+rmcallregs_s.r_ecx], ecx           ;
                mov     bl, 16h                                 ; int 16h, ah=05h
                xor     ecx,ecx                                 ;
                call    rmcall32                                ;
                or      al,al                                   ;
                setz    al                                      ; 1 on success
                pop     ebx                                     ;
                retn    4                                       ;

;void _std vio_intensity(u8t value);
                public  _vio_intensity
_vio_intensity  label   near
                mov     rmcall32ax, 0300h                       ;
                push    ebx                                     ;
                mov     bl, [esp+8]                             ;
                mov     edx,offset _rm_regs                     ;
                or      bl,bl                                   ;
                setz    bl                                      ;
                mov     [edx+rmcallregs_s.r_eax], 1003h         ;
                mov     byte ptr [edx+rmcallregs_s.r_ebx], bl   ;
                mov     bl, 10h                                 ; int 10h
                xor     ecx,ecx                                 ;
                call    rmcall32                                ;
                pop     ebx                                     ;
                retn    4                                       ;

;u16t int12mem(void);
                public  _int12mem                               ;
_int12mem       label   near                                    ; set keyboard rate
                mov     rmcall32ax, 0300h                       ;
                push    ebx                                     ;
                mov     bl, 12h                                 ; int 12h
                xor     ecx,ecx                                 ;
                call    rmcall32                                ;
                pop     ebx                                     ;
                retn                                            ;

;int _std _hlp_fddline(u32t disk);
                public  _hlp_fddline                            ;
_hlp_fddline    label   near                                    ; fdd change line test
                mov     rmcall32ax, 0300h                       ;
                mov     eax, [esp+4]                            ;
                push    ebx                                     ;
                xor     al, 80h                                 ; reverse FDD bit
                js      @@fddc_noch                             ;
                cmp     eax,100h                                ; <=7F in al, so will
                jnc     @@fddc_dsknerr                          ; be -1 on exit
                or      ah,ah                                   ; check high bits in disk
                mov     edx,offset _rm_regs                     ;
                mov     bh, 16h                                 ;
                mov     [edx+rmcallregs_s.r_eax], ebx           ;
                mov     [edx+rmcallregs_s.r_edx], eax           ;
                xor     ecx,ecx                                 ;
                mov     [edx+rmcallregs_s.r_esi], ecx           ;
                mov     bl, 13h                                 ; int 13h, ah=16h
                call    rmcall32                                ;
                setc    al                                      ;
                jnc     @@fddc_noch                             ;
                cmp     ah,al                                   ; ah==0?
                jc      @@fddc_noch                             ;
@@fddc_dsknerr:
                neg     al                                      ;
@@fddc_noch:
                movsx   eax,al                                  ;
                pop     ebx                                     ;
                retn    4                                       ;

                public  rmstop                                  ;
rmstop          label   near                                    ; exit to rm
                mov     edx,offset _rm_regs                     ;
                mov     [edx+rmcallregs_s.r_eax], ebx           ;
                mov     [edx+rmcallregs_s.r_edx], eax           ;
                xor     ecx,ecx                                 ;
                mov     edx,offset panic_initpm_np              ; setup addr
                call    make_rmaddr                             ;
                call    rmcall32                                ; never return

_TEXT           ends
                end
