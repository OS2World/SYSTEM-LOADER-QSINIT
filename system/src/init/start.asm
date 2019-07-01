;
; QSINIT
; init process
;

                include inc/qstypes.inc                         ;
                include inc/segdef.inc                          ;
                include inc/basemac.inc                         ;
                include inc/filetab.inc                         ;
                include inc/debug.inc                           ;
                include inc/ioint13.inc                         ; for BPB
                include inc/iopic.inc                           ;
                include inc/qsconst.inc                         ;
                include inc/cpudef.inc                          ;
                include inc/qsbinfmt.inc                        ;

                .486p

PMODE_TEXT      segment                                         ;
                extrn _pm_info:near                             ;
                extrn _pm_init:near                             ;
PMODE_TEXT      ends                                            ;

TEXT16          segment                                         ;
                extrn   _calibrate_delay:near                   ; io delay calibrate call
                extrn   settimerirq:near                        ; set int 50h handler
                extrn   pm16set:near                            ; 16bit pm init part
                extrn   int13check:near                         ; int13 support check
                extrn   _checkresetmode:near                    ;
                extrn   _init16:near                            ;
                extrn   _settextmode:near                       ;
                extrn   _LdrRstCS:word                          ;
                extrn   _dd_bootdisk:byte                       ;
                extrn   _dd_bootflags:byte                      ;
                extrn   _dd_bpbofs:word                         ;
                extrn   _dd_bpbseg:word                         ;
                extrn   _dd_rootofs:word                        ;
                extrn   _dd_rootseg:word                        ;
                extrn   _dd_firstsector:word                    ;
                extrn   LdrRstSS:word                           ;
                extrn   LdrRstSP:word                           ;
TEXT16          ends                                            ;

DATA16          segment                                         ;
                public  _filetable                              ;
                public  _minifsd_ptr                            ;
                public  _logbufseg                              ;
                public  _DiskBufRM_Seg                          ;
                public  _rmpart_size, _bin_header, bin32_seg    ;
                ; ----> must be in the same order and place,
                ;       exported to bootos2.exe <----------------
_minifsd_ptr    dd      0                                       ; minifsd copied here
minifsd_flags   dw      0                                       ; boot flags
_DiskBufRM_Seg  dw      0                                       ; RM far disk buffer segment
_filetable      filetable_s <>                                  ;
                ; -----------------------------------------------
filetabptr      dd      0                                       ;
_bin_header     mkbin_info <MKBIN_SIGN, size mkbin_info, 0, offset bssstart>
_rmpart_size    dw      0                                       ; own size, rounded up to 4k
bin32_seg       dw      0                                       ; seg of packed 32-bit part
_logbufseg      dw      0                                       ; rm log buffer segment
DATA16          ends                                            ;

BSSINIT         segment                                         ;
                align 2                                         ;
bssstart        label   word                                    ;
                public  _pmdataseg                              ;
                public  _physmem                                ;
                public  intrmatrix                              ;
                public  _DiskBufPM                              ;
init_print_attr db      ?                                       ;
                align   2                                       ;
_pmdataseg      dw      ?                                       ; pm data segment
intrmatrix      db      100h dup(?)                             ; INT redirectors for all INTs
_physmem        db      100h dup (?)                            ; 32 * 8 (!!)
                align   4                                       ;
_DiskBufPM      dd      ?                                       ;
BSSINIT         ends                                            ;

BSSFINI         segment
                public  bssend                                  ;
                align   2                                       ;
bssend          label word                                      ; end of BSS seg
BSSFINI         ends

_BSS16          segment                                         ;
                public  _rm16code                               ;
                public  _safeMode, _int12size                   ;
                public  _BootBPB, _physmem_entries              ;
_rm16code       dw      ?                                       ;
_physmem_entries dw     ?                                       ;
up_free_len     dw      ?                                       ; free paragraphs AFTER 16-bit part
up_free_seg     dw      ?                                       ; start seg of this memory
lo_used_seg     dw      ?                                       ; lowest seg used by micro-FSD/QSINIT
_int12size      dw      ?                                       ;
_safeMode       db      ?                                       ;
_BootBPB        Disk_BPB <>                                     ; BPB
                align  4                                        ;
                public  _headblock, _phmembase                  ;
                public  _highbase, _highstack, _stacksize       ;
_phmembase      dd      ?                                       ;
_highbase       dd      ?                                       ;
_highstack      dd      ?                                       ;
_stacksize      dd      ?                                       ;
_headblock      dw      ?                                       ;
_BSS16          ends                                            ;

;
; ENTRY
;    (CS:IP) = entry point address
;            = 2000:0000H for FAT boot system
;    (SS:SP) = 0030:00FAH for FAT boot system (BM/MBR dependent)
;    (BX)    = First data sector on the disk (0 based)
;    (DH)    = boot flags:
;              0x00 - FAT boot
;              BF_NOMFSHVOLIO  0x01    // mini-FSD does not use mFSH_DOVOLIO
;              BF_RIPL         0x02    // remote IPL flag
;              BF_MINIFSD      0x04    // mini-FSD flag
;              BF_NOPICINIT    0x08    // do not initialize the PIC
;              BF_MICROFSD     0x10    // micro-FSD flag
;              BF_RESERVED     0xA0    // reserved, must be 0
;    (DL)    = drive number for the boot disk (ignored if no BF_NOMFSHVOLIO or BF_MINIFSD)
;    (DS:SI) = address of BOOT MEDIA'S bpb (ignored if no BF_NOMFSHVOLIO or BF_MINIFSD)
;    (ES:DI) = address of root directory / micro-fsd file table
;
;  FAT32 boot sector:
;    (DH)    = boot flags
;    (DL)    = drive number for the boot disk
;    (DS:SI) = address of BOOT MEDIA'S bpb
;
;  exFAT boot sector:
;    (DH)    = 0x40 (BF_NEWBPB)
;    (DL)    = drive number for the boot disk
;    (DS:SI) = address of Disk_NewPB structure (see ioint13.inc)
;
                public  _aboutstr                               ;
INITCODE        segment                                         ;
                assume  cs:G16, ds:G16, es:nothing, ss:G16      ;
start:
                mov     cs:_dd_bpbseg,ds                        ;
                mov     ax,cs                                   ;
                mov     ds,ax                                   ;
                mov     LdrRstSS,ss                             ; saving start values for restart
                mov     LdrRstSP,sp                             ; loader function
                mov     word ptr _dd_bootdisk,dx                ;
                mov     minifsd_flags,dx                        ;
                mov     _dd_bpbofs,si                           ;
                mov     _dd_firstsector,bx                      ;
                mov     _dd_rootseg,es                          ;
                mov     _dd_rootofs,di                          ;
                mov     _LdrRstCS,cs                            ;
                mov     word ptr filetabptr+2,es                ; save filetable/rootdir pointer
                mov     word ptr filetabptr,di                  ;
                jmp     @@init_process                          ;

                db      10,10                                   ;
_aboutstr       label   near                                    ;
                include inc/version.inc                         ;
                db      10,0,10                                 ;
;
; here we can`t use bss and own stack until packed 32bit part will be moved up
;
@@init_process:
                cli                                             ; who knows - where is
                mov     dx,offset bssend                        ; our`s stack now? so cli it
                add     dx,QSM_STACK16                          ;
                add     dx,PAGESIZE - 1                         ; align to page
                and     dx,not PAGEMASK                         ;
                mov     _rmpart_size,dx                         ;
                mov     cx,dx                                   ;
                shr     cx,PARASHIFT                            ; move packed 32-bit
                add     cx,ax                                   ; code to the end of
                mov     es,cx                                   ; 16-bit stack
                mov     bin32_seg,cx                            ;
                mov     cx,word ptr _bin_header.pmPacked        ;
                mov     di,cx                                   ;
                dec     di                                      ;
                mov     si,_bin_header.rmStored                 ;
                add     si,di                                   ;
                std                                             ;
            rep movsb                                           ;
                mov     es,ax                                   ;
                cld                                             ;
                mov     ss,ax                                   ;
                movzx   esp,dx                                  ; switch stack
                sti                                             ;

                mov     di,offset bssstart                      ; clear BSS segment
                mov     cx,offset bssend                        ;
                sub     cx,di                                   ;
                shr     cx,1                                    ;
                xor     ax,ax                                   ;
            rep stosw                                           ;
                mov     di,offset intrmatrix                    ; fill int routers
                mov     cl,80h                                  ;
                mov     ax,0CCCCh                               ;
            rep stosw                                           ;
;
; here we have cleared bss & own stack
;
                push    ds                                      ;
                mov     ax,1003h                                ; blink off
                xor     bx,bx                                   ;
                int     10h                                     ;
                mov     ah,2                                    ; check left shift state
                int     16h                                     ;
                and     al,2                                    ;
                pop     ds                                      ;
                setnz   _safeMode                               ;
                add     byte ptr mhdr_about,al                  ; change color of |LOADING|
ifdef INITDEBUG
                push    0                                       ; set 80x50
                push    2                                       ;
                push    cs                                      ;
                call    _settextmode                            ;
endif
                xor     ax,ax                                   ;
                mov     dx,ax                                   ; left corner
                call    init_print                              ; hello world!

                push    ds                                      ;
                int     12h                                     ; get int 12 value
                pop     ds                                      ;
                mov     _int12size,ax                           ;
ifdef INITDEBUG
                dbg16print <10,"lowmem: %d kb",10>,<ax>         ;
                mov     dx,word ptr _dd_bootdisk                ;
                dbg16print <"bootdisk: %x",10>,<dx>             ;
endif
                mov     dx,ax                                   ; check for sufficient
                mov     ax,3                                    ; place to to put self
                cmp     dx,480 ;512                             ; in the top of low memory
                jc      panic_initpm                            ; <512kb -> error
                shl     dx,10-PARASHIFT                         ; kb->paragraphs
                mov     ax,_rmpart_size                         ;
                shr     ax,PARASHIFT                            ; use 9100:0 by default,
                sub     dx,ax                                   ; else try to calculate
                mov     cx,QSINIT_SEGREAL                       ;
                cmp     dx,cx                                   ;
                jnc     @@init_9100ok                           ;
                mov     cx,not ((1 shl (PAGESHIFT-PARASHIFT)) - 1) ; round to page mask
                and     cx,dx                                   ; own target segment
@@init_9100ok:
ifdef INITDEBUG
                dbg16print <"new cs: %x",10>,<cx>               ;
endif
                mov     _rm16code,cx                            ;
                sub     dx,cx                                   ; free paragraphs
                mov     up_free_len,dx                          ; at 9100 segment
                add     ax,cx                                   ;
                mov     up_free_seg,ax                          ; at its seg

                call    _pm_info                                ; check cpu type
                jc      panic_initpm                            ; exit on error
                ;dbg16print <"Need %x paragraphs",10>,bx         ;
                mov     _pmdataseg,bx                           ;
                cmp     cl, 4                                   ;
                mov     ax, 1                                   ; if 486?
                jc      panic_initpm                            ; no, exit too

                smsw    cx                                      ; 486SX?
                test    cx,CPU_CR0_ET                           ; deny it too
                jz      panic_initpm                            ;

                mov     bx,ds                                   ; save
                mov     dh,_dd_bootflags                        ;
                mov     ah,dh                                   ; copy flags to ah
                and     dh,not (BF_NOPICINIT or BF_NEWBPB)      ;

                movzx   cx,ah                                   ;
                mov     es,bx                                   ;
ifdef INITDEBUG
                dbg16print <"boot flags: %x, ds: %x",10>,<bx,cx> ;
endif
                cmp     dh,al                                   ; FSD mode?
                jc      @@init_noft                             ;
                lds     si,[filetabptr]                         ; (ds:si) = filetable pointer
                mov     di,offset _filetable                    ;
                mov     cx,(size filetable_s) shr 1             ;
            rep movsw                                           ; copying filetable
                mov     ds,bx                                   ; restore
ifdef INITDEBUG
                call    ftab_print                              ;
endif
; checking file table ...
                mov     al,5                                    ; message code
                mov     cx,[_filetable.ft_loaderseg]            ; check micro-fsd report
                cmp     bx,cx                                   ; table wrong
                jnz     panic_initpm                            ;
                cmp     bx,OS2LDR_SEGREAL                       ; we are loaded not at 1000:0
                jnz     panic_initpm                            ;

                and     dh,BF_MICROFSD or BF_RESERVED           ;
                cmp     dh,BF_MICROFSD                          ; check flags
                jnz     panic_initpm                            ;

                xor     si,si                                   ; zero
                cmp     si,word ptr [_filetable.ft_loaderlen+2] ; loader > 64k
                jnz     panic_initpm                            ;
                cmp     si,word ptr [_filetable.ft_mulen+2]     ; microfsd > 64k
                jnz     panic_initpm                            ;
                cmp     si,word ptr [_filetable.ft_mfsdlen+2]   ; minifsd > 64k
                jnz     panic_initpm                            ;
                cmp     si,word ptr [_filetable.ft_loaderlen]   ; loader len field == 0?
                jz      panic_initpm                            ;
                mov     cx,word ptr [_filetable.ft_mulen]       ; microfsd len field == 0?
                or      cx,cx                                   ;
                jz      panic_initpm                            ;
                shr     cx,PARASHIFT                            ;
                mov     dx,_filetable.ft_museg                  ;
                mov     lo_used_seg,dx                          ; lowest used seg on FSD boot
                add     cx,dx                                   ; cx = microfsd end
                mov     bx,_int12size                           ;
                shl     bx,10-PARASHIFT                         ;
                sub     bx,_64KB SHR PARASHIFT                  ;
                cmp     cx,bx                                   ; end above lowmem - 64k?
                ja      panic_initpm                            ;
                sub     bx,_64KB SHR PARASHIFT                  ;
                cmp     dx,bx                                   ; start below lowmem - 128k?
                jc      panic_initpm                            ;

                mov     cx,[_filetable.ft_cfiles]               ; check ft_cfiles
                cmp     cx,8                                    ;
                jnc     panic_initpm                            ; accept all of 3..7
                cmp     cl,3                                    ;
                jc      panic_initpm                            ;

                mov     cx,word ptr [_filetable.ft_mfsdlen]     ; minifsd check
                test    ah,BF_MINIFSD                           ;
                jz      @@init_nomini_1                         ;
                or      cx,cx                                   ; minifsd size == 0?
                jnz     @@init_nomini_1                         ;
                and     ah,not BF_MINIFSD                       ; drop BF_MINIFSD flag
                and     byte ptr _dd_bootflags,ah               ;
                and     byte ptr minifsd_flags+1,ah             ;
@@init_nomini_1:
                test    ah,BF_RIPL                              ; ripl on?
                jz      @@init_noripl                           ;
                cmp     si,word ptr [_filetable.ft_ripllen+2]   ; ripl data
                jnz     panic_initpm                            ; >64k
                mov     dx,word ptr [_filetable.ft_ripllen]     ;
                cmp     si,dx                                   ; ignore empty
                jz      @@init_noripl                           ; ripl data
                add     cx,dx                                   ; minifsd + ripl len
                jc      panic_initpm                            ; >64k
                mov     si,[_filetable.ft_riplseg]              ;
                cmp     si,MFSDBASE SHR PARASHIFT               ; check ripl seg location:
                jc      panic_initpm                            ; > RIPLBASE and < 1000:0
                shr     dx,PARASHIFT                            ; NOTE: replaced to MFSDBASE
                add     si,dx                                   ; for incorrect DToD PXE boot
                cmp     si,OS2LDR_SEGREAL                       ;
                ja      panic_initpm                            ;
@@init_noripl:
                test    ah,BF_MINIFSD                           ; minifsd present?
                jz      @@init_nomini_2                         ;
                mov     si,[_filetable.ft_mfsdseg]              ;
                cmp     si,MFSDBASE SHR PARASHIFT               ; check minifsd seg location:
                jc      panic_initpm                            ; > MFSDBASE
                shr     cx,PARASHIFT                            ; and
                add     si,cx                                   ; < 1000:0
                cmp     si,OS2LDR_SEGREAL                       ;
                ja      panic_initpm                            ;
@@init_nomini_2:
                clc                                             ; skip used mem FAT boot setup
@@init_noft:
                mov     bx,_rm16code                            ; our's next location
                jnc     @@init_nofsd_pm                         ;
                mov     lo_used_seg,bx                          ; lowest used mem in FAT boot
@@init_nofsd_pm:
; migrating to the top of low memory ...
                mov     es,bx                                   ; copy self to 9100:0
                xor     di,di                                   ;
                mov     cx,_rmpart_size                         ;
                mov     si,di                                   ;
                shr     cx,2                                    ;
            rep movsd                                           ;
                push    bx                                      ; jump to 9100:0
                push    offset @@init_9100                      ;
                mov     dx,cs                                   ;
                retf                                            ;
@@init_9100:
                mov     ss,bx                                   ; home, sweet home ;)
                mov     ds,bx                                   ;

                mov     cx,_pmdataseg                           ;
                add     cx,PAGEMASK shr PARASHIFT               ; align DMPI data size
                and     cx,not (PAGEMASK shr PARASHIFT)         ; to page
                call    init_alloc_rm                           ;
                mov     _pmdataseg,cx                           ; DMPI data (GDT, etc)

                mov     cx,LOGBUF_SIZE SHR PARASHIFT            ;
                call    init_alloc_rm                           ; 4k buffer for logging
                mov     _logbufseg,cx                           ; debug output in RM callbacks

                mov     cx,DISKBUF_SIZE SHR PARASHIFT           ; 32k disk i/o buffer
                call    init_alloc_nohigh                       ;
                mov     _DiskBufRM_Seg,cx                       ; must be BELOW 9100
                movzx   ecx,cx                                  ; else kernel start
                shl     ecx,PARASHIFT                           ; will be confused
                mov     _DiskBufPM,ecx                          ; (he uses it as a temp storage)

                test    ah,BF_NOMFSHVOLIO                       ; BPB available?
                jnz     @@init_noBPB                            ;
                mov     si,word ptr _dd_bpbofs                  ; yes, copy it
                mov     di,offset _BootBPB                      ;
                mov     ds,word ptr _dd_bpbseg                  ;
                mov     cx,size Disk_BPB                        ;
                test    ah,BF_NEWBPB                            ;
                jz      @@init_copyBPB
                mov     cx,size Disk_NewPB                      ;
@@init_copyBPB:
            rep movsb                                           ;
                mov     ds,bx                                   ; restore ds
                jmp     @@init_funclist                         ;
@@init_noBPB:
                mov     byte ptr _dd_bootdisk, 0FFh             ; guarantee invalid disk
                mov     byte ptr minifsd_flags, 0FFh            ; number
@@init_funclist:
                call    write_dot                               ;
                call    int13check                              ; check int 13h
                call    write_dot                               ;
                push    cs                                      ;
                call    _calibrate_delay                        ; calibrate io delay
                call    write_dot                               ;
                call    init_memory                             ; get system memory info
                call    write_dot                               ;

                test    byte ptr _dd_bootflags,BF_NOPICINIT     ;
                jnz     @@init_skippic                          ;
                call    remap_pic                               ; remap PIC1 to int 50h
                call    write_dot                               ;
@@init_skippic:
                call    settimerirq                             ; set own irq0 handler
                call    write_dot                               ;
                push    ds                                      ;
                pop     es                                      ;
                call    _init16                                 ; msg code in ax
                call    write_dot                               ;
ifdef INITDEBUG
                dbg16print <"init16 rc: %x",10>,<ax>            ;
endif
                or      ax,ax                                   ;
                jz      @@init_init16ok                         ;
                jmp     panic_initpm                            ;
@@init_init16ok:
ifdef INITDEBUG
                mov     ax,_pmdataseg                           ;
                mov     cx,_logbufseg                           ;
                mov     edx,_DiskBufPM                          ;
                dbg16print <"pmdata:%x, log:%x, iobuf:%lx",10>,<edx,cx,ax> ;

                mov     eax, _highbase
                mov     ecx, _highstack
                mov     edx, _minifsd_ptr
                dbg16print <"hibase:%lx, histack:%lx, fsd:%lx",10>,<edx,ecx,eax> ;
endif
; protected mode ...
                mov     es,_pmdataseg                           ;
                call    _pm_init                                ; init
                jc      panic_initpm                            ;

                mov     edi,_minifsd_ptr                        ; copy mini-FSD
                or      edi,edi                                 ; to buffer in
                jz      @@init_noMFSDcopy                       ; upper memory
                push    ds                                      ;
                movzx   esi,_filetable.ft_mfsdseg               ;
                shl     esi,PARASHIFT                           ;
                mov     ecx,_filetable.ft_mfsdlen               ;
                cld                                             ;
                push    es                                      ;
                pop     ds                                      ;
            rep movs    byte ptr es:[edi],byte ptr ds:[esi]     ;
                pop     ds                                      ;
@@init_noMFSDcopy:
                jmp     pm16set                                 ; go to 16 bit pm code

                public  panic_initpm                            ;
                public  panic_initpm_np                         ;
panic_initpm:
                mov     dx,200h                                 ; 3nd line, 0 col
panic_initpm_np:
                call    init_print                              ; print error
halt_sys:
                sti                                             ; allow C-A-D
                hlt                                             ; and loop forever
                jmp     short halt_sys                          ;

; ===============================================================
; IN : cx = number of paragraphs (page aligned)
; OUT: cx = segment (page aligned)
init_alloc_rm   label   near                                    ;
                cmp     up_free_len,cx                          ; trying to put in 9100
                jc      init_alloc_nohigh                       ;
                sub     up_free_len,cx                          ;
                xadd    up_free_seg,cx                          ;
                retn
init_alloc_nohigh:
                sub     lo_used_seg,cx                          ;
                mov     cx,lo_used_seg                          ;
                retn                                            ;

; ===============================================================
; initial (real mode) message print.
; IN: al = message number
;     dx = pos
;
                public  init_print
init_print      proc    near                                    ;
                xor     ah,ah                                   ; save pos & msg
                push    ax                                      ;
                push    dx                                      ;
                xor     edx,edx                                 ; far call to reset mode
                or      ax,ax                                   ; but clear screen on
                setz    dl                                      ; msg #0 only
                push    edx                                     ;
                push    cs                                      ;
                call    _checkresetmode                         ;
                pop     dx                                      ;
                pop     bx                                      ;
                push    cs                                      ; check ds
                pop     ds                                      ;
                shl     bx,1                                    ;
                mov     si,[bx+embedded_errs]                   ;
                lodsb                                           ;
                push    si                                      ;
                cbw                                             ;
                mov     bx,ax                                   ;
                shl     bx,1                                    ;
                mov     si,[bx+embedded_hdrs]                   ;
                lodsb                                           ; load color attribute
                mov     init_print_attr,al                      ; from first byte
@@msgloop:
                lodsb                                           ;
                or      al,al                                   ;
                jz      @@msgloop_end                           ;
                mov     ah,09h                                  ;
                push    ds                                      ; save all when we call bios
                push    si                                      ;
                                                                ;
                push    dx                                      ; update position
                push    ax                                      ;
                mov     ah,2                                    ;
                xor     bh,bh                                   ;
                int     10h                                     ;
                pop     ax                                      ;

                xor     bh,bh                                   ;
                mov     bl,cs:init_print_attr                   ; get attribute
                mov     cx,1                                    ;
                int     10h                                     ;
                pop     dx                                      ;
                inc     dx                                      ; update cursor position
                pop     si                                      ;
                pop     ds                                      ;
                jmp     @@msgloop                               ;
@@msgloop_end:
                cmp     si,offset msg_no486                     ; is this a header?
                jnc     @@msgloop_exit                          ;
                pop     si                                      ;
                jmp     @@msgloop                               ; yes, print msg
@@msgloop_exit:
                ret                                             ;
init_print      endp                                            ;

write_dot       proc    near                                    ;
                pushf                                           ; save all
                pushad                                          ;
                push    ds                                      ;
                push    es                                      ;
                mov     ah,3                                    ; save current pos
                xor     bh,bh                                   ;
                int     10h                                     ;
                movzx   cx,dotpos                               ; but if it equal,
                cmp     cx,dx                                   ; update it
                setz    al                                      ;
                add     dl,al                                   ;
                push    dx                                      ;
                mov     dx,cx                                   ;
                mov     ah,2                                    ; set pos
                xor     bh,bh                                   ;
                int     10h                                     ;
                mov     ah,9                                    ; print ".>"
                xor     bh,bh                                   ;
                mov     cx,1                                    ;
                mov     al,0fah ;0DEh                           ;
                mov     bl,mhdr_about                           ;
                push    bx                                      ;
                push    cx                                      ;
                int     10h                                     ;
                mov     ah,2                                    ;
                inc     byte ptr dotpos                         ;
                xor     bh,bh                                   ;
                movzx   dx,dotpos                               ;
                int     10h                                     ;
                pop     cx                                      ;
                pop     bx                                      ;
                mov     ah,9                                    ;
                mov     al,'>'                                  ;
                int     10h                                     ;
                pop     dx                                      ; restore cursor
                mov     ah,2                                    ; position
                xor     bh,bh                                   ;
                int     10h                                     ;
                pop     es                                      ;
                pop     ds                                      ; and exit
                popad                                           ;
                popf                                            ;
                ret                                             ;
write_dot       endp


ifdef INITDEBUG
ftab_print      proc    near                                    ;
                dbg16print <10,"Micro-FSD file table (%lx):",10>,<filetabptr>
                dbg16print <"cfiles: %d, loader seg %x, len %lx",10>,<_filetable.ft_loaderlen,_filetable.ft_loaderseg,_filetable.ft_cfiles>
                dbg16print <"micro seg %x, len %lx",10>,<_filetable.ft_mulen,_filetable.ft_museg>
                dbg16print <"mini  seg %x, len %lx",10>,<_filetable.ft_mfsdlen,_filetable.ft_mfsdseg>
                dbg16print <"ripl  seg %x, len %lx",10>,<_filetable.ft_ripllen,_filetable.ft_riplseg>
                dbg16print <"opn: %lx, read %lx, clse %lx, term, %lx",10>,<_filetable.ft_muTerminate,_filetable.ft_muClose,_filetable.ft_muRead,_filetable.ft_muOpen>
                cmp     _filetable.ft_cfiles,5                  ;
                jc      @@ftbp_exit                             ;
                dbg16print <"res addr: %lx, len %lx",10>,<_filetable.ft_resofs,_filetable.ft_reslen>
@@ftbp_exit:
                ret                                             ;
ftab_print      endp                                            ;
endif

INITCODE        ends

                include misc/meminit.inc
                include misc/mappic.inc

DATA16          segment
embedded_errs   dw      offset msg_loading, offset msg_no486, offset msg_notrm,
                        offset msg_memlow, offset msg_softfault, offset msg_filetab,
                        offset msg_dpmierr, offset msg_unperr, offset msg_no32mb,
                        offset msg_nothing, offset msg_mcberr, offset msg_faterr,
                        offset msg_diskerr, offset msg_noextdta, offset msg_runextdta,
                        offset msg_disknomem
embedded_hdrs   dw      offset mhdr_warning, offset mhdr_fatal, offset mhdr_about
dotpos          db      msg_no486 - msg_loading - 1
mhdr_warning    db      0Eh, 'Warning: ',0
mhdr_fatal      db      0Ch, 'Fatal: ', 0
mhdr_about      db      0Ah, 0, 0
msg_loading     db      2, 0DBh,0DBh,' LOADING ',0DBh,0DBh,0       ; 00
msg_no486       db      1,'no 80486DX+ detected.',0                ; 01
msg_notrm       db      1,'system already in protected mode.',0    ; 02
msg_memlow      db      1,'invalid low memory size.',0             ; 03
msg_softfault   db      1,'software fault occurs.',0               ; 04
msg_filetab     db      1,'invalid microfsd data.',0               ; 05
msg_dpmierr     db      1,'protected mode interface error.',0      ; 06
msg_unperr      db      1,'loader binary is damaged.',0            ; 07
msg_no32mb      db      1,'at least 24Mb is required.',0           ; 08
msg_nothing     db      0,'nothing to do.',0                       ; 09
msg_mcberr      db      1,'memory allocation error.',0             ; 10
msg_faterr      db      1,'FAT processing error.',0                ; 11
msg_diskerr     db      1,'disk read error.',0                     ; 12
msg_noextdta    db      1,'QSINIT.LDI is missing or invalid.',0    ; 13
msg_runextdta   db      1,'unable to unpack secondary code.',0     ; 14
msg_disknomem   db      1,'no memory for virtual disk.',0          ; 15
DATA16          ends
                end start
